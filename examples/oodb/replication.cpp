// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: replication.cpp
// Compiler Used: GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/12/2026
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

Database replication with gxrepl, object by object (DataReel 7.4).

A primary writes records and keeps two indexes made from them, a B-tree
of names and a vector index. gxrepl copies the records' committed pages
to a copy over gxStream (TLS) and never sends the indexes: the copy keeps
its own, updated object by object from the records each commit changed
(derived_apply = incremental), with this program's extractor
("example_records", replication_records.h) built into the copy's gxrepl
(replication_gxrepl). When the copy cannot be sure of an index (a damaged
stamp, a gap in the log, an extractor error), it rebuilds that folder's
indexes in full on its own with "replication rebuild", then goes back to
object by object. A full rebuild on every change is also a set's own
choice: derived_apply = rebuild.

replication_walkthrough.sh runs the whole thing on throwaway folders: a
primary and a copy, writes, the digests on both nodes, a simulated fault
on the copy, the rebuild, and the digests again.

Usage:
  replication write DIR [--commits N] [--per N] [--sleep MS] [--seed N]
        The primary's writer: records in DIR/data.gxd with DIR/by_name.btx
        and DIR/vectors.vix, in one gxdJournal transaction per commit, a
        ship log for gxrepl (object notes on, the two indexes left out)
        and a commit block DIR/commit.bin.
  replication rebuild DIR [LSN]
        The full rebuild of a copy's folder (gxrepl's derived_rebuild):
        both indexes from the records, and DIR/derived.stamp.
  replication digest DIR
        What the folder's indexes hold, whatever their layout: the
        B-tree's keys in order and the vector index's live nodes. Equal
        on the primary and the copy.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <string>
#include <vector>
#include "gxdjrnl.h"
#include "gxdrepl.h"
#include "gxrepl.h"
#include "replication_records.h"

using namespace datareel;
using namespace example;

static unsigned rng = 2026;
static unsigned rnd() { rng = rng * 1103515245u + 12345u; return rng >> 8; }

static int write_records(const std::string &dir, long commits, long per, long sleep_ms)
{
  gxdJournal j;
  gxDatabase db;
  NameKey type;
  gxBtree bt(type, ORDER);
  gxVectorIndex vix;
  if(j.Open((dir + "/writer.jnl").c_str()) != gxDBASE_NO_ERROR) { printf("cannot open the journal\n"); return 1; }
  if(db.Open((dir + "/data.gxd").c_str()) != gxDBASE_NO_ERROR && db.Create((dir + "/data.gxd").c_str()) != gxDBASE_NO_ERROR) return 1;
  if(bt.Open((dir + "/by_name.btx").c_str()) != gxDBASE_NO_ERROR && bt.Create((dir + "/by_name.btx").c_str()) != gxDBASE_NO_ERROR) return 1;
  if(vix.Open((dir + "/vectors.vix").c_str()) != gxv_NO_ERROR &&
     vix.Create((dir + "/vectors.vix").c_str(), DIM, gxvFLOAT32, gxvL2, 8, SPEC, 100) != gxv_NO_ERROR) return 1;
  if(j.Attach(&db) != gxDBASE_NO_ERROR || j.Attach(&bt) != gxDBASE_NO_ERROR || j.Attach(vix.Database()) != gxDBASE_NO_ERROR) return 1;
  // Replication: the ship log, its object notes, and the indexes the copy keeps itself left out
  if(j.EnableShipLog(dir.c_str()) != gxDBASE_NO_ERROR) { printf("cannot open the ship log\n"); return 1; }
  j.EnableObjectNotes(1);
  std::vector<std::string> derived;
  derived.push_back("by_name.btx");
  derived.push_back("vectors.vix");
  j.ShipLog()->SetExclude(derived);
  // The commit block: an odd generation while a commit writes the files
  int cb = open((dir + "/commit.bin").c_str(), O_RDWR | O_CREAT, 0644);
  unsigned char blk[4096];
  memset(blk, 0, sizeof(blk));
  if(pread(cb, blk, sizeof(blk), 0) != (ssize_t)sizeof(blk) && pwrite(cb, blk, sizeof(blk), 0) != (ssize_t)sizeof(blk)) return 1;
  uint64_t gen = 0;
  memcpy(&gen, blk + 8, 8);
  if(gen & 1) gen++;
  std::vector<FAU_t> live;
  for(FAU_t a = db.FindFirstObject(); a != (FAU_t)0; a = db.FindNextObject(a)) live.push_back(a);
  long next_name = (long)live.size() + 1000 * (long)(rng % 97);
  std::vector<float> v(DIM);
  std::vector<unsigned char> mem;
  for(long c = 0; c < commits; c++) {
    uint64_t g = ++gen;
    if(pwrite(cb, &g, 8, 8) != 8) return 1;
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
    // the commit block as it will be once this commit lands goes in the record
    unsigned char nb[4096];
    if(pread(cb, nb, sizeof(nb), 0) != (ssize_t)sizeof(nb)) return 1;
    uint64_t g2 = gen + 1;
    memcpy(nb + 8, &g2, 8);
    j.ShipLog()->SetCommitBlock(dir + "/commit.bin", nb, sizeof(nb));
    if(j.Commit() != gxDBASE_NO_ERROR) { printf("commit failed\n"); return 1; }
    g = ++gen;
    if(pwrite(cb, &g, 8, 8) != 8) return 1;
    if(sleep_ms) usleep((useconds_t)sleep_ms * 1000);
  }
  close(cb);
  j.Close();
  bt.Close();
  vix.Close();
  db.Close();
  printf("wrote %ld commits, %zu live records\n", commits, live.size());
  return 0;
}

