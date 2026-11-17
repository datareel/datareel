// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: replication_copy.cpp
// Compiler Used: GCC, MSVC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/14/2026
// Date Last Modified: 11/14/2026
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

A copy of a database folder by a plain file copy, on any platform
(DataReel 7.4), Windows and Linux. Where a copy minutes old is fine,
the writer stops (or finishes its job), the folder is copied with
the usual file copy tool (robocopy on Windows, rsync or cp elsewhere),
and the digests say the copy holds what the primary holds.

The records and their two indexes are in replication_records.h: a name, a day and a vector member in
data.gxd, a B-tree of names (by_name.btx) and a vector index
(vectors.vix). The writer changes all three in one gxdJournal
transaction per commit, so a writer that stops at any point leaves the
three files at a whole commit once its journal is opened again; a
writer that finishes closes the journal (a checkpoint) and the files
hold every commit.

If the copy loses or damages its indexes, it does not need them from
the primary: "rebuild" makes both again from the copy's records, and
the digests agree again.

replication_copy_walkthrough.cmd (Windows) runs it on throwaway folders:
write, copy, compare; write more, copy again, compare; the copy's
indexes lost, rebuilt, compare. tests/run_tests.sh runs the same steps
on Linux with cp.

Usage:
  replication_copy write DIR [--commits N] [--per N] [--seed N]
        The primary's writer: adds, changes and removes records in
        DIR/data.gxd with DIR/by_name.btx and DIR/vectors.vix, one
        gxdJournal transaction (DIR/writer.jnl) per commit. Run it again
        to go on writing to the same folder.
  replication_copy digest DIR
        What the folder holds, whatever the files' layout: the records
        (address and bytes, in address order), the B-tree's keys in
        order and the vector index's live nodes. Equal on the primary
        and the copy. Exit 1 if a file is missing or cannot be read.
  replication_copy rebuild DIR
        Both indexes made again from DIR/data.gxd.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <string>
#include <vector>
#include "gxdjrnl.h"
#include "replication_records.h"

using namespace datareel;
using namespace example;

static unsigned rng = 2026;
static unsigned rnd() { rng = rng * 1103515245u + 12345u; return rng >> 8; }

// FNV-1a, 64 bits: the example's own hash, the same on every platform
static uint64_t fnv(const void *p, size_t n, uint64_t h)
{
  const unsigned char *b = (const unsigned char *)p;
  for(size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ULL; }
  return h;
}
static const uint64_t FNV0 = 14695981039346656037ULL;

static int write_records(const std::string &dir, long commits, long per)
{
  gxdJournal j;
  gxDatabase db;
  NameKey type;
  gxBtree bt(type, ORDER);
  gxVectorIndex vix;
  // The journal first: it finishes or undoes a commit a stopped writer left half done
  if(j.Open((dir + "/writer.jnl").c_str()) != gxDBASE_NO_ERROR) { printf("cannot open the journal\n"); return 1; }
  if(db.Open((dir + "/data.gxd").c_str()) != gxDBASE_NO_ERROR && db.Create((dir + "/data.gxd").c_str()) != gxDBASE_NO_ERROR) return 1;
  if(bt.Open((dir + "/by_name.btx").c_str()) != gxDBASE_NO_ERROR && bt.Create((dir + "/by_name.btx").c_str()) != gxDBASE_NO_ERROR) return 1;
  if(vix.Open((dir + "/vectors.vix").c_str()) != gxv_NO_ERROR &&
     vix.Create((dir + "/vectors.vix").c_str(), DIM, gxvFLOAT32, gxvL2, 8, SPEC, 100) != gxv_NO_ERROR) return 1;
  if(j.Attach(&db) != gxDBASE_NO_ERROR || j.Attach(&bt) != gxDBASE_NO_ERROR || j.Attach(vix.Database()) != gxDBASE_NO_ERROR) return 1;
  std::vector<FAU_t> live;
  for(FAU_t a = db.FindFirstObject(); a != (FAU_t)0; a = db.FindNextObject(a)) live.push_back(a);
  long next_name = (long)live.size() + 1000 * (long)(rng % 97);
  std::vector<float> v(DIM);
  std::vector<unsigned char> mem;
  for(long c = 0; c < commits; c++) {
    j.Begin();
    unsigned day = 20261110 + (unsigned)(c % 5);
    for(long k = 0; k < per; k++) {
      unsigned r = rnd() % 10;
      Record rec;
      memset(&rec, 0, sizeof(rec));
      for(auto &x : v) x = (float)((rnd() % 2001) / 1000.0 - 1.0);
      vix.MakeMember(v.data(), mem);
      if(r < 7 || live.size() < 5) {                 // a new record
	snprintf(rec.name, sizeof(rec.name), "rec%08ld", next_name++);
	rec.day = day;
	memcpy(rec.vec, mem.data(), MEMBER);
	FAU_t a = db.Alloc(sizeof(rec));
	if(!a || db.Write(&rec, sizeof(rec), a) != gxDBASE_NO_ERROR) return 1;
	NameKey key(rec.name, a), cmp;
	bt.Insert(key, cmp);
	unsigned t[gxvTagCount] = { 0, 0, day, 0 };
	vix.InsertMember((long long)a, mem.data(), (unsigned)MEMBER, t);
	live.push_back(a);
      }
      else if(r < 9) {                                 // a new vector for a record
	FAU_t a = live[rnd() % live.size()];
	if(db.Read(&rec, sizeof(rec), a) != gxDBASE_NO_ERROR) return 1;
	memcpy(rec.vec, mem.data(), MEMBER);
	rec.day = day;
	if(db.Write(&rec, sizeof(rec), a) != gxDBASE_NO_ERROR) return 1;
	unsigned t[gxvTagCount] = { 0, 0, day, 0 };
	vix.UpdateMember((long long)a, mem.data(), (unsigned)MEMBER, t);
      }
      else {                                           // a deleted record
	size_t i = rnd() % live.size();
	FAU_t a = live[i];
	if(db.Read(&rec, sizeof(rec), a) != gxDBASE_NO_ERROR) return 1;
	NameKey key(rec.name, a), cmp;
	bt.Delete(key, cmp);
	vix.Delete((long long)a);
	db.Delete(a);
	live.erase(live.begin() + (long)i);
      }
    }
    if(j.Commit() != gxDBASE_NO_ERROR) { printf("commit failed\n"); return 1; }
  }
  // Close the journal first (a checkpoint): then the files hold every commit and can be copied
  j.Close();
  bt.Close();
  vix.Close();
  db.Close();
  printf("wrote %ld commits, %zu live records\n", commits, live.size());
  return 0;
}

