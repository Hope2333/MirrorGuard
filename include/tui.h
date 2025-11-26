#ifndef TUI_H
#define TUI_H

#include "config.h"

// TUI 相关函数
void init_tui();
void run_tui();
void cleanup_tui();
int is_tui_enabled();

// TUI 模式特定函数
void run_simple_tui();
void run_advanced_tui();
void run_minimal_tui();
void run_rich_tui();
void run_debug_tui();

// TUI 渲染函数
void render_simple_ui();
void render_advanced_ui();
void render_minimal_ui();
void render_rich_ui();
void render_debug_ui();

#endif // TUI_H
