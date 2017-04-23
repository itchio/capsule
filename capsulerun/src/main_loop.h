
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

#include <shoom.h>

#include <lab/packet.h>
#include <capsule/messages_generated.h>

#include "args.h"
#include "video_receiver.h"
#include "audio_receiver.h"
#include "session.h"
#include "connection.h"
#include "locking_queue.h"

#include <thread>
#include <mutex>
#include <vector>

namespace capsule {

typedef audio::AudioReceiver * (*AudioReceiverFactory)();

struct LoopMessage {
  Connection *conn;
  char *buf;
};

class MainLoop {
  public:
    MainLoop(MainArgs *args) :
      args_(args) {};
    void Run(void);
    void CaptureFlip();

    void AddConnection(Connection *conn);

    AudioReceiverFactory audio_receiver_factory_ = nullptr;

  private:
    void EndSession();
    void JoinSessions();
    void PollConnection(Connection *conn);

    void CaptureStart();
    void CaptureStop();
    void StartSession(const messages::VideoSetup *vs, Connection *conn);

    MainArgs *args_;
    LockingQueue<LoopMessage> queue_;

    std::vector<Connection *> conns_;
    std::mutex conns_mutex_;

    Session *session_ = nullptr;
    std::vector<Session *> old_sessions_;
};

} // namespace capsule
