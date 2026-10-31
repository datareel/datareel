// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: journal_test.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 10/31/2026
// Date Last Modified: 10/31/2026
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

Tests for the DataReel 7.2 page cache and journal (gxdpcache.h,
gxdjrnl.h), run by tests/run_tests.sh and wintest.bat:

  same_bytes   The same inserts and deletes without a cache, with the
               page cache and with the journal leave byte-identical
               data and index files.
  commit       Committed batches are in the files after a reopen
               without the journal.
  rollback     Rollback() drops new records and restores changed ones,
               with and without a BtreeCache.
  crash        A copy of the files taken at any point (what the
               operating system holds when the program is killed)
               recovers to the last commit, or the one after it.
  torn_journal The journal cut short anywhere after the last synced
               commit recovers to a whole batch, never a part of one.
  torn_ckpt    Data and index files that hold any mix of pages from
               before and after a checkpoint, with any file size in
               between, recover to the state after the checkpoint.

Each record is a 16 byte key and 100 bytes of data; the database is
correct if the B-tree holds keys 0 to n-1 in order, with n a multiple
of the batch size, and every key finds the right data.

Usage: journal_test [rounds] [seed]
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include "gxbtree.h"
#include "btcache.h"
#include "gxdjrnl.h"

using namespace datareel;
namespace fs = std::filesystem;

static const size_t KSIZE = 16;
static const size_t VSIZE = 100;
static const int BATCH = 200;
static int failures = 0;
static gxdJournalWriteMode g_mode = gxdJOURNAL_WRITE_THROUGH;
static std::string mode_name() {
  return g_mode == gxdJOURNAL_WRITE_THROUGH ? "write-through" : "write-back";
}

#define CHECK(cond, what) do { if(!(cond)) { \
  std::cerr << "FAIL " << what << " (line " << __LINE__ << ")\n"; \
  failures++; return; } } while(0)

struct KeyData { char k[KSIZE]; FAU addr; };

class Key : public DatabaseKeyB
{
public:
  Key(long id = -1, FAU_t a = 0) : DatabaseKeyB((char *)&d) { Set(id, a); }
  void Set(long id, FAU_t a = 0) {
    memset(&d, 0, sizeof(d));
    if(id >= 0) snprintf(d.k, KSIZE, "%015ld", id);
    d.addr = a;
  }
  size_t KeySize() { return sizeof(d); }
  int CompareKey(const DatabaseKeyB &o) const {
    return memcmp(d.k, ((const KeyData *)o.db_key)->k, KSIZE);
  }
  int operator==(const DatabaseKeyB &o) const { return CompareKey(o) == 0; }
  int operator>(const DatabaseKeyB &o) const { return CompareKey(o) > 0; }
  KeyData d;
};

static void make_value(long id, int gen, char *v)
{
  for(size_t i = 0; i < VSIZE; i++) v[i] = (char)((id * 31 + gen * 7 + i) & 0xff);
  memcpy(v, &id, sizeof(id));
  v[sizeof(id)] = (char)gen;
}

