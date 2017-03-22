
#pragma once

#include "../runner.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN

namespace capsule {
namespace windows {

class Process : public ProcessInterface {
  public:
    Process(HANDLE process_handle) :
      process_handle_(process_handle) {};
    ~Process() override;

    std::string Wait() override;

  private:
    HANDLE process_handle_ = INVALID_HANDLE_VALUE;
};

class Executor : public ExecutorInterface {
  public:
    Executor();
    ~Executor() override;

    ProcessInterface *LaunchProcess(MainArgs *args) override;
    AudioReceiverFactory GetAudioReceiverFactory() override;
};

} // namespace windows
} // namespace capsule
