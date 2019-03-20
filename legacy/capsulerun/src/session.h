
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
 
#pragma once

#include "audio_receiver.h"
#include "video_receiver.h"

#include <thread>

namespace capsule {

class Session {
  public: 
    Session(MainArgs *args, video::VideoReceiver *video, audio::AudioReceiver *audio) :
      args_(args),
      video_(video),
      audio_(audio) {};
    ~Session();
    void Start();
    void Stop();
    void Join();

    encoder::Params encoder_params_;

  private:
    std::thread *encoder_thread_;
    MainArgs *args_;

  public:
    // these need to be public for the C callbacks (to avoid
    // another level of indirection)
    video::VideoReceiver *video_;
    audio::AudioReceiver *audio_;
};

} // namespace capsule
