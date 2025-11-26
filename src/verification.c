#include "verification.h"
#include "config.h"
#include "logging.h"
#include "path_utils.h"
#include "directory_scan.h"
#include "file_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

extern Config config;
extern Statistics stats;
extern volatile sig_atomic_t g_interrupted;

// 生成清单 (多源模式)
int generate_manifest_multi(const char *manifest_path) {
    if (!manifest_path) {
        log_msg(LOG_ERROR, "生成清单参数错误");
        return MIRRORGUARD_ERROR_INVALID_ARGS;
    }

    FileList *list = create_file_list();
    if (!list) {
        return MIRRORGUARD_ERROR_MEMORY;
    }

    log_msg(LOG_INFO, "开始扫描 %d 个源目录", config.source_count);

    // 递归收集所有源目录的文件
    for (int i = 0; i < config.source_count; i++) {
        log_msg(LOG_INFO, "扫描源目录: %s", config.source_dirs[i]);
        if (scan_directory(config.source_dirs[i], list) != 0) {
            free_file_list(list);
            return MIRRORGUARD_ERROR_FILE_IO;
        }
    }

    if (list->count == 0) {
        log_msg(LOG_ERROR, "未找到可处理的文件");
        free_file_list(list);
        return MIRRORGUARD_ERROR_GENERAL;
    }

    log_msg(LOG_INFO, "找到 %zu 个文件，开始生成清单...", list->count);

    // 创建临时清单
    char temp_manifest[MAX_PATH];
    snprintf(temp_manifest, sizeof(temp_manifest), "%s.tmp.%d", manifest_path, getpid());

    if (!config.dry_run) {
        FILE *manifest = fopen(temp_manifest, "w");
        if (!manifest) {
            log_msg(LOG_ERROR, "无法创建临时清单: %s", strerror(errno));
            free_file_list(list);
            return MIRRORGUARD_ERROR_FILE_IO;
        }

        // 写入所有文件信息
        for (size_t i = 0; i < list->count; i++) {
            fprintf(manifest, "%s *%s\n", list->files[i].hash, list->files[i].path);
        }

        fclose(manifest);
    }

    if (!config.dry_run) {
        // 原子重命名临时清单
        if (rename(temp_manifest, manifest_path) != 0) {
            log_msg(LOG_ERROR, "无法完成清单: %s", strerror(errno));
            unlink(temp_manifest);
            free_file_list(list);
            return MIRRORGUARD_ERROR_FILE_IO;
        }
    }

    log_msg(LOG_INFO, "多源清单生成成功: %s", manifest_path);
    log_msg(LOG_INFO, "总计文件数: %zu", list->count);

    free_file_list(list);
    return MIRRORGUARD_OK;
}

