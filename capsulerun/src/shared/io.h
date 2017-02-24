#pragma once

#include <string>
#include <capsule/constants.h>
#include <capsulerun_types.h>

typedef struct capsule_io {
    std::string *fifo_r_path;
    std::string *fifo_w_path;
    FILE *fifo_r;
    FILE *fifo_w;
    char *mapped;
} capsule_io_t;

void capsule_io_init(
    capsule_io_t *io,
    std::string &fifo_r_path,
    std::string &fifo_w_path
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

