
#include "io.h"

namespace lab {
namespace io {

FILE* Fopen(std::string path, std::string mode) {
#if defined(LAB_WINDOWS)
  wchar_t *path_w;
  strings::ToWideChar(path.c_str(), &path_w);
  wchar_t *mode_w;
  strings::ToWideChar(mode.c_str(), &mode_w);
  FILE *result = _wfsopen(path_w, mode_w, _SH_DENYNO);
  free(path_w);  
  free(mode_w);  
  return result;
#else
  return fopen(path.c_str(), mode.c_str());
#endif
}

} // namespace io
} // namespace lab
