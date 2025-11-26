// src/tui.c (Corrected to access progress bars via Config struct)
#define _XOPEN_SOURCE 600
#include "tui.h"
#include "config.h"  // Includes the declaration of g_interrupted and Config
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
// Add the extern declaration for g_interrupted
extern volatile sig_atomic_t g_interrupted;

// Terminal control structure
static struct termios orig_termios;

// Check keyboard input - Make static
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

int is_tui_enabled() {
    return config.tui_mode != TUI_MODE_NONE;
}

void init_tui() {
    if (config.tui_mode == TUI_MODE_NONE) return;

    // Save original terminal settings
    tcgetattr(STDIN_FILENO, &orig_termios);

    // Set terminal to non-canonical mode
    struct termios new_termios = orig_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_termios);

    // Clear screen
    printf("\033[2J\033[H");
    fflush(stdout);
}

void cleanup_tui() {
    if (config.tui_mode == TUI_MODE_NONE) return;

    // Restore original terminal settings
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);

    // Clear screen and restore cursor
    printf("\033[2J\033[H");
    printf("\033[?25h"); // Show cursor
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
    printf("\033[2J\033[H"); // Clear screen
    printf("=== MirrorGuard TUI - Simple Mode ===\n");
    printf("Press 'q' to quit, 'r' to refresh\n\n");

    while (!g_interrupted) { // Now g_interrupted is declared
        render_simple_ui();
        struct timespec ts = {0, 100000000}; // 100ms
        nanosleep(&ts, NULL);

        // Check key press
        if (kbhit()) {
            char ch = getchar();
            if (ch == 'q' || ch == 'Q') {
                break;
            }
        }
    }
}

