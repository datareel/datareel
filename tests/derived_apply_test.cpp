// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: derived_apply_test.cpp
// Compiler Used: GCC
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
License along with this library; if not, see
<https://www.gnu.org/licenses/>.

Tests of the per-object derived apply parts (DataReel 7.4): object
notes (gxdonote.h), objects records in the ship log (type 5,
gxdrepl.h), derived index stamps (gxdstamp.h).

  notes_rules       how several changes to one object become one note
  journal_notes     a journal with object notes: adds, updates, removes,
                    a write inside an object, rollback, a B-tree file
  rotation          a segment change before every pair, never between
                    (segment size 1: every objects record and its commit
                    share a segment)
  reader_budget     Read() with any byte budget never ends a batch on an
                    objects record
  orphan            a crash between the two halves: the objects record is
                    an orphan, its commit never written, a gap follows;
                    a reopen writes an open record after it
  mixed_version     a copy that applies commit records only (older code)
                    skips the objects records and matches the source
  damage            damaged objects payloads are refused
  stamps            stamp names, fresh stamps, a stamp in a journal
                    transaction, damaged and missing stamps

Usage: derived_apply_test (in an empty folder)
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include "gxdbase.h"
#include "gxbtree.h"
#include "gxdjrnl.h"
#include "gxdrepl.h"
#include "gxdpcache.h"
#include "gxdstamp.h"
#include "gxdstage.h"

using namespace datareel;
namespace fs = std::filesystem;

static int failures = 0, checks = 0;

namespace datareel {
struct gxdShipLogTest {   // gxdShipLog's friend: the crash between an objects record and its commit
  static void Split(gxdShipLog *l) { l->test_split_pair = 1; }
};
}

#define CHECK(cond, what) do { checks++; if(!(cond)) { \
  std::cerr << "FAIL " << what << " (line " << __LINE__ << ")\n"; \
  failures++; return; } } while(0)

static std::string fresh_dir(const std::string &name)
{
  std::string d = (fs::current_path() / name).string();
  fs::remove_all(d);
  fs::create_directories(d);
  return d;
}

// Every record of a stream, in order
static bool read_all(const std::string &root, const std::string &stream,
		     std::vector<unsigned char> &buf, std::vector<gxdShipRecordView> &recs,
		     size_t budget = 64 * 1024 * 1024)
{
  gxdShipLogReader rd;
  if(rd.Open(root + "/.gxrepl/log/" + stream) != 0) return false;
  buf.clear();
  unsigned long long from = 1;
  for(;;) {
    std::vector<unsigned char> out;
    unsigned long long next = 0;
    unsigned long count = 0;
    int rc = rd.Read(from, budget, out, next, count);
    if(rc != 0) return false;
    if(count == 0) break;
    buf.insert(buf.end(), out.begin(), out.end());
    from = next;
  }
  return gxdParseShipRecords(buf.data(), buf.size(), recs) == 0;
}

static const gxdShipObjectFile *find_file(const gxdShipObjectsRecord &o, const std::string &name)
{
  for(auto &f : o.files) if(f.name == name) return &f;
  return 0;
}

// --------------------------------------------------------------

static void notes_rules()
{
  gxdObjectNotes n;
  n.Add(100, 40); n.Update(100, 40);
  CHECK(n.notes.size() == 1 && n.notes[100].op == gxdObjectAdd && n.complete, "add + update = add");
  n.Remove(100, 40);
  CHECK(n.notes.empty() && n.complete, "add + remove = nothing");
  n.Update(200, 32); n.Update(200, 32);
  CHECK(n.notes[200].op == gxdObjectUpdate && n.notes[200].old_len == 32, "update + update = update");
  n.Remove(200, 32);
  CHECK(n.notes[200].op == gxdObjectRemove && n.notes[200].old_len == 32 && n.notes[200].new_len == 0,
	"update + remove = remove");
  n.Add(200, 48);
  CHECK(n.notes[200].op == gxdObjectUpdate && n.notes[200].old_len == 32 && n.notes[200].new_len == 48,
	"remove + add (reused in place) = update");
  CHECK(n.Covers(200, 48) && n.Covers(210, 10) && !n.Covers(210, 40) && !n.Covers(199, 1), "covers");
  n.Remove(300, 16);
  CHECK(!n.Covers(300, 1), "a removed object covers nothing");
  n.Update(300, 16);
  CHECK(!n.complete, "an update of a removed object is incomplete");
  n.Clear();
  n.Add(400, 8); n.Add(400, 8);
  CHECK(!n.complete, "a second add is incomplete");
}

