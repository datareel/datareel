// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: gxdrepl.cpp
// Compiler Used: GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/07/2026
// Date Last Modified: 11/07/2026
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

Ship log for database replication. See gxdrepl.h. POSIX only (Linux,
the BSDs, macOS); on Windows Open() and the reader return errors.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <algorithm>
#include "gxdrepl.h"
#include "gxdpcache.h"

#if !defined (__WIN32__)
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace datareel {

static const char gxdShipSegMagic[8] = { 'G','X','R','S','E','G','0','1' };
static const uint32_t gxdShipRecMagic = 0x52525847;
static const uint32_t gxdShipMaxPayload = 1024U * 1024 * 1024;

uint64_t gxdReplHash(const void *vp, size_t n, uint64_t h)
{
  const unsigned char *p = (const unsigned char *)vp;
  const uint64_t m = 0x9E3779B97F4A7C15ULL;
  h ^= (uint64_t)n * 0xC2B2AE3D27D4EB4FULL;
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
  h ^= h >> 32;
  h *= m;
  h ^= h >> 29;
  return h;
}

static void sput32(std::vector<unsigned char> &v, uint32_t x)
{
  unsigned char b[4]; memcpy(b, &x, 4); v.insert(v.end(), b, b + 4);
}

static void sput64(std::vector<unsigned char> &v, uint64_t x)
{
  unsigned char b[8]; memcpy(b, &x, 8); v.insert(v.end(), b, b + 8);
}

static uint32_t sget32(const unsigned char *p)
{
  uint32_t x; memcpy(&x, p, 4); return x;
}

static uint64_t sget64(const unsigned char *p)
{
  uint64_t x; memcpy(&x, p, 8); return x;
}

static uint32_t rec_check(const unsigned char *hdr, const unsigned char *pl,
			  size_t len)
{
  uint64_t h = gxdReplHash(hdr + 4, 24, 0x5348);
  if(len) h = gxdReplHash(pl, len, h);
  return (uint32_t)(h ^ (h >> 32));
}

static unsigned long long now_us()
{
#if !defined (__WIN32__)
  struct timeval tv;
  gettimeofday(&tv, 0);
  return (unsigned long long)tv.tv_sec * 1000000ULL + tv.tv_usec;
#else
  return (unsigned long long)time(0) * 1000000ULL;
#endif
}

static std::string hex16(unsigned long long v)
{
  char b[32];
  snprintf(b, sizeof(b), "%016llx", v);
  return std::string(b);
}

// Check one record at "p" (n bytes available). Returns its total size,
// 0 if it is incomplete or damaged.
static size_t check_record(const unsigned char *p, size_t n,
			   gxdShipRecordView *v)
{
  if(n < gxdShipRecordHeaderSize) return 0;
  if(sget32(p) != gxdShipRecMagic) return 0;
  uint32_t len = sget32(p + 24);
  if(len > gxdShipMaxPayload) return 0;
  if(n - gxdShipRecordHeaderSize < len) return 0;
  if(rec_check(p, p + gxdShipRecordHeaderSize, len) != sget32(p + 28))
    return 0;
  if(v) {
    v->type = sget32(p + 4);
    v->lsn = sget64(p + 8);
    v->time_us = sget64(p + 16);
    v->payload = p + gxdShipRecordHeaderSize;
    v->len = len;
  }
  return gxdShipRecordHeaderSize + len;
}

int gxdParseShipRecords(const unsigned char *p, size_t n,
			std::vector<gxdShipRecordView> &recs)
{
  recs.clear();
  size_t off = 0;
  while(off < n) {
    gxdShipRecordView v;
    size_t sz = check_record(p + off, n - off, &v);
    if(sz == 0) return -1;
    recs.push_back(v);
    off += sz;
  }
  return 0;
}

int gxdParseShipCommit(const unsigned char *p, size_t n,
		       std::vector<gxdShipCommitFile> &files)
{
  files.clear();
  if(n < 4) return -1;
  uint32_t nf = sget32(p);
  size_t off = 4;
  if(nf > 4096) return -1;
  for(uint32_t i = 0; i < nf; i++) {
    gxdShipCommitFile f;
    if(n - off < 4) return -1;
    uint32_t nl = sget32(p + off); off += 4;
    if(nl == 0 || nl > 4096 || n - off < (size_t)nl + 12) return -1;
    f.name.assign((const char *)p + off, nl); off += nl;
    f.size = sget64(p + off); off += 8;
    uint32_t np = sget32(p + off); off += 4;
    if((unsigned long long)np * (8 + gxdPageSize) > n - off) return -1;
    f.pages.reserve(np);
    for(uint32_t k = 0; k < np; k++) {
      unsigned long long pg = sget64(p + off); off += 8;
      f.pages.push_back(std::make_pair(pg, p + off));
      off += gxdPageSize;
    }
    files.push_back(f);
  }
  return off == n ? 0 : -1;
}

#if !defined (__WIN32__)

static int mkdirs(const std::string &path)
{
  if(path.empty()) return -1;
  struct stat st;
  if(stat(path.c_str(), &st) == 0) return S_ISDIR(st.st_mode) ? 0 : -1;
  size_t slash = path.find_last_of('/');
  if(slash != std::string::npos && slash > 0) {
    if(mkdirs(path.substr(0, slash)) != 0) return -1;
  }
  if(mkdir(path.c_str(), 0750) != 0 && errno != EEXIST) return -1;
  return 0;
}

static int read_small(const std::string &path, std::string &s)
{
  s.clear();
  FILE *f = fopen(path.c_str(), "rb");
  if(!f) return -1;
  char b[4096];
  size_t n;
  while((n = fread(b, 1, sizeof(b), f)) > 0) {
    s.append(b, n);
    if(s.size() > 16 * 1024 * 1024) break;
  }
  fclose(f);
  return 0;
}

static int write_small(const std::string &path, const std::string &s)
// Atomic: a temp file, synced, renamed over the old one
{
  std::string tmp = path + ".tmp";
  int f = open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0640);
  if(f < 0) return -1;
  size_t off = 0;
  while(off < s.size()) {
    ssize_t w = write(f, s.data() + off, s.size() - off);
    if(w <= 0) { close(f); unlink(tmp.c_str()); return -1; }
    off += (size_t)w;
  }
  if(fdatasync(f) != 0) { close(f); unlink(tmp.c_str()); return -1; }
  close(f);
  if(rename(tmp.c_str(), path.c_str()) != 0) {
    unlink(tmp.c_str());
    return -1;
  }
  // 11/08/2026: The rename survives a power loss only once the folder is synced
  size_t k = path.find_last_of('/');
  int dfd = open(k == std::string::npos ? "." : path.substr(0, k).c_str(), O_RDONLY);
  if(dfd >= 0) { fsync(dfd); close(dfd); }
  return 0;
}

