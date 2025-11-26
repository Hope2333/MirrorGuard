#include "directory_scan.h"
#include "config.h"
#include "logging.h"
#include "path_utils.h"
#include "file_utils.h"  // Add this header file
#include "progress.h"    // Add progress bar header file
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h> // For readlink, realpath, getuid, etc.
#include <errno.h>  // For errno
#include <pthread.h>

extern Config config;
extern volatile sig_atomic_t g_interrupted;

// Pre-count files in a directory
size_t count_files_in_directory(const char *dir_path) {
    size_t count = 0;
    DIR *dir;
    struct dirent *entry;
    struct stat sb;
    char full_path[MAX_PATH];

    char *norm_dir_path = normalize_path(dir_path);
    if (!norm_dir_path) return 0;

    char temp_path[MAX_PATH];
    strncpy(temp_path, norm_dir_path, sizeof(temp_path) - 1);
    temp_path[sizeof(temp_path) - 1] = '\0';
    free_path(&norm_dir_path);  // Free dynamically allocated memory

    if (!(dir = opendir(temp_path))) {
        log_msg(LOG_WARN, "无法打开目录进行计数 '%s': %s", temp_path, strerror(errno)); // Add logging
        return 0;
    }

    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Check path length to prevent overflow
        size_t path_len = strlen(temp_path);
        size_t name_len = strlen(entry->d_name);
        if (path_len + name_len + 2 > MAX_PATH) {  // +2 for '/' and '\0'
            log_msg(LOG_WARN, "路径过长，跳过计数: %s/%s", temp_path, entry->d_name); // Log the skipped path
            continue;  // Skip overly long paths
        }

        snprintf(full_path, sizeof(full_path), "%s/%s", temp_path, entry->d_name); // Safe due to check above

        // Normalize path
        char *norm_path = normalize_path(full_path);
        if (!norm_path) {
            continue;
        }

        // Copy path
        char path_copy[MAX_PATH];
        strncpy(path_copy, norm_path, sizeof(path_copy) - 1);
        path_copy[sizeof(path_copy) - 1] = '\0';
        free_path(&norm_path);  // Free dynamically allocated memory

        // Check path safety
        if (!is_safe_path(path_copy)) {
            continue;
        }

        // Check exclude
        if (should_exclude(path_copy)) {
            continue;
        }

        // Get file status
        if (lstat(path_copy, &sb) == -1) {
            log_msg(LOG_WARN, "无法获取状态用于计数 '%s': %s", path_copy, strerror(errno)); // Log error
            continue;
        }

        // Handle symbolic links
        if (S_ISLNK(sb.st_mode)) {
            if (config.follow_symlinks) {
                char target[MAX_PATH];
                ssize_t len = readlink(path_copy, target, sizeof(target) - 1);
                if (len != -1) {
                    target[len] = '\0';
                    char resolved[MAX_PATH];
                    if (realpath(target, resolved)) {
                        struct stat resolved_sb;
                        if (stat(resolved, &resolved_sb) == 0 && S_ISREG(resolved_sb.st_mode)) {
                            count++;
                        }
                    }
                }
            }
            continue;
        }

        // Recursively process directory
        if (S_ISDIR(sb.st_mode)) {
            if (config.recursive) {
                count += count_files_in_directory(path_copy);
            }
            continue;
        }

        // Only count regular files
        if (S_ISREG(sb.st_mode)) {
            count++;
        }
    }

    closedir(dir);
    return count;
}

// Extract directory name for display
const char* extract_dir_name(const char *path) {
    const char *last_slash = strrchr(path, '/');
    if (last_slash) {
        return last_slash + 1;
    }
    return path;
}

