
#pragma once

#include "messages_generated.h"

// this contaminates files where this header is 
// included, but that might be a good thing.
using namespace Capsule::Messages;

/**
 * Read a packet-full of bytes from *file.
 * The returned char* must be delete[]'d.
 *
 * Blocks, returns null on closed pipe
 */
static char *capsule_read_packet (FILE *file) {
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
void capsule_write_packet(const flatbuffers::FlatBufferBuilder &builder, FILE *file) {
    // capsule_log("writing packet, size: %d bytes", builder.GetSize());
    uint32_t pkt_size = builder.GetSize();
    fwrite(&pkt_size, sizeof(pkt_size), 1, file);
    fwrite(builder.GetBufferPointer(), builder.GetSize(), 1, file);
    fflush(file);
}
