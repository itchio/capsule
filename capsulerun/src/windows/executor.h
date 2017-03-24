
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

#include "../runner.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN

namespace capsule {
namespace windows {

class Process : public ProcessInterface {
  public:
    Process(HANDLE process_handle) :
      process_handle_(process_handle) {};
    ~Process() override;

    void Wait(ProcessFate *fate) override;

  private:
    HANDLE process_handle_ = INVALID_HANDLE_VALUE;
};

class Executor : public ExecutorInterface {
  public:
    Executor();
    ~Executor() override;

    ProcessInterface *LaunchProcess(MainArgs *args) override;
    AudioReceiverFactory GetAudioReceiverFactory() override;
};

} // namespace windows
} // namespace capsule
