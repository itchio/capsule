
#include <capsule/messages.h>
#include <capsule/logging.h>

#include "VideoReceiver.h"

using namespace Capsule::Messages;
using namespace std;

#include <stdexcept>

#include <microprofile.h>

#define VIDEO_FRAME_PROCESSED 0
#define VIDEO_FRAME_COMMITTED 1
#define VIDEO_FRAME_PROCESSING 2

MICROPROFILE_DEFINE(VideoReceiverWait, "VideoReceiver", "VWait", MP_CHOCOLATE3);
MICROPROFILE_DEFINE(VideoReceiverCopy1, "VideoReceiver", "VCopy1", MP_CORNSILK3);
MICROPROFILE_DEFINE(VideoReceiverCopy2, "VideoReceiver", "VCopy2", MP_PINK3);

VideoReceiver::VideoReceiver (Connection *conn_in, video_format_t vfmt_in, shoom::Shm *shm_in, int num_frames_in) {
  capsule_log("Initializing VideoReceiver, buffered frames = %d", num_frames_in);
  conn = conn_in;
  vfmt = vfmt_in;
  shm = shm_in;
  stopped = false;

  num_frames = num_frames_in;
  frame_size = vfmt.pitch * vfmt.height;
  capsule_log("VideoReceiver: total buffer size in RAM: %.2f MB", (float) (frame_size * num_frames) / 1024.0f / 1024.0f);
  buffer = (char *) calloc(num_frames, frame_size);

  buffer_state = (int *) calloc(num_frames, sizeof(int));
  for (int i = 0; i < num_frames; i++) {
    buffer_state[i] = VIDEO_FRAME_PROCESSED;
  }
  commit_index = 0;
}

int VideoReceiver::ReceiveFormat(video_format_t *vfmt_out) {
  memcpy(vfmt_out, &vfmt, sizeof(video_format_t));
  return 0;
}

int VideoReceiver::ReceiveFrame(uint8_t *buffer_out, size_t buffer_size_out, int64_t *timestamp_out) {
  FrameInfo info {};

  while (true) {
    {
      MICROPROFILE_SCOPE(VideoReceiverWait);
      if (queue.tryWaitAndPop(info, 200)) {
        // got a frame!
        break;
      }
    }

    // didn't get a frame, are we stopped?
    {
      lock_guard<mutex> lock(stopped_mutex);
      if (stopped) {
        return 0;
      }
    }
  }

  if (frame_size != buffer_size_out) {
    fprintf(stderr, "capsule internal error: expected frame_size (%" PRIdS ") and buffer_size_out (%" PRIdS ") to match, but they didn't\n", frame_size, buffer_size_out);
    fflush(stderr);
    throw runtime_error("frame_size !== buffer_size_out");
  }

  *timestamp_out = info.timestamp;

  char *src = buffer + (info.index * frame_size);
  char *dst = (char *) buffer_out;
  {
    MICROPROFILE_SCOPE(VideoReceiverCopy2);
    memcpy(dst, src, frame_size);
  }

  {
    lock_guard<mutex> lock(buffer_mutex);

    buffer_state[info.index] = VIDEO_FRAME_PROCESSING;
    int prev_index = (info.index == 0) ? (num_frames - 1) : (info.index - 1);
    if (buffer_state[prev_index] == VIDEO_FRAME_PROCESSING) {
      buffer_state[prev_index] = VIDEO_FRAME_PROCESSED;
    }

    /////////////////////////////////
    // <poor man's profiling>
    /////////////////////////////////
    if ((info.index % 10) == 0) {
      int committed_frames = 0;
      for (int i = 0; i < num_frames; i++) {
        if (buffer_state[i] == VIDEO_FRAME_COMMITTED) {
          committed_frames++;
        }
      }
      fprintf(stderr, "buffer fill: %d/%d, skipped %d\n", committed_frames, num_frames, overrun);
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
    lock_guard<mutex> lock(stopped_mutex);
    if (stopped) {
      // just ignore
      return;
    }
  }

  int commit = 0;

  {
    lock_guard<mutex> lock(buffer_mutex);

    if (buffer_state[commit_index] != VIDEO_FRAME_PROCESSED) {
      // no room, just skip it
      overrun++;
    } else {
      commit = 1;
    }
  }

  if (commit) {
      // got room, copy it
      char *src = reinterpret_cast<char*>(shm->Data()) + (frame_size * index);
      char *dst = buffer + (frame_size * commit_index);
      {
        MICROPROFILE_SCOPE(VideoReceiverCopy1);
        memcpy(dst, src, frame_size);
      }

      FrameInfo info {commit_index, timestamp};
      queue.push(info);

      {
        lock_guard<mutex> lock(buffer_mutex);

        buffer_state[commit_index] = VIDEO_FRAME_COMMITTED;
        commit_index = (commit_index + 1) % num_frames;
      }
  }

  // in both cases, free up that index for the sender
  flatbuffers::FlatBufferBuilder builder(1024);
  auto vfp = CreateVideoFrameProcessed(builder, index); 
  auto opkt = CreatePacket(builder, Message_VideoFrameProcessed, vfp.Union());
  builder.Finish(opkt);
  conn->write(builder);
}

void VideoReceiver::Stop() {
  lock_guard<mutex> lock(stopped_mutex);
  stopped = true;
}

VideoReceiver::~VideoReceiver () {
  free(buffer_state);
  free(buffer);
  delete shm;
}