static std::string real_path(const std::string &p)
{
  char b[PATH_MAX];
  if(realpath(p.c_str(), b)) return std::string(b);
  return std::string();
}

static std::string random_id()
{
  unsigned long long v = 0;
  int f = open("/dev/urandom", O_RDONLY);
  if(f >= 0) {
    if(read(f, &v, sizeof(v)) != (ssize_t)sizeof(v)) v = 0;
    close(f);
  }
  if(v == 0) v = now_us() ^ ((unsigned long long)getpid() << 32);
  return hex16(v);
}

std::string gxdShipLog::StreamName(const std::string &rel)
{
  std::string s;
  for(size_t i = 0; i < rel.size(); i++) {
    char c = rel[i];
    if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
       (c >= '0' && c <= '9') || c == '-' || c == '.') s += c;
    else s += '_';
  }
  while(!s.empty() && s[0] == '.') s[0] = '_';
  if(s.empty()) s = "journal";
  if(s.size() > 200) s = s.substr(s.size() - 200);
  return s;
}

std::string gxdShipLog::RelativePath(const std::string &root,
				     const std::string &full)
{
  if(root.empty() || full.size() <= root.size() + 1) return std::string();
  if(full.compare(0, root.size(), root) != 0) return std::string();
  if(full[root.size()] != '/') return std::string();
  return full.substr(root.size() + 1);
}

