#include "logging.h"
#include "config.h"
#include "progress.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

extern Config config;

void log_msg(LogLevel level, const char *fmt, ...) {
    if (config.quiet && level > LOG_WARN) return;
    
    // 隐藏进度条，显示日志，然后重新显示进度
    hide_progress_temporarily();
    
    // 获取时间戳
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *tm_info = localtime(&tv.tv_sec);
    
    const char *prefix = "";
    switch(level) {
        case LOG_ERROR: prefix = "\033[1;31m[ERROR]\033[0m "; break;
        case LOG_WARN:  prefix = "\033[1;33m[WARN]\033[0m  "; break;
        case LOG_INFO:  prefix = "\033[1;32m[INFO]\033[0m  "; break;
        case LOG_DEBUG: prefix = "[DEBUG] "; break;
        case LOG_TRACE: prefix = "[TRACE] "; break;
    }
    
    // 构建时间戳
    char timestamp[64];
    snprintf(timestamp, sizeof(timestamp), "[%04d-%02d-%02d %02d:%02d:%02d.%06ld] ",
             tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, tv.tv_usec);
    
    // 写入日志
    FILE *output = stderr;
    if (config.log_fp) {
        output = config.log_fp;
    }
    
    fprintf(output, "%s%s", timestamp, prefix);
    
    va_list args;
    va_start(args, fmt);
    vfprintf(output, fmt, args);
    fprintf(output, "\n");
    va_end(args);
    fflush(output);
    
    // 重新显示进度条
    show_progress_after_log();
}

void log_set_quiet(int quiet) {
    config.quiet = quiet;
}

void log_set_logfile(const char *log_file) {
    if (config.log_fp) {
        fclose(config.log_fp);
    }
    
    if (log_file) {
        config.log_fp = fopen(log_file, "a");
        if (!config.log_fp) {
            log_msg(LOG_ERROR, "无法打开日志文件: %s", log_file);
        }
    } else {
        config.log_fp = NULL;
    }
}