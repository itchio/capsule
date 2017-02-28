
#include <capsule/messages.h>

#include "MainLoop.h"

using namespace Capsule::Messages;
using namespace std;

int receive_video_format (MainLoop *ml, video_format_t *vfmt) {
  return ml->videoReceiver->receiveFormat(vfmt);
}

int receive_video_frame (MainLoop *ml, uint8_t *buffer, size_t buffer_size, int64_t *timestamp) {
  return ml->videoReceiver->receiveFrame(buffer, buffer_size, timestamp);
}

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
        setupEncoder(pkt);
        break;
      }
      case Message_VideoFrameCommitted: {
        auto vfc = static_cast<const VideoFrameCommitted*>(pkt->message());
        videoReceiver->frameCommitted(vfc->index(), vfc->timestamp());
        break;
      }
      default: {
        capsule_log("Unknown message type, not sure what to do");
        break;
      }
    }

    delete[] buf;
  }

  if (videoReceiver) {
    videoReceiver->close();
  }

  if (encoderThread) {
    capsule_log("Waiting for encoder thread...");
    encoderThread->join();
    delete encoderThread;
  }
}

void MainLoop::setupEncoder (const Packet *pkt) {
  capsule_log("Setting up encoder");

  auto vs = static_cast<const VideoSetup*>(pkt->message());

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

  // TODO: split into Session class
  videoReceiver = new VideoReceiver(io, vfmt, shm);

  // FIXME: leak
  struct encoder_params_s *encoder_params = (struct encoder_params_s *) malloc(sizeof(struct encoder_params_s));
  memset(encoder_params, 0, sizeof(encoder_params));
  encoder_params->private_data = this;
  encoder_params->receive_video_format = (receive_video_format_t) receive_video_format;
  encoder_params->receive_video_frame  = (receive_video_frame_t) receive_video_frame;

  encoder_params->has_audio = 0;  

  encoderThread = new thread(encoder_run, encoder_params);
}
