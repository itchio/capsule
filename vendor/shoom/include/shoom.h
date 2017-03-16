#pragma once

#include <cstdint>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#endif  // _WIN32

namespace shoom {

enum ShoomError {
  kOK = 0,
  kErrorCreationFailed = 100,
  kErrorMappingFailed = 110,
  kErrorOpeningFailed = 120,
};

class Shm {
 public:
  // path should only contain alpha-numeric characters, and is normalized
  // on linux/macOS.
  explicit Shm(std::string path, size_t size);

  // create a shared memory area and open it for writing
  inline ShoomError Create() { return CreateOrOpen(true); };

  // open an existing shared memory for reading
  inline ShoomError Open() { return CreateOrOpen(false); };

  inline size_t Size() { return size_; };
  inline const std::string& Path() { return path_; }
  inline uint8_t* Data() { return data_; }

  ~Shm();

 private:
  ShoomError CreateOrOpen(bool create);

  std::string path_;
  uint8_t* data_ = nullptr;
  size_t size_ = 0;
#if defined(_WIN32)
  HANDLE handle_;
#else
  int fd_ = -1;
#endif
};
}