
#include <capsule.h>
 
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

struct capture_data capdata = {0};

#if !defined(LAB_WINDOWS)
void __attribute__((constructor)) CapsuleLoad() {
  pid_t pid = getpid();
  CapsuleLog("Initializing (pid %d)...", pid);

#if defined(LAB_LINUX)
  CapsuleLog("LD_LIBRARY_PATH: %s", getenv("LD_LIBRARY_PATH"));
  CapsuleLog("LD_PRELOAD: %s", getenv("LD_PRELOAD"));
#elif defined(LAB_MACOS)
  CapsuleLog("DYLD_INSERT_LIBRARIES: %s", getenv("DYLD_INSERT_LIBRARIES"));
  CapsuleLog("LD_PRELOAD: %s", getenv("LD_PRELOAD"));
#endif

  capsule::io::Init();
}

void __attribute__((destructor)) CapsuleUnload() {
  CapsuleLog("Winding down...");
}
#endif
