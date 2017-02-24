#pragma once

#include <string>
#include <capsule/constants.h>
#include <capsulerun_types.h>

#if defined(CAPSULE_WINDOWS)
#define LEAN_AND_MEAN
#include <windows.h>
#undef LEAN_AND_MEAN
#endif

typedef struct capsule_io {
#if defined(CAPSULE_WINDOWS)
    HANDLE pipe_handle;
    std::string *pipe_path;
#else
    std::string *fifo_r_path;
    std::string *fifo_w_path;
#endif
    int fifo_r;
    int fifo_w;
    char *mapped;
} capsule_io_t;

void capsule_io_init(
    capsule_io_t *io,
#if defined(CAPSULE_WINDOWS)
    std::string &pipe_path
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

