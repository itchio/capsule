
#pragma once

// this little trick is required because apparently
// windows.h defines 'min', and flatbuffers.h uses std::min,
// and they don't play well together. This lets us include
// messages.h even if windows.h was already included.
#if defined(min)
#define capsule__hide__min min
#undef min
#endif

#include "messages_generated.h"

#if defined(capsule__hide__min)
#define min capsule__hide__min
#undef capsule__hide__min
#endif

#include <capsule/constants.h>

#if defined(CAPSULE_WINDOWS)
#include <io.h>
#define LEAN_AND_MEAN
#include <windows.h>
#undef LEAN_AND_MEAN
#else
#include <unistd.h>
#endif // CAPSULE_WINDOWS

/**
 * Read a packet-full of bytes from *file.
 * The returned char* must be delete[]'d.
 *
 * Blocks, returns null on closed pipe
 */
static char *capsule_fread_packet (FILE *file) {
    uint32_t pkt_size = 0;
    int read_bytes = fread(&pkt_size, sizeof(pkt_size), 1, file);
    if (read_bytes == 0) {
        // closed pipe
        return nullptr;
    }

    char *buffer = new char[pkt_size];
    fread(buffer, pkt_size, 1, file);
    return buffer;
}

/**
 * Writes a packet (built with builder) to file.
 * builder.Finish(x) must have been called beforehand.
 */
static void capsule_fwrite_packet(const flatbuffers::FlatBufferBuilder &builder, FILE *file) {
    // capsule_log("writing packet, size: %d bytes", builder.GetSize());
    uint32_t pkt_size = builder.GetSize();
    fwrite(&pkt_size, sizeof(pkt_size), 1, file);
    fwrite(builder.GetBufferPointer(), builder.GetSize(), 1, file);
    fflush(file);
}

#if defined(CAPSULE_WINDOWS)

/**
 * Read a packet-full of bytes from *file.
 * The returned char* must be delete[]'d.
 *
 * Blocks, returns null on closed pipe
 */
static char *capsule_hread_packet (HANDLE handle) {
    uint32_t pkt_size = 0;

    DWORD bytes_read = 0;

    BOOL success = ReadFile(
        handle,
        &pkt_size,
        sizeof(pkt_size),
        &bytes_read,
        0
    );
    if (!success || bytes_read == 0) {
        return nullptr;
    }
    // fprintf(stdout, "reading %d bytes\n", pkt_size);
    // fflush(stdout);

    char *buffer = new char[pkt_size];
    success = ReadFile(
        handle,
        buffer,
        pkt_size,
        &bytes_read,
        0
    );

    auto pkt = Capsule::Messages::GetPacket(buffer);
    // fprintf(stdout, "read %d bytes packet, of type %s\n", pkt_size, Capsule::Messages::EnumNameMessage(pkt->message_type()));
    // fflush(stdout);

    return buffer;
}

/**
 * Writes a packet (built with builder) to file.
 * builder.Finish(x) must have been called beforehand.
 */
static void capsule_hwrite_packet(const flatbuffers::FlatBufferBuilder &builder, HANDLE handle) {
    DWORD bytes_written;

    // fprintf(stdout, "writing packet, size: %d bytes\n", builder.GetSize());
    // fflush(stdout);
    uint32_t pkt_size = builder.GetSize();
    WriteFile(
        handle,
        &pkt_size,
        sizeof(pkt_size),
        &bytes_written,
        0
    );

    WriteFile(
        handle,
        builder.GetBufferPointer(),
        builder.GetSize(),
        &bytes_written,
        0
    );
    FlushFileBuffers(handle);
}

#else // CAPSULE_WINDOWS

/**
 * Read a packet-full of bytes from fd.
 * The returned char* must be delete[]'d.
 *
 * Blocks, returns null on closed pipe
 */
static char *capsule_read_packet (int fd) {
    uint32_t pkt_size = 0;
    int read_bytes = read(fd, &pkt_size, sizeof(pkt_size));
    if (read_bytes == 0) {
        // closed pipe
        return nullptr;
    }

    char *buffer = new char[pkt_size];
    read(fd, buffer, pkt_size);
    return buffer;
}

/**
 * Writes a packet (built with builder) to fd.
 * builder.Finish(x) must have been called beforehand.
 */
static void capsule_write_packet(const flatbuffers::FlatBufferBuilder &builder, int fd) {
    uint32_t pkt_size = builder.GetSize();
    write(fd, &pkt_size, sizeof(pkt_size));
    write(fd, builder.GetBufferPointer(), builder.GetSize());
}

#endif // !CAPSULE_WINDOWS
