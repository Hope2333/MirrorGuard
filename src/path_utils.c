#include "path_utils.h"
#include "config.h"
#include "logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

extern Config config;

// 路径规范化 (移除冗余分隔符/..) - 返回动态分配的字符串
char* normalize_path(const char *path) {
    if (!path) return NULL;

    char *result = malloc(MAX_PATH);
    if (!result) return NULL;

    char temp[MAX_PATH];

    if (strlen(path) >= MAX_PATH) {
        // 注意：这里不能调用 log_msg，因为会导致循环依赖
        // 只能在调用者处处理错误
        free(result);
        return NULL;
    }

    // 复制并处理路径
    strncpy(temp, path, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    // 转换Windows分隔符
    for (char *p = temp; *p; ++p) {
        if (*p == '\\') *p = '/';
    }

    // 处理绝对路径 (以/开头)
    int is_absolute = (temp[0] == '/');

    char *p = temp;
    char *q = result;
    int skip_slash = 1;

    // 如果是绝对路径，保留开头的/
    if (is_absolute) {
        *q++ = '/';
        skip_slash = 0;
        p++;
    }

    while (*p) {
        if (*p == '/') {
            if (!skip_slash) {
                *q++ = '/';
                skip_slash = 1;
            }
            ++p;
        } else if (p[0] == '.' && p[1] == '.' && (p[2] == '/' || p[2] == '\0')) {
            // 处理 ..
            if (q > result) {
                if (is_absolute && q == result + 1) {
                    // 对于绝对路径，不能往上跳
                    p += 2;
                    if (*p == '/') p++;
                    continue;
                }
                q--; // 移除最后的斜杠
                while (q > result && *--q != '/');
                if (q == result && is_absolute) {
                    *q = '/';
                } else if (q >= result) {
                    q++;
                }
            }
            p += 2;
            if (*p == '/') p++; // 跳过后续斜杠
            skip_slash = 0;
        } else if (p[0] == '.' && (p[1] == '/' || p[1] == '\0')) {
            // 跳过 .
            p += 1;
            if (*p == '/') p++; // 跳过后续斜杠
            skip_slash = 0;
        } else {
            skip_slash = 0;
            while (*p && *p != '/') {
                *q++ = *p++;
            }
        }
    }

    // 如果结果为空（比如相对路径".."），返回"."
    if (q == result || (is_absolute && q == result + 1 && result[0] == '/')) {
        if (is_absolute) {
            *q++ = '.';
            *q = '\0';
        } else {
            result[0] = '.';
            result[1] = '\0';
        }
    } else {
        *q = '\0';
    }

    return result;
}

// 检查路径是否安全 (防止路径遍历攻击)
int is_safe_path(const char *path) {
    if (!path) return 0;

    // 检查是否包含 ../
    if (strstr(path, "../") || strstr(path, "..\\") ||
        (strlen(path) >= 3 && strcmp(path + strlen(path) - 3, "/..") == 0)) {
        return 0;
    }

    return 1;
}

// 检查是否应排除文件
int should_exclude(const char *path) {
    if (!path) return 1;

    // 检查隐藏文件
    if (config.ignore_hidden) {
        const char *base = strrchr(path, '/');
        if (!base) base = path;
        else base++;

        if (base[0] == '.' && base[1] != '\0' && base[1] != '/') {
            return 1;
        }
    }

    // 检查包含模式
    if (config.include_count > 0) {
        int matched = 0;
        for (int i = 0; i < config.include_count; i++) {
            if (config.include_patterns[i]) {
                if (config.case_sensitive) {
                    if (strstr(path, config.include_patterns[i])) {
                        matched = 1;
                        break;
                    }
                } else {
                    char *lower_path = strdup(path);
                    char *lower_pattern = strdup(config.include_patterns[i]);
                    if (lower_path && lower_pattern) {
                        for (char *p = lower_path; *p; p++) *p = tolower(*p);
                        for (char *p = lower_pattern; *p; p++) *p = tolower(*p);
                        if (strstr(lower_path, lower_pattern)) {
                            matched = 1;
                        }
                    }
                    free(lower_path);
                    free(lower_pattern);
                    if (matched) break;
                }
            }
        }
        if (!matched) return 1;
    }

    // 检查排除模式
    for (int i = 0; i < config.exclude_count; i++) {
        if (config.exclude_patterns[i]) {
            if (config.case_sensitive) {
                if (strstr(path, config.exclude_patterns[i])) {
                    return 1;
                }
            } else {
                char *lower_path = strdup(path);
                char *lower_pattern = strdup(config.exclude_patterns[i]);
                if (lower_path && lower_pattern) {
                    for (char *p = lower_path; *p; p++) *p = tolower(*p);
                    for (char *p = lower_pattern; *p; p++) *p = tolower(*p);
                    if (strstr(lower_path, lower_pattern)) {
                        free(lower_path);
                        free(lower_pattern);
                        return 1;
                    }
                }
                free(lower_path);
                free(lower_pattern);
            }
        }
    }

    return 0;
}
