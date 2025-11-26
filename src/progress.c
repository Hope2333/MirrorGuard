#include "progress.h"
#include "logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

extern Config config;
extern Statistics stats;

static ProgressBar file_bar = {0};
static ProgressBar overall_bar = {0};
static int progress_visible = 0;
static int progress_lines = 0;

void init_progress_system() {
    memset(&file_bar, 0, sizeof(ProgressBar));
    memset(&overall_bar, 0, sizeof(ProgressBar));
    pthread_mutex_init(&file_bar.lock, NULL);
    pthread_mutex_init(&overall_bar.lock, NULL);
}

void create_file_progress(const char *filename, size_t total_files) {
    if (config.no_progress_bar) return;
    
    pthread_mutex_lock(&file_bar.lock);
    // 截断文件名
    snprintf(file_bar.name, sizeof(file_bar.name) - 1, "%.40s", filename);
    strncpy(file_bar.full_name, filename, sizeof(file_bar.full_name) - 1);
    file_bar.full_name[sizeof(file_bar.full_name) - 1] = '\0';
    file_bar.total = total_files;
    file_bar.current = 0;
    file_bar.active = 1;
    file_bar.finished = 0;
    file_bar.style = config.progress_style;
    file_bar.color = config.progress_color;
    file_bar.level = 0;
    pthread_mutex_unlock(&file_bar.lock);
    
    progress_visible = 1;
    display_all_progress();
}

void update_file_progress(size_t current_file) {
    if (config.no_progress_bar) return;
    
    pthread_mutex_lock(&file_bar.lock);
    file_bar.current = current_file;
    time_t now = time(NULL);
    if (now != file_bar.last_update) {
        double elapsed = now - file_bar.last_update;
        if (elapsed > 0) {
            file_bar.speed = (current_file - file_bar.current) / elapsed;
        }
        file_bar.last_update = now;
    }
    pthread_mutex_unlock(&file_bar.lock);
    
    display_all_progress();
}

void finish_file_progress() {
    if (config.no_progress_bar) return;
    
    pthread_mutex_lock(&file_bar.lock);
    file_bar.finished = 1;
    file_bar.active = 0;
    pthread_mutex_unlock(&file_bar.lock);
    
    display_all_progress();
}

void create_overall_progress(const char *name, size_t total_sources) {
    if (config.no_progress_bar) return;
    
    pthread_mutex_lock(&overall_bar.lock);
    strncpy(overall_bar.name, name, sizeof(overall_bar.name) - 1);
    overall_bar.name[sizeof(overall_bar.name) - 1] = '\0';
    overall_bar.total = total_sources;
    overall_bar.current = 0;
    overall_bar.active = 1;
    overall_bar.finished = 0;
    overall_bar.style = config.progress_style;
    overall_bar.color = config.progress_color;
    overall_bar.level = 1;
    pthread_mutex_unlock(&overall_bar.lock);
    
    progress_visible = 1;
    display_all_progress();
}

void update_overall_progress(size_t current_source) {
    if (config.no_progress_bar) return;
    
    pthread_mutex_lock(&overall_bar.lock);
    overall_bar.current = current_source;
    pthread_mutex_unlock(&overall_bar.lock);
    
    display_all_progress();
}

void finish_overall_progress() {
    if (config.no_progress_bar) return;
    
    pthread_mutex_lock(&overall_bar.lock);
    overall_bar.finished = 1;
    overall_bar.active = 0;
    pthread_mutex_unlock(&overall_bar.lock);
    
    display_all_progress();
}

