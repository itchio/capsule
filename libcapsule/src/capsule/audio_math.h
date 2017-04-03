
/*
 *  capsule - the game recording and overlay toolkit
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
 * https://github.com/itchio/capsule/blob/master/LICENSE
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#pragma once

#include "messages_generated.h"

namespace capsule {
namespace audio {

// Return the width of a single sample, given a format, or 0 on error
static inline int64_t SampleWidth (messages::SampleFmt format) {
  switch (format) {
    case messages::SampleFmt_U8:
      return 8;
    case messages::SampleFmt_S16:
      return 16;
    case messages::SampleFmt_S32:
      return 32;
    case messages::SampleFmt_F32:
      return 32;
    case messages::SampleFmt_F64:
      return 64;
    default:
      return 0;
  }
}

} // namespace audio
} // namespace capsule
