
#include <capsule/messages.h>
#include <capsulerun_mainloop.h>

#include "../shared/io.h"

using namespace Capsule::Messages;
using namespace std;

int receive_video_format (MainLoop *ml, video_format_t *vfmt) {
  memcpy(vfmt, ml->vfmt);
}

int receive_video_frame (MainLoop *ml, uint8_t *buffer, size_t buffer_size, int64_t *timestamp) {
  ml->frameReceiver->receiveFrame(buffer, buffer_size, timestamp);
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
        capsule_log("Received VideoSetup");
        setupEncoder(pkt);
        break;
      }
      default: {
        capsule_log("Unknown message type, not sure what to do");
        break;
      }
    }

    delete[] buf;
  }
}

void MainLoop::setupEncoder (const Packet &pkt) {
  capsule_log("Setting up encoder");

  auto vs = static_cast<const VideoSetup*>(pkt->message());

  video_format_t vfmt;
  vfmt.width = vs->width();
  vfmt.height = vs->height();
  vfmt.format = (capsule_pix_fmt_t) vs->pix_fmt();
  vfmt.vflip = vs->vflip();
  vfmt.pitch = vs->pitch();

  frameReceiver = new VideoFrameReceiver(io, vfmt);
}

void VideoFrameReceiver::receiveFormat () {

}

void VideoFrameReceiver::receiveFrame () {

}

void VideoFrameReceiver::frameReady () {

}
