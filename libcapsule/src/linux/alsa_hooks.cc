
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
#include "../capture.h"
#include "../io.h"
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

typedef int (*snd_pcm_hw_params_current_t)(
  snd_pcm_t *pcm,
  snd_pcm_hw_params_t *params
);
static snd_pcm_hw_params_current_t snd_pcm_hw_params_current;

typedef int (*snd_pcm_hw_params_get_format_t)(
  snd_pcm_hw_params_t *params,
  snd_pcm_format_t *val
);
static snd_pcm_hw_params_get_format_t snd_pcm_hw_params_get_format;

typedef int (*snd_pcm_hw_params_get_channels_t)(
  snd_pcm_hw_params_t *params,
  unsigned int *val
);
static snd_pcm_hw_params_get_channels_t snd_pcm_hw_params_get_channels;

typedef int (*snd_pcm_hw_params_get_rate_t)(
  snd_pcm_hw_params_t *params,
  unsigned int *val,
  int *dir
);
static snd_pcm_hw_params_get_rate_t snd_pcm_hw_params_get_rate;

typedef int (*snd_pcm_hw_params_get_access_t)(
  snd_pcm_hw_params_t *params,
  snd_pcm_access_t *access
);
static snd_pcm_hw_params_get_access_t snd_pcm_hw_params_get_access;

static struct AlsaState {
  bool seen_write;
  bool seen_mmap;
} state = {0};

enum AlsaMethod {
  kAlsaMethodWrite = 0,
  kAlsaMethodMmap,
};

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

const char *FormatName(snd_pcm_format_t format) {
#define ALSA_FORMAT_CASE(fmt) case (fmt): return #fmt;
  switch (format) {
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_UNKNOWN)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_S8)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_U8)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_S16_LE)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_S16_BE)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_U16_LE)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_U16_BE)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_S24_LE)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_S24_BE)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_U24_LE)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_U24_BE)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_S32_LE)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_S32_BE)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_U32_LE)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_U32_BE)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_FLOAT_LE)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_FLOAT_BE)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_FLOAT64_LE)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_FLOAT64_BE)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_IEC958_SUBFRAME_LE)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_IEC958_SUBFRAME_BE)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_MU_LAW)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_A_LAW)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_IMA_ADPCM)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_MPEG)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_GSM)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_SPECIAL)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_S24_3LE)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_S24_3BE)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_U24_3LE)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_U24_3BE)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_S20_3LE)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_S20_3BE)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_U20_3LE)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_U20_3BE)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_S18_3LE)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_S18_3BE)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_U18_3LE)
    ALSA_FORMAT_CASE(SND_PCM_FORMAT_U18_3BE)
    // ALSA_FORMAT_CASE(SND_PCM_FORMAT_S16)
    // ALSA_FORMAT_CASE(SND_PCM_FORMAT_U16)
    // ALSA_FORMAT_CASE(SND_PCM_FORMAT_S24)
    // ALSA_FORMAT_CASE(SND_PCM_FORMAT_U24)
    // ALSA_FORMAT_CASE(SND_PCM_FORMAT_S32)
    // ALSA_FORMAT_CASE(SND_PCM_FORMAT_U32)
    // ALSA_FORMAT_CASE(SND_PCM_FORMAT_FLOAT)
    // ALSA_FORMAT_CASE(SND_PCM_FORMAT_FLOAT64)
    // ALSA_FORMAT_CASE(SND_PCM_FORMAT_IEC958_SUBFRAME)
    default: return "<not known by FormatName>";
  }
}

const char *AccessName(snd_pcm_access_t access) {
#define ALSA_ACCESS_CASE(acc) case (acc): return #acc;
  switch (access) {
    ALSA_ACCESS_CASE(SND_PCM_ACCESS_MMAP_INTERLEAVED)
    ALSA_ACCESS_CASE(SND_PCM_ACCESS_MMAP_NONINTERLEAVED)
    ALSA_ACCESS_CASE(SND_PCM_ACCESS_MMAP_COMPLEX)
    ALSA_ACCESS_CASE(SND_PCM_ACCESS_RW_INTERLEAVED)
    ALSA_ACCESS_CASE(SND_PCM_ACCESS_RW_NONINTERLEAVED)
    default: return "<not known by AccessName>";
  }
}

