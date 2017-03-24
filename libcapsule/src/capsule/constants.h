
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

namespace capsule {

enum PixFmt {
  kPixFmtUnknown = 0,
  kPixFmtRgba = 40069,    // R8,  G8,  B8,  A8
  kPixFmtBgra = 40070,    // B8,  G8,  R8,  A8
  kPixFmtRgb10A2 = 40071, // R10, G10, B10, A2

  kPixFmtYuv444P = 60021, // planar Y4 U4 B4
};

// numbers of buffers used for async GPU download
static const int kNumBuffers = 3;

} // namespace capsule
