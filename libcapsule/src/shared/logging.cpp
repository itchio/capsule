
#include <capsule.h>

FILE *logfile;

FILE *capsule_open_log () {
#ifdef CAPSULE_WINDOWS
  return _wfopen(capsule_log_path(), L"w");
#else // CAPSULE_WINDOWS
  return fopen(capsule_log_path(), "w");
#endif // CAPSULE_WINDOWS
}

#ifdef CAPSULE_WINDOWS

wchar_t *_capsule_log_path;

wchar_t *capsule_log_path () {
  if (!_capsule_log_path) {
    _capsule_log_path = (wchar_t*) malloc(sizeof(wchar_t) * CAPSULE_LOG_PATH_SIZE);
    ExpandEnvironmentStrings(L"%APPDATA%\\capsule.log.txt", _capsule_log_path, CAPSULE_LOG_PATH_SIZE);
  }
  return _capsule_log_path;
}

#else // defined CAPSULE_WINDOWS

char *capsule_log_path () {
  return (char*) "/tmp/capsule.log.txt";
}

#endif // not CAPSULE_WINDOWS
