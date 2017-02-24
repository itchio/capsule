
#include <stdio.h>
#include <NktHookLib.h>

// std::thread
#include <thread>

// ArgvQuote
#include "arguments.h"

#include "capsulerun.h"

using namespace std;

static bool connected = false;
static DWORD exitCode = 0;

void toWideChar (const char *s, wchar_t **ws) {
  int wchars_num = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
  *ws = (wchar_t *) malloc(wchars_num * sizeof(wchar_t));
  MultiByteToWideChar(CP_UTF8, 0, s, -1, *ws, wchars_num);
}

int receive_video_format (encoder_private_t *p, video_format_t *vfmt) {
  DWORD bytes_read;
  BOOL success = FALSE;

  int64_t num;

  success = ReadFile(p->pipe_handle, &num, sizeof(int64_t), &bytes_read, NULL);
  if (!success || bytes_read < sizeof(int64_t)) {
    capsule_log("Could not read width");
    return 1;
  }
  vfmt->width = (int) num;

  success = ReadFile(p->pipe_handle, &num, sizeof(int64_t), &bytes_read, NULL);
  if (!success || bytes_read < sizeof(int64_t)) {
    capsule_log("Could not read height");
    return 1;
  }
  vfmt->height = (int) num;

  success = ReadFile(p->pipe_handle, &num, sizeof(int64_t), &bytes_read, NULL);
  if (!success || bytes_read < sizeof(int64_t)) {
    capsule_log("Could not read format");
    return 1;
  }
  vfmt->format = (capsule_pix_fmt_t) num;

  success = ReadFile(p->pipe_handle, &num, sizeof(int64_t), &bytes_read, NULL);
  if (!success || bytes_read < sizeof(int64_t)) {
    capsule_log("Could not read vflip");
    return 1;
  }
  vfmt->vflip = (int) num;

  success = ReadFile(p->pipe_handle, &num, sizeof(int64_t), &bytes_read, NULL);
  if (!success || bytes_read < sizeof(int64_t)) {
    printf("Could not read pitch\n");
    return 1;
  }
  vfmt->pitch = (int) num;

  return 0;
}

int receive_video_frame (encoder_private_t *p, uint8_t *buffer, size_t buffer_size, int64_t *timestamp) {
  DWORD bytes_read = 0;
  DWORD total_bytes_read = 0;
  BOOL success = TRUE;

  success = ReadFile(p->pipe_handle, timestamp, sizeof(int64_t), &bytes_read, NULL);
  if (!success || bytes_read < sizeof(int64_t)) {
    capsule_log("Could not read timestamp");
    return 0;
  }

  bytes_read = 0;

  while (success && total_bytes_read < buffer_size) {
    success = ReadFile(p->pipe_handle, buffer + bytes_read, buffer_size - bytes_read, &bytes_read, NULL);
    total_bytes_read += bytes_read;
  }

  return total_bytes_read;
}

static void wait_for_child (HANDLE hProcess) {
  LONG platform = NktHookLibHelpers::GetProcessPlatform(hProcess);
  if (platform == NKTHOOKLIB_ProcessPlatformX86) {
    cdprintf("Child is 32-bit!");
  } else if (platform == NKTHOOKLIB_ProcessPlatformX64) {
    cdprintf("Child is 64-bit!");
  }

  cdprintf("Waiting on child...");
  WaitForSingleObject(hProcess, INFINITE);
  cdprintf("Done waiting on child");

  GetExitCodeProcess(hProcess, &exitCode);
  capsule_log("Exit code: %d (%x)", exitCode, exitCode);

  if (!connected) {
    exit(exitCode);
  }
}

