
#include <sys/types.h>
#include <unistd.h>

#include <lab/env.h>

#include "../logging.h"
#include "../io.h"

void __attribute__((constructor)) CapsuleConstructor() {
  pid_t pid = getpid();
  capsule::Log("Injected into pid %d...", pid);

  capsule::Log("LD_LIBRARY_PATH: %s", lab::env::Get("LD_LIBRARY_PATH").c_str());
  capsule::Log("LD_PRELOAD: %s", lab::env::Get("LD_PRELOAD").c_str());

  capsule::io::Init();
}

void __attribute__((destructor)) CapsuleDestructor() {
  capsule::Log("Winding down...");
}