// A data file and a B-tree in a set, with a journal shipping with notes
struct Key : public DatabaseKeyB {
  struct D { char k[16]; FAU addr; } d;
  Key(long id = -1, FAU_t a = 0) : DatabaseKeyB((char *)&d) {
    memset(&d, 0, sizeof(d));
    if(id >= 0) snprintf(d.k, sizeof(d.k), "%015ld", id);
    d.addr = a;
  }
  size_t KeySize() { return sizeof(d); }
  int CompareKey(const DatabaseKeyB &o) const { return memcmp(d.k, ((const D *)o.db_key)->k, 16); }
  int operator==(const DatabaseKeyB &o) const { return CompareKey(o) == 0; }
  int operator>(const DatabaseKeyB &o) const { return CompareKey(o) > 0; }
};

struct SetDb {
  std::string root;
  gxDatabase db;
  Key type;
  gxBtree *btx = 0;
  gxdJournal j;
  bool Open(const std::string &r, bool with_btree, bool notes = true) {
    root = r;
    if(j.Open((root + "/data.jnl").c_str()) != gxDBASE_NO_ERROR) return false;
    if(db.Create((root + "/data.gxd").c_str()) != gxDBASE_NO_ERROR) return false;
    if(j.Attach(&db, 256 * 1024) != gxDBASE_NO_ERROR) return false;
    if(with_btree) {
      btx = new gxBtree(type, 8);
      if(btx->Create((root + "/index.btx").c_str()) != gxDBASE_NO_ERROR) return false;
      if(j.Attach(btx, 256 * 1024) != gxDBASE_NO_ERROR) return false;
    }
    if(j.EnableShipLog(root.c_str(), "s1") != gxDBASE_NO_ERROR) return false;
    if(notes && j.EnableObjectNotes(1) != gxDBASE_NO_ERROR) return false;
    return true;
  }
  void Close() {
    j.Close();
    if(btx) { btx->Close(); delete btx; btx = 0; }
    db.Close();
  }
  ~SetDb() { Close(); }
};

static void journal_notes()
{
  std::string root = fresh_dir("da_notes");
  SetDb s;
  CHECK(s.Open(root, true), "open the set");
  char rec[64];
  FAU_t a[4];
  // 1: three adds (and B-tree inserts)
  CHECK(s.j.Begin() == gxDBASE_NO_ERROR, "begin 1");
  for(int i = 0; i < 3; i++) {
    memset(rec, 'a' + i, sizeof(rec));
    a[i] = s.db.Alloc(sizeof(rec));
    CHECK(a[i] && s.db.Write(rec, sizeof(rec), a[i]) == gxDBASE_NO_ERROR, "add");
    Key k(i, a[i]), cmp;
    CHECK(s.btx->Insert(k, cmp), "btree insert");
  }
  CHECK(s.j.Commit() == gxDBASE_NO_ERROR, "commit 1");
  // 2: an update at an object's start, a remove
  CHECK(s.j.Begin() == gxDBASE_NO_ERROR, "begin 2");
  memset(rec, 'z', sizeof(rec));
  CHECK(s.db.Write(rec, sizeof(rec), a[0]) == gxDBASE_NO_ERROR, "update");
  CHECK(s.db.Delete(a[1]) != 0, "delete");
  CHECK(s.j.Commit() == gxDBASE_NO_ERROR, "commit 2");
  // 3: a write inside an object not noted yet: incomplete
  CHECK(s.j.Begin() == gxDBASE_NO_ERROR, "begin 3");
  CHECK(s.db.Write(rec, 8, a[2] + 4) == gxDBASE_NO_ERROR, "write inside");
  CHECK(s.j.Commit() == gxDBASE_NO_ERROR, "commit 3");
  // 4: a rolled back add, then an add that commits
  CHECK(s.j.Begin() == gxDBASE_NO_ERROR, "begin 4");
  CHECK(s.db.Alloc(sizeof(rec)) != 0, "add rolled back");
  CHECK(s.j.Rollback() == gxDBASE_NO_ERROR, "rollback");
  CHECK(s.j.Begin() == gxDBASE_NO_ERROR, "begin 5");
  a[3] = s.db.Alloc(sizeof(rec));
  CHECK(a[3] && s.db.Write(rec, sizeof(rec), a[3]) == gxDBASE_NO_ERROR, "add 5");
  CHECK(s.j.Commit() == gxDBASE_NO_ERROR, "commit 5");
  s.Close();

  std::vector<unsigned char> buf;
  std::vector<gxdShipRecordView> recs;
  CHECK(read_all(root, "s1", buf, recs), "read the log");
  std::vector<gxdShipObjectsRecord> objs;
  for(size_t i = 0; i < recs.size(); i++) {
    if(recs[i].type == gxdShipObjects) {
      CHECK(i + 1 < recs.size() && (recs[i + 1].type == gxdShipCommit || recs[i + 1].type == gxdShipCommitZ) &&
	    recs[i + 1].lsn == recs[i].lsn + 1, "an objects record is followed by its commit");
      CHECK(!gxdShipObjectsOrphan(recs, i), "not an orphan");
    }
    if(recs[i].type == gxdShipCommit || recs[i].type == gxdShipCommitZ) {
      gxdShipObjectsRecord o;
      CHECK(gxdShipObjectsFor(recs, i, o) == 1, "every commit has its objects record");
      objs.push_back(o);
    }
  }
  CHECK(objs.size() == 4, "four commits (" << objs.size() << ")");
  const gxdShipObjectFile *f = find_file(objs[0], "data.gxd");
  CHECK(f && f->complete && f->objects.size() == 3, "commit 1: three objects");
  for(int i = 0; i < 3; i++)
    CHECK(f->objects[i].address == (unsigned long long)a[i] && f->objects[i].op == gxdObjectAdd &&
	  f->objects[i].old_len == 0 && f->objects[i].new_len >= sizeof(rec), "commit 1: add " << i);
  const gxdShipObjectFile *b = find_file(objs[0], "index.btx");
  CHECK(b && b->complete, "commit 1: the B-tree file's notes are complete");
  CHECK(objs[0].complete, "commit 1: complete");
  f = find_file(objs[1], "data.gxd");
  CHECK(f && f->complete && f->objects.size() == 2, "commit 2: two objects");
  CHECK(f->objects[0].address == (unsigned long long)a[0] && f->objects[0].op == gxdObjectUpdate &&
	f->objects[0].old_len == f->objects[0].new_len, "commit 2: update");
  CHECK(f->objects[1].address == (unsigned long long)a[1] && f->objects[1].op == gxdObjectRemove &&
	f->objects[1].new_len == 0 && f->objects[1].old_len >= sizeof(rec), "commit 2: remove");
  f = find_file(objs[2], "data.gxd");
  CHECK(f && !f->complete && !objs[2].complete && f->objects.empty(), "commit 3: incomplete");
  f = find_file(objs[3], "data.gxd");
  CHECK(f && f->complete && f->objects.size() == 1 && f->objects[0].op == gxdObjectAdd &&
	f->objects[0].address == (unsigned long long)a[3], "commit 5: one add (the rolled back one is gone)");
}

