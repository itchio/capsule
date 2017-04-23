
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
  conn_ = new Connection(std::string(args_->pipe));

  if (args_->exec) {
    process_ = executor_->LaunchProcess(args_);
    if (!process_) {
      Log("Couldn't start child, bailing out");
      exit(127);
    }

    Log("Watching on child in the background");
    wait_thread_ = new std::thread(&Runner::WaitForChild, this);
  }

  loop_ = new MainLoop(args_);
  loop_->audio_receiver_factory_ = executor_->GetAudioReceiverFactory();
  hotkey::Init(loop_);

  auto router = new Router(conn_, loop_);
  router->Start();

  Log("Running loop...");
  loop_->Run();
  Log("Loop finished running!");

  exit(exit_code_);
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

  if (!conn_->IsConnected()) {
    // if we haven't connected by this point, we never will:
    // the child doesn't exist anymore.
    // this also works for "disappearing launchers": even the launchers
    // should connect once first, and then their spawn will use a second connection.
    exit(fate.code);
  }
}

Runner::~Runner() {
  // muffin
}

} // namespace capsule
