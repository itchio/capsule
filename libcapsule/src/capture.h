
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

#include <lab/types.h>
#include <capsule/messages_generated.h>

namespace capsule {
namespace capture {

// numbers of buffers used for async GPU download
static const int kNumBuffers = 3;

struct Settings {
  int fps;
  int size_divider;
  bool gpu_color_conv;
};

struct State {
  bool saw_gl;
  bool saw_d3d9;
  bool saw_dxgi;

  bool has_audio_intercept;
  messages::SampleFmt audio_intercept_format;
  int audio_intercept_rate;
  int audio_intercept_channels;

  bool active;
  Settings settings;
};

enum Backend {
  kBackendUnknown = 0,
  kBackendGL,
  kBackendD3D9,
  kBackendDXGI,
};

bool Ready();
bool Active();
void Start(Settings *settings);
void Stop();

int64_t FrameTimestamp();

void SawBackend(Backend backend);
void HasAudioIntercept(messages::SampleFmt format, int rate, int channels);
State *GetState();

} // namespace capture
} // namespace capsule