// The files of one test database
struct TestDb {
  std::string dir, data, index, jnl;
  gxDatabase db;
  Key type;
  gxBtree *btx = 0;
  BtreeCache *cache = 0;
  gxdJournal j;
  TestDb(const std::string &d) : dir(d) {
    data = dir + "/data.gxd"; index = dir + "/index.btx"; jnl = dir + "/data.jnl";
  }
  ~TestDb() { Close(); }
  bool Open(bool create, int journal, int cache_buckets = 0, int page_cache = 0) {
    if(journal && j.Open(jnl.c_str()) != gxDBASE_NO_ERROR) return false;
    btx = new gxBtree(type, 16);
    if(create) {
      if(db.Create(data.c_str()) != gxDBASE_NO_ERROR) return false;
      if(btx->Create(index.c_str()) != gxDBASE_NO_ERROR) return false;
    }
    else {
      if(db.Open(data.c_str()) != gxDBASE_NO_ERROR) return false;
      if(btx->Open(index.c_str()) != gxDBASE_NO_ERROR) return false;
    }
    if(page_cache) {
      db.EnablePageCache(256 * 1024);
      btx->gxDatabasePtr()->EnablePageCache(256 * 1024);
    }
    if(cache_buckets) cache = new BtreeCache(cache_buckets, btx);
    if(journal) {
      // Small page caches and checkpoints, so the tests reach them
      if(j.Attach(&db, 256 * 1024) != gxDBASE_NO_ERROR) return false;
      if(j.Attach(btx, 256 * 1024) != gxDBASE_NO_ERROR) return false;
      if(cache && j.Attach(cache) != gxDBASE_NO_ERROR) return false;
      j.SetCheckpointSize(256 * 1024);
      j.SetWriteMode(g_mode);
    }
    return true;
  }
  void Close() {
    j.Close();
    if(cache) { delete cache; cache = 0; }
    if(btx) { btx->Close(); delete btx; btx = 0; }
    db.Close();
  }
  bool Insert(long id, int gen = 0) {
    char rec[KSIZE + VSIZE];
    Key key(id), cmp;
    memcpy(rec, key.d.k, KSIZE);
    make_value(id, gen, rec + KSIZE);
    FAU_t a = db.Alloc(sizeof(rec));
    if(!a || db.Write(rec, sizeof(rec), a) != gxDBASE_NO_ERROR) return false;
    key.d.addr = a;
    return cache ? btx->Insert(key, cmp, cache) : btx->Insert(key, cmp);
  }
  bool Find(long id, Key &key) {
    Key cmp;
    key.Set(id);
    return cache ? btx->Find(key, cmp, cache) : btx->Find(key, cmp);
  }
  bool Update(long id, int gen) {
    Key key;
    if(!Find(id, key)) return false;
    char rec[KSIZE + VSIZE];
    memcpy(rec, key.d.k, KSIZE);
    make_value(id, gen, rec + KSIZE);
    return db.Write(rec, sizeof(rec), key.d.addr) == gxDBASE_NO_ERROR;
  }
  bool Delete(long id) {
    Key key, cmp;
    if(!Find(id, key)) return false;
    FAU_t a = key.d.addr;
    if(!(cache ? btx->Delete(key, cmp, cache) : btx->Delete(key, cmp))) return false;
    return db.Delete(a) != 0;
  }
  // Returns the number of records if keys 0..n-1 are all present in
  // order with data of generation "gen" (or gen_alt), or -1
  long Verify(int gen = 0, int gen_alt = -1) {
    Key key, cmp;
    long n = 0;
    int ok = btx->FindFirst(key);
    while(ok) {
      Key want(n);
      if(memcmp(key.d.k, want.d.k, KSIZE) != 0) return -1;
      char rec[KSIZE + VSIZE], v[VSIZE];
      if(db.Read(rec, sizeof(rec), key.d.addr) != gxDBASE_NO_ERROR) return -1;
      make_value(n, gen, v);
      if(memcmp(rec + KSIZE, v, VSIZE) != 0) {
	if(gen_alt < 0) return -1;
	make_value(n, gen_alt, v);
	if(memcmp(rec + KSIZE, v, VSIZE) != 0) return -1;
      }
      n++;
      ok = btx->FindNext(key, cmp);
    }
    if(btx->NumKeys() != (BtreeSize_t)n) return -1;
    return n;
  }
};

static std::string fresh_dir(const std::string &name)
{
  std::string d = "jt_" + name;
  fs::remove_all(d);
  fs::create_directories(d);
  return d;
}

static void copy_files(const std::string &from, const std::string &to)
{
  fs::remove_all(to);
  fs::create_directories(to);
  for(auto &e : fs::directory_iterator(from))
    fs::copy_file(e.path(), to + "/" + e.path().filename().string());
}

static void restore_files(const std::string &image, const std::string &dir)
// Put a saved copy back in place of the database files
{
  for(auto &e : fs::directory_iterator(image))
    fs::copy_file(e.path(), dir + "/" + e.path().filename().string(),
		  fs::copy_options::overwrite_existing);
}

static std::vector<char> read_file(const std::string &f)
{
  std::ifstream in(f, std::ios::binary);
  return std::vector<char>((std::istreambuf_iterator<char>(in)),
			   std::istreambuf_iterator<char>());
}

static void write_file(const std::string &f, const std::vector<char> &v)
{
  std::ofstream out(f, std::ios::binary | std::ios::trunc);
  out.write(v.data(), (std::streamsize)v.size());
}

// ------------------------------------------------------------------ //
static std::string first_diff(const std::vector<char> &a,
			      const std::vector<char> &b)
{
  size_t i = 0;
  while(i < a.size() && i < b.size() && a[i] == b[i]) i++;
  return " at byte " + std::to_string(i) + " of " + std::to_string(a.size()) +
    "/" + std::to_string(b.size());
}

