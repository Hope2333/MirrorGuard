#include "tui.h"
#include "config.h"
#include "logging.h"
#include "progress.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <time.h>

extern Config config;
extern Statistics stats;

// 终端控制结构
static struct termios orig_termios;

// 函数声明
static int kbhit(void);

int is_tui_enabled() {
    return config.tui_mode != TUI_MODE_NONE;
}

void init_tui() {
    if (config.tui_mode == TUI_MODE_NONE) return;

    // 保存原始终端设置
    tcgetattr(STDIN_FILENO, &orig_termios);

    // 设置终端为非规范模式
    struct termios new_termios = orig_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_termios);

    // 清屏
    printf("\033[2J\033[H");
    fflush(stdout);
}

void cleanup_tui() {
    if (config.tui_mode == TUI_MODE_NONE) return;

    // 恢复原始终端设置
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);

    // 清屏并恢复光标
    printf("\033[2J\033[H");
    printf("\033[?25h"); // 显示光标
    fflush(stdout);
}

void run_tui() {
    if (config.tui_mode == TUI_MODE_NONE) return;

    switch (config.tui_mode) {
        case TUI_MODE_SIMPLE:
            run_simple_tui();
            break;
        case TUI_MODE_ADVANCED:
            run_advanced_tui();
            break;
        case TUI_MODE_MINIMAL:
            run_minimal_tui();
            break;
        case TUI_MODE_RICH:
            run_rich_tui();
            break;
        case TUI_MODE_DEBUG:
            run_debug_tui();
            break;
        default:
            run_simple_tui();
            break;
    }
}

void run_simple_tui() {
    printf("\033[2J\033[H"); // 清屏
    printf("=== MirrorGuard TUI - Simple Mode ===\n");
    printf("Press 'q' to quit, 'r' to refresh\n\n");

    while (!g_interrupted) {
        render_simple_ui();
        struct timespec ts = {0, 100000000}; // 100ms
        nanosleep(&ts, NULL);

        // 检查按键
        if (kbhit()) {
            char ch = getchar();
            if (ch == 'q' || ch == 'Q') {
                break;
            }
        }
    }
}

void run_advanced_tui() {
    printf("\033[2J\033[H"); // 清屏
    printf("=== MirrorGuard TUI - Advanced Mode ===\n");
    printf("Press 'q' to quit, 'r' to refresh, 'h' for help\n\n");

    while (!g_interrupted) {
        render_advanced_ui();
        struct timespec ts = {0, 100000000}; // 100ms
        nanosleep(&ts, NULL);

        if (kbhit()) {
            char ch = getchar();
            if (ch == 'q' || ch == 'Q') {
                break;
            } else if (ch == 'h' || ch == 'H') {
                printf("\nAdvanced TUI Help:\n");
                printf("  q - Quit\n");
                printf("  r - Refresh\n");
                printf("  h - Help\n");
                printf("  Arrows - Navigate\n");
                printf("  Enter - Select\n");
            }
        }
    }
}

void run_minimal_tui() {
    printf("\033[2J\033[H"); // 清屏
    printf("=== MirrorGuard TUI - Minimal Mode ===\n");

    while (!g_interrupted) {
        render_minimal_ui();
        struct timespec ts = {0, 200000000}; // 200ms
        nanosleep(&ts, NULL);
    }
}

void run_rich_tui() {
    printf("\033[2J\033[H"); // 清屏
    printf("=== MirrorGuard TUI - Rich Mode ===\n");

    while (!g_interrupted) {
        render_rich_ui();
        struct timespec ts = {0, 100000000}; // 100ms
        nanosleep(&ts, NULL);
    }
}

void run_debug_tui() {
    printf("\033[2J\033[H"); // 清屏
    printf("=== MirrorGuard TUI - Debug Mode ===\n");

    while (!g_interrupted) {
        render_debug_ui();
        struct timespec ts = {0, 50000000}; // 50ms
        nanosleep(&ts, NULL);
    }
}