void NotifyIntercept(snd_pcm_t *pcm) {
  snd_pcm_hw_params_t *params;
  snd_pcm_hw_params_alloca(&params);

  alsa::EnsureSymbol((void**) &alsa::snd_pcm_hw_params_current, "snd_pcm_hw_params_current");
  int ret = alsa::snd_pcm_hw_params_current(pcm, params);
  if (ret != 0) {
    Log("ALSA: Couldn't retrieve HW params, error %d", ret);
    return;
  }

  alsa::EnsureSymbol((void**) &alsa::snd_pcm_hw_params_get_format, "snd_pcm_hw_params_get_format");
  alsa::EnsureSymbol((void**) &alsa::snd_pcm_hw_params_get_channels, "snd_pcm_hw_params_get_channels");
  alsa::EnsureSymbol((void**) &alsa::snd_pcm_hw_params_get_rate, "snd_pcm_hw_params_get_rate");
  alsa::EnsureSymbol((void**) &alsa::snd_pcm_hw_params_get_access, "snd_pcm_hw_params_get_access");

  snd_pcm_format_t format;  
  ret = alsa::snd_pcm_hw_params_get_format(params, &format);
  if (ret != 0) {
    Log("ALSA: Couldn't retrieve HW params format, error %d", ret);
    return;
  }

  unsigned int channels = 0;
  ret = alsa::snd_pcm_hw_params_get_channels(params, &channels);
  if (ret != 0) {
    Log("ALSA: Couldn't retrieve HW params channels, error %d", ret);
    return;
  }

  unsigned int rate;
  int dir;
  ret = alsa::snd_pcm_hw_params_get_rate(params, &rate, &dir);
  if (ret != 0) {
    Log("ALSA: Couldn't retrieve HW params rate, error %d", ret);
    return;
  }

  snd_pcm_access_t access;  
  ret = snd_pcm_hw_params_get_access(params, &access);
  if (ret != 0) {
    Log("ALSA: Couldn't retrieve HW params access, error %d", ret);
    return;
  }

  Log("ALSA config: %s format, %d channels, %d rate, %s access",
    FormatName(format), channels, rate, AccessName(access)
  );

  if (format != SND_PCM_FORMAT_FLOAT_LE) {
    Log("ALSA: format isn't FLOAT_LE, we don't handle that yet");
    return;
  }

  // TODO: ensure that interleaved (or handle planar)
  capture::HasAudioIntercept(messages::SampleFmt_F32LE, rate, channels);
}

void SawMethod(snd_pcm_t *pcm, AlsaMethod method) {
  switch (method) {
    case kAlsaMethodWrite: {
      if (!state.seen_write) {
        capsule::Log("ALSA: saw write method");
        state.seen_write = true;
        NotifyIntercept(pcm);
      }
      break;
    }
    case kAlsaMethodMmap: {
      if (!state.seen_mmap) {
        capsule::Log("ALSA: saw mmap method");
        state.seen_mmap = true;
        NotifyIntercept(pcm);
      }
      break;
    }
  }
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
  capsule::alsa::SawMethod(pcm, capsule::alsa::kAlsaMethodWrite);
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
  capsule::alsa::SawMethod(pcm, capsule::alsa::kAlsaMethodWrite);
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

static const snd_pcm_channel_area_t *_last_areas = nullptr;

// interposed ALSA function
int snd_pcm_mmap_begin(
  snd_pcm_t *pcm,
  const snd_pcm_channel_area_t **areas,
  snd_pcm_uframes_t *offset,
  snd_pcm_uframes_t *frames
) {
  capsule::alsa::SawMethod(pcm, capsule::alsa::kAlsaMethodMmap);
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
    // if (!_raw) {
    //   _raw = fopen("capsule.rawaudio", "wb");
    //   capsule::Ensure("opened raw", !!_raw);
    // }
    // capsule::alsa::EnsureSymbol((void**) &capsule::alsa::snd_pcm_frames_to_bytes, "snd_pcm_frames_to_bytes");
    // auto bytes = capsule::alsa::snd_pcm_frames_to_bytes(pcm, static_cast<snd_pcm_sframes_t>(frames));
    // auto byte_offset = capsule::alsa::snd_pcm_frames_to_bytes(pcm, static_cast<snd_pcm_sframes_t>(offset));
    // DebugLog("snd_pcm_mmap_commit: ...that's %" PRIdS " bytes", bytes);
    // auto area = _last_areas[0];
    // auto buffer = reinterpret_cast<uint8_t*>(area.addr) + (area.first / 8) + byte_offset;
    // fwrite(reinterpret_cast<void*>(buffer), static_cast<size_t>(bytes), 1, _raw);

    if (capsule::capture::Active()) {
      capsule::alsa::EnsureSymbol((void**) &capsule::alsa::snd_pcm_frames_to_bytes, "snd_pcm_frames_to_bytes");
      auto byte_offset = capsule::alsa::snd_pcm_frames_to_bytes(pcm, static_cast<snd_pcm_sframes_t>(offset));
      auto area = _last_areas[0];
      auto buffer = reinterpret_cast<uint8_t*>(area.addr) + (area.first / 8) + byte_offset;
      capsule::io::WriteAudioFrames((char*) buffer, (size_t) frames);
    }
  }

  auto ret = capsule::alsa::snd_pcm_mmap_commit(pcm, offset, frames);
  DebugLog("snd_pcm_mmap_commit: ret %d, offset %d, frames %d", (int) ret,
    (int) offset, (int) frames);
  return ret;
}

} // extern "C"
