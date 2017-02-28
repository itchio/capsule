
#include <capsule/messages.h>

#include "MainLoop.h"

#if defined(CAPSULE_LINUX)
#include <sys/mman.h>
#include <sys/stat.h> // for mode constants
#include <fcntl.h>    // for O_* constants
#include <unistd.h>   // unlink
#elif defined(CAPSULE_MACOS)
#include <sys/mman.h>
#include <sys/stat.h> // for mode constants
#include <fcntl.h>    // for O_* constants
#include <errno.h>    // for errno
#include <unistd.h>   // unlink
#else
#define LEAN_AND_MEAN
#include <windows.h>
#undef LEAN_AND_MEAN

#include <io.h>
#endif

using namespace Capsule::Messages;
using namespace std;

int receive_video_format (MainLoop *ml, video_format_t *vfmt) {
  return ml->videoReceiver->receiveFormat(vfmt);
}

int receive_video_frame (MainLoop *ml, uint8_t *buffer, size_t buffer_size, int64_t *timestamp) {
  return ml->videoReceiver->receiveFrame(buffer, buffer_size, timestamp);
}

void MainLoop::run () {
  capsule_log("In MainLoop::run, exec is %s", args->exec);

  while (true) {
    char *buf = READPKT(io);
    if (!buf) {
      capsule_log("MainLoop::run: pipe closed");
      break;
    }

    auto pkt = GetPacket(buf);
    capsule_log("MainLoop::run: received %s", EnumNameMessage(pkt->message_type()));
    switch (pkt->message_type()) {
      case Message_VideoSetup: {
        setupEncoder(pkt);
        break;
      }
      case Message_VideoFrameCommitted: {
        auto vfc = static_cast<const VideoFrameCommitted*>(pkt->message());
        videoReceiver->frameCommitted(vfc->index(), vfc->timestamp());
        break;
      }
      default: {
        capsule_log("Unknown message type, not sure what to do");
        break;
      }
    }

    delete[] buf;
  }
}

void MainLoop::setupEncoder (const Packet *pkt) {
  capsule_log("Setting up encoder");

  auto vs = static_cast<const VideoSetup*>(pkt->message());

  video_format_t vfmt;
  vfmt.width = vs->width();
  vfmt.height = vs->height();
  vfmt.format = (capsule_pix_fmt_t) vs->pix_fmt();
  vfmt.vflip = vs->vflip();
  vfmt.pitch = vs->pitch();

  // TODO: wrap shm into class
  auto shmem = vs->shmem();
  capsule_log("shm path: %s", shmem->path()->str().c_str());
  capsule_log("shm size: %d", shmem->size());

  char *mapped = nullptr;

#if defined(CAPSULE_WINDOWS)
  HANDLE hMapFile = OpenFileMappingA(
    FILE_MAP_READ, // read access
    FALSE, // do not inherit the name
    shmem->path()->str().c_str() // name of mapping object
  );

  if (!hMapFile) {
    capsule_log("Could not open shmem area");
    exit(1);
  }

  mapped = (char*) MapViewOfFile(
    hMapFile,
    FILE_MAP_READ, // read access
    0,
    0,
    shmem->size()
  );
#else
  int shmem_handle = shm_open(shmem->path()->str().c_str(), O_RDONLY, 0755);
  if (!(shmem_handle > 0)) {
    capsule_log("Could not open shmem area");
    exit(1);
  }

  mapped = (char *) mmap(
      nullptr, // addr
      shmem->size(), // length
      PROT_READ, // prot
      MAP_SHARED, // flags
      shmem_handle, // fd
      0 // offset
  );
#endif

  if (!mapped) {
    capsule_log("Could not map shmem area");
    exit(1);
  }

  // TODO: split into Session class
  videoReceiver = new VideoReceiver(io, vfmt, mapped);

  // FIXME: leak
  struct encoder_params_s *encoder_params = (struct encoder_params_s *) malloc(sizeof(struct encoder_params_s));
  memset(encoder_params, 0, sizeof(encoder_params));
  encoder_params->private_data = this;
  encoder_params->receive_video_format = (receive_video_format_t) receive_video_format;
  encoder_params->receive_video_frame  = (receive_video_frame_t) receive_video_frame;

  encoder_params->has_audio = 0;  

  encoderThread = new thread(encoder_run, encoder_params);
}
