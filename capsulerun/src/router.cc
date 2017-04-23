
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

#include "router.h"
#include "logging.h"

#include <thread>

namespace capsule {

Router::~Router() {
  // muffin
}

void Router::Start() {
  new std::thread(&Router::Run, this);
}

void Router::Run() {
  while (true) {
    Log("Router: Accepting connection...");
    conn_->Connect();

    if (!conn_->IsConnected()) {
      Log("Router: Failed to accept connection, bailing out");
      exit(127);
    }

    Log("Router: Should dispatch connection!");
    // conn_->Close();
    loop_->AddConnection(conn_);
  }
}

} // namespace capsule
