#pragma once

#include <thread>
#include <vector>

#include <shoom.h>

#include <capsule/messages.h>
#include <capsulerun.h>
#include <capsulerun_args.h>

#include "VideoReceiver.h"
#include "AudioReceiver.h"
#include "Session.h"
#include "Connection.h"

typedef AudioReceiver * (*audio_receiver_factory_t)();

class MainLoop {
  public:
    MainLoop(capsule_args_t *args, Connection *conn) :
      args(args),
      conn_(conn)
      {};
    void Run(void);
    void CaptureFlip();

    audio_receiver_factory_t audio_receiver_factory = nullptr;

  private:
    void EndSession();
    void JoinSessions();

    void CaptureStart();
    void CaptureStop();
    void StartSession(const Capsule::Messages::VideoSetup *vs);

    capsule_args_t *args;

    Connection *conn_;
    Session *session_ = nullptr;
    std::vector<Session *> old_sessions;
};
