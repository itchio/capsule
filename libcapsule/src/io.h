
#pragma once

namespace capsule {
namespace io {

void Init();
void WriteVideoFormat(int width, int height, int format, bool vflip, int64_t pitch);
void WriteVideoFrame(int64_t timestamp, char *frame_data, size_t frame_data_size);
void WriteHotkeyPressed();

} // namespace io
} // namespace capsule
