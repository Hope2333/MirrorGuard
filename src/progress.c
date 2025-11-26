// src/progress.c (Revised to use Config's progress bar array)
#include "progress.h"
#include "logging.h"
#include "config.h" // Include config.h to access the global config variable
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

// Access the global config variable declared in config.c
extern Config config;
extern Statistics stats; // Also access stats if needed for display

// Indices to track the main file and overall progress bars within the config array
static int file_bar_index = -1;
static int overall_bar_index = -1;

// Static variables to track previous values for speed calculation per bar
// We'll need an array or a way to map these to the index in config.progress_bars
// For simplicity with current static approach, let's keep static tracking for the two main bars
// A more general solution would require a parallel array or struct member for speed tracking.
static size_t file_bar_prev_count = 0;
static size_t overall_bar_prev_count = 0;
static time_t file_bar_prev_time = 0;
static time_t overall_bar_prev_time = 0;

void init_progress_system() {
    // Initialize the global progress bar count
    config.progress_bar_count = 0;
    file_bar_index = -1;
    overall_bar_index = -1;

    // Initialize static tracking variables
    file_bar_prev_count = 0;
    overall_bar_prev_count = 0;
    file_bar_prev_time = 0;
    overall_bar_prev_time = 0;

    // Initialize any mutexes if needed for the global array (though individual bars have locks)
    // For now, assume individual bar locks are sufficient if array access is managed carefully.
}

void create_file_progress(const char *filename, size_t total_files) {
    if (config.no_progress_bar) return;

    // Find a slot in the config's progress bar array
    int idx = -1;
    pthread_mutex_lock(&config.progress_bars[config.progress_bar_count].lock); // Lock first available slot
    if (config.progress_bar_count < MAX_PROGRESS_BARS) {
        idx = config.progress_bar_count++;
        file_bar_index = idx; // Remember the index for this main file bar
    }
    if (idx == -1) {
         log_msg(LOG_ERROR, "Progress bar array full, cannot create file progress for: %s", filename);
         pthread_mutex_unlock(&config.progress_bars[config.progress_bar_count-1].lock);
         return; // Or handle error appropriately
    }

    ProgressBar *bar = &config.progress_bars[idx];

    // Truncate filename for display name
    snprintf(bar->name, sizeof(bar->name) - 1, "%.79s", filename); // 79 to leave room for null terminator
    // Use full path for logs if needed, but for display, the truncated name is used
    strncpy(bar->full_name, filename, sizeof(bar->full_name) - 1);
    bar->full_name[sizeof(bar->full_name) - 1] = '\0';

    bar->total = total_files;
    bar->current = 0;
    bar->active = 1;
    bar->finished = 0;
    bar->style = config.progress_style;
    bar->color = config.progress_color;
    bar->level = 0; // Detail level

    // Initialize speed tracking for this new bar
    bar->speed = 0.0;
    // Remove 'volatile' qualifier when calling time()
    time((time_t*)&bar->last_update); // Cast away volatile for time() function

    pthread_mutex_unlock(&bar->lock);

    // Update static tracking for this specific bar (file bar)
    file_bar_prev_count = 0;
    time(&file_bar_prev_time);

    display_all_progress();
}

void update_file_progress(size_t current_file) {
    if (config.no_progress_bar || file_bar_index == -1) return;

    int idx = file_bar_index;
    ProgressBar *bar = &config.progress_bars[idx];
    pthread_mutex_lock(&bar->lock);

    time_t now;
    time(&now); // Get current time
    // Calculate speed based on change since last update for this specific bar
    if (now != bar->last_update) { // Check if time actually passed
        double elapsed = now - bar->last_update; // Use bar's last_update time
        if (elapsed > 0 && current_file > bar->current) { // Check for progress
             double processed = (double)(current_file - bar->current); // Use bar's current value as baseline
             bar->speed = processed / elapsed;
        } else {
             bar->speed = 0.0; // No progress or no time elapsed
        }
        // Update bar's internal time tracking
        bar->last_update = now;
    }
    bar->current = current_file;
    pthread_mutex_unlock(&bar->lock);

    display_all_progress();
}

