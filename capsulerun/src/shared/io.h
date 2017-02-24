#pragma once

#include <string>
#include <capsule.h>

void capsule_create_fifo (std::string fifo_path);
int capsule_receive_video_format (FILE *file, video_format_t *vfmt, char **mapped);
int capsule_receive_video_frame (FILE *file, uint8_t *buffer, size_t buffer_size, int64_t *timestamp, char *mapped);
