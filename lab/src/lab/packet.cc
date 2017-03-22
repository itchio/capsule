
#include "packet.h"

namespace lab {
namespace packet {

char *Fread(FILE *file) {
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

void Fwrite(const flatbuffers::FlatBufferBuilder &builder, FILE *file) {
    // CapsuleLog("writing packet, size: %d bytes", builder.GetSize());
    uint32_t pkt_size = builder.GetSize();
    fwrite(&pkt_size, sizeof(pkt_size), 1, file);
    fwrite(builder.GetBufferPointer(), builder.GetSize(), 1, file);
    fflush(file);
}

#if defined(LAB_WINDOWS)

char *Hread(HANDLE handle) {
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

    return buffer;
}

void Hwrite(const flatbuffers::FlatBufferBuilder &builder, HANDLE handle) {
    DWORD bytes_written;

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

#else // LAB_WINDOWS

char *Read(int fd) {
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

void Write(const flatbuffers::FlatBufferBuilder &builder, int fd) {
    uint32_t pkt_size = builder.GetSize();
    write(fd, &pkt_size, sizeof(pkt_size));
    write(fd, builder.GetBufferPointer(), builder.GetSize());
}

#endif // !LAB_WINDOWS

} // namespace packet
} // namespace lab
