
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
#include "../ensure.h"
#include "dlopen_hooks.h"

namespace capsule {
namespace alsa {

typedef int (*snd_pcm_open_t)(
  snd_pcm_t **pcmp,
  const char *name,
  snd_pcm_stream_t stream,
  int mode
);
static snd_pcm_open_t snd_pcm_open = nullptr;

typedef snd_pcm_sframes_t (*snd_pcm_writen_t)(
  snd_pcm_t *pcm,
  void **bufs,
  snd_pcm_uframes_t size
);
static snd_pcm_writen_t snd_pcm_writen = nullptr;

typedef snd_pcm_sframes_t (*snd_pcm_writei_t)(
  snd_pcm_t *pcm,
  const void *buffer,
  snd_pcm_uframes_t size
);
static snd_pcm_writei_t snd_pcm_writei = nullptr;

typedef ssize_t (*snd_pcm_frames_to_bytes_t)(
  snd_pcm_t *pcm,
  snd_pcm_sframes_t frames
);
static snd_pcm_frames_to_bytes_t snd_pcm_frames_to_bytes = nullptr;

void EnsureSymbol(void **ptr, const char *name) {
  static void* handle = nullptr;

  if (*ptr) {
    return;
  }

  if (!handle) {
    handle = capsule::dl::NakedOpen("libasound.so.2", RTLD_NOW|RTLD_LOCAL);
    Ensure("real ALSA loaded", !!handle);
  }

  *ptr = dlsym(handle, name);
  Ensure(name, !!*ptr);
}

} // namespace alsa
} // namespace capsule

extern "C" {

// interposed ALSA function
int snd_pcm_open(
  snd_pcm_t **pcmp,
  const char *name,
  snd_pcm_stream_t stream,
  int mode
) {
  capsule::alsa::EnsureSymbol((void**) &capsule::alsa::snd_pcm_open, "snd_pcm_open");

  auto ret = capsule::alsa::snd_pcm_open(pcmp, name, stream, mode);
  capsule::Log("snd_pcm_open: returned %d, pcm address: %p", ret, *pcmp);
  return ret;
};

// interposed ALSA function
snd_pcm_sframes_t snd_pcm_writen(
  snd_pcm_t *pcm,
  void **bufs,
  snd_pcm_uframes_t size
) {
  capsule::alsa::EnsureSymbol((void**) &capsule::alsa::snd_pcm_writen, "snd_pcm_writen");

  auto ret = capsule::alsa::snd_pcm_writen(pcm, bufs, size);
  capsule::Log("snd_pcm_writen: %d frames written", (int) ret);
  return ret;
}

// interposed ALSA function
snd_pcm_sframes_t snd_pcm_writei(
  snd_pcm_t *pcm,
  const void *buffer,
  snd_pcm_uframes_t size
) {
  static FILE *raw;
  if (!raw) {
    raw = fopen("capsule.rawaudio", "wb");
    capsule::Ensure("opened raw", !!raw);
  }

  capsule::alsa::EnsureSymbol((void**) &capsule::alsa::snd_pcm_writei, "snd_pcm_writei");
  capsule::alsa::EnsureSymbol((void**) &capsule::alsa::snd_pcm_frames_to_bytes, "snd_pcm_frames_to_bytes");

  auto ret = capsule::alsa::snd_pcm_writei(pcm, buffer, size);
  capsule::Log("snd_pcm_writei: %d frames written", (int) ret);

  auto bytes = capsule::alsa::snd_pcm_frames_to_bytes(pcm, static_cast<snd_pcm_sframes_t>(ret));
  capsule::Log("snd_pcm_writei: ...that's %" PRIdS " bytes", bytes);

  fwrite(buffer, static_cast<size_t>(bytes), 1, raw);

  return ret;
}

} // extern "C"
