
#include <capsulerun.h>
#include <capsulerun_hotkey.h>

#include "../env.h"
#include "../main_loop.h"

#include <stdio.h>
#include <stdlib.h>

// posix_spawn
#include <spawn.h>

// unlink
#include <unistd.h>

// strerror
#include <string.h>

#include <thread>

extern char **environ;

namespace capsule {

void MainThread (capsule_args_t *args) {
  auto libcapsule_path = std::string(args->libpath) + "/libcapsule.dylib";

  // TODO: respect outside DYLD_INSERT_LIBRARIES ?
  auto dyld_insert_var = "DYLD_INSERT_LIBRARIES=" + libcapsule_path;

  std::string fifo_r_path = "/tmp/capsule.runr";
  std::string fifo_w_path = "/tmp/capsule.runw";

  // swapped on purpose
  auto fifo_r_var = "CAPSULE_PIPE_R_PATH=" + fifo_w_path;
  auto fifo_w_var = "CAPSULE_PIPE_W_PATH=" + fifo_r_path;
  char *env_additions[] = {
    (char *) fifo_r_var.c_str(),
    (char *) fifo_w_var.c_str(),
    (char *) dyld_insert_var.c_str(),
    NULL
  };

  char **child_environ = MergeEnvs(environ, env_additions);

  pid_t child_pid;

  Connection *conn = new Connection(fifo_r_path, fifo_w_path);

  int child_err = posix_spawn(
    &child_pid,
    args->exec,
    NULL, // file_actions
    NULL, // spawn_attrs
    args->exec_argv,
    child_environ
  );
  if (child_err != 0) {
    CapsuleLog("spawn error %d: %s", child_err, strerror(child_err));
  }

  CapsuleLog("pid %d given to child %s", child_pid, args->exec);

  conn->connect();

  MainLoop ml {args, conn};
  hotkey::Init(&ml);
  ml.Run();

  int child_status;
  pid_t wait_result;

  do {
    wait_result = waitpid(child_pid, &child_status, 0);
    if (wait_result == -1) {
      CapsuleLog("could not wait on child (error %d): %s", wait_result, strerror(wait_result));
      exit(1);
    }

    if (WIFEXITED(child_status)) {
      CapsuleLog("exited, status=%d", WEXITSTATUS(child_status));
    } else if (WIFSIGNALED(child_status)) {
      CapsuleLog("killed by signal %d", WTERMSIG(child_status));
    } else if (WIFSTOPPED(child_status)) {
      CapsuleLog("stopped by signal %d", WSTOPSIG(child_status));
    } else if (WIFCONTINUED(child_status)) {
      CapsuleLog("continued");
    }
  } while (!WIFEXITED(child_status) && !WIFSIGNALED(child_status));

  exit(0);
}

int Main (capsule_args_t *args) {
  std::thread main_thread(MainThread, args);
  RunApp();
  return 0;
}

} // namespace capsule
