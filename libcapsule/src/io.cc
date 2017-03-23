
#include <string>
#include <thread>
#include <mutex>

#include <shoom.h>

#include <lab/packet.h>
#include <lab/logging.h>
#include <lab/env.h>
#include <lab/paths.h>
#include <lab/io.h>

#include "capsule/messages_generated.h"
#include "capsule/constants.h"
#include "capture.h"
#include "logging.h"
#include "ensure.h"

namespace capsule {
namespace io {

static FILE *out_file = 0;
static FILE *in_file = 0;

bool frame_locked[kNumBuffers];
std::mutex frame_locked_mutex;
int next_frame_index = 0;

shoom::Shm *shm;

static bool IsFrameLocked(int i) {
    std::lock_guard<std::mutex> lock(frame_locked_mutex);
    return frame_locked[i];
}

static void LockFrame(int i) {
    std::lock_guard<std::mutex> lock(frame_locked_mutex);
    frame_locked[i] = true;
}

static void UnlockFrame(int i) {
    std::lock_guard<std::mutex> lock(frame_locked_mutex);
    frame_locked[i] = false;
}

static void PollInfile() {
    while (true) {
        char *buf = lab::packet::Fread(in_file);
        if (!buf) {
            break;
        }

        auto pkt = messages::GetPacket(buf);
        switch (pkt->message_type()) {
            case messages::Message_CaptureStart: {
                auto cps = static_cast<const messages::CaptureStart *>(pkt->message());
                Log("poll_infile: received CaptureStart");
                capture::Settings settings;
                settings.fps = cps->fps();
                settings.size_divider = cps->size_divider();
                settings.gpu_color_conv = cps->gpu_color_conv();
                Log("poll_infile: capture settings: %d fps, %d divider, %d gpu_color_conv", settings.fps, settings.size_divider, settings.gpu_color_conv);
                capture::Start(&settings);
                break;
            }
            case messages::Message_CaptureStop: {
                Log("poll_infile: received CaptureStop");
                capture::Stop();
                break;
            }
            case messages::Message_VideoFrameProcessed: {
                auto vfp = static_cast<const messages::VideoFrameProcessed *>(pkt->message());
                UnlockFrame(vfp->index());
                break;
            }
            default:
                Log("poll_infile: unknown message type %s", EnumNameMessage(pkt->message_type()));
        }

        delete[] buf;
    }
}

void WriteVideoFormat(int width, int height, int format, bool vflip, int64_t pitch) {
    Log("writing video format");
    for (int i = 0; i < kNumBuffers; i++) {
        frame_locked[i] = false;
    }

    flatbuffers::FlatBufferBuilder builder(1024);

    int64_t frame_size = height * pitch;
    Log("Frame size: %" PRId64 " bytes", frame_size);
    int64_t shmem_size = frame_size * kNumBuffers;
    Log("Should allocate %" PRId64 " bytes of shmem area", shmem_size);

    std::string shmem_path = "capsule.shm";
    shm = new shoom::Shm(shmem_path, static_cast<size_t>(shmem_size));
    int ret = shm->Create();
    if (ret != shoom::kOK) {
        Log("Could not create shared memory area: code %d", ret);
    }

    auto shmem_path_fbs = builder.CreateString(shmem_path);

    messages::ShmemBuilder shmem_builder(builder);
    shmem_builder.add_path(shmem_path_fbs);
    shmem_builder.add_size(shmem_size);
    auto shmem = shmem_builder.Finish();

    // TODO: support multiple linesizes (for planar formats)
    int64_t linesize[1];
    linesize[0] = pitch;
    auto linesize_vec = builder.CreateVector(linesize, 1);

    // TODO: support multiple offsets (for planar formats)
    int64_t offset[1];
    offset[0] = 0;
    auto offset_vec = builder.CreateVector(offset, 1);

    messages::VideoSetupBuilder vs_builder(builder);
    vs_builder.add_width(width);
    vs_builder.add_height(height);
    vs_builder.add_pix_fmt((messages::PixFmt) format);
    vs_builder.add_vflip(vflip);

    vs_builder.add_offset(offset_vec);
    vs_builder.add_linesize(linesize_vec);

    vs_builder.add_shmem(shmem);
    auto vs = vs_builder.Finish();

    messages::PacketBuilder pkt_builder(builder);
    pkt_builder.add_message_type(messages::Message_VideoSetup);
    pkt_builder.add_message(vs.Union());
    auto pkt = pkt_builder.Finish();

    builder.Finish(pkt);
    lab::packet::Fwrite(builder, out_file);
}

int is_skipping;

void WriteVideoFrame(int64_t timestamp, char *frame_data, size_t frame_data_size) {
    if (IsFrameLocked(next_frame_index)) {
        if (!is_skipping) {
            Log("frame buffer overrun (at %d)! skipping until further notice", next_frame_index);
            is_skipping = true;
        }
        return;
    }
    is_skipping = false;

    flatbuffers::FlatBufferBuilder builder(1024);

    int64_t offset = (frame_data_size * next_frame_index);
    char *target = reinterpret_cast<char*>(shm->Data() + offset);
    memcpy(target, frame_data, frame_data_size);

    messages::VideoFrameCommittedBuilder vfc_builder(builder);
    vfc_builder.add_timestamp(timestamp);
    vfc_builder.add_index(next_frame_index);
    auto vfc = vfc_builder.Finish();

    messages::PacketBuilder pkt_builder(builder);
    pkt_builder.add_message_type(messages::Message_VideoFrameCommitted);
    pkt_builder.add_message(vfc.Union());
    auto pkt = pkt_builder.Finish();

    builder.Finish(pkt);

    LockFrame(next_frame_index);
    lab::packet::Fwrite(builder, out_file);

    next_frame_index = (next_frame_index + 1) % kNumBuffers;
}

void WriteHotkeyPressed() {
    flatbuffers::FlatBufferBuilder builder(1024);

    messages::HotkeyPressedBuilder hkp_builder(builder);
    auto hkp = hkp_builder.Finish();

    messages::PacketBuilder pkt_builder(builder);
    pkt_builder.add_message_type(messages::Message_HotkeyPressed);
    pkt_builder.add_message(hkp.Union());
    auto pkt = pkt_builder.Finish();

    builder.Finish(pkt);

    lab::packet::Fwrite(builder, out_file);
}

void Init() {
    Log("Our pipe path is %s", lab::env::Get("CAPSULE_PIPE_PATH").c_str());

    {
        Log("Opening write channel...");
        std::string w_path = lab::env::Get("CAPSULE_PIPE_PATH") + ".runread";
        out_file = lab::io::Fopen(lab::paths::PipePath(w_path), "wb");
        Ensure("Opened output file", !!out_file);
    }

    {
        Log("Opening read channel...");
        std::string r_path = lab::env::Get("CAPSULE_PIPE_PATH") + ".runwrite";
        in_file = lab::io::Fopen(lab::paths::PipePath(r_path), "rb");
        Ensure("Opened input file", !!in_file);
    }

    std::thread poll_thread(PollInfile);
    poll_thread.detach();
    Log("Connection with capsulerun established!");
}

} // namespace io
} // namespace capsule
