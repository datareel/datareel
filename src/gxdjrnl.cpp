// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: gxdjrnl.cpp
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
version 3 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library. If not, see
<https://www.gnu.org/licenses/>.

Write-ahead journal for gxDatabase files. See gxdjrnl.h.

Journal file layout (native byte order; a journal is only read back
on the machine that wrote it):

Header, written by the first commit after the journal is emptied:
  char[8]  "GXDWAL01"
  uint32   version (1)
  uint32   salt (new for every header, repeated in every record)
  uint32   number of files
  uint32   page size (4096)
  per file: uint32 name length, name (full path, no terminating null)
  uint32   checksum of the header bytes above

Page record, one per page changed by a transaction:
  uint32   magic 0x4D524657 ("WFRM")
  uint32   salt
  uint32   file index
  uint32   commit number
  uint64   page number (file offset / page size)
  uint32   checksum of the fields from salt to page number and the page
  data     (one page)

Commit record, after the transaction's page records:
  uint32   magic 0x544D4357 ("WCMT")
  uint32   salt
  uint32   commit number (1, 2, 3, ... after each header)
  uint32   number of files
  uint64   size of each file after the transaction
  uint32   checksum of the fields from salt to the last size

Recovery reads the records in order and stops at the first record that
is not valid (wrong magic, salt, commit number or checksum). Pages of
a transaction are written to the files only when its commit record is
found, so a transaction cut short by a crash is ignored. A checkpoint
writes and syncs the database files and then clears the header's magic
number; records left in the file after that have an old salt.
*/
// ----------------------------------------------------------- //
#if !defined(_WIN32)
#include <unistd.h>
#include <fcntl.h>
#endif
#include <stdio.h>
#include "gxdlcode.h"

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <time.h>
#include <atomic>
#include <limits>
#include <map>
#include "gxdjrnl.h"
#include "gxdrepl.h"
#include "gxbtree.h"
#include "btcache.h"

