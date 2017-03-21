
#include <capsule.h>

FILE *logfile;

FILE *CapsuleOpenLog () {
#ifdef CAPSULE_WINDOWS
  return _wfopen(CapsuleLogPath(), L"w");
#else // CAPSULE_WINDOWS
  return fopen(CapsuleLogPath(), "w");
#endif // CAPSULE_WINDOWS
}

#ifdef CAPSULE_WINDOWS

wchar_t *capsule_log_path;

wchar_t *CapsuleLogPath () {
  if (!capsule_log_path) {
    capsule_log_path = (wchar_t*) malloc(sizeof(wchar_t) * CapsuleLog_PATH_SIZE);
    ExpandEnvironmentStrings(L"%APPDATA%\\capsule.log.txt", capsule_log_path, CapsuleLog_PATH_SIZE);
  }
  return capsule_log_path;
}

#else // defined CAPSULE_WINDOWS

char *CapsuleLogPath () {
  return (char*) "/tmp/capsule.log.txt";
}

#endif // not CAPSULE_WINDOWS
