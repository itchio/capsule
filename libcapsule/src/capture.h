
#pragma once

#include <lab/types.h>

namespace capsule {
namespace capture {

struct Settings {
  int fps;
  int size_divider;
  bool gpu_color_conv;
};

struct State {
  bool saw_gl;
  bool saw_d3d9;
  bool saw_dxgi;

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
State *GetState();

} // namespace capture
} // namespace capsule
