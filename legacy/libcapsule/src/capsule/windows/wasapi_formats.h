
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
        return messages::SampleFmt_F32;
      } else if (pwfx->wBitsPerSample == 64) {
        return messages::SampleFmt_F64;
      } else {
        Log("Wasapi: format is float but %d bits per sample, bailing out", pwfx->wBitsPerSample);
      }
    } else if (pwfxe->SubFormat == KSDATAFORMAT_SUBTYPE_PCM) {
      if (pwfx->wBitsPerSample == 8) {
        return messages::SampleFmt_U8;
      } else if (pwfx->wBitsPerSample == 16) {
        return messages::SampleFmt_S16;
      } else if (pwfx->wBitsPerSample == 32) {
        return messages::SampleFmt_S32;
      } else {
        Log("Wasapi: format is pcm but %d bits per sample, bailing out", pwfx->wBitsPerSample);
      }
    } else {
      Log("Wasapi: extensible but neither float nor pcm, bailing out");
    }
  } else if(pwfx->wFormatTag == WAVE_FORMAT_PCM) {
    Log("Wasapi: format isn't extensible");
    if (pwfx->wBitsPerSample == 8) {
      return messages::SampleFmt_U8;
    } else if (pwfx->wBitsPerSample == 16) {
      return messages::SampleFmt_S16;
    } else {
      Log("Wasapi: non-extensible pcm format but %d bits per sample, bailing out", pwfx->wBitsPerSample);
    }
  } else {
    Log("Wasapi: non-extensible, non-pcm format, bailing out");
  }
  return messages::SampleFmt_UNKNOWN;
}


} // namespace wasapi
} // namespace capsule

