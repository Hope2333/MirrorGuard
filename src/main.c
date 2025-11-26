#include "config.h"
#include "logging.h"
#include "verification.h"
#include "comparison.h"
#include "progress.h"
#include "tui.h"
#include "directory_scan.h"  // <-- Add this line
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h> // Add this for errno
#include <unistd.h> // Add this for getpid, unlink

void show_help(const char *prog_name);
void show_version();

int main(int argc, char **argv) {
    // Initialize config
    init_config();

    // Check if there are arguments
    if (argc == 1) {
        show_help(argv[0]);
        cleanup_config();
        return MIRRORGUARD_ERROR_INVALID_ARGS;
    }

    // Check if it's a help or version request
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            show_help(argv[0]);
            cleanup_config();
            return MIRRORGUARD_OK;
        }
        if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--version") == 0) {
            show_version();
            cleanup_config();
            return MIRRORGUARD_OK;
        }
    }

    // Parse command line arguments
    int result = parse_args(argc, argv);
    if (result != MIRRORGUARD_OK) {
        if (result == MIRRORGUARD_ERROR_INVALID_ARGS) {
            // Check if it's a help or version request
            for (int i = 1; i < argc; i++) {
                if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
                    show_help(argv[0]);
                    cleanup_config();
                    return MIRRORGUARD_OK;
                }
                if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--version") == 0) {
                    show_version();
                    cleanup_config();
                    return MIRRORGUARD_OK;
                }
            }
            show_help(argv[0]);
        }
        cleanup_config();
        return result;
    }

    // Validate arguments
    result = validate_args(argc, argv);
    if (result != MIRRORGUARD_OK) {
        if (result == MIRRORGUARD_ERROR_INVALID_ARGS) {
            fprintf(stderr, "错误: 参数不正确或不完整\n");
            show_help(argv[0]);
        } else if (result == MIRRORGUARD_ERROR_CONFLICT) {
            fprintf(stderr, "错误: 参数冲突\n");
        }
        cleanup_config();
        return result;
    }

    // If TUI is enabled, initialize TUI
    if (config.tui_mode != TUI_MODE_NONE) {
        init_tui();
    }

    // Set up logging
    log_set_logfile(config.log_file);
    log_set_quiet(config.quiet);

    // Execute according to operation mode
    if (config.generate_mode) {
        if (!config.manifest_path || config.source_count == 0) {
            fprintf(stderr, "错误: 生成模式需要指定源目录和清单文件\n");
            show_help(argv[0]);
            cleanup_config();
            return MIRRORGUARD_ERROR_INVALID_ARGS;
        }

        log_msg(LOG_INFO, "开始生成多源清单...");
        for (int i = 0; i < config.source_count; i++) {
            log_msg(LOG_INFO, "  源目录 %d: %s", i + 1, config.source_dirs[i]);
        }
        log_msg(LOG_INFO, "清单文件: %s", config.manifest_path);

        // Set overall progress
        if (config.progress && !config.no_progress_bar) {
            create_overall_progress("总体进度", config.source_count);
        }

        FileList *list = create_file_list();
        if (!list) {
            cleanup_config();
            return MIRRORGUARD_ERROR_MEMORY;
        }

        for (int i = 0; i < config.source_count; i++) {
            if (g_interrupted) break;

            if (config.progress && !config.no_progress_bar) {
                update_overall_progress(i + 1);
            }

            // scan_directory is declared in directory_scan.h, which is included via config.h indirectly
            // or should be included explicitly if needed here. Since verification.c uses it,
            // and main.c includes verification.h, it should be available.
            // However, let's ensure directory_scan.h is included if needed directly in main.c
            // But usually, including the module that uses it (verification.h) is sufficient.
            // The error likely means directory_scan.h wasn't found during the preprocessing of main.c
            // when looking for the declaration of scan_directory.
            // Ensure directory_scan.h declares scan_directory, which it does according to your header.
            // The error might be misleading if directory_scan.o was not built first, but make should handle dependencies.
            // Let's re-check the header inclusion chain.
            // config.h -> data_structs.h (defines FileList) -> directory_scan.h (declares scan_directory)
            // main.c -> config.h. So scan_directory should be declared.
            // Ah, the error says "implicit declaration", meaning the *definition* was found, but the *declaration* was not.
            // This happens if the header file declaring the function is not included.
            // Since scan_directory is declared in directory_scan.h, and main.c includes verification.h,
            // and verification.h includes data_structs.h, and data_structs.h should include directory_scan.h...
            // No, data_structs.h does not include directory_scan.h. verification.h does not include directory_scan.h.
            // main.c does not include directory_scan.h.
            // So scan_directory is not declared when main.c is compiled.
            // You need to include directory_scan.h in main.c or ensure its declaration is available somehow.
            // The cleanest way is often to include the header file for the module you are calling directly.
            // However, verification.c calls scan_directory, so verification.h doesn't necessarily need to declare it
            // unless verification.c needs to pass function pointers to it or something.
            // The error occurs in main.c trying to call scan_directory.
            // main.c -> verification.h (uses FileList) -> data_structs.h (defines FileList)
            // main.c needs scan_directory. So main.c should include directory_scan.h.
            // Or, verification.h could include directory_scan.h if verification functions rely on it internally.
            // Let's add the include to main.c for clarity, as main.c directly calls scan_directory.
            // Actually, looking again: main.c calls scan_directory within the generate_mode block.
            // This call is part of the logic that *uses* verification functionality (which scans).
            // verification.h declares generate_manifest_multi, which *internally* uses scan_directory.
            // So main.c calls generate_manifest_multi, and that function should handle scan_directory internally.
            // The error in main.c suggests the compiler saw the call to scan_directory *in main.c*.
            // But looking at the error context again:
            // src/main.c:118:17: 错误：隐式声明函数‘scan_directory’ [-Wimplicit-function-declaration]
            //  118 |             if (scan_directory(config.source_dirs[i], list) != 0) {
            // Ah! The call *is* in main.c (line 118). This means the code snippet provided in the knowledge base
            // for main.c might be slightly different from the one causing the error, OR the error message is
            // pointing to a call within a function defined in main.c that calls scan_directory directly,
            // not through generate_manifest_multi.
            // Re-reading the provided main.c, the call is indeed inside the main function within the generate_mode block.
            // So, main.c directly calls scan_directory.
            // Therefore, main.c MUST include directory_scan.h.
            // Let's add it:

            if (scan_directory(config.source_dirs[i], list) != 0) { // This line caused the error
                free_file_list(list);
                if (config.progress && !config.no_progress_bar) {
                    finish_overall_progress();
                }
                cleanup_config(); // Add cleanup before return
                return MIRRORGUARD_ERROR_FILE_IO;
            }
        }

        if (list->count == 0) {
            log_msg(LOG_ERROR, "未找到可处理的文件");
            free_file_list(list);
            if (config.progress && !config.no_progress_bar) {
                finish_overall_progress();
            }
            cleanup_config(); // Add cleanup before return
            return MIRRORGUARD_ERROR_GENERAL;
        }

        // Create temporary manifest
        char temp_manifest[MAX_PATH];
        // getpid requires unistd.h - Added above
        snprintf(temp_manifest, sizeof(temp_manifest), "%s.tmp.%d", config.manifest_path, getpid());

        if (!config.dry_run) {
            FILE *manifest = fopen(temp_manifest, "w");
            if (!manifest) {
                log_msg(LOG_ERROR, "无法创建临时清单: %s", strerror(errno)); // errno requires errno.h - Added above
                free_file_list(list);
                if (config.progress && !config.no_progress_bar) {
                    finish_overall_progress();
                }
                cleanup_config(); // Add cleanup before return
                return MIRRORGUARD_ERROR_FILE_IO;
            }

            // Write all file information
            for (size_t i = 0; i < list->count; i++) {
                fprintf(manifest, "%s *%s\n", list->files[i].hash, list->files[i].path);
            }

            fclose(manifest);
        }

        if (!config.dry_run) {
            // Atomic rename temporary manifest
            // unlink requires unistd.h - Added above
            if (rename(temp_manifest, config.manifest_path) != 0) {
                log_msg(LOG_ERROR, "无法完成清单: %s", strerror(errno)); // errno requires errno.h - Added above
                unlink(temp_manifest); // unlink requires unistd.h - Added above
                free_file_list(list);
                if (config.progress && !config.no_progress_bar) {
                    finish_overall_progress();
                }
                cleanup_config(); // Add cleanup before return
                return MIRRORGUARD_ERROR_FILE_IO;
            }
        }

        log_msg(LOG_INFO, "多源清单生成成功: %s", config.manifest_path);
        log_msg(LOG_INFO, "总计文件数: %zu", list->count);

        if (config.progress && !config.no_progress_bar) {
            finish_overall_progress();
        }

        free_file_list(list);
        result = MIRRORGUARD_OK;
    } else if (config.verify_mode) {
        if (!config.mirror_dir || !config.manifest_path) {
            fprintf(stderr, "错误: 验证模式需要指定镜像目录和清单文件\n");
            show_help(argv[0]);
            cleanup_config();
            return MIRRORGUARD_ERROR_INVALID_ARGS;
        }

        log_msg(LOG_INFO, "开始验证镜像...");
        log_msg(LOG_INFO, "镜像目录: %s", config.mirror_dir);
        log_msg(LOG_INFO, "清单文件: %s", config.manifest_path);

        result = verify_mirror(config.mirror_dir, config.manifest_path);
    } else if (config.compare_mode) {
        if (config.manifest_count != 2 || !config.manifest_files[0] || !config.manifest_files[1]) {
            fprintf(stderr, "错误: 比较模式需要指定两个清单文件\n");
            show_help(argv[0]);
            cleanup_config();
            return MIRRORGUARD_ERROR_INVALID_ARGS;
        }

        log_msg(LOG_INFO, "开始比较清单文件...");
        log_msg(LOG_INFO, "清单1: %s", config.manifest_files[0]);
        log_msg(LOG_INFO, "清单2: %s", config.manifest_files[1]);

        result = compare_manifests(config.manifest_files[0], config.manifest_files[1]);
    } else if (config.direct_compare_mode) {
        if (!config.source_dir1 || !config.source_dir2) {
            fprintf(stderr, "错误: 目录比较模式需要指定两个目录\n");
            show_help(argv[0]);
            cleanup_config();
            return MIRRORGUARD_ERROR_INVALID_ARGS;
        }

        log_msg(LOG_INFO, "开始直接比较目录...");
        log_msg(LOG_INFO, "目录1: %s", config.source_dir1);
        log_msg(LOG_INFO, "目录2: %s", config.source_dir2);

        result = compare_directories(config.source_dir1, config.source_dir2);
    } else {
        // If no operation mode is specified, show help
        show_help(argv[0]);
        result = MIRRORGUARD_ERROR_INVALID_ARGS;
    }

    // Record end time
    struct timeval end_time;
    gettimeofday(&end_time, NULL);

    // Calculate elapsed time
    double elapsed = (end_time.tv_sec - stats.start_time.tv_sec) +
                     (end_time.tv_usec - stats.start_time.tv_usec) / 1000000.0;

    if (config.generate_mode || config.verify_mode) {
        log_msg(LOG_INFO, "\n总耗时: %.2f秒", elapsed);
        if (elapsed > 0) {
            log_msg(LOG_INFO, "处理速度: %.2f MB/s", (stats.bytes_processed / 1024.0 / 1024.0) / elapsed);
        }
    }

    // Clean up resources
    cleanup_config();

    return result;
}

