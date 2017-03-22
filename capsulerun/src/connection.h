#pragma once

#include <capsule/messages.h>

class Connection {
  public:
    Connection(std::string &r_path, std::string &w_path);
    void Connect();
    void PrintDetails();

    void Write(const flatbuffers::FlatBufferBuilder &builder);
    char *Read();

  private:
    std::string *r_path_;
    std::string *w_path_;
#if defined(LAB_WINDOWS)
    HANDLE pipe_r_;
    HANDLE pipe_w_;
#else // LAB_WINDOWS
    int fifo_r_;
    int fifo_w_;
#endif // !LAB_WINDOWS
};
