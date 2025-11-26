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

void init_progress_bars() {
    config.progress_bar_count = 0;
    for (int i = 0; i < MAX_PROGRESS_BARS; i++) {
        strcpy(config.progress_bars[i].name, "");
        config.progress_bars[i].current = 0;
        config.progress_bars[i].total = 0;
        config.progress_bars[i].speed = 0.0;
        config.progress_bars[i].last_update = time(NULL);
        config.progress_bars[i].active = 0;
        config.progress_bars[i].finished = 0;
        config.progress_bars[i].style = config.progress_style;
        config.progress_bars[i].color = config.progress_color;
        pthread_mutex_init(&config.progress_bars[i].lock, NULL);
    }
}

void create_progress_bar(const char *name, size_t total, int index) {
    if (index >= MAX_PROGRESS_BARS || config.no_progress_bar) return;
    
    pthread_mutex_lock(&config.progress_bars[index].lock);
    strncpy(config.progress_bars[index].name, name, MAX_PATH - 1);
    config.progress_bars[index].name[MAX_PATH - 1] = '\0';
    config.progress_bars[index].total = total;
    config.progress_bars[index].current = 0;
    config.progress_bars[index].active = 1;
    config.progress_bars[index].finished = 0;
    config.progress_bars[index].style = config.progress_style;
    config.progress_bars[index].color = config.progress_color;
    pthread_mutex_unlock(&config.progress_bars[index].lock);
    
    if (index >= config.progress_bar_count) {
        config.progress_bar_count = index + 1;
    }
}

void update_progress_bar(int index, size_t current) {
    if (index >= MAX_PROGRESS_BARS || config.no_progress_bar) return;
    
    pthread_mutex_lock(&config.progress_bars[index].lock);
    config.progress_bars[index].current = current;
    time_t now = time(NULL);
    if (now != config.progress_bars[index].last_update) {
        double elapsed = now - config.progress_bars[index].last_update;
        if (elapsed > 0) {
            config.progress_bars[index].speed = 
                (current - config.progress_bars[index].current) / elapsed;
        }
        config.progress_bars[index].last_update = now;
    }
    pthread_mutex_unlock(&config.progress_bars[index].lock);
    
    if (config.progress) {
        display_progress_bars();
    }
}

void finish_progress_bar(int index) {
    if (index >= MAX_PROGRESS_BARS || config.no_progress_bar) return;
    
    pthread_mutex_lock(&config.progress_bars[index].lock);
    config.progress_bars[index].finished = 1;
    config.progress_bars[index].active = 0;
    pthread_mutex_unlock(&config.progress_bars[index].lock);
    
    if (config.progress) {
        display_progress_bars();
    }
}

void print_progress_bar(const ProgressBar *bar) {
    if (!bar || !bar->active) return;
    
    double percent = bar->total > 0 ? (double)bar->current / bar->total * 100.0 : 0.0;
    int bar_width = 30;
    int filled = (int)(bar_width * percent / 100.0);
    
    // 颜色设置
    const char *color = "";
    const char *reset = "\033[0m";
    switch (bar->color) {
        case PROGRESS_COLOR_GREEN: color = "\033[32m"; break;
        case PROGRESS_COLOR_BLUE: color = "\033[34m"; break;
        case PROGRESS_COLOR_YELLOW: color = "\033[33m"; break;
        case PROGRESS_COLOR_RED: color = "\033[31m"; break;
        case PROGRESS_COLOR_CYAN: color = "\033[36m"; break;
        case PROGRESS_COLOR_MAGENTA: color = "\033[35m"; break;
        case PROGRESS_COLOR_RAINBOW: color = "\033[35m"; break; // 简化彩虹色
        default: color = "\033[32m"; break; // 默认绿色
    }
    
    // 进度条样式
    const char *start_bracket = "[";
    const char *end_bracket = "]";
    const char *fill_char = "=";
    const char *empty_char = "-";
    
    switch (bar->style) {
        case PROGRESS_STYLE_BARS:
            start_bracket = "[";
            end_bracket = "]";
            fill_char = "=";
            empty_char = "-";
            break;
        case PROGRESS_STYLE_DOTS:
            start_bracket = "(";
            end_bracket = ")";
            fill_char = "•";
            empty_char = "°";
            break;
        case PROGRESS_STYLE_UNICODE:
            start_bracket = "⌊";
            end_bracket = "⌋";
            fill_char = "█";
            empty_char = "░";
            break;
        case PROGRESS_STYLE_ASCII:
            start_bracket = "|";
            end_bracket = "|";
            fill_char = "#";
            empty_char = " ";
            break;
        default:
            start_bracket = "[";
            end_bracket = "]";
            fill_char = "=";
            empty_char = "-";
            break;
    }
    
    printf("%s%-20s%s %s%3.0f%%%s %s", color, bar->name, reset, color, percent, reset, start_bracket);
    
    for (int i = 0; i < bar_width; i++) {
        if (i < filled) {
            printf("%s", fill_char);
        } else {
            printf("%s", empty_char);
        }
    }
    
    printf("%s %zu/%zu", end_bracket, bar->current, bar->total);
    
    if (bar->speed > 0) {
        printf(" (%.2f/s)", bar->speed);
    }
    
    if (bar->finished) {
        printf(" ✅");
    }
    
    printf("\n");
}

void display_progress_bars() {
    if (config.no_progress_bar) return;
    
    printf("\033[2J\033[H"); // 清屏并回到顶部
    
    for (int i = 0; i < config.progress_bar_count; i++) {
        pthread_mutex_lock(&config.progress_bars[i].lock);
        if (config.progress_bars[i].active || config.progress_bars[i].finished) {
            print_progress_bar(&config.progress_bars[i]);
        }
        pthread_mutex_unlock(&config.progress_bars[i].lock);
    }
    
    fflush(stdout);
}

void cleanup_progress_bars() {
    for (int i = 0; i < MAX_PROGRESS_BARS; i++) {
        pthread_mutex_destroy(&config.progress_bars[i].lock);
    }
    config.progress_bar_count = 0;
}