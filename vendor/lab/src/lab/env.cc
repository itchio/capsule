
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

#include "env.h"

#include <stdlib.h>

#include "platform.h"
#include "strings.h"

#if defined(LAB_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#endif // LAB_WINDOWS

#if defined(LAB_LINUX) || defined(LAB_MACOS)
extern char **environ;
#endif

namespace lab {
namespace env {

#if defined(LAB_WINDOWS)
const int kMaxCharacters = 16 * 1024;
#endif

char **MergeBlocks (char **a, char **b) {
    size_t total_size = 0;
    char **p = a;
    while (*p) {
        total_size++;
        p++;
    }

    p = b;
    while (*p) {
        total_size++;
        p++;
    }

    char **res = (char **) calloc(total_size + 1, sizeof(char *));
    size_t i = 0;

    p = a;
    while (*p) {
        res[i++] = *p;
        p++;
    }

    p = b;
    while (*p) {
        res[i++] = *p;
        p++;
    }

    res[i] = NULL;

    return res;
}

std::string Get(std::string name) {
#if defined(LAB_WINDOWS)
    auto name_w = strings::ToWide(name);
    const size_t value_w_characters = kMaxCharacters;
    wchar_t *value_w = new wchar_t[value_w_characters];
    int ret = GetEnvironmentVariableW(name_w.c_str(), value_w, value_w_characters);

    if (ret == 0) {
        delete[] value_w;
        return "";
    } else {
        auto result = strings::FromWide(std::wstring(value_w));
        delete[] value_w;
        return result;
    }
#else // LAB_WINDOWS
    // doesn't need to be freed, points to environment block
    const char *value = getenv(name.c_str());
    if (!value) {
        return "";
    } else {
        return std::string(value);
    }
#endif // !LAB_WINDOWS
}

bool Set(std::string name, std::string value) {
#if defined(LAB_WINDOWS)
    auto name_w = strings::ToWide(name);
    auto value_w = strings::ToWide(value);

    return TRUE == SetEnvironmentVariableW(name_w.c_str(), value_w.c_str());
#else  // LAB_WINDOWS
    int result = setenv(name.c_str(), value.c_str(), 1 /* overwrite */);
    return result == 0;
#endif // !LAB_WINDOWS
}

#if defined(LAB_WINDOWS)
std::string Expand(std::string input) {
    auto input_w = strings::ToWide(input.c_str());
    const size_t output_w_characters = kMaxCharacters;
    wchar_t *output_w = new wchar_t[output_w_characters];

    ExpandEnvironmentStringsW(input_w.c_str(), output_w, output_w_characters);

    auto result = strings::FromWide(std::wstring(output_w));
    delete[] output_w;
    return result;
}
#endif // LAB_WINDOWS

#if defined(LAB_LINUX) || defined(LAB_MACOS)
char **GetBlock() {
    return environ;
}
#endif // LAB_LINUX || LAB_MACOS

} // namespace env
} // namespace lab
