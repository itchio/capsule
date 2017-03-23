
#include "capture.h"

#include <string.h>

#include <chrono>
#include <thread>
#include <mutex>

#include <lab/platform.h>

#include "logging.h"

#if defined(LAB_WINDOWS)
#include "windows/win_capture.h"
#endif

namespace capsule {
namespace capture {

static int cur_fps = 60;
static auto frame_interval = std::chrono::microseconds(1000000 / cur_fps);

static bool first_frame = true;
static std::chrono::time_point<std::chrono::steady_clock> first_ts;

std::mutex state_mutex;

static State state = {0};

bool Active () {
  std::lock_guard<std::mutex> lock(state_mutex);
  return state.active;
}

bool TryStart (struct Settings *settings) {
  std::lock_guard<std::mutex> lock(state_mutex);
  if (state.active) {
    Log("TryStart: already active, ignoring start");
    return false;
  }

  memcpy(&state.settings, settings, sizeof(state.settings));
  Log("Setting FPS to %d", state.settings.fps);
  cur_fps = state.settings.fps;
  frame_interval = std::chrono::microseconds(1000000 / cur_fps);
  state.active = true;
  return true;
}

bool TryStop () {
  std::lock_guard<std::mutex> lock(state_mutex);
  if (!state.active) {
    Log("TryStop: not active, ignoring stop");
    return false;
  }

  state.active = false;
  return true;
}

static inline bool FrameReady () {
  static std::chrono::time_point<std::chrono::steady_clock> last_ts;

  if (!Active()) {
    first_frame = true;
    return false;
  }

  auto interval = frame_interval;

  if (first_frame) {
    first_frame = false;
    first_ts = std::chrono::steady_clock::now();
    last_ts = first_ts;
    return false;
  }

  auto t = std::chrono::steady_clock::now();
  auto elapsed = t - last_ts;

  if (elapsed < interval) {
    return false;
  }

  // logic taken from libobs  
  bool dragging = (elapsed > (interval * 2));
  if (dragging) {
    last_ts = t;
  } else {
    last_ts = last_ts + interval;
  }
  return true;
}

int64_t FrameTimestamp () {
  auto frame_timestamp = std::chrono::steady_clock::now() - first_ts;
  auto micro_timestamp = std::chrono::duration_cast<std::chrono::microseconds>(frame_timestamp);
  return (int64_t) micro_timestamp.count();
}

bool Ready () {
  return FrameReady();
}

// TODO: inform capsulerun!
void SawBackend(Backend backend) {
  switch (backend) {
    case kBackendGL: {
      if (!state.saw_gl) {
        Log("Saw GL backend!");
        state.saw_gl = true;
      }
      break;
    }
    case kBackendD3D9: {
      if (!state.saw_d3d9) {
        Log("Saw D3D9 backend!");
        state.saw_d3d9 = true;
      }
      break;
    }
    case kBackendDXGI: {
      if (!state.saw_dxgi) {
        Log("Saw DXGI backend!");
        state.saw_dxgi = true;
      }
      break;
    }
    default: {
      Log("Saw unknown backend %d", backend);
    }
  }
}

State *GetState() {
  return &state;
}

void Start (Settings *settings) {

#if defined(LAB_WINDOWS)

    if (state.saw_dxgi || state.saw_d3d9 || state.saw_gl) {
        // cool, these will initialize on next present/swapbuffers
        if (TryStart(settings)) {
            Log("Started D3D9/DXGI/GL capture");
            return;
        }
    } else {
        // try dc capture then
        bool success = dc::Init();
        if (!success) {
            Log("Cannot start capture: no capture method available");
            return;
        }

        if (TryStart(settings)) {
            Log("Started DC capture");
            return;
        } else {
            Log("stub: DC initialized but capture failed to start, should clean up");
            return;
        }
    }

#else // LAB_WINDOWS

    if (state.saw_gl) {
        // cool, it'll initialize on next swapbuffers
        if (TryStart(settings)) {
            Log("Started GL capture");
            return;
        }
    }

#endif // !LAB_WINDOWS

    Log("Cannot start capture: no capture method available");
}

void Stop() {
    if (TryStop()) {
        Log("capsule_capture_stop: stopped!");
    }
}

} // namespace capture
} // namespace capsule