void show_help(const char *prog_name) {
    // Hide progress bar to show help
    hide_progress_temporarily();

    printf("MirrorGuard v%s - 企业级镜像完整性校验工具\n", MIRRORGUARD_VERSION);
    printf("用法: %s [选项] 命令\n\n", prog_name);

    printf("命令:\n");
    printf("  -g, --generate <源目录1> [源目录2]... <清单文件>  生成多源校验清单\n");
    printf("  -v, --verify <镜像目录> <清单文件>               验证镜像完整性\n");
    printf("  -c, --compare <清单1> <清单2>                   比较两个清单文件\n");
    printf("  -d, --diff <源目录1> <源目录2>                  直接比较两个目录\n\n");

    printf("通用选项:\n");
    printf("  -f, --follow-symlinks        跟随符号链接 (默认: 不跟随)\n");
    printf("  -H, --no-hidden              忽略隐藏文件 (默认: 包含)\n");
    printf("  -x, --exclude <模式>         排除匹配模式的文件\n");
    printf("  -i, --include <模式>         仅包含匹配模式的文件\n");
    printf("  -e, --no-extra-check         禁用额外文件检查 (默认: 启用)\n");
    printf("  -r, --no-recursive           禁用递归扫描 (默认: 启用)\n");
    printf("  -p, --progress               显示处理进度 (默认: 启用)\n");
    printf("  --no-bar                     禁用进度条显示 (默认: 启用)\n");
    printf("  --tui=<0-5>                  TUI 模式: 0=无, 1=简单, 2=高级, 3=极简, 4=富文本, 5=调试\n");
    printf("  -V, --verbose                详细输出 (可多次使用)\n");
    printf("  -q, --quiet                  安静模式 (仅显示错误)\n");
    printf("  -n, --dry-run                模拟运行 (不实际写入)\n");
    printf("  -F, --force                  强制覆盖现有清单 (默认: 询问)\n");
    printf("  -C, --case-insensitive       不区分大小写匹配 (默认: 区分)\n");
    printf("  -o, --output-format <fmt>    输出格式: sha256sum/json/csv (默认: sha256sum)\n");
    printf("  -l, --log-file <文件>        日志输出到文件\n");
    printf("  -h, --help                   显示此帮助\n");
    printf("  -V, --version                显示版本信息\n\n");

    printf("示例:\n");
    printf("  # 生成多源清单 (排除临时文件)\n");
    printf("  %s -x '.tmp' -g /data/source1 /data/source2 manifest.sha256\n\n", prog_name);

    printf("  # 验证镜像 (安静模式)\n");
    printf("  %s -q -v /backup/mirror manifest.sha256\n\n", prog_name);

    printf("  # 比较两个清单文件\n");
    printf("  %s -c manifest1.sha256 manifest2.sha256\n\n", prog_name);

    printf("  # 直接比较两个目录\n");
    printf("  %s -d /data/source1 /data/source2\n\n", prog_name);

    printf("  # 启用 TUI 模式\n");
    printf("  %s --tui=1 -g /data/source1 manifest.sha256\n\n", prog_name);

    printf("  # 禁用进度条\n");
    printf("  %s --no-bar -g /data/source /data/manifest.sha256\n\n", prog_name);

    printf("TUI 模式说明:\n");
    printf("  0 - 无 TUI (默认)\n");
    printf("  1 - 简单 TUI: 基本进度显示\n");
    printf("  2 - 高级 TUI: 彩色界面，交互功能\n");
    printf("  3 - 极简 TUI: 最小化显示\n");
    printf("  4 - 富文本 TUI: 美观的彩色界面\n");
    printf("  5 - 调试 TUI: 显示内部状态信息\n");

    printf("\n短参数组合示例:\n");
    printf("  -qv  等同于 -q -v  (安静模式 + 详细输出)\n");
    printf("  -np  等同于 -n -p  (模拟运行 + 显示进度)\n");
    printf("  -fC  等同于 -f -C  (跟随链接 + 不区分大小写)\n");

    // Show progress bar again
    show_progress_after_log();
}

void show_version() {
    // Hide progress bar to show version
    hide_progress_temporarily();

    printf("MirrorGuard v%s\n", MIRRORGUARD_VERSION);
    printf("编译时间: %s %s\n", __DATE__, __TIME__);
    printf("系统信息: Linux POSIX\n");
    printf("OpenSSL版本: OpenSSL 3.0+ (EVP接口)\n");

    // Show progress bar again
    show_progress_after_log();
}
