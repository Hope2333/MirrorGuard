#include "data_structs.h"
#include "logging.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

FileList* create_file_list() {
    FileList *list = malloc(sizeof(FileList));
    if (!list) {
        return NULL;
    }
    
    list->files = NULL;
    list->count = 0;
    list->capacity = 0;
    pthread_mutex_init(&list->lock, NULL);
    
    return list;
}

void free_file_list(FileList *list) {
    if (!list) return;
    
    for (size_t i = 0; i < list->count; i++) {
        free_file_info(&list->files[i]);
    }
    free(list->files);
    pthread_mutex_destroy(&list->lock);
    free(list);
}

int add_file_to_list(FileList *list, const char *path, const char *hash, size_t size, time_t mtime) {
    if (!list || !path || !hash) return -1;
    
    FileInfo *info = create_file_info(path, hash, size, mtime);
    if (!info) return -1;
    
    pthread_mutex_lock(&list->lock);
    
    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity ? list->capacity * 2 : 128;
        if (new_capacity > 1000000) { // 防止内存耗尽
            pthread_mutex_unlock(&list->lock);
            free_file_info(info);
            log_msg(LOG_ERROR, "文件列表过大，防止内存溢出");
            return -1;
        }
        FileInfo *new_files = realloc(list->files, new_capacity * sizeof(FileInfo));
        if (!new_files) {
            pthread_mutex_unlock(&list->lock);
            free_file_info(info);
            log_msg(LOG_ERROR, "内存分配失败: 文件列表");
            return -1;
        }
        list->files = new_files;
        list->capacity = new_capacity;
    }
    
    list->files[list->count] = *info;
    list->count++;
    
    pthread_mutex_unlock(&list->lock);
    free_file_info(info);
    return 0;
}

FileInfo* create_file_info(const char *path, const char *hash, size_t size, time_t mtime) {
    FileInfo *info = malloc(sizeof(FileInfo));
    if (!info) return NULL;
    
    info->path = strdup(path);
    if (!info->path) {
        free(info);
        return NULL;
    }
    
    if (hash) {
        strncpy(info->hash, hash, sizeof(info->hash) - 1);
        info->hash[sizeof(info->hash) - 1] = '\0';
    } else {
        info->hash[0] = '\0';
    }
    
    info->size = size;
    info->mtime = mtime;
    
    return info;
}

void free_file_info(FileInfo *info) {
    if (info) {
        free(info->path);
        free(info);
    }
}

// 添加排序函数实现
int compare_file_info_by_path(const void *a, const void *b) {
    const FileInfo *info_a = (const FileInfo *)a;
    const FileInfo *info_b = (const FileInfo *)b;
    return strcmp(info_a->path, info_b->path);
}