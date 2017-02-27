
#include <capsule.h>
 
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

struct capture_data capdata = {0};

#if !defined(CAPSULE_WINDOWS)
void __attribute__((constructor)) capsule_load() {
  pid_t pid = getpid();
  capsule_log("Initializing (pid %d)...", pid);

#if defined(CAPSULE_LINUX)
  capsule_log("LD_LIBRARY_PATH: %s", getenv("LD_LIBRARY_PATH"));
  capsule_log("LD_PRELOAD: %s", getenv("LD_PRELOAD"));
#elif defined(CAPSULE_MACOS)
  capsule_log("DYLD_INSERT_LIBRARIES: %s", getenv("DYLD_INSERT_LIBRARIES"));
  capsule_log("LD_PRELOAD: %s", getenv("LD_PRELOAD"));
#endif

  capsule_log("CAPSULE_PIPE_R_PATH: %s", getenv("CAPSULE_PIPE_R_PATH"));
  capsule_log("CAPSULE_PIPE_W_PATH: %s", getenv("CAPSULE_PIPE_W_PATH"));
  capsule_io_init();
}

void __attribute__((destructor)) capsule_unload() {
  capsule_log("Winding down...");
}
#endif
