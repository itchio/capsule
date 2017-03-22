#pragma once

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

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

// printf macros for size_t, in the style of inttypes.h
#ifdef _LP64
#define __PRIS_PREFIX "z"
#else
#define __PRIS_PREFIX
#endif

// Use these macros after a % in a printf format string
// to get correct 32/64 bit behavior, like this:
// size_t size = records.size();
// printf("%" PRIuS "\n", size);

#define PRIdS __PRIS_PREFIX "d"
#define PRIxS __PRIS_PREFIX "x"
#define PRIuS __PRIS_PREFIX "u"
#define PRIXS __PRIS_PREFIX "X"
#define PRIoS __PRIS_PREFIX "o"