static int digest(const std::string &dir)
{
  // The records: address and bytes, in address order
  {
    gxDatabase db;
    if(db.Open((dir + "/data.gxd").c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) { printf("digest: no data.gxd\n"); return 1; }
    uint64_t h = FNV0, n = 0;
    std::vector<unsigned char> b;
    for(FAU_t a = db.FindFirstObject(); a != (FAU_t)0; a = db.FindNextObject(a)) {
      __ULWORD__ len = db.ObjectLength(a);
      b.resize(len);
      if(len && db.Read(b.data(), len, a) != gxDBASE_NO_ERROR) { printf("digest: cannot read a record\n"); return 1; }
      uint64_t addr = (uint64_t)a;
      h = fnv(&addr, sizeof(addr), h);
      h = fnv(b.data(), b.size(), h);
      n++;
    }
    db.Close();
    printf("data.gxd objects %016llx %llu records\n", (unsigned long long)h, (unsigned long long)n);
  }
  // The B-tree: its keys in order (the same for any node layout), the example's own key walk
  {
    NameKey k, c;
    gxBtree bt(k, ORDER);
    if(bt.Open((dir + "/by_name.btx").c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) { printf("digest: no by_name.btx\n"); return 1; }
    uint64_t h = FNV0, n = 0;
    for(int ok = bt.FindFirst(k); ok; ok = bt.FindNext(k, c)) { h = fnv(&k.d, sizeof(k.d), h); n++; }
    bt.Close();
    printf("by_name.btx btree %016llx %llu keys\n", (unsigned long long)h, (unsigned long long)n);
  }
  // The vector index: its live nodes (id, vector, tags) in id order
  {
    gxVectorIndex vix;
    if(vix.Open((dir + "/vectors.vix").c_str(), gxDBASE_READONLY) != gxv_NO_ERROR) { printf("digest: no vectors.vix\n"); return 1; }
    unsigned long long nodes = 0, d = vix.LogicalDigest(&nodes);
    printf("vectors.vix logical %016llx %llu nodes\n", d, nodes);
  }
  return 0;
}

int main(int argc, char **argv)
{
  if(argc < 3) {
    fprintf(stderr, "usage: replication_copy write DIR [--commits N] [--per N] [--seed N]\n"
	    "       replication_copy digest DIR\n"
	    "       replication_copy rebuild DIR\n");
    return 64;
  }
  std::string mode = argv[1], dir = argv[2];
  if(mode == "write") {
    long commits = 20, per = 10;
    for(int i = 3; i + 1 < argc; i += 2) {
      std::string a = argv[i];
      if(a == "--commits") commits = atol(argv[i + 1]);
      else if(a == "--per") per = atol(argv[i + 1]);
      else if(a == "--seed") rng ^= (unsigned)strtoul(argv[i + 1], 0, 10) * 2654435761u;
      else return 64;
    }
    return write_records(dir, commits, per);
  }
  if(mode == "digest") return digest(dir);
  if(mode == "rebuild") {
    unsigned long long records = 0, keys = 0;
    std::string why;
    if(rebuild(dir, records, keys, why) != 0) { printf("rebuild: %s\n", why.c_str()); return 1; }
    printf("rebuilt %s: %llu records, %llu keys\n", dir.c_str(), records, keys);
    return 0;
  }
  return 64;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
