
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

#include <stdarg.h>

#include <lab/platform.h>
#include <lab/env.h>
#include <lab/io.h>

namespace capsule {

namespace {

FILE *logfile;

std::string CapsuleLogPath () {
#if defined(LAB_WINDOWS)
  return lab::env::Expand("%APPDATA%\\capsule.log.txt");
#else // LAB_WINDOWS
  return "/tmp/capsule.log.txt";
#endif // !LAB_WINDOWS
}

} // namespace

void Log(const char *format, ...) {
  va_list args;

  if (!logfile) {
    logfile = lab::io::Fopen(CapsuleLogPath(), "w");
  }

  va_start(args, format);
  vfprintf(logfile, format, args);
  va_end(args);

  fprintf(logfile, "\n");
  fflush(logfile);

  fprintf(stderr, "[capsule] ");

  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);

  fprintf(stderr, "\n");
}

} // namespace capsule
