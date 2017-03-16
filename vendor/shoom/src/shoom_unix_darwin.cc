
#include <shoom.h>

#include <fcntl.h>     // for O_* constants
#include <sys/mman.h>  // mmap, munmap
#include <sys/stat.h>  // for mode constants
#include <unistd.h>    // unlink

#if defined(__APPLE__)
#include <errno.h>
#endif // __APPLE__

#include <stdexcept>

namespace shoom {

Shm::Shm(std::string path, size_t size) : size_(size) { path_ = "/" + path; };

ShoomError Shm::CreateOrOpen(bool create) {
  if (create) {
    // shm segments persist across runs, and macOS will refuse
    // to ftruncate an existing shm segment, so to be on the safe
    // side, we unlink it beforehand.
    // TODO(amos) check errno while ignoring ENOENT?
    int ret = shm_unlink(path_.c_str());
    if (ret < 0) {
      if (errno != ENOENT) {
        return kErrorCreationFailed;
      }
    }
  }

  int flags = create ? (O_CREAT | O_RDWR) : O_RDONLY;

  fd_ = shm_open(path_.c_str(), flags, 0755);
  if (fd_ < 0) {
    if (create) {
      return kErrorCreationFailed;
    } else {
      return kErrorOpeningFailed;
    }
  }

  if (create) {
    // this is the only way to specify the size of a
    // newly-created POSIX shared memory object
    int ret = ftruncate(fd_, size_);
    if (ret != 0) {
      return kErrorCreationFailed;
    }
  }

  int prot = create ? (PROT_READ | PROT_WRITE) : PROT_READ;

  data_ = static_cast<uint8_t *>(mmap(nullptr,     // addr
                                      size_,       // length
                                      prot,        // prot
                                      MAP_SHARED,  // flags
                                      fd_,         // fd
                                      0            // offset
                                      ));

  if (!data_) {
    return kErrorMappingFailed;
  }

  return kOK;
}

Shm::~Shm() {
  munmap(data_, size_);
  close(fd_);
  shm_unlink(path_.c_str());
}

}  // namespace shoom
