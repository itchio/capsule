#pragma once

#include <capsulerun.h>
#include "./io.h"

#include "LockingQueue.h"
#include "ShmemRead.h"

#include <mutex>

struct FrameInfo {
  int index;
  int64_t timestamp;
};

class VideoReceiver {
  public:
    VideoReceiver(capsule_io_t *io, video_format_t vfmt, ShmemRead *shm) :
      io(io),
      vfmt(vfmt),
      shm(shm),
      stopped(false)
      {};
    ~VideoReceiver();
    void frameCommitted(int index, int64_t timestamp);
    int receiveFormat(video_format_t *vfmt);
    int receiveFrame(uint8_t *buffer, size_t buffer_size, int64_t *timestamp);
    void stop();

  private:
    char *mapped;
    LockingQueue<FrameInfo> queue;

    capsule_io_t *io;
    video_format_t vfmt;
    ShmemRead *shm;

    bool stopped;
    std::mutex stopped_mutex;
};
