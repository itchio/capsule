
#pragma once

#include "../runner.h"

namespace capsule {
namespace macos {

class Process: public ProcessInterface {
  public:
    Process(pid_t pid) :
      pid_(pid) {};
    ~Process() override;

    void Wait(ProcessFate *fate) override;

  private:
    pid_t pid = -1;
};

class Executor : public ExecutorInterface {
  public:
    Executor();
    ~Executor() override;

    ProcessInterface *LaunchProcess(MainArgs *args) override;
    AudioReceiverFactory GetAudioReceiverFactory() override;
};

} // namespace macos
} // namespace capsule