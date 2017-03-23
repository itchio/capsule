
#pragma once

namespace capsule {
namespace io {

void LAB_STDCALL Init();
void LAB_STDCALL WriteVideoFormat(int width, int height, int format, bool vflip, int64_t pitch);
void LAB_STDCALL WriteVideoFrame(int64_t timestamp, char *frame_data, size_t frame_data_size);

} // namespace io
} // namespace capsule