void finish_file_progress() {
    if (config.no_progress_bar || file_bar_index == -1) return;

    int idx = file_bar_index;
    ProgressBar *bar = &config.progress_bars[idx];
    pthread_mutex_lock(&bar->lock);
    bar->finished = 1;
    bar->active = 0;
    pthread_mutex_unlock(&bar->lock);

    display_all_progress();
}

void create_overall_progress(const char *name, size_t total_sources) {
    if (config.no_progress_bar) return;

    // Find a slot in the config's progress bar array
    int idx = -1;
    pthread_mutex_lock(&config.progress_bars[config.progress_bar_count].lock); // Lock first available slot
    if (config.progress_bar_count < MAX_PROGRESS_BARS) {
        idx = config.progress_bar_count++;
        overall_bar_index = idx; // Remember the index for this overall bar
    }
    if (idx == -1) {
         log_msg(LOG_ERROR, "Progress bar array full, cannot create overall progress for: %s", name);
         pthread_mutex_unlock(&config.progress_bars[config.progress_bar_count-1].lock);
         return; // Or handle error appropriately
    }

    ProgressBar *bar = &config.progress_bars[idx];

    strncpy(bar->name, name, sizeof(bar->name) - 1);
    bar->name[sizeof(bar->name) - 1] = '\0';
    strncpy(bar->full_name, name, sizeof(bar->full_name) - 1);
    bar->full_name[sizeof(bar->full_name) - 1] = '\0';

    bar->total = total_sources;
    bar->current = 0;
    bar->active = 1;
    bar->finished = 0;
    bar->style = config.progress_style;
    bar->color = config.progress_color;
    bar->level = 1; // Overall level

    // Initialize speed tracking for this new bar
    bar->speed = 0.0;
    // Remove 'volatile' qualifier when calling time()
    time((time_t*)&bar->last_update); // Cast away volatile for time() function

    pthread_mutex_unlock(&bar->lock);

    // Update static tracking for this specific bar (overall bar)
    overall_bar_prev_count = 0;
    time(&overall_bar_prev_time);

    display_all_progress();
}

void update_overall_progress(size_t current_source) {
    if (config.no_progress_bar || overall_bar_index == -1) return;

    int idx = overall_bar_index;
    ProgressBar *bar = &config.progress_bars[idx];
    pthread_mutex_lock(&bar->lock);

    time_t now;
    time(&now); // Get current time
    // Calculate speed based on change since last update for this specific bar
    if (now != bar->last_update) { // Check if time actually passed
        double elapsed = now - bar->last_update; // Use bar's last_update time
        if (elapsed > 0 && current_source > bar->current) { // Check for progress
             double processed = (double)(current_source - bar->current); // Use bar's current value as baseline
             bar->speed = processed / elapsed;
        } else {
             bar->speed = 0.0; // No progress or no time elapsed
        }
        // Update bar's internal time tracking
        bar->last_update = now;
    }
    bar->current = current_source;
    pthread_mutex_unlock(&bar->lock);

    display_all_progress();
}

void finish_overall_progress() {
    if (config.no_progress_bar || overall_bar_index == -1) return;

    int idx = overall_bar_index;
    ProgressBar *bar = &config.progress_bars[idx];
    pthread_mutex_lock(&bar->lock);
    bar->finished = 1;
    bar->active = 0;
    pthread_mutex_unlock(&bar->lock);

    display_all_progress();
}

