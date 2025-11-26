#ifndef PROGRESS_H
#define PROGRESS_H

#include "config.h"

void init_progress_bars();
void create_progress_bar(const char *name, size_t total, int index);
void update_progress_bar(int index, size_t current);
void finish_progress_bar(int index);
void display_progress_bars();
void cleanup_progress_bars();
void update_single_progress_bar(int index);
void print_progress_bar(const ProgressBar *bar);

#endif // PROGRESS_H