void print_single_progress_bar(const ProgressBar *bar, int max_name_length) {
    if (!bar || (!bar->active && !bar->finished)) return;
    
    double percent = bar->total > 0 ? (double)bar->current / bar->total * 100.0 : 0.0;
    int bar_width = 30;
    int filled = (int)(bar_width * percent / 100.0);
    
    // 颜色设置
    const char *color = "";
    const char *reset = "\033[0m";
    const char *bright = "\033[1m";  // 加亮
    const char *bar_color_start = "";
    
    switch (bar->color) {
        case PROGRESS_COLOR_GREEN: 
            color = "\033[32m"; 
            bar_color_start = "\033[48;5;28m\033[38;5;15m";  // 绿色背景
            break;
        case PROGRESS_COLOR_BLUE: 
            color = "\033[34m"; 
            bar_color_start = "\033[48;5;21m\033[38;5;15m";  // 蓝色背景
            break;
        case PROGRESS_COLOR_YELLOW: 
            color = "\033[33m"; 
            bar_color_start = "\033[48;5;226m\033[38;5;0m";  // 黄色背景
            break;
        case PROGRESS_COLOR_RED: 
            color = "\033[31m"; 
            bar_color_start = "\033[48;5;196m\033[38;5;15m";  // 红色背景
            break;
        case PROGRESS_COLOR_CYAN: 
            color = "\033[36m"; 
            bar_color_start = "\033[48;5;51m\033[38;5;15m";  // 青色背景
            break;
        case PROGRESS_COLOR_MAGENTA: 
            color = "\033[35m"; 
            bar_color_start = "\033[48;5;201m\033[38;5;15m";  // 洋红色背景
            break;
        case PROGRESS_COLOR_RAINBOW: 
            color = "\033[35m"; 
            bar_color_start = "\033[48;5;208m\033[38;5;15m";  // 彩虹色效果（橙色）
            break;
        default: 
            color = "\033[32m"; 
            bar_color_start = "\033[48;5;28m\033[38;5;15m";  // 默认绿色背景
            break;
    }
    
    // 截断名称以适应长度
    char truncated_name[256];
    if (strlen(bar->name) > max_name_length) {
        strncpy(truncated_name, bar->name, max_name_length - 3);
        truncated_name[max_name_length - 3] = '\0';
        strcat(truncated_name, "...");
    } else {
        strcpy(truncated_name, bar->name);
    }
    
    // 根据样式打印进度条
    switch (bar->style) {
        case PROGRESS_STYLE_BAR:
            printf("%s%-*s%s %s%6.2f%%%s ", bright, max_name_length, truncated_name, reset, color, percent, reset);
            
            // 彩色条形进度条
            printf("[");
            for (int i = 0; i < bar_width; i++) {
                if (i < filled) {
                    printf("%s%s%s", bar_color_start, "━", reset);
                } else {
                    printf("─");
                }
            }
            printf("] ");
            printf("%zu/%zu", bar->current, bar->total);
            
            if (bar->speed > 0) {
                printf(" (%.2f/s)", bar->speed);
            }
            
            if (bar->finished) {
                printf(" \033[32m✓\033[0m");
            }
            break;
            
        case PROGRESS_STYLE_RICH:
            printf("%s%-*s%s %s%6.2f%%%s [", bright, max_name_length, truncated_name, reset, color, percent, reset);
            
            for (int i = 0; i < bar_width; i++) {
                if (i < filled) {
                    printf("\033[48;5;28m\033[38;5;15m█\033[0m");  // 绿色背景
                } else {
                    printf("░");
                }
            }
            printf("] ");
            printf("%zu/%zu", bar->current, bar->total);
            
            if (bar->speed > 0) {
                printf(" (%.2f/s)", bar->speed);
            }
            
            if (bar->finished) {
                printf(" \033[32m✓\033[0m");
            }
            break;
            
        case PROGRESS_STYLE_BARS:
        default:
            printf("%s%-*s%s %s%3.0f%%%s [", color, max_name_length, truncated_name, reset, color, percent, reset);
            
            for (int i = 0; i < bar_width; i++) {
                if (i < filled) {
                    printf("=");
                } else {
                    printf("-");
                }
            }
            
            printf("] %zu/%zu", bar->current, bar->total);
            
            if (bar->speed > 0) {
                printf(" (%.2f/s)", bar->speed);
            }
            
            if (bar->finished) {
                printf(" ✅");
            }
            break;
    }
}

void display_all_progress() {
    if (config.no_progress_bar || !progress_visible) return;
    
    // 隐藏当前行（进度条）
    printf("\033[2K\r");
    
    // 显示文件进度
    pthread_mutex_lock(&file_bar.lock);
    if (file_bar.active || file_bar.finished) {
        print_single_progress_bar(&file_bar, 30);
        printf("\n");
        progress_lines = 1;
    }
    pthread_mutex_unlock(&file_bar.lock);
    
    // 显示总体进度
    pthread_mutex_lock(&overall_bar.lock);
    if (overall_bar.active || overall_bar.finished) {
        printf("\033[2K\r"); // 清除当前行
        print_single_progress_bar(&overall_bar, 30);
        printf("\n");
        progress_lines = 2;
    }
    pthread_mutex_unlock(&overall_bar.lock);
    
    fflush(stdout);
}

void hide_progress_temporarily() {
    if (config.progress && !config.no_progress_bar && progress_visible) {
        // 清除进度条行
        for (int i = 0; i < progress_lines; i++) {
            printf("\033[1A\033[2K"); // 上移一行并清除
        }
        fflush(stdout);
        progress_visible = 0;
    }
}

void show_progress_after_log() {
    if (config.progress && !config.no_progress_bar) {
        progress_visible = 1;
        display_all_progress();
    }
}

void cleanup_progress_system() {
    // 清除当前行的进度条
    if (config.progress && !config.no_progress_bar) {
        printf("\r\033[2K"); // 清除当前行
        if (progress_lines > 0) {
            printf("\033[%dA\033[2K", progress_lines); // 清除多行
        }
        fflush(stdout);
    }
    
    pthread_mutex_destroy(&file_bar.lock);
    pthread_mutex_destroy(&overall_bar.lock);
    progress_visible = 0;
    progress_lines = 0;
}