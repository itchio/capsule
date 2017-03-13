#include "Shm.h"

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
#include <io.h>
#endif

using namespace std;

Shm::Shm (const std::string &path, uint64_t size, bool create): size(size) {
#if defined(CAPSULE_WINDOWS)
  if (create) {
    handle = OpenFileMappingA(
      FILE_MAP_READ, // read access
      FALSE, // do not inherit the name
      path.c_str() // name of mapping object
    );

    if (!handle) {
      throw runtime_error("OpenFileMappingA failed");
    }
  } else {
    handle = OpenFileMappingA(
      FILE_MAP_READ, // read access
      FALSE, // do not inherit the name
      path.c_str() // name of mapping object
    );

    if (!handle) {
      throw runtime_error("OpenFileMappingA failed");
    }
  }

  DWORD dwAccess = create ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ;

  mapped = (char*) MapViewOfFile(
    handle,
    dwAccess,
    0,
    0,
    size
  );

  if (!mapped) {
    throw runtime_error("MapViewOfFile failed");
  }
#else // CAPSULE_WINDOWS
  if (create) {
    // shm segments persist across runs, and macOS will refuse
    // to ftruncate an existing shm segment, so to be on the safe
    // side, we unlink it beforehand.
    shm_unlink(path.c_str());
  }

  int flags = create ? O_CREATE|O_RDWR : O_RDONLY;

  int handle = shm_open(path.c_str(), flags, 0755);
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

Shm::~Shm () {
#if defined(CAPSULE_WINDOWS)
  UnmapViewOfFile(mapped);
  CloseHandle(handle);
#else // CAPSULE_WINDOWS
  munmap(mapped, size);
  close(handle);
#endif // !CAPSULE_WINDOWS
}