static void test_same_bytes(std::mt19937 &rng)
{
  // mode 0: no cache, 1: page cache, 2: journal, 3: BtreeCache,
  // 4: BtreeCache and journal. Modes 1 and 2 must match mode 0 and
  // mode 4 must match mode 3.
  std::vector<std::vector<char> > data(5), index(5);
  unsigned seed = rng();
  for(int mode = 0; mode < 5; mode++) {
    std::mt19937 r(seed);
    int journal = (mode == 2 || mode == 4);
    TestDb t(fresh_dir("same" + std::to_string(mode)));
    CHECK(t.Open(true, journal, mode >= 3 ? 8 : 0, mode == 1), "same_bytes open");
    long next = 0;
    for(int b = 0; b < 8; b++) {
      if(journal) CHECK(t.j.Begin() == gxDBASE_NO_ERROR, "same_bytes begin");
      for(int i = 0; i < BATCH; i++) {
	int op = (int)(r() % 10);
	if(op < 7 || next < 10) CHECK(t.Insert(next++), "same_bytes insert");
	else if(op < 9) t.Update((long)(r() % next), 1);
	else t.Delete((long)(r() % next));
      }
      if(journal) CHECK(t.j.Commit(b & 1) == gxDBASE_NO_ERROR, "same_bytes commit");
      else if(t.cache) t.cache->Flush();
    }
    t.Close();
    data[mode] = read_file(t.data);
    index[mode] = read_file(t.index);
  }
  for(int mode = 1; mode < 5; mode++) {
    int ref = (mode == 4) ? 3 : 0;
    if(mode == 3) continue;
    CHECK(data[mode] == data[ref], "same_bytes data file differs, mode " +
	  std::to_string(mode) + first_diff(data[mode], data[ref]));
    CHECK(index[mode] == index[ref], "same_bytes index file differs, mode " +
	  std::to_string(mode) + first_diff(index[mode], index[ref]));
  }
  std::cout << "PASS same_bytes (" << mode_name() << ")\n";
}

static void test_commit()
{
  std::string dir = fresh_dir("commit");
  {
    TestDb t(dir);
    CHECK(t.Open(true, 1), "commit open");
    for(int b = 0; b < 10; b++) {
      CHECK(t.j.Begin() == gxDBASE_NO_ERROR, "commit begin");
      for(int i = 0; i < BATCH; i++) CHECK(t.Insert(b * BATCH + i), "commit insert");
      CHECK(t.j.Commit(b % 2) == gxDBASE_NO_ERROR, "commit commit");
    }
    CHECK(t.j.checkpoints > 0, "commit: no checkpoint ran");
  }
  TestDb t(dir);
  CHECK(t.Open(false, 0), "commit reopen");
  CHECK(t.Verify() == 10 * BATCH, "commit verify");
  std::cout << "PASS commit (" << mode_name() << ")\n";
}

static void test_rollback(int buckets)
{
  std::string name = buckets ? "rollback_cached" : "rollback";
  std::string dir = fresh_dir(name);
  TestDb t(dir);
  CHECK(t.Open(true, 1, buckets), name + " open");
  CHECK(t.j.Begin() == gxDBASE_NO_ERROR, name + " begin");
  for(int i = 0; i < 3 * BATCH; i++) CHECK(t.Insert(i), name + " insert");
  CHECK(t.j.Commit(0) == gxDBASE_NO_ERROR, name + " commit");

  // New records and updates of committed ones, then roll back
  CHECK(t.j.Begin() == gxDBASE_NO_ERROR, name + " begin 2");
  for(int i = 3 * BATCH; i < 5 * BATCH; i++) CHECK(t.Insert(i), name + " insert 2");
  for(int i = 0; i < 3 * BATCH; i += 3) CHECK(t.Update(i, 1), name + " update");
  for(int i = 1; i < 3 * BATCH; i += 7) CHECK(t.Delete(i), name + " delete");
  CHECK(t.j.Rollback() == gxDBASE_NO_ERROR, name + " rollback");
  CHECK(t.Verify() == 3 * BATCH, name + " verify after rollback");

  // The files still work after the rollback
  CHECK(t.j.Begin() == gxDBASE_NO_ERROR, name + " begin 3");
  for(int i = 3 * BATCH; i < 4 * BATCH; i++) CHECK(t.Insert(i), name + " insert 3");
  CHECK(t.j.Commit(1) == gxDBASE_NO_ERROR, name + " commit 3");
  t.Close();
  CHECK(t.Open(false, 0), name + " reopen");
  CHECK(t.Verify() == 4 * BATCH, name + " verify after reopen");
  std::cout << "PASS " << name << " (" << mode_name() << ")\n";
}