// Every revision letter: a noted write lands where it was asked to, and a
// write that follows it (the current position) too
static void write_position()
{
  const char revs[] = { 'A', 'B', 'C', 'D', 'E' };
  for(char rv : revs) {
    std::string root = fresh_dir(std::string("da_pos_") + rv);
    gxdJournal j;
    gxDatabase db;
    CHECK(j.Open((root + "/p.jnl").c_str()) == gxDBASE_NO_ERROR, "journal");
    CHECK(db.Create((root + "/p.gxd").c_str(), (FAU_t)0, (__SBYTE__)rv) == gxDBASE_NO_ERROR, "create rev " << rv);
    CHECK(j.Attach(&db, 64 * 1024) == gxDBASE_NO_ERROR && j.EnableObjectNotes(1) == gxDBASE_NO_ERROR, "attach");
    char a1[40], a2[24], back[64];
    memset(a1, '1', sizeof(a1)); memset(a2, '2', sizeof(a2));
    CHECK(j.Begin() == gxDBASE_NO_ERROR, "begin");
    FAU_t at = db.Alloc(64);
    CHECK(at, "alloc");
    CHECK(j.Commit() == gxDBASE_NO_ERROR, "commit");
    CHECK(j.Begin() == gxDBASE_NO_ERROR, "begin 2");
    CHECK(db.Write(a1, sizeof(a1), at) == gxDBASE_NO_ERROR && db.Write(a2, sizeof(a2)) == gxDBASE_NO_ERROR, "writes");
    CHECK(db.ObjectNotes() && db.ObjectNotes()->complete && db.ObjectNotes()->notes.size() == 1 &&
	  db.ObjectNotes()->notes.begin()->second.op == gxdObjectUpdate, "rev " << rv << ": one update");
    CHECK(j.Commit() == gxDBASE_NO_ERROR, "commit 2");
    CHECK(db.Read(back, sizeof(back), at) == gxDBASE_NO_ERROR, "read");
    CHECK(memcmp(back, a1, sizeof(a1)) == 0 && memcmp(back + sizeof(a1), a2, sizeof(a2)) == 0,
	  "rev " << rv << ": the bytes are where they were written");
    j.Close();
    db.Close();
  }
}

