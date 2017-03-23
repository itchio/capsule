
#include "runner.h"

#include <string>

#include "macros.h"
#include "hotkey.h"

namespace capsule {

void Runner::Run () {
  conn_ = new Connection(std::string(args_->pipe));

  if (args_->exec) {
    process_ = executor_->LaunchProcess(args_);
    if (!process_) {
      CapsuleLog("Couldn't start child, bailing out");
      exit(127);
    }

    CapsuleLog("Watching on child in the background");
    wait_thread_ = new std::thread(&Runner::WaitForChild, this);
  }

  CapsuleLog("Connecting pipe...");
  conn_->Connect();

  if (!conn_->IsConnected()) {
    CapsuleLog("Couldn't connect pipe, bailing out");
    exit(127);
  }

  loop_ = new MainLoop(args_, conn_);
  loop_->audio_receiver_factory_ = executor_->GetAudioReceiverFactory();
  hotkey::Init(loop_);
  loop_->Run();

  exit(exit_code_);
}

void Runner::WaitForChild () {
  ProcessFate fate;
  memset(&fate, 0, sizeof(fate));

  process_->Wait(&fate);
  exit_code_ = fate.code;

  if (fate.status == kProcessStatusExited) {
    if (fate.code == 0) {
      CapsuleLog("Child exited gracefully");
    } else {
      CapsuleLog("Child exited with code %d", fate.code);
    }
  } else if (fate.status == kProcessStatusSignaled) {
    CapsuleLog("Child killed by signal %d", fate.code);
  } else if (fate.status == kProcessStatusUnknown) {
    CapsuleLog("Child left in unknown state");
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
