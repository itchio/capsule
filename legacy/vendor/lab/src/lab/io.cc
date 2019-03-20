
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

#include "io.h"

namespace lab {
namespace io {

FILE* Fopen(std::string path, std::string mode) {
#if defined(LAB_WINDOWS)
  auto path_w = strings::ToWide(path);
  auto mode_w = strings::ToWide(mode);
  return _wfsopen(path_w.c_str(), mode_w.c_str(), _SH_DENYNO);
#else
  return fopen(path.c_str(), mode.c_str());
#endif
}

} // namespace io
} // namespace lab
