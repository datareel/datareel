// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: fuzz_stream.cpp
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

libFuzzer target for the gxStream block protocol (Linux only). Each
input is the byte stream a client sends: it is written into one end
of a socketpair and the stream_server dispatch loop (serve_client()
from examples/tls/stream_server.cpp, included below) reads the other
end against a fresh gxDatabase seeded with a few blocks, some of
them deleted. Replies are read and thrown away by a second thread.

TLS is not used. gxStream refuses a connection that is not secure,
so the target marks it with gxsTLSConnection::FuzzAssumeSecure(),
which exists only when DATAREEL_FUZZING is defined.

Build (clang, 64-bit engine, same defines as the library):
  clang++ -std=c++17 -fsanitize=fuzzer,address,undefined \
    -DDATAREEL_FUZZING $DEFS -I include -I examples/tls \
    tests/fuzz/fuzz_stream.cpp libdatareel.a -lssl -lcrypto -lpthread
The library itself does not need DATAREEL_FUZZING. Run in a scratch
folder: the database file fuzz_stream.gxd is made in the current
folder. Every run must end with an error code or good data: a crash,
hang, sanitizer report or leak is a finding.
*/
// ----------------------------------------------------------- //
#if !defined (DATAREEL_FUZZING)
#error fuzz_stream.cpp needs -DDATAREEL_FUZZING
#endif

#include "gxdlcode.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <thread>

#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

// The real server dispatch loop, without its main() and log output
#define STREAM_SERVER_NO_MAIN
#include "../../examples/tls/stream_server.cpp"

using namespace datareel;

static const char *fuzz_db_name = "fuzz_stream.gxd";
static const std::size_t fuzz_max_input = 256 * 1024;

static int seed_database(gxDatabase &db)
// Fresh database: four normal blocks, one deleted and one removed
{
  if(db.Create(fuzz_db_name) != gxDBASE_NO_ERROR) return -1;
  const char *recs[] = { "first block", "second, a longer block of data",
			 "third", "fourth block", "deleted block",
			 "removed block" };
  FAU_t addr[6];
  for(int i = 0; i < 6; i++) {
    __ULWORD__ len = (__ULWORD__)strlen(recs[i]);
    addr[i] = db.Alloc(len);
    if(addr[i] == (FAU_t)0) return -1;
    if(db.Write(recs[i], len, addr[i], 1) != gxDBASE_NO_ERROR) return -1;
  }
  db.Delete(addr[4]);
  db.Delete(addr[5], 1);
  return 0;
}

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
  signal(SIGPIPE, SIG_IGN);
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  if(size > fuzz_max_input) return 0;
  gxDatabase db;
  if(seed_database(db) != 0) {
    fprintf(stderr, "fuzz_stream: cannot create %s\n", fuzz_db_name);
    abort();
  }

  int sv[2];
  if(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) abort();
  int bufsize = 1024 * 1024;
  setsockopt(sv[1], SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize));
  setsockopt(sv[0], SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));

  // The client side: send the input, half-close, then read and drop
  // every reply until the server closes its end
  std::thread peer([&]() {
    std::size_t off = 0;
    while(off < size) {
      ssize_t n = send(sv[1], data + off, size - off, MSG_NOSIGNAL);
      if(n <= 0) break;
      off += (std::size_t)n;
    }
    shutdown(sv[1], SHUT_WR);
    char buf[65536];
    while(recv(sv[1], buf, sizeof(buf), 0) > 0) { }
  });

  {
    gxStream stream;
    stream.SetTimeOut(5); // A hang past this is a finding
    stream.SetMaxBlockSize(64 * 1024);
    stream.Connection().Socket() = gxSocket((gxsSocket_t)sv[0]);
    stream.Connection().FuzzAssumeSecure();
    // serve_client() returns at gxCloseConnection, an unknown block
    // type or a stream error. Keep serving the rest of the input until
    // the stream fails or gxKillServer is honored (odd sessions allow
    // remote shutdown, even ones refuse it).
    for(int sessions = 0; sessions < 1024; sessions++) {
      bool keep_running = serve_client(stream, db, sessions % 2 == 1);
      if(!keep_running || !stream.Connection().IsOpen()) break;
      if(stream.GetSocketError() != gxSOCKET_NO_ERROR) break;
    }
    stream.Close(); // Closes sv[0], so the peer thread sees EOF
  }
  peer.join();
  close(sv[1]);
  db.Close();
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