void run_advanced_tui() {
    printf("\033[2J\033[H"); // Clear screen
    printf("=== MirrorGuard TUI - Advanced Mode ===\n");
    printf("Press 'q' to quit, 'r' to refresh, 'h' for help\n\n");

    while (!g_interrupted) { // Now g_interrupted is declared
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
    printf("\033[2J\033[H"); // Clear screen
    printf("=== MirrorGuard TUI - Minimal Mode ===\n");

    while (!g_interrupted) { // Now g_interrupted is declared
        render_minimal_ui();
        struct timespec ts = {0, 200000000}; // 200ms
        nanosleep(&ts, NULL);
    }
}

void run_rich_tui() {
    printf("\033[2J\033[H"); // Clear screen
    printf("=== MirrorGuard TUI - Rich Mode ===\n");

    while (!g_interrupted) { // Now g_interrupted is declared
        render_rich_ui();
        struct timespec ts = {0, 100000000}; // 100ms
        nanosleep(&ts, NULL);
    }
}

void run_debug_tui() {
    printf("\033[2J\033[H"); // Clear screen
    printf("=== MirrorGuard TUI - Debug Mode ===\n");

    while (!g_interrupted) { // Now g_interrupted is declared
        render_debug_ui();
        struct timespec ts = {0, 50000000}; // 50ms
        nanosleep(&ts, NULL);
    }
}

void render_simple_ui() {
    printf("\033[2;1H"); // Move to row 2, column 1
    printf("MirrorGuard - File Integrity Verification Tool\n");
    printf("Operation: ");
    if (config.generate_mode) printf("Generating Manifest");
    else if (config.verify_mode) printf("Verifying Mirror");
    else if (config.compare_mode) printf("Comparing Manifests");
    else if (config.direct_compare_mode) printf("Comparing Directories");
    else printf("Idle");

    printf("\nProgress Bars:\n");
    // Iterate through the progress bars in the config array
    for (int i = 0; i < config.progress_bar_count && i < MAX_PROGRESS_BARS; i++) {
        pthread_mutex_lock(&config.progress_bars[i].lock);
        if (config.progress_bars[i].active || config.progress_bars[i].finished) {
            double percent = config.progress_bars[i].total > 0 ?
                (double)config.progress_bars[i].current / config.progress_bars[i].total * 100.0 : 0.0;
            printf("  [%3.0f%%] %-20s %zu/%zu\n",
                   percent, config.progress_bars[i].name, config.progress_bars[i].current, config.progress_bars[i].total);
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
    printf("\033[2;1H"); // Move to row 2, column 1

    // Title bar
    printf("\033[44m\033[37m"); // Blue background, white text
    printf(" %-76s \033[0m\n", "MirrorGuard - Advanced TUI Mode");

    // Main interface
    printf("\033[36mOperation:\033[0m ");
    if (config.generate_mode) printf("Generating Manifest");
    else if (config.verify_mode) printf("Verifying Mirror");
    else if (config.compare_mode) printf("Comparing Manifests");
    else if (config.direct_compare_mode) printf("Comparing Directories");
    else printf("Idle");
    printf("\n\n");

    // Progress bar area
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


    // Statistics area
    printf("\n\033[35mStatistics:\033[0m\n");
    printf("  \033[32mProcessed:\033[0m %8zu files  ", stats.processed_files);
    printf("\033[31mMissing:\033[0m %8zu files\n", stats.missing_files);
    printf("  \033[33mCorrupt:\033[0m   %8zu files  ", stats.corrupt_files);
    printf("\033[36mExtra:\033[0m   %8zu files\n", stats.extra_files);

    // Status bar
    printf("\n\033[40m\033[37m"); // Black background, white text
    printf(" %-76s \033[0m", "Commands: q-Quit h-Help r-Refresh");
    fflush(stdout);
}

void render_minimal_ui() {
    printf("\033[2J\033[H"); // Clear screen
    printf("MG ");

    // Simple progress display - iterate config's array
    for (int i = 0; i < config.progress_bar_count && i < MAX_PROGRESS_BARS; i++) {
        pthread_mutex_lock(&config.progress_bars[i].lock);
        if (config.progress_bars[i].active) { // Only show active bars in minimal mode
            double percent = config.progress_bars[i].total > 0 ?
                (double)config.progress_bars[i].current / config.progress_bars[i].total * 100.0 : 0.0;
            printf("[%s: %3.0f%%] ", config.progress_bars[i].name, percent); // Show name and percentage
        }
        pthread_mutex_unlock(&config.progress_bars[i].lock);
    }

    printf("P:%zu M:%zu C:%zu E:%zu",
           stats.processed_files, stats.missing_files,
           stats.corrupt_files, stats.extra_files);
    fflush(stdout);
}

void render_rich_ui() {
    printf("\033[2J\033[H"); // Clear screen

    // Rich text title
    printf("\n\033[1;38;5;208m"); // Bold, Orange
    printf("╔════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                           \033[1;38;5;45mMIRRORGUARD\033[1;38;5;208m                                    ║\n");
    printf("║                     \033[2;38;5;245mEnterprise File Integrity Tool\033[1;38;5;208m                      ║\n");
    printf("╚════════════════════════════════════════════════════════════════════════════╝\033[0m\n");

    // Status area
    printf("\n\033[1;37mStatus:\033[0m ");
    if (config.generate_mode) printf("\033[33mGenerating Manifest\033[0m");
    else if (config.verify_mode) printf("\033[32mVerifying Mirror\033[0m");
    else if (config.compare_mode) printf("\033[34mComparing Manifests\033[0m");
    else if (config.direct_compare_mode) printf("\033[36mComparing Directories\033[0m");
    else printf("\033[37mIdle\033[0m");

    // Progress bar area
    printf("\n\n\033[1;37mProgress:\033[0m\n");
    for (int i = 0; i < config.progress_bar_count && i < MAX_PROGRESS_BARS; i++) {
        pthread_mutex_lock(&config.progress_bars[i].lock);
        if (config.progress_bars[i].active || config.progress_bars[i].finished) {
            double percent = config.progress_bars[i].total > 0 ?
                (double)config.progress_bars[i].current / config.progress_bars[i].total * 100.0 : 0.0;

            // Colorful progress bar
            int bar_width = 50;
            int filled = (int)(bar_width * percent / 100.0);

            printf("  \033[38;5;208m%-15s\033[0m ", config.progress_bars[i].name);
            printf("\033[48;5;235m"); // Background color
            for (int j = 0; j < bar_width; j++) {
                if (j < filled) {
                    if (j < bar_width * 0.3) printf("\033[38;5;196m█\033[48;5;235m"); // Red
                    else if (j < bar_width * 0.7) printf("\033[38;5;226m█\033[48;5;235m"); // Yellow
                    else printf("\033[38;5;46m█\033[48;5;235m"); // Green
                } else {
                    printf("░");
                }
            }
            printf("\033[0m %6.2f%% (%zu/%zu)\n", percent,
                   config.progress_bars[i].current, config.progress_bars[i].total);
        }
        pthread_mutex_unlock(&config.progress_bars[i].lock);
    }


    // Statistics area
    printf("\n\033[1;37mStatistics:\033[0m\n");
    printf("  \033[32mProcessed:\033[0m %8zu files  ", stats.processed_files);
    printf("\033[31mMissing:\033[0m %8zu files\n", stats.missing_files);
    printf("  \033[33mCorrupt:\033[0m   %8zu files  ", stats.corrupt_files);
    printf("\033[36mExtra:\033[0m   %8zu files\n", stats.extra_files);

    fflush(stdout);
}

void render_debug_ui() {
    printf("\033[2J\033[H"); // Clear screen
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

    printf("\nProgress Bars (from Config array, count: %d):\n", config.progress_bar_count);
    for (int i = 0; i < config.progress_bar_count && i < MAX_PROGRESS_BARS; i++) {
        pthread_mutex_lock(&config.progress_bars[i].lock);
        printf("  [%d] '%s' - %zu/%zu (%s) Speed: %.2f/s\n",
               i,
               config.progress_bars[i].name,
               config.progress_bars[i].current,
               config.progress_bars[i].total,
               config.progress_bars[i].active ? "active" :
               config.progress_bars[i].finished ? "finished" : "inactive",
               config.progress_bars[i].speed);
        pthread_mutex_unlock(&config.progress_bars[i].lock);
    }


    printf("\nStatistics:\n");
    printf("  processed: %zu\n", stats.processed_files);
    printf("  missing: %zu\n", stats.missing_files);
    printf("  corrupt: %zu\n", stats.corrupt_files);
    printf("  extra: %zu\n", stats.extra_files);
    printf("  errors: %zu\n", stats.error_files);
    printf("  bytes: %zu\n", stats.bytes_processed);

    printf("\nInterrupted: %d\n", g_interrupted); // Now g_interrupted is declared
    printf("\nPress Ctrl+C to exit\n");
    fflush(stdout);
}
