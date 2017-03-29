
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

// this little trick is required because apparently
// windows.h defines 'min', and flatbuffers.h uses std::min,
// and they don't play well together. This lets us include
// packet.h even if windows.h was already included.
#if defined(min)
#define lab__hide__min min
#undef min
#endif

#include "../flatbuffers/flatbuffers.h"

#if defined(lab__hide__min)
#define min lab__hide__min
#undef lab__hide__min
#endif

#include "platform.h"

#if defined(LAB_WINDOWS)
#include <io.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#else
#include <unistd.h>
#endif // LAB_WINDOWS

namespace lab {
namespace packet {

/**
 * Read a packet-full of bytes from *file.
 * The returned char* must be delete[]'d.
 *
 * Blocks, returns null on closed pipe
 */
char *Fread(FILE *file);

/**
 * Writes a packet (built with builder) to file.
 * builder.Finish(x) must have been called beforehand.
 */
void Fwrite(const flatbuffers::FlatBufferBuilder &builder, FILE *file);

#if defined(LAB_WINDOWS)

/**
 * Read a packet-full of bytes from *file.
 * The returned char* must be delete[]'d.
 *
 * Blocks, returns null on closed pipe
 */
char *Hread(HANDLE handle);

/**
 * Writes a packet (built with builder) to file.
 * builder.Finish(x) must have been called beforehand.
 */
void Hwrite(const flatbuffers::FlatBufferBuilder &builder, HANDLE handle);

#else // LAB_WINDOWS

/**
 * Read a packet-full of bytes from fd.
 * The returned char* must be delete[]'d.
 *
 * Blocks, returns null on closed pipe
 */
char *Read(int fd);

/**
 * Writes a packet (built with builder) to fd.
 * builder.Finish(x) must have been called beforehand.
 */
void Write(const flatbuffers::FlatBufferBuilder &builder, int fd);

#endif // !LAB_WINDOWS

} // namespace lab
} // namespace packet
