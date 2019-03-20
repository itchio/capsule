
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

#include <vector>
#include <string>

#include "platform.h"

namespace lab {
namespace env {

char **MergeBlocks (char **a, char **b);

std::string Get(std::string name);
bool Set(std::string name, std::string value);

#if defined(LAB_WINDOWS)
std::string Expand(std::string input);
#endif // LAB_WINDOWS

#if defined(LAB_LINUX) || defined(LAB_MACOS)
char **GetBlock();
#endif // LAB_LINUX || LAB_MACOS

} // namespace env
} // namespace lab
