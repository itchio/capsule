#include "connection.h"

#if defined(LAB_LINUX)
#include <sys/mman.h>
#include <sys/stat.h> // for mode constants
#include <fcntl.h>    // for O_* constants
#include <unistd.h>   // unlink
#include <signal.h>   // signal, SIGPIPE
#elif defined(LAB_MACOS)
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

#if defined(LAB_WINDOWS)

static HANDLE CreatePipe(
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
    throw std::runtime_error("CreateNamedPipe failed");
  }

  return handle;
}

#else // LAB_WINDOWS

static void CreateFifo (
    std::string &fifo_path
) {
  // remove previous fifo if any  
  unlink(fifo_path.c_str());

  // create fifo
  int fifo_ret = mkfifo(fifo_path.c_str(), 0644);
  if (fifo_ret != 0) {
    // CapsuleLog("could not create fifo at %s (code %d)", fifo_path.c_str(), fifo_ret);
    throw std::runtime_error("mkfifo failed");
  }
}

static int OpenFifo (
  std::string &path,
  std::string purpose,
  int flags
) {
  // CapsuleLog("opening %s fifo...", purpose.c_str());

  int fd = open(path.c_str(), flags);
  if (!fd) {
    // CapsuleLog("could not open fifo for %s: %s", purpose.c_str(), strerror(errno));
    throw std::runtime_error("could not open fifo");
  }

  return fd;
}

#endif // !LAB_WINDOWS

Connection::Connection(std::string &r_path, std::string &w_path) {
  r_path_ = &r_path;
  w_path_ = &w_path;

#if defined(LAB_WINDOWS)
  pipe_r_ = CreatePipe(*r_path_, PIPE_ACCESS_INBOUND);
  pipe_w_ = CreatePipe(*w_path_, PIPE_ACCESS_OUTBOUND);
#else // LAB_WINDOWS
  // ignore SIGPIPE - those will get disconnected when
  // the game shuts down, and that's okay.
  signal(SIGPIPE, SIG_IGN);

  CreateFifo(*r_path_);
  CreateFifo(*w_path_);
#endif // !LAB_WINDOWS
}

void Connection::Connect() {
#if defined(LAB_WINDOWS)
  BOOL success = false;
  success = ConnectNamedPipe(pipe_r_, NULL);
  if (!success) {
    throw std::runtime_error("Could not connect to named pipe for reading");
  }

  success = ConnectNamedPipe(pipe_w_, NULL);
  if (!success) {
    throw std::runtime_error("Could not connect to named pipe for writing");
  }
#else // LAB_WINDOWS
  fifo_r_ = OpenFifo(*r_path_, "reading", O_RDONLY);
  fifo_w_ = OpenFifo(*w_path_, "writing", O_WRONLY);
#endif // !LAB_WINDOWS
}

void Connection::Write(const flatbuffers::FlatBufferBuilder &builder) {
#if defined(LAB_WINDOWS)
  CapsuleHwritePacket(builder, pipe_w_);
#else // LAB_WINDOWS
  CapsuleWritePacket(builder, fifo_w_);
#endif // !LAB_WINDOWS
}

char *Connection::Read() {
#if defined(LAB_WINDOWS)
  return CapsuleHreadPacket(pipe_r_);
#else // LAB_WINDOWS
  return CapsuleReadPacket(fifo_r_);
#endif // !LAB_WINDOWS
}

void Connection::PrintDetails() {
  fprintf(stderr, "<CAPS> {\"r_path\": \"%s\", \"w_path\": \"%s\"}\n",
          w_path_->c_str(), r_path_->c_str());
}
