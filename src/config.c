#include "config.h"
#include "logging.h"
#include "progress.h"
#include "tui.h"
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <string.h>

volatile sig_atomic_t g_interrupted = 0;
Config config = {0};
Statistics stats = {0};

// 信号处理
void signal_handler(int sig) {
    g_interrupted = 1;
    fprintf(stderr, "\n接收到信号 %d，正在安全退出...\n", sig);
    
    // 隐藏进度条以完成清理
    hide_progress_temporarily();
}

void safe_exit(int status) {
    // 清理进度系统
    cleanup_progress_system();
    
    // 清理配置
    cleanup_config();
    
    exit(status);
}

void init_config() {
    // 信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // 默认配置
    config.follow_symlinks = 0;
    config.quiet = 0;
    config.extra_check = 1;
    config.ignore_hidden = 0;
    config.progress = 1;  // 默认启用进度条
    config.verbose = 0;
    config.dry_run = 0;
    config.force_overwrite = 0;
    config.threads = sysconf(_SC_NPROCESSORS_ONLN); // 默认为CPU核心数
    if (config.threads > 32) config.threads = 32; // 限制最大线程数
    config.recursive = 1;
    config.preserve_timestamps = 0;
    config.case_sensitive = 1;
    config.no_progress_bar = 0;  // 默认不禁用进度条
    config.tui_mode = TUI_MODE_NONE;  // 默认无TUI
    config.progress_style = PROGRESS_STYLE_DEFAULT;
    config.progress_color = PROGRESS_COLOR_GREEN;
    config.exclude_count = 0;
    config.include_count = 0;
    config.output_format = "sha256sum";
    config.log_file = NULL;
    config.log_fp = NULL;
    
    // 操作模式
    config.generate_mode = 0;
    config.verify_mode = 0;
    config.compare_mode = 0;
    config.diff_mode = 0;
    config.direct_compare_mode = 0;
    
    // 参数初始化
    config.source_count = 0;
    config.mirror_dir = NULL;
    config.manifest_path = NULL;
    config.manifest_count = 0;
    config.source_dir1 = NULL;
    config.source_dir2 = NULL;
    
    // 初始化统计
    stats.total_files = 0;
    stats.processed_files = 0;
    stats.missing_files = 0;
    stats.corrupt_files = 0;
    stats.extra_files = 0;
    stats.error_files = 0;
    stats.bytes_processed = 0;
    pthread_mutex_init(&stats.lock, NULL);
    gettimeofday(&stats.start_time, NULL);
    
    // 初始化进度系统
    init_progress_system();
}

