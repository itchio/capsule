
#pragma once

// this little trick is required because apparently
// windows.h defines 'min', and flatbuffers.h uses std::min,
// and they don't play well together. This lets us include
// messages.h even if windows.h was already included.
#if defined(min)
#define capsule__hide__min min
#undef min
#endif

#include "../flatbuffers/flatbuffers.h"

#if defined(capsule__hide__min)
#define min capsule__hide__min
#undef capsule__hide__min
#endif

#include "platform.h"

#if defined(LAB_WINDOWS)
#include <io.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#else
#include <unistd.h>
#endif // LAB_WINDOWS

namespace lab {
namespace packet {

/**
 * Read a packet-full of bytes from *file.
 * The returned char* must be delete[]'d.
 *
 * Blocks, returns null on closed pipe
 */
char *Fread(FILE *file);

/**
 * Writes a packet (built with builder) to file.
 * builder.Finish(x) must have been called beforehand.
 */
void Fwrite(const flatbuffers::FlatBufferBuilder &builder, FILE *file);

#if defined(LAB_WINDOWS)

/**
 * Read a packet-full of bytes from *file.
 * The returned char* must be delete[]'d.
 *
 * Blocks, returns null on closed pipe
 */
char *Hread(HANDLE handle);

/**
 * Writes a packet (built with builder) to file.
 * builder.Finish(x) must have been called beforehand.
 */
void Hwrite(const flatbuffers::FlatBufferBuilder &builder, HANDLE handle);

#else // LAB_WINDOWS

/**
 * Read a packet-full of bytes from fd.
 * The returned char* must be delete[]'d.
 *
 * Blocks, returns null on closed pipe
 */
char *Read(int fd);

/**
 * Writes a packet (built with builder) to fd.
 * builder.Finish(x) must have been called beforehand.
 */
void Write(const flatbuffers::FlatBufferBuilder &builder, int fd);

#endif // !LAB_WINDOWS

} // namespace lab
} // namespace packet