gxdShipLog::gxdShipLog()
{
  fd = lock_fd = -1;
  next_lsn = 1;
  seg_first = seg_end = 0;
  seg_size = gxdShipDefaultSegmentSize;
  max_bytes_kept = gxdShipDefaultMaxBytes;
  keep_days = gxdShipDefaultKeepDays;
  sync = 0;
  pending_gap = 0;
  appends = failures = 0;
}

gxdShipLog::~gxdShipLog()
{
  Close();
}

int gxdShipLog::fail(const std::string &msg)
{
  error = msg;
  return -1;
}

void gxdShipLog::Close()
{
  if(fd >= 0) { close(fd); fd = -1; }
  if(lock_fd >= 0) { flock(lock_fd, LOCK_UN); close(lock_fd); lock_fd = -1; }
}

int gxdShipLog::Open(const char *set_root, const char *stream,
		     const char *jname)
{
  Close();
  error.clear();
  if(!set_root || !jname) return fail("no set root or journal name");
  root = real_path(set_root);
  if(root.empty()) return fail(std::string("no such folder: ") + set_root);
  std::string jfull = real_path(jname);
  if(jfull.empty()) jfull = jname;
  std::string jrel = RelativePath(root, jfull);
  if(stream && *stream) stream_name = StreamName(stream);
  else stream_name = StreamName(jrel.empty() ? jfull : jrel);
  dir = root + "/.gxrepl/log/" + stream_name;
  if(mkdirs(dir) != 0) return fail("cannot make " + dir);

  lock_fd = open((dir + "/writer.lock").c_str(), O_RDWR | O_CREAT, 0640);
  if(lock_fd < 0) return fail("cannot open " + dir + "/writer.lock");
  if(flock(lock_fd, LOCK_EX | LOCK_NB) != 0) {
    close(lock_fd); lock_fd = -1;
    return fail("another writer holds " + dir + "/writer.lock");
  }

  std::string s;
  if(read_small(dir + "/log.id", s) == 0 && s.size() >= 16) {
    log_id = s.substr(0, 16);
  }
  else {
    // A new log: segments left from an earlier one go
    DIR *od = opendir(dir.c_str());
    if(od) {
      struct dirent *e;
      while((e = readdir(od)) != 0) {
	std::string n = e->d_name;
	if(n.size() == 20 && n.compare(16, 4, ".gxl") == 0)
	  unlink((dir + "/" + n).c_str());
      }
      closedir(od);
    }
    log_id = random_id();
    if(write_small(dir + "/log.id", log_id + "\n") != 0) {
      Close();
      return fail("cannot write " + dir + "/log.id");
    }
  }
  write_small(dir + "/journal", (jrel.empty() ? jfull : jrel) + "\n");
  files_list.clear();
  if(read_small(dir + "/files", s) == 0) {
    size_t a = 0;
    while(a < s.size()) {
      size_t b = s.find('\n', a);
      if(b == std::string::npos) b = s.size();
      if(b > a) files_list.push_back(s.substr(a, b - a));
      a = b + 1;
    }
  }

  // Find the end of the log: the last valid record of the newest
  // segment. A torn record at the end is cut off.
  gxdShipLogReader rd;
  next_lsn = 1;
  if(rd.Open(dir) == 0) {
    gxdShipLogReader::Info info;
    if(rd.GetInfo(info) == 0 && info.log_id == log_id)
      next_lsn = info.next_lsn;
  }
  // The newest segment, opened for appending at its last valid record
  DIR *d = opendir(dir.c_str());
  unsigned long long newest = 0;
  int found = 0;
  if(d) {
    struct dirent *e;
    while((e = readdir(d)) != 0) {
      std::string n = e->d_name;
      if(n.size() != 20 || n.compare(16, 4, ".gxl") != 0) continue;
      unsigned long long v = strtoull(n.substr(0, 16).c_str(), 0, 16);
      if(!found || v > newest) { newest = v; found = 1; }
    }
    closedir(d);
  }
  if(found) {
    std::string path = dir + "/" + hex16(newest) + ".gxl";
    int f = open(path.c_str(), O_RDWR);
    if(f >= 0) {
      unsigned char h[gxdShipSegmentHeaderSize];
      if(pread(f, h, sizeof(h), 0) == (ssize_t)sizeof(h) &&
	 memcmp(h, gxdShipSegMagic, 8) == 0 &&
	 std::string((const char *)h + 8, 16) == log_id) {
	// Walk to the last valid record
	unsigned long long off = gxdShipSegmentHeaderSize;
	unsigned char rh[gxdShipRecordHeaderSize];
	std::vector<unsigned char> pl;
	while(pread(f, rh, sizeof(rh), (off_t)off) == (ssize_t)sizeof(rh)) {
	  if(sget32(rh) != gxdShipRecMagic) break;
	  uint32_t len = sget32(rh + 24);
	  if(len > gxdShipMaxPayload) break;
	  pl.resize(len);
	  if(len && pread(f, pl.data(), len, (off_t)(off + sizeof(rh))) !=
	     (ssize_t)len) break;
	  if(rec_check(rh, pl.data(), len) != sget32(rh + 28)) break;
	  off += sizeof(rh) + len;
	}
	if(ftruncate(f, (off_t)off) == 0) {
	  fd = f;
	  seg_first = newest;
	  seg_end = off;
	}
	else close(f);
      }
      else close(f);
    }
  }
  if(append(gxdShipOpen, 0, 0) != 0) {
    std::string e = error;
    Close();
    return fail(e);
  }
  return 0;
}

