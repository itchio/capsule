
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
#else // !(LAB_LINUX || LAB_MACOS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN

#include <io.h>
#endif // !(LAB_WINDOWS || LAB_MACOS)

#include <lab/paths.h>

#include "logging.h"

namespace capsule {

#if defined(LAB_WINDOWS)

static HANDLE CreatePipe(
  std::string pipe_path,
  DWORD pipe_mode
) {
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

  return handle;
}

#else // LAB_WINDOWS

static int CreateFifo (
    std::string fifo_path
) {
  // remove previous fifo if any  
  unlink(fifo_path.c_str());

  // create fifo
  return mkfifo(fifo_path.c_str(), 0644);
}

static int OpenFifo (
  std::string path,
  int flags
) {
  // CapsuleLog("opening %s fifo...", purpose.c_str());
  return open(path.c_str(), flags);
}

#endif // !LAB_WINDOWS

Connection::Connection(std::string pipe_name) {
  pipe_name_ = pipe_name;
  r_path_ = lab::paths::PipePath(pipe_name + ".runread");
  w_path_ = lab::paths::PipePath(pipe_name + ".runwrite");

#if defined(LAB_WINDOWS)
  pipe_r_ = CreatePipe(r_path_, PIPE_ACCESS_INBOUND);
  if (!pipe_r_) {
    Log("Could not create read pipe, bailing out...");
    exit(1);
  }
  pipe_w_ = CreatePipe(w_path_, PIPE_ACCESS_OUTBOUND);
  if (!pipe_w_) {
    Log("Could not create write pipe, bailing out...");
    exit(1);
  }
#else // LAB_WINDOWS
  // ignore SIGPIPE - those will get disconnected when
  // the game shuts down, and that's okay.
  signal(SIGPIPE, SIG_IGN);

  int ret;
  ret = CreateFifo(r_path_);
  if (ret != 0) {
    Log("Could not create read fifo at %s (code %d)", r_path_.c_str(), ret);
    exit(1);
  }
  ret = CreateFifo(w_path_);
  if (ret != 0) {
    Log("Could not create write fifo at %s (code %d)", w_path_.c_str(), ret);
    exit(1);
  }
#endif // !LAB_WINDOWS
}

// TODO: error reporting
void Connection::Connect() {
#if defined(LAB_WINDOWS)
  BOOL success = false;
  success = ConnectNamedPipe(pipe_r_, NULL);
  if (!success) {
    return;
  }

  success = ConnectNamedPipe(pipe_w_, NULL);
  if (!success) {
    return;
  }
#else // LAB_WINDOWS
  fifo_r_ = OpenFifo(r_path_, O_RDONLY);
  if (!fifo_r_) {
    return;
  }

  fifo_w_ = OpenFifo(w_path_, O_WRONLY);
  if (!fifo_w_) {
    return;
  }
#endif // !LAB_WINDOWS

  connected_ = true;
}

void Connection::Close() {
  connected_ = false;
#if defined(LAB_WINDOWS)
  CloseHandle(pipe_r_);
  CloseHandle(pipe_w_);
#else
  close(fifo_r_);
  close(fifo_w_);
#endif
}

void Connection::Write(const flatbuffers::FlatBufferBuilder &builder) {
  if (!connected_) {
    return;
  }

#if defined(LAB_WINDOWS)
  lab::packet::Hwrite(builder, pipe_w_);
#else // LAB_WINDOWS
  lab::packet::Write(builder, fifo_w_);
#endif // !LAB_WINDOWS
}

char *Connection::Read() {
  if (!connected_) {
    return nullptr;
  }

  char *result;

#if defined(LAB_WINDOWS)
  result = lab::packet::Hread(pipe_r_);
#else // LAB_WINDOWS
  result = lab::packet::Read(fifo_r_);
#endif // !LAB_WINDOWS

  if (!result) {
    connected_ = false;
  }

  return result;
}

} // namespace capsule