static int rebuild_folder(const std::string &dir, unsigned long long lsn)
{
  unsigned long long records = 0, keys = 0;
  std::string why;
  if(rebuild(dir, records, keys, why) != 0) { printf("rebuild: %s\n", why.c_str()); return 1; }
  // derived.stamp: what gxrepl checks against the folder (its commit block's generation, the log record)
  uint64_t g = 0;
  int cb = open((dir + "/commit.bin").c_str(), O_RDONLY);
  if(cb < 0 || pread(cb, &g, 8, 8) != 8) { printf("rebuild: no commit block\n"); return 1; }
  close(cb);
  std::string tmp = dir + "/.derived.stamp.tmp";
  FILE *f = fopen(tmp.c_str(), "w");
  if(!f) return 1;
  fprintf(f, "gen %llu lsn %llu records %llu keys %llu\n", (unsigned long long)g, lsn, records, keys);
  fclose(f);
  if(rename(tmp.c_str(), (dir + "/derived.stamp").c_str()) != 0) return 1;
  printf("rebuilt %s: %llu records, %llu keys\n", dir.c_str(), records, keys);
  return 0;
}

static int digest(const std::string &dir)
{
  uint64_t h = 0, n = 0;
  std::string err;
  if(gxReplDigest(dir + "/by_name.btx", "btree", h, n, err) != 0) { printf("digest: %s\n", err.c_str()); return 1; }
  printf("by_name.btx btree %016llx %llu keys\n", (unsigned long long)h, (unsigned long long)n);
  gxVectorIndex vix;
  if(vix.Open((dir + "/vectors.vix").c_str(), gxDBASE_READONLY) != gxv_NO_ERROR) { printf("digest: no vectors.vix\n"); return 1; }
  unsigned long long nodes = 0, d = vix.LogicalDigest(&nodes);
  printf("vectors.vix logical %016llx %llu nodes\n", d, nodes);
  return 0;
}

int main(int argc, char **argv)
{
  if(argc < 3) {
    fprintf(stderr, "usage: replication write DIR [--commits N] [--per N] [--sleep MS] [--seed N]\n"
	    "       replication rebuild DIR [LSN]\n"
	    "       replication digest DIR\n");
    return 64;
  }
  std::string mode = argv[1], dir = argv[2];
  if(mode == "write") {
    long commits = 20, per = 10, sleep_ms = 0;
    for(int i = 3; i + 1 < argc; i += 2) {
      std::string a = argv[i];
      if(a == "--commits") commits = atol(argv[i + 1]);
      else if(a == "--per") per = atol(argv[i + 1]);
      else if(a == "--sleep") sleep_ms = atol(argv[i + 1]);
      else if(a == "--seed") rng ^= (unsigned)strtoul(argv[i + 1], 0, 10) * 2654435761u;
      else return 64;
    }
    return write_records(dir, commits, per, sleep_ms);
  }
  if(mode == "rebuild") return rebuild_folder(dir, argc > 3 ? strtoull(argv[3], 0, 10) : 0);
  if(mode == "digest") return digest(dir);
  return 64;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
