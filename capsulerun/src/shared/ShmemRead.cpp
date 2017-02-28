#include "ShmemRead.h"

#include <stdexcept>

#if defined(CAPSULE_LINUX)
#include <sys/mman.h>
#include <sys/stat.h> // for mode constants
#include <fcntl.h>    // for O_* constants
#include <unistd.h>   // unlink
#elif defined(CAPSULE_MACOS)
#include <sys/mman.h>
#include <sys/stat.h> // for mode constants
#include <fcntl.h>    // for O_* constants
#include <errno.h>    // for errno
#include <unistd.h>   // unlink
#else
#define LEAN_AND_MEAN
#include <windows.h>
#undef LEAN_AND_MEAN

#include <io.h>
#endif

using namespace std;

ShmemRead::ShmemRead (const std::string &path, uint64_t size): size(size) {
#if defined(CAPSULE_WINDOWS)
  handle = OpenFileMappingA(
    FILE_MAP_READ, // read access
    FALSE, // do not inherit the name
    path.c_str() // name of mapping object
  );

  if (!handle) {
    throw runtime_error("OpenFileMappingA failed");
  }

  mapped = (char*) MapViewOfFile(
    handle,
    FILE_MAP_READ, // read access
    0,
    0,
    size
  );

  if (!mapped) {
    throw runtime_error("MapViewOfFile failed");
  }
#else // CAPSULE_WINDOWS
  int handle = shm_open(path.c_str(), O_RDONLY, 0755);
  if (!(handle > 0)) {
    throw runtime_error("shmem_open failed");
  }

  mapped = (char *) mmap(
      nullptr, // addr
      size, // length
      PROT_READ, // prot
      MAP_SHARED, // flags
      handle, // fd
      0 // offset
  );

  if (!mapped) {
    throw runtime_error("mmap failed");
  }
#endif // !CAPSULE_WINDOWS
}

ShmemRead::~ShmemRead () {
#if defined(CAPSULE_WINDOWS)
  UnmapViewOfFile(mapped);
  CloseHandle(handle);
#else // CAPSULE_WINDOWS
  munmap(mapped, size);
  close(handle);
#endif // !CAPSULE_WINDOWS
}
