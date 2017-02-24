
#include <capsule.h>

#if defined(CAPSULE_LINUX)
#include <capsule/messages.h>

#include <string>
using namespace std;

#include <sys/mman.h>
#include <sys/stat.h> // for mode constants
#include <fcntl.h>    // for O_* constants

#include <thread> // for making trouble double
#include <mutex> // for trouble mitigation
#endif // CAPSULE_LINUX

static FILE *out_file;

FILE *ensure_outfile() {
  if (!out_file) {
#if defined(CAPSULE_WINDOWS)
    const int pipe_path_len = CAPSULE_LOG_PATH_SIZE;
    wchar_t *pipe_path = (wchar_t*) malloc(sizeof(wchar_t) * pipe_path_len);
    pipe_path[0] = '\0';
    GetEnvironmentVariable(L"CAPSULE_PIPE_PATH", pipe_path, pipe_path_len);
    capsule_assert("Got pipe path", wcslen(pipe_path) > 0);

    capsule_log("Pipe path: %S", pipe_path);

    out_file = _wfopen(pipe_path, L"wb");
    free(pipe_path);
#else
#if defined(CAPSULE_LINUX)
    char *pipe_path = getenv("CAPSULE_PIPE_W_PATH");
#else
    char *pipe_path = getenv("CAPSULE_PIPE_PATH");
#endif
    capsule_log("Pipe path: %s", pipe_path);
    out_file = fopen(pipe_path, "wb");
#endif
    capsule_assert("Opened output file", !!out_file);
  }

  return out_file;
}

static FILE *in_file;

FILE *ensure_infile() {
  char *pipe_path;

  if (!in_file) {
#if defined(CAPSULE_WINDOWS)
    capsule_assert("ensure_infile on windows: stub", false);
#else
#if defined(CAPSULE_LINUX)
    pipe_path = getenv("CAPSULE_PIPE_R_PATH");
#else
    capsule_assert("ensure_infile on mac: stub", false);
#endif
    capsule_log("Pipe path: %s", pipe_path);
    in_file = fopen(pipe_path, "rb");
#endif
    capsule_assert("Opened input file", !!in_file);
  }

  return in_file;
}

void CAPSULE_STDCALL capsule_io_init () {
    capsule_log("Ensuring outfile..");
    ensure_outfile();
    capsule_log("outfile ensured!");

    capsule_log("Ensuring infile..");
    ensure_infile();
    capsule_log("infile ensured!");
}

#if defined(CAPSULE_LINUX)

bool frame_locked[NUM_BUFFERS];
mutex frame_locked_mutex;
int next_frame_index = 0;

static int shmem_handle;
static char *mapped;

static bool is_frame_locked(int i) {
    lock_guard<mutex> lock(frame_locked_mutex);
    return frame_locked[i];
}

static void lock_frame(int i) {
    lock_guard<mutex> lock(frame_locked_mutex);
    frame_locked[i] = true;
}

static void unlock_frame(int i) {
    lock_guard<mutex> lock(frame_locked_mutex);
    frame_locked[i] = false;
}

static void poll_infile() {
    FILE *file = ensure_infile();

    while (true) {
        char *buf = capsule_read_packet(file);
        if (!buf) {
            break;
        }

        auto pkt = GetPacket(buf);
        switch (pkt->message_type()) {
            case Message_VideoFrameProcessed: {
                auto vfp = static_cast<const VideoFrameProcessed *>(pkt->message());
                capsule_log("poll_infile: encoder processed frame %d", vfp->index());
                unlock_frame(vfp->index());
                break;
            }
            default:
                capsule_log("poll_infile: unknown message type %s", EnumNameMessage(pkt->message_type()));
        }

        delete[] buf;
    }
}

