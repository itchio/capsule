
#include <stdio.h>
#include <NktHookLib.h>

#include "capsulerun.h"

void toWideChar (const char *s, wchar_t **ws) {
  int wchars_num = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
  *ws = (wchar_t *) malloc(wchars_num * sizeof(wchar_t));
  MultiByteToWideChar(CP_UTF8, 0, s, -1, *ws, wchars_num);
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
    printf("Waiting...\n");
    WaitForSingleObject(pi.hProcess, INFINITE);
    printf("Done waiting.\n");

    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    printf("Exit code = %d (%x)\n", exitCode, exitCode);
  } else {
    printf("Error %lu: Cannot launch process and inject dll.\n", err);
  }

  return 0;
}
