
#include <capsule.h>

#include <lab/platform.h>
#include <lab/env.h>
#include <lab/io.h>

FILE *logfile;

std::string CapsuleLogPath () {
#if defined(LAB_WINDOWS)
  return lab::env::Expand("%APPDATA%\\capsule.log.txt");
#else // LAB_WINDOWS
  return "/tmp/capsule.log.txt";
#endif // !LAB_WINDOWS
}

FILE *CapsuleOpenLog () {
  return lab::io::Fopen(CapsuleLogPath(), "w");
}
