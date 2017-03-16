
#include <shoom.h>

#include <io.h>  // CreateFileMappingA, OpenFileMappingA, etc.

namespace shoom {

Shm::Shm(std::string path, size_t size) : path_(path), size_(size){};

ShoomError Shm::CreateOrOpen(bool create) {
  if (create) {
    DWORD size_high_order = 0;
    DWORD size_low_order = static_cast<DWORD>(size_);
    handle_ = CreateFileMappingA(INVALID_HANDLE_VALUE,  // use paging file
                                 NULL,                  // default security
                                 PAGE_READWRITE,        // read/write access
                                 size_high_order, size_low_order,
                                 path_.c_str()  // name of mapping object
                                 );

    if (!handle_) {
      return kErrorCreationFailed;
    }
  } else {
    handle_ = OpenFileMappingA(FILE_MAP_READ,  // read access
                               FALSE,          // do not inherit the name
                               path_.c_str()   // name of mapping object
                               );

    if (!handle_) {
      return kErrorOpeningFailed;
    }
  }

  DWORD access = create ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ;

  data_ = static_cast<uint8_t*>(MapViewOfFile(handle_, access, 0, 0, size_));

  if (!data_) {
    return kErrorMappingFailed;
  }

  return kOK;
}

/**
 * Destructor
 */
Shm::~Shm() {
  if (data_) {
    UnmapViewOfFile(data_);
    data_ = nullptr;
  }

  CloseHandle(handle_);
}

}  // namespace shoom
