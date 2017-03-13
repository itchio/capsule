#pragma once

#include <capsule/platform.h>
#include <string>

#if defined(CAPSULE_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#endif // CAPSULE_WINDOWS

#define SHM_CREATE true
#define SHM_OPEN false

class Shm {
  public:
    Shm(const std::string &path, uint64_t size, bool create);
    ~Shm();
    char *mapped;

  private:
    uint64_t size;
#if defined(CAPSULE_WINDOWS)
    HANDLE handle;
#else
    int handle;
#endif
};
