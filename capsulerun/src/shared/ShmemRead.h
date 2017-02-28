#pragma once

#include <capsule/platform.h>
#include <string>

class ShmemRead {
  public:
    ShmemRead(const std::string &path, uint64_t size);
    ~ShmemRead();
    char *mapped;

  private:
    uint64_t size;
#if defined(CAPSULE_WINDOWS)
    HANDLE handle;
#else
    int handle;
#endif
};
