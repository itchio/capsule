#pragma once

#include <lab/packet.h>

class Connection {
  public:
    Connection(std::string pipe_name);
    void Connect();

    void Write(const flatbuffers::FlatBufferBuilder &builder);
    char *Read();

    bool IsConnected() { return connected_; };

  private:
    std::string r_path_;
    std::string w_path_;
#if defined(LAB_WINDOWS)
    HANDLE pipe_r_ = INVALID_HANDLE_VALUE;
    HANDLE pipe_w_ = INVALID_HANDLE_VALUE;
#else // LAB_WINDOWS
    int fifo_r_ = 0;
    int fifo_w_ = 0;
#endif // !LAB_WINDOWS

    bool connected_ = false;
};