void render_simple_ui() {
    printf("\033[2;1H"); // 移动到第2行第1列
    printf("MirrorGuard - File Integrity Verification Tool\n");
    printf("Operation: ");
    if (config.generate_mode) printf("Generating Manifest");
    else if (config.verify_mode) printf("Verifying Mirror");
    else if (config.compare_mode) printf("Comparing Manifests");
    else if (config.direct_compare_mode) printf("Comparing Directories");
    else printf("Idle");

    printf("\nProgress Bars:\n");
    for (int i = 0; i < config.progress_bar_count && i < MAX_PROGRESS_BARS; i++) {
        pthread_mutex_lock(&config.progress_bars[i].lock);
        if (config.progress_bars[i].active || config.progress_bars[i].finished) {
            double percent = config.progress_bars[i].total > 0 ?
                (double)config.progress_bars[i].current / config.progress_bars[i].total * 100.0 : 0.0;

            printf("  [%3.0f%%] %-20s %zu/%zu\n",
                   percent,
                   config.progress_bars[i].name,
                   config.progress_bars[i].current,
                   config.progress_bars[i].total);
        }
        pthread_mutex_unlock(&config.progress_bars[i].lock);
    }

    printf("\nStatistics:\n");
    printf("  Processed: %zu files\n", stats.processed_files);
    printf("  Missing: %zu files\n", stats.missing_files);
    printf("  Corrupt: %zu files\n", stats.corrupt_files);
    printf("  Extra: %zu files\n", stats.extra_files);

    printf("\nPress 'q' to quit\n");
    fflush(stdout);
}

void render_advanced_ui() {
    printf("\033[2;1H"); // 移动到第2行第1列

    // 标题栏
    printf("\033[44m\033[37m"); // 蓝色背景，白色文字
    printf(" %-76s \033[0m\n", "MirrorGuard - Advanced TUI Mode");

    // 主界面
    printf("\033[36mOperation:\033[0m ");
    if (config.generate_mode) printf("Generating Manifest");
    else if (config.verify_mode) printf("Verifying Mirror");
    else if (config.compare_mode) printf("Comparing Manifests");
    else if (config.direct_compare_mode) printf("Comparing Directories");
    else printf("Idle");
    printf("\n\n");

    // 进度条区域
    printf("\033[33mProgress Bars:\033[0m\n");
    for (int i = 0; i < config.progress_bar_count && i < MAX_PROGRESS_BARS; i++) {
        pthread_mutex_lock(&config.progress_bars[i].lock);
        if (config.progress_bars[i].active || config.progress_bars[i].finished) {
            double percent = config.progress_bars[i].total > 0 ?
                (double)config.progress_bars[i].current / config.progress_bars[i].total * 100.0 : 0.0;

            int bar_width = 40;
            int filled = (int)(bar_width * percent / 100.0);

            printf("  \033[32m%-15s\033[0m [", config.progress_bars[i].name);
            for (int j = 0; j < bar_width; j++) {
                if (j < filled) printf("█");
                else printf("░");
            }
            printf("] %3.0f%% (%zu/%zu)\n", percent,
                   config.progress_bars[i].current, config.progress_bars[i].total);
        }
        pthread_mutex_unlock(&config.progress_bars[i].lock);
    }

    // 统计区域
    printf("\n\033[35mStatistics:\033[0m\n");
    printf("  \033[32mProcessed:\033[0m %8zu files  ", stats.processed_files);
    printf("\033[31mMissing:\033[0m %8zu files\n", stats.missing_files);
    printf("  \033[33mCorrupt:\033[0m   %8zu files  ", stats.corrupt_files);
    printf("\033[36mExtra:\033[0m   %8zu files\n", stats.extra_files);

    // 状态栏
    printf("\n\033[40m\033[37m"); // 黑色背景，白色文字
    printf(" %-76s \033[0m", "Commands: q-Quit h-Help r-Refresh");
    fflush(stdout);
}

void render_minimal_ui() {
    printf("\033[2J\033[H"); // 清屏
    printf("MG ");

    // 简单进度显示
    for (int i = 0; i < config.progress_bar_count && i < MAX_PROGRESS_BARS; i++) {
        pthread_mutex_lock(&config.progress_bars[i].lock);
        if (config.progress_bars[i].active) {
            double percent = config.progress_bars[i].total > 0 ?
                (double)config.progress_bars[i].current / config.progress_bars[i].total * 100.0 : 0.0;
            printf("[%3.0f%%] ", percent);
        }
        pthread_mutex_unlock(&config.progress_bars[i].lock);
    }

    printf("P:%zu M:%zu C:%zu E:%zu",
           stats.processed_files, stats.missing_files,
           stats.corrupt_files, stats.extra_files);
    fflush(stdout);
}

