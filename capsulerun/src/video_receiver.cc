
#include <capsule/messages.h>
#include <capsule/logging.h>

#include <lab/logging.h>

#include "video_receiver.h"

#include <stdexcept>

#include <microprofile.h>

MICROPROFILE_DEFINE(VideoReceiverWait, "VideoReceiver", "VWait", MP_CHOCOLATE3);
MICROPROFILE_DEFINE(VideoReceiverCopy1, "VideoReceiver", "VCopy1", MP_CORNSILK3);
MICROPROFILE_DEFINE(VideoReceiverCopy2, "VideoReceiver", "VCopy2", MP_PINK3);

namespace capsule {
namespace video {

VideoReceiver::VideoReceiver (Connection *conn, video_format_t vfmt, shoom::Shm *shm, int num_frames) {
  conn_ = conn;
  vfmt_ = vfmt;
  shm_ = shm;

  num_frames_ = num_frames;
  frame_size_ = vfmt_.pitch * vfmt_.height;
  CapsuleLog("VideoReceiver: initializing, buffer of %d frames", num_frames_);
  CapsuleLog("VideoReceiver: total buffer size in RAM: %.2f MB", (float) (frame_size_ * num_frames_) / 1024.0f / 1024.0f);
  buffer_ = (char *) calloc(num_frames_, frame_size_);

  buffer_state_ = (int *) calloc(num_frames_, sizeof(int));
  for (int i = 0; i < num_frames; i++) {
    buffer_state_[i] = kFrameStateAvailable;
  }
  commit_index_ = 0;
}

int VideoReceiver::ReceiveFormat(video_format_t *vfmt) {
  memcpy(vfmt, &vfmt_, sizeof(*vfmt));
  return 0;
}

int VideoReceiver::ReceiveFrame(uint8_t *buffer_out, size_t buffer_size_out, int64_t *timestamp_out) {
  FrameInfo info {};

  while (true) {
    {
      MICROPROFILE_SCOPE(VideoReceiverWait);
      if (queue_.TryWaitAndPop(info, 200)) {
        // got a frame!
        break;
      }
    }

    // didn't get a frame, are we stopped?
    {
      std::lock_guard<std::mutex> lock(stopped_mutex_);
      if (stopped_) {
        return 0;
      }
    }
  }

  if (frame_size_ != buffer_size_out) {
    fprintf(stderr, "capsule internal error: expected frame_size (%" PRIdS ") and buffer_size_out (%" PRIdS ") to match, but they didn't\n", frame_size_, buffer_size_out);
    fflush(stderr);
    throw std::runtime_error("frame_size !== buffer_size_out");
  }

  *timestamp_out = info.timestamp;

  char *src = buffer_ + (info.index * frame_size_);
  char *dst = (char *) buffer_out;
  {
    MICROPROFILE_SCOPE(VideoReceiverCopy2);
    memcpy(dst, src, frame_size_);
  }

  {
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    buffer_state_[info.index] = kFrameStateProcessing;
    int prev_index = (info.index == 0) ? (num_frames_ - 1) : (info.index - 1);
    if (buffer_state_[prev_index] == kFrameStateProcessing) {
      buffer_state_[prev_index] = kFrameStateAvailable;
    }

    /////////////////////////////////
    // <poor man's profiling>
    /////////////////////////////////
    if ((info.index % 10) == 0) {
      int committed_frames = 0;
      for (int i = 0; i < num_frames_; i++) {
        if (buffer_state_[i] == kFrameStateCommitted) {
          committed_frames++;
        }
      }
      fprintf(stderr, "buffer fill: %d/%d, skipped %d\n", committed_frames, num_frames_, overrun_);
      fflush(stderr);
    }
    /////////////////////////////////
    // </poor man's profiling>
    /////////////////////////////////
  }

  return buffer_size_out;
}

void VideoReceiver::FrameCommitted(int index, int64_t timestamp) {
  {
    std::lock_guard<std::mutex> lock(stopped_mutex_);
    if (stopped_) {
      // just ignore
      return;
    }
  }

  int commit = 0;

  {
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    if (buffer_state_[commit_index_] != kFrameStateAvailable) {
      // no room, just skip it
      overrun_++;
    } else {
      commit = 1;
    }
  }

  if (commit) {
      // got room, copy it
      char *src = reinterpret_cast<char*>(shm_->Data()) + (frame_size_ * index);
      char *dst = buffer_ + (frame_size_ * commit_index_);
      {
        MICROPROFILE_SCOPE(VideoReceiverCopy1);
        memcpy(dst, src, frame_size_);
      }

      FrameInfo info {commit_index_, timestamp};
      queue_.Push(info);

      {
        std::lock_guard<std::mutex> lock(buffer_mutex_);

        buffer_state_[commit_index_] = kFrameStateCommitted;
        commit_index_ = (commit_index_ + 1) % num_frames_;
      }
  }

  // in both cases, free up that index for the sender
  flatbuffers::FlatBufferBuilder builder(1024);
  auto vfp = messages::CreateVideoFrameProcessed(builder, index); 
  auto opkt = messages::CreatePacket(builder, messages::Message_VideoFrameProcessed, vfp.Union());
  builder.Finish(opkt);
  conn_->write(builder);
}

void VideoReceiver::Stop() {
  std::lock_guard<std::mutex> lock(stopped_mutex_);
  stopped_ = true;
}

VideoReceiver::~VideoReceiver () {
  free(buffer_state_);
  free(buffer_);
  delete shm_;
}

} // namespace video
} // namespace capsule