int gxdShipLog::open_segment()
{
  if(fd >= 0) { close(fd); fd = -1; }
  std::string path = dir + "/" + hex16(next_lsn) + ".gxl";
  int f = open(path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0640);
  if(f < 0) return fail("cannot make " + path);
  unsigned char h[gxdShipSegmentHeaderSize];
  memset(h, 0, sizeof(h));
  memcpy(h, gxdShipSegMagic, 8);
  memcpy(h + 8, log_id.data(), 16);
  uint64_t first = next_lsn;
  memcpy(h + 24, &first, 8);
  if(pwrite(f, h, sizeof(h), 0) != (ssize_t)sizeof(h)) {
    close(f);
    unlink(path.c_str());
    return fail("cannot write " + path);
  }
  fd = f;
  seg_first = next_lsn;
  seg_end = sizeof(h);
  // The new segment's name must survive a crash with its records
  int dfd = open(dir.c_str(), O_RDONLY);
  if(dfd >= 0) { fsync(dfd); close(dfd); }
  purge();
  return 0;
}

void gxdShipLog::purge()
// Remove whole segments older than keep_days, then the oldest while the
// log is over max_bytes_kept. The current segment always stays.
{
  struct SegF { unsigned long long first; std::string path;
    unsigned long long size; time_t mtime; };
  std::vector<SegF> segs;
  DIR *d = opendir(dir.c_str());
  if(!d) return;
  struct dirent *e;
  while((e = readdir(d)) != 0) {
    std::string n = e->d_name;
    if(n.size() != 20 || n.compare(16, 4, ".gxl") != 0) continue;
    SegF s;
    s.first = strtoull(n.substr(0, 16).c_str(), 0, 16);
    s.path = dir + "/" + n;
    struct stat st;
    if(stat(s.path.c_str(), &st) != 0) continue;
    s.size = (unsigned long long)st.st_size;
    s.mtime = st.st_mtime;
    segs.push_back(s);
  }
  closedir(d);
  std::sort(segs.begin(), segs.end(),
	    [](const SegF &a, const SegF &b) { return a.first < b.first; });
  unsigned long long total = 0;
  for(auto &s : segs) total += s.size;
  time_t cutoff = time(0) - (time_t)keep_days * 86400;
  // 11/08/2026: The ACK floor. Each copy's replicator writes the last lsn
  // the copy applied durably to acked/<copy> (gxrepl). An old segment goes
  // only when every copy has all its records; the size cap still wins (a
  // copy that needs a dropped record is verified whole instead).
  unsigned long long floor = 0;
  bool have_floor = false;
  DIR *ad = opendir((dir + "/acked").c_str());
  if(ad) {
    struct dirent *ae;
    while((ae = readdir(ad)) != 0) {
      if(ae->d_name[0] == '.') continue;
      std::string v;
      if(read_small(dir + "/acked/" + ae->d_name, v) != 0) continue;
      unsigned long long l = strtoull(v.c_str(), 0, 10);
      if(!have_floor || l < floor) { floor = l; have_floor = true; }
    }
    closedir(ad);
  }
  for(size_t i = 0; i < segs.size(); i++) {
    SegF &s = segs[i];
    if(s.first == seg_first) break;
    unsigned long long next_first = i + 1 < segs.size() ? segs[i + 1].first : next_lsn;
    int acked = !have_floor || next_first <= floor + 1;   // Every record in it applied by every copy
    int old = keep_days > 0 && s.mtime < cutoff && acked;
    int big = max_bytes_kept > 0 && total > max_bytes_kept;
    if(!old && !big) break;
    if(unlink(s.path.c_str()) == 0) total -= s.size;
  }
}

