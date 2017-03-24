
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

#include "pulse_dynamic.h"

#include <dlfcn.h>
#include <string.h>

#include "../macros.h"

namespace capsule {
namespace pulse {

static bool loaded;
pa_simple_new_t SimpleNew;
pa_simple_read_t SimpleRead;
pa_simple_free_t SimpleFree;

pa_mainloop_new_t MainloopNew;
pa_mainloop_get_api_t MainloopGetApi;
pa_mainloop_run_t MainloopRun;
pa_mainloop_quit_t MainloopQuit;
pa_mainloop_free_t MainloopFree;

pa_context_new_t ContextNew;
pa_context_connect_t ContextConnect;
pa_context_set_state_callback_t ContextSetStateCallback;
pa_context_get_state_t ContextGetState;
pa_context_get_server_info_t ContextGetServerInfo;
pa_context_get_sink_info_by_name_t ContextGetSinkInfoByName;
pa_context_disconnect_t ContextDisconnect;

bool Load() {
  if (loaded) {
    return true;
  }

#define PULSESYM(var, sym) { \
  var = reinterpret_cast<sym ## _t>(dlsym(handle, #sym));\
  if (! var) { \
    CapsuleLog("Could not find PulseAudio function %s", #sym); \
    return false; \
  } \
}

  void *handle = dlopen("libpulse-simple.so", RTLD_NOW);
  if (!handle) {
    handle = dlopen("libpulse-simple.so.0", RTLD_NOW);
  }
  if (!handle) {
    CapsuleLog("Could not load libpulse-simple.so(.0)");
    return false;
  }

  PULSESYM(SimpleNew, pa_simple_new)
  PULSESYM(SimpleRead, pa_simple_read);
  PULSESYM(SimpleFree, pa_simple_free);

  handle = dlopen("libpulse.so", RTLD_NOW);
  if (!handle) {
    handle = dlopen("libpulse.so.0", RTLD_NOW);
  }
  if (!handle) {
    CapsuleLog("Could not load libpulse.so(.0)");
    return false;
  }

  PULSESYM(MainloopNew, pa_mainloop_new)
  PULSESYM(MainloopGetApi, pa_mainloop_get_api);
  PULSESYM(MainloopRun, pa_mainloop_run);
  PULSESYM(MainloopQuit, pa_mainloop_quit);
  PULSESYM(MainloopFree, pa_mainloop_free);
  PULSESYM(ContextNew, pa_context_new);
  PULSESYM(ContextConnect, pa_context_connect);
  PULSESYM(ContextSetStateCallback, pa_context_set_state_callback);
  PULSESYM(ContextGetState, pa_context_get_state);
  PULSESYM(ContextGetServerInfo, pa_context_get_server_info);
  PULSESYM(ContextGetSinkInfoByName, pa_context_get_sink_info_by_name);
  PULSESYM(ContextDisconnect, pa_context_disconnect);

  return true;
}

struct SinkSeeker {
  pa_mainloop *loop;
  char *default_sink_monitor;
};

void SinkInfoCb(pa_context * /* unused */, const pa_sink_info *i, int eol, void *userdata) {
  auto ss = reinterpret_cast<SinkSeeker *>(userdata);

  if (eol) {
    MainloopQuit(ss->loop, 0);
    return;
  }

  ss->default_sink_monitor = strdup(i->monitor_source_name);
}

void ServerInfoCb(pa_context *ctx, const pa_server_info *i, void *userdata) {
  ContextGetSinkInfoByName(ctx, i->default_sink_name, SinkInfoCb, userdata);
}

void StateCb(pa_context *ctx, void *userdata) {
  auto state = ContextGetState(ctx);

  switch (state) {
  case PA_CONTEXT_READY: {
    ContextGetServerInfo(ctx, ServerInfoCb, userdata);
    break;
  }
  case PA_CONTEXT_FAILED: {
    CapsuleLog("Failed to connect to PulseAudio");
    auto ss = reinterpret_cast<SinkSeeker *>(userdata);
    MainloopQuit(ss->loop, 1);
    break;
  }
  default: {
    // ignore
  }
  }
}

char *GetDefaultSinkMonitor() {
  SinkSeeker ss;
  ss.default_sink_monitor = nullptr;
  ss.loop = MainloopNew();

  auto ctx = ContextNew(MainloopGetApi(ss.loop), "capsule");
  ContextSetStateCallback(ctx, StateCb, &ss);
  ContextConnect(ctx, nullptr, PA_CONTEXT_NOFLAGS, nullptr);
  MainloopRun(ss.loop, nullptr);
  ContextDisconnect(ctx);
  MainloopFree(ss.loop);

  return ss.default_sink_monitor;
}

} // namespace pulse
} // namespace capsule
