
#include <thread>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN

#include "../capsule/constants.h"
#include "../capture.h"
#include "../logging.h"
#include "../io.h"

namespace capsule {
namespace dc {

struct State {
  HWND window;
  HDC hdc_target;
  int cx;
  int cy;

  HBITMAP bmp;
  HDC hdc;
  char *frame_data;
};

static State state = {0};

static HWND g_hwnd = NULL;
std::thread *g_dc_thread;

static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
  DWORD lpdwProcessId;
  GetWindowThreadProcessId(hwnd, &lpdwProcessId);
  if (lpdwProcessId == lParam) {
    g_hwnd = hwnd;
    return FALSE;
  }
  return TRUE;
}

static void FindHwnd(DWORD pid) {
  if (g_hwnd) {
    return;
  }

  Log("Looking for hwnd");
  EnumWindows(EnumWindowsProc, pid);
}

static void CaptureHwnd(HWND hwnd) {
  static bool first_frame = true;
  const int components = 4;

  if (!capture::Ready()) {
    return;
  }

  auto timestamp = capture::FrameTimestamp();

  if (first_frame) {
    io::WriteVideoFormat(state.cx, state.cy, kPixFmtBgra, true /* vflip */,
                         state.cx * components);
    first_frame = false;
  }

  // TODO: error checking
  BitBlt(state.hdc,         // dest dc
         0, 0,             // dest x, y
         state.cx, state.cy, // dest width, height
         state.hdc_target,  // src dc
         0, 0,             // src x, y
         SRCCOPY);

  int frame_data_size = state.cx * state.cy * components;
  io::WriteVideoFrame(timestamp, state.frame_data, frame_data_size);
}

static void Capture() {
  CaptureHwnd(state.window);
}

static void Loop() {
  while (true) {
    Sleep(16); // FIXME: that's dumb
    if (!capture::Active()) {
      // TODO: proper cleanup somewhere
      // ReleaseDC(NULL, hdc_target);
      // DeleteDC(hdc);
      // DeleteObject(bmp);
      return;
    }

    Capture();
  }
}

static bool InitFormat() {
  bool success = false;
  state.window = g_hwnd;
  g_hwnd = NULL;

  // TODO: error checking
  state.hdc_target = GetDC(state.window);

  const int MAX_TITLE_LENGTH = 16384;
  wchar_t *window_title = new wchar_t[MAX_TITLE_LENGTH];
  window_title[0] = '\0';
  // TODO: error checking
  GetWindowText(state.window, window_title, MAX_TITLE_LENGTH);

  Log("dc::InitFormat: initializing for window %S", window_title);
  delete[] window_title;

  HDC hdc_target = GetDC(state.window);

  RECT rect;
  // TODO: error checking
  GetClientRect(state.window, &rect);

  state.cx = rect.right;
  state.cy = rect.bottom;

  Log("dc_capture_init_format: window size: %dx%d", state.cx, state.cy);

  // TODO: error checking
  state.hdc = CreateCompatibleDC(NULL);
  BITMAPINFO bi = {0};
  BITMAPINFOHEADER *bih = &bi.bmiHeader;
  bih->biSize = sizeof(BITMAPINFOHEADER);
  bih->biBitCount = 32;
  bih->biWidth = state.cx;
  bih->biHeight = state.cy;
  bih->biPlanes = 1;

  // TODO: error checking
  state.bmp = CreateDIBSection(state.hdc, &bi, DIB_RGB_COLORS,
                              (void **)&state.frame_data, nullptr, 0);

  // TODO: error checking
  SelectObject(state.hdc, state.bmp);

  return true;
}

bool Init() {
  DWORD pid = GetCurrentProcessId();
  Log("dc::Init: our pid is %d", pid);

  g_hwnd = NULL;
  FindHwnd(pid);
  if (g_hwnd == NULL) {
    Log("dc::Init: could not find a window, bailing out");
    return false;
  }
  Log("dc::Init: our hwnd is %p", g_hwnd);

  if (!InitFormat()) {
    return false;
  }

  g_dc_thread = new std::thread(Loop);

  return true;
}

} // namespace dc
} // namespace capsule
