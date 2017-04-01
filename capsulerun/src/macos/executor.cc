
/*
 *  capsule - the game recording and overlay toolkit
 *  Copyright (C) 2017, Amos Wenger
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details:
 * https://github.com/itchio/capsule/blob/master/LICENSE
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "executor.h"

#include <spawn.h> // posix_spawn
#include <stdio.h> // strerror
#include <sys/stat.h> // stat
#include <string.h> // strerror

#include <lab/paths.h>
#include <lab/env.h>

#include "../logging.h"
#include "bundle_utils.h"

namespace capsule {
namespace macos {

void Process::Wait(ProcessFate *fate) {
  int child_status;
  pid_t wait_result;

  do {
    wait_result = waitpid(pid_, &child_status, 0);
    if (wait_result == -1) {
      Log("Could not wait on child (error %d): %s", wait_result, strerror(wait_result));
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
  std::string libcapsule64_path = lab::paths::Join(std::string(args->libpath), "libcapsule64.dylib");

  std::string dyld_insert = lab::env::Get("DYLD_INSERT_LIBRARIES");
  if (dyld_insert != "") {
    dyld_insert += ":";
  }
  dyld_insert += libcapsule64_path;
  std::string dyld_insert_var = "DYLD_INSERT_LIBRARIES=" + dyld_insert;

  std::string pipe_var = "CAPSULE_PIPE_PATH=" + std::string(args->pipe);
  char *env_additions[] = {
    const_cast<char *>(dyld_insert_var.c_str()),
    const_cast<char *>(pipe_var.c_str()),
    nullptr
  };
  char **child_environ = lab::env::MergeBlocks(lab::env::GetBlock(), env_additions);

  struct stat exe_stat;
  int stat_ret = stat(args->exec, &exe_stat);
  if (stat_ret != 0) {
    Log("Could not access executable (error %d): %s", stat_ret, strerror(stat_ret));
    return nullptr;
  }

  if (exe_stat.st_mode & S_IFDIR) {
    auto exec = bundle::ExecPath(args->exec);
    if (exec == "") {
      Log("Cannot launch, exec is neither an executable or a valid app bundle: %s", args->exec);
      return nullptr;
    }
    Log("Launching an app bundle...");
    char *new_exec = strdup(exec.c_str());
    args->exec = new_exec;
    args->exec_argv[0] = new_exec;
  }

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
    Log("Spawning child failed with error %d: %s", child_err, strerror(child_err));
  }

  Log("PID %d given to %s", child_pid, args->exec);
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