static void test_crash(std::mt19937 &rng, int rounds)
{
  for(int r = 0; r < rounds; r++) {
    std::string dir = fresh_dir("crash"), image = dir + "_image";
    int batches = 6 + (int)(rng() % 10);
    int crash_at = (int)(rng() % (batches * BATCH)); // Operation to copy at
    long committed = 0, at_copy = -1;
    {
      TestDb t(dir);
      CHECK(t.Open(true, 1, (r & 1) ? 8 : 0), "crash open");
      int op = 0;
      for(int b = 0; b < batches; b++) {
	CHECK(t.j.Begin() == gxDBASE_NO_ERROR, "crash begin");
	for(int i = 0; i < BATCH; i++, op++) {
	  if(op == crash_at) { copy_files(dir, image); at_copy = committed; }
	  CHECK(t.Insert(b * BATCH + i), "crash insert");
	}
	CHECK(t.j.Commit((int)(rng() % 2)) == gxDBASE_NO_ERROR, "crash commit");
	committed += BATCH;
      }
      t.Close();
    }
    restore_files(image, dir);
    TestDb t(dir);
    CHECK(t.Open(false, 1), "crash recover open");
    long n = t.Verify();
    CHECK(n == at_copy, "crash: recovered " + std::to_string(n) +
	  " records, expected " + std::to_string(at_copy));
  }
  std::cout << "PASS crash (" << mode_name() << ", " << rounds << " rounds)\n";
}

static void test_torn_journal(std::mt19937 &rng, int rounds)
{
  for(int r = 0; r < rounds; r++) {
    std::string dir = fresh_dir("torn"), image = dir + "_image";
    long durable = 0;
    size_t durable_end = 0; // Journal size after the last synced commit
    int batches = 4 + (int)(rng() % 6);
    {
      TestDb t(dir);
      CHECK(t.Open(true, 1), "torn open");
      t.j.SetCheckpointSize(64 * 1024 * 1024); // No checkpoint
      for(int b = 0; b < batches; b++) {
	CHECK(t.j.Begin() == gxDBASE_NO_ERROR, "torn begin");
	for(int i = 0; i < BATCH; i++) CHECK(t.Insert(b * BATCH + i), "torn insert");
	int sync = (b < batches / 2) ? 1 : (int)(rng() % 2);
	CHECK(t.j.Commit(sync) == gxDBASE_NO_ERROR, "torn commit");
	if(sync) {
	  durable = (b + 1) * BATCH;
	  durable_end = (size_t)fs::file_size(t.jnl);
	}
      }
      copy_files(dir, image); // The program dies here, before a checkpoint
      t.Close();
    }
    restore_files(image, dir);
    // A power loss keeps the journal up to its last sync and may lose
    // any part of what came after. Cut it at a random point past the
    // end of the last synced commit, or overwrite the end with garbage.
    std::vector<char> jnl = read_file(dir + "/data.jnl");
    size_t cut = durable_end + (size_t)(rng() % (jnl.size() - durable_end + 1));
    if(rng() % 2) jnl.resize(cut);
    else for(size_t k = cut; k < jnl.size(); k++) jnl[k] = (char)rng();
    write_file(dir + "/data.jnl", jnl);
    TestDb t(dir);
    CHECK(t.Open(false, 1), "torn recover open");
    long n = t.Verify();
    CHECK(n >= 0 && n % BATCH == 0, "torn: part of a batch, " + std::to_string(n));
    CHECK(n >= durable && n <= batches * BATCH, "torn: recovered " +
	  std::to_string(n) + ", synced " + std::to_string(durable));
  }
  std::cout << "PASS torn_journal (" << mode_name() << ", " << rounds << " rounds)\n";
}

