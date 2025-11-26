#include "directory_scan.h"
#include "config.h"
#include "logging.h"
#include "path_utils.h"
#include "file_utils.h"  // 添加这个头文件
#include "progress.h"    // 添加进度条头文件
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

extern Config config;
extern volatile sig_atomic_t g_interrupted;

// 预先统计目录中的文件数量
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
    free_path(&norm_dir_path);  // 释放动态分配的内存

    if (!(dir = opendir(temp_path))) {
        return 0;
    }

    while ((entry = readdir(dir)) != NULL) {
        // 跳过 . 和 ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // 检查路径长度，防止溢出
        size_t path_len = strlen(temp_path);
        size_t name_len = strlen(entry->d_name);
        if (path_len + name_len + 2 > MAX_PATH) {  // +2 for '/' and '\0'
            continue;  // 跳过过长的路径
        }

        snprintf(full_path, sizeof(full_path), "%s/%s", temp_path, entry->d_name);

        // 规范化路径
        char *norm_path = normalize_path(full_path);
        if (!norm_path) {
            continue;
        }

        // 复制路径
        char path_copy[MAX_PATH];
        strncpy(path_copy, norm_path, sizeof(path_copy) - 1);
        path_copy[sizeof(path_copy) - 1] = '\0';
        free_path(&norm_path);  // 释放动态分配的内存

        // 检查路径安全性
        if (!is_safe_path(path_copy)) {
            continue;
        }

        // 检查排除
        if (should_exclude(path_copy)) {
            continue;
        }

        // 获取文件状态
        if (lstat(path_copy, &sb) == -1) {
            continue;
        }

        // 处理符号链接
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

        // 递归处理目录
        if (S_ISDIR(sb.st_mode)) {
            if (config.recursive) {
                count += count_files_in_directory(path_copy);
            }
            continue;
        }

        // 仅统计普通文件
        if (S_ISREG(sb.st_mode)) {
            count++;
        }
    }

    closedir(dir);
    return count;
}

// 提取目录名用于显示
const char* extract_dir_name(const char *path) {
    const char *last_slash = strrchr(path, '/');
    if (last_slash) {
        return last_slash + 1;
    }
    return path;
}

// 递归扫描目录
int scan_directory(const char *dir_path, FileList *list) {
    if (!dir_path || !list) {
        log_msg(LOG_ERROR, "扫描目录参数错误");
        return -1;
    }

    char full_path[MAX_PATH];
    DIR *dir;
    struct dirent *entry;
    struct stat sb;

    // 规范化目录路径
    char *norm_dir_path = normalize_path(dir_path);
    if (!norm_dir_path) {
        log_msg(LOG_ERROR, "路径规范化失败: %s", dir_path);
        return -1;
    }

    // 复制路径用于使用
    char temp_path[MAX_PATH];
    strncpy(temp_path, norm_dir_path, sizeof(temp_path) - 1);
    temp_path[sizeof(temp_path) - 1] = '\0';
    free_path(&norm_dir_path);  // 释放动态分配的内存

    if (!(dir = opendir(temp_path))) {
        log_msg(LOG_WARN, "无法打开目录 '%s': %s", temp_path, strerror(errno));
        return -1;
    }

    // 预先统计文件数量
    size_t total_files = count_files_in_directory(temp_path);

    // 创建进度条（如果启用）
    int progress_bar_index = -1;
    if (config.progress && !config.no_progress_bar) {
        // 创建进度条，使用目录名作为显示名称
        const char *dir_name = extract_dir_name(temp_path);
        char progress_name[256];
        snprintf(progress_name, sizeof(progress_name), "扫描目录: %s", dir_name);
        create_file_progress(progress_name, total_files);
        progress_bar_index = 0;  // 使用索引0表示文件进度条
    }

    size_t files_processed = 0;

    while ((entry = readdir(dir)) != NULL) {
        // 检查中断
        if (g_interrupted) {
            closedir(dir);
            if (config.progress && !config.no_progress_bar) {
                finish_file_progress();
            }
            return -1;
        }

        // 跳过 . 和 ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // 检查路径长度，防止溢出
        size_t path_len = strlen(temp_path);
        size_t name_len = strlen(entry->d_name);
        if (path_len + name_len + 2 > MAX_PATH) {  // +2 for '/' and '\0'
            continue;  // 跳过过长的路径
        }

        // 构建完整路径
        snprintf(full_path, sizeof(full_path), "%s/%s", temp_path, entry->d_name);

        // 规范化路径
        char *norm_path = normalize_path(full_path);
        if (!norm_path) {
            continue;
        }

        // 复制路径，因为 normalize_path 返回动态内存
        char path_copy[MAX_PATH];
        strncpy(path_copy, norm_path, sizeof(path_copy) - 1);
        path_copy[sizeof(path_copy) - 1] = '\0';

        // 检查路径安全性
        if (!is_safe_path(path_copy)) {
            log_msg(LOG_WARN, "不安全路径，跳过: %s", path_copy);
            free_path(&norm_path);  // 释放动态分配的内存
            continue;
        }

        // 检查排除
        if (should_exclude(path_copy)) {
            free_path(&norm_path);  // 释放动态分配的内存
            continue;
        }

        // 获取文件状态
        if (lstat(path_copy, &sb) == -1) {
            log_msg(LOG_WARN, "无法获取状态 '%s': %s", path_copy, strerror(errno));
            free_path(&norm_path);  // 释放动态分配的内存
            continue;
        }

        // 处理符号链接
        if (S_ISLNK(sb.st_mode)) {
            if (!config.follow_symlinks) {
                free_path(&norm_path);  // 释放动态分配的内存
                continue;
            }

            // 跟随符号链接 (防止循环)
            char target[MAX_PATH];
            ssize_t len = readlink(path_copy, target, sizeof(target) - 1);
            if (len != -1) {
                target[len] = '\0';
                char resolved[MAX_PATH];
                if (realpath(target, resolved)) {
                    struct stat resolved_sb;
                    if (stat(resolved, &resolved_sb) == 0 && S_ISREG(resolved_sb.st_mode)) {
                        // 计算哈希并添加到列表
                        char hash_str[SHA256_DIGEST_LENGTH * 2 + 1];
                        if (compute_sha256(resolved, hash_str) == 0) {
                            add_file_to_list(list, resolved, hash_str, resolved_sb.st_size, resolved_sb.st_mtime);
                        }
                        files_processed++;

                        // 更新进度条
                        if (config.progress && !config.no_progress_bar) {
                            update_file_progress(files_processed);
                        }
                    }
                }
            }
            free_path(&norm_path);  // 释放动态分配的内存
            continue;
        }

        // 递归处理目录
        if (S_ISDIR(sb.st_mode)) {
            free_path(&norm_path);  // 释放当前路径内存
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

        // 仅处理普通文件
        if (S_ISREG(sb.st_mode)) {
            // 计算哈希并添加到列表
            char hash_str[SHA256_DIGEST_LENGTH * 2 + 1];
            if (compute_sha256(path_copy, hash_str) == 0) {
                add_file_to_list(list, path_copy, hash_str, sb.st_size, sb.st_mtime);
            }

            files_processed++;

            // 更新进度条
            if (config.progress && !config.no_progress_bar) {
                update_file_progress(files_processed);
            }
        }

        free_path(&norm_path);  // 释放动态分配的内存
    }

    // 完成进度条
    if (config.progress && !config.no_progress_bar) {
        finish_file_progress();
    }

    closedir(dir);
    return 0;
}
