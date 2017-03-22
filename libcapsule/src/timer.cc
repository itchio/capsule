
#include <capsule.h>

#include <chrono>
#include <thread>
#include <mutex>

#include <string.h>

namespace capsule {

static int cur_fps = 60;
static auto frame_interval = std::chrono::microseconds(1000000 / cur_fps);

static bool first_frame = true;
static std::chrono::time_point<std::chrono::steady_clock> first_ts;

std::mutex capdata_mutex;

bool LAB_STDCALL CaptureActive () {
  std::lock_guard<std::mutex> lock(capdata_mutex);
  return capdata.active;
}

bool LAB_STDCALL CaptureTryStart (struct capture_data_settings *settings) {
  std::lock_guard<std::mutex> lock(capdata_mutex);
  if (capdata.active) {
    CapsuleLog("CaptureTryStart: already active, ignoring start");
    return false;
  }

  memcpy(&capdata.settings, settings, sizeof(struct capture_data_settings));
  CapsuleLog("Setting FPS to %d", capdata.settings.fps);
  cur_fps = capdata.settings.fps;
  frame_interval = std::chrono::microseconds(1000000 / cur_fps);
  capdata.active = true;
  return true;
}

bool LAB_STDCALL CaptureTryStop () {
  std::lock_guard<std::mutex> lock(capdata_mutex);
  if (!capdata.active) {
    CapsuleLog("CaptureTryStop: not active, ignoring stop");
    return false;
  }

  capdata.active = false;
  return true;
}

static inline bool FrameReady () {
  static std::chrono::time_point<std::chrono::steady_clock> last_ts;

  if (!CaptureActive()) {
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

int64_t LAB_STDCALL FrameTimestamp () {
  auto frame_timestamp = std::chrono::steady_clock::now() - first_ts;
  auto micro_timestamp = std::chrono::duration_cast<std::chrono::microseconds>(frame_timestamp);
  return (int64_t) micro_timestamp.count();
}

bool LAB_STDCALL CaptureReady () {
  return FrameReady();
}

} // namespace capsule
