#pragma once

#include <stdio.h>

#include <string>

#include <lab/platform.h>

extern FILE *logfile;

#define CapsuleLog(...) {\
  if (!logfile) { \
    logfile = CapsuleOpenLog(); \
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

FILE *CapsuleOpenLog();

#ifdef __cplusplus
}
#endif
