
#include <capsule/messages.h>

#include "MainLoop.h"

#include <microprofile.h>

using namespace Capsule::Messages;
using namespace std;

MICROPROFILE_DEFINE(MainLoopMain, "MainLoop", "Main", 0xff0000);
MICROPROFILE_DEFINE(MainLoopCycle, "MainLoop", "Cycle", 0xff00ff38);
MICROPROFILE_DEFINE(MainLoopRead, "MainLoop", "Read", 0xff00ff00);
MICROPROFILE_DEFINE(MainLoopProcess, "MainLoop", "Process", 0xff773744);

void MainLoop::Run () {
  MICROPROFILE_SCOPE(MainLoopCycle);
  capsule_log("In MainLoop::Run, exec is %s", args->exec);

  while (true) {
    MICROPROFILE_SCOPE(MainLoopCycle);
    MicroProfileFlip(0);

    char *buf;

    {
      MICROPROFILE_SCOPE(MainLoopRead);
      buf = conn->read();
      if (!buf) {
        capsule_log("MainLoop::Run: pipe closed");
        break;
      }
    }

    {
      MICROPROFILE_SCOPE(MainLoopProcess);
      auto pkt = GetPacket(buf);
      switch (pkt->message_type()) {
        case Message_VideoSetup: {
          auto vs = static_cast<const VideoSetup*>(pkt->message());
          StartSession(vs);
          break;
        }
        case Message_VideoFrameCommitted: {
          auto vfc = static_cast<const VideoFrameCommitted*>(pkt->message());
          if (session && session->video) {
            session->video->FrameCommitted(vfc->index(), vfc->timestamp());
          } else {
            capsule_log("no session, ignoring VideoFrameCommitted")
          }
          break;
        }
        default: {
          capsule_log("MainLoop::Run: received %s - not sure what to do", EnumNameMessage(pkt->message_type()));
          break;
        }
      }
    }

    delete[] buf;
  }

  EndSession();  
  JoinSessions();
}

void MainLoop::CaptureFlip () {
  capsule_log("MainLoop::CaptureFlip")
  if (session) {
    CaptureStop();
  } else {
    // TODO: ignore subsequent CaptureStart until the capture actually started
    CaptureStart();
  }
}

void MainLoop::CaptureStart () {
  flatbuffers::FlatBufferBuilder builder(1024);
  auto cps = CreateCaptureStart(builder, args->fps, args->size_divider, args->gpu_color_conv);
  auto opkt = CreatePacket(builder, Message_CaptureStart, cps.Union());
  builder.Finish(opkt);
  conn->write(builder);
}

void MainLoop::EndSession () {
  if (!session) {
    capsule_log("MainLoop::end_session: no session to end")
    return;
  }

  capsule_log("MainLoop::end_session: ending %p", session)
  auto old_session = session;
  session = nullptr;
  old_session->Stop();
  old_sessions.push_back(old_session);
}

void MainLoop::JoinSessions () {
  capsule_log("MainLoop::join_sessions: joining %d sessions", old_sessions.size())

  for (Session *session: old_sessions) {
    capsule_log("MainLoop::join_sessions: joining session %p", session)
    session->Join();
  }

  capsule_log("MainLoop::join_sessions: joined all sessions!")
}

void MainLoop::CaptureStop () {
  EndSession();

  flatbuffers::FlatBufferBuilder builder(1024);
  auto cps = CreateCaptureStop(builder);
  auto opkt = CreatePacket(builder, Message_CaptureStop, cps.Union());
  builder.Finish(opkt);
  conn->write(builder);
}

void MainLoop::StartSession (const VideoSetup *vs) {
  capsule_log("Setting up encoder");

  video_format_t vfmt;
  vfmt.width = vs->width();
  vfmt.height = vs->height();
  vfmt.format = (capsule_pix_fmt_t) vs->pix_fmt();
  vfmt.vflip = vs->vflip();
  
  // TODO: support offset (for planar formats)

  // TODO: support multiple linesizes (for planar formats)
  auto linesize_vec = vs->linesize();
  vfmt.pitch = linesize_vec->Get(0);

  auto shm_path = vs->shmem()->path()->str();  
  auto shm = new shoom::Shm(shm_path, vs->shmem()->size());
  int ret = shm->Open();
  if (ret != shoom::kOK) {
    capsule_log("Could not open shared memory area: code %d", ret);
    return;
  }

  int num_buffered_frames = 60;
  if (args->buffered_frames) {
    num_buffered_frames = args->buffered_frames;
  }

  auto video = new VideoReceiver(conn, vfmt, shm, num_buffered_frames);
  AudioReceiver *audio = nullptr;
  if (audio_receiver_factory && !args->no_audio) {
    audio = audio_receiver_factory();
  }

  session = new Session(args, video, audio);
  session->Start();
}
