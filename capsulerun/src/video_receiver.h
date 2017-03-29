
/*
 *  capsule - the game recording and overlay toolkit
 *  Copyright (C) 2017, Amos Wenger
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details:
 * https://github.com/itchio/capsule/blob/master/LICENSE
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

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
