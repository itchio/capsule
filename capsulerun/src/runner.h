
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

#pragma once

#include "args.h"
#include "connection.h"
#include "main_loop.h"
#include "router.h"

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

    Router *router_ = nullptr;

    MainArgs *args_ = nullptr;
    ExecutorInterface *executor_ = nullptr;

    ProcessInterface *process_ = nullptr;
    std::thread *wait_thread_ = nullptr;

    Connection *conn_ = nullptr;
    MainLoop *loop_ = nullptr;

    int exit_code_ = 0;
};

} // namespace capsule
