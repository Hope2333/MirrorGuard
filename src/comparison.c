#include "comparison.h"
#include "config.h"
#include "logging.h"
#include "path_utils.h"
#include "directory_scan.h"
#include "file_utils.h"
#include "data_structs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

extern Config config;
extern volatile sig_atomic_t g_interrupted;

// 比较两个清单文件
int compare_manifests(const char *manifest1, const char *manifest2) {
    if (!manifest1 || !manifest2) {
        log_msg(LOG_ERROR, "比较清单参数错误");
        return MIRRORGUARD_ERROR_INVALID_ARGS;
    }

    FILE *file1 = fopen(manifest1, "r");
    FILE *file2 = fopen(manifest2, "r");
    if (!file1 || !file2) {
        log_msg(LOG_ERROR, "无法打开清单文件");
        if (file1) fclose(file1);
        if (file2) fclose(file2);
        return MIRRORGUARD_ERROR_FILE_IO;
    }

    char line1[MAX_PATH + SHA256_DIGEST_LENGTH * 2 + 10];
    char line2[MAX_PATH + SHA256_DIGEST_LENGTH * 2 + 10];
    char hash1[SHA256_DIGEST_LENGTH * 2 + 1];
    char path1[MAX_PATH];
    char hash2[SHA256_DIGEST_LENGTH * 2 + 1];
    char path2[MAX_PATH];

    size_t same_count = 0;
    size_t diff_count = 0;
    size_t missing_in_1 = 0;
    size_t missing_in_2 = 0;

    log_msg(LOG_INFO, "开始比较清单: %s vs %s", manifest1, manifest2);

    // 读取两个清单的所有行到内存中进行比较
    FileList *list1 = create_file_list();
    FileList *list2 = create_file_list();
    if (!list1 || !list2) {
        if (file1) fclose(file1);
        if (file2) fclose(file2);
        if (list1) free_file_list(list1);
        if (list2) free_file_list(list2);
        return MIRRORGUARD_ERROR_MEMORY;
    }

    // 读取第一个清单
    while (fgets(line1, sizeof(line1), file1)) {
        if (sscanf(line1, "%64s *%[^\n]", hash1, path1) == 2) {
            add_file_to_list(list1, path1, hash1, 0, 0);
        }
    }

    // 读取第二个清单
    while (fgets(line2, sizeof(line2), file2)) {
        if (sscanf(line2, "%64s *%[^\n]", hash2, path2) == 2) {
            add_file_to_list(list2, path2, hash2, 0, 0);
        }
    }

    fclose(file1);
    fclose(file2);

    // 按路径排序，避免重复处理
    if (list1->count > 0) {
        qsort(list1->files, list1->count, sizeof(FileInfo), compare_file_info_by_path);
    }
    if (list2->count > 0) {
        qsort(list2->files, list2->count, sizeof(FileInfo), compare_file_info_by_path);
    }

    // 使用线性比较算法（类似归并排序）
    size_t i = 0, j = 0;
    while (i < list1->count && j < list2->count) {
        int cmp = strcmp(list1->files[i].path, list2->files[j].path);
        if (cmp == 0) {
            // 路径相同，比较哈希
            if (strcmp(list1->files[i].hash, list2->files[j].hash) == 0) {
                same_count++;
            } else {
                log_msg(LOG_WARN, "哈希不同: %s", list1->files[i].path);
                diff_count++;
            }
            i++;
            j++;
        } else if (cmp < 0) {
            // 仅在清单1中存在
            log_msg(LOG_WARN, "仅在清单1中存在: %s", list1->files[i].path);
            missing_in_2++;
            i++;
        } else {
            // 仅在清单2中存在
            log_msg(LOG_WARN, "仅在清单2中存在: %s", list2->files[j].path);
            missing_in_1++;
            j++;
        }
    }

    // 处理剩余的文件
    while (i < list1->count) {
        log_msg(LOG_WARN, "仅在清单1中存在: %s", list1->files[i].path);
        missing_in_2++;
        i++;
    }
    while (j < list2->count) {
        log_msg(LOG_WARN, "仅在清单2中存在: %s", list2->files[j].path);
        missing_in_1++;
        j++;
    }

    free_file_list(list1);
    free_file_list(list2);

    log_msg(LOG_INFO, "\n清单比较结果:");
    log_msg(LOG_INFO, "  完全相同: %zu", same_count);
    log_msg(LOG_INFO, "  哈希不同: %zu", diff_count);
    log_msg(LOG_INFO, "  仅在清单1: %zu", missing_in_2);
    log_msg(LOG_INFO, "  仅在清单2: %zu", missing_in_1);

    if (diff_count > 0 || missing_in_1 > 0 || missing_in_2 > 0) {
        log_msg(LOG_WARN, "❌ 清单内容不一致!");
        return MIRRORGUARD_ERROR_GENERAL;
    }

    log_msg(LOG_INFO, "✅ 两个清单内容完全一致!");
    return MIRRORGUARD_OK;
}

