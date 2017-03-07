
#include <capsule.h>

#include <chrono>
#include <thread>
#include <mutex>

#include <string.h>

static int cur_fps = 60;
static auto frame_interval = std::chrono::microseconds(1000000 / cur_fps);

static bool first_frame = true;
static int frame_count = 0;
static std::chrono::time_point<std::chrono::steady_clock> first_ts;

using namespace std;

mutex capdata_mutex;

bool CAPSULE_STDCALL capsule_capture_active () {
  lock_guard<mutex> lock(capdata_mutex);
  return capdata.active;
}

bool CAPSULE_STDCALL capsule_capture_try_start (struct capture_data_settings *settings) {
  lock_guard<mutex> lock(capdata_mutex);
  if (capdata.active) {
    capsule_log("capsule_capture_try_start: already active, ignoring start");
    return false;
  }

  memcpy(&capdata.settings, settings, sizeof(struct capture_data_settings));
  capsule_log("Setting FPS to %d", capdata.settings.fps);
  cur_fps = capdata.settings.fps;
  frame_interval = std::chrono::microseconds(1000000 / cur_fps);
  capdata.active = true;
  return true;
}

bool CAPSULE_STDCALL capsule_capture_try_stop () {
  lock_guard<mutex> lock(capdata_mutex);
  if (!capdata.active) {
    capsule_log("capsule_capture_try_stop: not active, ignoring stop");
    return false;
  }

  capdata.active = false;
  return true;
}

static inline bool capsule_frame_ready () {
  static std::chrono::time_point<std::chrono::steady_clock> last_ts;

  if (!capsule_capture_active()) {
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

int64_t CAPSULE_STDCALL capsule_frame_timestamp () {
  auto frame_timestamp = std::chrono::steady_clock::now() - first_ts;
  auto micro_timestamp = std::chrono::duration_cast<std::chrono::microseconds>(frame_timestamp);
  return (int64_t) micro_timestamp.count();
}

bool CAPSULE_STDCALL capsule_capture_ready () {
  return capsule_frame_ready();
}
