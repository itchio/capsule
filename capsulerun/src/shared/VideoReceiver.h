#pragma once

#include <mutex>

#include <shoom.h>

#include <capsulerun.h>
#include "LockingQueue.h"
#include "Connection.h"

struct FrameInfo {
  int index;
  int64_t timestamp;
};

class VideoReceiver {
  public:
    VideoReceiver(Connection *conn, video_format_t vfmt, shoom::Shm *shm, int num_frames);
    ~VideoReceiver();
    void FrameCommitted(int index, int64_t timestamp);
    int ReceiveFormat(video_format_t *vfmt);
    int ReceiveFrame(uint8_t *buffer, size_t buffer_size, int64_t *timestamp);
    void Stop();

  private:
    Connection *conn;
    video_format_t vfmt;
    shoom::Shm *shm;

    char *mapped;
    LockingQueue<FrameInfo> queue;

    int num_frames;
    size_t frame_size;
    int commit_index;
    char *buffer;
    int *buffer_state;
    std::mutex buffer_mutex;

    bool stopped;
    std::mutex stopped_mutex;

    int overrun = 0;
};
