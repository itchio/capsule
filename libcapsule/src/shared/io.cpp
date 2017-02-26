
#include <capsule/messages.h>
#include <capsule.h>

#include <string>

#if defined(CAPSULE_LINUX) || defined(CAPSULE_OSX)
#include <sys/mman.h>
#include <sys/stat.h> // for mode constants
#include <fcntl.h>    // for O_* constants

#include <unistd.h>
#else // CAPSULE_LINUX || CAPSULE_OSX
#include <io.h>
#include <fcntl.h>
#endif // !(CAPSULE_LINUX || CAPSULE_OSX)

#include <thread> // for making trouble double
#include <mutex> // for trouble mitigation

using namespace std;
using namespace Capsule::Messages;

static FILE *out_file = 0;
static FILE *in_file = 0;

#if defined(CAPSULE_WINDOWS)
static wchar_t *get_pipe_path (wchar_t *var_name) {
  const int pipe_path_len = CAPSULE_LOG_PATH_SIZE;
  wchar_t *pipe_path = (wchar_t*) calloc(pipe_path_len, sizeof(wchar_t));
  pipe_path[0] = '\0';
  GetEnvironmentVariable(var_name, pipe_path, pipe_path_len);
  capsule_assert("Got pipe path", wcslen(pipe_path) > 0);
  return pipe_path;
}
#endif

FILE *ensure_outfile() {
  if (!out_file) {
    capsule_log("ensure_outfile: out_file was nil");
#if defined(CAPSULE_WINDOWS)
    wchar_t *pipe_path = get_pipe_path(L"CAPSULE_PIPE_W_PATH");
    capsule_log("pipe_w path: %S", pipe_path);
    out_file = _wfopen(pipe_path, L"wb");
    free(pipe_path);
#else
    char *pipe_path = getenv("CAPSULE_PIPE_W_PATH");
    capsule_log("pipe_w path: %s", pipe_path);
    out_file = fopen(pipe_path, "wb");
#endif
    capsule_assert("Opened output file", !!out_file);
  }

  capsule_log("ensure_outfile: returning %d", out_file);
  return out_file;
}

FILE *ensure_infile() {
  if (!in_file) {
#if defined(CAPSULE_WINDOWS)
    wchar_t *pipe_path = get_pipe_path(L"CAPSULE_PIPE_R_PATH");
    capsule_log("pipe_r path: %S", pipe_path);
    in_file = _wfopen(pipe_path, L"rb");
    free(pipe_path);
#else
    char *pipe_path = getenv("CAPSULE_PIPE_R_PATH");
    capsule_log("pipe_r path: %s", pipe_path);
    in_file = fopen(pipe_path, "rb");
#endif
    capsule_assert("Opened input file", !!in_file);
  }

  return in_file;
}

bool frame_locked[NUM_BUFFERS];
mutex frame_locked_mutex;
int next_frame_index = 0;

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

#if defined(CAPSULE_WINDOWS)
static void capsule_capture_start () {
    capsule_log("capsule_capture_start: enumerating our options");
    if (capdata.saw_dxgi || capdata.saw_d3d9 || capdata.saw_opengl) {
        // cool, these will initialize on next present/swapbuffers
    } else {
        // try dc capture then
        bool success = dc_capture_init();
        if (!success) {
            capsule_log("Cannot start capture: no capture method available")
            return;
        }
    }
    capsule_log("capsule_capture_start: success!");
}
#else // CAPSULE_WINDOWS
static void capsule_capture_start () {
    capsule_log("non-windows capsule capture: stub!");
}
#endif // !CAPSULE_WINDOWS

static void poll_infile() {
    while (true) {
        char *buf = capsule_fread_packet(in_file);
        if (!buf) {
            break;
        }

        auto pkt = GetPacket(buf);
        switch (pkt->message_type()) {
            case Message_CaptureStart: {
                capsule_log("capture start!");
                capsule_capture_start();
                capsule_log("capture started, moving on!");
                break;
            }
            case Message_CaptureStop: {
                capsule_log("capture stop!");
                break;
            }
            case Message_VideoFrameProcessed: {
                auto vfp = static_cast<const VideoFrameProcessed *>(pkt->message());
                // capsule_log("poll_infile: encoder processed frame %d", vfp->index());
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
    capsule_log("writing video format");
    for (int i = 0; i < NUM_BUFFERS; i++) {
        frame_locked[i] = false;
    }

    flatbuffers::FlatBufferBuilder builder(1024);

    size_t frame_size = height * pitch;
    capsule_log("Frame size: %d bytes", frame_size);
    size_t shmem_size = frame_size * NUM_BUFFERS;
    capsule_log("Should allocate %d bytes of shmem area", shmem_size);

#if defined(CAPSULE_WINDOWS)
    string shmem_path = "capsule.shm";

    capsule_log("Creating file mapping");
    HANDLE hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE, // use paging file
        NULL,                 // default security
        PAGE_READWRITE,       // read/write access
        0,                    // maximum object size (high-order DWORD)
        shmem_size,           // maximum object size (low-order DWORD)
        shmem_path.c_str()    // name of mapping object
    );
    if (hMapFile == NULL) {
        capsule_log("File mapping didn't work");
        DWORD err = GetLastError();
        capsule_log("Could not create shmem area: %d (%x)", err, err);
    }

    capsule_log("Mapping view of file");
    mapped = (char *) MapViewOfFile(
        hMapFile,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        shmem_size
    );
#else
    string shmem_path = "/capsule.shm";

    // shm segments persist across runs, and macOS will refuse
    // to ftruncate an existing shm segment, so to be on the safe
    // side, we unlink it beforehand.
    shm_unlink(shmem_path.c_str());

    int shmem_handle = shm_open(shmem_path.c_str(), O_CREAT|O_RDWR, 0755);
    capsule_assert("Created shmem area", shmem_handle > 0);

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
#endif // CAPSULE_WINDOWS

    capsule_log("Checking if mapping worked");
    capsule_assert("Mapped shmem area", !!mapped);

    capsule_log("Creating flatbuffer");
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
    capsule_log("writing flatbuffer for real");
    capsule_fwrite_packet(builder, out_file);
    capsule_log("done video format");
}

int is_skipping;

void CAPSULE_STDCALL capsule_write_video_frame(int64_t timestamp, char *frame_data, size_t frame_data_size) {
    if (is_frame_locked(next_frame_index)) {
        if (!is_skipping) {
            capsule_log("frame buffer overrun (at %d)! skipping until further notice", next_frame_index);
            is_skipping = true;
        }
        return;
    }
    is_skipping = false;

    flatbuffers::FlatBufferBuilder builder(1024);

    size_t offset = (frame_data_size * next_frame_index);
    char *target = mapped + offset;
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

    lock_frame(next_frame_index);
    capsule_fwrite_packet(builder, out_file);

    next_frame_index = (next_frame_index + 1) % NUM_BUFFERS;
}

void CAPSULE_STDCALL capsule_io_init () {
    capsule_log("Ensuring outfile..");
    ensure_outfile();
    capsule_log("outfile ensured!");

    capsule_log("Ensuring infile..");
    ensure_infile();
    capsule_log("infile ensured!");

    thread poll_thread(poll_infile);
    poll_thread.detach();
    capsule_log("started infile poll thread");
}
