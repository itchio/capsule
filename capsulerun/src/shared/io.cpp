
#include <capsule/constants.h>
#include <capsule/messages.h>

// mkfifo
#include <sys/types.h>
#include <sys/stat.h>

#include "io.h"
#include <capsule/messages.h>

#if defined(CAPSULE_LINUX)
#include <sys/mman.h>
#include <sys/stat.h> // for mode constants
#include <fcntl.h>    // for O_* constants
#elif defined(CAPSULE_OSX)
// TODO: missing includes for shm_open, mmap etc.
#endif

using namespace std;

void create_fifo (string fifo_path) {
  // remove previous fifo if any  
  unlink(fifo_path.c_str());

  // create fifo
  int fifo_ret = mkfifo(fifo_path.c_str(), 0644);
  if (fifo_ret != 0) {
    capsule_log("could not create fifo at %s (code %d)", fifo_path.c_str(), fifo_ret);
    exit(1);
  }
}

int capsule_receive_video_format (FILE *file, video_format_t *vfmt, char **mapped) {
  capsule_log("receiving video format...");
  char *buf = capsule_read_packet(file);
  if (!buf) {
    capsule_log("receive_video_format: pipe closed");
    exit(1);
  }

  auto pkt = GetPacket(buf);
  if (pkt->message_type() != Message_VideoSetup) {
    capsule_log("receive_video_format: expected VideoSetup message, got %s", EnumNameMessage(pkt->message_type()));
    exit(1);
  }

  auto vs = static_cast<const VideoSetup*>(pkt->message());

  vfmt->width = vs->width();
  vfmt->height = vs->height();
  vfmt->format = (capsule_pix_fmt_t) vs->pix_fmt();
  vfmt->vflip = vs->vflip();
  vfmt->pitch = vs->pitch();

  auto shmem = vs->shmem();
  capsule_log("shm path: %s", shmem->path()->str().c_str());
  capsule_log("shm size: %d", shmem->size());

  int shmem_handle = shm_open(shmem->path()->str().c_str(), O_RDONLY, 0755);
  if (!(shmem_handle > 0)) {
    capsule_log("Could not open shmem area");
    exit(1);
  }

  *mapped = (char *) mmap(
      nullptr, // addr
      shmem->size(), // length
      PROT_READ, // prot
      MAP_SHARED, // flags
      shmem_handle, // fd
      0 // offset
  );

  if (!*mapped) {
    capsule_log("Could not map shmem area");
    exit(1);
  }

  delete[] buf;
  return 0;
}

int capsule_receive_video_frame (FILE *file, uint8_t *buffer, size_t buffer_size, int64_t *timestamp, char *mapped) {
  char *buf = capsule_read_packet(file);

  if (!buf) {
    // pipe closed
    return 0;
  }

  auto pkt = GetPacket(buf);
  if (pkt->message_type() != Message_VideoFrameCommitted) {
    capsule_log("receive_video_frame: expected VideoFrameCommitted, got %s", EnumNameMessage(pkt->message_type()));
    exit(1);
  }

  auto vfc = static_cast<const VideoFrameCommitted*>(pkt->message());
  *timestamp = vfc->timestamp();
  void *source = mapped + (buffer_size * vfc->index());
  memcpy(buffer, source, buffer_size);

  flatbuffers::FlatBufferBuilder builder(1024);
  auto vfp = CreateVideoFrameProcessed(builder, vfc->index()); 
  auto opkt = CreatePacket(builder, Message_VideoFrameProcessed, vfp.Union());
  builder.Finish(opkt);
  capsule_write_packet(builder, p->fifo_w_file);

  delete[] buf;
  return buffer_size;
}