void print_single_progress_bar(const ProgressBar *bar, int max_name_length) {
    if (!bar || (!bar->active && !bar->finished)) return;

    double percent = bar->total > 0 ? (double)bar->current / bar->total * 100.0 : 0.0;
    int bar_width = 30;
    int filled = (int)(bar_width * percent / 100.0);

    // Color settings
    const char *color = "";
    const char *reset = "\033[0m";
    const char *bright = "\033[1m";  // Bright
    const char *bar_color_start = "";

    switch (bar->color) {
        case PROGRESS_COLOR_GREEN:
            color = "\033[32m";
            bar_color_start = "\033[48;5;28m\033[38;5;15m";  // Green background
            break;
        case PROGRESS_COLOR_BLUE:
            color = "\033[34m";
            bar_color_start = "\033[48;5;21m\033[38;5;15m";  // Blue background
            break;
        case PROGRESS_COLOR_YELLOW:
            color = "\033[33m";
            bar_color_start = "\033[48;5;226m\033[38;5;0m";  // Yellow background
            break;
        case PROGRESS_COLOR_RED:
            color = "\033[31m";
            bar_color_start = "\033[48;5;196m\033[38;5;15m";  // Red background
            break;
        case PROGRESS_COLOR_CYAN:
            color = "\033[36m";
            bar_color_start = "\033[48;5;51m\033[38;5;15m";  // Cyan background
            break;
        case PROGRESS_COLOR_MAGENTA:
            color = "\033[35m";
            bar_color_start = "\033[48;5;201m\033[38;5;15m";  // Magenta background
            break;
        case PROGRESS_COLOR_RAINBOW:
            color = "\033[35m";
            bar_color_start = "\033[48;5;208m\033[38;5;15m";  // Rainbow effect (orange)
            break;
        default:
            color = "\033[32m";
            bar_color_start = "\033[48;5;28m\033[38;5;15m";  // Default green background
            break;
    }

    // Truncate name to fit length - Fix the warning by casting strlen result
    char truncated_name[256];
    if ((int)strlen(bar->name) > max_name_length) { // Cast size_t to int for comparison
        strncpy(truncated_name, bar->name, max_name_length - 3);
        truncated_name[max_name_length - 3] = '\0';
        strcat(truncated_name, "...");
    } else {
        strcpy(truncated_name, bar->name);
    }

    // Print progress bar based on style
    switch (bar->style) {
        case PROGRESS_STYLE_BAR:
            printf("%s%-*s%s %s%6.2f%%%s ", bright, max_name_length, truncated_name, reset, color, percent, reset);

            // Colorful bar progress
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
                    printf("\033[48;5;28m\033[38;5;15m█\033[0m");  // Green background
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
    if (config.no_progress_bar) return;

    // Hide current line (progress bar)
    printf("\033[2K\r");

    // Iterate through the progress bars in the config array and display active/finished ones
    for (int i = 0; i < config.progress_bar_count && i < MAX_PROGRESS_BARS; i++) {
        pthread_mutex_lock(&config.progress_bars[i].lock);
        if (config.progress_bars[i].active || config.progress_bars[i].finished) {
            // Use a reasonable max name length for display
            print_single_progress_bar(&config.progress_bars[i], 30);
            printf("\n"); // Add a newline for each bar displayed
        }
        pthread_mutex_unlock(&config.progress_bars[i].lock);
    }

    fflush(stdout);
}

void hide_progress_temporarily() {
    if (config.progress && !config.no_progress_bar) {
        // Calculate how many lines need to be cleared
        int lines_to_clear = 0;
        for (int i = 0; i < config.progress_bar_count && i < MAX_PROGRESS_BARS; i++) {
            pthread_mutex_lock(&config.progress_bars[i].lock);
            if (config.progress_bars[i].active || config.progress_bars[i].finished) {
                lines_to_clear++;
            }
            pthread_mutex_unlock(&config.progress_bars[i].lock);
        }
        // Clear the lines
        for (int i = 0; i < lines_to_clear; i++) {
            printf("\033[1A\033[2K"); // Move up one line and clear
        }
        fflush(stdout);
    }
}

void show_progress_after_log() {
    if (config.progress && !config.no_progress_bar) {
        display_all_progress();
    }
}

void cleanup_progress_system() {
    // Clear any remaining progress bar lines
    if (config.progress && !config.no_progress_bar) {
        int lines_to_clear = 0;
        for (int i = 0; i < config.progress_bar_count && i < MAX_PROGRESS_BARS; i++) {
            pthread_mutex_lock(&config.progress_bars[i].lock);
            if (config.progress_bars[i].active || config.progress_bars[i].finished) {
                lines_to_clear++;
            }
            pthread_mutex_unlock(&config.progress_bars[i].lock);
        }
        printf("\r\033[2K"); // Clear current line first
        if (lines_to_clear > 0) {
            printf("\033[%dA\033[2K", lines_to_clear); // Clear multiple lines
        }
        fflush(stdout);
    }
    // Reset the progress bar count
    config.progress_bar_count = 0;
    file_bar_index = -1;
    overall_bar_index = -1;
}
