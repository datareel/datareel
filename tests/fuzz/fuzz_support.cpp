// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: fuzz_support.cpp
// Compiler Used: Clang (libFuzzer)
// Produced By: DataReel Software Development Team
// File Creation Date: 11/01/2026
// Date Last Modified: 11/01/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- //
// ------------- Program Description and Details ------------- //
// ----------------------------------------------------------- //
/*
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library. If not, see
<https://www.gnu.org/licenses/>.

libFuzzer target for the support classes (POSIX). The first input
byte picks the code under test and the rest is its input:

  0  telnet option/IAC processing: every byte goes through
     gxsTelnetClient::ReadTelnetData() on a socket pair, whose
     replies are drained and discarded
  1  config files: the input is loaded as a config file, every
     value is read as a number and one value is changed
  2  CDate::SetDate() with '/' and with the delimiter from byte 1
  3  ping replies: gxsPing::ParseEchoReply() with the flags in byte 1
  4  damaged database files: the input is written to a file, opened
     with gxDatabaseDebugManager::BlindOpen(), analyzed, walked with
     the db_debug block walk and rebuilt into a second file

Temporary files go in the system temp folder, named by process id.

Build (clang, 64-bit engine, as tests/fuzz/build_fuzz.sh builds the
library):
  clang++ -std=c++17 -fsanitize=fuzzer,address,undefined $DEFS \
    -Iinclude tests/fuzz/fuzz_support.cpp libdatareel.a -lssl -lcrypto \
    -lpthread -o fuzz_support
Every run must end with an error code or good data: a crash, hang,
sanitizer report or leak is a finding.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "cdate.h"
#include "gxconfig.h"
#include "gxsping.h"
#include "gxtelnet.h"
#include "dbugmgr.h"
#include "../../utils/db_debug/db_walk.h"

using namespace datareel;
namespace fs = std::filesystem;

static std::string temp_name(const char *suffix)
{
  std::error_code ec;
  fs::path p = fs::temp_directory_path(ec);
  if(ec) p = ".";
  return (p / ("fuzz_support_" + std::to_string((long long)getpid()) +
	       suffix)).string();
}

static bool write_file(const std::string &name, const uint8_t *data,
		       size_t size)
{
  std::ofstream o(name, std::ios::binary | std::ios::trunc);
  if(!o) return false;
  o.write((const char *)data, (std::streamsize)size);
  return bool(o);
}

static void drain(int fd)
{
  char buf[4096];
  while(read(fd, buf, sizeof(buf)) > 0) { }
}

static void fuzz_telnet(const uint8_t *data, size_t size)
{
  int sv[2];
  if(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) return;
  // Non blocking both ways: a reply that does not fit fails instead of
  // blocking the fuzzer
  fcntl(sv[0], F_SETFL, fcntl(sv[0], F_GETFL) | O_NONBLOCK);
  fcntl(sv[1], F_SETFL, fcntl(sv[1], F_GETFL) | O_NONBLOCK);
  gxsTelnetClient client;
  static_cast<gxSocket &>(client) = gxSocket(sv[0]);
  if(size > 0 && (data[0] & 1)) client.SetTermType(gxsTEL_VT100F_TERM);
  unsigned char c;
  for(size_t i = 0; i < size; i++) {
    client.ReadTelnetData(data[i]);
    if(client.GetChar(c)) { } // Filtered output
    if((i & 255) == 255) drain(sv[1]);
  }
  drain(sv[1]);
  close(sv[1]);
}

static void fuzz_config(const uint8_t *data, size_t size)
{
  std::string name = temp_name(".cfg");
  if(!write_file(name, data, size)) return;
  gxConfig cfg(name);
  if(cfg.Load()) {
    for(const auto &e : cfg.Entries()) {
      cfg.GetIntValue(e.first);
      cfg.GetLongValue(e.first);
      cfg.GetFloatValue(e.first);
      cfg.GetStrValue(e.first, false);
    }
    if(!cfg.Entries().empty()) {
      std::string key = cfg.Entries().front().first;
      cfg.ChangeConfigValue(key, "fuzz");
    }
  }
  std::error_code ec;
  fs::remove(name, ec);
  fs::remove(name + ".tmp", ec);
}

static void fuzz_date(const uint8_t *data, size_t size)
{
  std::string_view s((const char *)data, size);
  CDate d;
  d.SetDate(s);
  d.str();
  if(size > 1) {
    CDate d2;
    d2.SetDate(s.substr(1), (char)data[0]);
    d2.str();
  }
}

static void fuzz_ping(const uint8_t *data, size_t size)
{
  if(size < 1) return;
  int flags = data[0];
  int ttl = 0;
  gxsPing::ParseEchoReply(data + 1, (int)(size - 1), (flags & 1) != 0,
			  (flags & 2) != 0, 0x1234, 1, (flags & 4) != 0, ttl);
}

static void fuzz_dbdebug(const uint8_t *data, size_t size)
{
  std::string in = temp_name("_in.gxd");
  std::string out = temp_name("_out.gxd");
  if(!write_file(in, data, size)) return;

  // BlindOpen() and Rebuild() change the static version information;
  // restore it so each input starts the same way
  __SBYTE__ sig[gxSignatureSize];
  memcpy(sig, gxDatabase::gxSignature, gxSignatureSize);
  FAU ver = gxDatabase::gxVersion;
  gxUINT32 cw = gxDatabase::gxInternalCheckWord;
  {
    gxDatabaseDebugManager dm;
    if(dm.BlindOpen(in.c_str()) == gxDBASE_NO_ERROR) {
      dm.AnalyzeHeader();
      FAU_t good, bad;
      gxdWalkBlocks(&dm, good, bad);
      dm.Rebuild(out.c_str());
      dm.Rebuild(in.c_str()); // Must be refused, never truncate the input
    }
  }
  memcpy(gxDatabase::gxSignature, sig, gxSignatureSize);
  gxDatabase::gxVersion = ver;
  gxDatabase::gxInternalCheckWord = cw;

  std::error_code ec;
  fs::remove(in, ec);
  fs::remove(out, ec);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  if(size < 1) return 0;
  const uint8_t *rest = data + 1;
  size_t n = size - 1;
  switch(data[0] % 5) {
    case 0: fuzz_telnet(rest, n); break;
    case 1: fuzz_config(rest, n); break;
    case 2: fuzz_date(rest, n); break;
    case 3: fuzz_ping(rest, n); break;
    case 4: fuzz_dbdebug(rest, n); break;
  }
  return 0;
}

// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
