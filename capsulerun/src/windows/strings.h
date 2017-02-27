
#pragma once

/**
 * Convert s (UTF-8) to a wide-char (UCS-2) string, for use
 * with win32 functions.
 * Note: toWideChar mallocs the result, the caller is responsible for freeing it.
 */
void toWideChar (const char *s, wchar_t **ws);

/**
 * Convert ws (UCS-2) to a UTF-8 string, for internal use.
 * Note: fromWideChar mallocs the result, the caller is responsible for freeing it.
 */
void fromWideChar (const wchar_t *ws, char **s);