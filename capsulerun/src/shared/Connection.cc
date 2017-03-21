#include "Connection.h"

#if defined(CAPSULE_LINUX)
#include <sys/mman.h>
#include <sys/stat.h> // for mode constants
#include <fcntl.h>    // for O_* constants
#include <unistd.h>   // unlink
#include <signal.h>   // signal, SIGPIPE
#elif defined(CAPSULE_MACOS)
#include <sys/mman.h>
#include <sys/stat.h> // for mode constants
#include <fcntl.h>    // for O_* constants
#include <errno.h>    // for errno
#include <unistd.h>   // unlink
#include <signal.h>   // signal, SIGPIPE
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN

#include <io.h>
#endif

#include <stdexcept>

using namespace std;

#if defined(CAPSULE_WINDOWS)

static HANDLE create_pipe(
  std::string &pipe_path,
  DWORD pipe_mode
) {
  // CapsuleLog("Created named pipe %s", pipe_path.c_str());
  HANDLE handle = CreateNamedPipeA(
    pipe_path.c_str(),
    pipe_mode, // open mode
    PIPE_TYPE_BYTE | PIPE_WAIT, // pipe mode
    1, // max instances
    16 * 1024 * 1024, // nOutBufferSize
    16 * 1024 * 1024, // nInBufferSize
    0, // nDefaultTimeout
    NULL // lpSecurityAttributes
  );

  if (!handle) {
    DWORD err = GetLastError();
    // CapsuleLog("CreateNamedPipe failed, err = %d (%x)", err, err);
    throw runtime_error("CreateNamedPipe failed");
  }

  return handle;
}

#else // CAPSULE_WINDOWS

static void create_fifo(
    std::string &fifo_path
) {
  // remove previous fifo if any  
  unlink(fifo_path.c_str());

  // create fifo
  int fifo_ret = mkfifo(fifo_path.c_str(), 0644);
  if (fifo_ret != 0) {
    // CapsuleLog("could not create fifo at %s (code %d)", fifo_path.c_str(), fifo_ret);
    throw runtime_error("mkfifo failed");
  }
}

static int open_fifo (
  std::string &path,
  std::string purpose,
  int flags
) {
  // CapsuleLog("opening %s fifo...", purpose.c_str());

  int fd = open(path.c_str(), flags);
  if (!fd) {
    // CapsuleLog("could not open fifo for %s: %s", purpose.c_str(), strerror(errno));
    throw runtime_error("could not open fifo");
  }

  return fd;
}

#endif // !CAPSULE_WINDOWS

Connection::Connection(std::string &r_path_in, std::string &w_path_in) {
  r_path = &r_path_in;
  w_path = &w_path_in;

#if defined(CAPSULE_WINDOWS)
  pipe_r = create_pipe(*r_path, PIPE_ACCESS_INBOUND);
  pipe_w = create_pipe(*w_path, PIPE_ACCESS_OUTBOUND);
#else // CAPSULE_WINDOWS
  // ignore SIGPIPE - those will get disconnected when
  // the game shuts down, and that's okay.
  signal(SIGPIPE, SIG_IGN);

  create_fifo(*r_path);
  create_fifo(*w_path);
#endif // !CAPSULE_WINDOWS
}

void Connection::connect() {
#if defined(CAPSULE_WINDOWS)
  BOOL success = false;
  success = ConnectNamedPipe(pipe_r, NULL);
  if (!success) {
    throw runtime_error("Could not connect to named pipe for reading");
  }

  success = ConnectNamedPipe(pipe_w, NULL);
  if (!success) {
    throw runtime_error("Could not connect to named pipe for writing");
  }
#else // CAPSULE_WINDOWS
  fifo_r = open_fifo(*r_path, "reading", O_RDONLY);
  fifo_w = open_fifo(*w_path, "writing", O_WRONLY);
#endif // !CAPSULE_WINDOWS
}

void Connection::write(const flatbuffers::FlatBufferBuilder &builder) {
#if defined(CAPSULE_WINDOWS)
  CapsuleHwritePacket(builder, pipe_w);
#else // CAPSULE_WINDOWS
  CapsuleWritePacket(builder, fifo_w);
#endif // !CAPSULE_WINDOWS
}

char *Connection::read() {
#if defined(CAPSULE_WINDOWS)
  return CapsuleHreadPacket(pipe_r);
#else // CAPSULE_WINDOWS
  return CapsuleReadPacket(fifo_r);
#endif // !CAPSULE_WINDOWS
}

void Connection::printDetails() {
  fprintf(stderr, "<CAPS> {\"r_path\": \"%s\", \"w_path\": \"%s\"}\n",
          w_path->c_str(), r_path->c_str());
}
