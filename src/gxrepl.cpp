// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: gxrepl.cpp
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
version 3 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library. If not, see
<https://www.gnu.org/licenses/>.

Database replication over gxStream. See gxrepl.h. POSIX only.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include "gxrepl.h"

#if !defined (__WIN32__)

#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <algorithm>
#include <chrono>
#include <list>
#include <deque>
#include <functional>
#include <set>
#include <thread>
#include "gxdjrnl.h"
#include <sys/syscall.h>
#include "gxdpcache.h"

#if defined (DATAREEL_HAVE_ZLIB)
#include <zlib.h>
#endif

namespace datareel {

// ---------------- Small helpers ----------------

static long long now_s() { return (long long)time(0); }

static long long now_ns()
{
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static std::mutex log_lock;

// 11/12/2026 (F4c): a child gxrepl starts (the derived rebuild, the ssh tunnel) gets only stdin/stdout/stderr. Without
// this it inherited every fd not opened O_CLOEXEC (gxrepl.lock, ship log state files, writer locks ...): an orphaned
// rebuild then held gxrepl.lock, and a restarted gxrepl could not start until it ended. Called between fork and exec
// (async-signal-safe calls only).
static void close_inherited_fds()
{
#ifdef SYS_close_range
  if(syscall(SYS_close_range, 3U, ~0U, 0U) == 0) return;
#endif
  long mx = sysconf(_SC_OPEN_MAX);
  if(mx <= 0 || mx > 1048576) mx = 65536;
  for(long fd = 3; fd < mx; fd++) close((int)fd);
}

// sh -c cmd with stdin/stdout/stderr on /dev/null and no other fd: the wait status (system()'s), -1 if it could not run
static int run_quiet(const std::string &cmd)
{
  pid_t pid = fork();
  if(pid < 0) return -1;
  if(pid == 0) {
    int nul = open("/dev/null", O_RDWR);
    if(nul >= 0) { dup2(nul, 0); dup2(nul, 1); dup2(nul, 2); if(nul > 2) close(nul); }
    close_inherited_fds();
    execl("/bin/sh", "sh", "-c", cmd.c_str(), (char *)0);
    _exit(127);
  }
  int status = 0;
  while(waitpid(pid, &status, 0) < 0) { if(errno != EINTR) return -1; }
  return status;
}

// 11/12/2026 (F4a/F4b): the apply lock (apply.jnl, exclusive) taken with a bounded wait instead of a blocking flock.
// On 11/12 a derived index rebuild held it for 5-10 min (a busy rebuild of a big day on a 2-core node); an apply
// blocked in flock while it held its set's mutex and writer holds, the other requests piled up behind that mutex and
// the multiplexed connection had no free worker left: all 11 sets stopped. Now an apply that cannot get the lock in
// GXREPL_APPLY_LOCK_WAIT seconds (5) answers "busy" and lets go of everything; the client tries again next pass, and
// only that set waits for the rebuild. 0 = locked, else errno-like (EWOULDBLOCK = busy).
static int lock_apply_bounded(int fd)
{
  double wait = 5;
  if(const char *e = getenv("GXREPL_APPLY_LOCK_WAIT")) { double v = atof(e); if(v >= 0) wait = v; }
  auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds((long)(wait * 1000));
  while(flock(fd, LOCK_EX | LOCK_NB) != 0) {
    if(errno != EWOULDBLOCK && errno != EINTR) return errno;
    if(std::chrono::steady_clock::now() >= end) return EWOULDBLOCK;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  return 0;
}

static void glog(const char *fmt, ...)
{
  char tb[64];
  time_t t = time(0);
  struct tm tmv;
  localtime_r(&t, &tmv);
  strftime(tb, sizeof(tb), "%Y-%m-%d %H:%M:%S", &tmv);
  char msg[2048];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof(msg), fmt, ap);
  va_end(ap);
  std::lock_guard<std::mutex> g(log_lock);
  fprintf(stderr, "%s gxrepl: %s\n", tb, msg);
  fflush(stderr);
}

static std::string trim(const std::string &s)
{
  size_t a = s.find_first_not_of(" \t\r\n");
  if(a == std::string::npos) return std::string();
  size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

static std::vector<std::string> split_list(const std::string &s)
{
  std::vector<std::string> v;
  std::string cur;
  for(char c : s) {
    if(c == ',' ) { cur = trim(cur); if(!cur.empty()) v.push_back(cur); cur.clear(); }
    else cur += c;
  }
  cur = trim(cur);
  if(!cur.empty()) v.push_back(cur);
  return v;
}

static std::string expand_env(const std::string &s)
{
  std::string out;
  for(size_t i = 0; i < s.size(); i++) {
    if(s[i] == '$' && i + 1 < s.size() && s[i + 1] == '{') {
      size_t e = s.find('}', i + 2);
      if(e != std::string::npos) {
	const char *v = getenv(s.substr(i + 2, e - i - 2).c_str());
	if(v) out += v;
	i = e;
	continue;
      }
    }
    out += s[i];
  }
  return out;
}

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

static std::string real_path(const std::string &p)
{
  char b[PATH_MAX];
  if(realpath(p.c_str(), b)) return std::string(b);
  return std::string();
}

// 11/08/2026: fsync a folder, so a create, rename or remove in it is durable
static void sync_dir(const std::string &dir)
{
  int dfd = open(dir.empty() ? "." : dir.c_str(), O_RDONLY | O_DIRECTORY);
  if(dfd >= 0) { fsync(dfd); close(dfd); }
}

static int write_atomic(const std::string &path, const std::string &s)
{
  std::string tmp = path + ".gxrtmp";
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
  if(rename(tmp.c_str(), path.c_str()) != 0) { unlink(tmp.c_str()); return -1; }
  // 11/08/2026: The applied-lsn state must survive a power loss: sync the folder
  sync_dir(path.substr(0, path.find_last_of('/')));
  return 0;
}

// 11/10/2026: helpers for the commit-block units and the .gxrepl size
static std::string dir_of(const std::string &rel)
{
  size_t k = rel.find_last_of('/');
  return k == std::string::npos ? std::string() : rel.substr(0, k);
}

static std::string base_of(const std::string &rel)
{
  size_t k = rel.find_last_of('/');
  return k == std::string::npos ? rel : rel.substr(k + 1);
}

static unsigned long long dir_bytes(const std::string &path)
{
  unsigned long long n = 0;
  DIR *d = opendir(path.c_str());
  if(!d) return 0;
  struct dirent *e;
  while((e = readdir(d)) != 0) {
    std::string name = e->d_name;
    if(name == "." || name == "..") continue;
    std::string p = path + "/" + name;
    struct stat st;
    if(lstat(p.c_str(), &st) != 0) continue;
    if(S_ISDIR(st.st_mode)) n += dir_bytes(p);
    else if(S_ISREG(st.st_mode)) n += (unsigned long long)st.st_size;
  }
  closedir(d);
  return n;
}

static std::string json_str(const std::string &s)
{
  std::string o = "\"";
  for(unsigned char c : s) {
    if(c == '"' || c == '\\') { o += '\\'; o += (char)c; }
    else if(c == '\n') o += "\\n";
    else if(c < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", c); o += b; }
    else o += (char)c;
  }
  return o + "\"";
}

static bool interruptible_sleep(int ms, const std::atomic<bool> &stop)
{
  while(ms > 0 && !stop) {
    int step = ms > 100 ? 100 : ms;
    std::this_thread::sleep_for(std::chrono::milliseconds(step));
    ms -= step;
  }
  return !stop;
}

// ---------------- Wire buffers ----------------

struct WBuf {
  std::vector<unsigned char> b;
  void raw(const void *p, size_t n) {
    const unsigned char *c = (const unsigned char *)p; b.insert(b.end(), c, c + n);
  }
  void u8(uint8_t x) { b.push_back(x); }
  void u32(uint32_t x) { raw(&x, 4); }
  void u64(uint64_t x) { raw(&x, 8); }
  void i64(int64_t x) { raw(&x, 8); }
  void str(const std::string &s) { u32((uint32_t)s.size()); raw(s.data(), s.size()); }
  void bytes(const std::vector<unsigned char> &v) {
    u64((uint64_t)v.size()); if(!v.empty()) raw(v.data(), v.size());
  }
};

struct RBuf {
  const unsigned char *p; size_t n; size_t off = 0; bool ok = true;
  RBuf(const unsigned char *pp, size_t nn) : p(pp), n(nn) { }
  bool need(size_t k) { if(!ok || n - off < k) { ok = false; return false; } return true; }
  uint8_t u8() { if(!need(1)) return 0; return p[off++]; }
  uint32_t u32() { uint32_t x = 0; if(need(4)) { memcpy(&x, p + off, 4); off += 4; } return x; }
  uint64_t u64() { uint64_t x = 0; if(need(8)) { memcpy(&x, p + off, 8); off += 8; } return x; }
  int64_t i64() { int64_t x = 0; if(need(8)) { memcpy(&x, p + off, 8); off += 8; } return x; }
  std::string str() {
    uint32_t l = u32(); if(!need(l)) return std::string();
    std::string s((const char *)p + off, l); off += l; return s;
  }
  void bytes(std::vector<unsigned char> &v) {
    uint64_t l = u64(); v.clear(); if(!need((size_t)l)) return;
    v.assign(p + off, p + off + l); off += (size_t)l;
  }
};

static void enc(WBuf &w, const gxReplFile &f)
{ w.str(f.path); w.u64(f.size); w.i64(f.mtime_ns); w.u32(f.mode); w.u8(f.covered); }
static void dec(RBuf &r, gxReplFile &f)
{ f.path = r.str(); f.size = r.u64(); f.mtime_ns = r.i64(); f.mode = r.u32(); f.covered = r.u8(); }

static void enc(WBuf &w, const gxReplStream &s)
{
  w.str(s.name); w.str(s.log_id); w.str(s.journal); w.u64(s.first_lsn); w.u64(s.next_lsn);
  w.u32((uint32_t)s.files.size()); for(auto &f : s.files) w.str(f);
}
static void dec(RBuf &r, gxReplStream &s)
{
  s.name = r.str(); s.log_id = r.str(); s.journal = r.str();
  s.first_lsn = r.u64(); s.next_lsn = r.u64();
  uint32_t n = r.u32(); s.files.clear();
  for(uint32_t i = 0; i < n && r.ok; i++) s.files.push_back(r.str());
}

static void enc(WBuf &w, const gxReplStreamState &s) { w.str(s.name); w.str(s.log_id); w.u64(s.lsn); }
static void dec(RBuf &r, gxReplStreamState &s) { s.name = r.str(); s.log_id = r.str(); s.lsn = r.u64(); }

static void enc(WBuf &w, const gxReplFileUpdate &u)
{
  w.str(u.path); w.u64(u.size); w.i64(u.mtime_ns); w.u32(u.mode); w.u8(u.last);
  w.u32((uint32_t)u.pieces.size());
  for(auto &p : u.pieces) { w.u64(p.offset); w.bytes(p.data); }
  w.str(u.stream); w.str(u.log_id); w.u64(u.lsn); w.bytes(u.records);
}
static void dec(RBuf &r, gxReplFileUpdate &u)
{
  u.path = r.str(); u.size = r.u64(); u.mtime_ns = r.i64(); u.mode = r.u32(); u.last = r.u8();
  uint32_t n = r.u32(); u.pieces.clear();
  for(uint32_t i = 0; i < n && r.ok; i++) {
    gxReplPiece p; p.offset = r.u64(); r.bytes(p.data); u.pieces.push_back(std::move(p));
  }
  u.stream = r.str(); u.log_id = r.str(); u.lsn = r.u64(); r.bytes(u.records);
}

// ---------------- Messages ----------------

#if defined (DATAREEL_HAVE_ZLIB)
static const uint32_t have_zlib = 1;
#else
static const uint32_t have_zlib = 0;
#endif

enum {
  OP_HELLO = 1, OP_STREAMS = 10, OP_READLOG = 11, OP_SMANIFEST = 12,
  OP_DIFF = 13, OP_COMMITGEN = 14, OP_MARKGEN = 15, OP_SEALUNIT = 16, OP_SEALGEN = 17, OP_STATE = 20, OP_TMANIFEST = 21, OP_HASHES = 22,
  OP_APPLYLOG = 23, OP_APPLYFILES = 24, OP_DELETE = 25, OP_SETSTATE = 26,
  OP_OK = 100, OP_ERR = 101
};

// 11/08/2026: A message (op, flags, raw length, body; zlib when it pays)
// as bytes: the body of a gxStream 'S' block (protocol 2) or of a
// multiplex message (protocol 3)
static void pack_msg(uint8_t op, const std::vector<unsigned char> &body, int level,
		     std::vector<unsigned char> &m)
{
  m.clear();
  m.reserve(body.size() + 6);
  m.push_back(op);
  uint32_t raw = (uint32_t)body.size();
  int packed = 0;
#if defined (DATAREEL_HAVE_ZLIB)
  if(level > 0 && body.size() > 512) {
    uLongf cap = compressBound((uLong)body.size());
    std::vector<unsigned char> z(cap);
    if(compress2(z.data(), &cap, body.data(), (uLong)body.size(), level) == Z_OK &&
       cap < body.size()) {
      m.push_back(1);
      m.insert(m.end(), (unsigned char *)&raw, (unsigned char *)&raw + 4);
      m.insert(m.end(), z.data(), z.data() + cap);
      packed = 1;
    }
  }
#else
  (void)level;
#endif
  if(!packed) {
    m.push_back(0);
    m.insert(m.end(), (unsigned char *)&raw, (unsigned char *)&raw + 4);
    m.insert(m.end(), body.begin(), body.end());
  }
}

static int send_msg(gxStream &s, uint8_t op, const std::vector<unsigned char> &body,
		    int level, unsigned long long *bytes_out, std::string &err)
{
  std::vector<unsigned char> m;
  pack_msg(op, body, level, m);
  gxBlockHeader gx;
  s.SetBlockStatus(gx, gxSendBlock);
  gx.block_length = (gxUINT32)m.size();
  if(s.WriteBlock(m.data(), gx) != gxSOCKET_NO_ERROR) {
    err = std::string("send failed: ") + s.SocketExceptionMessage();
    return -1;
  }
  if(bytes_out) *bytes_out += m.size();
  return 0;
}

// 11/08/2026: A message from a block already read (the database object
// server reads a session's first block to choose the service)
static int unpack_msg(const std::vector<unsigned char> &m, uint8_t &op,
		      std::vector<unsigned char> &body, size_t max_raw, std::string &err);

static int decode_msg(const gxBlockHeader &gx, const std::vector<unsigned char> &m, uint8_t &op,
		      std::vector<unsigned char> &body, size_t max_raw, std::string &err)
{
  if(gxStream::BlockStatus(gx) == gxCloseConnection) { err.clear(); return 1; }
  if(gxStream::BlockStatus(gx) != gxSendBlock) { err = "unexpected block"; return -1; }
  return unpack_msg(m, op, body, max_raw, err);
}

// 11/08/2026: The message bytes back to op and body
static int unpack_msg(const std::vector<unsigned char> &m, uint8_t &op,
		      std::vector<unsigned char> &body, size_t max_raw, std::string &err)
{
  if(m.size() < 6) { err = "short message"; return -1; }
  op = m[0];
  uint8_t flags = m[1];
  uint32_t raw;
  memcpy(&raw, m.data() + 2, 4);
  if(raw > max_raw) { err = "message too large"; return -1; }
  if(flags & 1) {
#if defined (DATAREEL_HAVE_ZLIB)
    body.resize(raw);
    uLongf dl = raw;
    if(uncompress(body.data(), &dl, m.data() + 6, (uLong)(m.size() - 6)) != Z_OK ||
       dl != raw) { err = "bad compressed message"; return -1; }
#else
    err = "compressed message, but built without zlib"; return -1;
#endif
  }
  else {
    if(m.size() - 6 != raw) { err = "bad message length"; return -1; }
    body.assign(m.begin() + 6, m.end());
  }
  return 0;
}

static int recv_msg(gxStream &s, uint8_t &op, std::vector<unsigned char> &body,
		    size_t max_raw, unsigned long long *bytes_in, std::string &err)
{
  gxBlockHeader gx;
  if(s.ReadHeader(gx) != gxSOCKET_NO_ERROR) {
    err = std::string("receive failed: ") + s.SocketExceptionMessage();
    return -1;
  }
  if(gxStream::BlockStatus(gx) == gxCloseConnection) { err.clear(); return 1; }
  if(gxStream::BlockStatus(gx) != gxSendBlock) { err = "unexpected block"; return -1; }
  std::vector<unsigned char> m;
  if(s.ReadBlock(m, gx) != gxSOCKET_NO_ERROR || m.size() < 6) {
    err = std::string("receive failed: ") + s.SocketExceptionMessage();
    return -1;
  }
  if(bytes_in) *bytes_in += m.size();
  return decode_msg(gx, m, op, body, max_raw, err);
}

int gxReplHasZlib() { return (int)have_zlib; }

// ---------------- Configuration ----------------

int gxReplConfig::Load(const std::string &fname, std::string &err)
{
  FILE *f = fopen(fname.c_str(), "r");
  if(!f) { err = "cannot open " + fname; return -1; }
  char line[8192];
  int ln = 0;
  gxReplSetConfig *cur = 0;
  gxObjectDbConfig *curdb = 0;   // 11/08/2026: [db NAME] sections (the object service)
  sets.clear();
  object_dbs.clear();
  while(fgets(line, sizeof(line), f)) {
    ln++;
    std::string s = trim(line);
    if(s.empty() || s[0] == '#') continue;
    if(s[0] == '[') {
      size_t e = s.find(']');
      std::string sec = trim(s.substr(1, e == std::string::npos ? std::string::npos : e - 1));
      if(sec.compare(0, 3, "db ") == 0 && !trim(sec.substr(3)).empty()) {
	gxObjectDbConfig dc;
	dc.name = trim(sec.substr(3));
	for(auto &o : object_dbs) if(o.name == dc.name) {
	  fclose(f); err = fname + ": db " + dc.name + " twice"; return -1;
	}
	object_dbs.push_back(dc);
	curdb = &object_dbs.back();
	cur = 0;
	continue;
      }
      curdb = 0;
      if(sec.compare(0, 4, "set ") != 0 || trim(sec.substr(4)).empty()) {
	fclose(f);
	err = fname + ":" + std::to_string(ln) + ": expected [set NAME] or [db NAME]";
	return -1;
      }
      gxReplSetConfig sc;
      sc.name = trim(sec.substr(4));
      for(auto &o : sets) if(o.name == sc.name) {
	fclose(f); err = fname + ": set " + sc.name + " twice"; return -1;
      }
      sets.push_back(sc);
      cur = &sets.back();
      continue;
    }
    size_t eq = s.find('=');
    if(eq == std::string::npos) {
      fclose(f);
      err = fname + ":" + std::to_string(ln) + ": expected key = value";
      return -1;
    }
    std::string k = trim(s.substr(0, eq)), v = expand_env(trim(s.substr(eq + 1)));
    bool known = true;
    if(curdb) {
      if(k == "path") curdb->path = v;
      else if(k == "journal") curdb->journal = v;
      else if(k == "access") curdb->access = v;
      else known = false;
    }
    else if(cur) {
      if(k == "root") cur->root = v;
      else if(k == "direction") cur->direction = v;
      else if(k == "accept") cur->accept = v;
      else if(k == "scan_interval") cur->scan_interval = atoi(v.c_str());
      else if(k == "verify_interval") cur->verify_interval = atoi(v.c_str());
      else if(k == "verify_recent_hours") cur->verify_recent_hours = atoi(v.c_str());
      else if(k == "full_verify_interval") cur->full_verify_interval = atoi(v.c_str());
      else if(k == "exclude") { auto l = split_list(v); cur->exclude.insert(cur->exclude.end(), l.begin(), l.end()); }
      else if(k == "writer_lock") { auto l = split_list(v); cur->writer_locks.insert(cur->writer_locks.end(), l.begin(), l.end()); }
      else if(k == "deletes") cur->deletes = (v == "yes" || v == "true" || v == "1");
      else if(k == "include") { auto l = split_list(v); cur->include.insert(cur->include.end(), l.begin(), l.end()); }
      else if(k == "commit_block") {         // 11/10/2026: NAME[@OFFSET] (offset of the u64 generation, default 8)
	size_t at = v.find('@');
	cur->commit_block = v.substr(0, at);
	if(at != std::string::npos) cur->commit_gen_offset = (uint32_t)strtoul(v.c_str() + at + 1, 0, 10);
	if(cur->commit_block.empty() || cur->commit_block.find('/') != std::string::npos || cur->commit_gen_offset > 4088) {
	  fclose(f); err = fname + ":" + std::to_string(ln) + ": commit_block must be NAME@OFFSET (a file name, offset 0-4088)";
	  return -1;
	}
      }
      else if(k == "ship_log_max_total") cur->ship_log_max_total = strtoull(v.c_str(), 0, 10);
      else if(k == "derived") { auto l = split_list(v); cur->derived.insert(cur->derived.end(), l.begin(), l.end()); }
      else if(k == "derived_rebuild") cur->derived_rebuild = v;
      else if(k == "derived_delay") cur->derived_delay = atoi(v.c_str());
      else if(k == "derived_busy_rebuild") cur->derived_busy_rebuild = atoi(v.c_str());   // 11/11/2026
      else known = false;
    }
    else {
      if(k == "node") node = v;
      else if(k == "legacy_db") legacy_db = v;   // 11/08/2026: 6.x style object clients
      else if(k == "role") role = v;
      else if(k == "connect") {
	size_t c = v.rfind(':');
	if(c != std::string::npos) { connect_host = v.substr(0, c); connect_port = atoi(v.c_str() + c + 1); }
	else connect_host = v;
      }
      else if(k == "server_name") server_name = v;
      else if(k == "ssh_tunnel") ssh_tunnel = v;
      else if(k == "ssh_command") ssh_command = v;
      else if(k == "ssh_option") ssh_options.push_back(v);
      else if(k == "ssh_local_port") ssh_local_port = atoi(v.c_str());
      else if(k == "ssh_remote") ssh_remote = v;
      else if(k == "poll_ms") poll_ms = atoi(v.c_str());
      else if(k == "listen") {
	size_t c = v.rfind(':');
	if(c != std::string::npos) { listen_address = v.substr(0, c); listen_port = atoi(v.c_str() + c + 1); }
	else listen_port = atoi(v.c_str());
	if(listen_address == "*" || listen_address == "0.0.0.0") listen_address.clear();
      }
      else if(k == "allow_cn") { auto l = split_list(v); allow_cn.insert(allow_cn.end(), l.begin(), l.end()); }
      else if(k == "allow_fingerprint") { auto l = split_list(v); allow_fingerprint.insert(allow_fingerprint.end(), l.begin(), l.end()); }
      else if(k == "max_clients") max_clients = atoi(v.c_str());
      else if(k == "max_handshakes") max_handshakes = atoi(v.c_str());
      else if(k == "handshake_timeout") handshake_timeout = atoi(v.c_str());
      else if(k == "cert") cert = v;
      else if(k == "key") key = v;
      else if(k == "key_passphrase") key_passphrase = v;
      else if(k == "ca") ca = v;
      else if(k == "status_file") status_file = v;
      else if(k == "timeout") timeout = atoi(v.c_str());
      else if(k == "max_message") max_message = (size_t)atol(v.c_str());
      else if(k == "compress") {
	// yes / no, or a zlib level 1-9
	if(v == "yes" || v == "true" || v == "on") compress = 1;
	else if(v == "no" || v == "false" || v == "off") compress = 0;
	else compress = atoi(v.c_str());
      }
      else known = false;
    }
    if(!known) {
      fclose(f);
      err = fname + ":" + std::to_string(ln) + ": unknown key " + k;
      return -1;
    }
  }
  fclose(f);
  if(role != "client" && role != "server") { err = "role must be client or server"; return -1; }
  if(node.empty()) { err = "node is not set"; return -1; }
  // The key may come later from --key-fd (gxrepl.sh passes it on a pipe)
  if(cert.empty() || ca.empty()) { err = "cert and ca are required"; return -1; }
  if(role == "client" && connect_host.empty() && ssh_tunnel.empty()) {
    err = "connect is not set"; return -1;
  }
  if(role == "server" && allow_cn.empty() && allow_fingerprint.empty()) {
    err = "allow_cn or allow_fingerprint is required"; return -1;
  }
  if(max_message < 64 * 1024) max_message = 64 * 1024;
  if(max_message > 64 * 1024 * 1024) max_message = 64 * 1024 * 1024;
  if(poll_ms < 50) poll_ms = 50;
  if(timeout < 5) timeout = 5;
  if(handshake_timeout < 2) handshake_timeout = 2;
  if(max_handshakes < 1) max_handshakes = 1;
  for(auto &sc : sets) {
    if(sc.root.empty()) { err = "set " + sc.name + ": root is not set"; return -1; }
    if(sc.direction != "push" && sc.direction != "pull") {
      err = "set " + sc.name + ": direction must be push or pull"; return -1;
    }
    if(sc.accept != "target" && sc.accept != "source" && sc.accept != "both") {
      err = "set " + sc.name + ": accept must be target, source or both"; return -1;
    }
    if(sc.scan_interval < 1) sc.scan_interval = 1;
    for(char c : sc.name) {
      if(!(isalnum((unsigned char)c) || c == '_' || c == '-' || c == '.')) {
	err = "set name " + sc.name + ": letters, digits, _ - . only"; return -1;
      }
    }
  }
  return 0;
}

const gxReplSetConfig *gxReplConfig::Set(const std::string &name) const
{
  for(auto &s : sets) if(s.name == name) return &s;
  return 0;
}

// ---------------- Local sets ----------------

struct gxReplLocal::SetState_ {
  gxReplSetConfig sc;
  std::string root;
  std::mutex m;
  std::map<std::string, std::unique_ptr<gxdShipLogReader> > readers;
};

static const char *default_excludes[] = {
  "*.jnl", "*.lock", "*.gxrtmp", "*.tmp", "*~", ".nfs*", 0
};

static bool has_databento(const std::string &rel)
{
  size_t a = 0;
  while(a <= rel.size()) {
    size_t b = rel.find('/', a);
    if(b == std::string::npos) b = rel.size();
    std::string c = rel.substr(a, b - a);
    std::string lc;
    for(char ch : c) lc += (char)tolower((unsigned char)ch);
    if(lc.find("databento") != std::string::npos) return true;
    a = b + 1;
  }
  return false;
}

// 11/10/2026: a derived index (gxReplSetConfig::derived), by name or path
static bool is_derived(const gxReplSetConfig &sc, const std::string &rel)
{
  if(sc.derived.empty()) return false;
  std::string base = rel.substr(rel.find_last_of('/') == std::string::npos ? 0 : rel.find_last_of('/') + 1);
  for(auto &p : sc.derived)
    if(fnmatch(p.c_str(), base.c_str(), 0) == 0 || fnmatch(p.c_str(), rel.c_str(), 0) == 0) return true;
  return false;
}

static bool excluded(const gxReplSetConfig &sc, const std::string &rel)
{
  if(is_derived(sc, rel)) return true;   // 11/10/2026: rebuilt by the copy, never sent or deleted
  std::string base = rel.substr(rel.find_last_of('/') == std::string::npos ? 0 : rel.find_last_of('/') + 1);
  for(int i = 0; default_excludes[i]; i++) {
    if(fnmatch(default_excludes[i], base.c_str(), 0) == 0) return true;
  }
  for(auto &p : sc.exclude) {
    // "/var/" or "/var" (rsync style): only that path under the root and what is in it
    if(!p.empty() && p[0] == '/') {
      std::string a = p.substr(1);
      if(!a.empty() && a.back() == '/') a.pop_back();
      if(fnmatch(a.c_str(), rel.c_str(), 0) == 0) return true;
      if(fnmatch((a + "/*").c_str(), rel.c_str(), 0) == 0) return true;
      continue;
    }
    std::string q = p;
    if(q.size() > 1 && q.back() == '/') q.pop_back();  // "logs/": a folder named logs anywhere
    if(fnmatch(q.c_str(), base.c_str(), 0) == 0) return true;
    if(fnmatch(q.c_str(), rel.c_str(), 0) == 0) return true;
    // A file inside an excluded folder (any level)
    for(size_t i = rel.find('/'); i != std::string::npos; i = rel.find('/', i + 1)) {
      std::string dir = rel.substr(0, i);
      std::string dbase = dir.substr(dir.find_last_of('/') == std::string::npos ? 0 : dir.find_last_of('/') + 1);
      if(q != p && fnmatch(q.c_str(), dbase.c_str(), 0) == 0) return true;
    }
  }
  return has_databento(rel);
}

// 11/08/2026: gxrepl carries DataReel databases only: .gxd and .btx files,
// and the files a set names with "include" (pattern on the path or the name)
static bool is_db_file(const gxReplSetConfig &sc, const std::string &rel)
{
  size_t k = rel.find_last_of('/');
  std::string base = k == std::string::npos ? rel : rel.substr(k + 1);
  size_t dot = base.find_last_of('.');
  std::string ext = dot == std::string::npos ? "" : base.substr(dot);
  if(ext == ".gxd" || ext == ".btx") return true;
  for(auto &p : sc.include) {
    if(fnmatch(p.c_str(), base.c_str(), 0) == 0) return true;
    if(fnmatch(p.c_str(), rel.c_str(), 0) == 0) return true;
  }
  return false;
}

// 11/10/2026: A unit's seal: <root>/.gxrepl/units/<folder key>: "copying"
// from the first mark of a copy (MarkGen), then, written atomically before
// the commit block lands, "gen G" and, per database file of the
// folder (not the commit block, not derived), "path<TAB>size<TAB>hash". A copy
// is whole when its commit block's generation is G and every file matches.
static std::string unit_key(const std::string &rel)
{
  std::string n = rel.empty() ? std::string("_root") : "_" + rel;
  for(auto &c : n) if(c == '/') c = '#';
  return n;
}

static int file_hash(const std::string &path, unsigned long long &size, uint64_t &h)
{
  int fd = open(path.c_str(), O_RDONLY);
  if(fd < 0) return -1;
  std::vector<unsigned char> b(1 << 20);
  h = 0x5EA1ULL; size = 0;
  ssize_t n;
  while((n = read(fd, b.data(), b.size())) > 0) { h = gxdReplHash(b.data(), (size_t)n, h); size += (unsigned long long)n; }
  close(fd);
  return n < 0 ? -1 : 0;
}

// The files of a unit folder a seal lists, sorted
static std::vector<std::string> unit_files(const gxReplSetConfig &sc, const std::string &root, const std::string &rel)
{
  std::vector<std::string> out;
  std::string dir = rel.empty() ? root : root + "/" + rel;
  DIR *d = opendir(dir.c_str());
  if(!d) return out;
  struct dirent *e;
  while((e = readdir(d)) != 0) {
    std::string n = e->d_name;
    if(n.empty() || n[0] == '.' || n == sc.commit_block) continue;
    struct stat st;
    if(lstat((dir + "/" + n).c_str(), &st) != 0 || !S_ISREG(st.st_mode)) continue;
    std::string r = rel.empty() ? n : rel + "/" + n;
    if(!is_db_file(sc, r) || excluded(sc, r)) continue;
    out.push_back(r);
  }
  closedir(d);
  std::sort(out.begin(), out.end());
  return out;
}

static std::string unit_manifest(const gxReplSetConfig &sc, const std::string &root, const std::string &rel,
				 uint64_t gen, bool &ok)
{
  ok = true;
  std::string m = "gen " + std::to_string((unsigned long long)gen) + "\n";
  for(auto &r : unit_files(sc, root, rel)) {
    unsigned long long size = 0; uint64_t h = 0;
    if(file_hash(root + "/" + r, size, h) != 0) { ok = false; continue; }
    char hx[17]; snprintf(hx, sizeof(hx), "%016llx", (unsigned long long)h);
    m += r + "\t" + std::to_string(size) + "\t" + hx + "\n";
  }
  return m;
}

// "": sealed and the same; else why not
static std::string unit_seal_check(const gxReplSetConfig &sc, const std::string &rel, uint64_t gen)
{
  std::string path = sc.root + "/.gxrepl/units/" + unit_key(rel), have;
  FILE *f = fopen(path.c_str(), "rb");
  if(!f) return "never sealed (no whole copy since the seal was added)";
  char b[65536]; size_t n;
  while((n = fread(b, 1, sizeof(b), f)) > 0) have.append(b, n);
  fclose(f);
  unsigned long long sg = 0;
  if(have.compare(0, 11, "copying-log") == 0)
    return "ship log records changed it without its commit block (an older writer?): torn until a record carries it";
  if(have.compare(0, 7, "copying") == 0) return "a copy of it is under way, or stopped half way";
  if(have.compare(0, 4, "log ") == 0) {             // fed by the ship log: whole at the record that sealed it
    unsigned long long lg = 0, ll = 0;
    char sname[256] = {0};
    int got = sscanf(have.c_str(), "log %llu %llu %255s", &lg, &ll, sname);
    if(got < 2) return "a damaged seal";
    if(lg != gen) return "sealed by ship log record " + std::to_string(ll) + " at generation " + std::to_string(lg) +
		    ", the commit block says " + std::to_string((unsigned long long)gen);
    // 11/10/2026 (review): and no record past the seal changed it unsealed: the stream's applied lsn is the seal's
    if(got < 3) return "an old seal without its stream (sealed before the stream check)";
    FILE *sf = fopen((sc.root + "/.gxrepl/state/" + sname + ".state").c_str(), "r");
    if(!sf) return std::string("no applied position for stream ") + sname;
    char id[64]; unsigned long long al = 0;
    int sg = fscanf(sf, "%63s %llu", id, &al);
    fclose(sf);
    if(sg != 2) return std::string("a damaged position for stream ") + sname;
    if(al != ll) return "sealed at ship log record " + std::to_string(ll) + ", the copy applied up to " +
		   std::to_string(al) + " (records past the seal)";
    return "";
  }
  if(sscanf(have.c_str(), "gen %llu", &sg) != 1) return "a damaged seal";
  if(sg != gen) return "sealed at generation " + std::to_string(sg) + ", the commit block says " + std::to_string((unsigned long long)gen);
  bool ok = true;
  std::string now = unit_manifest(sc, sc.root, rel, gen, ok);
  if(!ok) return "a file cannot be read";
  if(now != have) return "its files differ from the seal of generation " + std::to_string(sg);
  return "";
}


static void walk(const gxReplSetConfig &sc, const std::string &root,
		 const std::string &rel, std::vector<gxReplFile> &v, int depth)
{
  if(depth > 64) return;
  std::string dir = rel.empty() ? root : root + "/" + rel;
  DIR *d = opendir(dir.c_str());
  if(!d) return;
  struct dirent *e;
  std::vector<std::string> names;
  while((e = readdir(d)) != 0) {
    std::string n = e->d_name;
    if(n == "." || n == "..") continue;
    if(rel.empty() && n == ".gxrepl") continue;
    // 11/08/2026: Hidden names are never replicated (publish leftovers
    // such as .patterns.tmp-old-..., editor and NFS files)
    if(n[0] == '.') continue;
    names.push_back(n);
  }
  closedir(d);
  std::sort(names.begin(), names.end());
  for(auto &n : names) {
    std::string r = rel.empty() ? n : rel + "/" + n;
    struct stat st;
    if(lstat((root + "/" + r).c_str(), &st) != 0) continue;
    if(S_ISDIR(st.st_mode)) {
      if(has_databento(r)) continue;
      // A folder named by an exclude pattern ("var/osa_sip_manager") is not walked
      bool skip = false;
      for(auto &p : sc.exclude) {
	if(!p.empty() && p[0] == '/') {
	  std::string a = p.substr(1);
	  if(!a.empty() && a.back() == '/') a.pop_back();
	  if(fnmatch(a.c_str(), r.c_str(), 0) == 0) { skip = true; break; }
	  continue;
	}
	std::string q = p;
	if(q.size() > 1 && q.back() == '/') q.pop_back();
	if(fnmatch(q.c_str(), r.c_str(), 0) == 0 || fnmatch(q.c_str(), n.c_str(), 0) == 0) { skip = true; break; }
      }
      if(skip) continue;
      walk(sc, root, r, v, depth + 1);
    }
    else if(S_ISREG(st.st_mode)) {
      if(excluded(sc, r)) continue;
      if(!is_db_file(sc, r)) continue;   // 11/08/2026: databases only
      gxReplFile f;
      f.path = r;
      f.size = (unsigned long long)st.st_size;
      f.mtime_ns = (long long)st.st_mtim.tv_sec * 1000000000LL + st.st_mtim.tv_nsec;
      f.mode = st.st_mode & 07777;
      v.push_back(f);
    }
  }
}

gxReplLocal::gxReplLocal(const gxReplConfig &c) : cfg(c) { }

gxReplLocal::~gxReplLocal()
{
  for(auto &e : sets) delete e.second;
}

gxReplLocal::SetState_ *gxReplLocal::set_state(const std::string &name,
					       std::string &err)
{
  std::lock_guard<std::mutex> g(lock);
  auto it = sets.find(name);
  if(it != sets.end()) return it->second;
  const gxReplSetConfig *sc = cfg.Set(name);
  if(!sc) { err = "no set " + name + " on " + cfg.node; return 0; }
  std::string root = real_path(sc->root);
  if(root.empty()) {
    if(mkdirs(sc->root) != 0) { err = "cannot make " + sc->root; return 0; }
    root = real_path(sc->root);
    if(root.empty()) { err = "no folder " + sc->root; return 0; }
  }
  SetState_ *s = new SetState_;
  s->sc = *sc;
  s->root = root;
  sets[name] = s;
  return s;
}

int gxReplLocal::SafePath(const gxReplSetConfig &sc, const std::string &rel,
			  std::string &full, std::string &err) const
{
  if(rel.empty() || rel.size() > 4000 || rel[0] == '/' ||
     rel.find('\0') != std::string::npos) {
    err = "bad path " + rel; return -1;
  }
  size_t a = 0;
  while(a <= rel.size()) {
    size_t b = rel.find('/', a);
    if(b == std::string::npos) b = rel.size();
    std::string c = rel.substr(a, b - a);
    if(c.empty() || c == "." || c == "..") { err = "bad path " + rel; return -1; }
    if(a == 0 && c == ".gxrepl") { err = "refused path " + rel; return -1; }
    if(c[0] == '.') { err = "refused hidden path " + rel; return -1; }   // 11/08/2026
    a = b + 1;
  }
  if(excluded(sc, rel)) { err = "excluded path " + rel; return -1; }
  // 11/08/2026: Both sides refuse anything that is not a database file
  if(!is_db_file(sc, rel)) { err = "not a database file: " + rel; return -1; }
  std::string root = real_path(sc.root);
  if(root.empty()) { err = "no folder " + sc.root; return -1; }
  full = root + "/" + rel;
  // No symlink may lead out of the set: check the deepest existing folder
  std::string dir = full.substr(0, full.find_last_of('/'));
  while(dir.size() > root.size()) {
    struct stat st;
    if(lstat(dir.c_str(), &st) == 0) {
      std::string rp = real_path(dir);
      if(rp.empty() || (rp != root && rp.compare(0, root.size() + 1, root + "/") != 0)) {
	err = "path leaves the set: " + rel; return -1;
      }
      break;
    }
    dir = dir.substr(0, dir.find_last_of('/'));
  }
  struct stat st;
  if(lstat(full.c_str(), &st) == 0 && !S_ISREG(st.st_mode)) {
    err = "not a regular file: " + rel; return -1;
  }
  return 0;
}

// 11/10/2026 (review): the writer locks held (shared) for the whole of a
// change to the copy: writer_active() checked them and let go at once, so a
// writer starting between the check and the apply (a split brain) could
// share the files with it. A writer's exclusive lock now waits or fails
// until the change is done.
struct WriterHold {
  std::vector<int> fds;
  ~WriterHold() { for(int fd : fds) { flock(fd, LOCK_UN); close(fd); } }
};

static int hold_writer_locks(const gxReplSetConfig &sc, WriterHold &h, std::string &err)
{
  for(auto &rel : sc.writer_locks) {
    std::string p = sc.root + "/" + rel;
    int fd = open(p.c_str(), O_RDONLY | O_CLOEXEC);
    // 11/11/2026 (review): a lock file that does not exist yet is made (in a folder that exists), so the hold
    // covers a writer that starts during this change too (it would have created the file and locked it)
    if(fd < 0 && errno == ENOENT) {
      std::string dir = p.substr(0, p.find_last_of('/'));
      struct stat dst;
      if(stat(dir.c_str(), &dst) == 0 && S_ISDIR(dst.st_mode)) fd = open(p.c_str(), O_RDONLY | O_CREAT | O_CLOEXEC, 0644);
    }
    if(fd < 0) continue;
    if(flock(fd, LOCK_SH | LOCK_NB) != 0) {
      bool busy = errno == EWOULDBLOCK;
      close(fd);
      if(busy) {
	err = "set " + sc.name + ": a local writer holds " + rel +
	  " (this node is active), refusing to change the copy";
	return 1;
      }
      continue;
    }
    h.fds.push_back(fd);
  }
  return 0;
}

int gxReplLocal::writer_active(const gxReplSetConfig &sc, std::string &err)
{
  for(auto &rel : sc.writer_locks) {
    std::string p = sc.root + "/" + rel;
    int fd = open(p.c_str(), O_RDONLY);
    if(fd < 0) continue;
    int busy = 0;
    if(flock(fd, LOCK_SH | LOCK_NB) != 0) busy = (errno == EWOULDBLOCK);
    else flock(fd, LOCK_UN);
    close(fd);
    if(busy) {
      err = "set " + sc.name + ": a local writer holds " + rel +
	" (this node is active), refusing to change the copy";
      return 1;
    }
  }
  return 0;
}

int gxReplLocal::Streams(const std::string &set, std::vector<gxReplStream> &v,
			 std::string &err)
{
  v.clear();
  SetState_ *s = set_state(set, err);
  if(!s) return -1;
  std::lock_guard<std::mutex> g(s->m);
  std::string logdir = s->root + "/.gxrepl/log";
  DIR *d = opendir(logdir.c_str());
  if(!d) return 0;
  struct dirent *e;
  std::vector<std::string> names;
  while((e = readdir(d)) != 0) {
    std::string n = e->d_name;
    // 11/10/2026: dot names are never streams (.purged-<stream>-<pid> while a purge removes one)
    if(n.empty() || n[0] == '.') continue;
    names.push_back(n);
  }
  closedir(d);
  std::sort(names.begin(), names.end());
  for(auto &n : names) {
    auto &rd = s->readers[n];
    if(!rd) {
      rd.reset(new gxdShipLogReader);
      if(rd->Open(logdir + "/" + n) != 0) { rd.reset(); s->readers.erase(n); continue; }
    }
    gxdShipLogReader::Info info;
    if(rd->GetInfo(info) != 0) continue;
    gxReplStream st;
    st.name = n;
    st.log_id = info.log_id;
    st.journal = info.journal;
    st.first_lsn = info.first_lsn;
    st.next_lsn = info.next_lsn;
    st.files = info.files;
    v.push_back(st);
  }
  return 0;
}

int gxReplLocal::ReadLog(const std::string &set, const std::string &stream,
			 unsigned long long from, size_t max_bytes,
			 std::vector<unsigned char> &out, unsigned long long &next,
			 int &status, std::string &err)
{
  out.clear();
  status = 0;
  next = from;
  SetState_ *s = set_state(set, err);
  if(!s) return -1;
  std::lock_guard<std::mutex> g(s->m);
  auto &rd = s->readers[stream];
  if(!rd) {
    rd.reset(new gxdShipLogReader);
    if(rd->Open(s->root + "/.gxrepl/log/" + stream) != 0) {
      err = rd->Error(); rd.reset(); s->readers.erase(stream); return -1;
    }
  }
  unsigned long count = 0;
  int rc = rd->Read(from, max_bytes, out, next, count);
  if(rc < 0) { err = rd->Error(); return -1; }
  if(rc == 1) status = 1;
  if(rc == 2) status = 2;   // 11/10/2026: the record at "from" is bigger than a message
  return 0;
}

int gxReplLocal::Manifest(const std::string &set, std::vector<gxReplFile> &v,
			  std::string &err)
{
  v.clear();
  SetState_ *s = set_state(set, err);
  if(!s) return -1;
  std::vector<gxReplStream> streams;
  if(Streams(set, streams, err) != 0) return -1;
  std::set<std::string> covered;
  for(auto &st : streams) for(auto &f : st.files) covered.insert(f);
  walk(s->sc, s->root, std::string(), v, 0);
  for(auto &f : v) f.covered = covered.count(f.path) ? 1 : 0;
  return 0;
}

int gxReplLocal::Diff(const std::string &set, const gxReplDiffRequest &rq,
		      gxReplDiffResult &res, std::string &err)
{
  res = gxReplDiffResult();
  SetState_ *s = set_state(set, err);
  if(!s) return -1;
  std::string full;
  if(SafePath(s->sc, rq.path, full, err) != 0) return -1;
  uint32_t piece = rq.piece;
  if(piece < gxdPageSize || piece % gxdPageSize != 0 || piece > 16 * 1024 * 1024) {
    err = "bad piece size"; return -1;
  }
  size_t max_bytes = rq.max_bytes ? rq.max_bytes : cfg.max_message;
  if(max_bytes > cfg.max_message) max_bytes = cfg.max_message;

  // The stream that writes this file, if any
  std::vector<gxReplStream> streams;
  if(Streams(set, streams, err) != 0) return -1;
  const gxReplStream *cover = 0;
  for(auto &st : streams) {
    if(std::find(st.files.begin(), st.files.end(), rq.path) != st.files.end()) {
      cover = &st; break;
    }
  }

  gxdJournalReadLock rl;
  gxReplFileUpdate &u = res.update;
  // 11/10/2026: the 4 MB / 100 ms budget of a request counts from here, so the
  // ship-log read under the lock is part of it
  struct timespec t_start;
  clock_gettime(CLOCK_MONOTONIC, &t_start);
  if(cover) {
    if(!cover->journal.empty()) {
      std::string jp = cover->journal[0] == '/' ? cover->journal : s->root + "/" + cover->journal;
      if(access(jp.c_str(), F_OK) == 0 && rl.Lock(jp.c_str()) != gxDBASE_NO_ERROR) {
	err = "cannot lock " + jp; return -1;
      }
    }
    // The log position the files match while the lock is held
    std::lock_guard<std::mutex> g(s->m);
    auto &rd = s->readers[cover->name];
    if(!rd) {
      rd.reset(new gxdShipLogReader);
      if(rd->Open(s->root + "/.gxrepl/log/" + cover->name) != 0) {
	err = rd->Error(); rd.reset(); return -1;
      }
    }
    gxdShipLogReader::Info info;
    if(rd->GetInfo(info) != 0) { err = rd->Error(); return -1; }
    u.stream = cover->name;
    u.log_id = info.log_id;
    u.lsn = info.next_lsn ? info.next_lsn - 1 : 0;
    const gxReplStreamState &cs = rq.copy_state;
    if(cs.name == u.stream && cs.log_id == u.log_id && cs.lsn < u.lsn) {
      unsigned long long next = 0;
      unsigned long count = 0;
      int rc = rd->Read(cs.lsn + 1, max_bytes, u.records, next, count);
      if(rc != 0 || next != u.lsn + 1 || u.records.size() > max_bytes) {
	u.records.clear();
	res.status = 2; // Too far behind: apply the log first
	return 0;
      }
    }
    // 11/10/2026: a commit block the ship log keeps is never read from its
    // file (the writer changes it outside the journal lock): it is the one
    // the latest record carried, at or before this position. None left in
    // the log: "gone" (the copy keeps its own, and promote-check judges it)
    if(!s->sc.commit_block.empty() && base_of(rq.path) == s->sc.commit_block) {
      std::vector<unsigned char> page;
      unsigned long long fsize = 0;
      bool found = false, stale = false;
      unsigned long long from = u.lsn > 64 ? u.lsn - 63 : 1;
      if(info.first_lsn && from < info.first_lsn) from = info.first_lsn;
      while(from <= u.lsn && info.first_lsn) {
	std::vector<unsigned char> out;
	unsigned long long nx = 0;
	unsigned long cnt = 0;
	int rc = rd->Read(from, cfg.max_message, out, nx, cnt);
	if(rc == 2) { from++; continue; }           // a record bigger than a message: past it
	if(rc != 0 || cnt == 0) break;
	std::vector<gxdShipRecordView> recs;
	if(gxdParseShipRecords(out.data(), out.size(), recs) != 0) break;
	for(auto &r : recs) {
	  if(r.lsn > u.lsn) break;
	  if(r.type != gxdShipCommit && r.type != gxdShipCommitZ) continue;
	  std::vector<unsigned char> storage;
	  std::vector<gxdShipCommitFile> fl;
	  if(gxdParseShipCommitRecord(r, storage, fl) != 0) continue;
	  bool has_cb = false, touches = false;
	  for(auto &f : fl) {
	    if(f.name == rq.path) {
	      for(auto &pp : f.pages) if(pp.first == 0) {
		page.assign(pp.second, pp.second + gxdPageSize);
		fsize = f.size;
		has_cb = true;
	      }
	    }
	    else if(dir_of(f.name) == dir_of(rq.path) && !is_derived(s->sc, f.name)) touches = true;
	  }
	  // 11/10/2026 (review r4c): a later record that changed the folder
	  // without its commit block makes the block at this position unknown
	  if(has_cb) { found = true; stale = false; }
	  else if(touches) stale = true;
	}
	from = nx;
	if(nx == 0) break;
      }
      if(!found || stale) { res.status = 1; return 0; }   // "gone": the copy keeps its own (and its seal)
      struct stat cst;
      u.path = rq.path;
      u.size = fsize;
      bool have_st = stat(full.c_str(), &cst) == 0;
      u.mode = have_st ? (cst.st_mode & 07777) : 0644;
      u.mtime_ns = have_st ? (long long)cst.st_mtim.tv_sec * 1000000000LL + cst.st_mtim.tv_nsec : 0;
      gxReplPiece p;
      p.offset = 0;
      p.data.assign(page.begin(), page.begin() + (size_t)std::min<unsigned long long>(fsize, gxdPageSize));
      u.pieces.push_back(std::move(p));
      res.pieces = 1; res.sent = 1; res.more = 0; res.next = fsize;
      u.last = 1;
      return 0;
    }
  }

  // 11/08/2026: A database file no ship log covers. If its folder has a
  // journal, the writer is live: read under the journal's read lock, so no
  // commit is writing pages while the pieces are read. A folder without a
  // journal is published (written once, swapped in by a rename): the read is
  // checked against a stat taken before and after.
  if(!cover) {
    std::string dir = full.substr(0, full.find_last_of('/'));
    DIR *dd = opendir(dir.c_str());
    std::string jp;
    if(dd) {
      struct dirent *de;
      while((de = readdir(dd)) != 0) {
	std::string n = de->d_name;
	if(n.size() > 4 && n.compare(n.size() - 4, 4, ".jnl") == 0) { jp = dir + "/" + n; break; }
      }
      closedir(dd);
    }
    if(!jp.empty() && rl.Lock(jp.c_str()) != gxDBASE_NO_ERROR) {
      err = "cannot lock " + jp; return -1;
    }
  }

  int fd = open(full.c_str(), O_RDONLY);
  if(fd < 0) {
    if(errno == ENOENT) { res.status = 1; return 0; }
    err = "cannot open " + rq.path; return -1;
  }
  struct stat st0;
  if(fstat(fd, &st0) != 0) { close(fd); err = "cannot stat " + rq.path; return -1; }
  unsigned long long size = (unsigned long long)st0.st_size;
  u.path = rq.path;
  u.size = size;
  u.mode = st0.st_mode & 07777;
  u.mtime_ns = (long long)st0.st_mtim.tv_sec * 1000000000LL + st0.st_mtim.tv_nsec;
  unsigned long long off = (rq.start / piece) * piece;
  size_t sent = u.records.size();
  std::vector<unsigned char> buf(piece);
  res.more = 0;
  // 11/10/2026: A request holds the journal read lock for at most 4 MB of
  // reading or 100 ms, then returns (more) so the writer can commit before
  // the next request: a verify of a large live file never stalls it.
  unsigned long long read_bytes = 0;
  while(off < size) {
    size_t n = (size_t)std::min<unsigned long long>(piece, size - off);
    ssize_t got = pread(fd, buf.data(), n, (off_t)off);
    if(got != (ssize_t)n) {
      // The file shrank while it was read: send what it is now next time
      n = got > 0 ? (size_t)got : 0;
      u.mtime_ns = 0;
      if(n == 0) break;
    }
    uint64_t h = gxdReplHash(buf.data(), n);
    size_t k = (size_t)(off / piece);
    res.pieces++;
    if(k >= rq.hashes.size() || rq.hashes[k] != h) {
      gxReplPiece p;
      p.offset = off;
      p.data.assign(buf.data(), buf.data() + n);
      sent += n;
      u.pieces.push_back(std::move(p));
      res.sent++;
    }
    off += n;
    read_bytes += n;
    if(sent >= max_bytes && off < size) { res.more = 1; break; }
    if(off < size) {
      struct timespec t_now;
      clock_gettime(CLOCK_MONOTONIC, &t_now);
      long long ms = (t_now.tv_sec - t_start.tv_sec) * 1000LL + (t_now.tv_nsec - t_start.tv_nsec) / 1000000;
      // (Test hook GXREPL_TEST_WHOLE_FILE_LOCK=1: the old behavior, the whole file in one request, to compare)
      static const bool whole = getenv("GXREPL_TEST_WHOLE_FILE_LOCK") != 0;
      if(!whole && (read_bytes >= 4ULL * 1024 * 1024 || ms >= 100)) { res.more = 1; break; }
    }
  }
  res.next = off;
  u.last = res.more ? 0 : 1;
  if(!cover) {
    // 11/08/2026: A file that changed while it was read is not sent (it
    // could be torn): status 3, read it again. The path is checked too, so
    // a published folder swapped in by a rename is noticed.
    struct stat st1, st2;
    if(fstat(fd, &st1) != 0 || st1.st_mtim.tv_sec != st0.st_mtim.tv_sec ||
       st1.st_mtim.tv_nsec != st0.st_mtim.tv_nsec || st1.st_size != st0.st_size ||
       stat(full.c_str(), &st2) != 0 || st2.st_ino != st0.st_ino) {
      res = gxReplDiffResult();
      res.status = 3;
      close(fd);
      return 0;
    }
  }
  close(fd);
  return 0;
}

int gxReplLocal::read_states(SetState_ *s,
			     std::map<std::string, gxReplStreamState> &m,
			     std::string &err)
{
  (void)err;
  m.clear();
  std::string dir = s->root + "/.gxrepl/state";
  DIR *d = opendir(dir.c_str());
  if(!d) return 0;
  struct dirent *e;
  while((e = readdir(d)) != 0) {
    std::string n = e->d_name;
    if(n.size() < 7 || n.compare(n.size() - 6, 6, ".state") != 0) continue;
    FILE *f = fopen((dir + "/" + n).c_str(), "r");
    if(!f) continue;
    char id[64]; unsigned long long lsn = 0;
    if(fscanf(f, "%63s %llu", id, &lsn) == 2) {
      gxReplStreamState st;
      st.name = n.substr(0, n.size() - 6);
      st.log_id = id;
      st.lsn = lsn;
      m[st.name] = st;
    }
    fclose(f);
  }
  closedir(d);   // 11/10/2026: was missing: every State() call leaked a folder handle
  return 0;
}

int gxReplLocal::write_state(SetState_ *s, const gxReplStreamState &st,
			     std::string &err)
{
  if(st.name.empty() || st.name.find('/') != std::string::npos || st.name[0] == '.' ||
     st.log_id.empty() || st.log_id.size() > 32 ||
     st.log_id.find_first_of(" \t\r\n/") != std::string::npos) {
    err = "bad stream state"; return -1;
  }
  std::string dir = s->root + "/.gxrepl/state";
  if(mkdirs(dir) != 0) { err = "cannot make " + dir; return -1; }
  std::string body = st.log_id + " " + std::to_string(st.lsn) + "\n";
  if(write_atomic(dir + "/" + st.name + ".state", body) != 0) {
    err = "cannot write the state of " + st.name; return -1;
  }
  return 0;
}

int gxReplLocal::State(const std::string &set, std::vector<gxReplStreamState> &v,
		       std::string &err)
{
  v.clear();
  SetState_ *s = set_state(set, err);
  if(!s) return -1;
  std::lock_guard<std::mutex> g(s->m);
  std::map<std::string, gxReplStreamState> m;
  if(read_states(s, m, err) != 0) return -1;
  for(auto &e : m) v.push_back(e.second);
  return 0;
}

int gxReplLocal::SetState(const std::string &set, const gxReplStreamState &st,
			  std::string &err)
{
  SetState_ *s = set_state(set, err);
  if(!s) return -1;
  std::lock_guard<std::mutex> g(s->m);
  WriterHold wh;                             // 11/10/2026: held until this change is done
  if(hold_writer_locks(s->sc, wh, err)) return -1;
  return write_state(s, st, err);
}

int gxReplLocal::Hashes(const std::string &set, const std::string &path,
			uint32_t piece, std::vector<uint64_t> &h, int &exists,
			std::string &err)
{
  h.clear();
  exists = 0;
  SetState_ *s = set_state(set, err);
  if(!s) return -1;
  if(piece < gxdPageSize || piece % gxdPageSize != 0 || piece > 16 * 1024 * 1024) {
    err = "bad piece size"; return -1;
  }
  std::string full;
  if(SafePath(s->sc, path, full, err) != 0) return -1;
  std::lock_guard<std::mutex> g(s->m);
  int fd = open(full.c_str(), O_RDONLY);
  if(fd < 0) return 0;
  exists = 1;
  std::vector<unsigned char> buf(piece);
  unsigned long long off = 0;
  while(1) {
    ssize_t n = pread(fd, buf.data(), piece, (off_t)off);
    if(n <= 0) break;
    h.push_back(gxdReplHash(buf.data(), (size_t)n));
    off += (unsigned long long)n;
    if((size_t)n < piece) break;
  }
  close(fd);
  return 0;
}

// 11/10/2026: The generation of a seqlock commit block (gxReplSetConfig::commit_block)
int gxReplLocal::CommitGen(const std::string &set, const std::string &path, uint32_t offset,
			   uint64_t &gen, int &exists, std::string &err)
{
  gen = 0;
  exists = 0;
  SetState_ *s = set_state(set, err);
  if(!s) return -1;
  if(offset > 4088) { err = "bad commit block offset"; return -1; }
  std::string full;
  if(SafePath(s->sc, path, full, err) != 0) return -1;
  int fd = open(full.c_str(), O_RDONLY);
  if(fd < 0) return 0;
  uint64_t g = 0;
  ssize_t n = pread(fd, &g, sizeof(g), (off_t)offset);
  close(fd);
  if(n != (ssize_t)sizeof(g)) return 0;   // shorter than the block: as if missing
  exists = 1;
  gen = g;
  return 0;
}

// 11/10/2026: Mark the copy's commit block in progress (a copy of a unit is
// under way): an odd generation, synced. A copy left odd is torn ("gxrepl torn")
int gxReplLocal::MarkGen(const std::string &set, const std::string &path, uint32_t offset,
			 uint64_t gen, std::string &err)
{
  SetState_ *s = set_state(set, err);
  if(!s) return -1;
  if(offset > 4088) { err = "bad commit block offset"; return -1; }
  std::string full;
  if(SafePath(s->sc, path, full, err) != 0) return -1;
  std::lock_guard<std::mutex> g(s->m);
  WriterHold wh;                             // 11/10/2026: held until this change is done
  if(hold_writer_locks(s->sc, wh, err)) return -1;
  // the unit's seal says "copying" until the copy is whole again (SealUnit): the folder's own writers
  // (osa_repl_copy.h in open_stock) refuse to open a folder sealed so with an odd generation
  if(mkdirs(s->root + "/.gxrepl/units") != 0 ||
     write_atomic(s->root + "/.gxrepl/units/" + unit_key(dir_of(path)), "copying\n") != 0) {
    err = "cannot write the seal of " + dir_of(path); return -1;
  }
  int fd = open(full.c_str(), O_WRONLY);
  if(fd < 0) return 0;                       // no copy yet: a folder with no commit block is incomplete anyway
  ssize_t n = pwrite(fd, &gen, sizeof(gen), (off_t)offset);
  int rc = (n == (ssize_t)sizeof(gen) && fdatasync(fd) == 0) ? 0 : -1;
  close(fd);
  if(rc != 0) err = "cannot mark " + path;
  return rc;
}

int gxReplLocal::SealUnit(const std::string &set, const std::string &folder, uint64_t gen, std::string &err)
{
  SetState_ *s = set_state(set, err);
  if(!s) return -1;
  if(s->sc.commit_block.empty()) { err = "set " + set + " has no commit_block"; return -1; }
  std::string full;
  if(SafePath(s->sc, folder.empty() ? s->sc.commit_block : folder + "/" + s->sc.commit_block, full, err) != 0) return -1;
  std::lock_guard<std::mutex> g(s->m);
  WriterHold wh;                             // 11/10/2026: held until this change is done
  if(hold_writer_locks(s->sc, wh, err)) return -1;
  bool ok = true;
  std::string m = unit_manifest(s->sc, s->root, folder, gen, ok);
  if(!ok) { err = "cannot read the files of " + folder; return -1; }
  if(mkdirs(s->root + "/.gxrepl/units") != 0 ||
     write_atomic(s->root + "/.gxrepl/units/" + unit_key(folder), m) != 0) { err = "cannot write the seal of " + folder; return -1; }
  return 0;
}

int gxReplLocal::SealGen(const std::string &set, const std::string &folder, uint64_t &gen, std::string &err)
{
  gen = 0;
  SetState_ *s = set_state(set, err);
  if(!s) return -1;
  FILE *f = fopen((s->root + "/.gxrepl/units/" + unit_key(folder)).c_str(), "rb");
  if(!f) return 0;
  unsigned long long g = 0;
  if(fscanf(f, "gen %llu", &g) == 1) gen = g;
  fclose(f);
  return 0;
}

int gxReplLocal::apply_batch(SetState_ *s,
  std::map<std::string, std::pair<unsigned long long,
  std::map<unsigned long long, const unsigned char *> > > &pages,
  std::string &err)
{
  if(pages.empty()) return 0;
  // 11/10/2026: commit blocks after the files they describe, the unit seals
  // last (one transaction; the order is the order the pages reach the files)
  std::vector<gxdApplyFile> af;
  for(int pass = 0; pass < 3; pass++) {
    for(auto &e : pages) {
      int kind = 0;
      if(e.first.compare(0, s->root.size() + 15, s->root + "/.gxrepl/units/") == 0) kind = 2;
      else if(!s->sc.commit_block.empty() && base_of(e.first) == s->sc.commit_block) kind = 1;
      if(kind != pass) continue;
      std::string dir = e.first.substr(0, e.first.find_last_of('/'));
      if(mkdirs(dir) != 0) { err = "cannot make " + dir; return -1; }
      gxdApplyFile f;
      f.path = e.first;
      f.size = e.second.first;
      f.pages = e.second.second;
      af.push_back(f);
    }
  }
  std::string jdir = s->root + "/.gxrepl";
  if(mkdirs(jdir) != 0) { err = "cannot make " + jdir; return -1; }
  std::string jname = jdir + "/apply.jnl";
  // 11/08/2026: Readers of the copy (the object service) hold
  // gxdJournalReadLock on apply.jnl; an apply holds it exclusively, so a
  // reader never sees a half-applied batch (flock: also between threads)
  int lkfd = open(jname.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
  if(lkfd < 0) { err = "cannot lock " + jname; return -1; }
  if(int le = lock_apply_bounded(lkfd)) {
    close(lkfd);
    err = le == EWOULDBLOCK ? "busy: " + jname + " is held (a derived index rebuild or a reader of the copy); tried again "
                              "next pass" : "cannot lock " + jname;
    return -1;
  }
  struct LockGuard { int fd; ~LockGuard() { flock(fd, LOCK_UN); close(fd); } } lkg{lkfd};
  // At most 1024 files a transaction
  for(size_t i = 0; i < af.size(); i += 1000) {
    std::vector<gxdApplyFile> part(af.begin() + i, af.begin() + std::min(af.size(), i + 1000));
    unsigned long long n = 0;
    gxDatabaseError rc = gxdJournal::ApplyPages(jname.c_str(), part, &n);
    if(rc != gxDBASE_NO_ERROR) {
      err = std::string("apply failed: ") + gxDatabaseExceptionMessage(rc);
      return -1;
    }
    applied_pages += n;
  }
  // Recover() emptied the journal; give the space back
  if(truncate(jname.c_str(), 0) != 0) { }
  // 11/08/2026: Files and folders the batch created are durable only once
  // their folders are synced, up to the set root (the pages are synced)
  std::set<std::string> dirs;
  for(auto &f : af) {
    std::string d = f.path.substr(0, f.path.find_last_of('/'));
    while(d.size() >= s->root.size() && dirs.insert(d).second) {
      if(d.size() == s->root.size()) break;
      d = d.substr(0, d.find_last_of('/'));
    }
  }
  for(auto &d : dirs) sync_dir(d);
  return 0;
}

typedef std::map<std::string, std::pair<unsigned long long,
  std::map<unsigned long long, const unsigned char *> > > PageBatch;

// Add the pages of whole log records to a batch. Returns the last lsn,
// or 0 with "err" set.
// unit_cb (11/10/2026 review): for each commit_block unit a commit record
// changed, whether the LAST such record carried the unit's commit block
static unsigned long long add_records(gxReplLocal &L, const gxReplSetConfig &sc,
				      const std::vector<unsigned char> &records,
				      unsigned long long expect_first,
				      PageBatch &batch, unsigned long long &count,
				      std::string &err, std::deque<std::vector<unsigned char> > &keep,
				      std::map<std::string, bool> *unit_cb = 0)
{
  std::vector<gxdShipRecordView> recs;
  if(gxdParseShipRecords(records.data(), records.size(), recs) != 0) {
    err = "damaged log records"; return 0;
  }
  if(recs.empty()) { err = "no log records"; return 0; }
  unsigned long long want = expect_first;
  for(auto &r : recs) {
    if(r.lsn != want) { err = "log records out of order"; return 0; }
    want++;
    if(r.type != gxdShipCommit && r.type != gxdShipCommitZ) continue;
    std::vector<gxdShipCommitFile> files;
    keep.emplace_back();                     // a compressed payload is inflated here (the batch points into it)
    if(gxdParseShipCommitRecord(r, keep.back(), files) != 0) {
      err = "damaged commit record " + std::to_string(r.lsn); return 0;
    }
    if(unit_cb && !sc.commit_block.empty()) {
      std::map<std::string, bool> here;
      for(auto &f : files) {
	if(is_derived(sc, f.name)) continue;
	bool cb = base_of(f.name) == sc.commit_block;
	bool &h = here[dir_of(f.name)];
	h = h || cb;
      }
      for(auto &hu : here) (*unit_cb)[hu.first] = hu.second;
    }
    for(auto &f : files) {
      if(is_derived(sc, f.name)) continue;   // 11/10/2026: the copy rebuilds it
      std::string full, e2;
      if(L.SafePath(sc, f.name, full, e2) != 0) {
	glog("set %s: skipped %s in record %llu: %s", sc.name.c_str(),
	     f.name.c_str(), r.lsn, e2.c_str());
	continue;
      }
      auto &ent = batch[full];
      ent.first = f.size;
      for(auto &p : f.pages) ent.second[p.first] = p.second;
    }
  }
  count += recs.size();
  return want - 1;
}

// 11/10/2026: For each commit block in a batch that came from the ship log
// (a record's, or the source's copy of a record's), the unit's seal "log G L"
// (its generation, the stream position) joins the batch; apply_batch writes
// it after the commit block, in the same transaction.
static std::string read_seal(const std::string &root, const std::string &unit)
{
  std::string s;
  FILE *f = fopen((root + "/.gxrepl/units/" + unit_key(unit)).c_str(), "rb");
  if(!f) return s;
  char b[64]; size_t n = fread(b, 1, sizeof(b), f);
  fclose(f);
  s.assign(b, n);
  return s;
}

static void put_seal(PageBatch &batch, const std::string &root, const std::string &unit, const std::string &body,
		     std::deque<std::string> &seals)
{
  seals.push_back(body);
  seals.back().resize(gxdPageSize, '\0');
  auto &ent = batch[root + "/.gxrepl/units/" + unit_key(unit)];
  ent.first = body.size();
  ent.second.clear();
  ent.second[0] = (const unsigned char *)seals.back().data();
}

// A seal a verify's fill left ("copying"): only the commit block sent after
// the unit's files clears it. "copying-log" (records changed the unit without
// its commit block) clears at the next record that carries it.
static bool fill_copying(const std::string &seal)
{
  return seal.compare(0, 8, "copying\n") == 0 || seal == "copying";
}

// 11/10/2026 (review): the seals of a ship log apply, in its transaction:
// - a unit whose last changing record lacked its commit block: "copying-log"
//   (its data is past its commit block; torn until a record carries it);
// - a unit whose commit block came (allow()): "log G L S" (generation, lsn,
//   stream);
// - every other unit sealed "log G L S" by this stream: L moves to "lsn" (an
//   open or gap record changes no data), so a seal's L always equals the
//   stream's applied lsn and promote-check can compare them.
static int add_unit_seals(const gxReplSetConfig &sc, const std::string &root, PageBatch &batch,
			  unsigned long long lsn, const std::string &stream,
			  const std::map<std::string, bool> &unit_cb,
			  std::deque<std::string> &seals, std::string &err,
			  const std::function<bool(const std::string &)> &allow)
{
  if(sc.commit_block.empty()) return 0;
  std::vector<std::pair<std::string, std::string> > add;
  std::set<std::string> done;
  for(auto &uc : unit_cb) if(!uc.second) {
    done.insert(uc.first);
    if(!allow(uc.first)) continue;           // a fill's "copying" stays: it is the stronger mark
    add.push_back(std::make_pair(uc.first, std::string("copying-log\n")));
  }
  for(auto &e : batch) {
    if(e.first.compare(0, root.size() + 1, root + "/") != 0) continue;
    std::string rel = e.first.substr(root.size() + 1);
    if(base_of(rel) != sc.commit_block) continue;
    if(done.count(dir_of(rel)) || !allow(dir_of(rel))) continue;
    auto pg = e.second.second.find(0);
    if(pg == e.second.second.end() || e.second.first < sc.commit_gen_offset + 8) continue;
    uint64_t g = 0;
    memcpy(&g, pg->second + sc.commit_gen_offset, 8);
    if(g & 1) continue;                              // a block inside a batch: never a seal
    add.push_back(std::make_pair(dir_of(rel), "log " + std::to_string((unsigned long long)g) + " " +
				 std::to_string(lsn) + " " + stream + "\n"));
    done.insert(dir_of(rel));
  }
  // carry forward: this stream's other "log" seals follow its lsn
  if(!stream.empty()) {
    std::string ud = root + "/.gxrepl/units";
    DIR *d = opendir(ud.c_str());
    struct dirent *de;
    while(d && (de = readdir(d)) != 0) {
      std::string n = de->d_name;
      if(n.empty() || n[0] != '_') continue;
      FILE *f = fopen((ud + "/" + n).c_str(), "rb");
      if(!f) continue;
      char sname[256] = {0}; unsigned long long g = 0, l = 0;
      int got = fscanf(f, "log %llu %llu %255s", &g, &l, sname);
      fclose(f);
      if(got != 3 || stream != sname || l == lsn) continue;
      std::string unit = n == "_root" ? std::string() : n.substr(1);
      for(auto &c : unit) if(c == '#') c = '/';
      if(done.count(unit)) continue;
      add.push_back(std::make_pair(unit, "log " + std::to_string(g) + " " + std::to_string(lsn) + " " + stream + "\n"));
    }
    if(d) closedir(d);
  }
  if(!add.empty() && mkdirs(root + "/.gxrepl/units") != 0) { err = "cannot make the seals folder"; return -1; }
  for(auto &a2 : add) put_seal(batch, root, a2.first, a2.second, seals);
  return 0;
}

int gxReplLocal::ApplyLog(const std::string &set, const std::string &stream,
			  const std::string &log_id,
			  const std::vector<unsigned char> &records,
			  unsigned long long &lsn, std::string &err)
{
  SetState_ *s = set_state(set, err);
  if(!s) return -1;
  std::lock_guard<std::mutex> g(s->m);
  WriterHold wh;                             // 11/10/2026: held until this change is done
  if(hold_writer_locks(s->sc, wh, err)) return -1;
  std::map<std::string, gxReplStreamState> m;
  read_states(s, m, err);
  auto it = m.find(stream);
  if(it == m.end() || it->second.log_id != log_id) {
    err = "stream " + stream + " is not in step (verify first)"; return -1;
  }
  PageBatch batch;
  std::deque<std::vector<unsigned char> > keep;
  unsigned long long count = 0;
  std::map<std::string, bool> unit_cb;
  unsigned long long last = add_records(*this, s->sc, records, it->second.lsn + 1,
					batch, count, err, keep, &unit_cb);
  if(last == 0) return -1;
  // 11/10/2026: the record's commit block (gxdShipLog::SetCommitBlock) makes
  // its unit whole at this record: its seal goes in the same transaction
  std::deque<std::string> seals;
  // (a unit a verify is filling, "copying", stays so: only its commit block, sent after its files, seals it)
  if(add_unit_seals(s->sc, s->root, batch, last, stream, unit_cb, seals, err,
		    [&](const std::string &u) { return !fill_copying(read_seal(s->root, u)); }) != 0) return -1;
  if(apply_batch(s, batch, err) != 0) return -1;
  gxReplStreamState st = it->second;
  st.lsn = last;
  if(write_state(s, st, err) != 0) return -1;
  applied_records += count;
  lsn = last;
  return 0;
}

int gxReplLocal::ApplyFiles(const std::string &set,
			    const std::vector<gxReplFileUpdate> &v,
			    std::vector<int> &results, std::string &err)
{
  results.assign(v.size(), 1);
  SetState_ *s = set_state(set, err);
  if(!s) return -1;
  std::lock_guard<std::mutex> g(s->m);
  WriterHold wh;                             // 11/10/2026: held until this change is done
  if(hold_writer_locks(s->sc, wh, err)) return -1;
  std::map<std::string, gxReplStreamState> states;
  read_states(s, states, err);
  std::map<std::string, gxReplStreamState> changed;
  PageBatch batch;
  std::deque<std::vector<unsigned char> > keep;
  std::list< std::vector<unsigned char> > padded;
  std::vector<std::string> fulls(v.size());
  std::set<std::string> not_in_step;
  std::map<std::string, bool> unit_cb;      // 11/10/2026 (review): see add_records()
  for(size_t i = 0; i < v.size(); i++) {
    const gxReplFileUpdate &u = v[i];
    if(SafePath(s->sc, u.path, fulls[i], err) != 0) return -1;
    if(!u.stream.empty()) {
      auto it = states.find(u.stream);
      bool same = it != states.end() && it->second.log_id == u.log_id;
      // 11/10/2026: an update not in step with the copy's stream (a first
      // fill, a new log, a resync) leaves its unit "copying" (see below)
      if(!same) not_in_step.insert(dir_of(u.path));
      if(same && it->second.lsn != u.lsn) {
	if(it->second.lsn > u.lsn || u.records.empty()) continue; // Stale
	unsigned long long count = 0, e = 0;
	std::string e2;
	e = add_records(*this, s->sc, u.records, it->second.lsn + 1, batch, count, e2, keep, &unit_cb);
	if(e != u.lsn) continue; // Stale
	applied_records += count;
      }
      gxReplStreamState ns;
      ns.name = u.stream; ns.log_id = u.log_id; ns.lsn = u.lsn;
      states[u.stream] = ns;
      changed[u.stream] = ns;
    }
    auto &ent = batch[fulls[i]];
    ent.first = u.size;
    for(auto &p : u.pieces) {
      if(p.offset % gxdPageSize != 0) { err = "piece not page aligned"; return -1; }
      size_t n = p.data.size();
      for(size_t k = 0; k < n; k += gxdPageSize) {
	unsigned long long pg = (p.offset + k) / gxdPageSize;
	if(n - k >= gxdPageSize) ent.second[pg] = p.data.data() + k;
	else {
	  padded.push_back(std::vector<unsigned char>(gxdPageSize, 0));
	  memcpy(padded.back().data(), p.data.data() + k, n - k);
	  ent.second[pg] = padded.back().data();
	}
      }
    }
    results[i] = 0;
  }
  // 11/10/2026: seals. A file of a commit_block unit that gets pieces not in
  // step with the copy's stream (a first fill, a new log, a resync; in step,
  // records bring every file to the same position) makes the unit "copying"
  // in this transaction; its
  // commit block, sent from the log after the folder's files, seals it "log G
  // L". A commit block carried by records seals a unit only if no file of it
  // is being filled (not "copying", no pieces here).
  std::deque<std::string> seals;
  if(!s->sc.commit_block.empty()) {
    std::set<std::string> filling, cb_units;
    for(size_t i = 0; i < v.size(); i++) {
      if(results[i] != 0) continue;
      if(base_of(v[i].path) == s->sc.commit_block) { if(!v[i].stream.empty()) cb_units.insert(dir_of(v[i].path)); }
      else if(!v[i].pieces.empty() && (v[i].stream.empty() || not_in_step.count(dir_of(v[i].path))))
	filling.insert(dir_of(v[i].path));
    }
    if(!filling.empty() && mkdirs(s->root + "/.gxrepl/units") != 0) { err = "cannot make the seals folder"; return -1; }
    for(auto &u : filling) put_seal(batch, s->root, u, "copying\n", seals);
    // a commit block sent from the log, after the folder's files, is the
    // unit's own: it seals it whatever records came before it in this batch
    for(auto &u : cb_units) unit_cb.erase(u);
    for(auto &u : filling) unit_cb.erase(u);
    bool first = true;
    for(auto &c : changed) {
      std::map<std::string, bool> mine;      // the copying-log marks do not depend on the stream: once
      if(first) mine = unit_cb;
      first = false;
      if(add_unit_seals(s->sc, s->root, batch, c.second.lsn, c.first, mine, seals, err, [&](const std::string &u) {
	    if(filling.count(u)) return false;
	    if(cb_units.count(u)) return true;
	    return !fill_copying(read_seal(s->root, u));
	  }) != 0) return -1;
    }
  }
  if(apply_batch(s, batch, err) != 0) return -1;
  for(size_t i = 0; i < v.size(); i++) {
    if(results[i] != 0 || !v[i].last) continue;
    chmod(fulls[i].c_str(), (mode_t)(v[i].mode & 07777));
    struct timespec ts[2];
    ts[0].tv_sec = time(0); ts[0].tv_nsec = 0;
    ts[1].tv_sec = (time_t)(v[i].mtime_ns / 1000000000LL);
    ts[1].tv_nsec = (long)(v[i].mtime_ns % 1000000000LL);
    utimensat(AT_FDCWD, fulls[i].c_str(), ts, 0);
  }
  for(auto &e : changed) {
    if(write_state(s, e.second, err) != 0) return -1;
  }
  return 0;
}

int gxReplLocal::Delete(const std::string &set, const std::vector<std::string> &paths,
			std::string &err)
{
  SetState_ *s = set_state(set, err);
  if(!s) return -1;
  std::lock_guard<std::mutex> g(s->m);
  WriterHold wh;                             // 11/10/2026: held until this change is done
  if(hold_writer_locks(s->sc, wh, err)) return -1;
  // 11/09/2026: Sync the folders after the removes, so a power loss cannot
  // bring a deleted file back (the next pass would delete it again, but a
  // copy should not go back in time)
  std::set<std::string> dirs;
  for(auto &p : paths) {
    std::string full;
    if(SafePath(s->sc, p, full, err) != 0) return -1;
    if(unlink(full.c_str()) != 0 && errno != ENOENT) { err = "cannot remove " + p; return -1; }
    std::string dir = full.substr(0, full.find_last_of('/'));
    while(dir.size() > s->root.size() && rmdir(dir.c_str()) == 0)
      dir = dir.substr(0, dir.find_last_of('/'));
    dirs.insert(dir.size() >= s->root.size() ? dir : s->root);
  }
  for(auto &d : dirs) sync_dir(d);
  return 0;
}

int gxReplLocal::WriteAckFloor(const std::string &set, const std::string &stream,
			       const std::string &copy, unsigned long long lsn, std::string &err)
{
  SetState_ *s = set_state(set, err);
  if(!s) return -1;
  if(stream.empty() || stream.find('/') != std::string::npos || stream[0] == '.') { err = "bad stream"; return -1; }
  std::string name;
  for(char c : copy) name += (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.') ? c : '_';
  if(name.empty() || name[0] == '.') name = "copy";
  std::string dir = s->root + "/.gxrepl/log/" + stream + "/acked";
  if(mkdirs(dir) != 0) { err = "cannot make " + dir; return -1; }
  if(write_atomic(dir + "/" + name, std::to_string(lsn) + "\n") != 0) { err = "cannot write " + dir; return -1; }
  return 0;
}

int gxReplLocal::RecoverAll(std::string &err)
{
  for(auto &sc : cfg.sets) {
    std::string j = sc.root + "/.gxrepl/apply.jnl";
    if(access(j.c_str(), F_OK) != 0) continue;
    unsigned long n = 0;
    if(gxdJournal::Recover(j.c_str(), &n) != gxDBASE_NO_ERROR) {
      err = "cannot recover " + j; return -1;
    }
    if(n) glog("set %s: finished %lu unfinished apply batch(es)", sc.name.c_str(), n);
  }
  return 0;
}

// ---------------- Remote ----------------

gxReplRemote::gxReplRemote(const gxReplConfig &c) : cfg(c) { }

gxReplRemote::~gxReplRemote()
{
  Close();
  stop_tunnel();
}

bool gxReplRemote::IsOpen() const
{
  if(mux) return mux->Alive();      // 11/08/2026: a multiplex channel
  return stream && stream->Connection().IsOpen();
}

void gxReplRemote::Close()
{
  if(own_mux) {                     // 11/08/2026: the multiplex owns the I/O now
    own_mux->Shutdown("closed");
    own_mux.reset();
    mux = 0;
    if(stream) { stream->Close(); stream.reset(); }
    return;
  }
  mux = 0;
  if(stream) {
    if(stream->Connection().IsOpen() && stream->Connection().IsSecure()) 
      stream->CloseConnection(); // Tell the server this client is done
    stream->Close();
    stream.reset();
  }
}

static int port_open(int port)
{
  gxSocket s;
  return s.Connect("127.0.0.1", port, 2) == 0;
}

int gxReplRemote::start_tunnel(std::string &err)
{
  if(tunnel_pid > 0) {
    int status;
    if(waitpid(tunnel_pid, &status, WNOHANG) == 0 && port_open(cfg.ssh_local_port))
      return 0;
    stop_tunnel();
  }
  std::vector<std::string> args;
  args.push_back(cfg.ssh_command);
  args.push_back("-N");
  args.push_back("-o"); args.push_back("ExitOnForwardFailure=yes");
  args.push_back("-o"); args.push_back("ServerAliveInterval=15");
  args.push_back("-o"); args.push_back("ServerAliveCountMax=3");
  args.push_back("-o"); args.push_back("BatchMode=yes");
  for(auto &o : cfg.ssh_options) { args.push_back("-o"); args.push_back(o); }
  args.push_back("-L");
  args.push_back("127.0.0.1:" + std::to_string(cfg.ssh_local_port) + ":" + cfg.ssh_remote);
  args.push_back(cfg.ssh_tunnel);
  pid_t pid = fork();
  if(pid < 0) { err = "fork failed"; return -1; }
  if(pid == 0) {
    std::vector<char *> av;
    for(auto &a : args) av.push_back(const_cast<char *>(a.c_str()));
    av.push_back(0);
    int nul = open("/dev/null", O_RDONLY);
    if(nul >= 0) { dup2(nul, 0); close(nul); }
    close_inherited_fds();
    execvp(av[0], av.data());
    _exit(127);
  }
  tunnel_pid = pid;
  for(int i = 0; i < 80; i++) {
    int status;
    if(waitpid(pid, &status, WNOHANG) == pid) {
      tunnel_pid = -1;
      err = "the ssh tunnel exited (" + std::to_string(WIFEXITED(status) ? WEXITSTATUS(status) : -1) + ")";
      return -1;
    }
    if(port_open(cfg.ssh_local_port)) {
      glog("ssh tunnel up: 127.0.0.1:%d -> %s via %s", cfg.ssh_local_port,
	   cfg.ssh_remote.c_str(), cfg.ssh_tunnel.c_str());
      return 0;
    }
    usleep(250000);
  }
  stop_tunnel();
  err = "the ssh tunnel did not open 127.0.0.1:" + std::to_string(cfg.ssh_local_port);
  return -1;
}

void gxReplRemote::stop_tunnel()
{
  if(tunnel_pid > 0) {
    kill(tunnel_pid, SIGTERM);
    for(int i = 0; i < 20; i++) {
      int status;
      if(waitpid(tunnel_pid, &status, WNOHANG) == tunnel_pid) { tunnel_pid = -1; return; }
      usleep(100000);
    }
    kill(tunnel_pid, SIGKILL);
    int status;
    waitpid(tunnel_pid, &status, 0);
    tunnel_pid = -1;
  }
}

int gxReplRemote::Connect(std::string &err)
// Protocol 2: TLS, then the gxrepl HELLO on the plain session
{
  if(connect_tls(err) != 0) return -1;
  return hello(err);
}

// 11/08/2026: Protocol 3: TLS, the multiplex, and the gxrepl HELLO on the
// control channel (1). Each set then opens its own channel.
int gxReplRemote::ConnectMux(std::string &err)
{
  if(connect_tls(err) != 0) return -1;
  gxdMuxOptions o;
  o.max_block = cfg.max_message * 4 + 1024 * 1024;
  own_mux.reset(new gxdMuxSession(gxdMuxStreamTransport(*stream), o));
  own_mux->Start();
  if(own_mux->Hello(err, cfg.timeout * 1000) != 0) { Close(); return -1; }
  if(own_mux->Open(1, "repl", "", "control", 0, err, cfg.timeout * 1000) != 0) { Close(); return -1; }
  mux = own_mux.get();
  mux_ch = 1;
  return hello(err);
}

int gxReplRemote::OpenSetChannel(uint32_t ch, const std::string &set, const std::string &mode,
				 std::string &err)
{
  if(!mux) { err = "not a multiplex connection"; return -1; }
  return mux->Open(ch, "repl", set, mode, 0, err, cfg.timeout * 1000);
}

void gxReplRemote::UseChannel(gxReplRemote &c, uint32_t ch)
{
  peer_node = c.peer_node;
  peer_sets = c.peer_sets;
  peer_zlib = c.peer_zlib;
  mux = c.mux;
  mux_ch = ch;
}

int gxReplRemote::connect_tls(std::string &err)
{
  Close();
  std::string host = cfg.connect_host;
  int port = cfg.connect_port;
  if(!cfg.ssh_tunnel.empty()) {
    if(start_tunnel(err) != 0) return -1;
    host = "127.0.0.1";
    port = cfg.ssh_local_port;
  }
  std::string name = cfg.server_name.empty() ? cfg.connect_host : cfg.server_name;
  if(name.empty()) { err = "server_name is not set"; return -1; }
  stream.reset(new gxStream);
  stream->SetTimeOut(cfg.timeout);
  stream->SetMaxBlockSize((gxUINT32)(cfg.max_message * 2 + 1024 * 1024));
  gxsTLSConnection &c = stream->Connection();
  c.SetCAFile(cfg.ca);
  c.SetClientCert(cfg.cert, cfg.key, cfg.key_passphrase);
  c.SetVerifyPeer(true);
  gxSocket sock;
  if(sock.Connect(host, port, cfg.timeout) != 0) {
    err = std::string(sock.SocketExceptionMessage()) + ": " + host + " port " + std::to_string(port);
    stream.reset();
    return -1;
  }
  sock.SetIOTimeout(cfg.timeout);
  sock.SetKeepAlive(true);
  sock.SetNoDelay(true);
  if(c.AttachClient(std::move(sock), name) != 0) {
    err = "TLS: " + c.Error();
    stream.reset();
    return -1;
  }
  return 0;
}

int gxReplRemote::hello(std::string &err)
{
  WBuf w;
  w.str("GXREPL"); w.u32(gxREPL_PROTOCOL); w.str(cfg.node);
  w.u32((uint32_t)sizeof(FAU)); w.u32((uint32_t)gxdPageSize); w.u32(0x01020304);
  w.u32(have_zlib);
  std::vector<unsigned char> reply;
  std::vector<unsigned char> req;
  req.push_back(OP_HELLO);
  req.insert(req.end(), w.b.begin(), w.b.end());
  if(call(req, reply, err) != 0) { Close(); return -1; }
  RBuf r(reply.data(), reply.size());
  uint32_t proto = r.u32();
  peer_node = r.str();
  uint32_t n = r.u32();
  peer_sets.clear();
  for(uint32_t i = 0; i < n && r.ok; i++) {
    std::string a = r.str(), b = r.str();
    peer_sets[a] = b;
  }
  peer_zlib = (int)r.u32();
  if(!r.ok || proto != (uint32_t)gxREPL_PROTOCOL) {
    err = "bad hello from the server"; Close(); return -1;
  }
  return 0;
}

int gxReplRemote::call(const std::vector<unsigned char> &req,
		       std::vector<unsigned char> &reply, std::string &err)
{
  reply.clear();
  if(req.empty()) return 0;
  if(mux) {
    // 11/08/2026: Protocol 3: the request and its reply on this object's channel
    std::vector<unsigned char> body(req.begin() + 1, req.end()), m, back;
    if(chan_broken) { err = "the channel was reset after a request had no reply"; return -1; }
    pack_msg(req[0], body, peer_zlib ? cfg.compress : 0, m);
    if(mux->Send(mux_ch, m, gxdMUX_LAST, err) != 0) return -1;
    bytes_out += m.size();
    uint64_t seq = 0;
    int rc = mux->ReceiveMessage(mux_ch, back, seq, cfg.timeout * 1000, err);
    if(rc == 1) {
      err = "no reply in " + std::to_string(cfg.timeout) + " s";
      chan_broken = true;                 // its reply may still come: never read it as the next request's
      mux->CloseChannel(mux_ch, err);
      return -1;
    }
    if(rc != 0) return -1;
    bytes_in += back.size();
    uint8_t op = 0;
    if(unpack_msg(back, op, reply, cfg.max_message * 4 + 1024 * 1024, err) != 0) return -1;
    messages++;
    if(op == OP_ERR) {
      RBuf r(reply.data(), reply.size());
      err = r.str();
      if(err.empty()) err = "error from the server";
      return 1;
    }
    if(op != OP_OK) { err = "bad reply"; chan_broken = true; mux->CloseChannel(mux_ch, err); return -1; }
    return 0;
  }
  if(!stream) { err = "not connected"; return -1; }
  std::vector<unsigned char> body(req.begin() + 1, req.end());
  if(send_msg(*stream, req[0], body, peer_zlib ? cfg.compress : 0, &bytes_out, err) != 0) {
    Close(); return -1;
  }
  uint8_t op = 0;
  if(recv_msg(*stream, op, reply, cfg.max_message * 4 + 1024 * 1024, &bytes_in, err) != 0) {
    if(err.empty()) err = "the server closed the connection";
    Close(); return -1;
  }
  messages++;
  if(op == OP_ERR) {
    RBuf r(reply.data(), reply.size());
    err = r.str();
    if(err.empty()) err = "error from the server";
    return 1; // The operation failed; the connection is fine
  }
  if(op != OP_OK) { err = "bad reply"; Close(); return -1; }
  return 0;
}

static std::vector<unsigned char> mkreq(uint8_t op, const WBuf &w)
{
  std::vector<unsigned char> v;
  v.push_back(op);
  v.insert(v.end(), w.b.begin(), w.b.end());
  return v;
}

int gxReplRemote::Streams(const std::string &set, std::vector<gxReplStream> &v,
			  std::string &err)
{
  WBuf w; w.str(set);
  std::vector<unsigned char> rep;
  int rc = call(mkreq(OP_STREAMS, w), rep, err);
  if(rc) return -1;
  RBuf r(rep.data(), rep.size());
  uint32_t n = r.u32(); v.clear();
  for(uint32_t i = 0; i < n && r.ok; i++) { gxReplStream s; dec(r, s); v.push_back(s); }
  if(!r.ok) { err = "bad reply"; if(mux) { chan_broken = true; mux->CloseChannel(mux_ch, err); } return -1; }
  return 0;
}

int gxReplRemote::ReadLog(const std::string &set, const std::string &stream_name,
			  unsigned long long from, size_t max_bytes,
			  std::vector<unsigned char> &out, unsigned long long &next,
			  int &status, std::string &err)
{
  WBuf w; w.str(set); w.str(stream_name); w.u64(from); w.u64(max_bytes);
  std::vector<unsigned char> rep;
  if(call(mkreq(OP_READLOG, w), rep, err)) return -1;
  RBuf r(rep.data(), rep.size());
  status = (int)r.u32(); next = r.u64(); r.bytes(out);
  if(!r.ok) { err = "bad reply"; if(mux) { chan_broken = true; mux->CloseChannel(mux_ch, err); } return -1; }
  return 0;
}

static int dec_files(const std::vector<unsigned char> &rep, std::vector<gxReplFile> &v,
		     std::string &err)
{
  RBuf r(rep.data(), rep.size());
  uint32_t n = r.u32(); v.clear();
  v.reserve(n < 1000000 ? n : 0);
  for(uint32_t i = 0; i < n && r.ok; i++) { gxReplFile f; dec(r, f); v.push_back(f); }
  if(!r.ok) { err = "bad reply"; return -1; }
  return 0;
}

int gxReplRemote::Manifest(const std::string &set, std::vector<gxReplFile> &v,
			   std::string &err)
{
  WBuf w; w.str(set);
  std::vector<unsigned char> rep;
  if(call(mkreq(OP_SMANIFEST, w), rep, err)) return -1;
  if(dec_files(rep, v, err) != 0) { if(mux) { chan_broken = true; mux->CloseChannel(mux_ch, err); } return -1; }
  return 0;
}

int gxReplRemote::TargetManifest(const std::string &set, std::vector<gxReplFile> &v,
				 std::string &err)
{
  WBuf w; w.str(set);
  std::vector<unsigned char> rep;
  if(call(mkreq(OP_TMANIFEST, w), rep, err)) return -1;
  if(dec_files(rep, v, err) != 0) { if(mux) { chan_broken = true; mux->CloseChannel(mux_ch, err); } return -1; }
  return 0;
}

static void enc_diffreq(WBuf &w, const gxReplDiffRequest &rq)
{
  w.str(rq.path); w.u32(rq.piece); w.u64(rq.start); w.u64(rq.max_bytes);
  enc(w, rq.copy_state);
  w.u32((uint32_t)rq.hashes.size());
  for(auto h : rq.hashes) w.u64(h);
}

static void dec_diffreq(RBuf &r, gxReplDiffRequest &rq)
{
  rq.path = r.str(); rq.piece = r.u32(); rq.start = r.u64(); rq.max_bytes = (size_t)r.u64();
  dec(r, rq.copy_state);
  uint32_t n = r.u32();
  rq.hashes.clear();
  for(uint32_t i = 0; i < n && r.ok; i++) rq.hashes.push_back(r.u64());
}

int gxReplRemote::Diff(const std::string &set, const gxReplDiffRequest &rq,
		       gxReplDiffResult &res, std::string &err)
{
  WBuf w; w.str(set); enc_diffreq(w, rq);
  std::vector<unsigned char> rep;
  if(call(mkreq(OP_DIFF, w), rep, err)) return -1;
  RBuf r(rep.data(), rep.size());
  res = gxReplDiffResult();
  res.status = (int)r.u32(); res.next = r.u64(); res.more = (int)r.u32();
  res.pieces = r.u64(); res.sent = r.u64();
  dec(r, res.update);
  if(!r.ok) { err = "bad reply"; if(mux) { chan_broken = true; mux->CloseChannel(mux_ch, err); } return -1; }
  return 0;
}

int gxReplRemote::State(const std::string &set, std::vector<gxReplStreamState> &v,
			std::string &err)
{
  WBuf w; w.str(set);
  std::vector<unsigned char> rep;
  if(call(mkreq(OP_STATE, w), rep, err)) return -1;
  RBuf r(rep.data(), rep.size());
  uint32_t n = r.u32(); v.clear();
  for(uint32_t i = 0; i < n && r.ok; i++) { gxReplStreamState s; dec(r, s); v.push_back(s); }
  if(!r.ok) { err = "bad reply"; if(mux) { chan_broken = true; mux->CloseChannel(mux_ch, err); } return -1; }
  return 0;
}

int gxReplRemote::Hashes(const std::string &set, const std::string &path,
			 uint32_t piece, std::vector<uint64_t> &h, int &exists,
			 std::string &err)
{
  WBuf w; w.str(set); w.str(path); w.u32(piece);
  std::vector<unsigned char> rep;
  if(call(mkreq(OP_HASHES, w), rep, err)) return -1;
  RBuf r(rep.data(), rep.size());
  exists = (int)r.u32();
  uint32_t n = r.u32(); h.clear();
  for(uint32_t i = 0; i < n && r.ok; i++) h.push_back(r.u64());
  if(!r.ok) { err = "bad reply"; if(mux) { chan_broken = true; mux->CloseChannel(mux_ch, err); } return -1; }
  return 0;
}

int gxReplRemote::CommitGen(const std::string &set, const std::string &path, uint32_t offset,
			    uint64_t &gen, int &exists, std::string &err)
{
  WBuf w; w.str(set); w.str(path); w.u32(offset);
  std::vector<unsigned char> rep;
  if(call(mkreq(OP_COMMITGEN, w), rep, err)) return -1;
  RBuf r(rep.data(), rep.size());
  exists = (int)r.u32();
  gen = r.u64();
  if(!r.ok) { err = "bad reply"; if(mux) { chan_broken = true; mux->CloseChannel(mux_ch, err); } return -1; }
  return 0;
}

int gxReplRemote::MarkGen(const std::string &set, const std::string &path, uint32_t offset,
			  uint64_t gen, std::string &err)
{
  WBuf w; w.str(set); w.str(path); w.u32(offset); w.u64(gen);
  std::vector<unsigned char> rep;
  if(call(mkreq(OP_MARKGEN, w), rep, err)) return -1;
  return 0;
}

int gxReplRemote::SealUnit(const std::string &set, const std::string &folder, uint64_t gen, std::string &err)
{
  WBuf w; w.str(set); w.str(folder); w.u64(gen);
  std::vector<unsigned char> rep;
  return call(mkreq(OP_SEALUNIT, w), rep, err) ? -1 : 0;
}

int gxReplRemote::SealGen(const std::string &set, const std::string &folder, uint64_t &gen, std::string &err)
{
  WBuf w; w.str(set); w.str(folder);
  std::vector<unsigned char> rep;
  if(call(mkreq(OP_SEALGEN, w), rep, err)) return -1;
  RBuf r(rep.data(), rep.size());
  gen = r.u64();
  if(!r.ok) { err = "bad reply"; if(mux) { chan_broken = true; mux->CloseChannel(mux_ch, err); } return -1; }
  return 0;
}

int gxReplRemote::ApplyLog(const std::string &set, const std::string &stream_name,
			   const std::string &log_id,
			   const std::vector<unsigned char> &records,
			   unsigned long long &lsn, std::string &err)
{
  WBuf w; w.str(set); w.str(stream_name); w.str(log_id); w.bytes(records);
  std::vector<unsigned char> rep;
  if(call(mkreq(OP_APPLYLOG, w), rep, err)) return -1;
  RBuf r(rep.data(), rep.size());
  lsn = r.u64();
  if(!r.ok) { err = "bad reply"; if(mux) { chan_broken = true; mux->CloseChannel(mux_ch, err); } return -1; }
  return 0;
}

int gxReplRemote::ApplyFiles(const std::string &set,
			     const std::vector<gxReplFileUpdate> &v,
			     std::vector<int> &results, std::string &err)
{
  WBuf w; w.str(set); w.u32((uint32_t)v.size());
  for(auto &u : v) enc(w, u);
  std::vector<unsigned char> rep;
  if(call(mkreq(OP_APPLYFILES, w), rep, err)) return -1;
  RBuf r(rep.data(), rep.size());
  uint32_t n = r.u32(); results.clear();
  for(uint32_t i = 0; i < n && r.ok; i++) results.push_back((int)r.u32());
  if(!r.ok || results.size() != v.size()) { err = "bad reply"; return -1; }
  return 0;
}

int gxReplRemote::Delete(const std::string &set, const std::vector<std::string> &paths,
			 std::string &err)
{
  WBuf w; w.str(set); w.u32((uint32_t)paths.size());
  for(auto &p : paths) w.str(p);
  std::vector<unsigned char> rep;
  if(call(mkreq(OP_DELETE, w), rep, err)) return -1;
  return 0;
}

int gxReplRemote::SetState(const std::string &set, const gxReplStreamState &s,
			   std::string &err)
{
  WBuf w; w.str(set); enc(w, s);
  std::vector<unsigned char> rep;
  if(call(mkreq(OP_SETSTATE, w), rep, err)) return -1;
  return 0;
}

// ---------------- Server ----------------

gxReplServer::gxReplServer(const gxReplConfig &c) : cfg(c), local(c)
{
  // 11/08/2026: The object service, when the config names databases. A
  // database inside a set this server receives is a copy: read-only, read
  // under the set's apply journal lock.
  if(!cfg.object_dbs.empty()) {
    objsvc.reset(new gxObjectService(cfg.object_dbs, cfg.legacy_db));
    for(auto &sc : cfg.sets)
      if(sc.accept == "target" || sc.accept == "both")
	objsvc->ProtectRoot(real_path(sc.root).empty() ? sc.root : real_path(sc.root), sc.root + "/.gxrepl/apply.jnl");
  }
}

gxReplServer::~gxReplServer() { }

void gxReplServer::Stop()
{
  stop = true;
  gxDatabaseServer *d = dbs;
  if(d) d->Stop();
}

bool gxReplServer::Claims(const gxBlockHeader &gx, const std::vector<unsigned char> &body)
{
  // A gxrepl HELLO (protocol 2), or a multiplex HELLO (protocol 3)
  if(gxStream::BlockStatus(gx) != gxSendBlock || body.size() < 6) return false;
  if(body[0] == OP_HELLO) return true;
  gxdMuxFrame f;
  return body[0] == gxdMUX_MARK && gxdMuxDecodeBody(body.data(), body.size(), f) == 0 &&
    f.kind == gxdMUX_HELLO;
}

void gxReplServer::write_status()
{
  if(cfg.status_file.empty()) return;
  std::string j = "{\"node\": " + json_str(cfg.node) + ", \"role\": \"server\", \"time\": " +
    std::to_string(now_s()) + ", \"port\": " + std::to_string(cfg.listen_port) +
    ", \"applied_records\": " + std::to_string(local.applied_records) +
    ", \"applied_pages\": " + std::to_string(local.applied_pages) + ", \"clients\": [";
  std::vector<std::string> conns;
  gxDatabaseServer *d = dbs;
  if(d) conns = d->Connections();
  std::lock_guard<std::mutex> g(status_lock);
  bool first = true;
  for(auto &e : conns) { j += (first ? "" : ", ") + json_str(e); first = false; }
  j += "], \"sets\": {";
  first = true;
  for(auto &sc : cfg.sets) {
    j += (first ? "" : ", ") + json_str(sc.name) + ": {\"accept\": " + json_str(sc.accept) +
      ", \"root\": " + json_str(sc.root) + ", \"last\": " + json_str(set_last[sc.name]) + "}";
    first = false;
  }
  // 11/08/2026: Each multiplex channel: its set and its queue (frames and
  // bytes waiting to be applied)
  j += "}, \"channels\": [";
  first = true;
  for(auto *mx : muxes) {
    for(auto &c : mx->Status()) {
      if(c.mode == "control") continue;
      j += std::string(first ? "" : ", ") + "{\"set\": " + json_str(c.name) + ", \"mode\": " + json_str(c.mode) +
	", \"state\": " + json_str(c.state) + ", \"queue_frames\": " + std::to_string(c.in_frames) +
	", \"queue_bytes\": " + std::to_string(c.in_bytes) + ", \"frames\": " + std::to_string(c.received_frames) +
	", \"error\": " + json_str(c.error) + "}";
      first = false;
    }
  }
  j += "]}\n";
  write_atomic(cfg.status_file, j);
}

int gxReplServer::Run(std::string &err)
// 11/08/2026: The listener, handshakes and limits are the database object
// server's (gxdbserver.h); this server is its replication service.
{
  if(local.RecoverAll(err) != 0) return -1;
  gxDatabaseServerConfig dc;
  dc.listen_address = cfg.listen_address;
  dc.listen_port = cfg.listen_port;
  dc.cert = cfg.cert; dc.key = cfg.key; dc.key_passphrase = cfg.key_passphrase;
  dc.ca = cfg.ca;
  dc.allow_cn = cfg.allow_cn;
  dc.allow_fingerprint = cfg.allow_fingerprint;
  dc.max_clients = cfg.max_clients;
  dc.max_handshakes = cfg.max_handshakes;
  dc.handshake_timeout = cfg.handshake_timeout;
  dc.timeout = cfg.timeout;
  dc.max_block = (unsigned long)(cfg.max_message * 4 + 1024 * 1024);
  gxDatabaseServer server(dc);
  server.AddService(this);
  if(objsvc) server.AddService(objsvc.get());   // 11/08/2026: 6.x style object sessions
  bool announced = false;
  server.SetTick([&]() {
    if(!announced) {
      glog("serving %zu set(s) on %s:%d", cfg.sets.size(),
	   cfg.listen_address.empty() ? "*" : cfg.listen_address.c_str(), server.Port());
      announced = true;
    }
    write_status();
  }, 5);
  dbs = &server;
  if(stop) server.Stop();
  // 11/10/2026: the derived index rebuilds of the sets this server copies (their own thread: a rebuild can take long)
  std::thread derived_worker([&]() {
    std::map<std::string, std::map<std::string, std::pair<uint64_t, long long> > > seen;
    while(!stop) {
      for(auto &sc : cfg.sets) {
	if(stop || sc.accept == "source" || sc.derived.empty()) continue;
	std::vector<std::string> lines;
	gxReplDerivedRefresh(sc, seen[sc.name], lines);
	for(auto &l : lines) glog("%s", l.c_str());
      }
      for(int i = 0; i < 10 && !stop; i++) usleep(200000);
    }
  });
  int rc = server.Run(err);
  stop = true;
  derived_worker.join();
  dbs = 0;
  write_status();
  return rc;
}

// 11/08/2026: One request (op and body), its reply. Returns 0 (reply =
// the OK body) or -1 (e = the error). Shared by the plain session and
// every multiplex channel.
int gxReplServer::dispatch(uint8_t op, const std::vector<unsigned char> &body,
			   std::vector<unsigned char> &reply, std::string &e, ReqState &st)
{
  RBuf r(body.data(), body.size());
  WBuf w;
  int rc = 0;
  auto role_ok = [&](const std::string &set, bool as_target) -> bool {
      // 11/08/2026: A multiplex channel serves only its own set
      if(!st.only_set.empty() && set != st.only_set) { e = "set " + set + " is not this channel's"; return false; }
      const gxReplSetConfig *sc = cfg.Set(set);
      if(!sc) { e = "no set " + set + " on " + cfg.node; return false; }
      if(as_target && sc->accept == "source") { e = "set " + set + " is a source on " + cfg.node; return false; }
      if(!as_target && sc->accept == "target") { e = "set " + set + " is a copy on " + cfg.node; return false; }
      return true;
    };

    switch(op) {
    case OP_HELLO: {
      std::string magic = r.str(); uint32_t proto = r.u32(); std::string node = r.str();
      uint32_t fau = r.u32(), page = r.u32(), endian = r.u32();
      st.peer_zlib = r.u32() != 0 && have_zlib;
      if(!r.ok || magic != "GXREPL" || proto != (uint32_t)gxREPL_PROTOCOL ||
	 fau != sizeof(FAU) || page != gxdPageSize || endian != 0x01020304) {
	e = "incompatible client (protocol, build or platform)"; rc = -1; break;
      }
      st.hello = true;
      st.who = st.peer_cn + " (" + node + ")@" + st.peer_host;
      if(st.ses) st.ses->SetDescription(st.who);
      w.u32(gxREPL_PROTOCOL); w.str(cfg.node); w.u32((uint32_t)cfg.sets.size());
      for(auto &sc : cfg.sets) { w.str(sc.name); w.str(sc.accept); }
      w.u32(have_zlib);
      break;
    }
    case OP_STREAMS: {
      std::string set = r.str();
      std::vector<gxReplStream> v;
      if(!role_ok(set, false) || local.Streams(set, v, e) != 0) { rc = -1; break; }
      w.u32((uint32_t)v.size()); for(auto &x : v) enc(w, x);
      break;
    }
    case OP_READLOG: {
      std::string set = r.str(), st = r.str(); uint64_t from = r.u64(); uint64_t mx = r.u64();
      if(mx > cfg.max_message) mx = cfg.max_message;
      std::vector<unsigned char> out; unsigned long long next = 0; int status = 0;
      if(!role_ok(set, false) || local.ReadLog(set, st, from, (size_t)mx, out, next, status, e) != 0) { rc = -1; break; }
      w.u32((uint32_t)status); w.u64(next); w.bytes(out);
      break;
    }
    case OP_SMANIFEST: case OP_TMANIFEST: {
      std::string set = r.str();
      std::vector<gxReplFile> v;
      if(!role_ok(set, op == OP_TMANIFEST) || local.Manifest(set, v, e) != 0) { rc = -1; break; }
      w.u32((uint32_t)v.size()); for(auto &x : v) enc(w, x);
      break;
    }
    case OP_DIFF: {
      std::string set = r.str();
      gxReplDiffRequest rq; dec_diffreq(r, rq);
      gxReplDiffResult res;
      if(!r.ok) { e = "bad request"; rc = -1; break; }
      if(!role_ok(set, false) || local.Diff(set, rq, res, e) != 0) { rc = -1; break; }
      w.u32((uint32_t)res.status); w.u64(res.next); w.u32((uint32_t)res.more);
      w.u64(res.pieces); w.u64(res.sent); enc(w, res.update);
      break;
    }
    case OP_STATE: {
      std::string set = r.str();
      std::vector<gxReplStreamState> v;
      if(!role_ok(set, true) || local.State(set, v, e) != 0) { rc = -1; break; }
      w.u32((uint32_t)v.size()); for(auto &x : v) enc(w, x);
      break;
    }
    case OP_SEALUNIT: {           // 11/10/2026: the copy's side only
      std::string set = r.str(), folder = r.str(); uint64_t g = r.u64();
      if(!r.ok) { e = "bad request"; rc = -1; break; }
      if(!role_ok(set, true) || local.SealUnit(set, folder, g, e) != 0) { rc = -1; break; }
      break;
    }
    case OP_SEALGEN: {
      std::string set = r.str(), folder = r.str();
      uint64_t g = 0;
      if(!r.ok) { e = "bad request"; rc = -1; break; }
      if(!role_ok(set, true) || local.SealGen(set, folder, g, e) != 0) { rc = -1; break; }
      w.u64(g);
      break;
    }
    case OP_MARKGEN: {            // 11/10/2026: the copy's side only
      std::string set = r.str(), path = r.str(); uint32_t off = r.u32(); uint64_t g = r.u64();
      if(!r.ok) { e = "bad request"; rc = -1; break; }
      if(!role_ok(set, true) || local.MarkGen(set, path, off, g, e) != 0) { rc = -1; break; }
      break;
    }
    case OP_COMMITGEN: {          // 11/10/2026: either side of a set reads it
      std::string set = r.str(), path = r.str(); uint32_t off = r.u32();
      uint64_t g = 0; int exists = 0;
      if(!(role_ok(set, true) || role_ok(set, false)) || local.CommitGen(set, path, off, g, exists, e) != 0) { rc = -1; break; }
      w.u32((uint32_t)exists); w.u64(g);
      break;
    }
    case OP_HASHES: {
      std::string set = r.str(), path = r.str(); uint32_t piece = r.u32();
      std::vector<uint64_t> h; int exists = 0;
      if(!role_ok(set, true) || local.Hashes(set, path, piece, h, exists, e) != 0) { rc = -1; break; }
      w.u32((uint32_t)exists); w.u32((uint32_t)h.size()); for(auto x : h) w.u64(x);
      break;
    }
    case OP_APPLYLOG: {
      std::string set = r.str(), st = r.str(), id = r.str();
      std::vector<unsigned char> recs; r.bytes(recs);
      unsigned long long lsn = 0;
      if(!r.ok) { e = "bad request"; rc = -1; break; }
      if(!role_ok(set, true) || local.ApplyLog(set, st, id, recs, lsn, e) != 0) { rc = -1; break; }
      w.u64(lsn);
      { std::lock_guard<std::mutex> g(status_lock);
	set_last[set] = "log " + st + " to " + std::to_string(lsn) + " at " + std::to_string(now_s()); }
      break;
    }
    case OP_APPLYFILES: {
      std::string set = r.str(); uint32_t n = r.u32();
      std::vector<gxReplFileUpdate> v;
      for(uint32_t i = 0; i < n && r.ok && i < 100000; i++) { gxReplFileUpdate u; dec(r, u); v.push_back(std::move(u)); }
      if(!r.ok) { e = "bad request"; rc = -1; break; }
      std::vector<int> res;
      if(!role_ok(set, true) || local.ApplyFiles(set, v, res, e) != 0) { rc = -1; break; }
      w.u32((uint32_t)res.size()); for(int x : res) w.u32((uint32_t)x);
      if(!v.empty()) { std::lock_guard<std::mutex> g(status_lock);
	set_last[set] = "file " + v.back().path + " at " + std::to_string(now_s()); }
      break;
    }
    case OP_DELETE: {
      std::string set = r.str(); uint32_t n = r.u32();
      std::vector<std::string> v;
      for(uint32_t i = 0; i < n && r.ok && i < 100000; i++) v.push_back(r.str());
      if(!r.ok) { e = "bad request"; rc = -1; break; }
      if(!role_ok(set, true) || local.Delete(set, v, e) != 0) { rc = -1; break; }
      break;
    }
    case OP_SETSTATE: {
      std::string set = r.str(); gxReplStreamState st; dec(r, st);
      if(!r.ok) { e = "bad request"; rc = -1; break; }
      if(!role_ok(set, true) || local.SetState(set, st, e) != 0) { rc = -1; break; }
      break;
    }
    default:
      e = "unknown operation"; rc = -1;
    }
  reply.swap(w.b);
  return rc;
}

// 11/08/2026: Protocol 3. One multiplex session, many sets at once: the
// control channel (1) carries the HELLO, then each set has its own
// channel, served by its own thread in order (the per-database queue and
// applier of the design). A channel only touches its own set; an error
// closes that channel only. After a durable log apply the channel gets an
// ACK with the applied lsn.
void gxReplServer::serve_mux(gxDatabaseSession &ses)
{
  gxdMuxFrame first;
  if(gxdMuxDecodeBody(ses.first_body.data(), ses.first_body.size(), first) != 0) return;
  gxdMuxOptions o;
  o.max_block = cfg.max_message * 4 + 1024 * 1024;
  gxdMuxSession mux(gxdMuxStreamTransport(ses.Stream()), o);
  std::mutex hm;
  ReqState base;
  base.peer_cn = ses.PeerCN(); base.peer_host = ses.PeerHost(); base.ses = &ses;
  base.who = base.peer_cn + "@" + base.peer_host;
  std::atomic<bool> hello_ok{false};
  mux.SetOpenHandler([&](uint32_t, const std::string &service, const std::string &name,
			 const std::string &mode, std::vector<unsigned char> &, std::string &why) -> bool {
    if(service == "object") {   // 11/08/2026: database objects
      if(!objsvc) { why = "no object service on " + cfg.node; return false; }
      return objsvc->OpenAllowed(name, mode, why);
    }
    if(service != "repl") { why = "no service " + service + " on " + cfg.node; return false; }
    if(mode == "control") return !hello_ok;
    if(!hello_ok) { why = "hello first"; return false; }
    const gxReplSetConfig *sc = cfg.Set(name);
    if(!sc) { why = "no set " + name + " on " + cfg.node; return false; }
    if(mode == "push" && sc->accept == "source") { why = "set " + name + " is a source on " + cfg.node; return false; }
    if(mode == "pull" && sc->accept == "target") { why = "set " + name + " is a copy on " + cfg.node; return false; }
    if(mode != "push" && mode != "pull") { why = "bad mode " + mode; return false; }
    return true;
  });
  mux.Start(&first);
  { std::lock_guard<std::mutex> g(status_lock); muxes.insert(&mux); }
  glog("client %s connected (multiplex)", base.who.c_str());
  std::vector<std::thread> workers;
  while(!stop && mux.Alive()) {
    uint32_t ch; std::string sv, nm, md;
    if(mux.AcceptChannel(ch, sv, nm, md, 500) != 0) continue;
    workers.emplace_back([&, ch, sv, nm, md]() {
      if(sv == "object") { objsvc->ServeChannel(mux, ch, nm, md, stop); return; }
      ReqState st;
      { std::lock_guard<std::mutex> g(hm); st = base; }
      // Test hook: GXREPL_TEST_SLOW_SET=<set>:<ms> slows that set's channel
      // (each request waits ms first), to show it does not hold up others
      int slow_ms = 0;
      const char *sl = getenv("GXREPL_TEST_SLOW_SET");
      if(sl) { std::string v = sl; size_t c = v.find(':');
	if(c != std::string::npos && v.substr(0, c) == nm) slow_ms = atoi(v.c_str() + c + 1); }
      st.only_set = md == "control" ? std::string("\x01") : nm;  // The control channel serves no set
      st.hello = md != "control";
      while(!stop) {
	std::vector<unsigned char> msg, body, reply;
	uint64_t seq = 0;
	std::string e;
	int rc = mux.ReceiveMessage(ch, msg, seq, 1000, e);
	if(rc == 1) continue;
	if(rc < 0) break;
	uint8_t op = 0;
	if(unpack_msg(msg, op, body, cfg.max_message * 4 + 1024 * 1024, e) != 0) { mux.CloseChannel(ch, e); break; }
	if(slow_ms > 0) usleep((useconds_t)slow_ms * 1000);
	int r;
	if((op == OP_HELLO) != (md == "control")) { e = "the control channel carries the hello, the set channels the rest"; r = -1; }
	else r = dispatch(op, body, reply, e, st);
	if(op == OP_HELLO && r == 0) {
	  std::lock_guard<std::mutex> g(hm);
	  base.peer_zlib = st.peer_zlib; base.who = st.who;
	  hello_ok = true;
	}
	std::vector<unsigned char> out;
	if(r == 0) pack_msg(OP_OK, reply, st.peer_zlib ? cfg.compress : 0, out);
	else { WBuf we; we.str(e); pack_msg(OP_ERR, we.b, 0, out); }
	if(mux.Send(ch, out, gxdMUX_LAST, e) != 0) break;
	if(op == OP_APPLYLOG && r == 0 && reply.size() >= 8) {
	  uint64_t lsn; memcpy(&lsn, reply.data(), 8);
	  mux.Ack(ch, seq, lsn);   // Durable: the pages and the applied lsn are synced
	}
      }
    });
  }
  mux.Shutdown(stop ? "server stopping" : "");
  for(auto &t : workers) t.join();
  { std::lock_guard<std::mutex> g(status_lock); muxes.erase(&mux); }
  std::string why = mux.Error();
  glog("client %s disconnected%s%s", base.who.c_str(), why.empty() ? "" : ": ", why.c_str());
}

void gxReplServer::Serve(gxDatabaseSession &ses)
{
  // 11/08/2026: A multiplex session (protocol 3: many sets at once)
  if(!ses.first_body.empty() && ses.first_body[0] == gxdMUX_MARK) { serve_mux(ses); return; }
  gxStream *s = &ses.Stream();
  ReqState st;
  st.peer_cn = ses.PeerCN(); st.peer_host = ses.PeerHost(); st.ses = &ses;
  st.who = st.peer_cn + "@" + st.peer_host;
  glog("client %s connected", st.who.c_str());
  bool first = true;
  std::string err;
  while(!stop) {
    uint8_t op = 0;
    std::vector<unsigned char> body;
    if(first) {
      first = false;
      if(decode_msg(ses.first_header, ses.first_body, op, body, cfg.max_message * 4 + 1024 * 1024, err) != 0) break;
    }
    else if(recv_msg(*s, op, body, cfg.max_message * 4 + 1024 * 1024, 0, err) != 0) break;
    if(!st.hello && op != OP_HELLO) { glog("client %s: no hello", st.who.c_str()); break; }
    std::vector<unsigned char> reply;
    std::string e;
    int rc = dispatch(op, body, reply, e, st);
    int sent;
    if(rc == 0) sent = send_msg(*s, OP_OK, reply, st.peer_zlib ? cfg.compress : 0, 0, err);
    else {
      WBuf we; we.str(e);
      sent = send_msg(*s, OP_ERR, we.b, 0, 0, err);
      if(op == OP_HELLO) { glog("client %s refused: %s", st.who.c_str(), e.c_str()); break; }
    }
    if(sent != 0) break;
  }
  s->Close();
  glog("client %s disconnected%s%s", st.who.c_str(), err.empty() ? "" : ": ", err.c_str());
}

// ---------------- Client ----------------

gxReplClient::gxReplClient(const gxReplConfig &c) : cfg(c), local(c) { }

gxReplClient::~gxReplClient() { }

void gxReplClient::write_status(bool connected, const std::string &note)
{
  if(cfg.status_file.empty()) return;
  std::lock_guard<std::mutex> wl(status_write_lock);   // 11/08/2026: set threads write it too
  last_status_write = now_s();
  std::string j = "{\"node\": " + json_str(cfg.node) + ", \"role\": \"client\", \"time\": " +
    std::to_string(now_s()) + ", \"connected\": " + (connected ? "true" : "false") +
    ", \"peer\": " + json_str(peer) + ", \"since\": " + std::to_string(connected_since) +
    ", \"via\": " + json_str(cfg.ssh_tunnel.empty() ? "direct" : "ssh " + cfg.ssh_tunnel) +
    ", \"note\": " + json_str(note) + ", \"sets\": {";
  bool first = true;
  for(auto &sc : cfg.sets) {
    auto &st = status[sc.name];
    std::lock_guard<std::mutex> lg(st.m);   // 11/08/2026: last_error and streams
    j += (first ? "" : ", ") + json_str(sc.name) + ": {\"direction\": " + json_str(sc.direction) +
      ", \"ok\": " + (st.ok ? "true" : "false") + ", \"last_ok\": " + std::to_string(st.last_ok) +
      ", \"last_scan\": " + std::to_string(st.last_scan) +
      ", \"last_verify\": " + std::to_string(st.last_verify) +
      ", \"last_full_verify\": " + std::to_string(st.last_full_verify) +
      ", \"records\": " + std::to_string(st.records) + ", \"files_sent\": " + std::to_string(st.files_sent) +
      ", \"pieces_sent\": " + std::to_string(st.pieces_sent) + ", \"bytes_sent\": " + std::to_string(st.bytes_sent) +
      ", \"deletes\": " + std::to_string(st.deletes) + ", \"last_error\": " + json_str(st.last_error) +
      ", \"gxrepl_bytes\": " + std::to_string(st.gxrepl_bytes) + ", \"ship_log_bytes\": " +
      std::to_string(st.ship_log_bytes) + ", \"warn\": " + json_str(st.warn) +
      ", \"streams\": {";
    bool f2 = true;
    for(auto &e : st.streams) {
      // 11/11/2026 (review): never negative (a copy ahead of a restarted source log, a stopped client's old entry)
      long long lag = e.second.first > e.second.second ? (long long)(e.second.first - e.second.second) : 0;
      j += (f2 ? "" : ", ") + json_str(e.first) + ": {\"source\": " + std::to_string(e.second.first) +
	", \"copy\": " + std::to_string(e.second.second) + ", \"lag\": " + std::to_string(lag) + "}";
      f2 = false;
    }
    j += "}}";
    first = false;
  }
  j += "}}\n";
  write_atomic(cfg.status_file, j);
}

int gxReplClient::drain(gxReplSource &src, gxReplTarget &tgt, const std::string &set,
			const gxReplStream &s, gxReplStreamState &ts,
			gxReplSetStatus &st, bool &verify, std::string &err)
{
  for(int rounds = 0; rounds < 100000 && !stop; rounds++) {
    std::vector<unsigned char> out;
    unsigned long long next = 0;
    int status = 0;
    if(src.ReadLog(set, s.name, ts.lsn + 1, cfg.max_message, out, next, status, err) != 0)
      return -1;
    if(status == 1) { verify = true; return 0; }
    if(status == 2) {
      // 11/10/2026: the next record is bigger than a message (an osa_ingest
      // rebuild writes one of 500 MB): never sent. The copy's position is
      // reset, so its files are resynced in pieces from a position after it
      // (the verify), and the records after that are drained as usual.
      glog("set %s: stream %s: record %llu is bigger than max_message (%zu): the copy resyncs its files",
	   set.c_str(), s.name.c_str(), ts.lsn + 1, cfg.max_message);
      gxReplStreamState rs;
      rs.name = s.name; rs.log_id = "resync"; rs.lsn = 0;
      if(tgt.SetState(set, rs, err) != 0) return -1;
      ts = rs;
      verify = true;
      // Every file of the stream is resynced (the stream verify), not only the
      // one a sync_file() had in hand: the first file's pieces move the copy's
      // position past the big record for all of them
      auto &vl = verify_streams[set];
      if(std::find(vl.begin(), vl.end(), s.name) == vl.end()) {
	glog("set %s: stream %s needs a verify (a record bigger than max_message)", set.c_str(), s.name.c_str());
	vl.push_back(s.name);
      }
      return 0;
    }
    if(out.empty()) return 0;
    std::vector<gxdShipRecordView> recs;
    if(gxdParseShipRecords(out.data(), out.size(), recs) != 0) { err = "damaged log records"; return -1; }
    for(auto &r : recs) {
      if(r.type == gxdShipOpen || r.type == gxdShipGap) verify = true;
    }
    unsigned long long lsn = 0;
    if(tgt.ApplyLog(set, s.name, s.log_id, out, lsn, err) != 0) return -1;
    ts.lsn = lsn;
    // 11/08/2026: The copy has it durably: the retention floor, when this
    // node is the source (never fatal)
    if(static_cast<gxReplSource *>(&local) == &src) {
      std::string fe;
      if(local.WriteAckFloor(set, s.name, peer.empty() ? "copy" : peer, lsn, fe) != 0)
	glog("set %s: %s", set.c_str(), fe.c_str());
    }
    st.records += recs.size();
    st.bytes_sent += out.size();
  }
  return 0;
}

int gxReplClient::sync_file(gxReplSource &src, gxReplTarget &tgt, const std::string &set,
			    const std::string &path, const gxReplStream *cover,
			    gxReplSetStatus &st, std::string &err)
{
  for(int attempt = 0; attempt < 6 && !stop; attempt++) {
    gxReplStreamState cs;
    if(cover) {
      std::vector<gxReplStreamState> v;
      if(tgt.State(set, v, err) != 0) return -1;
      for(auto &x : v) if(x.name == cover->name) cs = x;
    }
    std::vector<uint64_t> hashes;
    int exists = 0;
    if(tgt.Hashes(set, path, gxREPL_PIECE, hashes, exists, err) != 0) return -1;
    unsigned long long start = 0;
    bool retry = false;
    // 11/08/2026: A file read in several parts must come from one version.
    // If the writer changed it between parts, the copy gets mtime 0 (so a
    // later scan sends it again) and the file starts over.
    long long first_mt = -1;
    bool torn = false;
    while(!stop) {
      gxReplDiffRequest rq;
      rq.path = path;
      rq.hashes = hashes;
      rq.start = start;
      rq.max_bytes = cfg.max_message;
      rq.copy_state = cs;
      gxReplDiffResult res;
      if(src.Diff(set, rq, res, err) != 0) return -1;
      if(res.status == 1) return 0; // Gone from the source
      if(res.status == 3) { retry = true; usleep(200000); break; } // 11/08/2026: changed while read
      if(res.status == 2 || res.update.stream.empty() != (cover == 0)) {
	if(cover && cs.log_id == cover->log_id) {
	  bool v = false;
	  if(drain(src, tgt, set, *cover, cs, st, v, err) != 0) return -1;
	}
	retry = true;
	break;
      }
      if(!cover) {
	if(first_mt < 0) first_mt = res.update.mtime_ns;
	else if(res.update.mtime_ns != first_mt) torn = true;
	if(torn) res.update.mtime_ns = 0;
      }
      // 11/10/2026: a batch the source cut short (4 MB / 100 ms of reading, so the
      // writer can commit) with no piece that differs: nothing to apply, read on
      if(res.more && res.update.pieces.empty() && res.update.records.empty()) {
	st.pieces_sent += res.sent;
	start = res.next;
	continue;
      }
      std::vector<gxReplFileUpdate> ups(1, res.update);
      std::vector<int> results;
      if(tgt.ApplyFiles(set, ups, results, err) != 0) return -1;
      if(results.empty() || results[0] != 0) {
	// Stale: the copy is not at the stream position of the pieces
	if(cover && cs.log_id == cover->log_id) {
	  bool v = false;
	  if(drain(src, tgt, set, *cover, cs, st, v, err) != 0) return -1;
	}
	retry = true;
	break;
      }
      if(cover) { cs.name = res.update.stream; cs.log_id = res.update.log_id; cs.lsn = res.update.lsn; }
      st.pieces_sent += res.sent;
      for(auto &p : res.update.pieces) st.bytes_sent += p.data.size();
      st.bytes_sent += res.update.records.size();
      if(!res.more && torn) { retry = true; usleep(200000); break; }  // 11/08/2026: read it again whole
      if(!res.more) { st.files_sent++; return 0; }
      if(torn) { retry = true; usleep(200000); break; }
      start = res.next;
    }
    if(!retry) return 0;
  }
  if(stop) return 0;
  err = "gave up on " + path + " after 6 tries (it keeps changing)";
  return 1;
}

// 11/10/2026: One database folder with a seqlock commit block, copied so the
// copy matches one commit: the source generation is read (even: no batch
// writing), every file of the folder copied, the commit block last, and the
// generation read again; any change and the folder is read again.
int gxReplClient::sync_unit(gxReplSource &src, gxReplTarget &tgt, const gxReplSetConfig &sc,
			    const std::vector<const gxReplFile *> &files, const std::string &cb_path,
			    gxReplSetStatus &st, std::string &err)
{
  // 11/10/2026: the copy's commit block is kept odd (in progress, torn) from
  // the first change to its files until a whole commit is in; a failed
  // attempt copied the source's (even) block, so it is marked again
  auto mark = [&]() -> int {
    uint64_t tg = 0; int te = 0;
    if(tgt.CommitGen(sc.name, cb_path, sc.commit_gen_offset, tg, te, err) != 0) return -1;
    // always: the target also turns the unit's seal into "copying" (a writer refuses to open such a folder)
    if(tgt.MarkGen(sc.name, cb_path, sc.commit_gen_offset, te ? (tg | 1) : 1, err) != 0) return -1;
    return 0;
  };
  // 11/10/2026 (review): a retry lists the unit's files again (a file the
  // writer added or removed since the pass listed them is not missed)
  std::vector<gxReplFile> fresh;
  std::vector<const gxReplFile *> cur(files);
  for(int attempt = 0; attempt < 10 && !stop; attempt++) {
    uint64_t g0 = 0, g1 = 0;
    int exists = 0;
    if(attempt > 0) {
      if(src.Manifest(sc.name, fresh, err) != 0) return -1;
      cur.clear();
      std::string unit = dir_of(cb_path);
      for(auto &f : fresh) if(dir_of(f.path) == unit) cur.push_back(&f);
    }
    if(src.CommitGen(sc.name, cb_path, sc.commit_gen_offset, g0, exists, err) != 0) return -1;
    if(!exists) { err = cb_path + " is gone from the source"; return 1; }
    if(g0 & 1) { usleep(100000); continue; }        // a batch is writing: wait for it
    // the copy is marked in progress (an odd generation) before its files change
    if(mark() != 0) return -1;
    bool redo = false;
    for(auto *f : cur) {
      if(f->path == cb_path) continue;
      std::string e;
      int rc = sync_file(src, tgt, sc.name, f->path, 0, st, e);
      if(rc < 0) { err = e; return -1; }
      if(rc > 0) { redo = true; break; }
    }
    if(redo) { usleep(100000); continue; }
    // 11/10/2026: the commit block, read ONCE: its own generation must be g0
    // (even: no batch began since the files were read), and those very bytes
    // are written last, after the data files (their apply syncs them) and
    // the unit's seal (the hashes of its files)
    gxReplDiffRequest rq;
    rq.path = cb_path;
    rq.max_bytes = cfg.max_message;
    gxReplDiffResult res;
    if(src.Diff(sc.name, rq, res, err) != 0) return -1;
    uint64_t inside = 0;
    bool whole = res.status == 0 && !res.more && !res.update.pieces.empty() &&
      res.update.pieces[0].offset == 0 && res.update.pieces[0].data.size() >= sc.commit_gen_offset + 8;
    if(whole) memcpy(&inside, res.update.pieces[0].data.data() + sc.commit_gen_offset, 8);
    if(!whole || inside != g0) { if(mark() != 0) return -1; usleep(100000); continue; }
    // The seal before the commit block, while the copy is still marked odd: a stop between the two leaves an
    // odd (refused) copy, never a whole copy with a stale seal
    if(tgt.SealUnit(sc.name, dir_of(cb_path), g0, err) != 0) return -1;
    std::vector<gxReplFileUpdate> ups(1, res.update);
    std::vector<int> results;
    if(tgt.ApplyFiles(sc.name, ups, results, err) != 0) return -1;
    if(results.empty() || results[0] != 0) { if(mark() != 0) return -1; usleep(100000); continue; }
    st.files_sent++;
    (void)g1;
    {
      uint64_t tg = 0; int te = 0;
      if(tgt.CommitGen(sc.name, cb_path, sc.commit_gen_offset, tg, te, err) != 0) return -1;
      if(te && tg == g0) return 0;                 // the copy is commit g0, whole and sealed
    }
    if(mark() != 0) return -1;                     // not one commit: torn until the next try
    usleep(100000);                                // it committed meanwhile: read the folder again
  }
  if(mark() != 0) return -1;
  if(stop) return 0;
  // 11/11/2026 (review): the set's root folder has an empty relative name: say "." (was "gave up on  after")
  err = "gave up on " + (dir_of(cb_path).empty() ? std::string(".") : dir_of(cb_path)) +
    " after 10 tries (it commits faster than a copy)";
  return 1;
}

int gxReplClient::sync_set(gxReplSource &src, gxReplTarget &tgt,
			   const gxReplSetConfig &sc, bool once, std::string &err)
{
  gxReplSetStatus &st = status[sc.name];
  st.direction = sc.direction;
  long long now = now_s();
  std::vector<gxReplStream> streams;
  if(src.Streams(sc.name, streams, err) != 0) return -1;
  std::vector<gxReplStreamState> tv;
  if(tgt.State(sc.name, tv, err) != 0) return -1;
  std::map<std::string, gxReplStreamState> tstates;
  for(auto &x : tv) tstates[x.name] = x;

  bool asked, cverify;
  {
    std::lock_guard<std::mutex> g(flag_lock);
    asked = verify_seen[sc.name] != verify_gen;   // A SIGHUP this set has not answered
    cverify = cverify_sets.count(sc.name) > 0;    // A connect this set has not answered
  }
  bool full = once || asked || (sc.full_verify_interval > 0 && now - st.last_full_verify >= sc.full_verify_interval);
  bool verify = full || (sc.verify_interval > 0 && now - st.last_verify >= sc.verify_interval);
  // 11/08/2026: After a connect, verify the files the ship-log streams cover (the commit feed may have
  // moved on while the link was down). Plain files only get the size/mtime scan: hashing every one of
  // them on each connect read all of a 22 GB osa_data at idle I/O (13 hours) for nothing.

  auto &vlist = verify_streams[sc.name];

  // 1. The commit feed
  for(auto &s : streams) {
    gxReplStreamState ts = tstates[s.name];
    bool v = false;
    if(ts.log_id != s.log_id) v = true;
    else if(s.first_lsn > 0 && ts.lsn + 1 < s.first_lsn) v = true;
    else if(ts.lsn + 1 < s.next_lsn) {
      if(drain(src, tgt, sc.name, s, ts, st, v, err) != 0) return -1;
    }
    if(v && std::find(vlist.begin(), vlist.end(), s.name) == vlist.end()) {
      glog("set %s: stream %s needs a verify (%s)", sc.name.c_str(), s.name.c_str(),
	   ts.log_id.empty() ? "new" : ts.log_id != s.log_id ? "new log id" : "open, gap or old position");
      vlist.push_back(s.name);
    }
    { std::lock_guard<std::mutex> lg(st.m); st.streams[s.name] = std::make_pair(s.next_lsn ? s.next_lsn - 1 : 0, ts.lsn); }
  }

  // 2. Verify streams that opened, had a gap or are new to the copy
  std::vector<gxReplFile> sm;
  bool have_sm = false;
  for(auto it = vlist.begin(); it != vlist.end() && !stop; ) {
    const gxReplStream *s = 0;
    for(auto &x : streams) if(x.name == *it) s = &x;
    if(!s) { it = vlist.erase(it); continue; }
    if(!have_sm) { if(src.Manifest(sc.name, sm, err) != 0) return -1; have_sm = true; }
    int synced = 0, failed = 0;
    // 11/10/2026: the commit blocks last, and only when every other file of
    // the stream is in step: the copy's unit is "copying" until then
    std::vector<std::string> order;
    for(auto &f : s->files) if(sc.commit_block.empty() || base_of(f) != sc.commit_block) order.push_back(f);
    for(auto &f : s->files) if(!sc.commit_block.empty() && base_of(f) == sc.commit_block) order.push_back(f);
    for(auto &f : order) {
      if(failed && !sc.commit_block.empty() && base_of(f) == sc.commit_block) break;
      bool on_source = false;
      for(auto &m : sm) if(m.path == f) { on_source = true; break; }
      if(!on_source) continue;
      std::string e;
      int rc = sync_file(src, tgt, sc.name, f, s, st, e);
      if(rc < 0) { err = e; return -1; }
      if(rc > 0) { failed++; { std::lock_guard<std::mutex> lg(st.m); st.last_error = e; } glog("set %s: %s", sc.name.c_str(), e.c_str()); }
      else synced++;
    }
    if(failed) { ++it; continue; }
    if(synced == 0) {
      // No file left to match the position against: take the log's end
      gxReplStreamState ns;
      ns.name = s->name; ns.log_id = s->log_id; ns.lsn = s->next_lsn ? s->next_lsn - 1 : 0;
      if(tgt.SetState(sc.name, ns, err) != 0) return -1;
    }
    std::vector<gxReplStreamState> tv2;
    if(tgt.State(sc.name, tv2, err) != 0) return -1;
    for(auto &x : tv2) if(x.name == s->name) {
      tstates[x.name] = x;
      { std::lock_guard<std::mutex> lg(st.m); st.streams[s->name] = std::make_pair(s->next_lsn ? s->next_lsn - 1 : 0, x.lsn); }
      // 11/08/2026: The verified copy's position is durable: the retention floor
      if(static_cast<gxReplSource *>(&local) == &src && x.log_id == s->log_id) {
	std::string fe;
	if(local.WriteAckFloor(sc.name, s->name, peer.empty() ? "copy" : peer, x.lsn, fe) != 0)
	  glog("set %s: %s", sc.name.c_str(), fe.c_str());
      }
    }
    st.verifies++;
    glog("set %s: stream %s verified (%d file(s))", sc.name.c_str(), s->name.c_str(), synced);
    it = vlist.erase(it);
  }

  // 3. Scan, and verify when due
  if(verify || cverify || now - st.last_scan >= sc.scan_interval) {
    // 11/10/2026: The source's .gxrepl and ship logs: measured, and the ship logs capped
    if(static_cast<gxReplSource *>(&local) == &src) {
      unsigned long long before = 0;
      int removed = 0;
      st.ship_log_bytes = gxdShipLogEnforceTotal(sc.root, sc.ship_log_max_total, &before, &removed);
      st.gxrepl_bytes = dir_bytes(sc.root + "/.gxrepl");
      std::lock_guard<std::mutex> lg(st.m);
      if(sc.ship_log_max_total && before > sc.ship_log_max_total) {
	st.warn = "WARN: the ship logs reached ship_log_max_total (" + std::to_string(before) + " > " +
	  std::to_string(sc.ship_log_max_total) + " bytes): " + std::to_string(removed) +
	  " old segment(s) removed; a copy that needed them is verified whole";
	st.warn_time = now;
	glog("set %s: %s", sc.name.c_str(), st.warn.c_str());
      }
      else if(!st.warn.empty() && now - st.warn_time >= 3600) st.warn.clear();   // an hour under the cap
    }
    if(!have_sm) { if(src.Manifest(sc.name, sm, err) != 0) return -1; have_sm = true; }
    std::vector<gxReplFile> tm;
    if(tgt.Manifest(sc.name, tm, err) != 0) return -1;
    std::map<std::string, const gxReplFile *> tmap;
    for(auto &t : tm) tmap[t.path] = &t;
    std::map<std::string, const gxReplStream *> cover_of;
    for(auto &s : streams) for(auto &f : s.files) cover_of[f] = &s;
    long long recent_ns = now_ns() - (long long)sc.verify_recent_hours * 3600LL * 1000000000LL;
    int failures = 0;
    // 11/10/2026: Folders with a seqlock commit block (commit_block) go as a unit:
    // changed when the generation (or a file's size/mtime) differs, and copied
    // until the source generation was even and unchanged over the whole read
    std::set<std::string> unit_dirs;
    if(!sc.commit_block.empty()) {
      std::map<std::string, std::vector<const gxReplFile *> > folders;
      for(auto &f : sm) folders[dir_of(f.path)].push_back(&f);
      for(auto &fd : folders) {
	if(stop) break;
	const gxReplFile *cb = 0;
	bool covered = false;
	for(auto *f : fd.second) {
	  if(base_of(f->path) == sc.commit_block) cb = f;
	  if(cover_of.count(f->path)) covered = true;
	}
	if(!cb || covered) continue;                 // no commit block, or the ship log keeps it
	unit_dirs.insert(fd.first);
	bool changed = full || verify;
	for(auto *f : fd.second) {
	  auto t = tmap.find(f->path);
	  if(t == tmap.end()) { changed = true; continue; }
	  // 11/10/2026: the commit block goes by its generation, never its size or
	  // mtime (a heartbeat write must not make the folder be read again)
	  if(f == cb) continue;
	  if(t->second->size != f->size || t->second->mtime_ns != f->mtime_ns || t->second->mode != f->mode) changed = true;
	}
	if(!changed) {
	  uint64_t sg = 0, tg = 0, sealed = 0; int se = 0, te = 0; std::string e;
	  if(src.CommitGen(sc.name, cb->path, sc.commit_gen_offset, sg, se, e) != 0) { err = e; return -1; }
	  if(tgt.CommitGen(sc.name, cb->path, sc.commit_gen_offset, tg, te, e) != 0) { err = e; return -1; }
	  // 11/10/2026: a copy not sealed at its own generation is copied (and sealed) again
	  if(tgt.SealGen(sc.name, fd.first, sealed, e) != 0) { err = e; return -1; }
	  changed = !te || sg != tg || sealed != tg;
	}
	if(!changed) continue;
	std::string e;
	int rc = sync_unit(src, tgt, sc, fd.second, cb->path, st, e);
	if(rc < 0) { err = e; return -1; }
	if(rc > 0) { failures++; { std::lock_guard<std::mutex> lg(st.m); st.last_error = e; } glog("set %s: %s", sc.name.c_str(), e.c_str()); }
      }
    }
    // 11/10/2026: in a folder the ship log keeps, the commit block comes only
    // in the records (gxdShipLog::SetCommitBlock): never by this loop, whose
    // copy would race the records (an old-format log without it: unsealed)
    std::set<std::string> log_dirs, sent_dirs, failed_dirs;
    std::vector<const gxReplFile *> deferred_cb;
    if(!sc.commit_block.empty()) for(auto &cf : cover_of) log_dirs.insert(dir_of(cf.first));
    for(auto &f : sm) {
      if(stop) break;
      if(!unit_dirs.empty() && unit_dirs.count(dir_of(f.path))) continue;
      if(!log_dirs.empty() && base_of(f.path) == sc.commit_block && log_dirs.count(dir_of(f.path))) {
	if(cover_of.count(f.path)) deferred_cb.push_back(&f);   // after the folder's other files
	continue;
      }
      // 11/08/2026: Keep the status current during a long pass
      if(now_s() - last_status_write >= 5) write_status(pass_connected, "");
      auto t = tmap.find(f.path);
      bool missing = t == tmap.end();
      bool changed = missing || t->second->size != f.size ||
	t->second->mtime_ns != f.mtime_ns || t->second->mode != f.mode;
      bool check = full || (verify && f.mtime_ns >= recent_ns);
      const gxReplStream *cover = 0;
      auto c = cover_of.find(f.path);
      if(c != cover_of.end()) cover = c->second;
      if(cover) {
	// The commit feed keeps it; compare only on a verify (or after a connect) or if missing
	if(!missing && !check && !cverify) continue;
	if(std::find(vlist.begin(), vlist.end(), cover->name) != vlist.end()) continue;
	auto ts = tstates.find(cover->name);
	if(ts == tstates.end() || ts->second.log_id != cover->log_id) continue;
      }
      else if(!changed && !check) continue;
      std::string e;
      int rc = sync_file(src, tgt, sc.name, f.path, cover, st, e);
      if(rc < 0) { err = e; return -1; }
      if(rc > 0) { failures++; failed_dirs.insert(dir_of(f.path)); { std::lock_guard<std::mutex> lg(st.m); st.last_error = e; } glog("set %s: %s", sc.name.c_str(), e.c_str()); }
      else sent_dirs.insert(dir_of(f.path));
    }
    // 11/10/2026: a ship-log folder's commit block, from the log, after its
    // other files (it seals the unit): when one of them was synced, or it is
    // missing on the copy, or on a verify
    for(auto *cbf : deferred_cb) {
      if(stop) break;
      std::string d = dir_of(cbf->path);
      if(failed_dirs.count(d)) continue;
      auto cv = cover_of.find(cbf->path);
      bool missing = tmap.find(cbf->path) == tmap.end();
      if(!missing && !sent_dirs.count(d) && !(full || verify || cverify)) continue;
      if(std::find(vlist.begin(), vlist.end(), cv->second->name) != vlist.end()) continue;
      std::string e;
      int rc = sync_file(src, tgt, sc.name, cbf->path, cv->second, st, e);
      if(rc < 0) { err = e; return -1; }
      if(rc > 0) { failures++; { std::lock_guard<std::mutex> lg(st.m); st.last_error = e; } glog("set %s: %s", sc.name.c_str(), e.c_str()); }
    }
    if(sc.deletes && !stop) {
      std::set<std::string> on_source;
      for(auto &f : sm) on_source.insert(f.path);
      std::vector<std::string> gone;
      for(auto &t : tm) if(!on_source.count(t.path)) gone.push_back(t.path);
      for(size_t i = 0; i < gone.size(); i += 500) {
	std::vector<std::string> part(gone.begin() + i, gone.begin() + std::min(gone.size(), i + 500));
	if(tgt.Delete(sc.name, part, err) != 0) return -1;
	st.deletes += part.size();
      }
    }
    st.last_scan = now;
    if(verify && failures == 0) st.last_verify = now;
    if(full && failures == 0) st.last_full_verify = now;
    if(failures) return 1;
  }
  return vlist.empty() ? 0 : 1;
}

// 11/10/2026: derived index state, one file per folder:
// <root>/.gxrepl/derived/<folder with '/' as '#'>: "<generation> ok|failed <note>"
static std::string derived_state_path(const gxReplSetConfig &sc, const std::string &rel)
{
  std::string n = rel.empty() ? std::string("_root") : "_" + rel;   // never "." or a dot name
  for(auto &c : n) if(c == '/') c = '#';
  return sc.root + "/.gxrepl/derived/" + n;
}

// 11/10/2026: the lsn of the ship log record that last sealed a unit ("log G L"), 0 if the ship log did not
static unsigned long long unit_log_lsn(const gxReplSetConfig &sc, const std::string &rel, unsigned long long *gen = 0)
{
  FILE *f = fopen((sc.root + "/.gxrepl/units/" + unit_key(rel)).c_str(), "rb");
  if(!f) return 0;
  unsigned long long g = 0, l = 0;
  if(fscanf(f, "log %llu %llu", &g, &l) != 2) l = 0;
  fclose(f);
  if(gen) *gen = g;
  return l;
}

// The rebuild's stamp, <folder>/derived.stamp: "gen G lsn L records N keys K"
// (the derived_rebuild command writes it; osa_ingest --rebuild-index does)
static bool derived_stamp_ok(const gxReplSetConfig &sc, const std::string &rel, uint64_t gen, std::string &why)
{
  std::string dir = rel.empty() ? sc.root : sc.root + "/" + rel;
  FILE *f = fopen((dir + "/derived.stamp").c_str(), "r");
  if(!f) { why = "not stamped (no derived.stamp)"; return false; }
  unsigned long long g = 0, l = 0, n = 0, k = 0;
  int got = fscanf(f, "gen %llu lsn %llu records %llu keys %llu", &g, &l, &n, &k);
  fclose(f);
  if(got != 4) { why = "a damaged derived.stamp"; return false; }
  if(g != gen) { why = "stamped at generation " + std::to_string(g) + ", the copy is at " + std::to_string((unsigned long long)gen); return false; }
  if(n != k) { why = "stamped with " + std::to_string(k) + " keys for " + std::to_string(n) + " records"; return false; }
  unsigned long long sl = unit_log_lsn(sc, rel);
  if(sl && l != sl) { why = "stamped at ship log record " + std::to_string(l) + ", the copy applied " + std::to_string(sl); return false; }
  return true;
}

// 11/11/2026 (r4e): a folder's derived indexes: 0 good for this generation,
// 1 behind (missing, stale, not stamped yet: a busy writer's day never goes
// quiet; the take over rebuilds them, osa_ingest's writer open), 2 bad (a
// rebuild that ran and failed, a damaged state or stamp: a take over's
// rebuild would fail the same way)
static int derived_status(const gxReplSetConfig &sc, const std::string &rel, uint64_t gen, std::string &why)
{
  FILE *f = fopen(derived_state_path(sc, rel).c_str(), "r");
  if(!f) { why = "missing (never built)"; return 1; }
  unsigned long long g = 0; char st[32] = {0};
  int n = fscanf(f, "%llu %31s", &g, st);
  fclose(f);
  if(n != 2) { why = "unknown (a damaged state file)"; return 2; }
  if(strcmp(st, "ok") != 0) { why = "failed their rebuild or check (generation " + std::to_string(g) + ")"; return 2; }
  if(g != gen) { why = "stale (built from generation " + std::to_string(g) + ", the copy is at " + std::to_string((unsigned long long)gen) + ")"; return 1; }
  // and every derived file the rebuild made is there
  std::string dir = rel.empty() ? sc.root : sc.root + "/" + rel;
  DIR *d = opendir(dir.c_str());
  int found = 0;
  struct dirent *e;
  while(d && (e = readdir(d)) != 0) if(e->d_name[0] != '.' && is_derived(sc, rel.empty() ? std::string(e->d_name) : rel + "/" + e->d_name)) found++;
  if(d) closedir(d);
  if(!found) { why = "missing (no derived file in the folder)"; return 1; }
  if(derived_stamp_ok(sc, rel, gen, why)) return 0;
  // a stamp that does not say this generation / record is behind; one that is
  // damaged or holds the wrong key count is a bad rebuild
  return (why.compare(0, 10, "stamped at") == 0 || why.compare(0, 11, "not stamped") == 0) ? 1 : 2;
}

static bool derived_ok(const gxReplSetConfig &sc, const std::string &rel, uint64_t gen, std::string &why)
{
  return derived_status(sc, rel, gen, why) == 0;
}

static void derived_folders(const gxReplSetConfig &sc, const std::string &rel, std::vector<std::string> &out)
{
  std::string dir = rel.empty() ? sc.root : sc.root + "/" + rel;
  DIR *d = opendir(dir.c_str());
  if(!d) return;
  std::vector<std::string> subs;
  bool has_cb = false;
  struct dirent *e;
  while((e = readdir(d)) != 0) {
    std::string n = e->d_name;
    if(n.empty() || n[0] == '.') continue;
    struct stat st;
    if(lstat((dir + "/" + n).c_str(), &st) != 0) continue;
    if(S_ISDIR(st.st_mode)) subs.push_back(rel.empty() ? n : rel + "/" + n);
    else if(n == sc.commit_block) has_cb = true;
  }
  closedir(d);
  if(has_cb) out.push_back(rel);
  for(auto &s : subs) derived_folders(sc, s, out);
}

int gxReplDerivedRefresh(const gxReplSetConfig &sc,
			 std::map<std::string, std::pair<uint64_t, long long> > &seen,
			 std::vector<std::string> &log)
{
  if(sc.derived.empty() || sc.derived_rebuild.empty() || sc.commit_block.empty()) return 0;
  std::vector<std::string> folders;
  derived_folders(sc, "", folders);
  int rebuilt = 0;
  long long now = (long long)time(0);
  // 11/11/2026 (r4e): when a folder's indexes went behind, and when they
  // were last rebuilt: a busy day (never quiet for derived_delay) is
  // rebuilt every derived_busy_rebuild seconds anyway
  static std::map<std::string, long long> behind_since, built_at;
  static std::set<std::string> told;            // folders whose "a writer is active" skip was logged
  for(auto &rel : folders) {
    std::string key = sc.name + "|" + rel;
    std::string dir = rel.empty() ? sc.root : sc.root + "/" + rel;
    auto read_gen = [&](uint64_t &g) -> bool {
      int fd = open((dir + "/" + sc.commit_block).c_str(), O_RDONLY);
      ssize_t n = fd >= 0 ? pread(fd, &g, sizeof(g), (off_t)sc.commit_gen_offset) : -1;
      if(fd >= 0) close(fd);
      return n == (ssize_t)sizeof(g);
    };
    uint64_t g = 0;
    if(!read_gen(g) || (g & 1)) continue;                       // torn or in progress: not yet
    std::string why;
    if(derived_ok(sc, rel, g, why)) { seen.erase(rel); behind_since.erase(key); continue; }
    if(!behind_since.count(key)) behind_since[key] = now;
    bool busy = sc.derived_busy_rebuild > 0 && now - behind_since[key] >= sc.derived_busy_rebuild &&
      now - (built_at.count(key) ? built_at[key] : 0) >= sc.derived_busy_rebuild;
    auto &s = seen[rel];
    if(!busy) {
      if(s.first != g || s.second == 0) { s.first = g; s.second = now; continue; }   // first seen: wait to be quiet
      if(now - s.second < sc.derived_delay) continue;
    }
    // 11/11/2026 (review r4e): never while a local writer is active (a take over: the writer owns its
    // indexes, and its own open rebuilds them; a rebuild here would rename files under it). The writer
    // locks are held shared for the whole rebuild, so a writer starting meanwhile waits or refuses.
    WriterHold wh;
    std::string we;
    if(hold_writer_locks(sc, wh, we)) {
      if(told.insert(key).second)
	log.push_back("set " + sc.name + ": " + (rel.empty() ? std::string(".") : rel) +
		      ": derived indexes not rebuilt: " + we);
      continue;
    }
    // The apply lock (exclusive): no apply changes the folder while its indexes are rebuilt
    std::string jname = sc.root + "/.gxrepl/apply.jnl";
    mkdirs(sc.root + "/.gxrepl/derived");
    int lk = open(jname.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if(lk < 0 || flock(lk, LOCK_EX) != 0) { if(lk >= 0) close(lk); continue; }
    uint64_t g2 = 0;
    // (a busy rebuild takes the generation the folder holds now: no apply
    // can change it while the lock is held)
    if(!read_gen(g2) || (g2 & 1) || (!busy && g2 != g)) { flock(lk, LOCK_UN); close(lk); s.first = g2; s.second = now; continue; }
    g = g2;
    std::string cmd = sc.derived_rebuild, q = "'";
    for(char c : dir) { if(c == '\'') q += "'\\''"; else q += c; }
    q += "'";
    for(size_t at; (at = cmd.find("{dir}")) != std::string::npos; ) cmd.replace(at, 5, q);
    // 11/10/2026: {lsn}: the ship log record the folder was sealed at (0 if none), for the stamp
    std::string ls = std::to_string(unit_log_lsn(sc, rel));
    for(size_t at; (at = cmd.find("{lsn}")) != std::string::npos; ) cmd.replace(at, 5, ls);
    int rc = run_quiet(cmd);
    flock(lk, LOCK_UN);
    close(lk);
    // 11/11/2026 (review): exit 3 = the folder moved during the rebuild (osa_ingest --rebuild-index): not a
    // failure, the indexes stay behind and are tried again (only a real failure is recorded as failed)
    if(rc != -1 && WIFEXITED(rc) && WEXITSTATUS(rc) == 3) {
      log.push_back("set " + sc.name + ": " + (rel.empty() ? std::string(".") : rel) +
		    ": derived indexes not rebuilt: the folder moved during the rebuild (tried again)");
      s.first = 0; s.second = 0;
      built_at[key] = now;
      continue;
    }
    bool ok = rc == 0;
    std::string body = std::to_string((unsigned long long)g) + (ok ? " ok" : " failed") +
      " rc=" + std::to_string(rc) + "\n";
    if(write_atomic(derived_state_path(sc, rel), body) != 0)
      log.push_back("set " + sc.name + ": cannot write " + derived_state_path(sc, rel));
    log.push_back("set " + sc.name + ": " + (rel.empty() ? std::string(".") : rel) + ": derived indexes " +
		  (ok ? "rebuilt" : "FAILED (rc " + std::to_string(rc) + ")") + " for generation " +
		  std::to_string((unsigned long long)g) + (busy ? " (busy: never quiet)" : ""));
    seen.erase(rel);
    built_at[key] = now;
    told.erase(key);
    rebuilt++;
  }
  return rebuilt;
}

// 11/10/2026: see gxrepl.h
static void torn_walk(const gxReplSetConfig &sc, const std::string &root, const std::string &rel,
		      int &checked, std::vector<std::string> &torn, std::vector<std::string> &warn)
{
  std::string dir = rel.empty() ? root : root + "/" + rel;
  DIR *d = opendir(dir.c_str());
  if(!d) return;
  std::vector<std::string> subs;
  bool has_db = false, has_cb = false;
  struct dirent *e;
  while((e = readdir(d)) != 0) {
    std::string n = e->d_name;
    if(n.empty() || n[0] == '.') continue;                 // .gxrepl, publish leftovers
    std::string p = dir + "/" + n;
    struct stat st;
    if(lstat(p.c_str(), &st) != 0) continue;
    if(S_ISDIR(st.st_mode)) { subs.push_back(rel.empty() ? n : rel + "/" + n); continue; }
    if(!S_ISREG(st.st_mode)) continue;
    size_t dot = n.find_last_of('.');
    std::string ext = dot == std::string::npos ? "" : n.substr(dot);
    if(ext == ".gxd" || ext == ".btx") has_db = true;
    if(n == sc.commit_block) has_cb = true;
  }
  closedir(d);
  if(has_db || has_cb) {
    checked++;
    std::string where = rel.empty() ? "." : rel;
    if(!has_cb) torn.push_back(where + ": UNSEALED: database files and no " + sc.commit_block +
			       " (a copy that never got one, or a ship log whose records do not carry it)");
    else {
      uint64_t g = 0;
      int fd = open((dir + "/" + sc.commit_block).c_str(), O_RDONLY);
      ssize_t n = fd >= 0 ? pread(fd, &g, sizeof(g), (off_t)sc.commit_gen_offset) : -1;
      if(fd >= 0) close(fd);
      if(n != (ssize_t)sizeof(g)) torn.push_back(where + ": " + sc.commit_block + " is short");
      else if(g & 1) torn.push_back(where + ": generation " + std::to_string((unsigned long long)g) +
				    " is odd (a copy in progress, or one that stopped half way)");
      else {
	// 11/10/2026: an even generation is not enough: the files must be the sealed ones
	std::string why = unit_seal_check(sc, rel, g);
	if(!why.empty()) torn.push_back(where + ": " + why);
	// and its derived indexes: a bad rebuild is torn (a take over's would
	// fail too); indexes behind a busy day are a warning: the take over
	// rebuilds them (11/11/2026, r4e)
	else if(!sc.derived.empty()) {
	  int ds = derived_status(sc, rel, g, why);
	  if(ds == 2) torn.push_back(where + ": derived indexes " + why);
	  else if(ds == 1) warn.push_back(where + ": derived indexes " + why + " (rebuilt by the take over's writer open)");
	}
      }
    }
  }
  for(auto &s : subs) torn_walk(sc, root, s, checked, torn, warn);
}

int gxReplTornCopies(const gxReplSetConfig &sc, std::vector<std::string> &torn, std::vector<std::string> &warn)
{
  torn.clear();
  warn.clear();
  int checked = 0;
  if(sc.commit_block.empty()) return 0;
  torn_walk(sc, sc.root, "", checked, torn, warn);
  return checked;
}

int gxReplTornCopies(const gxReplSetConfig &sc, std::vector<std::string> &torn)
{
  std::vector<std::string> warn;
  return gxReplTornCopies(sc, torn, warn);
}

unsigned long long gxReplClient::work_count() const
{
  unsigned long long n = 0;
  for(auto &e : status) n += e.second.records + e.second.files_sent + e.second.pieces_sent +
			  e.second.deletes + e.second.verifies;
  return n;
}

int gxReplClient::pass(gxReplRemote &remote, bool once, const std::string &only_set,
		       std::string &err)
{
  gxReplRemoteTarget rtgt(remote);
  int worst = 0;
  unsigned long gen0 = verify_gen;
  for(auto &sc : cfg.sets) {
    if(stop) break;
    if(!only_set.empty() && sc.name != only_set) continue;
    gxReplSetStatus &st = status[sc.name];
    auto ps = remote.PeerSets().find(sc.name);
    std::string e;
    int rc;
    if(ps == remote.PeerSets().end()) {
      e = "the server has no set " + sc.name; rc = 1;
    }
    else if(sc.direction == "push" && ps->second == "source") {
      e = "the server's set " + sc.name + " is a source, not a copy"; rc = 1;
    }
    else if(sc.direction == "pull" && ps->second == "target") {
      e = "the server's set " + sc.name + " is a copy, not a source"; rc = 1;
    }
    else if(sc.direction == "push") rc = sync_set(local, rtgt, sc, once, e);
    else rc = sync_set(remote, local, sc, once, e);
    if(rc == 0) {
      if(!st.ok) glog("set %s: in step", sc.name.c_str());
      st.ok = true; st.last_ok = now_s(); { std::lock_guard<std::mutex> lg(st.m); st.last_error.clear(); }
    }
    else {
      if(!e.empty()) {
	if(e != st.last_error) glog("set %s: %s", sc.name.c_str(), e.c_str());
	{ std::lock_guard<std::mutex> lg(st.m); st.last_error = e; }
      }
      st.ok = false;
      if(rc > worst) worst = rc;
    }
    if(rc < 0 && !remote.IsOpen()) { err = e; return -1; }
    if(rc < 0) worst = 2;
  }
  {
    // 11/08/2026: The verify requests this pass answered, for its sets only
    std::lock_guard<std::mutex> g(flag_lock);
    for(auto &sc : cfg.sets) {
      if(!only_set.empty() && sc.name != only_set) continue;
      verify_seen[sc.name] = gen0;
      cverify_sets.erase(sc.name);
    }
  }
  return worst;
}

int gxReplClient::Run(bool once, const std::string &only_set, std::string &err)
// 11/08/2026: Protocol 3: one connection, one multiplex channel and one
// thread for each set, so the sets replicate at once and a large set
// does not hold up a small one.
{
  signal(SIGPIPE, SIG_IGN);
  if(local.RecoverAll(err) != 0) return 2;
  {
    // Every set's status and flags exist before any set's thread starts
    std::lock_guard<std::mutex> g(flag_lock);
    for(auto &sc : cfg.sets) {
      status[sc.name].direction = sc.direction;
      verify_seen[sc.name] = verify_gen;
      verify_streams[sc.name];   // Each set's thread then only looks its own entry up
    }
  }
  int backoff = 1;
  while(!stop) {
    gxReplRemote remote(cfg);
    std::string e;
    if(remote.ConnectMux(e) != 0) {
      glog("cannot connect: %s (retry in %d s)", e.c_str(), backoff);
      peer.clear();
      write_status(false, e);
      if(once) { err = e; return 2; }
      interruptible_sleep(backoff * 1000, stop);
      backoff = std::min(backoff * 2, 60);
      continue;
    }
    backoff = 1;
    peer = remote.PeerNode();
    connected_since = now_s();
    glog("connected to %s (%s)", peer.c_str(), cfg.ssh_tunnel.empty() ? "direct" : "ssh tunnel");
    {
      // A connect verifies the ship-log streams of every set once
      std::lock_guard<std::mutex> g(flag_lock);
      cverify_sets.clear();
      for(auto &sc : cfg.sets) cverify_sets.insert(sc.name);
    }
    pass_connected = true;
    int rc = run_sets(remote, once, only_set, e);
    pass_connected = false;
    if(once) {
      if(rc != 0) err = e.empty() ? "a set is not in step (see the status)" : e;
      write_status(remote.IsOpen(), e);
      return rc;
    }
    if(!stop) glog("connection lost: %s", e.c_str());
    write_status(false, "disconnected");
  }
  return 0;
}

int gxReplClient::run_sets(gxReplRemote &remote, bool once, const std::string &only_set,
			   std::string &err)
// Returns (once) 0 every set in step, 1 a set not, 2 the connection
// failed; otherwise when the connection ends or Stop().
{
  struct Job {
    const gxReplSetConfig *sc = 0;
    std::unique_ptr<gxReplRemote> ch;
    int rc = 0;
    std::string e;
    std::atomic<bool> done{false};
  };
  std::vector<std::unique_ptr<Job> > jobs;
  uint32_t next = 2;
  for(auto &sc : cfg.sets) {
    if(!only_set.empty() && sc.name != only_set) continue;
    std::unique_ptr<Job> j(new Job);
    j->sc = &sc;
    std::string e;
    uint32_t ch = next++;
    if(remote.OpenSetChannel(ch, sc.name, sc.direction, e) != 0) {
      // Refused (no such set, the wrong role): this set only
      gxReplSetStatus &st = status[sc.name];
      if(e != st.last_error) glog("set %s: %s", sc.name.c_str(), e.c_str());
      st.ok = false; { std::lock_guard<std::mutex> lg(st.m); st.last_error = e; }
      j->rc = remote.IsOpen() ? 1 : -1; j->e = e; j->done = true;
      jobs.push_back(std::move(j));
      continue;
    }
    j->ch.reset(new gxReplRemote(cfg));
    j->ch->UseChannel(remote, ch);
    jobs.push_back(std::move(j));
  }
  std::vector<std::thread> threads;
  std::atomic<uint32_t> next_ch{next};
  for(auto &jp : jobs) {
    Job *j = jp.get();
    if(j->done) continue;
    threads.emplace_back([this, j, once, &remote, &next_ch]() {
      while(!stop) {
	std::string pe;
	int rc = pass(*j->ch, once, j->sc->name, pe);
	// 11/12/2026 (F4e): a channel out of step (a request without a reply in time): a fresh one, the set goes on
	if(rc != 0 && j->ch->ChannelBroken() && remote.IsOpen()) {
	  uint32_t nch = next_ch++;
	  std::string oe;
	  if(remote.OpenSetChannel(nch, j->sc->name, j->sc->direction, oe) == 0) {
	    glog("set %s: channel reset after \"%s\" (new channel %u)", j->sc->name.c_str(), pe.c_str(), nch);
	    j->ch.reset(new gxReplRemote(cfg));
	    j->ch->UseChannel(remote, nch);
	    interruptible_sleep(cfg.poll_ms, stop);
	    continue;
	  }
	  glog("set %s: cannot open a fresh channel: %s", j->sc->name.c_str(), oe.c_str());
	}
	if(rc < 0 && !j->ch->IsOpen()) { j->rc = -1; j->e = pe; break; }
	if(once) {
	  // A second pass finds nothing to do when the set is the same
	  if(rc == 0) rc = pass(*j->ch, false, j->sc->name, pe);
	  j->rc = rc < 0 ? -1 : rc; j->e = pe;
	  break;
	}
	interruptible_sleep(cfg.poll_ms, stop);
      }
      j->done = true;
    });
  }
  // The status while the sets run
  for(;;) {
    bool all = true;
    for(auto &jp : jobs) if(!jp->done) all = false;
    if(all) break;
    if(now_s() - last_status_write >= 2) write_status(remote.IsOpen(), "");
    usleep(200000);
  }
  for(auto &t : threads) t.join();
  int worst = 0;
  std::string e;
  for(auto &jp : jobs) {
    if(jp->rc < 0) { worst = -1; e = jp->e; }
    else if(worst >= 0 && jp->rc > worst) { worst = jp->rc; e = jp->e; }
  }
  err = e;
  if(once) return worst < 0 ? 2 : (worst == 0 ? 0 : 1);
  return worst < 0 ? -1 : 0;
}

} // namespace datareel

#else // __WIN32__

namespace datareel {
int gxReplConfig::Load(const std::string &, std::string &err)
{ err = "replication is not supported on Windows"; return -1; }
const gxReplSetConfig *gxReplConfig::Set(const std::string &) const { return 0; }
} // namespace datareel

#endif // __WIN32__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
