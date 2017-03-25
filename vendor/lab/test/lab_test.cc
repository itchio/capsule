
/*
 *  lab - a general-purpose C++ toolkit
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
 * https://github.com/itchio/lab/blob/master/LICENSE
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <lab/env.h>
#include <lab/io.h>
#include <lab/packet.h>
#include <lab/platform.h>
#include <lab/strings.h>
#include <lab/types.h>
#include <lab/paths.h>

#include <flatbuffers/flatbuffers.h>
#include "test_generated.h"

#if defined(LAB_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#else
#include <fcntl.h>
#endif

#include "lest.hpp"

const lest::test specification[] = {
#if defined(LAB_WINDOWS)
  CASE("lab::strings::{ToWide,FromWide} converts from utf-8 to utf-16 and back") {
    std::string input = "漢字 🐶 hello";
    auto wide = lab::strings::ToWide(input);
    auto result = lab::strings::FromWide(wide);
    EXPECT(result == input);
  },

  CASE("lab::strings::ArgvQuote quotes shell arguments") {
    std::wstring line;
    lab::strings::ArgvQuote(std::wstring(L"hello"), line, false);
    EXPECT(L"hello" == line);

    line = L"";
    lab::strings::ArgvQuote(std::wstring(L"no pressure.exe"), line, false);
    EXPECT(L"\"no pressure.exe\"" == line);

    line = L"";
    lab::strings::ArgvQuote(std::wstring(L"some \\sort of \"quote\", kind of"), line, false);
    EXPECT(L"\"some \\sort of \\\"quote\\\", kind of\"" == line);
  },
#endif // LAB_WINDOWS

  CASE("lab::strings::CContains") {
    EXPECT(true == lab::strings::CContains("booga", "oog"));
    EXPECT(true == lab::strings::CContains("booga", "boo"));
    EXPECT(true == lab::strings::CContains("booga", "oga"));
    EXPECT(false == lab::strings::CContains("booga", "oof"));
    EXPECT(false == lab::strings::CContains("booga", "heh"));
    EXPECT(false == lab::strings::CContains("booga", "yay"));
  },

  CASE("lab::strings::CEquals") {
    EXPECT(true == lab::strings::CEquals("booga", "booga"));
    EXPECT(false == lab::strings::CEquals("booga", "oogah"));
  },

  CASE("lab::env::{Get,Set}") {
    std::string name = "LAB_TEST";
    std::string value = "漢字 🐶 hello";
    lab::env::Set(name, value);
    std::string value2 = lab::env::Get(name);
    EXPECT(value == value2);
  },

#if defined(LAB_WINDOWS)
  CASE("lab::env::Expand") {
    auto result = lab::env::Expand("hello %LOCALAPPDATA% world");
    auto control = "hello " + lab::env::Get("LOCALAPPDATA") + " world";
    EXPECT(control == result);
  },
#endif

  CASE("lab::env::MergeBlocks") {
    const char *a[] = { "A=1", "C=3", nullptr };
    const char *b[] = { "B=2", "D=4", nullptr };
    auto c = lab::env::MergeBlocks(const_cast<char **>(a), const_cast<char **>(b));
    EXPECT(std::string("A=1") == std::string(c[0]));
    EXPECT(std::string("C=3") == std::string(c[1]));
    EXPECT(std::string("B=2") == std::string(c[2]));
    EXPECT(std::string("D=4") == std::string(c[3]));
    EXPECT(nullptr == c[4]);
  },

  CASE("lab::paths::SelfPath") {
    EXPECT(lab::paths::SelfPath().length() > static_cast<size_t>(0));
  },

  CASE("lab::paths::{Join,DirName}") {
#if defined(LAB_WINDOWS)
    auto base = "C:\\Program Files";
    auto control = "C:\\Program Files\\CMake";
#else
    auto base = "/opt";
    auto control = "/opt/CMake";
#endif
    auto result = lab::paths::Join(base, "CMake");
    EXPECT(control == result);
    EXPECT(base == lab::paths::DirName(result));
  },

  CASE("lab::packet (fstream)") {
    auto path = std::string("test.bin");
    auto writer = lab::io::Fopen(path, "wb");
    EXPECT(!!writer);

    flatbuffers::FlatBufferBuilder builder(1024);
    auto pkt = CreateTestPacket(builder, 42, 3.14f);
    builder.Finish(pkt);

    lab::packet::Fwrite(builder, writer);
    fclose(writer);

    auto reader = lab::io::Fopen(path, "rb");
    EXPECT(!!reader);
    auto blob = lab::packet::Fread(reader);
    fclose(reader);

    auto rpkt = GetTestPacket(blob);
    EXPECT(rpkt->answer() == 42);
    EXPECT(3.13f < rpkt->pi());
    EXPECT(rpkt->pi() < 3.15f);
    delete[] blob;
  },

#if defined(LAB_WINDOWS)
  CASE("lab::packet (HANDLE)") {
    const wchar_t *path = L"test.bin";
    auto writer = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    EXPECT(!!writer);

    flatbuffers::FlatBufferBuilder builder(1024);
    auto pkt = CreateTestPacket(builder, 42, 3.14f);
    builder.Finish(pkt);

    lab::packet::Hwrite(builder, writer);
    CloseHandle(writer);

    auto reader = CreateFileW(path, GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    EXPECT(!!reader);
    auto blob = lab::packet::Hread(reader);
    CloseHandle(reader);

    auto rpkt = GetTestPacket(blob);
    EXPECT(rpkt->answer() == 42);
    EXPECT(3.13f < rpkt->pi());
    EXPECT(rpkt->pi() < 3.15f);
    delete[] blob;
  },
#else // LAB_WINDOWS
  CASE("lab::packet (syscall)") {
    const char *path = "test.bin";
    auto writer = open(path, O_CREAT | O_TRUNC | O_WRONLY);
    EXPECT(!!writer);

    flatbuffers::FlatBufferBuilder builder(1024);
    auto pkt = CreateTestPacket(builder, 42, 3.14f);
    builder.Finish(pkt);

    lab::packet::Write(builder, writer);
    close(writer);

    auto reader = open(path, O_RDONLY);
    EXPECT(!!reader);
    auto blob = lab::packet::Read(reader);
    close(reader);

    auto rpkt = GetTestPacket(blob);
    EXPECT(rpkt->answer() == 42);
    EXPECT(3.13f < rpkt->pi());
    EXPECT(rpkt->pi() < 3.15f);
    delete[] blob;
  },

#endif // !LAB_WINDOWS
};

int main (int argc, char *argv[]) {
  return lest::run(specification, argc, argv);
}
