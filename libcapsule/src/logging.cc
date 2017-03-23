
#include <stdarg.h>

#include <lab/platform.h>
#include <lab/env.h>
#include <lab/io.h>

namespace capsule {

namespace {

FILE *logfile;

std::string CapsuleLogPath () {
#if defined(LAB_WINDOWS)
  return lab::env::Expand("%APPDATA%\\capsule.log.txt");
#else // LAB_WINDOWS
  return "/tmp/capsule.log.txt";
#endif // !LAB_WINDOWS
}

} // namespace

void Log(const char *format, ...) {
  va_list args;

  if (!logfile) {
    logfile = lab::io::Fopen(CapsuleLogPath(), "w");
  }

  va_start(args, format);
  vfprintf(logfile, format, args);
  va_end(args);

  fprintf(logfile, "\n");
  fflush(logfile);

  fprintf(stderr, "[capsule] ");

  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);

  fprintf(stderr, "\n");
}

} // namespace capsule
