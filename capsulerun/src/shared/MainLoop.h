#pragma once

#include <capsule/messages.h>

#include <capsulerun.h>
#include <capsulerun_args.h>

#include "VideoReceiver.h"
#include "AudioReceiver.h"
#include "Session.h"
#include "Connection.h"

#include <thread>

class MainLoop {
  public:
    MainLoop(capsule_args_t *args, Connection *conn) :
      args(args),
      conn(conn)
      {};
    void run(void);
    void capture_flip();

  private:
    void capture_start();
    void capture_stop();
    void start_session(const Capsule::Messages::VideoSetup *vs);

    capsule_args_t *args;

    Connection *conn;
    Session *session = nullptr;
};
