
#pragma once

#include "platform.h"

#include <string>

namespace lab {
namespace strings {

#if defined(LAB_WINDOWS)

/**
 * Convert s (UTF-8) to a wide-char (UCS-2) string, for use
 * with win32 functions.
 * Note: ToWideChar mallocs the result, the caller is responsible for freeing it.
 */
void ToWideChar (const char *s, wchar_t **ws);

/**
 * Convert ws (UCS-2) to a UTF-8 string, for internal use.
 * Note: FromWideChar mallocs the result, the caller is responsible for freeing it.
 */
void FromWideChar (const wchar_t *ws, char **s);

/**
 * Appends the given argument to a command line such that CommandLineToArgvW
 * will return the argument string unchanged. Arguments in a command line
 * should be separated by spaces; this function does not add these spaces.
 */
void ArgvQuote (const std::wstring& argument, std::wstring& command_line, bool force);

#endif // LAB_WINDOWS

bool CContains (const char *needle, const char *haystack);
bool CEquals (const char *a, const char *b);

} // namespace strings
} // namespace lab
