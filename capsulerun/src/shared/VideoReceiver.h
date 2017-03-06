#pragma once

#include <capsulerun.h>
#include "./io.h"

#include "LockingQueue.h"
#include "ShmemRead.h"
#include "Connection.h"

#include <mutex>

struct FrameInfo {
  int index;
  int64_t timestamp;
};

class VideoReceiver {
  public:
    VideoReceiver(Connection *conn, video_format_t vfmt, ShmemRead *shm, int num_frames);
    ~VideoReceiver();
    void frame_committed(int index, int64_t timestamp);
    int receive_format(video_format_t *vfmt);
    int receive_frame(uint8_t *buffer, size_t buffer_size, int64_t *timestamp);
    void stop();

  private:
    Connection *conn;
    video_format_t vfmt;
    ShmemRead *shm;

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
