#pragma once

#include "platform.h"

#include <stdio.h>

#define CAPSULE_LOG_PATH_SIZE 32*1024

extern FILE *logfile;

#define capsule_log(...) {\
  if (!logfile) { \
    logfile = capsule_open_log(); \
  } \
  fprintf(logfile, __VA_ARGS__); \
  fprintf(logfile, "\n"); \
  fflush(logfile); \
  fprintf(stderr, "[capsule] "); \
  fprintf(stderr, __VA_ARGS__); \
  fprintf(stderr, "\n"); }

#ifdef __cplusplus
extern "C" {
#endif

FILE *capsule_open_log();

#ifdef CAPSULE_WINDOWS
wchar_t *capsule_log_path();
#else
char *capsule_log_path();
#endif // CAPSULE_WINDOWS

#ifdef __cplusplus
}
#endif
