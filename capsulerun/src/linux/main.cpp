
#include "capsulerun.h"

#include "../shared/env.h" // merge_envs

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

static bool connected = false;
static int exit_code = 0;

int capsule_hotkey_init(MainLoop *ml);

static AudioReceiver *audio_receiver_factory () {
  return new PulseReceiver();
}

static void wait_for_child (int child_pid) {
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
      exit_code = WEXITSTATUS(child_status);
    } else if (WIFSIGNALED(child_status)) {
      capsule_log("killed by signal %d", WTERMSIG(child_status));
    } else if (WIFSTOPPED(child_status)) {
      capsule_log("stopped by signal %d", WSTOPSIG(child_status));
    } else if (WIFCONTINUED(child_status)) {
      capsule_log("continued");
    }
  } while (!WIFEXITED(child_status) && !WIFSIGNALED(child_status));

  if (!connected) {
    exit(exit_code);
  }
}

int capsulerun_main (capsule_args_t *args) {
  std::string libcapsule32_path = std::string(args->libpath) + "/libcapsule32.so";
  std::string libcapsule64_path = std::string(args->libpath) + "/libcapsule64.so";
  std::string ldpreload = libcapsule32_path + ":" + libcapsule64_path;

  pid_t child_pid;

  if (setenv("LD_PRELOAD", ldpreload.c_str(), 1 /* replace */) != 0) {
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

  if (strcmp("headless", args->exec) == 0) {
    capsule_log("Running in headless mode...");
    conn->printDetails();
  } else {
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

    thread child_thread(wait_for_child, child_pid);
    child_thread.detach();
  }

  conn->connect();
  connected = true;

  MainLoop ml {args, conn};
  ml.audio_receiver_factory = audio_receiver_factory;

  capsule_hotkey_init(&ml);
  ml.run();

  return exit_code;
}