int gxdShipLog::append(uint32_t type, const unsigned char *payload,
		       size_t len)
{
  if(lock_fd < 0) return fail("the ship log is not open");
  if(len > gxdShipMaxPayload) return fail("record too large");
  if(fd < 0 || seg_end >= seg_size) {
    if(open_segment() != 0) return -1;
  }
  unsigned char h[gxdShipRecordHeaderSize];
  uint32_t magic = gxdShipRecMagic, l32 = (uint32_t)len;
  uint64_t lsn = next_lsn, t = now_us();
  memcpy(h, &magic, 4);
  memcpy(h + 4, &type, 4);
  memcpy(h + 8, &lsn, 8);
  memcpy(h + 16, &t, 8);
  memcpy(h + 24, &l32, 4);
  uint32_t check = rec_check(h, payload, len);
  memcpy(h + 28, &check, 4);
  int ok = pwrite(fd, h, sizeof(h), (off_t)seg_end) == (ssize_t)sizeof(h);
  if(ok && len) {
    size_t off = 0;
    while(ok && off < len) {
      ssize_t w = pwrite(fd, payload + off, len - off,
			 (off_t)(seg_end + sizeof(h) + off));
      if(w <= 0) ok = 0;
      else off += (size_t)w;
    }
  }
  if(ok && sync && fdatasync(fd) != 0) ok = 0;
  if(!ok) {
    if(ftruncate(fd, (off_t)seg_end) != 0) { close(fd); fd = -1; }
    return fail("cannot write the ship log in " + dir);
  }
  seg_end += sizeof(h) + len;
  next_lsn++;
  return 0;
}

int gxdShipLog::write_files_list(const std::vector<std::string> &names)
{
  std::string s;
  for(auto &n : names) s += n + "\n";
  if(write_small(dir + "/files", s) != 0)
    return fail("cannot write " + dir + "/files");
  files_list = names;
  return 0;
}

int gxdShipLog::AppendCommit(const std::vector<gxdShipFile> &files)
{
  if(lock_fd < 0) return fail("the ship log is not open");
  int rc = 0;
  if(pending_gap) {
    if(append(gxdShipGap, 0, 0) != 0) { failures++; return -1; }
    pending_gap = 0;
  }
  std::vector<std::string> names;
  buf.clear();
  sput32(buf, (uint32_t)files.size());
  for(size_t i = 0; i < files.size(); i++) {
    std::string rel = RelativePath(root, files[i].path);
    if(rel.empty()) {
      pending_gap = 1;
      failures++;
      return fail(files[i].path + " is outside the set " + root);
    }
    names.push_back(rel);
    sput32(buf, (uint32_t)rel.size());
    buf.insert(buf.end(), rel.begin(), rel.end());
    sput64(buf, (uint64_t)files[i].size);
    size_t np = files[i].pages ? files[i].pages->size() : 0;
    sput32(buf, (uint32_t)np);
    for(size_t k = 0; k < np; k++) {
      const gxdPage *pg = (*files[i].pages)[k];
      sput64(buf, (uint64_t)pg->page_no);
      buf.insert(buf.end(), pg->data, pg->data + gxdPageSize);
    }
  }
  std::vector<std::string> sorted = names;
  std::sort(sorted.begin(), sorted.end());
  sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
  if(sorted != files_list) {
    // New files join the stream before their first record
    std::vector<std::string> all = files_list;
    all.insert(all.end(), sorted.begin(), sorted.end());
    std::sort(all.begin(), all.end());
    all.erase(std::unique(all.begin(), all.end()), all.end());
    if(all != files_list && write_files_list(all) != 0) rc = -1;
  }
  if(append(gxdShipCommit, buf.data(), buf.size()) != 0) {
    pending_gap = 1;
    failures++;
    return -1;
  }
  appends++;
  return rc;
}

