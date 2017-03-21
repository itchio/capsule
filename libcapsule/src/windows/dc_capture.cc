
#include <capsule.h>
#include <thread>

struct dc_data {
  HWND window;
  HDC hdc_target;
  int cx;
  int cy;

  HBITMAP bmp;
  HDC hdc;
  char *frame_data;
};

static struct dc_data data = {0};

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

  CapsuleLog("Looking for hwnd");
  EnumWindows(EnumWindowsProc, pid);
}

static void CaptureHwnd(HWND hwnd) {
  static bool first_frame = true;
  const int components = 4;

  if (!capsule::CaptureReady()) {
    return;
  }

  auto timestamp = capsule::FrameTimestamp();

  if (first_frame) {
    capsule::io::WriteVideoFormat(data.cx, data.cy, CAPSULE_PIX_FMT_BGRA, 1 /* vflip */, data.cx * components);
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
  capsule::io::WriteVideoFrame(timestamp, data.frame_data, frame_data_size);
}

static void DcCaptureCapture() {
  CaptureHwnd(data.window);
}

static void DcCaptureLoop() {
  while (true) {
    Sleep(16); // FIXME: that's dumb
    if (!capsule::CaptureActive()) {
      // TODO: proper cleanup somewhere
      // ReleaseDC(NULL, hdc_target);
      // DeleteDC(hdc);
      // DeleteObject(bmp);
      return;
    }

    DcCaptureCapture();
  }
}

static bool DcCaptureInitFormat() {
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

  CapsuleLog("dc_capture_init_format: initializing for window %S",
              window_title);
  delete[] window_title;

  HDC hdc_target = GetDC(data.window);

  RECT rect;
  // TODO: error checking
  GetClientRect(data.window, &rect);

  data.cx = rect.right;
  data.cy = rect.bottom;

  CapsuleLog("dc_capture_init_format: window size: %dx%d", data.cx, data.cy);

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

bool DcCaptureInit() {
  DWORD pid = GetCurrentProcessId();
  CapsuleLog("DcCaptureInit: our pid is %d", pid);

  g_hwnd = NULL;
  FindHwnd(pid);
  if (g_hwnd == NULL) {
    CapsuleLog("DcCaptureInit: could not find a window, bailing out");
    return false;
  }
  CapsuleLog("DcCaptureInit: our hwnd is %p", g_hwnd);

  if (!DcCaptureInitFormat()) {
    return false;
  }

  std::thread dc_thread(DcCaptureLoop);
  dc_thread.detach();
  g_dc_thread = &dc_thread;

  return true;
}