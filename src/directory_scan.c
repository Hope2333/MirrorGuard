#include "directory_scan.h"
#include "config.h"
#include "logging.h"
#include "path_utils.h"
#include "file_utils.h"
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

    if (!(dir = opendir(norm_dir_path))) {
        log_msg(LOG_WARN, "无法打开目录 '%s': %s", norm_dir_path, strerror(errno));
        free(norm_dir_path);  // 释放内存
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        // 检查中断
        if (g_interrupted) {
            closedir(dir);
            free(norm_dir_path);  // 释放内存
            return -1;
        }

        // 跳过 . 和 ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // 构建完整路径
        snprintf(full_path, sizeof(full_path), "%s/%s", norm_dir_path, entry->d_name);

        // 规范化路径
        char *norm_path = normalize_path(full_path);
        if (!norm_path) {
            continue;
        }

        // 检查路径安全性
        if (!is_safe_path(norm_path)) {
            log_msg(LOG_WARN, "不安全路径，跳过: %s", norm_path);
            free(norm_path);  // 释放内存
            continue;
        }

        // 检查排除
        if (should_exclude(norm_path)) {
            free(norm_path);  // 释放内存
            continue;
        }

        // 获取文件状态
        if (lstat(norm_path, &sb) == -1) {
            log_msg(LOG_WARN, "无法获取状态 '%s': %s", norm_path, strerror(errno));
            free(norm_path);  // 释放内存
            continue;
        }

        // 处理符号链接
        if (S_ISLNK(sb.st_mode)) {
            if (!config.follow_symlinks) {
                free(norm_path);  // 释放内存
                continue;
            }

            // 跟随符号链接 (防止循环)
            char target[MAX_PATH];
            ssize_t len = readlink(norm_path, target, sizeof(target) - 1);
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
                    }
                }
            }
            free(norm_path);  // 释放内存
            continue;
        }

        // 递归处理目录
        if (S_ISDIR(sb.st_mode)) {
            free(norm_path);  // 释放当前路径内存
            if (config.recursive) {
                if (scan_directory(full_path, list) != 0) {  // 使用原始路径
                    closedir(dir);
                    free(norm_dir_path);  // 释放内存
                    return -1;
                }
            }
            continue;
        }

        // 仅处理普通文件
        if (S_ISREG(sb.st_mode)) {
            // 计算哈希并添加到列表
            char hash_str[SHA256_DIGEST_LENGTH * 2 + 1];
            if (compute_sha256(norm_path, hash_str) == 0) {
                add_file_to_list(list, norm_path, hash_str, sb.st_size, sb.st_mtime);
            }
        }

        free(norm_path);  // 释放内存
    }

    closedir(dir);
    free(norm_dir_path);  // 释放内存
    return 0;
}
