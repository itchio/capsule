
#include "capsulerun.h"

#include "../shared/env.h" // merge_envs
#include "../shared/io.h" // create_fifo, receive stuff

#include "../shared/MainLoop.h"
#include "./PulseReceiver.h"

#include <stdio.h>
#include <stdlib.h>

// environ
#include <unistd.h>

// posix_spawn
#include <spawn.h>

// waitpid
#include <sys/types.h>
#include <sys/wait.h>

// strerror
#include <string.h>

// errno
#include <errno.h>

// thread
#include <thread>

using namespace std;

int capsule_hotkey_init(MainLoop *ml);

static AudioReceiver *audio_receiver_factory () {
  return new PulseReceiver();
}

int capsulerun_main (capsule_args_t *args) {
  char libcapsule_path[CAPSULE_MAX_PATH_LENGTH];
  const int libcapsule_path_length = snprintf(libcapsule_path, CAPSULE_MAX_PATH_LENGTH, "%s/libcapsule.so", args->libpath);

  if (libcapsule_path_length > CAPSULE_MAX_PATH_LENGTH) {
    capsule_log("libcapsule path too long (%d > %d)", libcapsule_path_length, CAPSULE_MAX_PATH_LENGTH);
    exit(1);
  }

  pid_t child_pid;

  if (setenv("LD_PRELOAD", libcapsule_path, 1 /* replace */) != 0) {
    capsule_log("couldn't set LD_PRELOAD'");
    exit(1);
  }

  auto fifo_r_path = string("/tmp/capsule.runr");
  auto fifo_w_path = string("/tmp/capsule.runw");

  // swapped on purpose
  auto fifo_r_var = "CAPSULE_PIPE_R_PATH=" + fifo_w_path;
  auto fifo_w_var = "CAPSULE_PIPE_W_PATH=" + fifo_r_path;
  char *env_additions[] = {
    (char *) fifo_r_var.c_str(),
    (char *) fifo_w_var.c_str(),
    NULL
  };
  char **child_environ = merge_envs(environ, env_additions);

  auto conn = new Connection(fifo_r_path, fifo_w_path);

  // spawn game
  int child_err = posix_spawn(
    &child_pid,
    args->exec,
    NULL, // file_actions
    NULL, // spawn_attrs
    args->exec_argv,
    child_environ // environment
  );
  if (child_err != 0) {
    capsule_log("child spawn error %d: %s", child_err, strerror(child_err));
  }

  capsule_log("pid %d given to child %s", child_pid, args->exec);

  conn->connect();
  MainLoop ml {args, conn};
  ml.audio_receiver_factory = audio_receiver_factory;

  capsule_hotkey_init(&ml);
  ml.run();

  int child_status;
  pid_t wait_result;

  do {
    wait_result = waitpid(child_pid, &child_status, 0);
    if (wait_result == -1) {
      capsule_log("could not wait on child (error %d): %s", wait_result, strerror(wait_result));
      exit(1);
    }

    if (WIFEXITED(child_status)) {
      capsule_log("exited, status=%d", WEXITSTATUS(child_status));
    } else if (WIFSIGNALED(child_status)) {
      capsule_log("killed by signal %d", WTERMSIG(child_status));
    } else if (WIFSTOPPED(child_status)) {
      capsule_log("stopped by signal %d", WSTOPSIG(child_status));
    } else if (WIFCONTINUED(child_status)) {
      capsule_log("continued");
    }
  } while (!WIFEXITED(child_status) && !WIFSIGNALED(child_status));

  return 0;
}
