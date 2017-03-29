
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

#include "strings.h"

#if defined(LAB_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#endif // LAB_WINDOWS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace lab {
namespace strings {

#if defined(LAB_WINDOWS)

std::wstring ToWide(const std::string &utf8_string) {
  auto codepage = CP_UTF8;
  DWORD flags = 0;
  auto utf8_c_string = utf8_string.c_str();
  // -1 instructs the conversion routine to treat the input as null-terminated
  // and to null-terminate the output, which also gives us the correct buffer size
  int utf8_c_string_len = -1;

  // first, determine length of result
  int ret = MultiByteToWideChar(
    codepage, flags,
    utf8_c_string, utf8_c_string_len,
    NULL, 0
  );

  int utf16_c_string_len = ret;
  // "If this value is 0, the function returns the required buffer size,
  // in characters, including any terminating null character,
  // and makes no use of the lpWideCharStr buffer."
  // -- MSDN doc for MultiByteToWideChar
  wchar_t *utf16_c_string = new wchar_t[utf16_c_string_len];

  MultiByteToWideChar(
    codepage, flags,
    utf8_c_string, utf8_c_string_len,
    utf16_c_string, utf16_c_string_len
  );

  auto result = std::wstring(utf16_c_string);
  delete[] utf16_c_string;
  return result;
}

std::string FromWide(const std::wstring &utf16_string) {
  auto codepage = CP_UTF8;
  DWORD flags = 0;
  const wchar_t *utf16_c_string = utf16_string.c_str();
  // -1 instructs the conversion routine to treat the input as null-terminated
  // and to null-terminate the output, which also gives us the correct buffer size
  int utf16_c_string_len = -1;

  int ret = WideCharToMultiByte(
    codepage, flags,
    utf16_c_string, utf16_c_string_len,
    nullptr, 0,
    nullptr, nullptr
  );

  if (ret == 0) {
    return "<invalid utf-16 input>";
  }

  int utf8_c_string_len = ret;
  char *utf8_c_string = new char[utf8_c_string_len];

  ret = WideCharToMultiByte(
    codepage, flags,
    utf16_c_string, utf16_c_string_len,
    utf8_c_string, utf8_c_string_len,
    nullptr, nullptr
  );

  if (ret == 0) {
    return "<invalid utf-16 input>";
  }

  auto result = std::string(utf8_c_string);
  delete[] utf8_c_string;
  return result;
}

void ArgvQuote(const std::wstring &argument, std::wstring &command_line,
               bool force) {

  // Unless we're told otherwise, don't quote unless we actually
  // need to do so --- hopefully avoid problems if programs won't
  // parse quotes properly
  if (force == false && argument.empty() == false &&
      argument.find_first_of(L" \t\n\v\"") == std::string::npos) {
    command_line.append(argument);
  } else {
    command_line.push_back(L'"');

    for (auto it = argument.begin();; ++it) {
      int number_backslashes = 0;

      while (it != argument.end() && *it == L'\\') {
        ++it;
        ++number_backslashes;
      }

      if (it == argument.end()) {
        // Escape all backslashes, but let the terminating
        // double quotation mark we add below be interpreted
        // as a metacharacter.
        command_line.append(number_backslashes * 2, L'\\');
        break;
      } else if (*it == L'"') {
        // Escape all backslashes and the following
        // double quotation mark.
        command_line.append(number_backslashes * 2 + 1, L'\\');
        command_line.push_back(*it);
      } else {
        // Backslashes aren't special here.
        command_line.append(number_backslashes, L'\\');
        command_line.push_back(*it);
      }
    }

    command_line.push_back(L'"');
  }
}

#endif // LAB_WINDOWS

bool CContains (const char *haystack, const char *needle) {
  return nullptr != strstr(haystack, needle);
}

bool CEquals (const char *a, const char *b) {
  return 0 == strcmp(a, b);
}

} // namespace strings
} // namespace lab