static void test_torn_checkpoint(std::mt19937 &rng, int rounds)
{
  for(int r = 0; r < rounds; r++) {
    std::string dir = fresh_dir("tornckpt"), before = dir + "_before",
      after = dir + "_after";
    int batches = 3 + (int)(rng() % 5);
    long total = 0;
    {
      TestDb t(dir);
      CHECK(t.Open(true, 1), "tornckpt open");
      t.j.SetCheckpointSize(64 * 1024 * 1024);
      // Some data already checkpointed into the files
      CHECK(t.j.Begin() == gxDBASE_NO_ERROR, "tornckpt begin");
      for(int i = 0; i < BATCH; i++) CHECK(t.Insert(total++), "tornckpt insert");
      CHECK(t.j.Commit(1) == gxDBASE_NO_ERROR, "tornckpt commit");
      CHECK(t.j.Checkpoint() == gxDBASE_NO_ERROR, "tornckpt checkpoint 1");
      for(int b = 0; b < batches; b++) {
	CHECK(t.j.Begin() == gxDBASE_NO_ERROR, "tornckpt begin");
	for(int i = 0; i < BATCH; i++) {
	  if(total > 10 && (rng() % 5) == 0) t.Update((long)(rng() % total), 1);
	  CHECK(t.Insert(total++), "tornckpt insert");
	}
	CHECK(t.j.Commit(1) == gxDBASE_NO_ERROR, "tornckpt commit");
      }
      copy_files(dir, before);
      CHECK(t.j.Checkpoint() == gxDBASE_NO_ERROR, "tornckpt checkpoint 2");
      copy_files(dir, after);
      t.Close();
    }
    // Each page of the data and index files from before or after the
    // checkpoint, the file cut anywhere between the two sizes. The
    // journal is the one from before the checkpoint (its cleared
    // header was never synced).
    for(const char *name : { "data.gxd", "index.btx" }) {
      std::vector<char> a = read_file(before + "/" + name);
      std::vector<char> b = read_file(after + "/" + name);
      size_t lo = std::min(a.size(), b.size()), hi = std::max(a.size(), b.size());
      size_t size = lo + (size_t)(rng() % (hi - lo + 1));
      std::vector<char> m(size, 0);
      for(size_t off = 0; off < size; off += 4096) {
	const std::vector<char> &src = (rng() % 2) ? a : b;
	for(size_t k = off; k < std::min(size, off + 4096); k++)
	  m[k] = (k < src.size()) ? src[k] : 0;
      }
      write_file(dir + "/" + name, m);
    }
    fs::copy_file(before + "/data.jnl", dir + "/data.jnl",
		  fs::copy_options::overwrite_existing);
    TestDb t(dir);
    CHECK(t.Open(false, 1), "tornckpt recover open");
    long n = t.Verify(0, 1);
    CHECK(n == total, "tornckpt: recovered " + std::to_string(n) +
	  ", expected " + std::to_string(total));
    t.Close();
    std::vector<char> want = read_file(after + "/data.gxd");
    CHECK(read_file(dir + "/data.gxd") == want, "tornckpt: data file differs");
    CHECK(read_file(dir + "/index.btx") == read_file(after + "/index.btx"),
	  "tornckpt: index file differs");
  }
  std::cout << "PASS torn_ckpt (" << mode_name() << ", " << rounds << " rounds)\n";
}

static void test_reader()
// Write-through: a second set of gxDatabase and gxBtree objects (as in
// another process) sees each batch when Commit() returns, holding a
// gxdJournalReadLock while it reads.
{
  std::string dir = fresh_dir("reader");
  TestDb w(dir);
  CHECK(w.Open(true, 1), "reader open writer");
  w.j.SetWriteLock(1);
  w.j.SetCheckpointSize(64 * 1024 * 1024);
  for(int b = 0; b < 6; b++) {
    CHECK(w.j.Begin() == gxDBASE_NO_ERROR, "reader begin");
    for(int i = 0; i < BATCH; i++) CHECK(w.Insert(b * BATCH + i), "reader insert");
    CHECK(w.j.Commit(b % 2) == gxDBASE_NO_ERROR, "reader commit");
    gxdJournalReadLock rl;
    CHECK(rl.Lock(w.jnl.c_str()) == gxDBASE_NO_ERROR, "reader lock");
    TestDb r(dir);
    CHECK(r.Open(false, 0), "reader open");
    long n = r.Verify();
    CHECK(n == (b + 1) * BATCH, "reader: saw " + std::to_string(n) +
	  " records after commit " + std::to_string(b + 1));
    r.Close();
    rl.Unlock();
  }
  std::cout << "PASS reader (write-through)\n";
}

