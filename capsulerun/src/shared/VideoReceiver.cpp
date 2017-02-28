
#include <capsule/messages.h>

#include "VideoReceiver.h"

using namespace Capsule::Messages;

int VideoReceiver::receiveFormat(video_format_t *vfmt_out) {
  memcpy(vfmt_out, &vfmt, sizeof(video_format_t));
  return 0;
}

int VideoReceiver::receiveFrame(uint8_t *buffer, size_t buffer_size, int64_t *timestamp) {
  // FIXME: use tryWaitAndPop and check another condition
  // for early stoppage
  FrameInfo info {};
  frameQueue.waitAndPop(info);
  
  void *source = mapped + (buffer_size * info.index);
  memcpy(buffer, source, buffer_size);
  *timestamp = info.timestamp;

  flatbuffers::FlatBufferBuilder builder(1024);
  auto vfp = CreateVideoFrameProcessed(builder, info.index); 
  auto opkt = CreatePacket(builder, Message_VideoFrameProcessed, vfp.Union());
  builder.Finish(opkt);
  WRITEPKT(builder, io);

  return buffer_size;
}

void VideoReceiver::frameCommitted(int index, int64_t timestamp) {
  FrameInfo info {index, timestamp};
  frameQueue.push(info);
}
