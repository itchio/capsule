
#pragma once

#if defined(_WIN32)
#define CAPSULERUN_WINDOWS

#elif defined(__APPLE__)
#define CAPSULERUN_OSX

#elif defined(__linux__) || defined(__unix__)
#define CAPSULERUN_LINUX

#endif

#define CAPSULE_MAX_PATH_LENGTH 16384

int capsulerun_main (int argc, char **argv);
