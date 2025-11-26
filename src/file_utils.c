#include "file_utils.h"
#include "config.h"
#include "logging.h"
#include "path_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>

extern Config config;
extern Statistics stats;
extern volatile sig_atomic_t g_interrupted;

// 计算文件的SHA256 (使用EVP接口，OpenSSL 3.0兼容)
int compute_sha256(const char *file_path, char *hash_str) {
    if (!file_path || !hash_str) {
        log_msg(LOG_ERROR, "计算SHA256参数错误");
        return -1;
    }
    
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    const EVP_MD *md = EVP_sha256();
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        log_msg(LOG_ERROR, "无法创建EVP上下文");
        return -1;
    }
    
    int fd = -1;
    ssize_t bytes_read;
    unsigned char buffer[64 * 1024]; // 64KB
    struct stat sb;
    
    // 检查文件是否存在且可读
    if (stat(file_path, &sb) != 0) {
        log_msg(LOG_WARN, "无法访问文件 '%s': %s", file_path, strerror(errno));
        EVP_MD_CTX_free(mdctx);
        return -1;
    }
    
    if (!S_ISREG(sb.st_mode)) {
        log_msg(LOG_WARN, "非普通文件: %s", file_path);
        EVP_MD_CTX_free(mdctx);
        return -1;
    }
    
    if ((fd = open(file_path, O_RDONLY)) == -1) {
        log_msg(LOG_WARN, "无法打开文件 '%s': %s", file_path, strerror(errno));
        EVP_MD_CTX_free(mdctx);
        return -1;
    }
    
    // 检查是否被中断
    if (g_interrupted) {
        close(fd);
        EVP_MD_CTX_free(mdctx);
        return -1;
    }
    
    if (EVP_DigestInit_ex(mdctx, md, NULL) != 1) {
        log_msg(LOG_ERROR, "EVP_DigestInit_ex failed");
        close(fd);
        EVP_MD_CTX_free(mdctx);
        return -1;
    }
    
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        if (EVP_DigestUpdate(mdctx, buffer, bytes_read) != 1) {
            log_msg(LOG_ERROR, "EVP_DigestUpdate failed");
            close(fd);
            EVP_MD_CTX_free(mdctx);
            return -1;
        }
        
        // 更新统计
        pthread_mutex_lock(&stats.lock);
        stats.bytes_processed += bytes_read;
        pthread_mutex_unlock(&stats.lock);
        
        // 检查是否被中断
        if (g_interrupted) {
            close(fd);
            EVP_MD_CTX_free(mdctx);
            return -1;
        }
    }
    
    if (bytes_read == -1) {
        close(fd);
        EVP_MD_CTX_free(mdctx);
        log_msg(LOG_ERROR, "读取文件 '%s' 失败: %s", file_path, strerror(errno));
        return -1;
    }
    
    if (EVP_DigestFinal_ex(mdctx, hash, &hash_len) != 1) {
        log_msg(LOG_ERROR, "EVP_DigestFinal_ex failed");
        close(fd);
        EVP_MD_CTX_free(mdctx);
        return -1;
    }
    
    close(fd);
    EVP_MD_CTX_free(mdctx);
    
    // 转换为十六进制字符串
    for (unsigned int i = 0; i < hash_len; i++) {
        sprintf(hash_str + (i * 2), "%02x", hash[i]);
    }
    hash_str[hash_len * 2] = '\0';
    
    return 0;
}

// 验证单个文件
FileStatus verify_file(const char *mirror_dir, const char *rel_path, const char *expected_hash) {
    if (!mirror_dir || !rel_path || !expected_hash) {
        return FILE_STATUS_ERROR;
    }
    
    char full_path[MAX_PATH];
    char actual_hash[SHA256_DIGEST_LENGTH * 2 + 1] = {0};
    
    // 构建完整路径
    if (mirror_dir[strlen(mirror_dir)-1] == '/') {
        snprintf(full_path, sizeof(full_path), "%s%s", mirror_dir, rel_path);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", mirror_dir, rel_path);
    }
    
    // 规范化
    char *norm_path = normalize_path(full_path);
    if (!norm_path) return FILE_STATUS_ERROR;
    
    // 检查路径安全性
    if (!is_safe_path(norm_path)) {
        log_msg(LOG_WARN, "不安全路径: %s", norm_path);
        free_path((char**)&norm_path);  // 使用安全的内存释放函数
        return FILE_STATUS_ERROR;
    }
    
    struct stat sb;
    if (stat(norm_path, &sb) != 0) {
        free_path((char**)&norm_path);  // 释放内存
        return FILE_STATUS_MISSING; // 文件不存在
    }
    
    if (!S_ISREG(sb.st_mode)) {
        log_msg(LOG_WARN, "非普通文件: %s", norm_path);
        free_path((char**)&norm_path);  // 释放内存
        return FILE_STATUS_ERROR; // 非规文件
    }
    
    // 计算实际哈希
    int hash_result = compute_sha256(norm_path, actual_hash);
    free_path((char**)&norm_path);  // 释放内存
    
    if (hash_result != 0) {
        return FILE_STATUS_ERROR; // 哈希计算失败
    }
    
    // 比较哈希
    if (strcmp(actual_hash, expected_hash) == 0) {
        return FILE_STATUS_VALID;
    } else {
        return FILE_STATUS_CORRUPT;
    }
}