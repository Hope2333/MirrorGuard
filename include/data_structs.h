#ifndef DATA_STRUCTS_H
#define DATA_STRUCTS_H

#include <openssl/sha.h>
#include <sys/types.h>
#include <pthread.h>

#define SHA256_DIGEST_LENGTH 32

// 文件信息结构
typedef struct {
    char *path;
    char hash[SHA256_DIGEST_LENGTH * 2 + 1];
    size_t size;
    time_t mtime;
} FileInfo;

// 文件列表结构
typedef struct {
    FileInfo *files;
    size_t count;
    size_t capacity;
    pthread_mutex_t lock;
} FileList;

// 文件状态
typedef enum {
    FILE_STATUS_MISSING = -2,
    FILE_STATUS_CORRUPT = -1,
    FILE_STATUS_VALID = 0,
    FILE_STATUS_EXTRA = 1,
    FILE_STATUS_ERROR = 2
} FileStatus;

// 比对结果
typedef enum {
    COMPARE_SAME = 0,
    COMPARE_DIFFERENT = 1,
    COMPARE_MISSING_IN_FIRST = 2,
    COMPARE_MISSING_IN_SECOND = 3,
    COMPARE_ERROR = 4
} CompareResult;

FileList* create_file_list();
void free_file_list(FileList *list);
int add_file_to_list(FileList *list, const char *path, const char *hash, size_t size, time_t mtime);
FileInfo* create_file_info(const char *path, const char *hash, size_t size, time_t mtime);
void free_file_info(FileInfo *info);

// 添加排序函数声明
int compare_file_info_by_path(const void *a, const void *b);

#endif // DATA_STRUCTS_H