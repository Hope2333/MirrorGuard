// include/config.h (Add version fallback)
#ifndef CONFIG_H
#define CONFIG_H

// Add version definition here as a fallback, or ensure Makefile defines it consistently
#ifndef MIRRORGUARD_VERSION
// This is a fallback. The Makefile should define MIRRORGUARD_VERSION="0.1.0alpha"
// If Makefile definition is missed, this fallback will be used.
#define MIRRORGUARD_VERSION "0.1.0alpha_fallback"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>

// Macro definitions
#define MAX_EXCLUDE_PATTERNS 16
#define MAX_INCLUDE_PATTERNS 16
#define MAX_SOURCE_DIRS 32
#define MAX_MANIFEST_FILES 32
#define MAX_PATH 4096
#define MAX_PROGRESS_BARS 32  // New: Maximum number of progress bars

// TUI mode
typedef enum {
    TUI_MODE_NONE = 0,      // No TUI (default)
    TUI_MODE_SIMPLE,        // Simple TUI
    TUI_MODE_ADVANCED,      // Advanced TUI
    TUI_MODE_MINIMAL,       // Minimal TUI
    TUI_MODE_RICH,          // Rich text TUI
    TUI_MODE_DEBUG          // Debug TUI
} TuiMode;

// Log levels
typedef enum { LOG_ERROR, LOG_WARN, LOG_INFO, LOG_DEBUG, LOG_TRACE } LogLevel;

// Error codes
typedef enum {
    MIRRORGUARD_OK = 0,
    MIRRORGUARD_ERROR_GENERAL = 1,
    MIRRORGUARD_ERROR_INVALID_ARGS = 2,
    MIRRORGUARD_ERROR_FILE_IO = 3,
    MIRRORGUARD_ERROR_MEMORY = 4,
    MIRRORGUARD_ERROR_VERIFY_FAILED = 5,
    MIRRORGUARD_ERROR_INTERRUPTED = 6,
    MIRRORGUARD_ERROR_INVALID_FORMAT = 7,
    MIRRORGUARD_ERROR_CONFLICT = 8
} MirrorGuardError;

// Progress bar styles
typedef enum {
    PROGRESS_STYLE_DEFAULT = 0,    // Default style
    PROGRESS_STYLE_BARS,           // Bracket style
    PROGRESS_STYLE_DOTS,           // Dot style
    PROGRESS_STYLE_UNICODE,        // Unicode character style
    PROGRESS_STYLE_ASCII,          // ASCII character style
    PROGRESS_STYLE_CUSTOM,         // Custom style
    PROGRESS_STYLE_BAR,            // Colorful bar style
    PROGRESS_STYLE_RICH            // Rich style
} ProgressStyle;

// Progress bar color schemes
typedef enum {
    PROGRESS_COLOR_DEFAULT = 0,    // Default color
    PROGRESS_COLOR_GREEN,          // Green
    PROGRESS_COLOR_BLUE,           // Blue
    PROGRESS_COLOR_YELLOW,         // Yellow
    PROGRESS_COLOR_RED,            // Red
    PROGRESS_COLOR_CYAN,           // Cyan
    PROGRESS_COLOR_MAGENTA,        // Magenta
    PROGRESS_COLOR_RAINBOW         // Rainbow
} ProgressColor;

// Progress bar structure
typedef struct {
    char name[80];                 // Truncated name
    char full_name[MAX_PATH];      // Full name for logs
    volatile size_t current;       // Current progress
    volatile size_t total;         // Total amount
    volatile double speed;         // Speed (bytes/s)
    volatile time_t last_update;   // Last update time
    volatile int active;           // Is active
    volatile int finished;         // Is finished
    pthread_mutex_t lock;          // Thread lock
    ProgressStyle style;           // Style
    ProgressColor color;           // Color
    int level;                     // 0=detail, 1=overall
} ProgressBar;

// Global configuration
typedef struct {
    int follow_symlinks;
    int quiet;
    int extra_check;
    int ignore_hidden;
    int progress;                  // Whether to show progress bar
    int verbose;
    int dry_run;
    int force_overwrite;
    int threads;
    int recursive;
    int preserve_timestamps;
    int case_sensitive;
    int no_progress_bar;           // New: Disable progress bar
    int tui_mode;                  // New: TUI mode
    ProgressStyle progress_style;  // New: Progress bar style
    ProgressColor progress_color;  // New: Progress bar color
    const char *exclude_patterns[MAX_EXCLUDE_PATTERNS];
    int exclude_count;
    const char *include_patterns[MAX_INCLUDE_PATTERNS];
    int include_count;
    const char *output_format; // "sha256sum", "json", "csv"
    const char *log_file;
    FILE *log_fp;

    // Operation modes
    int generate_mode;
    int verify_mode;
    int compare_mode;
    int diff_mode;
    int direct_compare_mode;

    // Parameters
    const char *source_dirs[MAX_SOURCE_DIRS];
    int source_count;
    const char *mirror_dir;
    const char *manifest_path;
    const char *manifest_files[MAX_MANIFEST_FILES];
    int manifest_count;
    const char *source_dir1;
    const char *source_dir2;

    // Progress bar management
    ProgressBar progress_bars[MAX_PROGRESS_BARS];
    int progress_bar_count;
} Config;

// Statistics
typedef struct {
    volatile size_t total_files;
    volatile size_t processed_files;
    volatile size_t missing_files;
    volatile size_t corrupt_files;
    volatile size_t extra_files;
    volatile size_t error_files;
    volatile size_t bytes_processed;
    struct timeval start_time;
    struct timeval end_time;
    pthread_mutex_t lock;
} Statistics;

extern Config config;
extern Statistics stats;
extern volatile sig_atomic_t g_interrupted;

void init_config();
int parse_args(int argc, char **argv);
int validate_args(int argc, char **argv);
void cleanup_config();
int is_tui_option(const char *arg);

// Progress bar related functions
void init_progress_system();
void create_file_progress(const char *filename, size_t total_files);
void update_file_progress(size_t current_file);
void create_overall_progress(const char *name, size_t total_sources);
void update_overall_progress(size_t current_source);
void finish_file_progress();
void finish_overall_progress();
void display_all_progress();
void cleanup_progress_system();
void hide_progress_temporarily();
void show_progress_after_log();

// TUI related functions
void init_tui();
void run_tui();
void cleanup_tui();
int is_tui_enabled();

#endif // CONFIG_H