namespace datareel {

#if defined(GXREPL_TEST_HOOKS) && !defined(_WIN32)
// 11/14/2026 (test builds only): set by gxrepl's GXREPL_TEST_CRASH_IN_APPLY hook: the next ApplyPages() ends the
// process once its batch is committed to the journal, before its pages reach the files
GXDLCODE_API std::atomic<int> gxdJournalTestCrashNext(0);
#endif

static const char gxdJournalMagic[8] = { 'G','X','D','W','A','L','0','1' };
static const uint32_t gxdJournalVersion = 1;
static const uint32_t gxdJournalPageMagic = 0x4D524657;
static const uint32_t gxdJournalCommitMagic = 0x544D4357;
static const __ULWORD__ gxdJournalPageHdrSize = 28;
static const size_t gxdJournalMaxFiles = 1024;

static void jput32(std::vector<unsigned char> &v, uint32_t x)
{
  unsigned char b[4];
  memcpy(b, &x, 4);
  v.insert(v.end(), b, b + 4);
}

static void jput64(std::vector<unsigned char> &v, uint64_t x)
{
  unsigned char b[8];
  memcpy(b, &x, 8);
  v.insert(v.end(), b, b + 8);
}

static uint32_t jget32(const unsigned char *p)
{
  uint32_t x; memcpy(&x, p, 4); return x;
}

static uint64_t jget64(const unsigned char *p)
{
  uint64_t x; memcpy(&x, p, 8); return x;
}

static uint64_t jsum(const unsigned char *p, size_t n, uint64_t h)
// Checksum used to find torn journal writes. It reads 8 bytes per step,
// many times faster than the table driven CRC-32 on 4 KB pages.
{
  const uint64_t m = 0x9E3779B97F4A7C15ULL;
  // Four independent lanes for 32 bytes at a time, so the multiplies
  // overlap; then the lanes are folded in
  if(n >= 32) {
    uint64_t l0 = h, l1 = h ^ 0x6A09E667F3BCC909ULL, 
      l2 = h ^ 0xBB67AE8584CAA73BULL, l3 = h ^ 0x3C6EF372FE94F82BULL;
    while(n >= 32) {
      uint64_t w[4];
      memcpy(w, p, 32);
      l0 = (l0 ^ w[0]) * m; l0 ^= l0 >> 29;
      l1 = (l1 ^ w[1]) * m; l1 ^= l1 >> 29;
      l2 = (l2 ^ w[2]) * m; l2 ^= l2 >> 29;
      l3 = (l3 ^ w[3]) * m; l3 ^= l3 >> 29;
      p += 32; n -= 32;
    }
    h = ((((l0 * m) ^ l1) * m ^ l2) * m ^ l3) * m;
  }
  while(n >= 8) {
    uint64_t w;
    memcpy(&w, p, 8);
    h = (h ^ w) * m;
    h ^= h >> 29;
    p += 8; n -= 8;
  }
  while(n--) {
    h = (h ^ *p++) * m;
    h ^= h >> 29;
  }
  return h;
}

static uint32_t jcheck(const unsigned char *p, size_t n,
		       const unsigned char *data = 0, size_t len = 0)
{
  uint64_t h = jsum(p, n, (uint64_t)(n + len));
  if(data) h = jsum(data, len, h);
  return (uint32_t)(h ^ (h >> 32));
}

static std::string jfull_path(const char *name)
{
#if defined (__WIN32__)
  char buf[_MAX_PATH];
  if(_fullpath(buf, name, sizeof(buf))) return std::string(buf);
#else
  char buf[PATH_MAX];
  if(realpath(name, buf)) return std::string(buf);
#endif
  return std::string(name);
}

static const FAU_t gxdJournalGrowBy = 4 * 1024 * 1024;

static int jreserve(gxdFPTR *jfp, FAU_t &jalloc, FAU_t end)
// Keep the journal file longer than its contents so a commit's sync
// does not also have to write a new file size. The space past the end
// of the records holds zeros or old records, which recovery ignores.
{
  if(end <= jalloc) return 0;
  FAU_t size = ((end / gxdJournalGrowBy) + 1) * gxdJournalGrowBy;
  if(gxdFPTRAllocate(jfp, size) != 0) return 1;
  jalloc = size;
  return 0;
}

static int jclear(gxdFPTR *jfp, int sync)
// Clear the header's magic number. Returns 0 if successful.
{
  char zero[8];
  memset(zero, 0, sizeof(zero));
  if(gxdFPTRWriteAt(jfp, zero, sizeof(zero), (FAU_t)0) != sizeof(zero))
    return 1;
  return sync ? gxdFPTRSync(jfp, 0) : 0;
}

gxdJournal::gxdJournal()
{
  jfp = 0;
  in_txn = header_written = unsynced = durable_logged = 0;
  sync_level = gxdJOURNAL_SYNC_NORMAL;
  recovered = 0;
  jsize = jalloc = (FAU_t)0;
  seq = 0;
  salt = 0;
  ckpt_size = 0;
  write_mode = gxdJOURNAL_WRITE_THROUGH;
  write_lock = 0;
  jerror = gxDBASE_NO_ERROR;
  commits = checkpoints = 0;
  pages_logged = pages_written = 0;
  ship = 0;
  onotes_on = 0;
}

gxdJournal::~gxdJournal()
{
  Close();
  DisableShipLog();
}

// 11/15/2026: the marker a replay leaves for the next EnableShipLog() (see Recover())
static std::string recovered_marker(const char *jname) { return std::string(jname) + ".recovered"; }

// 11/16/2026 (beta4 review M2): "<n>\n" written to a temporary file, flushed to the disk, renamed over the marker
// and (POSIX) the folder synced. Returns 0, or -1 if any step failed (nothing is left behind then).
#ifndef _WIN32
static int write_recovered_marker(const std::string &mk, unsigned long n)
{
  std::string tmp = mk + ".tmp", body = std::to_string(n) + "\n";
  int mf = open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0640);
  if(mf < 0) return -1;
  bool ok = write(mf, body.data(), body.size()) == (ssize_t)body.size() && fsync(mf) == 0;
  close(mf);
  if(!ok || rename(tmp.c_str(), mk.c_str()) != 0) { unlink(tmp.c_str()); return -1; }
  std::string::size_type sl = mk.find_last_of('/');
  std::string d = sl == std::string::npos ? std::string(".") : (sl == 0 ? std::string("/") : mk.substr(0, sl));
  int df = open(d.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if(df < 0) return -1;
  int rc = fsync(df);
  close(df);
  return rc == 0 ? 0 : -1;
}
#endif

gxDatabaseError gxdJournal::Recover(const char *jname,
				    unsigned long *recovered, int mark)
{
  if(recovered) *recovered = 0;
  if(!jname) return gxDBASE_NULL_PTR;
  if(!gxdFPTRExists(jname)) return gxDBASE_NO_ERROR;
  gxdFPTR *jfp = gxdFPTROpen(jname, gxDBASE_READWRITE);
  if(!jfp) return gxDBASE_FILE_OPEN_ERROR;

  // Read the header
  std::vector<std::string> paths;
  uint32_t hsalt = 0;
  FAU_t pos = 0;
  int valid = 0;
  unsigned char fixed[24];
  if((gxdFPTRReadAt(jfp, fixed, 24, (FAU_t)0) == 24) &&
     (memcmp(fixed, gxdJournalMagic, 8) == 0) &&
     (jget32(fixed + 8) == gxdJournalVersion) &&
     (jget32(fixed + 20) == gxdPageSize)) {
    hsalt = jget32(fixed + 12);
    uint32_t nfiles = jget32(fixed + 16);
    std::vector<unsigned char> h(fixed, fixed + 24);
    pos = 24;
    valid = (nfiles > 0) && (nfiles <= gxdJournalMaxFiles);
    for(uint32_t i = 0; valid && i < nfiles; i++) {
      unsigned char lb[4];
      if(gxdFPTRReadAt(jfp, lb, 4, pos) != 4) { valid = 0; break; }
      uint32_t len = jget32(lb);
      if(len == 0 || len > 4096) { valid = 0; break; }
      std::vector<char> name(len);
      if(gxdFPTRReadAt(jfp, name.data(), len, pos + 4) != len) {
	valid = 0; break;
      }
      pos += 4 + len;
      h.insert(h.end(), lb, lb + 4);
      h.insert(h.end(), name.begin(), name.end());
      paths.push_back(std::string(name.data(), len));
    }
    unsigned char cb[4];
    if(valid && ((gxdFPTRReadAt(jfp, cb, 4, pos) != 4) ||
		 (jget32(cb) != jcheck(h.data(), h.size())))) valid = 0;
    pos += 4;
  }
  if(!valid) { // Nothing to replay
    gxdFPTRClose(jfp);
    return gxDBASE_NO_ERROR;
  }

  // Find the committed pages. The last record of a page wins.
  typedef std::pair<uint32_t, uint64_t> PageKey;
  std::map<PageKey, FAU_t> committed, pending;
  std::vector<uint64_t> sizes(paths.size(), 0);
  std::vector<int> has_size(paths.size(), 0);
  unsigned long ncommits = 0;
  uint32_t next_seq = 1;
  std::vector<unsigned char> buf(gxdJournalPageHdrSize + gxdPageSize);
  while(1) {
    unsigned char *r = buf.data();
    if(gxdFPTRReadAt(jfp, r, 16, pos) != 16) break;
    if(jget32(r + 4) != hsalt) break;
    uint32_t magic = jget32(r);
    if(magic == gxdJournalPageMagic) {
      if(gxdFPTRReadAt(jfp, r, gxdJournalPageHdrSize + gxdPageSize, pos) !=
	 gxdJournalPageHdrSize + gxdPageSize) break;
      uint32_t idx = jget32(r + 8);
      if((idx >= paths.size()) || (jget32(r + 12) != next_seq)) break;
      if(jcheck(r + 4, 20, r + gxdJournalPageHdrSize, gxdPageSize) !=
	 jget32(r + 24)) break;
      pending[PageKey(idx, jget64(r + 16))] = pos + gxdJournalPageHdrSize;
      pos += gxdJournalPageHdrSize + gxdPageSize;
    }
    else if(magic == gxdJournalCommitMagic) {
      if(jget32(r + 8) != next_seq || jget32(r + 12) != paths.size()) break;
      size_t len = 16 + 8 * paths.size();
      std::vector<unsigned char> c(len + 4);
      if(gxdFPTRReadAt(jfp, c.data(), (__ULWORD__)c.size(), pos) != c.size())
	break;
      if(jcheck(c.data() + 4, len - 4) != jget32(c.data() + len)) break;
      // 10/31/2026: A size the file address type cannot hold is damage
      int bad_size = 0;
      for(size_t i = 0; i < paths.size(); i++) {
	if(jget64(c.data() + 16 + 8 * i) > 
	   (uint64_t)std::numeric_limits<FAU_t>::max()) bad_size = 1;
      }
      if(bad_size) break;
      for(size_t i = 0; i < paths.size(); i++) {
	sizes[i] = jget64(c.data() + 16 + 8 * i);
	has_size[i] = 1;
      }
      for(auto &e : pending) committed[e.first] = e.second;
      pending.clear();
      ncommits++;
      next_seq++;
      pos += (FAU_t)c.size();
    }
    else break;
  }

  gxDatabaseError err = gxDBASE_NO_ERROR;
  // 11/15/2026 (hardening): the marker, durably, before the first page is replayed: a crash in the middle of the
  // replay still leaves it
  // 11/16/2026 (beta4 review M2): a marker that cannot be made durable refuses the replay (the journal is kept for
  // another try): a replay without it would reach the files with no record and no open record saying so.
#ifndef _WIN32
  if(ncommits && mark && write_recovered_marker(recovered_marker(jname), ncommits) != 0) {
    gxdFPTRClose(jfp);
    return gxDBASE_FILE_WRITE_ERROR;
  }
#endif
  if(ncommits) {
    std::vector<gxdFPTR *> fps(paths.size(), (gxdFPTR *)0);
    for(size_t i = 0; i < paths.size(); i++) {
      if(!has_size[i]) continue;
      fps[i] = gxdFPTROpen(paths[i].c_str(), gxDBASE_READWRITE);
      if(!fps[i]) { err = gxDBASE_FILE_OPEN_ERROR; break; }
    }
#if defined(GXREPL_TEST_HOOKS) && !defined(_WIN32)
    // 11/13/2026 test hook (test builds only): GXREPL_TEST_APPLY_FAIL=N makes the first N recoveries of a batch
    // into a gxrepl stage fail before any page is written (an ENOSPC): the journal is kept for another try
    if(err == gxDBASE_NO_ERROR && getenv("GXREPL_TEST_APPLY_FAIL")) {
      static std::atomic<int> failed{0};
      bool staged = false;
      for(size_t i = 0; i < paths.size(); i++) if(paths[i].find("/.gxrepl/stage/") != std::string::npos) staged = true;
      if(staged && failed.fetch_add(1) < atoi(getenv("GXREPL_TEST_APPLY_FAIL"))) err = gxDBASE_FILE_WRITE_ERROR;
    }
#endif
    for(auto &e : committed) {
      if(err != gxDBASE_NO_ERROR) break;
      // 10/31/2026: A page past the file's committed size is cut off by
      // the truncate below anyway. Skipping it also keeps a damaged or
      // crafted page number from turning into a wrapped file offset.
      if(e.first.second >= ((uint64_t)sizes[e.first.first] + gxdPageSize - 1) /
	 gxdPageSize) continue;
      if(gxdFPTRReadAt(jfp, buf.data(), gxdPageSize, e.second) != gxdPageSize ||
	 gxdFPTRWriteAt(fps[e.first.first], buf.data(), gxdPageSize,
			(FAU_t)(e.first.second * gxdPageSize)) != gxdPageSize)
	err = gxDBASE_FILE_WRITE_ERROR;
    }
    for(size_t i = 0; i < fps.size(); i++) {
      if(!fps[i]) continue;
      if(err == gxDBASE_NO_ERROR) {
	if(gxdFPTRTruncate(fps[i], (FAU_t)sizes[i]) != 0 ||
	   gxdFPTRSync(fps[i], 1) != 0) err = gxDBASE_FILE_WRITE_ERROR;
      }
      gxdFPTRClose(fps[i]);
    }
  }
  // Keep the journal if the files could not be updated, for another try
  if(err == gxDBASE_NO_ERROR) {
    if(jclear(jfp, 1) != 0) err = gxDBASE_FILE_WRITE_ERROR;
    else if(recovered) *recovered = ncommits;
  }
  gxdFPTRClose(jfp);
  return err;
}

gxDatabaseError gxdJournal::Open(const char *name)
{
  Close();
  recovered = 0;
  jerror = Recover(name, &recovered);
  if(jerror != gxDBASE_NO_ERROR) return jerror;
  jfp = gxdFPTRExists(name) ? gxdFPTROpen(name, gxDBASE_READWRITE) :
    gxdFPTRCreate(name);
  if(!jfp) return jerror = gxDBASE_FILE_OPEN_ERROR;
  jname = name;
  jsize = (FAU_t)0;
  jalloc = gxdFPTRSeek(jfp, (FAU_t)0, gxDBASE_SEEK_END);
  if(jalloc < (FAU_t)0) jalloc = (FAU_t)0;
  header_written = unsynced = durable_logged = 0;
  seq = 0;
  return jerror = gxDBASE_NO_ERROR;
}

gxDatabaseError gxdJournal::Close()
{
  gxDatabaseError err = gxDBASE_NO_ERROR;
  if(in_txn) Rollback();
  if(jfp) err = Detach();
  for(size_t i = 0; i < files.size(); i++) notes_detach(files[i]);
  files.clear();
  caches.clear();
  // After a clean checkpoint the journal holds nothing to recover
  if(jfp && (err == gxDBASE_NO_ERROR) && !header_written) {
    gxdFPTRTruncate(jfp, (FAU_t)0);
  }
  if(jfp) {
    gxdFPTRClose(jfp);
    jfp = 0;
  }
  return jerror = err;
}

gxDatabaseError gxdJournal::Attach(gxDatabase *f, size_t cache_bytes)
{
  if(!f || !f->IsOK()) return jerror = gxDBASE_FILE_NOT_READY;
  if(in_txn) return jerror = gxDBASE_FILE_NOT_READY;
  for(size_t i = 0; i < files.size(); i++) {
    if(files[i].f == f) return jerror = gxDBASE_NO_ERROR;
  }
  // A new file changes the journal's file list
  if(Checkpoint() != gxDBASE_NO_ERROR) return jerror;
  if(!f->PageCache()) {
    if(f->EnablePageCache(cache_bytes) != gxDBASE_NO_ERROR)
      return jerror = f->GetDatabaseError();
  }
  JFile jf;
  jf.f = f;
  jf.btx = 0;
  jf.path = jfull_path(f->DatabaseName());
  jf.written = 0;
  jf.notes = 0;
  if(onotes_on) notes_attach(jf);
  files.push_back(jf);
  return jerror = gxDBASE_NO_ERROR;
}

gxDatabaseError gxdJournal::Attach(gxBtree *btx, size_t cache_bytes)
{
  if(!btx) return jerror = gxDBASE_NULL_PTR;
  gxDatabase *f = btx->gxDatabasePtr();
  if(Attach(f, cache_bytes) != gxDBASE_NO_ERROR) return jerror;
  for(size_t i = 0; i < files.size(); i++) {
    if(files[i].f == f) files[i].btx = btx;
  }
  return jerror = gxDBASE_NO_ERROR;
}

gxDatabaseError gxdJournal::Attach(BtreeCache *cache)
{
  if(!cache) return jerror = gxDBASE_NULL_PTR;
  if(in_txn) return jerror = gxDBASE_FILE_NOT_READY;
  for(size_t i = 0; i < caches.size(); i++) {
    if(caches[i] == cache) return jerror = gxDBASE_NO_ERROR;
  }
  caches.push_back(cache);
  return jerror = gxDBASE_NO_ERROR;
}

gxDatabaseError gxdJournal::Detach()
{
  if(in_txn) return jerror = gxDBASE_FILE_NOT_READY;
  if(Checkpoint() != gxDBASE_NO_ERROR) return jerror;
  for(size_t i = 0; i < files.size(); i++) notes_detach(files[i]);
  files.clear();
  caches.clear();
  return jerror = gxDBASE_NO_ERROR;
}

gxDatabaseError gxdJournal::Begin()
{
  if(!jfp) return jerror = gxDBASE_FILE_NOT_READY;
  if(in_txn) return jerror = gxDBASE_FILE_NOT_READY;
  if(files.empty()) return jerror = gxDBASE_FILE_NOT_READY;
  for(size_t i = 0; i < files.size(); i++) {
    if(!files[i].f->IsOK() || !files[i].f->PageCache())
      return jerror = gxDBASE_FILE_NOT_READY;
  }
  for(size_t i = 0; i < files.size(); i++)
    files[i].f->PageCache()->BeginTxn();
  // 11/14/2026: notes made outside a transaction: their pages may go
  // with this commit, so this commit's notes for the file are incomplete
  for(size_t i = 0; i < files.size(); i++)
    if(files[i].notes && !files[i].notes->notes.empty()) files[i].notes->Incomplete();
  in_txn = 1;
  return jerror = gxDBASE_NO_ERROR;
}

gxDatabaseError gxdJournal::WriteHeader()
{
  static unsigned counter = 0;
  salt = (unsigned long)(((unsigned)time(0) * 2654435761u) ^
			 (++counter * 40503u) ^ (unsigned)(size_t)this);
  salt &= 0xFFFFFFFFUL;
  if(salt == 0) salt = 1;
  std::vector<unsigned char> h(gxdJournalMagic, gxdJournalMagic + 8);
  jput32(h, gxdJournalVersion);
  jput32(h, (uint32_t)salt);
  jput32(h, (uint32_t)files.size());
  jput32(h, (uint32_t)gxdPageSize);
  for(size_t i = 0; i < files.size(); i++) {
    jput32(h, (uint32_t)files[i].path.size());
    h.insert(h.end(), files[i].path.begin(), files[i].path.end());
  }
  jput32(h, jcheck(h.data(), h.size()));
  if(gxdFPTRWriteAt(jfp, h.data(), (__ULWORD__)h.size(), (FAU_t)0) !=
     h.size()) return gxDBASE_FILE_WRITE_ERROR;
  jsize = (FAU_t)h.size();
  seq = 0;
  header_written = 1;
  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxdJournal::Commit(int durable)
{
  if(!in_txn) return jerror = gxDBASE_FILE_NOT_READY;

  // Write the in-memory headers and cached B-tree nodes into the pages
  for(size_t i = 0; i < caches.size(); i++) {
    if(caches[i]->Flush() != gxDBASE_NO_ERROR)
      return jerror = gxDBASE_FILE_WRITE_ERROR;
  }
  for(size_t i = 0; i < files.size(); i++) {
    if(files[i].btx) {
      if(files[i].btx->WriteBtreeHeader() != gxDBASE_NO_ERROR)
	return jerror = gxDBASE_FILE_WRITE_ERROR;
    }
    if(files[i].f->ReadyForWriting()) {
      if(files[i].f->WriteFileHdr() != gxDBASE_NO_ERROR)
	return jerror = gxDBASE_FILE_WRITE_ERROR;
    }
  }

  size_t dirty = 0;
  for(size_t i = 0; i < files.size(); i++)
    dirty += files[i].f->PageCache()->DirtyPages();

  // 11/07/2026: The pages of each file, kept for the ship log
  std::vector< std::vector<gxdPage *> > shipped;
  if(ship && dirty) shipped.resize(files.size());

  if(dirty) {
    if(!header_written) {
      if(WriteHeader() != gxDBASE_NO_ERROR)
	return jerror = gxDBASE_FILE_WRITE_ERROR;
    }
    uint32_t cseq = (uint32_t)(seq + 1);
    out.clear();
    std::vector<gxdPage *> v;
    for(size_t i = 0; i < files.size(); i++) {
      files[i].f->PageCache()->GetDirtyPages(v);
      if(ship) shipped[i] = v;
      for(size_t k = 0; k < v.size(); k++) {
	size_t start = out.size();
	jput32(out, gxdJournalPageMagic);
	jput32(out, (uint32_t)salt);
	jput32(out, (uint32_t)i);
	jput32(out, cseq);
	jput64(out, (uint64_t)v[k]->page_no);
	uint32_t check = jcheck(out.data() + start + 4, 20,
				v[k]->data, gxdPageSize);
	jput32(out, check);
	out.insert(out.end(), v[k]->data, v[k]->data + gxdPageSize);
	if(out.size() >= 1024 * 1024) {
	  if(jreserve(jfp, jalloc, jsize + (FAU_t)out.size()) != 0)
	    return jerror = gxDBASE_FILE_WRITE_ERROR;
	  if(gxdFPTRWriteAt(jfp, out.data(), (__ULWORD__)out.size(), jsize) !=
	     out.size()) return jerror = gxDBASE_FILE_WRITE_ERROR;
	  jsize += (FAU_t)out.size();
	  out.clear();
	}
      }
      pages_logged += v.size();
    }
    size_t start = out.size();
    jput32(out, gxdJournalCommitMagic);
    jput32(out, (uint32_t)salt);
    jput32(out, cseq);
    jput32(out, (uint32_t)files.size());
    for(size_t i = 0; i < files.size(); i++)
      jput64(out, (uint64_t)files[i].f->PageCache()->FileSize());
    jput32(out, jcheck(out.data() + start + 4, out.size() - start - 4));
      if(jreserve(jfp, jalloc, jsize + (FAU_t)out.size()) != 0)
      return jerror = gxDBASE_FILE_WRITE_ERROR;
    if(gxdFPTRWriteAt(jfp, out.data(), (__ULWORD__)out.size(), jsize) !=
       out.size()) return jerror = gxDBASE_FILE_WRITE_ERROR;
    jsize += (FAU_t)out.size();
    if(durable) {
      if(gxdFPTRSync(jfp, 0) != 0) return jerror = gxDBASE_FILE_WRITE_ERROR;
      unsynced = 0;
      durable_logged = 1;
    }
    else {
      unsynced = 1;
    }
    seq = cseq;
  }

  gxDatabaseError err = gxDBASE_NO_ERROR;
  if(write_mode == gxdJOURNAL_WRITE_THROUGH) {
    // Write the batch to the files now. The journal holds it, so a
    // crash part way through is repaired by recovery.
    if(dirty && write_lock) gxdFPTRLock(jfp, 1);
    // 11/07/2026: The commit is in the journal. Append it to the ship
    // log under the write lock, before its pages reach the files, so a
    // reader holding gxdJournalReadLock sees files that match the log.
#if defined(GXREPL_TEST_HOOKS) && !defined(_WIN32)
    // 11/14/2026: test builds: GXDJRNL_TEST_KILL_BEFORE_SHIP: stop the writer with its batch committed in the
    // journal but not yet in the ship log (the next writer's recovery replays it: the drain-verify test)
    if(ship && dirty && getenv("GXDJRNL_TEST_KILL_BEFORE_SHIP")) _exit(9);
#endif
    if(ship && dirty) ship_commit(shipped);
    for(size_t i = 0; i < files.size(); i++) {
      gxdPageCache *pc = files[i].f->PageCache();
      pc->CommitTxn();
      size_t n = pc->LoggedPages();
      if(n == 0) continue;
      if(pc->WriteLoggedPages() != 0) err = gxDBASE_FILE_WRITE_ERROR;
      files[i].written = 1;
      pages_written += n;
    }
    if(dirty && write_lock) gxdFPTRUnlock(jfp);
  }
  else {
#if defined(GXREPL_TEST_HOOKS) && !defined(_WIN32)
    // 11/14/2026: test builds: GXDJRNL_TEST_KILL_BEFORE_SHIP: stop the writer with its batch committed in the
    // journal but not yet in the ship log (the next writer's recovery replays it: the drain-verify test)
    if(ship && dirty && getenv("GXDJRNL_TEST_KILL_BEFORE_SHIP")) _exit(9);
#endif
    if(ship && dirty) ship_commit(shipped);
    for(size_t i = 0; i < files.size(); i++)
      files[i].f->PageCache()->CommitTxn();
  }
  in_txn = 0;
  commits++;
  notes_clear(); // 11/14/2026: the objects record went with the commit
  if(err != gxDBASE_NO_ERROR) return jerror = err;

  // Checkpoint when the journal is large or the committed pages pinned
  // in memory pass a file's page cache size
  int ckpt = (size_t)jsize > CheckpointSize();
  for(size_t i = 0; !ckpt && i < files.size(); i++) {
    gxdPageCache *pc = files[i].f->PageCache();
    if(pc->LoggedPages() * gxdPageSize > pc->MaxBytes()) ckpt = 1;
  }
  if(ckpt) return Checkpoint();
  return jerror = gxDBASE_NO_ERROR;
}

size_t gxdJournal::CheckpointSize() const
{
  if(ckpt_size) return ckpt_size;
  return (write_mode == gxdJOURNAL_WRITE_BACK) ? 
    gxdDefaultWriteBackCheckpointSize : gxdDefaultCheckpointSize;
}

gxDatabaseError gxdJournal::Checkpoint()
// Write the committed pages to the database files and empty the
// journal. The journal is synced first, so a crash during the
// checkpoint replays the same transactions again.
{
  if(in_txn) return jerror = gxDBASE_FILE_NOT_READY;
  if(!jfp || !header_written) return jerror = gxDBASE_NO_ERROR;
  int sync = (sync_level != gxdJOURNAL_SYNC_OFF) || durable_logged;
  if(sync && unsynced) {
    if(gxdFPTRSync(jfp, 0) != 0) return jerror = gxDBASE_FILE_WRITE_ERROR;
  }
  unsynced = 0;
  for(size_t i = 0; i < files.size(); i++) {
    gxdPageCache *pc = files[i].f->PageCache();
    size_t n = pc->LoggedPages();
    if(n) {
      if(pc->WriteLoggedPages() != 0) return jerror = gxDBASE_FILE_WRITE_ERROR;
      pages_written += n;
      files[i].written = 1;
    }
    if(!files[i].written) continue;
    if(sync && (gxdFPTRSync(files[i].f->GetFP(), 0) != 0))
      return jerror = gxDBASE_FILE_WRITE_ERROR;
    files[i].written = 0;
  }
  // The cleared header is not synced: if it is lost, recovery writes
  // the same pages to the files again. The next header is synced before
  // the files are changed again.
  if(jclear(jfp, 0) != 0) return jerror = gxDBASE_FILE_WRITE_ERROR;
  header_written = 0;
  durable_logged = 0;
  jsize = (FAU_t)0;
  seq = 0;
  checkpoints++;
  // Keep the journal file from staying much larger than one checkpoint
  if(jalloc > (FAU_t)(2 * CheckpointSize() + gxdJournalGrowBy)) {
    if(gxdFPTRTruncate(jfp, (FAU_t)0) == 0) jalloc = (FAU_t)0;
  }
  return jerror = gxDBASE_NO_ERROR;
}

gxDatabaseError gxdJournal::Rollback()
{
  if(!in_txn) return jerror = gxDBASE_FILE_NOT_READY;
  for(size_t i = 0; i < caches.size(); i++) caches[i]->Discard();
  for(size_t i = 0; i < files.size(); i++)
    files[i].f->PageCache()->RollbackTxn();
  in_txn = 0;
  notes_clear(); // 11/14/2026

  // Reload the in-memory headers from the committed pages
  for(size_t i = 0; i < files.size(); i++) {
    files[i].f->NoteFileChanged();
    files[i].f->ReadFileHdr();
    if(files[i].btx) files[i].btx->ReadBtreeHeader();
  }
  return jerror = gxDBASE_NO_ERROR;
}

void gxdJournal::ship_commit(const std::vector< std::vector<gxdPage *> > &v)
// 11/07/2026: Append a commit to the ship log. A failure is kept in
// the ship log (it writes a gap record with its next record) and never
// fails the commit.
{
  std::vector<gxdShipFile> sf(files.size());
  for(size_t i = 0; i < files.size(); i++) {
    sf[i].path = files[i].path;
    sf[i].size = (unsigned long long)files[i].f->PageCache()->FileSize();
    if(i < v.size()) sf[i].pages = &v[i];
    sf[i].notes = files[i].notes; // 11/14/2026: 0 when object notes are off
  }
  // 11/08/2026: Power-off safety: the record is synced before the pages
  // reach the files (unless the journal itself does not sync)
  ship->SetSync(sync_level != gxdJOURNAL_SYNC_OFF ? 1 : 0);
  ship->AppendCommit(sf);
}

gxDatabaseError gxdJournal::EnableShipLog(const char *set_root, 
					  const char *stream)
{
  if(!set_root || !*set_root) return jerror = gxDBASE_NULL_PTR;
  if(!jfp) return jerror = gxDBASE_FILE_NOT_READY;
  if(in_txn) return jerror = gxDBASE_FILE_NOT_READY;
  if(write_mode != gxdJOURNAL_WRITE_THROUGH) 
    return jerror = gxDBASE_FILE_NOT_READY;
  DisableShipLog();
  gxdShipLog *log = new gxdShipLog;
  // 11/15/2026 (hardening): the open record says what this writer's journal recovered: Open()'s replay and a
  // static Recover()'s marker. A copy keeps its derived indexes object by object only across an open that says 0.
  unsigned long long rec = recovered;
  std::string mk;
#ifndef _WIN32
  mk = recovered_marker(jname.c_str());
  {
    FILE *mf = fopen(mk.c_str(), "r");
    if(mf) { unsigned long long n = 0; if(fscanf(mf, "%llu", &n) != 1 || n == 0) n = 1; rec += n; fclose(mf); }
  }
#endif
  log->SetOpenRecovered(rec);
  if(log->Open(set_root, stream, jfull_path(jname.c_str()).c_str()) != 0) {
    delete log;
    return jerror = gxDBASE_FILE_OPEN_ERROR;
  }
#ifndef _WIN32
  if(!mk.empty()) unlink(mk.c_str());                    // said in the open record (synced by the ship log)
#endif
  ship = log;
  write_lock = 1;
  // 11/14/2026: object notes from the environment (EnableObjectNotes())
  const char *on = getenv("GXDSHIP_OBJECTS");
  if(on && atoi(on) > 0) EnableObjectNotes(1);
  return jerror = gxDBASE_NO_ERROR;
}

gxDatabaseError gxdJournal::EnableObjectNotes(int on)
{
  if(in_txn) return jerror = gxDBASE_FILE_NOT_READY;
  onotes_on = on ? 1 : 0;
  for(size_t i = 0; i < files.size(); i++) {
    if(onotes_on) notes_attach(files[i]);
    else notes_detach(files[i]);
  }
  return jerror = gxDBASE_NO_ERROR;
}

void gxdJournal::notes_attach(JFile &jf)
{
  if(jf.notes) return;
  jf.notes = new gxdObjectNotes;
  jf.f->SetObjectNotes(jf.notes);
}

void gxdJournal::notes_detach(JFile &jf)
{
  if(!jf.notes) return;
  if(jf.f && jf.f->ObjectNotes() == jf.notes) jf.f->SetObjectNotes(0);
  delete jf.notes;
  jf.notes = 0;
}

void gxdJournal::notes_clear()
{
  for(size_t i = 0; i < files.size(); i++)
    if(files[i].notes) files[i].notes->Clear();
}

void gxdJournal::DisableShipLog()
{
  if(ship) {
    ship->Close();
    delete ship;
    ship = 0;
  }
}

gxDatabaseError gxdJournal::ApplyPages(const char *jname,
				       const std::vector<gxdApplyFile> &af,
				       unsigned long long *pages_written)
{
  if(pages_written) *pages_written = 0;
  if(!jname) return gxDBASE_NULL_PTR;
  if(af.empty()) return gxDBASE_NO_ERROR;
  if(af.size() > gxdJournalMaxFiles) return gxDBASE_BAD_REFERENCE;

  // Finish a batch an earlier call left in the journal
  gxDatabaseError err = Recover(jname, 0, 0);
  if(err != gxDBASE_NO_ERROR) return err;

  // Every file must exist before Recover() opens it
  for(size_t i = 0; i < af.size(); i++) {
    if(af[i].path.empty() || af[i].path.size() > 4096) 
      return gxDBASE_BAD_REFERENCE;
    if(af[i].size > (unsigned long long)std::numeric_limits<FAU_t>::max())
      return gxDBASE_BAD_REFERENCE;
    if(!gxdFPTRExists(af[i].path.c_str())) {
      gxdFPTR *f = gxdFPTRCreate(af[i].path.c_str());
      if(!f) return gxDBASE_FILE_CREATION_ERROR;
      gxdFPTRClose(f);
    }
  }

  gxdFPTR *jfp = gxdFPTRExists(jname) ? 
    gxdFPTROpen(jname, gxDBASE_READWRITE) : gxdFPTRCreate(jname);
  if(!jfp) return gxDBASE_FILE_OPEN_ERROR;

  // 11/08/2026: atomic, several replication channels apply at once
  static std::atomic<unsigned> counter{0};
  uint32_t salt = (uint32_t)(((unsigned)time(0) * 2654435761u) ^
			     ((counter.fetch_add(1) + 1) * 40503u) ^ 0x5A17u);
  if(salt == 0) salt = 1;
  std::vector<unsigned char> h(gxdJournalMagic, gxdJournalMagic + 8);
  jput32(h, gxdJournalVersion);
  jput32(h, salt);
  jput32(h, (uint32_t)af.size());
  jput32(h, (uint32_t)gxdPageSize);
  for(size_t i = 0; i < af.size(); i++) {
    jput32(h, (uint32_t)af[i].path.size());
    h.insert(h.end(), af[i].path.begin(), af[i].path.end());
  }
  jput32(h, jcheck(h.data(), h.size()));

  // The header goes in last, after every record is synced, so a crash
  // part way leaves nothing that Recover() would replay.
  std::vector<unsigned char> out;
  out.reserve(1024 * 1024 + gxdJournalPageHdrSize + gxdPageSize);
  FAU_t pos = (FAU_t)h.size();
  unsigned long long npages = 0;
  int ok = 1;
  if(gxdFPTRTruncate(jfp, (FAU_t)0) != 0) ok = 0;
  for(size_t i = 0; ok && i < af.size(); i++) {
    for(auto &e : af[i].pages) {
      if(!e.second) { ok = 0; break; }
      size_t start = out.size();
      jput32(out, gxdJournalPageMagic);
      jput32(out, salt);
      jput32(out, (uint32_t)i);
      jput32(out, 1);
      jput64(out, (uint64_t)e.first);
      jput32(out, jcheck(out.data() + start + 4, 20, e.second, gxdPageSize));
      out.insert(out.end(), e.second, e.second + gxdPageSize);
      npages++;
      if(out.size() >= 1024 * 1024) {
	if(gxdFPTRWriteAt(jfp, out.data(), (__ULWORD__)out.size(), pos) !=
	   out.size()) { ok = 0; break; }
	pos += (FAU_t)out.size();
	out.clear();
      }
    }
  }
  if(ok) {
    size_t start = out.size();
    jput32(out, gxdJournalCommitMagic);
    jput32(out, salt);
    jput32(out, 1);
    jput32(out, (uint32_t)af.size());
    for(size_t i = 0; i < af.size(); i++) jput64(out, (uint64_t)af[i].size);
    jput32(out, jcheck(out.data() + start + 4, out.size() - start - 4));
    if(gxdFPTRWriteAt(jfp, out.data(), (__ULWORD__)out.size(), pos) !=
       out.size()) ok = 0;
  }
  if(ok && gxdFPTRSync(jfp, 0) != 0) ok = 0;
  if(ok && (gxdFPTRWriteAt(jfp, h.data(), (__ULWORD__)h.size(), (FAU_t)0) !=
	    h.size() || gxdFPTRSync(jfp, 0) != 0)) ok = 0;
  gxdFPTRClose(jfp);
  if(!ok) return gxDBASE_FILE_WRITE_ERROR;
#if defined(GXREPL_TEST_HOOKS) && !defined(_WIN32)
  // 11/13/2026 test hook (test builds only): GXREPL_TEST_APPLY_CRASH=1 ends the process once a batch writing into
  // a gxrepl stage is committed to the journal and before its pages reach the files (a crash inside an apply)
  if(getenv("GXREPL_TEST_APPLY_CRASH"))
    for(size_t i = 0; i < af.size(); i++)
      if(af[i].path.find("/.gxrepl/stage/") != std::string::npos) _exit(9);
  // 11/14/2026: gxrepl's GXREPL_TEST_CRASH_IN_APPLY (the drain-verify variants) asks for this batch
  if(gxdJournalTestCrashNext.exchange(0)) _exit(9);
#endif

  unsigned long n = 0;
  err = Recover(jname, &n, 0);
  if(err != gxDBASE_NO_ERROR) return err;
  if(n != 1) return gxDBASE_FILE_WRITE_ERROR;
  if(pages_written) *pages_written = npages;
  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxdJournalReadLock::Lock(const char *jname)
{
  Unlock();
  if(!jname) return gxDBASE_NULL_PTR;
  jfp = gxdFPTROpen(jname, gxDBASE_READONLY);
  if(!jfp) return gxDBASE_FILE_OPEN_ERROR;
  if(gxdFPTRLock(jfp, 0) != 0) {
    gxdFPTRClose(jfp);
    jfp = 0;
    return gxDBASE_FILELOCK_ERROR;
  }
  return gxDBASE_NO_ERROR;
}

void gxdJournalReadLock::Unlock()
{
  if(jfp) {
    gxdFPTRUnlock(jfp);
    gxdFPTRClose(jfp);
    jfp = 0;
  }
}

} // namespace datareel
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
