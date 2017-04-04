
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

#include "main_loop.h"

#include <microprofile.h>

#include "logging.h"
#include "audio_intercept_receiver.h"

MICROPROFILE_DEFINE(MainLoopMain, "MainLoop", "Main", 0xff0000);
MICROPROFILE_DEFINE(MainLoopCycle, "MainLoop", "Cycle", 0xff00ff38);
MICROPROFILE_DEFINE(MainLoopRead, "MainLoop", "Read", 0xff00ff00);
MICROPROFILE_DEFINE(MainLoopProcess, "MainLoop", "Process", 0xff773744);

namespace capsule {

void MainLoop::Run () {
  MICROPROFILE_SCOPE(MainLoopCycle);
  Log("In MainLoop::Run, exec is %s", args_->exec);

  while (true) {
    MICROPROFILE_SCOPE(MainLoopCycle);
    MicroProfileFlip(0);

    char *buf;

    {
      MICROPROFILE_SCOPE(MainLoopRead);
      buf = conn_->Read();
      if (!buf) {
        Log("MainLoop::Run: pipe closed");
        break;
      }
    }

    {
      MICROPROFILE_SCOPE(MainLoopProcess);
      auto pkt = messages::GetPacket(buf);
      switch (pkt->message_type()) {
        case messages::Message_HotkeyPressed: {
          CaptureFlip();
          break;
        }
        case messages::Message_CaptureStop: {
          CaptureStop();
          break;
        }
        case messages::Message_VideoSetup: {
          auto vs = pkt->message_as_VideoSetup();
          StartSession(vs);
          break;
        }
        case messages::Message_VideoFrameCommitted: {
          auto vfc = pkt->message_as_VideoFrameCommitted();
          if (session_ && session_->video_) {
            session_->video_->FrameCommitted(vfc->index(), vfc->timestamp());
          }
          break;
        }
        case messages::Message_AudioFramesCommitted: {
          auto afc = pkt->message_as_AudioFramesCommitted();
          if (session_ && session_->audio_) {
            session_->audio_->FramesCommitted(afc->offset(), afc->frames());
          }
          break;
        }
        default: {
          Log("MainLoop::Run: received %s - not sure what to do", messages::EnumNameMessage(pkt->message_type()));
          break;
        }
      }
    }

    delete[] buf;
  }

  EndSession();  
  JoinSessions();
}

void MainLoop::CaptureFlip () {
  Log("MainLoop::CaptureFlip");
  if (session_) {
    CaptureStop();
  } else {
    // TODO: ignore subsequent CaptureStart until the capture actually started
    CaptureStart();
  }
}

void MainLoop::CaptureStart () {
  flatbuffers::FlatBufferBuilder builder(1024);
  auto cps = messages::CreateCaptureStart(builder, args_->fps, args_->size_divider, args_->gpu_color_conv);
  auto opkt = messages::CreatePacket(builder, messages::Message_CaptureStart, cps.Union());
  builder.Finish(opkt);
  conn_->Write(builder);
}

void MainLoop::EndSession () {
  if (!session_) {
    Log("MainLoop::end_session: no session to end");
    return;
  }

  Log("MainLoop::end_session: ending %p", session_);
  auto old_session = session_;
  session_ = nullptr;
  old_session->Stop();
  old_sessions_.push_back(old_session);
}

void MainLoop::JoinSessions () {
  Log("MainLoop::join_sessions: joining %" PRIdS " sessions", old_sessions_.size());

  for (Session *session: old_sessions_) {
    Log("MainLoop::join_sessions: joining session_ %p", session);
    session->Join();
  }

  Log("MainLoop::join_sessions: joined all sessions!");
}

void MainLoop::CaptureStop () {
  EndSession();

  flatbuffers::FlatBufferBuilder builder(1024);
  auto cps = messages::CreateCaptureStop(builder);
  auto opkt = messages::CreatePacket(builder, messages::Message_CaptureStop, cps.Union());
  builder.Finish(opkt);
  conn_->Write(builder);
}

void MainLoop::StartSession (const messages::VideoSetup *vs) {
  Log("Setting up encoder");

  encoder::VideoFormat vfmt;
  vfmt.width = vs->width();
  vfmt.height = vs->height();
  vfmt.format = vs->pix_fmt();
  vfmt.vflip = vs->vflip();
  
  // TODO: support offset (for planar formats)

  // TODO: support multiple linesizes (for planar formats)
  auto linesize_vec = vs->linesize();
  vfmt.pitch = linesize_vec->Get(0);

  auto shm_path = vs->shmem()->path()->str();  
  auto shm = new shoom::Shm(shm_path, static_cast<size_t>(vs->shmem()->size()));
  int ret = shm->Open();
  if (ret != shoom::kOK) {
    Log("Could not open shared memory area: code %d", ret);
    return;
  }

  int num_buffered_frames = 60;
  if (args_->buffered_frames) {
    num_buffered_frames = args_->buffered_frames;
  }

  auto video = new video::VideoReceiver(conn_, vfmt, shm, num_buffered_frames);

  audio::AudioReceiver *audio = nullptr;
  if (args_->no_audio) {
    Log("Audio capture disabled by command-line flag");
  } else {
    auto as = vs->audio();
    if (as) {
      audio = new audio::AudioInterceptReceiver(conn_, *as);
    } else if (audio_receiver_factory_) {
      Log("No audio intercept (or disabled), trying factory");
      audio = audio_receiver_factory_();
    } else {
      Log("No audio intercept or factory = no audio");
    }
  }

  session_ = new Session(args_, video, audio);
  session_->Start();
}

} // namespace capsule