// ---------------- Reader ----------------

int gxdShipLogReader::Open(const std::string &stream_dir)
{
  dir = stream_dir;
  error.clear();
  pos_path.clear();
  pos_off = pos_lsn = 0;
  std::string s;
  if(read_small(dir + "/log.id", s) != 0 || s.size() < 16) {
    error = "no log.id in " + dir;
    return -1;
  }
  log_id = s.substr(0, 16);
  return 0;
}

int gxdShipLogReader::list_segments(std::vector<Seg> &segs)
{
  segs.clear();
  DIR *d = opendir(dir.c_str());
  if(!d) { error = "cannot read " + dir; return -1; }
  struct dirent *e;
  while((e = readdir(d)) != 0) {
    std::string n = e->d_name;
    if(n.size() != 20 || n.compare(16, 4, ".gxl") != 0) continue;
    Seg s;
    s.first = strtoull(n.substr(0, 16).c_str(), 0, 16);
    s.path = dir + "/" + n;
    segs.push_back(s);
  }
  closedir(d);
  std::sort(segs.begin(), segs.end(),
	    [](const Seg &a, const Seg &b) { return a.first < b.first; });
  return 0;
}

// Walk the records of one segment from "off" (lsn "lsn"), calling
// "fn(view, offset)" for each valid record until fn returns false.
// Returns the offset and lsn after the last record walked.
template <class F>
static int walk_segment(int f, const std::string &log_id,
			unsigned long long &off, unsigned long long &lsn,
			F fn)
{
  if(off == 0) {
    unsigned char h[gxdShipSegmentHeaderSize];
    if(pread(f, h, sizeof(h), 0) != (ssize_t)sizeof(h)) return -1;
    if(memcmp(h, gxdShipSegMagic, 8) != 0) return -1;
    if(std::string((const char *)h + 8, 16) != log_id) return -1;
    off = gxdShipSegmentHeaderSize;
    lsn = sget64(h + 24);
  }
  std::vector<unsigned char> rec;
  unsigned char rh[gxdShipRecordHeaderSize];
  while(pread(f, rh, sizeof(rh), (off_t)off) == (ssize_t)sizeof(rh)) {
    if(sget32(rh) != gxdShipRecMagic) break;
    uint32_t len = sget32(rh + 24);
    if(len > gxdShipMaxPayload) break;
    if(sget64(rh + 8) != lsn) break;
    rec.resize(sizeof(rh) + len);
    memcpy(rec.data(), rh, sizeof(rh));
    if(len && pread(f, rec.data() + sizeof(rh), len,
		    (off_t)(off + sizeof(rh))) != (ssize_t)len) break;
    if(rec_check(rec.data(), rec.data() + sizeof(rh), len) != sget32(rh + 28))
      break;
    if(!fn(rec, lsn)) return 0;
    off += rec.size();
    lsn++;
  }
  return 0;
}

int gxdShipLogReader::scan_end(const std::vector<Seg> &segs,
			       unsigned long long &next)
{
  next = 1;
  if(segs.empty()) return 0;
  const Seg &last = segs.back();
  int f = open(last.path.c_str(), O_RDONLY);
  if(f < 0) { error = "cannot open " + last.path; return -1; }
  unsigned long long off = 0, lsn = last.first;
  if(pos_path == last.path && pos_lsn >= last.first) {
    off = pos_off; lsn = pos_lsn;
  }
  int rc = walk_segment(f, log_id, off, lsn,
			[](const std::vector<unsigned char> &,
			   unsigned long long) { return true; });
  close(f);
  if(rc != 0) {
    // A segment of another log id: nothing readable after it
    next = last.first;
    return 0;
  }
  pos_path = last.path;
  pos_off = off;
  pos_lsn = lsn;
  next = lsn;
  return 0;
}