// 直接比较两个目录
int compare_directories(const char *dir1, const char *dir2) {
    if (!dir1 || !dir2) {
        log_msg(LOG_ERROR, "目录比较参数错误");
        return MIRRORGUARD_ERROR_INVALID_ARGS;
    }

    FileList *list1 = create_file_list();
    FileList *list2 = create_file_list();
    if (!list1 || !list2) {
        return MIRRORGUARD_ERROR_MEMORY;
    }

    log_msg(LOG_INFO, "开始扫描目录1: %s", dir1);
    if (scan_directory(dir1, list1) != 0) {
        free_file_list(list1);
        free_file_list(list2);
        return MIRRORGUARD_ERROR_FILE_IO;
    }

    log_msg(LOG_INFO, "开始扫描目录2: %s", dir2);
    if (scan_directory(dir2, list2) != 0) {
        free_file_list(list1);
        free_file_list(list2);
        return MIRRORGUARD_ERROR_FILE_IO;
    }

    size_t same_count = 0;
    size_t diff_count = 0;
    size_t missing_in_1 = 0;
    size_t missing_in_2 = 0;

    log_msg(LOG_INFO, "开始比较 %zu 个文件 vs %zu 个文件", list1->count, list2->count);

    // 按路径排序，避免重复处理
    if (list1->count > 0) {
        qsort(list1->files, list1->count, sizeof(FileInfo), compare_file_info_by_path);
    }
    if (list2->count > 0) {
        qsort(list2->files, list2->count, sizeof(FileInfo), compare_file_info_by_path);
    }

    // 使用线性比较算法（类似归并排序）
    size_t i = 0, j = 0;
    while (i < list1->count && j < list2->count) {
        int cmp = strcmp(list1->files[i].path, list2->files[j].path);
        if (cmp == 0) {
            // 路径相同，比较哈希
            if (strcmp(list1->files[i].hash, list2->files[j].hash) == 0) {
                same_count++;
            } else {
                log_msg(LOG_WARN, "文件内容不同: %s", list1->files[i].path);
                diff_count++;
            }
            i++;
            j++;
        } else if (cmp < 0) {
            // 仅在目录1中存在
            log_msg(LOG_WARN, "仅在目录1中存在: %s", list1->files[i].path);
            missing_in_2++;
            i++;
        } else {
            // 仅在目录2中存在
            log_msg(LOG_WARN, "仅在目录2中存在: %s", list2->files[j].path);
            missing_in_1++;
            j++;
        }
    }

    // 处理剩余的文件
    while (i < list1->count) {
        log_msg(LOG_WARN, "仅在目录1中存在: %s", list1->files[i].path);
        missing_in_2++;
        i++;
    }
    while (j < list2->count) {
        log_msg(LOG_WARN, "仅在目录2中存在: %s", list2->files[j].path);
        missing_in_1++;
        j++;
    }

    free_file_list(list1);
    free_file_list(list2);

    log_msg(LOG_INFO, "\n目录比较结果:");
    log_msg(LOG_INFO, "  完全相同: %zu", same_count);
    log_msg(LOG_INFO, "  内容不同: %zu", diff_count);
    log_msg(LOG_INFO, "  仅在目录1: %zu", missing_in_2);
    log_msg(LOG_INFO, "  仅在目录2: %zu", missing_in_1);

    if (diff_count > 0 || missing_in_1 > 0 || missing_in_2 > 0) {
        log_msg(LOG_WARN, "❌ 目录内容不一致!");
        return MIRRORGUARD_ERROR_GENERAL;
    }

    log_msg(LOG_INFO, "✅ 两个目录内容完全一致!");
    return MIRRORGUARD_OK;
}
