
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

void ToWideChar (const char *s, wchar_t **ws) {
  int wchars_num = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
  *ws = (wchar_t *) malloc(wchars_num * sizeof(wchar_t));
  MultiByteToWideChar(CP_UTF8, 0, s, -1, *ws, wchars_num);
}

void FromWideChar (const wchar_t *ws, char **s) {
  int ret;
  int ws_len = wcslen(ws);

  ret = WideCharToMultiByte(
    CP_UTF8, // CodePage
    0, // dwFlags
    ws, // lpWideCharStr
    ws_len, // lpWideCharStr (number of characters, *not* bytes)
    NULL, // lpMultiByteStr
    0, // cbMultiByte (size of output string, in bytes)
    NULL, // lpDefaultChar (must be null for UTF-8)
    NULL // lpUsedDefaultChar (must be null for UTF-8)
  );

  if (ret == 0) {
    // we'll treat this as a fatal error since if it happens,
    // the rest of the program won't run right
    DWORD err = GetLastError();
    fprintf(stderr, "While converting string to utf-8 (sizing): %d (%x)", err, err);
    exit(131);
  }

  int num_chars = ret;

  *s = (char *) calloc(num_chars + 1, sizeof(char));
  ret = WideCharToMultiByte(
    CP_UTF8, // CodePage
    0, // dwFlags
    ws, // lpWideCharStr
    ws_len, // lpWideCharStr (number of characters, *not* bytes)
    *s, // lpMultiByteStr
    num_chars, // cbMultiByte (size of output string, in bytes)
    NULL, // lpDefaultChar (must be null for UTF-8)
    NULL // lpUsedDefaultChar (must be null for UTF-8)
  );

  if (ret == 0) {
    DWORD err = GetLastError();
    fprintf(stderr, "While converting string to utf-8 (conversion): %d (%x)", err, err);
    exit(132);
  }
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

bool CContains (const char *needle, const char *haystack) {
  return nullptr != strstr(needle, haystack);
}

bool CEquals (const char *a, const char *b) {
  return 0 == strcmp(a, b);
}

} // namespace strings
} // namespace lab
