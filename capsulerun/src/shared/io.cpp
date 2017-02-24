
#include <capsule/constants.h>
#include <capsule/messages.h>

#include <capsulerun_macros.h>

// mkfifo
#include <sys/types.h>
#include <sys/stat.h>

#include "io.h"

#if defined(CAPSULE_LINUX)
#include <sys/mman.h>
#include <sys/stat.h> // for mode constants
#include <fcntl.h>    // for O_* constants
#include <unistd.h>   // unlink
#elif defined(CAPSULE_OSX)
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

using namespace std;
using namespace Capsule::Messages;

#if defined(CAPSULE_WINDOWS)

static void create_pipe(
  capsule_io_t *io,
  std::string &pipe_path
) {
  capsule_log("Created named pipe %s", pipe_path.c_str());
  io->pipe_handle = CreateNamedPipeA(
    pipe_path.c_str(),
    PIPE_ACCESS_DUPLEX, // open mode
    PIPE_TYPE_BYTE | PIPE_WAIT, // pipe mode
    1, // max instances
    16 * 1024 * 1024, // nOutBufferSize
    16 * 1024 * 1024, // nInBufferSize
    0, // nDefaultTimeout
    NULL // lpSecurityAttributes
  );

  if (!io->pipe_handle) {
    DWORD err = GetLastError();
    capsule_log("CreateNamedPipe failed, err = %d (%x)", err, err);
    exit(1);
  }
}
#else

static void create_fifo(
    std::string &fifo_path
) {
  // remove previous fifo if any  
  unlink(fifo_path.c_str());

  // create fifo
  int fifo_ret = mkfifo(fifo_path.c_str(), 0644);
  if (fifo_ret != 0) {
    capsule_log("could not create fifo at %s (code %d)", fifo_path.c_str(), fifo_ret);
    exit(1);
  }
}

#endif // CAPSULE_WINDOWS

void capsule_io_init(
    capsule_io_t *io,
#if defined(CAPSULE_WINDOWS)
    std::string &pipe_path
#else
    std::string &fifo_r_path,
    std::string &fifo_w_path
#endif // CAPSULE_WINDOWS
) {

#if defined(CAPSULE_WINDOWS)
  create_pipe(io, pipe_path);
#else
  create_fifo(fifo_r_path);
  io->fifo_r_path = &fifo_r_path;

  create_fifo(fifo_w_path);
  io->fifo_w_path = &fifo_w_path;
#endif // CAPSULE_WINDOWS
}

static int open_fifo (
  std::string &path,
  std::string purpose,
  int flags
) {
  capsule_log("opening %s fifo...", purpose.c_str());

  int fd = open(path.c_str(), flags);
  if (!fd) {
    capsule_log("could not open fifo for %s: %s", purpose.c_str(), strerror(errno));
    exit(1);
  }

  return fd;
}

void capsule_io_connect(
    capsule_io_t *io
) {
#if defined(CAPSULE_WINDOWS)
  BOOL success = ConnectNamedPipe(io->pipe_handle, NULL);
  if (!success) {
    capsule_log("Could not connect to named pipe");
    exit(1);
  }
  capsule_log("Connected to named pipe");

  int fd = _open_osfhandle((intptr_t) io->pipe_handle, 0);
  if (fd < 0) {
    capsule_log("Could not re-open pipe handle");
    exit(1);
  }
  capsule_log("Re-opened named pipe");

  io->fifo_r = fd;
  io->fifo_w = fd;
#else
  io->fifo_r = open_fifo(*io->fifo_r_path, "reading", O_RDONLY);
  io->fifo_w = open_fifo(*io->fifo_w_path, "writing", O_WRONLY);
#endif
  capsule_log("communication established!");
}

int capsule_io_receive_video_format (
    capsule_io_t *io,
    video_format_t *vfmt
) {
  capsule_log("receiving video format...");
  char *buf = capsule_read_packet(io->fifo_r);
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

  io->mapped = (char*) MapViewOfFile(
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

  io->mapped = (char *) mmap(
      nullptr, // addr
      shmem->size(), // length
      PROT_READ, // prot
      MAP_SHARED, // flags
      shmem_handle, // fd
      0 // offset
  );
#endif

  if (!io->mapped) {
    capsule_log("Could not map shmem area");
    exit(1);
  }

  delete[] buf;
  return 0;
}

int capsule_io_receive_video_frame (capsule_io_t *io, uint8_t *buffer, size_t buffer_size, int64_t *timestamp) {
  char *buf = capsule_read_packet(io->fifo_r);

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
  void *source = io->mapped + (buffer_size * vfc->index());
  memcpy(buffer, source, buffer_size);

  flatbuffers::FlatBufferBuilder builder(1024);
  auto vfp = CreateVideoFrameProcessed(builder, vfc->index()); 
  auto opkt = CreatePacket(builder, Message_VideoFrameProcessed, vfp.Union());
  builder.Finish(opkt);
  capsule_write_packet(builder, io->fifo_w);

  delete[] buf;
  return buffer_size;
}