static void rotation()
{
  std::string root = fresh_dir("da_rotate");
  SetDb s;
  CHECK(s.Open(root, false), "open");
  s.j.ShipLog()->SetSegmentSize(1);   // A new segment before every append
  char rec[32];
  memset(rec, 'r', sizeof(rec));
  for(int c = 0; c < 20; c++) {
    CHECK(s.j.Begin() == gxDBASE_NO_ERROR, "begin");
    FAU_t at = s.db.Alloc(sizeof(rec));
    CHECK(at && s.db.Write(rec, sizeof(rec), at) == gxDBASE_NO_ERROR, "add");
    CHECK(s.j.Commit() == gxDBASE_NO_ERROR, "commit");
  }
  s.Close();
  // Every segment: the open record alone, or an objects record and its commit
  int pairs = 0, segs = 0;
  for(auto &e : fs::directory_iterator(root + "/.gxrepl/log/s1")) {
    std::string n = e.path().filename().string();
    if(n.size() != 20 || n.substr(16) != ".gxl") continue;
    segs++;
    std::ifstream in(e.path(), std::ios::binary);
    std::vector<unsigned char> b((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(b.size() >= gxdShipSegmentHeaderSize, "segment header");
    std::vector<gxdShipRecordView> recs;
    CHECK(gxdParseShipRecords(b.data() + gxdShipSegmentHeaderSize, b.size() - gxdShipSegmentHeaderSize, recs) == 0,
	  "segment records");
    CHECK(!recs.empty(), "a segment with records");
    if(recs.size() == 1) { CHECK(recs[0].type == gxdShipOpen, "a lone record is an open record"); continue; }
    CHECK(recs.size() == 2 && recs[0].type == gxdShipObjects &&
	  (recs[1].type == gxdShipCommit || recs[1].type == gxdShipCommitZ) && recs[1].lsn == recs[0].lsn + 1,
	  "segment " << n << " holds one pair");
    pairs++;
  }
  CHECK(pairs == 20 && segs >= 21, "20 pairs, each in its own segment (" << pairs << ", " << segs << ")");
}

static void reader_budget()
{
  std::string root = fresh_dir("da_budget");
  {
    SetDb s;
    CHECK(s.Open(root, false), "open");
    s.j.ShipLog()->SetSegmentSize(16 * 1024);
    char rec[200];
    for(int c = 0; c < 40; c++) {
      memset(rec, c, sizeof(rec));
      CHECK(s.j.Begin() == gxDBASE_NO_ERROR, "begin");
      for(int k = 0; k <= c % 4; k++) {
	FAU_t at = s.db.Alloc(sizeof(rec));
	CHECK(at && s.db.Write(rec, sizeof(rec), at) == gxDBASE_NO_ERROR, "add");
      }
      CHECK(s.j.Commit() == gxDBASE_NO_ERROR, "commit");
    }
  }
  std::vector<unsigned char> all;
  std::vector<gxdShipRecordView> ref;
  CHECK(read_all(root, "s1", all, ref), "read all");
  // A record bigger than the budget is never read (status 2, the copy
  // resyncs): budgets start at the biggest record
  size_t big = 0;
  for(auto &r : ref) {
    size_t c = r.len;   // A compressed commit counts at its inflated size
    if(r.type == gxdShipCommitZ && r.len >= 8) { uint32_t raw; memcpy(&raw, r.payload, 4); if(raw > c) c = raw; }
    if(c + gxdShipRecordHeaderSize > big) big = c + gxdShipRecordHeaderSize;
  }
  const size_t budgets[] = { big, big + 1, big + 40, big * 3 / 2, big * 2, big * 2 + 7, big * 3, big * 10 };
  for(size_t bud : budgets) {
    gxdShipLogReader rd;
    CHECK(rd.Open(root + "/.gxrepl/log/s1") == 0, "open reader");
    unsigned long long from = 1;
    size_t got = 0;
    for(int guard = 0; guard < 10000; guard++) {
      std::vector<unsigned char> out;
      unsigned long long next = 0;
      unsigned long count = 0;
      int rc = rd.Read(from, bud, out, next, count);
      CHECK(rc == 0, "read budget " << bud << " rc " << rc);
      if(count == 0) break;
      std::vector<gxdShipRecordView> recs;
      CHECK(gxdParseShipRecords(out.data(), out.size(), recs) == 0 && recs.size() == count, "batch parses");
      CHECK(recs.back().type != gxdShipObjects, "budget " << bud << ": a batch ends on an objects record");
      for(size_t i = 0; i < recs.size(); i++)
	if(recs[i].type == gxdShipObjects)
	  CHECK(i + 1 < recs.size() && recs[i + 1].lsn == recs[i].lsn + 1, "pair in one batch");
      got += count;
      from = next;
    }
    CHECK(got == ref.size(), "budget " << bud << ": every record (" << got << " of " << ref.size() << ")");
  }
}

static void orphan()
{
  std::string root = fresh_dir("da_orphan");
  char rec[48];
  memset(rec, 'o', sizeof(rec));
  {
    SetDb s;
    CHECK(s.Open(root, false), "open");
    for(int c = 0; c < 3; c++) {
      CHECK(s.j.Begin() == gxDBASE_NO_ERROR, "begin");
      FAU_t at = s.db.Alloc(sizeof(rec));
      CHECK(at && s.db.Write(rec, sizeof(rec), at) == gxDBASE_NO_ERROR, "add");
      if(c == 1) gxdShipLogTest::Split(s.j.ShipLog());   // The commit record after it is never written
      CHECK(s.j.Commit() == gxDBASE_NO_ERROR, "commit (a failed append never fails the commit)");
    }
  }
  {
    // The writer again, as after a crash: an open record
    SetDb s;
    CHECK(s.j.Open((root + "/data.jnl").c_str()) == gxDBASE_NO_ERROR, "reopen journal");
    CHECK(s.db.Open((root + "/data.gxd").c_str()) == gxDBASE_NO_ERROR, "reopen data");
    CHECK(s.j.Attach(&s.db, 256 * 1024) == gxDBASE_NO_ERROR, "attach");
    CHECK(s.j.EnableShipLog(root.c_str(), "s1") == gxDBASE_NO_ERROR, "ship log");
    // With the reopen only: the orphan is the log's last whole record but one
    std::vector<unsigned char> b0;
    std::vector<gxdShipRecordView> r0;
    CHECK(read_all(root, "s1", b0, r0), "read after reopen");
  }
  std::vector<unsigned char> buf;
  std::vector<gxdShipRecordView> recs;
  CHECK(read_all(root, "s1", buf, recs), "read");
  // open(1), pair(2,3), orphan objects(4), gap(5), pair(6,7), open(8)
  std::vector<uint32_t> want = { gxdShipOpen, gxdShipObjects, gxdShipCommit, gxdShipObjects, gxdShipGap,
				 gxdShipObjects, gxdShipCommit, gxdShipOpen };
  CHECK(recs.size() == want.size(), "eight records (" << recs.size() << ")");
  for(size_t i = 0; i < want.size(); i++) {
    uint32_t t = recs[i].type == gxdShipCommitZ ? gxdShipCommit : recs[i].type;
    CHECK(t == want[i], "record " << i << " type " << recs[i].type);
    CHECK(recs[i].lsn == i + 1, "lsn " << i + 1);
  }
  CHECK(gxdShipObjectsOrphan(recs, 3) == 1, "the objects record without its commit is an orphan");
  CHECK(gxdShipObjectsOrphan(recs, 1) == 0 && gxdShipObjectsOrphan(recs, 5) == 0, "the others are not");
  gxdShipObjectsRecord o;
  CHECK(gxdShipObjectsFor(recs, 6, o) == 1 && o.lsn == 6, "the commit after the gap has its own");
  CHECK(gxdShipObjectsFor(recs, 2, o) == 1 && o.lsn == 2, "the first commit has its own");

  // An objects record that is the log's last record is held back by Read()
  std::string r2 = fresh_dir("da_orphan_tail");
  {
    SetDb s;
    CHECK(s.Open(r2, false), "open tail");
    CHECK(s.j.Begin() == gxDBASE_NO_ERROR, "begin");
    FAU_t at = s.db.Alloc(sizeof(rec));
    CHECK(at && s.db.Write(rec, sizeof(rec), at) == gxDBASE_NO_ERROR, "add");
    gxdShipLogTest::Split(s.j.ShipLog());
    CHECK(s.j.Commit() == gxDBASE_NO_ERROR, "commit");
    gxdShipLogReader rd;
    CHECK(rd.Open(r2 + "/.gxrepl/log/s1") == 0, "reader");
    std::vector<unsigned char> out;
    unsigned long long next = 0;
    unsigned long count = 0;
    CHECK(rd.Read(1, 1 << 20, out, next, count) == 0 && count == 1 && next == 2, "only the open record");
    CHECK(rd.Read(2, 1 << 20, out, next, count) == 0 && count == 0 && next == 2,
	  "a trailing objects record is held back");
    gxdShipLogReader::Info info;
    CHECK(rd.GetInfo(info) == 0 && info.next_lsn == 2, "GetInfo agrees with Read (next " << info.next_lsn << ")");
    // 11/14/2026 (review: the stall): once it is 10 s old, its commit never comes: it is let go as an orphan
    std::cout << "     (orphan_tail: waiting 11 s for the hold to end)\n";
    sleep(11);
    CHECK(rd.GetInfo(info) == 0 && info.next_lsn == 3, "GetInfo counts the old orphan (next " << info.next_lsn << ")");
    CHECK(rd.Read(2, 1 << 20, out, next, count) == 0 && count == 1 && next == 3, "the old orphan is read");
    std::vector<gxdShipRecordView> rv;
    CHECK(gxdParseShipRecords(out.data(), out.size(), rv) == 0 && rv.size() == 1 && rv[0].type == gxdShipObjects,
	  "it is the objects record");
  }
}

static void mixed_version()
{
  // The source: adds, updates, removes, with notes
  std::string src = fresh_dir("da_mixed_src"), dst = fresh_dir("da_mixed_dst");
  {
    SetDb s;
    CHECK(s.Open(src, true), "open");
    // The copy starts from the files as made (a first sync)
    s.db.Flush();
    s.btx->gxDatabasePtr()->Flush();
    fs::copy_file(src + "/data.gxd", dst + "/data.gxd");
    fs::copy_file(src + "/index.btx", dst + "/index.btx");
    std::vector<FAU_t> live;
    char rec[96];
    for(int c = 0; c < 30; c++) {
      CHECK(s.j.Begin() == gxDBASE_NO_ERROR, "begin");
      memset(rec, c, sizeof(rec));
      FAU_t at = s.db.Alloc(sizeof(rec));
      CHECK(at && s.db.Write(rec, sizeof(rec), at) == gxDBASE_NO_ERROR, "add");
      Key k(c, at), cmp;
      CHECK(s.btx->Insert(k, cmp), "insert");
      live.push_back(at);
      if(c % 3 == 2) CHECK(s.db.Write(rec, sizeof(rec), live[c / 2]) == gxDBASE_NO_ERROR, "update");
      if(c % 5 == 4) { CHECK(s.db.Delete(live[c - 4]) != 0, "remove"); }
      CHECK(s.j.Commit() == gxDBASE_NO_ERROR, "commit");
    }
  }
  // The copy, as a copy on older code does it: commit records only
  std::vector<unsigned char> buf;
  std::vector<gxdShipRecordView> recs;
  CHECK(read_all(src, "s1", buf, recs), "read");
  int skipped = 0, applied = 0;
  for(auto &r : recs) {
    if(r.type != gxdShipCommit && r.type != gxdShipCommitZ) { if(r.type == gxdShipObjects) skipped++; continue; }
    std::vector<unsigned char> storage;
    std::vector<gxdShipCommitFile> files;
    CHECK(gxdParseShipCommitRecord(r, storage, files) == 0, "commit parses");
    std::vector<gxdApplyFile> af;
    for(auto &f : files) {
      gxdApplyFile a;
      a.path = dst + "/" + f.name;
      a.size = f.size;
      for(auto &p : f.pages) a.pages[p.first] = p.second;
      af.push_back(a);
    }
    CHECK(gxdJournal::ApplyPages((dst + "/apply.jnl").c_str(), af) == gxDBASE_NO_ERROR, "apply");
    applied++;
  }
  CHECK(skipped == 30 && applied == 30, "30 objects records skipped, 30 commits applied");
  for(const char *n : { "data.gxd", "index.btx" }) {
    std::ifstream a(src + "/" + n, std::ios::binary), b(dst + "/" + n, std::ios::binary);
    std::vector<char> va((std::istreambuf_iterator<char>(a)), std::istreambuf_iterator<char>());
    std::vector<char> vb((std::istreambuf_iterator<char>(b)), std::istreambuf_iterator<char>());
    CHECK(!va.empty() && va == vb, std::string(n) + ": the copy matches the source");
  }
}

static void put32(std::vector<unsigned char> &v, uint32_t x) { unsigned char b[4]; memcpy(b, &x, 4); v.insert(v.end(), b, b + 4); }
static void put64(std::vector<unsigned char> &v, uint64_t x) { unsigned char b[8]; memcpy(b, &x, 8); v.insert(v.end(), b, b + 8); }

static std::vector<unsigned char> objects_payload(uint32_t format, uint32_t flags, unsigned char op,
						  uint64_t a1, uint64_t a2, int truncate = 0)
{
  std::vector<unsigned char> p;
  put32(p, format); put32(p, flags);
  put32(p, 1);
  put32(p, 8); p.insert(p.end(), (const unsigned char *)"data.gxd", (const unsigned char *)"data.gxd" + 8);
  put32(p, 1); put32(p, 2);
  put64(p, a1); p.push_back(op); put32(p, 0); put32(p, 10);
  put64(p, a2); p.push_back(op); put32(p, 0); put32(p, 10);
  if(truncate) p.resize(p.size() - truncate);
  return p;
}

static void damage()
{
  gxdShipRecordView v;
  v.type = gxdShipObjects;
  v.lsn = 7;
  gxdShipObjectsRecord o;
  auto good = objects_payload(1, 1, gxdObjectAdd, 100, 200);
  v.payload = good.data(); v.len = (uint32_t)good.size();
  CHECK(gxdParseShipObjectsRecord(v, o) == 0 && o.complete && o.files.size() == 1 && o.files[0].objects.size() == 2 &&
	o.lsn == 7, "a good record");
  auto bad_format = objects_payload(2, 1, gxdObjectAdd, 100, 200);
  v.payload = bad_format.data(); v.len = (uint32_t)bad_format.size();
  CHECK(gxdParseShipObjectsRecord(v, o) == -1, "an unknown format");
  auto bad_op = objects_payload(1, 1, 9, 100, 200);
  v.payload = bad_op.data(); v.len = (uint32_t)bad_op.size();
  CHECK(gxdParseShipObjectsRecord(v, o) == -1, "a bad op");
  auto unordered = objects_payload(1, 1, gxdObjectAdd, 200, 100);
  v.payload = unordered.data(); v.len = (uint32_t)unordered.size();
  CHECK(gxdParseShipObjectsRecord(v, o) == -1, "addresses not ascending");
  auto repeat = objects_payload(1, 1, gxdObjectAdd, 100, 100);
  v.payload = repeat.data(); v.len = (uint32_t)repeat.size();
  CHECK(gxdParseShipObjectsRecord(v, o) == -1, "a repeated address");
  for(int t = 1; t < 30; t += 3) {
    auto cut = objects_payload(1, 1, gxdObjectAdd, 100, 200, t);
    v.payload = cut.data(); v.len = (uint32_t)cut.size();
    CHECK(gxdParseShipObjectsRecord(v, o) == -1 && o.files.empty() && !o.complete, "truncated by " << t);
  }
  auto extra = good;
  extra.push_back(0);
  v.payload = extra.data(); v.len = (uint32_t)extra.size();
  CHECK(gxdParseShipObjectsRecord(v, o) == -1, "a trailing byte");
  auto z = objects_payload(1, 1 | 2, gxdObjectAdd, 100, 200);   // Says zlib, is not
  v.payload = z.data(); v.len = (uint32_t)z.size();
  CHECK(gxdParseShipObjectsRecord(v, o) == -1, "a bad zlib stream");
  v.type = gxdShipCommit;
  v.payload = good.data(); v.len = (uint32_t)good.size();
  CHECK(gxdParseShipObjectsRecord(v, o) == -1, "not an objects record");
}

static void stamps()
{
  CHECK(gxdDerivedStamp::IsStamp("by_symbol.btx.stamp") && gxdDerivedStamp::IsStamp("a/b/x.vix.stamp"), "stamp names");
  CHECK(!gxdDerivedStamp::IsStamp("derived.stamp") && !gxdDerivedStamp::IsStamp(".stamp") &&
	!gxdDerivedStamp::IsStamp("x.stamps") && !gxdDerivedStamp::IsStamp("x.btx"), "not stamps");
  CHECK(gxdDerivedStamp::PathFor("d/by_x.btx") == "d/by_x.btx.stamp", "PathFor");
  std::string d = fresh_dir("da_stamps");
  gxdStampData sd, rd;
  CHECK(gxdDerivedStamp::ReadFile(d + "/none.stamp", rd) == 1, "no stamp");
  sd.log_id = "0123456789abcdef"; sd.lsn = 42; sd.records = 7; sd.digest_kind = gxdStampDigestBytes;
  CHECK(gxdDerivedStamp::WriteFresh(d + "/i.vix.stamp", sd) == 0, "fresh stamp");
  CHECK(gxdDerivedStamp::ReadFile(d + "/i.vix.stamp", rd) == 0 && rd.log_id == sd.log_id && rd.lsn == 42 &&
	rd.records == 7 && rd.digest_kind == gxdStampDigestBytes, "read it back");
  CHECK(!fs::exists(d + "/i.vix.stamp.tmp"), "no temporary left");
  sd.lsn = 43;
  CHECK(gxdDerivedStamp::WriteFresh(d + "/i.vix.stamp", sd) == 0 &&
	gxdDerivedStamp::ReadFile(d + "/i.vix.stamp", rd) == 0 && rd.lsn == 43, "replaced");
  // Damaged: one byte of the record changed
  {
    std::fstream f(d + "/i.vix.stamp", std::ios::in | std::ios::out | std::ios::binary);
    gxDatabase probe;
    CHECK(probe.Open((d + "/i.vix.stamp").c_str(), gxDBASE_READONLY) == gxDBASE_NO_ERROR, "probe");
    std::streamoff at = (std::streamoff)probe.FileHeaderSize() + 33;
    probe.Close();
    f.seekg(at); char c = 0; f.read(&c, 1); c ^= 0x40; f.seekp(at); f.write(&c, 1);
  }
  CHECK(gxdDerivedStamp::ReadFile(d + "/i.vix.stamp", rd) == -1, "a damaged stamp");
  std::ofstream(d + "/junk.stamp") << "not a database";
  CHECK(gxdDerivedStamp::ReadFile(d + "/junk.stamp", rd) == -1, "not a stamp file");

  // In a journal transaction with its index: a rollback leaves the old stamp
  gxdJournal j;
  gxDatabase ix;
  gxdDerivedStamp st;
  CHECK(j.Open((d + "/ix.jnl").c_str()) == gxDBASE_NO_ERROR, "journal");
  CHECK(ix.Create((d + "/by_x.btx").c_str()) == gxDBASE_NO_ERROR, "index file");
  CHECK(st.Open(gxdDerivedStamp::PathFor(d + "/by_x.btx").c_str()) == gxDBASE_NO_ERROR, "open stamp");
  CHECK(st.Read(rd) == 0 && rd.lsn == 0, "a new stamp says lsn 0");
  CHECK(j.Attach(&ix, 64 * 1024) == gxDBASE_NO_ERROR && j.Attach(st.File(), 64 * 1024) == gxDBASE_NO_ERROR, "attach");
  sd.lsn = 100;
  CHECK(j.Begin() == gxDBASE_NO_ERROR && st.Write(sd) == gxDBASE_NO_ERROR && j.Commit(1) == gxDBASE_NO_ERROR, "stamp 100");
  sd.lsn = 101;
  CHECK(j.Begin() == gxDBASE_NO_ERROR && st.Write(sd) == gxDBASE_NO_ERROR && j.Rollback() == gxDBASE_NO_ERROR, "stamp 101 rolled back");
  CHECK(st.Read(rd) == 0 && rd.lsn == 100, "the stamp says 100 after the rollback");
  j.Close();
  st.Close();
  ix.Close();
  CHECK(gxdDerivedStamp::ReadFile(gxdDerivedStamp::PathFor(d + "/by_x.btx"), rd) == 0 && rd.lsn == 100, "on disk: 100");
}

// 11/14/2026 (review 3): a staged scope used for several indexes, Rollback() between them: a file closed with
// changes after a Rollback() is still captured (before, Rollback() ended the scope and its changes went without a
// word); a file opened again after it was closed with changes fails the capture; nothing reaches the files
static bool file_bytes(const std::string &p, std::string &out)
{
  std::ifstream f(p, std::ios::binary);
  if(!f) return false;
  out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  return true;
}

static void staged_scope()
{
  std::string d = fresh_dir("da_scope");
  std::string pa = d + "/a.btx", pb = d + "/b.btx";
  FAU_t aa = 0, ab = 0;
  char rec[64];
  memset(rec, 'x', sizeof(rec));
  for(auto *p : { &pa, &pb }) {
    gxDatabase f;
    CHECK(f.Create(p->c_str()) == gxDBASE_NO_ERROR, "create " << *p);
    FAU_t a = f.Alloc(sizeof(rec));
    CHECK(a && f.Write(rec, sizeof(rec), a) == gxDBASE_NO_ERROR, "write " << *p);
    (p == &pa ? aa : ab) = a;
    f.Close();
  }
  std::string a0, b0, now;
  CHECK(file_bytes(pa, a0) && file_bytes(pb, b0), "read the files");
  memset(rec, 'y', sizeof(rec));
  {
    gxdStagedScope scope;
    std::vector<gxdStagedFile> cap;
    // index 1: open, change, capture while open, roll back
    {
      gxDatabase f;
      CHECK(f.Open(pa.c_str()) == gxDBASE_NO_ERROR, "open a");
      CHECK(f.Write(rec, sizeof(rec), aa) == gxDBASE_NO_ERROR, "change a");
      CHECK(scope.Capture(cap) == 0 && cap.size() == 1 && cap[0].path == pa, "a captured while open");
      scope.Rollback();
      f.Close();
    }
    CHECK(scope.Capture(cap) == 0 && cap.empty(), "nothing after the rollback (a closed with no changes)");
    // index 2, after a Rollback(): changed and closed before the capture: still captured
    {
      gxDatabase f;
      CHECK(f.Open(pb.c_str()) == gxDBASE_NO_ERROR, "open b");
      CHECK(f.Write(rec, sizeof(rec), ab) == gxDBASE_NO_ERROR, "change b");
      f.Close();
    }
    CHECK(scope.Capture(cap) == 0 && cap.size() == 1 && cap[0].path == pb && !cap[0].page_no.empty(),
	  "b closed with changes after a rollback is captured (" << cap.size() << ")");
    // b opened again in the scope after it was closed with changes: its reads would miss them
    {
      gxDatabase f;
      CHECK(f.Open(pb.c_str()) == gxDBASE_NO_ERROR, "open b again");
      char back[64];
      CHECK(f.Read(back, sizeof(back), ab) == gxDBASE_NO_ERROR, "read b");
      CHECK(scope.Capture(cap) == -1, "opened again after a close with changes: the capture fails");
      f.Close();
    }
    scope.Rollback();
    CHECK(scope.Capture(cap) == 0 && cap.empty(), "the next index starts clean");
  }
  CHECK(file_bytes(pa, now) && now == a0, "a on disk unchanged");
  CHECK(file_bytes(pb, now) && now == b0, "b on disk unchanged");
}

int main()
{
  struct T { const char *name; void (*fn)(); } tests[] = {
    { "notes_rules", notes_rules }, { "journal_notes", journal_notes }, { "write_position", write_position }, { "rotation", rotation },
    { "reader_budget", reader_budget }, { "orphan", orphan }, { "mixed_version", mixed_version },
    { "damage", damage }, { "stamps", stamps }, { "staged_scope", staged_scope },
  };
  for(auto &t : tests) {
    int before = failures;
    t.fn();
    std::cout << (failures == before ? "ok   " : "FAIL ") << t.name << "\n";
  }
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
