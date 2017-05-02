
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

#include <lab/io.h>
#include <lab/paths.h>
#include <lab/strings.h>

#include "logging.h"

#if defined(LAB_LINUX)
#elif defined(LAB_MACOS)
#else // !(LAB_LINUX || LAB_MACOS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#endif // !(LAB_LINUX || LAB_MACOS)

namespace capsule {

Connection::Connection(std::string pipe_name) {
  pipe_name_ = pipe_name;
  // sic - swapped on purpose!
  // we read from what capsulerun-writes and vice versa.
  r_path_ = lab::paths::PipePath(pipe_name + ".runwrite");
  w_path_ = lab::paths::PipePath(pipe_name + ".runread");
}

enum OpenMode {
  kOpenRead,
  kOpenWrite,
};

#if defined(LAB_WINDOWS)
static HANDLE OpenPipe(std::string path, OpenMode mode) {
  auto wide_path = lab::strings::ToWide(path);
  auto access = mode == kOpenRead ? GENERIC_READ : GENERIC_WRITE;
  return CreateFileW(wide_path.c_str(),    /* lpFileName */
                     access,               /* dwDesiredAccess */
                     0,                    /* dwShareMode */
                     nullptr,              /* lpSecurityAttributes */
                     OPEN_EXISTING,        /* dwCreationDisposition */
                     FILE_FLAG_OVERLAPPED, /* dwFlagsAndAttributes */
                     nullptr               /* hTemplateFile */
                     );
}
#endif // LAB_WINDOWS

void Connection::Connect() {
  // opening order is important
#if defined(LAB_WINDOWS)
  Log("Opening write end...");
  pipe_w_ = OpenPipe(w_path_, kOpenWrite);
  if (pipe_w_ == INVALID_HANDLE_VALUE) {
    Log("Could not connect write end, bailing out...");
    return;
  }
  Log("Write end opened!");
  Log("Opening read end...");
  pipe_r_ = OpenPipe(r_path_, kOpenRead);
  if (pipe_r_ == INVALID_HANDLE_VALUE) {
    Log("Could not connect read end, bailing out...");
    return;
  }
  Log("Read end opened!");
#else // LAB_WINDOWS
  fifo_w_ = lab::io::Fopen(w_path_, "wb");
  if (!fifo_w_) {
    Log("Could not connect write end, bailing out...");
    return;
  }
  fifo_r_ = lab::io::Fopen(r_path_, "rb");
  if (!fifo_r_) {
    Log("Could not connect write end, bailing out...");
    return;
  }
#endif

  connected_ = true;
}

void Connection::Close() {
#if defined(LAB_WINDOWS)
  CloseHandle(pipe_w_);
  CloseHandle(pipe_r_);
#else // LAB_WINDOWS
  fclose(fifo_w_);
  fclose(fifo_r_);
#endif // !LAB_WINDOWS
  connected_ = false;
}

#if defined(LAB_WINDOWS)
struct OutgoingMessage {
  OVERLAPPED overlapped;
  char *buffer;
};
#endif // LAB_WINDOWS

#if defined(LAB_WINDOWS)
static void WINAPI WriteComplete(DWORD dwErrorCode,
                                 DWORD dwNumberOfBytesTransfered,
                                 OVERLAPPED *overlapped) {
  Log("Write finished!");
  auto message = reinterpret_cast<OutgoingMessage*>(overlapped);
  delete[] message->buffer;
  delete message;
}
#endif // LAB_WINDOWS

void Connection::Write(const flatbuffers::FlatBufferBuilder &builder) {
  if (!connected_) {
    return;
  }

#if defined(LAB_WINDOWS)
  uint32_t pkt_size = builder.GetSize();
  // TODO: re-use buffers instead
  auto total_size = sizeof(uint32_t) + pkt_size;
  auto buffer = new char[total_size];
  memcpy(buffer, &pkt_size, sizeof(uint32_t));
  memcpy(buffer + sizeof(uint32_t), builder.GetBufferPointer(), pkt_size);

  auto message = new OutgoingMessage{};
  message->buffer = buffer;

  // TODO: check bool return
  WriteFileEx(pipe_w_,               /* hFile */
              message,               /* lpBuffer */
              total_size,            /* nNumberOfBytesToWrite */
              (LPOVERLAPPED)message, /* lpOverlapped */
              WriteComplete          /* lpCompletionRoutine */
              );
#else // LAB_WINDOWS
  lab::packet::Fwrite(builder, fifo_w_);
#endif // !LAB_WINDOWS
}

#if defined(LAB_WINDOWS)
static void WINAPI ReadComplete(DWORD dwErrorCode,
                                DWORD dwNumberOfBytesTransfered,
                                OVERLAPPED *overlapped) {
  Log("Read finished");
}
#endif // LAB_WINDOWS

char *Connection::Read() {
  if (!connected_) {
    return nullptr;
  }

#if defined(LAB_WINDOWS)
  uint32_t msg_size = 0;
  OVERLAPPED overlapped;
  ZeroMemory(&overlapped, sizeof(overlapped));

  // TODO: error checking
  ReadFileEx(pipe_r_, /* hFile */
             (char*) &msg_size, /* lPBuffer */
             sizeof(msg_size) , /* nNumberOfBytesToRead */
             &overlapped,                 /* lpOverlapped */
             ReadComplete                 /* lpCompletionRoutine */);

  // TODO: error checking
  WaitForSingleObjectEx(pipe_r_, INFINITE, true);

  Log("Will read message of %d bytes", msg_size);

  char *buf = new char[msg_size];

  // TODO: error checking
  ReadFileEx(pipe_r_, /* hFile */
             buf, /* lPBuffer */
             msg_size , /* nNumberOfBytesToRead */
             &overlapped,                 /* lpOverlapped */
             ReadComplete                 /* lpCompletionRoutine */);

  WaitForSingleObjectEx(pipe_r_, INFINITE, true);
  Log("Just read message of %d bytes, addr = %P", msg_size, buf);

  return buf;
#else // LAB_WINDOWS
  return lab::packet::Fread(fifo_r_);
#endif // !LAB_WINDOWS
}

} // namespace capsule
