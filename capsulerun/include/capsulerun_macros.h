#pragma once

// Change these to suit your needs
// #define CAPSULERUN_PROFILE
// #define CAPSULERUN_DEBUG

#include <capsulerun_macros.h>

#ifdef CAPSULERUN_PROFILE
#define eprintf(...) { fprintf(stdout, "[capsulerun-profile] "); fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); fflush(stdout); }
#else
#define eprintf(...)
#endif

#ifdef CAPSULERUN_DEBUG
#define cdprintf(...) { fprintf(stdout, "[capsulerun-debug] "); fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); fflush(stdout); }
#else
#define cdprintf(...)
#endif

#define CapsuleLog(...) { fprintf(stdout, "[capsulerun] "); fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); fflush(stdout); }
