
#include <stdio.h>
#include <NktHookLib.h>

// std::thread
#include <thread>

// ArgvQuote
#include "quote.h"

#include "capsulerun.h"

#include "../shared/MainLoop.h"
#include "./WasapiReceiver.h"

#include "strings.h"

using namespace std;

static bool connected = false;
static DWORD exitCode = 0;

int capsule_hotkey_init(MainLoop *ml);

AudioReceiver *audio_receiver_factory() {
  return new WasapiReceiver();
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
  CapsuleLog("Exit code: %d (%x)", exitCode, exitCode);

  if (!connected) {
    exit(exitCode);
  }
}

int capsulerun_main (capsule_args_t *args) {
  // From Deviare-InProc's doc: 
  //   If "szDllNameW" string ends with 'x86.dll', 'x64.dll', '32.dll', '64.dll', the dll name will be adjusted
  //   in order to match the process platform. I.e.: "mydll_x86.dll" will become "mydll_x64.dll" on 64-bit processes.
  // hence the blanket 'capsule32.dll' here
  // N.B: even if the .exe we launch appears to be PE32, it might actually end up being a 64-bit process.
  // Don't ask me, I don't know either.
  const char *libcapsule_name = "capsule32.dll";
  char libcapsule_path[CAPSULE_MAX_PATH_LENGTH];
  const int libcapsule_path_length = snprintf(libcapsule_path, CAPSULE_MAX_PATH_LENGTH, "%s\\%s", args->libpath, libcapsule_name);

  if (libcapsule_path_length > CAPSULE_MAX_PATH_LENGTH) {
    CapsuleLog("libcapsule path too long (%d > %d)", libcapsule_path_length, CAPSULE_MAX_PATH_LENGTH);
    exit(1);
  }

  STARTUPINFOW si;
  PROCESS_INFORMATION pi;

  DWORD err;

  ZeroMemory(&si, sizeof(si));  
  si.cb = sizeof(si);

  ZeroMemory(&pi, sizeof(pi));  

  wchar_t *executable_path_w;
  ToWideChar(args->exec, &executable_path_w);

  wchar_t *libcapsule_path_w;
  ToWideChar(libcapsule_path, &libcapsule_path_w);

  string pipe_r_path = "\\\\.\\pipe\\capsule.runr";
  string pipe_w_path = "\\\\.\\pipe\\capsule.runw";

  Connection *conn = new Connection(pipe_r_path, pipe_w_path);

  // swapped on purpose
  err = SetEnvironmentVariableA("CAPSULE_PIPE_R_PATH", pipe_w_path.c_str());
  if (err == 0) {
    CapsuleLog("Could not set pipe_r path environment variable");
    exit(1);
  }

  // swapped on purpose
  err = SetEnvironmentVariableA("CAPSULE_PIPE_W_PATH", pipe_r_path.c_str());
  if (err == 0) {
    CapsuleLog("Could not set pipe_w path environment variable");
    exit(1);
  }

  err = SetEnvironmentVariableA("CAPSULE_LIBRARY_PATH", libcapsule_path);
  if (err == 0) {
    CapsuleLog("Could not set library path environment variable");
    exit(1);
  }

  bool first_arg = true;

  wstring command_line_w;
  for (int i = 0; i < args->exec_argc; i++) {
    wchar_t *arg;
    // this "leaks" mem, but it's one time, so don't care
    ToWideChar(args->exec_argv[i], &arg);

    if (first_arg) {
      first_arg = false;
    } else {
      command_line_w.append(L" ");
    }
    ArgvQuote(arg, command_line_w, false);
  }

  CapsuleLog("Launching %S", command_line_w.c_str());
  CapsuleLog("Injecting %S", libcapsule_path_w);
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
    CapsuleLog("Process #%lu successfully launched with dll injected!", pi.dwProcessId);

    thread child_thread(wait_for_child, pi.hProcess);
    child_thread.detach();

    conn->connect();
    connected = true;

    MainLoop ml {args, conn};
    ml.audio_receiver_factory = audio_receiver_factory;

    capsule_hotkey_init(&ml);

    ml.Run();
  } else {
    CapsuleLog("Error %lu: Cannot launch process and inject dll.", err);
    return 127;
  }

  return exitCode;
}
