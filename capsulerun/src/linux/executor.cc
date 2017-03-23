
#include "executor.h"

#include <spawn.h> // posix_spawn
#include <sys/types.h>
#include <sys/wait.h> // waitpid

#include <lab/paths.h>
#include <lab/env.h>

#include "pulse_receiver.h"

namespace capsule {
namespace linux {
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
  // muffin
}

ProcessInterface *Executor::LaunchProcess(MainArgs *args) {
  std::string libcapsule32_path = lab::paths::Join(std::string(args->libpath), "libcapsule32.so");
  std::string libcapsule64_path = lab::paths::Join(std::string(args->libpath), "libcapsule64.so");
  std::string ldpreload_var = libcapsule32_path + ":" + libcapsule64_path;

  pid_t child_pid;

  if (!lab::env::Set("LD_PRELOAD", ldpreload_var)) {
    CapsuleLog("Couldn't set up library injection");
    return nullptr;
  }

  std::string pipe_var = "CAPSULE_PIPE_PATH=" + std::string(args->pipe);
  char *env_additions[] = {
    const_cast<char *>(pipe_var.c_str()),
    nullptr
  };
  char **child_environ = lab::env::MergeBlocks(lab::env::GetBlock(), env_additions);

  int child_err = posix_spawn(
    &child_pid,
    args->exec,
    nullptr, // file_actions
    nullptr, // spawn-attrs,
    args->exec_argv,
    child_environ // environment
  );

  if (child_err != 0) {
    CapsuleLog("Spawning child failed with error %d: %s", child_err, strerror(child_err));
  }

  CapsuleLog("PID %d given to %s", child_pid, args->exec)
  return new Process(child_pid);
}

static audio::AudioReceiver *PulseReceiverFactory() {
  return new audio::PulseReceiver();
}

AudioReceiverFactory Executor::GetAudioReceiverFactory() {
  return PulseReceiverFactory;
}

Executor::~Executor() {
  // muffin
}

} // namespace linux
} // namespace capsule

