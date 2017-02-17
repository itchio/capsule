
#include <capsule.h>
 
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#ifndef CAPSULE_WINDOWS
void __attribute__((constructor)) capsule_load() {
  pid_t pid = getpid();
  capsule_log("Initializing (pid %d)...", pid);

#ifdef CAPSULE_LINUX
  capsule_log("LD_LIBRARY_PATH: %s", getenv("LD_LIBRARY_PATH"));
  capsule_log("LD_PRELOAD: %s", getenv("LD_PRELOAD"));
  capsule_log("CAPSULE_PIPE_PATH: %s", getenv("CAPSULE_PIPE_PATH"));
  capsule_x11_init();
#endif
}

void __attribute__((destructor)) capsule_unload() {
  capsule_log("Winding down...");
}
#endif
