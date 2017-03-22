
#pragma once

#include "args.h"
#include "connection.h"
#include "main_loop.h"

#include <thread>

namespace capsule {

enum ProcessStatus {
  kProcessStatusUnknown = 0,
  kProcessStatusExited,
  kProcessStatusSignaled,
};

struct ProcessFate {
  ProcessStatus status;
  int code; // exit code if exited, signal if signalled
};

class ProcessInterface {
  public:
    virtual ~ProcessInterface() {};
    virtual void Wait(ProcessFate *fate) = 0;
};

class ExecutorInterface {
  public:
    virtual ~ExecutorInterface() {};
    virtual ProcessInterface *LaunchProcess(MainArgs *args) = 0;
    virtual AudioReceiverFactory GetAudioReceiverFactory() = 0;
};

class Runner {
  public:
    Runner(MainArgs *args, ExecutorInterface *executor) :
      args_(args),
      executor_(executor) {};
    ~Runner();

    void Run();
    

  private:
    void WaitForChild();

    MainArgs *args_ = nullptr;
    ExecutorInterface *executor_ = nullptr;

    ProcessInterface *process_ = nullptr;
    std::thread *wait_thread_ = nullptr;

    Connection *conn_ = nullptr;
    MainLoop *loop_ = nullptr;

    int exit_code_ = 0;
};

} // namespace capsule