static void test_security_fixes()
// 10/31/2026: Regression tests for the 7.3 security scan findings.
{
  std::string dir = fresh_dir("secfix");
  // 1. A file that fails the signature test is closed by Close()
  {
    std::string junk = dir + "/junk.gxd";
    std::vector<char> bytes(4096, 'x');
    write_file(junk, bytes);
#if defined (__LINUX__)
    auto fds = []() { size_t n = 0;
      for(auto &e : fs::directory_iterator("/proc/self/fd")) { (void)e; n++; }
      return n; };
    size_t before = fds();
#endif
    for(int i = 0; i < 50; i++) {
      gxDatabase db;
      CHECK(db.Open(junk.c_str()) != gxDBASE_NO_ERROR, "secfix: junk file opened");
      db.Close();
    }
#if defined (__LINUX__)
    CHECK(fds() <= before + 2, "secfix: descriptors leaked by failed opens");
#endif
  }
  // 2. A block header with a zero length, or a length so large that the
  // next address wraps back, does not hang TotalBlocks()
  for(int fill = 0; fill < 2; fill++) {
    std::string f = dir + "/zero.gxd";
    {
      gxDatabase db;
      CHECK(db.Create(f.c_str(), (FAU_t)0, ' ') == gxDBASE_NO_ERROR, "secfix: create");
      for(int i = 0; i < 3; i++) {
	char rec[32] = "record";
	FAU_t a = db.Alloc(sizeof(rec));
	db.Write(rec, sizeof(rec), a);
      }
      db.Close();
    }
    std::vector<char> v = read_file(f);
    gxDatabase probe;
    CHECK(probe.Open(f.c_str()) == gxDBASE_NO_ERROR, "secfix: open");
    size_t first = (size_t)probe.FindFirstBlock();
    size_t len_at = first + sizeof(gxUINT32); // block_check_word, then length
    probe.Close();
    CHECK(len_at + sizeof(gxUINT32) <= v.size(), "secfix: block layout");
    memset(&v[len_at], fill ? 0xFF : 0, sizeof(gxUINT32));
    write_file(f, v);
    gxDatabase db;
    CHECK(db.Open(f.c_str()) == gxDBASE_NO_ERROR, "secfix: reopen");
    db.TotalBlocks(); // Returned at all: the loop ends
    db.Close();
  }
  // 3. A journal page with a page number past the file is ignored
  {
    std::string sub = dir + "/jnl";
    fs::create_directories(sub);
    TestDb t(sub);
    CHECK(t.Open(true, 1), "secfix: journal open");
    t.j.SetWriteMode(gxdJOURNAL_WRITE_BACK);
    t.j.SetCheckpointSize(64 * 1024 * 1024);
    CHECK(t.j.Begin() == gxDBASE_NO_ERROR, "secfix: begin");
    for(int i = 0; i < BATCH; i++) CHECK(t.Insert(i), "secfix: insert");
    CHECK(t.j.Commit(1) == gxDBASE_NO_ERROR, "secfix: commit");
    std::vector<char> j = read_file(t.jnl);
    t.Close();
    // Find the first page record and set a huge page number. Its
    // checksum no longer matches, so recovery stops there: the files
    // must still open and the call must return quickly.
    for(size_t k = 0; k + 28 < j.size(); k++) {
      if(memcmp(&j[k], "WFRM", 4) == 0) { memset(&j[k + 16], 0xFF, 8); break; }
    }
    write_file(t.jnl, j);
    unsigned long n = 0;
    CHECK(gxdJournal::Recover(t.jnl.c_str(), &n) == gxDBASE_NO_ERROR, "secfix: recover");
    CHECK(t.Open(false, 0), "secfix: files open after recovery");
  }
  std::cout << "PASS security_fixes\n";
}

int main(int argc, char **argv)
{
  int rounds = argc > 1 ? atoi(argv[1]) : 20;
  unsigned seed = argc > 2 ? (unsigned)atoi(argv[2]) : 7;
  std::mt19937 rng(seed);
  for(int m = 0; m < 2; m++) {
    g_mode = m ? gxdJOURNAL_WRITE_BACK : gxdJOURNAL_WRITE_THROUGH;
    test_same_bytes(rng);
    test_commit();
    test_rollback(0);
    test_rollback(8);
    test_crash(rng, rounds);
    test_torn_journal(rng, rounds);
    test_torn_checkpoint(rng, rounds);
  }
  g_mode = gxdJOURNAL_WRITE_THROUGH;
  test_reader();
  test_security_fixes();
  if(failures) {
    std::cout << failures << " journal tests failed\n";
    return 1;
  }
  for(auto &e : fs::directory_iterator("."))
    if(e.path().filename().string().rfind("jt_", 0) == 0) fs::remove_all(e.path());
  std::cout << "All journal tests passed\n";
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
