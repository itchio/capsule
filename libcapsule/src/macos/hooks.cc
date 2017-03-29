
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

#include <sys/types.h>
#include <unistd.h>

#include <lab/env.h>

#include "../logging.h"
#include "../io.h"

void __attribute__((constructor)) CapsuleConstructor() {
  pid_t pid = getpid();
  capsule::Log("Injected into pid %d...", pid);

  capsule::Log("DYLD_INSERT_LIBRARIES: %s",
               lab::env::Get("DYLD_INSERT_LIBRARIES").c_str());
  capsule::Log("LD_PRELOAD: %s", lab::env::Get("LD_PRELOAD").c_str());

  capsule::io::Init();
}

void __attribute__((destructor)) CapsuleDestructor() {
  capsule::Log("Winding down...");
}
