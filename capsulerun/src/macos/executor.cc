
#include "executor.h"

#include <spawn.h> // posix_spawn
#include <string.h> // strerror

#include <lab/paths.h>
#include <lab/env.h>

namespace capsule {
namespace macos {

void Process::Wait(ProcessFate *fate) {
  int child_status;
  pid_t wait_result;

  do {
    wait_result = waitpid(pid_, &child_status, 0);
    if (wait_result == -1) {
      CapsuleLog("Could not wait on child (error %d): %s", wait_result, strerror(wait_result));
      fate->status = kProcessStatusUnknown;
      fate->code = 127;
      return;
    }

    if (WIFEXITED(child_status)) {
      fate->status = kProcessStatusExited;
      fate->code = WEXITSTATUS(child_status);
    } else if (WIFSIGNALED(child_status)) {
      fate->status = kProcessStatusSignaled;
      fate->code = WTERMSIG(child_status);
    }
    // ignoring WIFSTOPPED and WIFCONTINUED
  } while (!WIFEXITED(child_status) && !WIFSIGNALED(child_status));
}

Process::~Process() {
  // muffin
}

Executor::Executor() {
  // stub
}

ProcessInterface *Executor::LaunchProcess(MainArgs *args) {
  std::string libcapsule64_path = lab::paths::Join(std::string(args->libpath), "libcapsule64.dylib");

  std::string dyld_insert = lab::env::Get("DYLD_INSERT_LIBRARIES");
  if (dyld_insert != "") {
    dyld_insert += ":";
  }
  dyld_insert += libcapsule64_path;
  std::string dyld_insert_var = "DYLD_INSERT_LIBRARIES=" + dyld_insert;

  std::string pipe_var = "CAPSULE_PIPE_PATH=" + std::string(args->pipe);
  CapsuleLog("pipe_var: %s", pipe_var.c_str());
  CapsuleLog("dyld_insert_var: %s", dyld_insert_var.c_str());

  char *env_additions[] = {
    const_cast<char *>(dyld_insert_var.c_str()),
    const_cast<char *>(pipe_var.c_str()),
    nullptr
  };
  char **child_environ = lab::env::MergeBlocks(lab::env::GetBlock(), env_additions);

  pid_t child_pid;

  int child_err = posix_spawn(
    &child_pid,
    args->exec,
    NULL, // file_actions
    NULL, // spawn_attrs
    args->exec_argv,
    child_environ
  );
  if (child_err != 0) {
    CapsuleLog("Spawning child failed with error %d: %s", child_err, strerror(child_err));
  }

  CapsuleLog("PID %d given to %s", child_pid, args->exec);
  return new Process(child_pid);
}

AudioReceiverFactory Executor::GetAudioReceiverFactory() {
  return nullptr;
}

Executor::~Executor() {
  // stub
}

} // namespace macos
} // namespace capsule
