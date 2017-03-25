
/*
 *  lab - a general-purpose C++ toolkit
 *  Copyright (C) 2017, Amos Wenger
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details:
 * https://github.com/itchio/lab/blob/master/LICENSE
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "paths.h"

#include "platform.h"

#include <stdlib.h>

#if defined(LAB_LINUX)
#include <unistd.h> // readlink
#include <limits.h> // realpath
#endif

#if defined(LAB_MACOS)
#include <mach-o/dyld.h> // _NSGetExecutablePath
#include <sys/param.h> // realpath
#endif

#if defined(LAB_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#endif // LAB_WINDOWS

#include "strings.h"

namespace lab {
namespace paths {

#if defined(LAB_WINDOWS)
const char *kSeparator = "\\";
#else
const char *kSeparator = "/";
#endif

// TODO: error reporting?
std::string SelfPath() {
  const size_t file_name_characters = 16 * 1024;
#if defined(LAB_WINDOWS)
  wchar_t *file_name = new wchar_t[file_name_characters];
  GetModuleFileNameW(NULL, file_name, static_cast<DWORD>(file_name_characters));
  auto self_path = strings::FromWide(file_name);
  delete[] file_name;
#elif defined(LAB_MACOS)
  // style: have to use uint32_t for _NSGetExecutablePath
  uint32_t mac_file_name_characters = file_name_characters;
  char *utf8_file_name = new char[file_name_characters];
  if (_NSGetExecutablePath(utf8_file_name, &mac_file_name_characters) != 0) {
    return "";
  }
  char *utf8_absolute_file_name = realpath(utf8_file_name, nullptr);
  delete[] utf8_file_name;
  auto self_path = std::string(utf8_absolute_file_name);
  free(utf8_absolute_file_name);
#elif defined(LAB_LINUX)
  char *utf8_file_name = new char[file_name_characters];
  ssize_t dest_num_characters = readlink("/proc/self/exe", utf8_file_name, file_name_characters);
  // readlink does not append a null character
  utf8_file_name[dest_num_characters] = '\0';
  char *utf8_absolute_file_name = realpath(utf8_file_name, nullptr);
  delete[] utf8_file_name;
  auto self_path = std::string(utf8_absolute_file_name);
  free(utf8_absolute_file_name);
#endif

  return self_path;
}

std::string DirName(const std::string path) {
  size_t slash_index = path.find_last_of(kSeparator);
  return path.substr(0, slash_index);
}

std::string Join(const std::string a, const std::string b) {
  return a + std::string(kSeparator) + b;
}

std::string PipePath(std::string name) {
#if defined(LAB_WINDOWS)
  return Join("\\\\.\\pipe", name);
#else
  return Join("/tmp", name);
#endif
}

} // namespace paths
} // namespace lab
