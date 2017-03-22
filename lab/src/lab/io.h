
#pragma once

#include <stdio.h>

#include "platform.h"
#include "strings.h"

namespace lab {
namespace io {

FILE* Fopen(std::string path, std::string mode) {
#if defined(LAB_WINDOWS)
  return fopen(path.c_str(), mode.c_str());
#else
  wchar_t *path_w;
  strings::ToWideChar(path.c_str(), &path_w);
  wchar_t *mode_w;
  strings::ToWideChar(mode.c_str(), &mode_w);
  FILE *result = _wfopen(path_w, mode_w);
  free(path_w);  
  free(mode_w);  
  return result;
#endif
}

} // namespace io
} // namespace lab
