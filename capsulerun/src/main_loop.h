#pragma once

#include <thread>
#include <vector>

#include <shoom.h>

#include <capsule/messages.h>
#include <capsulerun.h>

#include "args.h"
#include "video_receiver.h"
#include "audio_receiver.h"
#include "session.h"
#include "connection.h"

namespace capsule {

typedef audio::AudioReceiver * (*AudioReceiverFactory)();

class MainLoop {
  public:
    MainLoop(MainArgs *args, Connection *conn) :
      args_(args),
      conn_(conn)
      {};
    void Run(void);
    void CaptureFlip();

    AudioReceiverFactory audio_receiver_factory_ = nullptr;

  private:
    void EndSession();
    void JoinSessions();

    void CaptureStart();
    void CaptureStop();
    void StartSession(const messages::VideoSetup *vs);

    MainArgs *args_;

    Connection *conn_;
    Session *session_ = nullptr;
    std::vector<Session *> old_sessions_;
};

} // namespace capsule
