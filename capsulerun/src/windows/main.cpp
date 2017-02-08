
#include <stdio.h>
#include <NktHookLib.h>

// std::thread
#include <thread>

#include "capsulerun.h"

using namespace std;

void toWideChar (const char *s, wchar_t **ws) {
  int wchars_num = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
  *ws = (wchar_t *) malloc(wchars_num * sizeof(wchar_t));
  MultiByteToWideChar(CP_UTF8, 0, s, -1, *ws, wchars_num);
}

int receive_resolution (encoder_private_t *p, int64_t *width, int64_t *height) {
  DWORD bytes_read;
  BOOL success = FALSE;

  success = ReadFile(p->pipe_handle, width, sizeof(int64_t), &bytes_read, NULL);
  if (!success || bytes_read < sizeof(int64_t)) {
    printf("Could not read width\n");
    return 1;
  }

  success = ReadFile(p->pipe_handle, height, sizeof(int64_t), &bytes_read, NULL);
  if (!success || bytes_read < sizeof(int64_t)) {
    printf("Could not read height\n");
    return 1;
  }

  return 0;
}

int receive_frame (encoder_private_t *p, uint8_t *buffer, size_t buffer_size, int64_t *delta) {
  DWORD bytes_read = 0;
  DWORD total_bytes_read = 0;
  BOOL success = TRUE;

  success = ReadFile(p->pipe_handle, delta, sizeof(int64_t), &bytes_read, NULL);
  if (!success || bytes_read < sizeof(int64_t)) {
    printf("Could not read delta\n");
    return 0;
  }

  bytes_read = 0;

  while (success && total_bytes_read < buffer_size) {
    success = ReadFile(p->pipe_handle, buffer + bytes_read, buffer_size - bytes_read, &bytes_read, NULL);
    total_bytes_read += bytes_read;
  }

  return total_bytes_read;
}

int capsulerun_main (int argc, char **argv) {
  printf("thanks for flying capsule on Windows\n");

  if (argc < 3) {
    printf("usage: capsulerun LIBCAPSULE_DIR EXECUTABLE\n");
    exit(1);
  }

  char *libcapsule_dir = argv[1];
  char *executable_path = argv[2];

  char libcapsule_path[CAPSULE_MAX_PATH_LENGTH];
  const int libcapsule_path_length = snprintf(libcapsule_path, CAPSULE_MAX_PATH_LENGTH, "%s\\capsule.dll", libcapsule_dir);

  if (libcapsule_path_length > CAPSULE_MAX_PATH_LENGTH) {
    printf("libcapsule path too long (%d > %d)\n", libcapsule_path_length, CAPSULE_MAX_PATH_LENGTH);
    exit(1);
  }

  STARTUPINFOW si;
  PROCESS_INFORMATION pi;

  DWORD err;

  ZeroMemory(&si, sizeof(si));  
  si.cb = sizeof(si);

  ZeroMemory(&pi, sizeof(pi));  

  char *exeName = argv[1];

  wchar_t *executable_path_w;
  wchar_t *libcapsule_path_w;

  toWideChar(executable_path, &executable_path_w);
  toWideChar(libcapsule_path, &libcapsule_path_w);

  wchar_t *pipe_path = L"\\\\.\\pipe\\capsule_pipe";
  printf("Creating named pipe %S...\n", pipe_path);

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
    printf("CreateNamedPipe failed, err = %d (%x)\n", err, err);
  }

  printf("Created named pipe!\n");

  err = SetEnvironmentVariable(L"CAPSULE_PIPE_PATH", pipe_path);
  if (err == 0) {
    printf("Could not set pipe path environment variable\n");
    exit(1);
  }

  printf("Launching %S\n", executable_path_w);
  printf("Injecting %S\n", libcapsule_path_w);

  err = NktHookLibHelpers::CreateProcessWithDllW(
    executable_path_w, // applicationName
    NULL, // commandLine
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
    NULL // initFunctionName
  );

  if (err == ERROR_SUCCESS) {
    printf("Process #%lu successfully launched with dll injected!\n", pi.dwProcessId);

    printf("Connecting named pipe...\n");
    BOOL success = ConnectNamedPipe(pipe_handle, NULL);
    if (!success) {
      printf("Could not connect to named pipe\n");
      exit(1);
    }

    struct encoder_private_s private_data;
    ZeroMemory(&private_data, sizeof(encoder_private_s));
    private_data.pipe_handle = pipe_handle;

    wasapi_start(&private_data);

    struct encoder_params_s encoder_params;
    ZeroMemory(&encoder_params, sizeof(encoder_private_s));
    encoder_params.private_data = &private_data;
    encoder_params.receive_video_resolution = (receive_video_resolution_t) receive_resolution;
    encoder_params.receive_video_frame = (receive_video_frame_t) receive_frame;

    encoder_params.has_audio = 1;
    encoder_params.receive_audio_format = (receive_audio_format_t) wasapi_receive_audio_format;
    encoder_params.receive_audio_frames = (receive_audio_frames_t) wasapi_receive_audio_frames;

    printf("Starting encoder thread...\n");
    thread encoder_thread(encoder_run, &encoder_params);

    printf("Waiting on child...\n");
    WaitForSingleObject(pi.hProcess, INFINITE);
    printf("Done waiting on child\n");

    printf("Waiting on encoder thread...\n");
    encoder_thread.join();

    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    printf("Exit code = %d (%x)\n", exitCode, exitCode);
  } else {
    printf("Error %lu: Cannot launch process and inject dll.\n", err);
  }

  return 0;
}
