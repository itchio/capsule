
/*
 *  capsule - the game recording and overlay toolkit
 *  Copyright (C) 2017, Amos Wenger
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details:
 * https://github.com/itchio/capsule/blob/master/LICENSE
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

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
#include "capture.h"
#include "logging.h"
#include "ensure.h"

namespace capsule {
namespace io {

static FILE *out_file = 0;
static FILE *in_file = 0;

bool frame_locked[capture::kNumBuffers];
std::mutex frame_locked_mutex;
int next_frame_index = 0;

shoom::Shm *shm;
shoom::Shm *audio_shm;
int64_t audio_frame_size;
int64_t audio_shm_committed_offset;
int64_t audio_shm_processed_offset;

std::mutex out_mutex;

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
                auto cps = pkt->message_as_CaptureStart();
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
                if (shm) {
                    delete shm;
                    shm = nullptr;
                }
                if (audio_shm) {
                    delete audio_shm;
                    audio_shm = nullptr;
                }
                break;
            }
            case messages::Message_VideoFrameProcessed: {
                auto vfp = pkt->message_as_VideoFrameProcessed();
                UnlockFrame(vfp->index());
                break;
            }
            case messages::Message_AudioFramesProcessed: {
                auto afp = pkt->message_as_AudioFramesProcessed();
                Log("AudioFramesProcessed: offset %d frames %d", afp->offset(), afp->frames());
                break;
            }
            default:
                Log("poll_infile: unknown message type %s", EnumNameMessage(pkt->message_type()));
        }

        delete[] buf;
    }
}

void WriteVideoFormat(int width, int height, int format, bool vflip, int64_t pitch) {
    flatbuffers::FlatBufferBuilder builder(1024);

    Log("writing video format");
    auto state = capture::GetState();

    flatbuffers::Offset<messages::AudioSetup> audio_setup;
    if (state->has_audio_intercept) {
        Log("has audio intercept!");

        // let's say we want a 4 second buffer
        int64_t seconds = 4;
        // assuming F32LE
        Ensure("audio intercept format is F32LE", state->audio_intercept_format == messages::SampleFmt_F32LE);
        int64_t sample_size = 4;

        audio_frame_size = sample_size * (int64_t) state->audio_intercept_channels;
        int64_t audio_shmem_size = seconds * audio_frame_size * (int64_t) state->audio_intercept_rate;
        std::string audio_shmem_path = "capsule_audio.shm";
        audio_shm = new shoom::Shm(audio_shmem_path, static_cast<size_t>(audio_shmem_size));
        audio_shm_committed_offset = 0;
        audio_shm_processed_offset = (size_t) audio_shmem_size;

        int ret = audio_shm->Create();
        if (ret != shoom::kOK) {
            Log("Could not create audio shared memory area: code %d", ret);
        }

        auto audio_shmem = messages::CreateShmem(
            builder,
            builder.CreateString(audio_shmem_path),
            audio_shmem_size
        );

        audio_setup = messages::CreateAudioSetup(
            builder,
            state->audio_intercept_channels,
            state->audio_intercept_format,
            state->audio_intercept_rate,
            audio_shmem
        );
    }

    for (int i = 0; i < capture::kNumBuffers; i++) {
        frame_locked[i] = false;
    }

    int64_t frame_size = height * pitch;
    Log("Frame size: %" PRId64 " bytes", frame_size);
    int64_t shmem_size = frame_size * capture::kNumBuffers;
    Log("Should allocate %" PRId64 " bytes of shmem area", shmem_size);

    std::string shmem_path = "capsule_video.shm";
    shm = new shoom::Shm(shmem_path, static_cast<size_t>(shmem_size));
    int ret = shm->Create();
    if (ret != shoom::kOK) {
        Log("Could not create video shared memory area: code %d", ret);
    }

    auto shmem = messages::CreateShmem(
        builder,
        builder.CreateString(shmem_path),
        shmem_size
    );

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

    vs_builder.add_audio(audio_setup);
    auto vs = vs_builder.Finish();

    messages::PacketBuilder pkt_builder(builder);
    pkt_builder.add_message_type(messages::Message_VideoSetup);
    pkt_builder.add_message(vs.Union());
    auto pkt = pkt_builder.Finish();

    builder.Finish(pkt);
    {
        std::lock_guard<std::mutex> lock(out_mutex);
        lab::packet::Fwrite(builder, out_file);
    }
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
    {
        std::lock_guard<std::mutex> lock(out_mutex);
        lab::packet::Fwrite(builder, out_file);
    }

    next_frame_index = (next_frame_index + 1) % capture::kNumBuffers;
}

void WriteAudioFrames(char *data, size_t frames) {
    if (!audio_shm) {
        Log("WriteAudioFrames called but no audio_shm, ignoring");
        return;
    }
 
    int64_t remain_frames = frames;
    int64_t offset = 0;
    while (remain_frames > 0) {
        if (audio_shm_committed_offset == (int64_t) audio_shm->Size()) {
            Log("io: Wrapping!");
            // wrap around!
            audio_shm_committed_offset = 0;
        }

        int64_t write_frames = remain_frames;
        int64_t avail_frames = (audio_shm->Size() - audio_shm_committed_offset) / audio_frame_size;

        if (write_frames > avail_frames) {
            write_frames = avail_frames;
        }

        {
            // Log("putting %" PRIdS " frames at %d", frames, audio_shm_committed_offset);
            char *src = data + (offset * audio_frame_size);
            char *dst = (char*) audio_shm->Data() + audio_shm_committed_offset;
            int64_t copy_size = write_frames * audio_frame_size;
            memcpy(dst, src, copy_size);

            flatbuffers::FlatBufferBuilder builder(64);
            auto afc = messages::CreateAudioFramesCommitted(
                builder,
                audio_shm_committed_offset / audio_frame_size,
                write_frames
            );

            auto pkt = messages::CreatePacket(
                builder,
                messages::Message_AudioFramesCommitted,
                afc.Union()
            );

            builder.Finish(pkt);
            {
                std::lock_guard<std::mutex> lock(out_mutex);
                lab::packet::Fwrite(builder, out_file);
            }

            audio_shm_committed_offset += copy_size;
        }

        remain_frames -= write_frames;
        offset += write_frames;
    }
}

void WriteHotkeyPressed() {
    flatbuffers::FlatBufferBuilder builder(32);

    auto hkp = messages::CreateHotkeyPressed(builder);
    auto pkt = messages::CreatePacket(
        builder,
        messages::Message_HotkeyPressed,
        hkp.Union()
    );

    builder.Finish(pkt);

    {
        std::lock_guard<std::mutex> lock(out_mutex);
        lab::packet::Fwrite(builder, out_file);
    }
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
