
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

#include "alsa_hooks.h"

#include <alsa/asoundlib.h>
#include <alsa/pcm.h>

#include "../logging.h"
#include "dlopen_hooks.h"

namespace capsule {
namespace alsa {

typedef int (*snd_pcm_open_t)(
  snd_pcm_t **pcmp,
  const char *name,
  snd_pcm_stream_t stream,
  int mode
);
static snd_pcm_open_t _snd_pcm_open = nullptr;

} // namespace alsa
} // namespace capsule

extern "C" {

int snd_pcm_open(
  snd_pcm_t **pcmp,
  const char *name,
  snd_pcm_stream_t stream,
  int mode
) {
  capsule::Log("In hooked snd_pcm_open!");
  if (!capsule::alsa::_snd_pcm_open) {
    capsule::Log("Loading real libasound...");
    void *handle = capsule::dl::NakedOpen("libasound.so.2", RTLD_NOW|RTLD_LOCAL);
    capsule::Log("Finding real snd_pcm_open...");
    capsule::alsa::_snd_pcm_open = (capsule::alsa::snd_pcm_open_t) dlsym(handle, "snd_pcm_open");
  }

  capsule::Log("Calling real snd_pcm_open...");
  auto ret = capsule::alsa::_snd_pcm_open(pcmp, name, stream, mode);
  capsule::Log("Return value: %d, pcm address: %p", ret, *pcmp);
  return ret;
};

} // extern "C"
