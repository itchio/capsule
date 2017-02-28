#pragma once

#include <capsule/messages.h>

#include <capsulerun.h>
#include <capsulerun_args.h>
#include "./io.h"

#include "VideoReceiver.h"
#include "AudioReceiver.h"
#include "Session.h"

#include <thread>

class MainLoop {
  public:
    MainLoop(capsule_args_t *args, capsule_io_t *io) :
      args(args),
      io(io)
      {};
    void run(void);
    void startSession(const Capsule::Messages::VideoSetup *vs);

  private:
    capsule_args_t *args;
    capsule_io_t *io;

    Session *session;    
};
