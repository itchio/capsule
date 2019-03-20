
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

#include "capsule/audio_math.h"
#include "capture.h"
#include "logging.h"
#include "ensure.h"
#include "connection.h"

// #define DebugLog(...) Log(__VA_ARGS__)
#define DebugLog(...)

namespace capsule {
namespace io {

static Connection *connection = nullptr;

bool frame_locked[capture::kNumBuffers];
std::mutex frame_locked_mutex;
int next_frame_index = 0;

shoom::Shm *shm = nullptr;
shoom::Shm *audio_shm = nullptr;
int64_t audio_frame_size = 0;
int64_t audio_shm_num_frames = 0;
int64_t audio_shm_committed_offset = 0;
int64_t audio_shm_processed_offset = 0;

std::mutex out_mutex;
std::mutex shm_mutex;
std::mutex audio_shm_mutex;

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

static void HandlePacket(char *buf) {
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
                std::lock_guard<std::mutex> lock(shm_mutex);
                delete shm;
                shm = nullptr;
            }
            if (audio_shm) {
                std::lock_guard<std::mutex> lock(audio_shm_mutex);
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

void WriteVideoFormat(int width, int height, int format, bool vflip, int64_t pitch) {
    flatbuffers::FlatBufferBuilder builder(1024);

    Log("Writing video format");
    auto state = capture::GetState();

    flatbuffers::Offset<messages::AudioSetup> audio_setup;
    if (state->has_audio_intercept) {
        Log("Sending audio intercept info: %d channels, %d rate, %s format",
            state->audio_intercept_channels,
            state->audio_intercept_rate,
            messages::EnumNameSampleFmt(state->audio_intercept_format)
        );

        int64_t seconds = 4;
        int64_t sample_size = audio::SampleWidth(state->audio_intercept_format) / 8;
        Ensure("audio intercept format is valid", sample_size != 0);

        audio_frame_size = sample_size * (int64_t) state->audio_intercept_channels;
        audio_shm_num_frames = seconds * (int64_t) state->audio_intercept_rate;
        audio_shm_committed_offset = 0;
        audio_shm_processed_offset = audio_shm_num_frames;

        int64_t audio_shmem_size = audio_shm_num_frames * audio_frame_size;
        std::string audio_shmem_path = "capsule_audio.shm";
        audio_shm = new shoom::Shm(audio_shmem_path, static_cast<size_t>(audio_shmem_size));

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
        connection->Write(builder);
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
    if (is_skipping) {
        Log("not skipping anymore!");
        is_skipping = false;
    }

    flatbuffers::FlatBufferBuilder builder(64);

    int64_t offset = (frame_data_size * next_frame_index);

    {
        std::lock_guard<std::mutex> lock(shm_mutex);
        if (!shm) {
            Log("SHM is gone, not writing video frame");
            return;
        }

        char *target = reinterpret_cast<char*>(shm->Data() + offset);
        memcpy(target, frame_data, frame_data_size);
    }

    auto vfc = messages::CreateVideoFrameCommitted(builder, timestamp,
                                                   next_frame_index);
    auto pkt = messages::CreatePacket(
        builder, messages::Message_VideoFrameCommitted, vfc.Union());
    builder.Finish(pkt);

    LockFrame(next_frame_index);
    {
        std::lock_guard<std::mutex> lock(out_mutex);
        connection->Write(builder);
    }

    next_frame_index = (next_frame_index + 1) % capture::kNumBuffers;
}

void WriteAudioFrames(char *src_data, int64_t src_frames) {
    std::lock_guard<std::mutex> lock(audio_shm_mutex);
    if (!audio_shm) {
        Log("Audio SHM is gone, not writing audio frame");
        return;
    }

    auto dst_data = (char*) audio_shm->Data();
 
    int64_t src_offset = 0;
    while (src_offset < src_frames) {
      if (audio_shm_committed_offset == audio_shm_num_frames) {
        DebugLog("io: Wrapping!");
        // wrap around!
        audio_shm_committed_offset = 0;
      }

      int64_t write_frames = src_frames - src_offset;
      int64_t avail_frames = audio_shm_num_frames - audio_shm_committed_offset;

      if (write_frames > avail_frames) {
        write_frames = avail_frames;
      }

      {
        DebugLog("io: putting %" PRIdS " frames at %d", write_frames,
            audio_shm_committed_offset);
        char *src = src_data + (src_offset * audio_frame_size);
        char *dst = dst_data + (audio_shm_committed_offset * audio_frame_size);
        int64_t copy_size = write_frames * audio_frame_size;
        memcpy(dst, src, copy_size);

        flatbuffers::FlatBufferBuilder builder(64);
        auto afc = messages::CreateAudioFramesCommitted(
            builder, audio_shm_committed_offset,
            write_frames);
        auto pkt = messages::CreatePacket(
            builder, messages::Message_AudioFramesCommitted, afc.Union());
        builder.Finish(pkt);

        {
          std::lock_guard<std::mutex> lock(out_mutex);
          connection->Write(builder);
        }

        audio_shm_committed_offset += write_frames;
        src_offset += write_frames;
      }
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
        connection->Write(builder);
    }
}

void WriteCaptureStop() {
    flatbuffers::FlatBufferBuilder builder(32);

    auto cs = messages::CreateCaptureStop(builder);
    auto pkt = messages::CreatePacket(
        builder,
        messages::Message_CaptureStop,
        cs.Union()
    );

    builder.Finish(pkt);

    {
        std::lock_guard<std::mutex> lock(out_mutex);
        connection->Write(builder);
    }
}

void WriteSawBackend(messages::Backend backend) {
    flatbuffers::FlatBufferBuilder builder(32);

    auto sb = messages::CreateSawBackend(builder, backend);
    auto pkt = messages::CreatePacket(
        builder,
        messages::Message_SawBackend,
        sb.Union()
    );

    builder.Finish(pkt);

    {
        std::lock_guard<std::mutex> lock(out_mutex);
        connection->Write(builder);
    }
}

static void PollInfile() {
  while (true) {
    if (!connection) {
      break;
    }

    char *buf = connection->Read();
    if (!buf) {
        break;
    }

    HandlePacket(buf);
  }
}

void Init() {
    std::string pipe_path = lab::env::Get("CAPSULE_PIPE_PATH");
    Log("First pipe path is '%s'", pipe_path.c_str());
    auto temp_conn = new Connection(pipe_path);
    temp_conn->Connect();
    if (!temp_conn->IsConnected()) {
        delete temp_conn;
        Log("Error: Could not reach capsulerun router, bailing out");
        return;
    }

    Log("Waiting for ready...");
    char *buf = temp_conn->Read();
    temp_conn->Close();
    delete temp_conn;
    if (!buf) {
        Log("Error: Could not even get ready, bailing out");
        return;
    }

    auto pkt = messages::GetPacket(buf);
    if (pkt->message_type() != messages::Message_ReadyForYou) {
        Log("Error: didn't get ReadyForYou, got %s", EnumNameMessage(pkt->message_type()));
        delete[] buf;
        return;
    }

    {
        auto rfy = pkt->message_as_ReadyForYou();
        pipe_path = rfy->pipe()->str();
        Log("Second pipe path is '%s'", pipe_path.c_str());
        connection = new Connection(pipe_path);
        connection->Connect();
        if (!connection->IsConnected()) {
            delete connection;
            connection = nullptr;
            Log("Error: could not make second connection");
            return;
        }
    }

    delete[] buf;

    new std::thread(PollInfile);
    Log("Connection with capsulerun established!");
}

void Cleanup() {
    Log("capsule::io cleaning up");
    if (connection) {
        connection->Close();
        delete connection;
        connection = nullptr;
    }
}

} // namespace io
} // namespace capsule
