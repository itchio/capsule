#pragma once

#include <capsulerun.h>
#include <capsulerun_args.h>
#include "../shared/io.h"

class VideoFrameReceiver {
  public:
    VideoFrameReceiver(capsule_io_t *io, video_format_t vfmt) :
      io(io),
      vfmt(vfmt)
      {};
    void frameReady(int index);
    void receiveFrame();

  private:
    char *mapped;

    capsule_io_t *io;
    video_format_t vfmt;
};

class MainLoop {
  public:
    MainLoop(capsule_args_t *args, capsule_io_t *io) :
      args(args),
      io(io)
      {};
    void run(void);

  private:
    capsule_args_t *args;
    capsule_io_t *io;
    bool running;
    bool capturing;

    std::thread *encoderThread;
    VideoFrameReceiver *frameReceiver;
};
