// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: repl6_check.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/15/2026
// Date Last Modified: 11/17/2026
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

DataReel 6.4 replication example: a DataReel 6.x program serving as
the copy of a DataReel 7.x object replication pair (gxv7ObjRepl,
dr7features.h). The primary can be repl6_writer, a DataReel 7.4
program or the 7.4 objrepl tool, on Linux or Windows.

  repl6_check copy ROOT CONF [--check] serve until TERM or INT (Ctrl+C on
                                       Windows) or until ROOT/objrepl.stop
                                       appears. --check: then read every
                                       live record back and check its
                                       CRC (records repl6_writer wrote);
                                       with "index station:readings:B:0:16"
                                       in CONF, find each one by station
                                       with FindKey()
  repl6_check digest ROOT | status ROOT | engine   as repl6_writer

CONF is an objrepl.conf file: the copy needs listen ([ADDR:]PORT), ca,
cert, key and allow-cn (the primary's certificate CN). See README.txt.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <atomic>
#include <chrono>
#include <thread>
#if defined(__WIN32__) || defined(_WIN32)
#include <windows.h>
#include <io.h>
#define access _access
#define F_OK 0
#else
#include <unistd.h>
#endif
#include "dr7features.h"
#include "repl6_records.h"

// digest ROOT, status ROOT, engine: the same in both programs (-1: none of them)
static int folder_cmd(int argc, char **argv)
{
  gxString line;
  if(argc == 2 && strcmp(argv[1], "engine") == 0) {
    printf("engine %s\n", gxv7ObjRepl::Engine64() ? "64-bit" : "32-bit");
    return gxv7ObjRepl::Engine64() ? 0 : 1;
  }
  if(argc == 3 && strcmp(argv[1], "digest") == 0) {
    int rc = gxv7ObjRepl::Digest(argv[2], line);
    printf("%s\n", line.c_str());
    return rc == 0 ? 0 : 1;
  }
  if(argc == 3 && strcmp(argv[1], "status") == 0) {
    int rc = gxv7ObjRepl::Status(argv[2], line);
    printf("%s\n", line.c_str());
    return rc == 0 ? 0 : 1;
  }
  return -1;
}

static gxv7ObjRepl *serving = 0;
static void on_signal(int) { if(serving) serving->Stop(); }
#if defined(__WIN32__) || defined(_WIN32)
static BOOL WINAPI on_ctrl(DWORD) { if(serving) serving->Stop(); return TRUE; }
#endif

static int copy(const char *root, const char *conf, int check)
{
  gxv7ObjRepl c;
  if(c.LoadConfig(conf) != 0 || c.Open(root, gxv7ObjRepl::COPY) != 0) {
    printf("repl6_check: %s\n", c.Error().c_str());
    return 1;
  }
  serving = &c;
  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);
#if defined(__WIN32__) || defined(_WIN32)
  SetConsoleCtrlHandler(on_ctrl, TRUE);
#endif
  // ROOT/objrepl.stop (any platform): stop within a second
  gxString stopf(root);
  stopf << "/objrepl.stop";
  remove(stopf.c_str());
  std::atomic<int> done(0);
  std::thread watch([&]() {
    while(!done) {
      if(access(stopf.c_str(), F_OK) == 0) { c.Stop(); remove(stopf.c_str()); break; }
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
  });
  printf("repl6_check: serving %s\n", root);
  fflush(stdout);
  int rc = c.Serve();
  done = 1;
  watch.join();
  serving = 0;
  if(rc != 0) printf("repl6_check: %s\n", c.Error().c_str());
  printf("repl6_check: applied_seq %llu epoch %llu\n", c.AppliedSeq(), c.Epoch());
  if(check) {
    unsigned long long *ids = 0;
    unsigned long n = 0, bad = 0;
    if(c.Ids(REPL6_DB, ids, n) != 0) { printf("repl6_check: Ids: %s\n", c.Error().c_str()); rc = -1; }
    // With a "station" index in CONF (index station:readings:B:0:16, the
    // copy's own B-tree on the station name): each record is found by
    // its station with FindKey()
    unsigned long found = 0, missed = 0;
    int have_index = 0;
    for(unsigned long i = 0; i < n; i++) {
      MemoryBuffer b;
      if(c.Get(REPL6_DB, ids[i], b) != 0 || !reading_ok(b)) { bad++; continue; }
      Reading r;
      memcpy(&r, b.m_buf(), sizeof(r));
      unsigned long long hits[256];
      unsigned count = 0;
      if(c.FindKey("station", r.station, sizeof(r.station), hits, 256, count) != 0) continue;
      have_index = 1;
      int in = 0;
      for(unsigned k = 0; k < count && k < 256; k++) if(hits[k] == ids[i]) in = 1;
      if(in) found++; else missed++;
    }
    delete[] ids;
    printf("repl6_check: %lu live records, %lu with a bad CRC or unreadable\n", n, bad);
    if(have_index) {
      printf("repl6_check: station index: %lu records found by FindKey(), %lu not found\n", found, missed);
      if(missed) bad++;
    }
    if(bad) rc = -1;
  }
  c.Close();
  return rc == 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
  int rc = folder_cmd(argc, argv);
  if(rc >= 0) return rc;
  if((argc == 4 || argc == 5) && strcmp(argv[1], "copy") == 0)
    return copy(argv[2], argv[3], argc == 5 && strcmp(argv[4], "--check") == 0);
  fprintf(stderr, "usage: repl6_check copy ROOT CONF [--check] | digest ROOT | status ROOT | engine\n");
  return 64;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
