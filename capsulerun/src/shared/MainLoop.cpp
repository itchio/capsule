
#include <capsule/messages.h>

#include "MainLoop.h"

#include <microprofile.h>

using namespace Capsule::Messages;
using namespace std;

MICROPROFILE_DEFINE(MainLoopMain, "MainLoop", "Main", 0xff0000);
MICROPROFILE_DEFINE(MainLoopCycle, "MainLoop", "Cycle", 0xff00ff38);
MICROPROFILE_DEFINE(MainLoopRead, "MainLoop", "Read", 0xff00ff00);
MICROPROFILE_DEFINE(MainLoopProcess, "MainLoop", "Process", 0xff773744);

void MainLoop::run () {
  MICROPROFILE_SCOPE(MainLoopCycle);
  capsule_log("In MainLoop::run, exec is %s", args->exec);

  while (true) {
    MICROPROFILE_SCOPE(MainLoopCycle);
    MicroProfileFlip(0);

    char *buf;

    {
      MICROPROFILE_SCOPE(MainLoopRead);
      buf = conn->read();
      if (!buf) {
        capsule_log("MainLoop::run: pipe closed");
        break;
      }
    }

    {
      MICROPROFILE_SCOPE(MainLoopProcess);
      auto pkt = GetPacket(buf);
      switch (pkt->message_type()) {
        case Message_VideoSetup: {
          auto vs = static_cast<const VideoSetup*>(pkt->message());
          start_session(vs);
          break;
        }
        case Message_VideoFrameCommitted: {
          auto vfc = static_cast<const VideoFrameCommitted*>(pkt->message());
          if (session && session->video) {
            session->video->frame_committed(vfc->index(), vfc->timestamp());
          } else {
            capsule_log("no session, ignoring VideoFrameCommitted")
          }
          break;
        }
        default: {
          capsule_log("MainLoop::run: received %s - not sure what to do", EnumNameMessage(pkt->message_type()));
          break;
        }
      }
    }

    delete[] buf;
  }

  end_session();  
  join_sessions();
}

void MainLoop::capture_flip () {
  capsule_log("MainLoop::capture_flip")
  if (session) {
    capture_stop();
  } else {
    // TODO: ignore subsequent capture_start until the capture actually started
    capture_start();
  }
}

void MainLoop::capture_start () {
  flatbuffers::FlatBufferBuilder builder(1024);
  auto cps = CreateCaptureStart(builder);
  auto opkt = CreatePacket(builder, Message_CaptureStart, cps.Union());
  builder.Finish(opkt);
  conn->write(builder);
}

void MainLoop::end_session () {
  if (!session) {
    capsule_log("MainLoop::end_session: no session to end")
    return;
  }

  capsule_log("MainLoop::end_session: ending %p", session)
  auto old_session = session;
  session = nullptr;
  old_session->stop();
  old_sessions.push_back(old_session);
}

void MainLoop::join_sessions () {
  capsule_log("MainLoop::join_sessions: joining %d sessions", old_sessions.size())

  for (Session *session: old_sessions) {
    capsule_log("MainLoop::join_sessions: joining session %p", session)
    session->join();
  }

  capsule_log("MainLoop::join_sessions: joined all sessions!")
}

void MainLoop::capture_stop () {
  end_session();

  flatbuffers::FlatBufferBuilder builder(1024);
  auto cps = CreateCaptureStop(builder);
  auto opkt = CreatePacket(builder, Message_CaptureStop, cps.Union());
  builder.Finish(opkt);
  conn->write(builder);
}

void MainLoop::start_session (const VideoSetup *vs) {
  capsule_log("Setting up encoder");

  video_format_t vfmt;
  vfmt.width = vs->width();
  vfmt.height = vs->height();
  vfmt.format = (capsule_pix_fmt_t) vs->pix_fmt();
  vfmt.vflip = vs->vflip();
  vfmt.pitch = vs->pitch();

  auto shm_path = vs->shmem()->path()->str();  
  auto shm = new ShmemRead(
    shm_path,
    vs->shmem()->size()
  );

  auto video = new VideoReceiver(conn, vfmt, shm);
  AudioReceiver *audio = nullptr;
  if (audio_receiver_factory) {
    audio = audio_receiver_factory();
  }

  session = new Session(args, video, audio);
  session->start();
}