int parse_args(int argc, char **argv) {
    if (argc == 0 || argv == NULL) return MIRRORGUARD_OK; // 避免未使用警告
    
    // 检查是否有 --tui 参数
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--tui=", 6) == 0) {
            const char *tui_val = argv[i] + 6;
            int tui_num = atoi(tui_val);
            if (tui_num >= 0 && tui_num <= 5) {
                config.tui_mode = tui_num;
            } else {
                fprintf(stderr, "错误: TUI 模式必须在 0-5 之间\n");
                return MIRRORGUARD_ERROR_INVALID_ARGS;
            }
        }
    }
    
    // 检查是否有 --no-bar 参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-bar") == 0) {
            config.no_progress_bar = 1;
            config.progress = 0;  // 同时禁用进度显示
        }
    }
    
    // 参数解析逻辑
    int opt;
    while ((opt = getopt(argc, argv, "gvcdhVqnpfrHeCFx:i:o:l:")) != -1) {
        switch (opt) {
            case 'g': // generate mode
                config.generate_mode = 1;
                break;
            case 'v': // verify mode
                config.verify_mode = 1;
                break;
            case 'c': // compare mode
                config.compare_mode = 1;
                break;
            case 'd': // diff mode
                config.direct_compare_mode = 1;
                break;
            case 'h': // help
                return MIRRORGUARD_ERROR_INVALID_ARGS; // 触发帮助显示
            case 'V': // verbose
                config.verbose++;
                break;
            case 'q': // quiet
                config.quiet = 1;
                break;
            case 'n': // dry run
                config.dry_run = 1;
                break;
            case 'p': // progress
                config.progress = 1;
                break;
            case 'f': // follow symlinks
                config.follow_symlinks = 1;
                break;
            case 'r': // no recursive
                config.recursive = 0;
                break;
            case 'H': // no hidden
                config.ignore_hidden = 1;
                break;
            case 'e': // no extra check
                config.extra_check = 0;
                break;
            case 'C': // case insensitive
                config.case_sensitive = 0;
                break;
            case 'F': // force
                config.force_overwrite = 1;
                break;
            case 'x': // exclude
                if (config.exclude_count < MAX_EXCLUDE_PATTERNS) {
                    config.exclude_patterns[config.exclude_count++] = optarg;
                }
                break;
            case 'i': // include
                if (config.include_count < MAX_INCLUDE_PATTERNS) {
                    config.include_patterns[config.include_count++] = optarg;
                }
                break;
            case 'o': // output format
                config.output_format = optarg;
                break;
            case 'l': // log file
                config.log_file = optarg;
                break;
            default:
                return MIRRORGUARD_ERROR_INVALID_ARGS;
        }
    }
    
    // 解析剩余参数（源目录、清单文件等）
    int remaining = optind;
    
    if (config.generate_mode) {
        // 在生成模式下，最后一个非选项参数应该是清单文件
        // 其他参数是源目录
        if (remaining < argc) {
            // 从后往前找，最后一个非选项参数应该是清单文件
            int last_arg = argc - 1;
            while (last_arg >= remaining && argv[last_arg][0] == '-') {
                last_arg--;
            }
            
            if (last_arg >= remaining) {
                config.manifest_path = argv[last_arg];
                
                // 其余参数是源目录
                for (int i = remaining; i < last_arg; i++) {
                    if (config.source_count < MAX_SOURCE_DIRS) {
                        config.source_dirs[config.source_count++] = argv[i];
                    }
                }
            }
        }
    } else if (config.verify_mode) {
        // 解析验证模式的参数
        if (remaining < argc) config.mirror_dir = argv[remaining++];
        if (remaining < argc) config.manifest_path = argv[remaining];
    } else if (config.compare_mode) {
        // 解析比较模式的参数
        if (remaining < argc) config.manifest_files[0] = argv[remaining++];
        if (remaining < argc) config.manifest_files[1] = argv[remaining];
        config.manifest_count = 2;
    } else if (config.direct_compare_mode) {
        // 解析直接比较模式的参数
        if (remaining < argc) config.source_dir1 = argv[remaining++];
        if (remaining < argc) config.source_dir2 = argv[remaining];
    }
    
    return MIRRORGUARD_OK;
}

int validate_args(int argc, char **argv) {
    if (argc == 0 || argv == NULL) return MIRRORGUARD_OK; // 避免未使用警告
    
    int mode_count = config.generate_mode + config.verify_mode + 
                     config.compare_mode + config.direct_compare_mode;
    
    if (mode_count == 0) {
        // 如果没有操作模式，但有 -V 参数，这可能是版本请求
        return MIRRORGUARD_OK; // 让 main 函数处理
    }
    
    if (mode_count > 1) {
        return MIRRORGUARD_ERROR_CONFLICT;
    }
    
    // 验证各个模式的参数
    if (config.generate_mode) {
        if (config.source_count < 1 || !config.manifest_path) {
            fprintf(stderr, "错误: 生成模式需要至少一个源目录和一个清单文件\n");
            return MIRRORGUARD_ERROR_INVALID_ARGS;
        }
    } else if (config.verify_mode) {
        if (!config.mirror_dir || !config.manifest_path) {
            return MIRRORGUARD_ERROR_INVALID_ARGS;
        }
    } else if (config.compare_mode) {
        if (config.manifest_count != 2 || !config.manifest_files[0] || !config.manifest_files[1]) {
            return MIRRORGUARD_ERROR_INVALID_ARGS;
        }
    } else if (config.direct_compare_mode) {
        if (!config.source_dir1 || !config.source_dir2) {
            return MIRRORGUARD_ERROR_INVALID_ARGS;
        }
    }
    
    return MIRRORGUARD_OK;
}

void cleanup_config() {
    // 清理资源
    if (config.log_fp) {
        fclose(config.log_fp);
        config.log_fp = NULL;
    }
    
    // 清理 TUI
    if (config.tui_mode != TUI_MODE_NONE) {
        cleanup_tui();
    }
    
    // 清理进度系统
    cleanup_progress_system();
    
    // 清理排除/包含模式
    for (int i = 0; i < config.exclude_count; i++) {
        config.exclude_patterns[i] = NULL;
    }
    config.exclude_count = 0;
    
    for (int i = 0; i < config.include_count; i++) {
        config.include_patterns[i] = NULL;
    }
    config.include_count = 0;
    
    // 重置源目录
    config.source_count = 0;
    
    // 重置清单文件
    config.manifest_count = 0;
}