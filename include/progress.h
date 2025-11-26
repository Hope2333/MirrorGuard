#ifndef PROGRESS_H
#define PROGRESS_H

#include "config.h"

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

#endif // PROGRESS_H
