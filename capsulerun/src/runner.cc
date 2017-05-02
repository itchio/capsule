
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

#include "runner.h"

#include <string>

#include "logging.h"
#include "hotkey.h"
#include "router.h"

namespace capsule {

void Runner::Run () {
  if (args_->exec) {
    process_ = executor_->LaunchProcess(args_);
    if (!process_) {
      Log("Couldn't start child, bailing out");
      Exit(127);
    }

    Log("Watching on child in the background");
    wait_thread_ = new std::thread(&Runner::WaitForChild, this);
  }

  loop_ = new MainLoop(args_);
  loop_->audio_receiver_factory_ = executor_->GetAudioReceiverFactory();
  hotkey::Init(loop_);

  router_ = new Router(args_->pipe, loop_);
  router_->Start();

  Log("Running loop...");
  loop_->Run();
  Log("Loop finished running!");

  runner_done_ = true;
  if (!args_->exec || exec_done_) {
    Log("Quitting from Runner::Run");
    Exit(exit_code_);
  } else {
    Log("Child still running, waiting...");
    while (true) {
      Sleep(1000);
    }
  }
}

void Runner::WaitForChild () {
  ProcessFate fate;
  memset(&fate, 0, sizeof(fate));

  process_->Wait(&fate);
  exit_code_ = fate.code;

  if (fate.status == kProcessStatusExited) {
    if (fate.code == 0) {
      Log("Child exited gracefully");
    } else {
      Log("Child exited with code %d", fate.code);
    }
  } else if (fate.status == kProcessStatusSignaled) {
    Log("Child killed by signal %d", fate.code);
  } else if (fate.status == kProcessStatusUnknown) {
    Log("Child left in unknown state");
  }
  exec_done_ = true;

  // FIXME: evil (the runner_done_ part)
  if ((router_ == nullptr) || (!router_->HadConnections()) || runner_done_) {
    // if we haven't connected by this point, we never will:
    // the child doesn't exist anymore.
    // this also works for "disappearing launchers": even the launchers
    // should connect once first, and then their spawn will use a second connection.
    Exit(fate.code);
  }
  Log("Had connections, waiting for something else to exit");
}

void Runner::Exit(int code) {
  fflush(stdout);
  fflush(stderr);
  exit(code);
}

Runner::~Runner() {
  // muffin
}

} // namespace capsule
