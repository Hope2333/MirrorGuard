#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>

// 宏定义
#define MAX_EXCLUDE_PATTERNS 16
#define MAX_INCLUDE_PATTERNS 16
#define MAX_SOURCE_DIRS 32
#define MAX_MANIFEST_FILES 32
#define MAX_PATH 4096
#define MAX_PROGRESS_BARS 32  // 新增：最大进度条数量

// TUI 模式
typedef enum {
    TUI_MODE_NONE = 0,      // 无 TUI（默认）
    TUI_MODE_SIMPLE,        // 简单 TUI
    TUI_MODE_ADVANCED,      // 高级 TUI
    TUI_MODE_MINIMAL,       // 极简 TUI
    TUI_MODE_RICH,          // 富文本 TUI
    TUI_MODE_DEBUG          // 调试 TUI
} TuiMode;

// 日志级别
typedef enum { LOG_ERROR, LOG_WARN, LOG_INFO, LOG_DEBUG, LOG_TRACE } LogLevel;

// 错误代码
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

// 进度条样式
typedef enum {
    PROGRESS_STYLE_DEFAULT = 0,    // 默认样式
    PROGRESS_STYLE_BARS,           // 方括号样式
    PROGRESS_STYLE_DOTS,           // 点样式
    PROGRESS_STYLE_UNICODE,        // Unicode字符样式
    PROGRESS_STYLE_ASCII,          // ASCII字符样式
    PROGRESS_STYLE_CUSTOM          // 自定义样式
} ProgressStyle;

// 进度条颜色方案
typedef enum {
    PROGRESS_COLOR_DEFAULT = 0,    // 默认颜色
    PROGRESS_COLOR_GREEN,          // 绿色
    PROGRESS_COLOR_BLUE,           // 蓝色
    PROGRESS_COLOR_YELLOW,         // 黄色
    PROGRESS_COLOR_RED,            // 红色
    PROGRESS_COLOR_CYAN,           // 青色
    PROGRESS_COLOR_MAGENTA,        // 洋红
    PROGRESS_COLOR_RAINBOW         // 彩虹色
} ProgressColor;

// 进度条结构
typedef struct {
    char name[MAX_PATH];           // 进度条名称
    volatile size_t current;       // 当前进度
    volatile size_t total;         // 总量
    volatile double speed;         // 速度 (bytes/s)
    volatile time_t last_update;   // 最后更新时间
    volatile int active;           // 是否活跃
    volatile int finished;         // 是否完成
    pthread_mutex_t lock;          // 线程锁
    ProgressStyle style;           // 样式
    ProgressColor color;           // 颜色
} ProgressBar;

// 全局配置
typedef struct {
    int follow_symlinks;
    int quiet;
    int extra_check;
    int ignore_hidden;
    int progress;                  // 是否显示进度条
    int verbose;
    int dry_run;
    int force_overwrite;
    int threads;
    int recursive;
    int preserve_timestamps;
    int case_sensitive;
    int no_progress_bar;           // 新增：禁用进度条
    int tui_mode;                  // 新增：TUI 模式
    ProgressStyle progress_style;  // 新增：进度条样式
    ProgressColor progress_color;  // 新增：进度条颜色
    const char *exclude_patterns[MAX_EXCLUDE_PATTERNS];
    int exclude_count;
    const char *include_patterns[MAX_INCLUDE_PATTERNS];
    int include_count;
    const char *output_format; // "sha256sum", "json", "csv"
    const char *log_file;
    FILE *log_fp;

    // 操作模式
    int generate_mode;
    int verify_mode;
    int compare_mode;
    int diff_mode;
    int direct_compare_mode;

    // 参数
    const char *source_dirs[MAX_SOURCE_DIRS];
    int source_count;
    const char *mirror_dir;
    const char *manifest_path;
    const char *manifest_files[MAX_MANIFEST_FILES];
    int manifest_count;
    const char *source_dir1;
    const char *source_dir2;

    // 进度条管理
    ProgressBar progress_bars[MAX_PROGRESS_BARS];
    int progress_bar_count;
} Config;

// 统计信息
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
int is_tui_option(const char *arg);  // 新增

// 进度条相关函数
void init_progress_bars();
void create_progress_bar(const char *name, size_t total, int index);
void update_progress_bar(int index, size_t current);
void finish_progress_bar(int index);
void display_progress_bars();
void cleanup_progress_bars();

// TUI 相关函数
void init_tui();
void run_tui();
void cleanup_tui();
int is_tui_enabled();

#endif // CONFIG_H
