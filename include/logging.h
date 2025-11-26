#ifndef LOGGING_H
#define LOGGING_H

#include <stdarg.h>
#include "config.h"

void log_msg(LogLevel level, const char *fmt, ...);
void log_set_quiet(int quiet);
void log_set_logfile(const char *log_file);

#endif // LOGGING_H