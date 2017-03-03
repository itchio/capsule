#pragma once

#include <string>
#include <capsule/constants.h>
#include <capsulerun_types.h>

#if defined(CAPSULE_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#endif

typedef struct capsule_io {
#if defined(CAPSULE_WINDOWS)
    HANDLE pipe_r;
    HANDLE pipe_w;
#else
    std::string *fifo_r_path;
    std::string *fifo_w_path;
    int fifo_r;
    int fifo_w;
#endif
    char *mapped;
} capsule_io_t;

void capsule_io_init(
    capsule_io_t *io,
#if defined(CAPSULE_WINDOWS)
    std::string &pipe_r_path,
    std::string &pipe_w_path
#else
    std::string &fifo_r_path,
    std::string &fifo_w_path
#endif
);

void capsule_io_connect (
    capsule_io_t *io
);

int capsule_io_receive_video_format (
    capsule_io_t *io,
    video_format_t *vfmt
);

int capsule_io_receive_video_frame (
    capsule_io_t *io,
    uint8_t *buffer,
    size_t buffer_size,
    int64_t *timestamp
);

void capsule_io_capture_start(
    capsule_io_t *io
);

#if defined(CAPSULE_WINDOWS)
#define READPKT(io)           capsule_hread_packet((io)->pipe_r)
#define WRITEPKT(builder, io) capsule_hwrite_packet((builder), (io)->pipe_w)
#else // CAPSULE_WINDOWS
#define READPKT(io)           capsule_read_packet((io)->fifo_r)
#define WRITEPKT(builder, io) capsule_write_packet((builder), (io)->fifo_w)
#endif // !CAPSULE_WINDOWS