// 验证镜像
int verify_mirror(const char *mirror_dir, const char *manifest_path) {
    if (!mirror_dir || !manifest_path) {
        log_msg(LOG_ERROR, "验证镜像参数错误");
        return MIRRORGUARD_ERROR_INVALID_ARGS;
    }

    FILE *manifest = fopen(manifest_path, "r");
    if (!manifest) {
        log_msg(LOG_ERROR, "无法打开清单: %s", strerror(errno));
        return MIRRORGUARD_ERROR_FILE_IO;
    }

    char line[MAX_PATH + SHA256_DIGEST_LENGTH * 2 + 10];
    char expected_hash[SHA256_DIGEST_LENGTH * 2 + 1];
    char rel_path[MAX_PATH];
    int missing_count = 0;
    int corrupt_count = 0;
    int total_files = 0;
    int total_errors = 0;

    // 用于检测额外文件
    FileList *mirror_files = create_file_list();
    if (!mirror_files) {
        fclose(manifest);
        return MIRRORGUARD_ERROR_MEMORY;
    }

    log_msg(LOG_INFO, "开始验证镜像: %s", mirror_dir);

    if (config.extra_check) {
        log_msg(LOG_INFO, "扫描镜像目录以检测额外文件...");
        scan_directory(mirror_dir, mirror_files);
        log_msg(LOG_INFO, "镜像中找到 %zu 个文件", mirror_files->count);
    }

    log_msg(LOG_INFO, "读取清单文件: %s", manifest_path);

    // 验证清单中的每个文件
    while (fgets(line, sizeof(line), manifest)) {
        if (g_interrupted) {
            break;
        }

        // 清单格式: <hash> *<relative_path>
        if (sscanf(line, "%64s *%[^\n]", expected_hash, rel_path) != 2) {
            continue; // 跳过无效行
        }

        total_files++;

        if (should_exclude(rel_path)) {
            total_files--;
            continue;
        }

        FileStatus result = verify_file(mirror_dir, rel_path, expected_hash);

        if (result == FILE_STATUS_MISSING) {
            log_msg(LOG_ERROR, "❌ 缺失文件: %s", rel_path);
            missing_count++;
        } else if (result == FILE_STATUS_CORRUPT) {
            log_msg(LOG_ERROR, "❌ 哈希不匹配: %s", rel_path);
            corrupt_count++;
        } else if (result == FILE_STATUS_ERROR) {
            log_msg(LOG_ERROR, "❌ 验证错误: %s", rel_path);
            total_errors++;
        } else if (!config.quiet) {
            log_msg(LOG_INFO, "✅ 有效: %s", rel_path);
        }

        // 更新统计
        pthread_mutex_lock(&stats.lock);
        stats.processed_files++;
        if (result == FILE_STATUS_MISSING) stats.missing_files++;
        else if (result == FILE_STATUS_CORRUPT) stats.corrupt_files++;
        else if (result == FILE_STATUS_ERROR) stats.error_files++;
        pthread_mutex_unlock(&stats.lock);

        // 从mirror_files中移除已验证的文件
        if (config.extra_check) {
            for (size_t i = 0; i < mirror_files->count; i++) {
                if (mirror_files->files[i].path) {
                    const char *mirror_path = mirror_files->files[i].path;
                    if (strstr(mirror_path, rel_path) &&
                        strlen(mirror_path) - strlen(mirror_dir) - 1 == strlen(rel_path)) {
                        // 标记为已验证
                        free(mirror_files->files[i].path);
                        mirror_files->files[i].path = NULL;
                        break;
                    }
                }
            }
        }
    }

    fclose(manifest);

    // 检查额外文件
    if (config.extra_check) {
        for (size_t i = 0; i < mirror_files->count; i++) {
            if (mirror_files->files[i].path && !should_exclude(mirror_files->files[i].path)) {
                log_msg(LOG_WARN, "⚠  额外文件: %s", mirror_files->files[i].path);
                pthread_mutex_lock(&stats.lock);
                stats.extra_files++;
                pthread_mutex_unlock(&stats.lock);
                free(mirror_files->files[i].path);
                mirror_files->files[i].path = NULL;
            }
        }
        free_file_list(mirror_files);
    } else {
        free_file_list(mirror_files);
    }

    log_msg(LOG_INFO, "\n验证结果:");
    log_msg(LOG_INFO, "  总文件数: %d", total_files);
    log_msg(LOG_INFO, "  已处理: %zu", stats.processed_files);
    log_msg(LOG_INFO, "  缺失文件: %zu", stats.missing_files);
    log_msg(LOG_INFO, "  损坏文件: %zu", stats.corrupt_files);
    log_msg(LOG_INFO, "  验证错误: %zu", stats.error_files);
    log_msg(LOG_INFO, "  额外文件: %zu", stats.extra_files);

    if (missing_count > 0 || corrupt_count > 0 || total_errors > 0) {
        log_msg(LOG_ERROR, "❌ 镜像验证失败!");
        return MIRRORGUARD_ERROR_VERIFY_FAILED;
    }

    log_msg(LOG_INFO, "✅ 镜像验证成功 - 100%% 完整!");
    return MIRRORGUARD_OK;
}