// Recursively scan directory
int scan_directory(const char *dir_path, FileList *list) {
    if (!dir_path || !list) {
        log_msg(LOG_ERROR, "扫描目录参数错误");
        return -1;
    }

    char full_path[MAX_PATH];
    DIR *dir;
    struct dirent *entry;
    struct stat sb;

    // Normalize directory path
    char *norm_dir_path = normalize_path(dir_path);
    if (!norm_dir_path) {
        log_msg(LOG_ERROR, "路径规范化失败: %s", dir_path);
        return -1;
    }

    // Copy path for use
    char temp_path[MAX_PATH];
    strncpy(temp_path, norm_dir_path, sizeof(temp_path) - 1);
    temp_path[sizeof(temp_path) - 1] = '\0';
    free_path(&norm_dir_path);  // Free dynamically allocated memory

    if (!(dir = opendir(temp_path))) {
        log_msg(LOG_WARN, "无法打开目录 '%s': %s", temp_path, strerror(errno));
        return -1;
    }

    // Pre-count files
    size_t total_files = count_files_in_directory(temp_path);

    // Create progress bar (if enabled)
    // int progress_bar_index = -1; // Remove unused variable
    if (config.progress && !config.no_progress_bar) {
        // Create progress bar, using directory name for display
        const char *dir_name = extract_dir_name(temp_path);
        char progress_name[256]; // Size matches potential warning context
        // Truncate dir_name if necessary to fit "扫描目录: " prefix + dir_name + null terminator within 256
        size_t prefix_len = strlen("扫描目录: ");
        size_t max_name_len = sizeof(progress_name) - prefix_len - 1; // -1 for null terminator
        if (strlen(dir_name) > max_name_len) {
             snprintf(progress_name, sizeof(progress_name), "扫描目录: %.*s...", (int)max_name_len - 3, dir_name);
        } else {
             snprintf(progress_name, sizeof(progress_name), "扫描目录: %s", dir_name);
        }
        create_file_progress(progress_name, total_files);
        // progress_bar_index = 0; // Not used, so no need to assign
    }

    size_t files_processed = 0;

    while ((entry = readdir(dir)) != NULL) {
        // Check interruption
        if (g_interrupted) {
            closedir(dir);
            if (config.progress && !config.no_progress_bar) {
                finish_file_progress();
            }
            return -1;
        }

        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Check path length to prevent overflow
        size_t path_len = strlen(temp_path);
        size_t name_len = strlen(entry->d_name);
        if (path_len + name_len + 2 > MAX_PATH) {  // +2 for '/' and '\0'
            log_msg(LOG_WARN, "路径过长，跳过: %s/%s", temp_path, entry->d_name); // Log the skipped path
            continue;  // Skip overly long paths
        }

        // Build full path
        snprintf(full_path, sizeof(full_path), "%s/%s", temp_path, entry->d_name); // Safe due to check above

        // Normalize path
        char *norm_path = normalize_path(full_path);
        if (!norm_path) {
            continue;
        }

        // Copy path, because normalize_path returns dynamic memory
        char path_copy[MAX_PATH];
        strncpy(path_copy, norm_path, sizeof(path_copy) - 1);
        path_copy[sizeof(path_copy) - 1] = '\0';

        // Check path safety
        if (!is_safe_path(path_copy)) {
            log_msg(LOG_WARN, "不安全路径，跳过: %s", path_copy);
            free_path(&norm_path);  // Free dynamically allocated memory
            continue;
        }

        // Check exclude
        if (should_exclude(path_copy)) {
            free_path(&norm_path);  // Free dynamically allocated memory
            continue;
        }

        // Get file status
        if (lstat(path_copy, &sb) == -1) {
            log_msg(LOG_WARN, "无法获取状态 '%s': %s", path_copy, strerror(errno));
            free_path(&norm_path);  // Free dynamically allocated memory
            continue;
        }

        // Handle symbolic links
        if (S_ISLNK(sb.st_mode)) {
            if (!config.follow_symlinks) {
                free_path(&norm_path);  // Free dynamically allocated memory
                continue;
            }

            // Follow symbolic link (prevent loops)
            char target[MAX_PATH];
            ssize_t len = readlink(path_copy, target, sizeof(target) - 1);
            if (len != -1) {
                target[len] = '\0';
                char resolved[MAX_PATH];
                if (realpath(target, resolved)) {
                    struct stat resolved_sb;
                    if (stat(resolved, &resolved_sb) == 0 && S_ISREG(resolved_sb.st_mode)) {
                        // Compute hash and add to list
                        char hash_str[SHA256_DIGEST_LENGTH * 2 + 1];
                        if (compute_sha256(resolved, hash_str) == 0) {
                            add_file_to_list(list, resolved, hash_str, resolved_sb.st_size, resolved_sb.st_mtime);
                        }
                        files_processed++;

                        // Update progress bar
                        if (config.progress && !config.no_progress_bar) {
                            update_file_progress(files_processed);
                        }
                    }
                }
            }
            free_path(&norm_path);  // Free dynamically allocated memory
            continue;
        }

        // Recursively process directory
        if (S_ISDIR(sb.st_mode)) {
            free_path(&norm_path);  // Free current path memory
            if (config.recursive) {
                if (scan_directory(full_path, list) != 0) {
                    closedir(dir);
                    if (config.progress && !config.no_progress_bar) {
                        finish_file_progress();
                    }
                    return -1;
                }
            }
            continue;
        }

        // Only process regular files
        if (S_ISREG(sb.st_mode)) {
            // Compute hash and add to list
            char hash_str[SHA256_DIGEST_LENGTH * 2 + 1];
            if (compute_sha256(path_copy, hash_str) == 0) {
                add_file_to_list(list, path_copy, hash_str, sb.st_size, sb.st_mtime);
            }

            files_processed++;

            // Update progress bar
            if (config.progress && !config.no_progress_bar) {
                update_file_progress(files_processed);
            }
        }

        free_path(&norm_path);  // Free dynamically allocated memory
    }

    // Finish progress bar
    if (config.progress && !config.no_progress_bar) {
        finish_file_progress();
    }

    closedir(dir);
    return 0;
}
