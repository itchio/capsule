
#include <capsule.h>
#include <thread>

struct dc_data {
  HWND window;
  HDC hdc_target;
  uint32_t cx;
  uint32_t cy;

  HBITMAP bmp;
  HDC hdc;
  char *frame_data;
};

static struct dc_data data = {0};

static HWND g_hwnd = NULL;
std::thread *g_dc_thread;

static BOOL CALLBACK enum_windows_proc(HWND hwnd, LPARAM lParam) {
  DWORD lpdwProcessId;
  GetWindowThreadProcessId(hwnd, &lpdwProcessId);
  if (lpdwProcessId == lParam) {
    g_hwnd = hwnd;
    return FALSE;
  }
  return TRUE;
}

static void find_hwnd(DWORD pid) {
  if (g_hwnd) {
    return;
  }

  capsule_log("Looking for hwnd");
  EnumWindows(enum_windows_proc, pid);
}

static void capture_hwnd(HWND hwnd) {
  static bool first_frame = true;
  const int components = 4;

  if (!capsule_capture_ready()) {
    return;
  }

  auto timestamp = capsule_frame_timestamp();

  if (first_frame) {
    capsule_write_video_format(data.cx, data.cy, CAPSULE_PIX_FMT_BGRA, 1 /* vflip */, data.cx * components);
    first_frame = false;
  }

  // TODO: error checking
  BitBlt(data.hdc,         // dest dc
         0, 0,             // dest x, y
         data.cx, data.cy, // dest width, height
         data.hdc_target,  // src dc
         0, 0,             // src x, y
         SRCCOPY);

  int frame_data_size = data.cx * data.cy * components;
  capsule_write_video_frame(timestamp, data.frame_data, frame_data_size);
}

static void dc_capture_capture() { capture_hwnd(data.window); }

static void dc_capture_loop() {
  while (true) {
    Sleep(16); // FIXME: that's dumb
    if (!capsule_capture_active()) {
      // TODO: proper cleanup somewhere
      // ReleaseDC(NULL, hdc_target);
      // DeleteDC(hdc);
      // DeleteObject(bmp);
      return;
    }

    dc_capture_capture();
  }
}

static bool dc_capture_init_format() {
  bool success = false;
  data.window = g_hwnd;
  g_hwnd = NULL;

  // TODO: error checking
  data.hdc_target = GetDC(data.window);

  const int MAX_TITLE_LENGTH = 16384;
  wchar_t *window_title = new wchar_t[MAX_TITLE_LENGTH];
  window_title[0] = '\0';
  // TODO: error checking
  GetWindowText(data.window, window_title, MAX_TITLE_LENGTH);

  capsule_log("dc_capture_init_format: initializing for window %S",
              window_title);
  delete[] window_title;

  HDC hdc_target = GetDC(data.window);

  RECT rect;
  // TODO: error checking
  GetClientRect(data.window, &rect);

  data.cx = rect.right;
  data.cy = rect.bottom;

  capsule_log("dc_capture_init_format: window size: %dx%d", data.cx, data.cy);

  // TODO: error checking
  data.hdc = CreateCompatibleDC(NULL);
  BITMAPINFO bi = {0};
  BITMAPINFOHEADER *bih = &bi.bmiHeader;
  bih->biSize = sizeof(BITMAPINFOHEADER);
  bih->biBitCount = 32;
  bih->biWidth = data.cx;
  bih->biHeight = data.cy;
  bih->biPlanes = 1;

  // TODO: error checking
  data.bmp = CreateDIBSection(data.hdc, &bi, DIB_RGB_COLORS,
                              (void **)&data.frame_data, NULL, 0);

  // TODO: error checking
  SelectObject(data.hdc, data.bmp);

  return true;
}

bool dc_capture_init() {
  DWORD pid = GetCurrentProcessId();
  capsule_log("dc_capture_init: our pid is %d", pid);

  g_hwnd = NULL;
  find_hwnd(pid);
  if (g_hwnd == NULL) {
    capsule_log("dc_capture_init: could not find a window, bailing out");
    return false;
  }
  capsule_log("dc_capture_init: our hwnd is %p", g_hwnd);

  if (!dc_capture_init_format()) {
    return false;
  }

  std::thread dc_thread(dc_capture_loop);
  dc_thread.detach();
  g_dc_thread = &dc_thread;

  return true;
}