int capsulerun_main (int argc, char **argv) {
  capsule_log("thanks for flying capsule on Windows");

  if (argc < 3) {
    capsule_log("usage: capsulerun LIBCAPSULE_DIR EXECUTABLE");
    exit(1);
  }

  char *libcapsule_dir = argv[1];
  char *executable_path = argv[2];

  // From Deviare-InProc's doc: 
  //   If "szDllNameW" string ends with 'x86.dll', 'x64.dll', '32.dll', '64.dll', the dll name will be adjusted
  //   in order to match the process platform. I.e.: "mydll_x86.dll" will become "mydll_x64.dll" on 64-bit processes.
  // hence the blanket 'capsule32.dll' here
  // N.B: even if the .exe we launch appears to be PE32, it might actually end up being a 64-bit process.
  // Don't ask me, I don't know either.
  const char *libcapsule_name = "capsule32.dll";
  char libcapsule_path[CAPSULE_MAX_PATH_LENGTH];
  const int libcapsule_path_length = snprintf(libcapsule_path, CAPSULE_MAX_PATH_LENGTH, "%s\\%s", libcapsule_dir, libcapsule_name);

  if (libcapsule_path_length > CAPSULE_MAX_PATH_LENGTH) {
    capsule_log("libcapsule path too long (%d > %d)", libcapsule_path_length, CAPSULE_MAX_PATH_LENGTH);
    exit(1);
  }

  STARTUPINFOW si;
  PROCESS_INFORMATION pi;

  DWORD err;

  ZeroMemory(&si, sizeof(si));  
  si.cb = sizeof(si);

  ZeroMemory(&pi, sizeof(pi));  

  wchar_t *executable_path_w;
  wchar_t *libcapsule_path_w;

  toWideChar(executable_path, &executable_path_w);
  toWideChar(libcapsule_path, &libcapsule_path_w);

  wchar_t *pipe_path = L"\\\\.\\pipe\\capsule_pipe";
  cdprintf("Creating named pipe %S...", pipe_path);

  HANDLE pipe_handle = CreateNamedPipe(
    pipe_path,
    PIPE_ACCESS_INBOUND, // open mode
    PIPE_TYPE_BYTE | PIPE_WAIT, // pipe mode
    1, // max instances
    16 * 1024 * 1024, // nOutBufferSize
    16 * 1024 * 1024, // nInBufferSize
    0, // nDefaultTimeout
    NULL // lpSecurityAttributes
  );

  if (!pipe_handle) {
    err = GetLastError();
    capsule_log("CreateNamedPipe failed, err = %d (%x)", err, err);
  }

  cdprintf("Created named pipe!");

  err = SetEnvironmentVariable(L"CAPSULE_PIPE_PATH", pipe_path);
  if (err == 0) {
    capsule_log("Could not set pipe path environment variable");
    exit(1);
  }

  err = SetEnvironmentVariable(L"CAPSULE_LIBRARY_PATH", libcapsule_path_w);
  if (err == 0) {
    capsule_log("Could not set library path environment variable");
    exit(1);
  }

  bool first_arg = true;
  LPWSTR in_command_line = GetCommandLineW();
  int num_args;
  LPWSTR* argv_w = CommandLineToArgvW(in_command_line, &num_args);

  wstring command_line_w;
  for (int i = 2; i < num_args; i++) {
    string arg = argv[i];
    if (first_arg) {
      first_arg = false;
    } else {
      command_line_w.append(L" ");
    }
    ArgvQuote(argv_w[i], command_line_w, false);
  }

  capsule_log("Launching %S", command_line_w.c_str());
  capsule_log("Injecting %S", libcapsule_path_w);
  auto libcapsule_init_function_name = "capsule_windows_init";

  err = NktHookLibHelpers::CreateProcessWithDllW(
    executable_path_w, // applicationName
    (LPWSTR) command_line_w.c_str(), // commandLine
    NULL, // processAttributes
    NULL, // threadAttributes
    FALSE, // inheritHandles
    0, // creationFlags
    NULL, // environment
    NULL, // currentDirectory
    &si, // startupInfo
    &pi, // processInfo
    libcapsule_path_w, // dllName
    NULL, // signalCompletedEvent
    libcapsule_init_function_name // initFunctionName
  );

  if (err == ERROR_SUCCESS) {
    capsule_log("Process #%lu successfully launched with dll injected!", pi.dwProcessId);

    thread child_thread(wait_for_child, pi.hProcess);
    child_thread.detach();

    cdprintf("Connecting named pipe...");
    BOOL success = ConnectNamedPipe(pipe_handle, NULL);
    if (!success) {
      capsule_log("Could not connect to named pipe");
      exit(1);
    }
    connected = true;

    struct encoder_private_s private_data;
    ZeroMemory(&private_data, sizeof(encoder_private_s));
    private_data.pipe_handle = pipe_handle;

    wasapi_start(&private_data);

    struct encoder_params_s encoder_params;
    ZeroMemory(&encoder_params, sizeof(encoder_private_s));
    encoder_params.private_data = &private_data;
    encoder_params.receive_video_format = (receive_video_format_t) receive_video_format;
    encoder_params.receive_video_frame = (receive_video_frame_t) receive_video_frame;

    encoder_params.has_audio = 1;
    encoder_params.receive_audio_format = (receive_audio_format_t) wasapi_receive_audio_format;
    encoder_params.receive_audio_frames = (receive_audio_frames_t) wasapi_receive_audio_frames;

    capsule_log("Starting encoder thread...");
    thread encoder_thread(encoder_run, &encoder_params);

    capsule_log("Waiting on encoder thread...");
    encoder_thread.join();
  } else {
    capsule_log("Error %lu: Cannot launch process and inject dll.", err);
    return 127;
  }

  return exitCode;
}
