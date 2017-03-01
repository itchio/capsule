
#include <capsule/messages.h>

#include "MainLoop.h"

using namespace Capsule::Messages;
using namespace std;

void MainLoop::run () {
  capsule_log("In MainLoop::run, exec is %s", args->exec);

  while (true) {
    char *buf = READPKT(io);
    if (!buf) {
      capsule_log("MainLoop::run: pipe closed");
      break;
    }

    auto pkt = GetPacket(buf);
    capsule_log("MainLoop::run: received %s", EnumNameMessage(pkt->message_type()));
    switch (pkt->message_type()) {
      case Message_VideoSetup: {
        auto vs = static_cast<const VideoSetup*>(pkt->message());
        startSession(vs);
        break;
      }
      case Message_VideoFrameCommitted: {
        auto vfc = static_cast<const VideoFrameCommitted*>(pkt->message());
        if (session && session->video) {
          session->video->frameCommitted(vfc->index(), vfc->timestamp());
        } else {
          capsule_log("no session, ignoring VideoFrameCommitted")
        }
        break;
      }
      default: {
        capsule_log("Unknown message type, not sure what to do");
        break;
      }
    }

    delete[] buf;
  }

  capsule_log("address of session: %p", session);
  if (session) {
    session->stop();
  }
}

void MainLoop::startSession (const VideoSetup *vs) {
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

  auto video = new VideoReceiver(io, vfmt, shm);

  // TODO: handle case where we can't start session
  session = new Session(video, nullptr);
  session->start();
}
