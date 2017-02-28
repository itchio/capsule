
#include <capsulerun_mainloop.h>

#include <mutex>
#include <thread>

using namespace std;

mutex capturing_mutex;
mutex frame_mutex;

#define mlprint(...) capsule_log(__VA_ARGS__)

int capsule_mainloop_receive_video_frame (encoder_private_t *p, uint8_t *buffer, size_t buffer_size, int64_t *timestamp) {

}

void capsule_mainloop_run(capsule_mainloop_t *loop) {
  loop->running = true;
  thread *encoder_thread_p;
  encoder_params_t *encoder_params_p;

  while (loop->running) {
    {
      lock_guard<mutex> lock(capturing_mutex);
      if (loop->capturing && !encoder_thread_p) {
        capsule_mainloop_start_capture(loop);
      }
    }

    char *buf = READPKT(io);
    if (!buf) {
      capsule_log("capsule_mainloop_run: pipe closed");
      loop->running = false;
    }

    auto pkt = GetPacket(buf);
    mlprint("capsule_mainloop_run: received %s", EnumNameMessage(pkt->message_type()));
    switch (pkt->message_type()) {
      case Message_VideoSetup: {
        break;
      }
      case Message_VideoFrameCommitted: {
        break;
      }
    }
  }

  if (encoder_thread_p) {
    encoder_thread_p->join();
  }
}
