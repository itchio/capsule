
#pragma once

#include "messages_generated.h"

#include <capsule/constants.h>

#if defined(CAPSULE_WINDOWS)
#include <io.h>
#else
#include <unistd.h>
#endif // CAPSULE_WINDOWS

/**
 * Read a packet-full of bytes from *file.
 * The returned char* must be delete[]'d.
 *
 * Blocks, returns null on closed pipe
 */
static char *capsule_read_packet (int fd) {
    fprintf(stdout, "reading packet\n");
    fflush(stdout);

    uint32_t pkt_size = 0;
    int read_bytes = read(fd, &pkt_size, sizeof(pkt_size));
    if (read_bytes == 0) {
        // closed pipe
        return nullptr;
    }

    fprintf(stdout, "reading packet, size = %d\n", pkt_size);
    fflush(stdout);

    char *buffer = new char[pkt_size];
    read(fd, buffer, pkt_size);
    return buffer;
}

/**
 * Writes a packet (built with builder) to file.
 * builder.Finish(x) must have been called beforehand.
 */
void capsule_write_packet(const flatbuffers::FlatBufferBuilder &builder, int fd) {
    fprintf(stdout, "writing packet, size: %d bytes\n", builder.GetSize());
    fflush(stdout);

    uint32_t pkt_size = builder.GetSize();
    write(fd, &pkt_size, sizeof(pkt_size));
    write(fd, builder.GetBufferPointer(), builder.GetSize());
}
