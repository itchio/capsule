
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

namespace lab {

extern const char *kPlatform;

#if defined(_WIN32)
#define LAB_WINDOWS
#elif defined(__APPLE__)
#define LAB_MACOS
#elif defined(__linux__) || defined(__unix__)
#define LAB_LINUX
#else
#error Unsupported platform
#endif

#if defined(LAB_WINDOWS)
#define LAB_STDCALL __stdcall
#define LAB_STDCALL __stdcall
#else
#define LAB_STDCALL
#endif

}
