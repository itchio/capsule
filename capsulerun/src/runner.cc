
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
}

void Runner::WaitForChild () {
  // TODO: a string as error isn't good enough, we need a data structure
  // for child exit status
  auto error = process_->Wait();

  if (error == "") {
    CapsuleLog("Child exited peacefully");
  } else {
    CapsuleLog("Child error: %s", error.c_str());
  }

  if (!conn_->IsConnected()) {
    exit(error == "" ? 0 : 127);
  }
}

Runner::~Runner() {
  // muffin
}

} // namespace capsule
