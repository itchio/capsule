#pragma once

#include <mutex>

#include <shoom.h>

#include "locking_queue.h"
#include "connection.h"
#include "encoder.h"

namespace capsule {
namespace video {

enum FrameState {
  kFrameStateAvailable = 0,
  kFrameStateCommitted,
  kFrameStateProcessing,
};

struct FrameInfo {
  int index;
  int64_t timestamp;
};

class VideoReceiver {
  public:
    VideoReceiver(Connection *conn, encoder::VideoFormat vfmt, shoom::Shm *shm, int num_frames);
    ~VideoReceiver();
    void FrameCommitted(int index, int64_t timestamp);
    int ReceiveFormat(encoder::VideoFormat *vfmt);
    int ReceiveFrame(uint8_t *buffer, size_t buffer_size, int64_t *timestamp);
    void Stop();

  private:
    Connection *conn_;
    encoder::VideoFormat vfmt_;
    shoom::Shm *shm_;

    char *mapped_;
    LockingQueue<FrameInfo> queue_;

    int num_frames_;
    size_t frame_size_;
    int commit_index_;
    char *buffer_;
    int *buffer_state_;
    std::mutex buffer_mutex_;

    bool stopped_ = false;
    std::mutex stopped_mutex_;

    int overrun_ = 0;
};

} // namespace video
} // namespace capsule
