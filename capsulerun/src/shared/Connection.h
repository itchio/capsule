#pragma once

#include <capsule/messages.h>

class Connection {
  public:
    Connection(std::string &r_path, std::string &w_path);
    void connect();

    void write(const flatbuffers::FlatBufferBuilder &builder);
    char *read();

  private:
#if defined(CAPSULE_WINDOWS)
    HANDLE pipe_r;
    HANDLE pipe_w;
#else // CAPSULE_WINDOWS
    std::string *fifo_r_path;
    std::string *fifo_w_path;
    int fifo_r;
    int fifo_w;
#endif // !CAPSULE_WINDOWS
};