void render_rich_ui() {
    printf("\033[2J\033[H"); // 清屏

    // 富文本标题
    printf("\n\033[1;38;5;208m"); // 加粗，橙色
    printf("╔════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                           \033[1;38;5;45mMIRRORGUARD\033[1;38;5;208m                                    ║\n");
    printf("║                     \033[2;38;5;245mEnterprise File Integrity Tool\033[1;38;5;208m                      ║\n");
    printf("╚════════════════════════════════════════════════════════════════════════════╝\033[0m\n");

    // 状态区域
    printf("\n\033[1;37mStatus:\033[0m ");
    if (config.generate_mode) printf("\033[33mGenerating Manifest\033[0m");
    else if (config.verify_mode) printf("\033[32mVerifying Mirror\033[0m");
    else if (config.compare_mode) printf("\033[34mComparing Manifests\033[0m");
    else if (config.direct_compare_mode) printf("\033[36mComparing Directories\033[0m");
    else printf("\033[37mIdle\033[0m");

    // 进度条区域
    printf("\n\n\033[1;37mProgress:\033[0m\n");
    for (int i = 0; i < config.progress_bar_count && i < MAX_PROGRESS_BARS; i++) {
        pthread_mutex_lock(&config.progress_bars[i].lock);
        if (config.progress_bars[i].active || config.progress_bars[i].finished) {
            double percent = config.progress_bars[i].total > 0 ?
                (double)config.progress_bars[i].current / config.progress_bars[i].total * 100.0 : 0.0;

            // 彩色进度条
            int bar_width = 50;
            int filled = (int)(bar_width * percent / 100.0);

            printf("  \033[38;5;208m%-15s\033[0m ", config.progress_bars[i].name);
            printf("\033[48;5;235m"); // 背景色
            for (int j = 0; j < bar_width; j++) {
                if (j < filled) {
                    if (j < bar_width * 0.3) printf("\033[38;5;196m█\033[48;5;235m"); // 红色
                    else if (j < bar_width * 0.7) printf("\033[38;5;226m█\033[48;5;235m"); // 黄色
                    else printf("\033[38;5;46m█\033[48;5;235m"); // 绿色
                } else {
                    printf("░");
                }
            }
            printf("\033[0m %6.2f%% (%zu/%zu)\n", percent,
                   config.progress_bars[i].current, config.progress_bars[i].total);
        }
        pthread_mutex_unlock(&config.progress_bars[i].lock);
    }

    // 统计区域
    printf("\n\033[1;37mStatistics:\033[0m\n");
    printf("  \033[32mProcessed:\033[0m %8zu files  ", stats.processed_files);
    printf("\033[31mMissing:\033[0m %8zu files\n", stats.missing_files);
    printf("  \033[33mCorrupt:\033[0m   %8zu files  ", stats.corrupt_files);
    printf("\033[36mExtra:\033[0m   %8zu files\n", stats.extra_files);

    fflush(stdout);
}

void render_debug_ui() {
    printf("\033[2J\033[H"); // 清屏
    printf("\033[35m=== DEBUG TUI MODE ===\033[0m\n\n");

    printf("Config:\n");
    printf("  tui_mode: %d\n", config.tui_mode);
    printf("  progress: %d\n", config.progress);
    printf("  quiet: %d\n", config.quiet);
    printf("  verbose: %d\n", config.verbose);
    printf("  threads: %d\n", config.threads);

    printf("\nOperation Modes:\n");
    printf("  generate: %d\n", config.generate_mode);
    printf("  verify: %d\n", config.verify_mode);
    printf("  compare: %d\n", config.compare_mode);
    printf("  diff: %d\n", config.direct_compare_mode);

    printf("\nProgress Bars (%d):\n", config.progress_bar_count);
    for (int i = 0; i < config.progress_bar_count && i < MAX_PROGRESS_BARS; i++) {
        pthread_mutex_lock(&config.progress_bars[i].lock);
        printf("  [%d] '%s' - %zu/%zu (%s)\n",
               i,
               config.progress_bars[i].name,
               config.progress_bars[i].current,
               config.progress_bars[i].total,
               config.progress_bars[i].active ? "active" :
               config.progress_bars[i].finished ? "finished" : "inactive");
        pthread_mutex_unlock(&config.progress_bars[i].lock);
    }

    printf("\nStatistics:\n");
    printf("  processed: %zu\n", stats.processed_files);
    printf("  missing: %zu\n", stats.missing_files);
    printf("  corrupt: %zu\n", stats.corrupt_files);
    printf("  extra: %zu\n", stats.extra_files);
    printf("  errors: %zu\n", stats.error_files);
    printf("  bytes: %zu\n", stats.bytes_processed);

    printf("\nInterrupted: %d\n", g_interrupted);
    printf("\nPress Ctrl+C to exit\n");
    fflush(stdout);
}

// 检查键盘输入
static int kbhit() {
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if(ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
}
