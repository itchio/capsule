
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

// #define DebugLog(...) capsule::Log(__VA_ARGS__)
#define DebugLog(...)

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

typedef int (*snd_async_add_pcm_handler_t)(
  snd_async_handler_t **handler,
  snd_pcm_t *pcm,
  snd_async_callback_t callback,
  void *private_data
);
static snd_async_add_pcm_handler_t snd_async_add_pcm_handler = nullptr;

typedef int (*snd_pcm_mmap_begin_t)(
  snd_pcm_t *pcm,
  const snd_pcm_channel_area_t **areas,
  snd_pcm_uframes_t *offset,
  snd_pcm_uframes_t *frames
);
static snd_pcm_mmap_begin_t snd_pcm_mmap_begin = nullptr;

typedef snd_pcm_sframes_t (*snd_pcm_mmap_commit_t)(
  snd_pcm_t *pcm,
  snd_pcm_uframes_t offset,
  snd_pcm_uframes_t frames
);
static snd_pcm_mmap_commit_t snd_pcm_mmap_commit = nullptr;

static struct AlsaState {
  bool seen_write;
  bool seen_callback;
  bool seen_mmap;
} state = {0};

enum AlsaMethod {
  kAlsaMethodWrite = 0,
  kAlsaMethodCallback,
  kAlsaMethodMmap,
};

void SawMethod(AlsaMethod method) {
  switch (method) {
    case kAlsaMethodWrite: {
      if (!state.seen_write) {
        capsule::Log("ALSA: saw write method");
        state.seen_write = true;
      }
      break;
    }
    case kAlsaMethodCallback: {
      if (!state.seen_callback) {
        capsule::Log("ALSA: saw callback method");
        state.seen_callback = true;
      }
      break;
    }
    case kAlsaMethodMmap: {
      if (!state.seen_mmap) {
        capsule::Log("ALSA: saw mmap method");
        state.seen_mmap = true;
      }
      break;
    }
  }
}

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
  capsule::alsa::SawMethod(capsule::alsa::kAlsaMethodWrite);
  capsule::alsa::EnsureSymbol((void**) &capsule::alsa::snd_pcm_writen, "snd_pcm_writen");

  auto ret = capsule::alsa::snd_pcm_writen(pcm, bufs, size);
  DebugLog("snd_pcm_writen: %d frames written", (int) ret);
  return ret;
}

static FILE *_raw;

// interposed ALSA function
snd_pcm_sframes_t snd_pcm_writei(
  snd_pcm_t *pcm,
  const void *buffer,
  snd_pcm_uframes_t size
) {
  capsule::alsa::SawMethod(capsule::alsa::kAlsaMethodWrite);
  capsule::alsa::EnsureSymbol((void**) &capsule::alsa::snd_pcm_writei, "snd_pcm_writei");

  auto ret = capsule::alsa::snd_pcm_writei(pcm, buffer, size);
  DebugLog("snd_pcm_writei: %d frames written", (int) ret);

  {
    if (!_raw) {
      _raw = fopen("capsule.rawaudio", "wb");
      capsule::Ensure("opened raw", !!_raw);
    }
    capsule::alsa::EnsureSymbol((void**) &capsule::alsa::snd_pcm_frames_to_bytes, "snd_pcm_frames_to_bytes");
    auto bytes = capsule::alsa::snd_pcm_frames_to_bytes(pcm, static_cast<snd_pcm_sframes_t>(ret));
    DebugLog("snd_pcm_writei: ...that's %" PRIdS " bytes", bytes);
    fwrite(buffer, static_cast<size_t>(bytes), 1, _raw);
  }

  return ret;
}

// interposed ALSA function
int snd_async_add_pcm_handler(
  snd_async_handler_t **handler,
  snd_pcm_t *pcm,
  snd_async_callback_t callback,
  void *private_data
) {
  capsule::alsa::SawMethod(capsule::alsa::kAlsaMethodCallback);
  capsule::alsa::EnsureSymbol((void**) &capsule::alsa::snd_async_add_pcm_handler, "snd_async_add_pcm_handler");

  auto ret = capsule::alsa::snd_async_add_pcm_handler(handler, pcm, callback, private_data);
  capsule::Log("snd_async_add_pcm_handler: ret %d, handler address = %p", ret, *handler);
  return ret;
}

static const snd_pcm_channel_area_t *_last_areas = nullptr;

// interposed ALSA function
int snd_pcm_mmap_begin(
  snd_pcm_t *pcm,
  const snd_pcm_channel_area_t **areas,
  snd_pcm_uframes_t *offset,
  snd_pcm_uframes_t *frames
) {
  capsule::alsa::SawMethod(capsule::alsa::kAlsaMethodMmap);
  capsule::alsa::EnsureSymbol((void**) &capsule::alsa::snd_pcm_mmap_begin, "snd_pcm_mmap_begin");

  DebugLog("snd_pcm_mmap_begin: input offset %d, input frames %d",
    (int) *offset, (int) *frames);
  auto ret = capsule::alsa::snd_pcm_mmap_begin(pcm, areas, offset, frames);
  DebugLog("snd_pcm_mmap_begin: ret %d, offset %d, frames %d", (int) ret,
    (int) *offset, (int) *frames);
  if (ret == 0) {
    DebugLog("snd_pcm_mmap_begin: area first: %u, step %u", areas[0]->first, areas[0]->step);
    _last_areas = *areas;
  }
  return ret;
}

// interposed ALSA function
snd_pcm_sframes_t snd_pcm_mmap_commit(
  snd_pcm_t *pcm,
  snd_pcm_uframes_t offset,
  snd_pcm_uframes_t frames
) {
  capsule::alsa::EnsureSymbol((void**) &capsule::alsa::snd_pcm_mmap_commit, "snd_pcm_mmap_commit");

  if (_last_areas) {
    if (!_raw) {
      _raw = fopen("capsule.rawaudio", "wb");
      capsule::Ensure("opened raw", !!_raw);
    }
    capsule::alsa::EnsureSymbol((void**) &capsule::alsa::snd_pcm_frames_to_bytes, "snd_pcm_frames_to_bytes");
    auto bytes = capsule::alsa::snd_pcm_frames_to_bytes(pcm, static_cast<snd_pcm_sframes_t>(frames));
    auto byte_offset = capsule::alsa::snd_pcm_frames_to_bytes(pcm, static_cast<snd_pcm_sframes_t>(offset));
    DebugLog("snd_pcm_mmap_commit: ...that's %" PRIdS " bytes", bytes);
    auto area = _last_areas[0];
    auto buffer = reinterpret_cast<uint8_t*>(area.addr) + (area.first / 8) + byte_offset;
    fwrite(reinterpret_cast<void*>(buffer), static_cast<size_t>(bytes), 1, _raw);
  }

  auto ret = capsule::alsa::snd_pcm_mmap_commit(pcm, offset, frames);
  DebugLog("snd_pcm_mmap_commit: ret %d, offset %d, frames %d", (int) ret,
    (int) offset, (int) frames);
  return ret;
}

} // extern "C"
