
#include "strings.h"

#if defined(LAB_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#endif // LAB_WINDOWS

#include <stdio.h>
#include <stdlib.h>

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
    exit(127);
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
    exit(127);
  }
}

#endif // LAB_WINDOWS

} // namespace strings
} // namespace lab
