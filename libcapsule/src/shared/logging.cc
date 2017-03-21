
#include <capsule.h>

FILE *logfile;

FILE *CapsuleOpenLog () {
#ifdef CAPSULE_WINDOWS
  return _wfopen(CapsuleLog_path(), L"w");
#else // CAPSULE_WINDOWS
  return fopen(CapsuleLog_path(), "w");
#endif // CAPSULE_WINDOWS
}

#ifdef CAPSULE_WINDOWS

wchar_t *_capsule_log_path;

wchar_t *CapsuleLogPath () {
  if (!_capsule_log_path) {
    _capsule_log_path = (wchar_t*) malloc(sizeof(wchar_t) * CapsuleLog_PATH_SIZE);
    ExpandEnvironmentStrings(L"%APPDATA%\\capsule.log.txt", _CapsuleLog_path, CapsuleLog_PATH_SIZE);
  }
  return _capsule_log_path;
}

#else // defined CAPSULE_WINDOWS

char *CapsuleLogPath () {
  return (char*) "/tmp/capsule.log.txt";
}

#endif // not CAPSULE_WINDOWS
