
#include "executor.h"

#include <NktHookLib.h>

#include <lab/strings.h>
#include <lab/paths.h>
#include <lab/env.h>

#include <string>

#include "quote.h"
#include "wasapi_receiver.h"
#include "../macros.h"

namespace capsule {
namespace windows {

void Process::Wait(ProcessFate *fate) {
  WaitForSingleObject(process_handle_, INFINITE);

  DWORD exit_code;
  GetExitCodeProcess(process_handle_, &exit_code);

  fate->status = kProcessStatusExited;
  fate->code = static_cast<int>(exit_code);
}

Process::~Process() {
  // muffin
}

Executor::Executor() {
  // muffin
}

ProcessInterface * Executor::LaunchProcess(MainArgs *args) {
  // If the injected dll string passed to Deviare-InProc ends with 'x86.dll', 'x64.dll', '32.dll',
  // '64.dll', the dll name will be adjusted in order to match the process platform.
  // i.e.: "mydll_x86.dll" will become "mydll_x64.dll" on 64-bit processes.
  std::string libcapsule_path = lab::paths::Join(std::string(args->libpath), "capsule32.dll");

  STARTUPINFOW si;
  PROCESS_INFORMATION pi;

  ZeroMemory(&si, sizeof(si));  
  si.cb = sizeof(si);

  ZeroMemory(&pi, sizeof(pi));  

  wchar_t *executable_path_w;
  lab::strings::ToWideChar(args->exec, &executable_path_w);

  wchar_t *libcapsule_path_w;
  lab::strings::ToWideChar(libcapsule_path.c_str(), &libcapsule_path_w);

  bool env_success = true;
  env_success &= lab::env::Set("CAPSULE_PIPE_PATH", std::string(args->pipe));
  env_success &= lab::env::Set("CAPSULE_LIBRARY_PATH", libcapsule_path);
  if (!env_success) {
    CapsuleLog("Could not set environment variables for the child");
    return nullptr;
  }

  bool first_arg = true;

  std::wstring command_line_w;
  for (int i = 0; i < args->exec_argc; i++) {
    wchar_t *arg;
    // this "leaks" mem, but it's one time, so don't care
    lab::strings::ToWideChar(args->exec_argv[i], &arg);

    if (first_arg) {
      first_arg = false;
    } else {
      command_line_w.append(L" ");
    }

    std::wstring arg_w = arg;
    ArgvQuote(arg_w, command_line_w, false);
  }

  CapsuleLog("Launching '%S' with args '%S'", executable_path_w, command_line_w.c_str());
  CapsuleLog("Injecting '%S'", libcapsule_path_w);
  const char* libcapsule_init_function_name = "CapsuleWindowsInit";

  DWORD err = NktHookLibHelpers::CreateProcessWithDllW(
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
  } else {
    CapsuleLog("Error %lu: Cannot launch process and inject dll.", err);
    return nullptr;
  }

  return new Process(pi.hProcess);
}

static audio::AudioReceiver *WasapiReceiverFactory() {
  return new audio::WasapiReceiver();
}

AudioReceiverFactory Executor::GetAudioReceiverFactory() {
  return WasapiReceiverFactory;
}

Executor::~Executor() {
  // stub
}

}
}