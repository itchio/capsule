
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

#pragma once

#include "platform.h"

#include <string>

namespace lab {
namespace strings {

#if defined(LAB_WINDOWS)

/**
 * Convert an UTF-8 string to a UTF-16 string using the Win32 API.
 */
std::wstring ToWide(const std::string &utf8_string);

/**
 * Convert an UTF-16 string to a UTF-8 string using the Win32 API.
 */
std::string FromWide(const std::wstring &utf16_string);

/**
 * Appends the given argument to a command line such that CommandLineToArgvW
 * will return the argument string unchanged. Arguments in a command line
 * should be separated by spaces; this function does not add these spaces.
 */
void ArgvQuote(const std::wstring& argument, std::wstring& command_line, bool force);

#endif // LAB_WINDOWS

bool CContains(const char *haystack, const char *needle);
bool CEquals(const char *a, const char *b);

} // namespace strings
} // namespace lab
