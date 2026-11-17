// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: objrepl6.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/17/2026
// Date Last Modified: 11/17/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- //
// ------------- Program Description and Details ------------- //
// ----------------------------------------------------------- //
/*
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
USA

DataReel 6.4 example: a DataReel 6.x program replicating its records
with the DataReel 7.x object replication (gxv7ObjRepl, dr7features.h).
The primary and the copy can each be this program, a DataReel 7.4
program or the 7.4 objrepl tool, on Linux or Windows.

  objrepl6 primary ROOT CONF RECORDS   write RECORDS station readings
                                       (adds, every 7th updated, every
                                       11th removed), commit them 25 at
                                       a time, send them to the copy and
                                       wait up to 120 s for the copy to
                                       have them all. Exit 0 (drained),
                                       2 (not drained), 1 (an error)
  objrepl6 copy ROOT CONF [--check]    serve until TERM or INT (Ctrl+C on
                                       Windows) or until ROOT/objrepl.stop
                                       appears. --check: then read every
                                       live record and check its CRC
                                       (records this program wrote)
  objrepl6 digest ROOT                 every database: ids and content
                                       hashes (the same line as the 7.4
                                       "objrepl digest")
  objrepl6 status ROOT                 the node's status line
  objrepl6 engine                      the 7.x library's database engine
                                       (exit 0 for 64-bit, 1 for 32-bit)

CONF is an objrepl.conf file, one "key value" line each (see
utils/objrepl/objrepl_certs.sh in DataReel 7.4 for certificates and
sample files): the primary needs copy, ca, cert and key; the copy needs
listen, ca, cert, key and allow-cn.
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

#include "gxstring.h"
#include "gxcrc32.h"
#include "membuf.h"
#include "dr7features.h"

// One station reading, a plain 6.x record. The CRC covers the bytes
// before it.
struct Reading {
  char station[16];
  long long seq;
  double value;
  unsigned int day;
  unsigned int crc;
};

static const char *DB = "readings";

static void make_reading(Reading &r, long long seq, int version)
{
  memset(&r, 0, sizeof(r));
  gxString s;
  s << "KST" << (int)(seq % 97);
  strncpy(r.station, s.c_str(), sizeof(r.station) - 1);
  r.seq = seq;
  r.value = (double)seq * 0.25 + version;
  r.day = 20261117 + (unsigned)(seq % 3);
  r.crc = (unsigned int)calcCRC32((const char *)&r, (unsigned)((char *)&r.crc - (char *)&r));
}

static int reading_ok(const MemoryBuffer &b)
{
  if(b.length() != sizeof(Reading)) return 0;
  Reading r;
  memcpy(&r, b.m_buf(), sizeof(r));
  return r.crc == (unsigned int)calcCRC32((const char *)&r, (unsigned)((char *)&r.crc - (char *)&r));
}

static gxv7ObjRepl *serving = 0;
static void on_signal(int) { if(serving) serving->Stop(); }
#if defined(__WIN32__) || defined(_WIN32)
static BOOL WINAPI on_ctrl(DWORD) { if(serving) serving->Stop(); return TRUE; }
#endif

static int primary(const char *root, const char *conf, long records)
{
  gxv7ObjRepl p;
  if(p.LoadConfig(conf) != 0 || p.Open(root, gxv7ObjRepl::PRIMARY) != 0 ||
     p.OpenDatabase(DB) != 0) {
    printf("objrepl6 primary: %s\n", p.Error().c_str());
    return 1;
  }
  unsigned long long *ids = new unsigned long long[records > 0 ? records : 1];
  long adds = 0, updates = 0, removes = 0;
  Reading r;
  for(long i = 0; i < records; ) {
    if(p.Begin() != 0) { printf("objrepl6 primary: Begin: %s\n", p.Error().c_str()); delete[] ids; return 1; }
    for(int k = 0; k < 25 && i < records; k++, i++) {
      ids[i] = p.NewId();
      make_reading(r, i + 1, 0);
      if(ids[i] == 0 || p.Put(DB, ids[i], &r, sizeof(r)) != 0) {
	printf("objrepl6 primary: Put: %s\n", p.Error().c_str()); delete[] ids; return 1;
      }
      adds++;
      if(i % 7 == 6) { // an update of an earlier record
	make_reading(r, i - 5, 1);
	if(p.Put(DB, ids[i - 6], &r, sizeof(r)) != 0) { printf("objrepl6 primary: update: %s\n", p.Error().c_str()); delete[] ids; return 1; }
	updates++;
      }
      if(i % 11 == 10 && (i - 3) % 7 != 0) { // a remove (the id stays as a
					      // tombstone), never of one updated later
	if(p.Remove(DB, ids[i - 3]) != 0) { printf("objrepl6 primary: Remove: %s\n", p.Error().c_str()); delete[] ids; return 1; }
	removes++;
      }
    }
    if(p.Commit() != 0) { printf("objrepl6 primary: Commit: %s\n", p.Error().c_str()); delete[] ids; return 1; }
    p.Pump(); // The copy may be away: Drain() below waits for it
  }
  delete[] ids;
  int rc = p.Drain(120);
  printf("objrepl6 primary: %ld adds, %ld updates, %ld removes, last_seq %llu, copy has %llu (%s)\n",
	 adds, updates, removes, p.LastSeq(), p.CopyApplied(), rc == 0 ? "drained" : "not drained");
  if(rc != 0) printf("objrepl6 primary: %s\n", p.Error().c_str());
  p.Close();
  return rc == 0 ? 0 : 2;
}

static int copy(const char *root, const char *conf, int check)
{
  gxv7ObjRepl c;
  if(c.LoadConfig(conf) != 0 || c.Open(root, gxv7ObjRepl::COPY) != 0) {
    printf("objrepl6 copy: %s\n", c.Error().c_str());
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
  printf("objrepl6 copy: serving %s\n", root);
  fflush(stdout);
  int rc = c.Serve();
  done = 1;
  watch.join();
  serving = 0;
  if(rc != 0) printf("objrepl6 copy: %s\n", c.Error().c_str());
  printf("objrepl6 copy: applied_seq %llu epoch %llu\n", c.AppliedSeq(), c.Epoch());
  if(check) {
    unsigned long long *ids = 0;
    unsigned long n = 0, bad = 0;
    if(c.Ids(DB, ids, n) != 0) { printf("objrepl6 copy: Ids: %s\n", c.Error().c_str()); rc = -1; }
    for(unsigned long i = 0; i < n; i++) {
      MemoryBuffer b;
      if(c.Get(DB, ids[i], b) != 0 || !reading_ok(b)) bad++;
    }
    delete[] ids;
    printf("objrepl6 copy: %lu live records, %lu with a bad CRC or unreadable\n", n, bad);
    if(bad) rc = -1;
  }
  c.Close();
  return rc == 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
  gxString line;
  if(argc == 2 && strcmp(argv[1], "engine") == 0) {
    printf("engine %s\n", gxv7ObjRepl::Engine64() ? "64-bit" : "32-bit");
    return gxv7ObjRepl::Engine64() ? 0 : 1;
  }
  if(argc == 5 && strcmp(argv[1], "primary") == 0) return primary(argv[2], argv[3], atol(argv[4]));
  if((argc == 4 || argc == 5) && strcmp(argv[1], "copy") == 0)
    return copy(argv[2], argv[3], argc == 5 && strcmp(argv[4], "--check") == 0);
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
  fprintf(stderr, "usage: objrepl6 primary ROOT CONF RECORDS | copy ROOT CONF [--check] | digest ROOT | status ROOT | engine\n");
  return 64;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
