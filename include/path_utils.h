#ifndef PATH_UTILS_H
#define PATH_UTILS_H

#include <stddef.h>

char* normalize_path(const char *path);
int is_safe_path(const char *path);
int should_exclude(const char *path);

#endif // PATH_UTILS_H
