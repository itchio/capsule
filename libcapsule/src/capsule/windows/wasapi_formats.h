
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

#include <audioclient.h>

#include "../messages_generated.h"

namespace capsule {
namespace wasapi {

static inline messages::SampleFmt ToCapsuleSampleFmt (WAVEFORMATEX *pwfx) {
  if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
    auto pwfxe = reinterpret_cast<WAVEFORMATEXTENSIBLE *>(pwfx);
    if (pwfxe->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
      if (pwfx->wBitsPerSample == 32) {
        return messages::SampleFmt_F32LE;
      } else {
        Log("Wasapi: format is float but not 32, bailing out");
        return messages::SampleFmt_UNKNOWN;
      }
    } else {
      Log("Wasapi: format extensible but not float, bailing out");
      return messages::SampleFmt_UNKNOWN;
    }
  } else {
    Log("Wasapi: format isn't F32LE, bailing out");
    return messages::SampleFmt_UNKNOWN;
  }
  return messages::SampleFmt_UNKNOWN;
}


} // namespace wasapi
} // namespace capsule