void CAPSULE_STDCALL capsule_write_video_format(int width, int height, int format, int vflip, int pitch) {
    thread poll_thread(poll_infile);
    poll_thread.detach();

    for (int i = 0; i < NUM_BUFFERS; i++) {
        frame_locked[i] = false;
    }

    flatbuffers::FlatBufferBuilder builder(1024);

    size_t frame_size = height * pitch;
    capsule_log("Frame size: %d bytes", frame_size);
    size_t shmem_size = frame_size * NUM_BUFFERS;
    capsule_log("Should allocate %d bytes of shmem area", shmem_size);

    string shmem_path = "/capsule.shm";
    shmem_handle = shm_open(shmem_path.c_str(), O_CREAT|O_RDWR, 0755);
    capsule_assert("Opened shmem area", shmem_handle > 0);

    int ret = ftruncate(shmem_handle, shmem_size);
    capsule_assert("Truncated shmem area", ret == 0);

    mapped = (char *) mmap(
        nullptr, // addr
        shmem_size, // length
        PROT_READ | PROT_WRITE, // prot
        MAP_SHARED, // flags
        shmem_handle, // fd
        0 // offset
    );
    capsule_assert("Mapped shmem area", !!mapped);

    auto shmem_path_fbs = builder.CreateString(shmem_path);

    ShmemBuilder shmem_builder(builder);
    shmem_builder.add_path(shmem_path_fbs);
    shmem_builder.add_size(shmem_size);
    auto shmem = shmem_builder.Finish();

    VideoSetupBuilder vs_builder(builder);
    vs_builder.add_width(width);
    vs_builder.add_height(height);
    vs_builder.add_pix_fmt((PixFmt) format);
    vs_builder.add_vflip(vflip);
    vs_builder.add_pitch(pitch);
    vs_builder.add_shmem(shmem);
    auto vs = vs_builder.Finish();

    PacketBuilder pkt_builder(builder);
    pkt_builder.add_message_type(Message_VideoSetup);
    pkt_builder.add_message(vs.Union());
    auto pkt = pkt_builder.Finish();

    builder.Finish(pkt);
    capsule_write_packet(builder, ensure_outfile());
}
#else
void CAPSULE_STDCALL capsule_write_video_format(int width, int height, int format, int vflip, int pitch) {
  ensure_outfile();
  int64_t num = (int64_t) width;
  fwrite(&num, sizeof(int64_t), 1, out_file);

  num = (int64_t) height;
  fwrite(&num, sizeof(int64_t), 1, out_file);

  num = (int64_t) format;
  fwrite(&num, sizeof(int64_t), 1, out_file);

  num = (int64_t) vflip;
  fwrite(&num, sizeof(int64_t), 1, out_file);

  num = (int64_t) pitch;
  fwrite(&num, sizeof(int64_t), 1, out_file);
}
#endif // CAPSULE_LINUX

#if defined(CAPSULE_LINUX)

void CAPSULE_STDCALL capsule_write_video_frame(int64_t timestamp, char *frame_data, size_t frame_data_size) {
    if (is_frame_locked(next_frame_index)) {
        capsule_log("frame buffer overrun! skipping.");
        return;
    }

    flatbuffers::FlatBufferBuilder builder(1024);

    char *target = mapped + (frame_data_size * next_frame_index);
    memcpy(target, frame_data, frame_data_size);

    VideoFrameCommittedBuilder vfc_builder(builder);
    vfc_builder.add_timestamp(timestamp);
    vfc_builder.add_index(next_frame_index);
    auto vfc = vfc_builder.Finish();

    PacketBuilder pkt_builder(builder);
    pkt_builder.add_message_type(Message_VideoFrameCommitted);
    pkt_builder.add_message(vfc.Union());
    auto pkt = pkt_builder.Finish();

    builder.Finish(pkt);
    capsule_write_packet(builder, ensure_outfile());

    lock_frame(next_frame_index);
    next_frame_index = (next_frame_index + 1) % NUM_BUFFERS;
}
#else
void CAPSULE_STDCALL capsule_write_video_frame(int64_t timestamp, char *frame_data, size_t frame_data_size) {
  ensure_outfile();
  fwrite(&timestamp, 1, sizeof(int64_t), out_file);
  fwrite(frame_data, 1, frame_data_size, out_file);
}
#endif
