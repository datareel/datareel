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
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
USA

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
#include "gxdlcode.h"

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <time.h>
#include <map>
#include "gxdjrnl.h"
#include "gxbtree.h"
#include "btcache.h"


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
}

gxdJournal::~gxdJournal()
{
  Close();
}

gxDatabaseError gxdJournal::Recover(const char *jname,
				    unsigned long *recovered)
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
  if(ncommits) {
    std::vector<gxdFPTR *> fps(paths.size(), (gxdFPTR *)0);
    for(size_t i = 0; i < paths.size(); i++) {
      if(!has_size[i]) continue;
      fps[i] = gxdFPTROpen(paths[i].c_str(), gxDBASE_READWRITE);
      if(!fps[i]) { err = gxDBASE_FILE_OPEN_ERROR; break; }
    }
    for(auto &e : committed) {
      if(err != gxDBASE_NO_ERROR) break;
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
  // 10/31/2026 (6.3): The journal holds copies of database pages until a
  // checkpoint, so only the owner may read it: a new journal is created
  // owner-only (mode 0600, or a protected DACL for the current user on
  // Windows) and an existing one is made owner-only before it is used.
  if(gxdFPTRExists(name)) {
    if(gxdFPTRMakePrivate(name) != 0) return jerror = gxDBASE_FILE_OPEN_ERROR;
    jfp = gxdFPTROpen(name, gxDBASE_READWRITE);
  }
  else {
    jfp = gxdFPTRCreatePrivate(name);
  }
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
  files.clear();
  caches.clear();
  // After a clean checkpoint the journal holds nothing to recover
  int clean = jfp && (err == gxDBASE_NO_ERROR) && !header_written;
  if(clean) {
    gxdFPTRTruncate(jfp, (FAU_t)0);
  }
  if(jfp) {
    gxdFPTRClose(jfp);
    jfp = 0;
  }
  // 10/31/2026 (6.3): A clean close leaves no journal file behind. If it
  // cannot be removed (on Windows while another process has it open for
  // a gxdJournalReadLock) it stays, empty.
  if(clean && !jname.empty()) remove(jname.c_str());
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
    for(size_t i = 0; i < files.size(); i++)
      files[i].f->PageCache()->CommitTxn();
  }
  in_txn = 0;
  commits++;
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

  // Reload the in-memory headers from the committed pages
  for(size_t i = 0; i < files.size(); i++) {
    files[i].f->NoteFileChanged();
    files[i].f->ReadFileHdr();
    if(files[i].btx) files[i].btx->ReadBtreeHeader();
  }
  return jerror = gxDBASE_NO_ERROR;
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

// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