int gxdShipLogReader::GetInfo(Info &info)
{
  info = Info();
  std::string s;
  if(read_small(dir + "/log.id", s) != 0 || s.size() < 16) {
    error = "no log.id in " + dir;
    return -1;
  }
  if(s.substr(0, 16) != log_id) {
    log_id = s.substr(0, 16); // The log was made again
    pos_path.clear();
  }
  info.log_id = log_id;
  if(read_small(dir + "/journal", s) == 0) {
    while(!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    info.journal = s;
  }
  if(read_small(dir + "/files", s) == 0) {
    size_t a = 0;
    while(a < s.size()) {
      size_t b = s.find('\n', a);
      if(b == std::string::npos) b = s.size();
      if(b > a) info.files.push_back(s.substr(a, b - a));
      a = b + 1;
    }
  }
  std::vector<Seg> segs;
  if(list_segments(segs) != 0) return -1;
  if(segs.empty()) return 0;
  info.first_lsn = segs.front().first;
  if(scan_end(segs, info.next_lsn) != 0) return -1;
  if(info.next_lsn <= info.first_lsn) info.first_lsn = 0;
  return 0;
}

int gxdShipLogReader::Read(unsigned long long from, size_t max_bytes,
			   std::vector<unsigned char> &out,
			   unsigned long long &next, unsigned long &count)
{
  out.clear();
  count = 0;
  next = from;
  std::vector<Seg> segs;
  if(list_segments(segs) != 0) return -1;
  if(segs.empty()) return from <= 1 ? 0 : 1;
  if(from < segs.front().first) return 1;
  size_t i = 0;
  for(size_t k = 0; k < segs.size(); k++) if(segs[k].first <= from) i = k;
  for(; i < segs.size(); i++) {
    int f = open(segs[i].path.c_str(), O_RDONLY);
    if(f < 0) {
      if(i + 1 < segs.size()) return 1; // Purged under us
      error = "cannot open " + segs[i].path;
      return -1;
    }
    unsigned long long off = 0, lsn = segs[i].first;
    if(pos_path == segs[i].path && pos_lsn <= from && pos_lsn >= lsn) {
      off = pos_off; lsn = pos_lsn;
    }
    int full = 0;
    int rc = walk_segment(f, log_id, off, lsn,
      [&](const std::vector<unsigned char> &rec, unsigned long long l) {
	if(l < from) return true;
	if(count > 0 && out.size() + rec.size() > max_bytes) {
	  full = 1;
	  return false;
	}
	out.insert(out.end(), rec.begin(), rec.end());
	count++;
	next = l + 1;
	return true;
      });
    close(f);
    if(rc != 0) {
      if(count == 0) return 1; // A segment of another log id
      return 0;
    }
    pos_path = segs[i].path;
    pos_off = off;
    pos_lsn = lsn;
    if(full) return 0;
  }
  if(count == 0 && next < from) next = from;
  return 0;
}

#else // __WIN32__

std::string gxdShipLog::StreamName(const std::string &rel) { return rel; }
std::string gxdShipLog::RelativePath(const std::string &, const std::string &)
{ return std::string(); }
gxdShipLog::gxdShipLog() { fd = lock_fd = -1; }
gxdShipLog::~gxdShipLog() { }
void gxdShipLog::Close() { }
int gxdShipLog::Open(const char *, const char *, const char *)
{ error = "replication is not supported on Windows"; return -1; }
int gxdShipLog::AppendCommit(const std::vector<gxdShipFile> &) { return -1; }
int gxdShipLogReader::Open(const std::string &) { return -1; }
int gxdShipLogReader::GetInfo(Info &) { return -1; }
int gxdShipLogReader::Read(unsigned long long, size_t,
			   std::vector<unsigned char> &, unsigned long long &,
			   unsigned long &) { return -1; }

#endif // __WIN32__

} // namespace datareel
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
