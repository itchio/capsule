
#include <stdio.h>
#include <stdlib.h>

// posix_spawn
#include <spawn.h>

// strerror
#include <string.h>

#include "capsulerun.h"

int capsulerun_main (int argc, char **argv) {
  printf("thanks for flying capsule on macOS\n");

  if (argc < 3) {
    printf("usage: capsulerun LIBCAPSULE_DIR EXECUTABLE");
    exit(1);
  }

  char *libcapsule_dir = argv[1];
  char *executable_path = argv[2];

  char libcapsule_path[CAPSULE_MAX_PATH_LENGTH];
  const int libcapsule_path_length = snprintf(libcapsule_path, CAPSULE_MAX_PATH_LENGTH, "%s/libcapsule.dylib", libcapsule_dir);

  if (libcapsule_path_length > CAPSULE_MAX_PATH_LENGTH) {
    printf("libcapsule path too long (%d > %d)", libcapsule_path_length, CAPSULE_MAX_PATH_LENGTH);
    exit(1);
  }

  char dyld_insert[CAPSULE_MAX_PATH_LENGTH];
  const int dyld_insert_length = snprintf(dyld_insert, CAPSULE_MAX_PATH_LENGTH, "DYLD_INSERT_LIBRARIES=%s", libcapsule_path);

  if (dyld_insert_length > CAPSULE_MAX_PATH_LENGTH) {
    printf("dyld_insert path too long (%d > %d)", dyld_insert_length, CAPSULE_MAX_PATH_LENGTH);
    exit(1);
  }

  pid_t child_pid;
  char **child_argv = &argv[2];

  char *env[2];
  // TODO: respect outside DYLD_INSERT_LIBRARIES ?
  env[0] = dyld_insert;
  env[1] = NULL;

  int err = posix_spawn(
    &child_pid,
    executable_path,
    NULL, // file_actions
    NULL, // spawn_attrs
    child_argv,
    env
  );
  if (err != 0) {
    printf("spawn error %d: %s", err, strerror(err));
  }

  printf("pid %d given to child %s", child_pid, executable_path);

  int child_status;
  pid_t wait_result;

  do {
    wait_result = waitpid(child_pid, &child_status, 0);
    if (wait_result == -1) {
      printf("could not wait on child (error %d): %s", wait_result, strerror(wait_result));
      exit(1);
    }

    if (WIFEXITED(child_status)) {
      printf("exited, status=%d\n", WEXITSTATUS(child_status));
    } else if (WIFSIGNALED(child_status)) {
      printf("killed by signal %d\n", WTERMSIG(child_status));
    } else if (WIFSTOPPED(child_status)) {
      printf("stopped by signal %d\n", WSTOPSIG(child_status));
    } else if (WIFCONTINUED(child_status)) {
      printf("continued\n");
    }
  } while (!WIFEXITED(child_status) && !WIFSIGNALED(child_status));

  return 0;
}
