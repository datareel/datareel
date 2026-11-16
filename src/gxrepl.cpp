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
#include <dlfcn.h>
#include <limits.h>
#include <climits>
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
#include <condition_variable>
#include "gxdjrnl.h"
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <sys/statvfs.h>
#include <linux/fs.h>
#include "gxdpcache.h"
#include "gxdstamp.h"
#include "gxbtree.h"
#include "gxdstage.h"
#include "gxrextract.h"
#include "gxvmember.h"

#if defined (DATAREEL_HAVE_ZLIB)
#include <zlib.h>
#endif

namespace datareel {

#if defined(GXREPL_TEST_HOOKS)
extern GXDLCODE_API std::atomic<int> gxdJournalTestCrashNext;   // gxdjrnl.cpp
// 11/14/2026 (test builds): GXREPL_TEST_CRASH_IN_APPLY=log:N or files:N: the server ends inside its Nth ApplyLog
// or ApplyFiles batch, after the apply journal has it and before its pages reach the files
static thread_local const char *apply_kind = "";
static void test_crash_in_apply()
{
  const char *e = getenv("GXREPL_TEST_CRASH_IN_APPLY");
  if(!e) return;
  std::string v(e);
  size_t c = v.find(':');
  if(c == std::string::npos || v.compare(0, c, apply_kind) != 0) return;
  static std::atomic<long> seen_log(0), seen_files(0);
  long n = atol(v.c_str() + c + 1);
  long k = strcmp(apply_kind, "log") == 0 ? ++seen_log : ++seen_files;
  if(k == n) gxdJournalTestCrashNext = 1;
}
#endif

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
// 11/13/2026: a dead copy folder's files must be quiet this long (GXREPL_DEAD_QUIET_SEC, default 600 s)
static long long dead_quiet_sec()
{
  const char *e = getenv("GXREPL_DEAD_QUIET_SEC");
  return e && *e ? std::max(0LL, atoll(e)) : 600;
}

// 11/15/2026 (hardening, PurgeOrphans): a "copying" seal unchanged for the quiet time is a copy nobody finishes (the
// source's stream was purged with its folder while the copy was still "copying": ACK is applied, not sealed); a fill
// in progress writes its files and its seal within seconds
// (GXREPL_COPYING_ABANDONED_SEC, default 3600 s; the folder must hold no database file as well: dead_unit_candidate)
static bool copying_abandoned(const std::string &seal_path)
{
  const char *e = getenv("GXREPL_COPYING_ABANDONED_SEC");
  long long q = e && *e ? std::max(0LL, atoll(e)) : 3600;
  struct stat st;
  return stat(seal_path.c_str(), &st) == 0 && (long long)st.st_mtime < (long long)time(0) - q;
}

// 11/13/2026: the dead copy folder sweep's period (GXREPL_SWEEP_SEC, default 60 s)
static long long sweep_every()
{
  const char *e = getenv("GXREPL_SWEEP_SEC");
  return e && *e ? std::max(1LL, atoll(e)) : 60;
}

// 11/14/2026: test builds (-DGXREPL_TEST_HOOKS): GXREPL_TEST_POSITION_DELAY_MS=N sleeps between an apply's
// transaction (its seals written) and its streams' applied position write (a position write that lags under load)
static void test_position_delay()
{
#ifdef GXREPL_TEST_HOOKS
  if(const char *d = getenv("GXREPL_TEST_POSITION_DELAY_MS"))
    std::this_thread::sleep_for(std::chrono::milliseconds(atol(d)));
#endif
}

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
      else if(k == "vectors") curdb->vectors = v;      // 11/14/2026 (7.4): a .vixset or a .vix
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
      else if(k == "derived_apply") {        // 11/14/2026: rebuild | incremental
	if(v != "rebuild" && v != "incremental") {
	  fclose(f); err = fname + ":" + std::to_string(ln) + ": derived_apply must be rebuild or incremental";
	  return -1;
	}
	cur->derived_apply = v;
      }
      else if(k == "derived_extractor") cur->derived_extractor = v;
      else if(k == "derived_extractor_config") cur->derived_extractor_config = v;
      else if(k == "derived_source") cur->derived_source = v;
      else if(k == "derived_apply_max_mb") cur->derived_apply_max_mb = strtoull(v.c_str(), 0, 10);
      else if(k == "derived_count") cur->derived_count = v;   // 11/14/2026 (review 2)
      else if(k == "derived_apply_max_s") cur->derived_apply_max_s = strtoull(v.c_str(), 0, 10);   // review 5
      else if(k == "staged_fill") cur->staged_fill = (v == "yes" || v == "true" || v == "1");   // 11/12/2026 F7
      else if(k == "stage_max_bytes") cur->stage_max_bytes = strtoull(v.c_str(), 0, 10);
      else known = false;
    }
    else {
      if(k == "node") node = v;
      else if(k == "legacy_db") legacy_db = v;   // 11/08/2026: 6.x style object clients
      else if(k == "vector_threads") vector_threads = (unsigned)atoi(v.c_str());          // 11/14/2026 (7.4)
      else if(k == "vector_max_concurrent") vector_max_concurrent = (unsigned)atoi(v.c_str());
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
  // 11/14/2026 (review): the log.id file each cached reader was opened on (inode, mtime): a stream's log made again
  // (its folder removed and a writer opening a new log) gets a new reader, not the old log's ghost
  std::map<std::string, std::string> reader_id;     // (11/15: the log.id file's content, not its inode and mtime)
  gxdShipLogReader *reader(const std::string &stream, std::string &err);
  std::set<std::string> staged;   // 11/13/2026 (F7 review 3): units this process began a staged copy of (MarkGen)
};

// 11/14/2026 (review): the stream's cached reader, made again when the log's log.id changed or is gone (s->m held)
gxdShipLogReader *gxReplLocal::SetState_::reader(const std::string &stream, std::string &err)
{
  SetState_ *s = this;
  const std::string dir = s->root + "/.gxrepl/log/" + stream;
  // 11/15/2026: the log id itself (log.id's content): a log removed and made again at once can reuse the old
  // log.id's inode and get the same mtime (coarse file times), so inode and mtime missed it 1 time in 8
  std::string id;
  if(FILE *f = fopen((dir + "/log.id").c_str(), "r")) {
    char b[64] = {0};
    size_t n = fread(b, 1, sizeof(b) - 1, f);
    fclose(f);
    id.assign(b, n);
  }
  auto &rd = s->readers[stream];
  if(rd && s->reader_id[stream] != id) rd.reset();
  if(!rd) {
    rd.reset(new gxdShipLogReader);
    if(rd->Open(dir) != 0) {
      err = rd->Error(); rd.reset(); s->readers.erase(stream); s->reader_id.erase(stream); return 0;
    }
    s->reader_id[stream] = id;
  }
  return rd.get();
}

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
  // 11/14/2026: "<derived>.stamp" (gxdstamp.h) is derived too: a copy's own,
  // never sent, never deleted, and a primary's never shipped over it
  if(gxdDerivedStamp::IsStamp(base)) return is_derived(sc, rel.substr(0, rel.size() - 6));
  return false;
}

// 11/14/2026: a derived index file itself (not its stamp)
static bool is_derived_index(const gxReplSetConfig &sc, const std::string &rel)
{
  return !gxdDerivedStamp::IsStamp(rel) && is_derived(sc, rel);
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
  // 11/14/2026: a derived index stamp (<index>.stamp) and the rebuild's
  // derived.stamp are never data files, even when an "include" pattern
  // matches them
  if(gxdDerivedStamp::IsStamp(base) || base == "derived.stamp") return false;
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

// ---------------- Staged unit copies (11/12/2026, F7) ----------------
// <root>/.gxrepl/stage/<unit key>/ holds a copy-on-write clone of a unit's files while a copy of a newer commit
// fills it; ".stage" names the unit, when it began, the copy's sealed generation then, and the files cloned
// ("file NAME" lines); ".seal" is the new seal (SealUnit); ".swap" (the stage's file list) is written before the
// files are renamed into the unit, so a stop in the middle is finished at the next start or by gxrepl promote.
static std::string stage_dir(const std::string &root, const std::string &unit)
{
  return root + "/.gxrepl/stage/" + unit_key(unit);
}

static std::vector<std::string> stage_lines(const std::string &path, const char *prefix);

static std::string read_small(const std::string &path)
{
  std::string out;
  FILE *f = fopen(path.c_str(), "rb");
  if(!f) return out;
  char b[65536]; size_t n;
  while((n = fread(b, 1, sizeof(b), f)) > 0) out.append(b, n);
  fclose(f);
  return out;
}

static bool read_unit_gen(const gxReplSetConfig &sc, const std::string &dir, uint64_t &g)
{
  int fd = open((dir + "/" + sc.commit_block).c_str(), O_RDONLY | O_CLOEXEC);
  ssize_t n = fd >= 0 ? pread(fd, &g, sizeof(g), (off_t)sc.commit_gen_offset) : -1;
  if(fd >= 0) close(fd);
  return n == (ssize_t)sizeof(g);
}

// 11/13/2026 (F7 review 1): a stage counts only while the unit it was cloned from is as it was: sealed (not
// "copying", not dropped by a writer that took the folder) at the generation the stage began from. A stage whose
// swap began always counts (it is finished, never dropped). Anything else is stale: never read or written, and
// discarded at the next stage_begin, gxrepl start or promote.
static bool stage_valid(const gxReplSetConfig &sc, const std::string &root, const std::string &unit)
{
  std::string sd = stage_dir(root, unit);
  struct stat st;
  if(stat((sd + "/.stage").c_str(), &st) != 0) return false;
  // a swap that began counts while its commit block is still in the stage (the renames not done); once it moved,
  // the unit is the new commit and its files are read in place (11/13/2026, F7 review round 2)
  if(stat((sd + "/.swap").c_str(), &st) == 0) return stat((sd + "/" + sc.commit_block).c_str(), &st) == 0;
  std::string seal = read_small(root + "/.gxrepl/units/" + unit_key(unit));
  if(seal.empty() || seal.compare(0, 7, "copying") == 0) return false;
  std::vector<std::string> from = stage_lines(sd + "/.stage", "from ");
  uint64_t g = 0;
  if(from.empty() || !read_unit_gen(sc, unit.empty() ? root : root + "/" + unit, g)) return false;
  return strtoull(from[0].c_str(), 0, 10) == g;
}

// rel (set-relative) as it is read and written now: in its unit's stage while a valid one is open, else in place
// 11/13/2026 (F7 review round 3, 1): only for a unit this process began a staged copy of (MarkGen, as the copy's
// target: "mine"); a source, or an apply for a unit no staged copy of this process is filling, reads and writes
// in place
static std::string staged_path(const gxReplSetConfig &sc, const std::string &root, const std::string &rel,
			       const std::string &full, bool mine)
{
  if(!mine || !stage_valid(sc, root, dir_of(rel))) return full;
  return stage_dir(root, dir_of(rel)) + "/" + base_of(rel);
}

static int remove_tree(const std::string &dir)
{
  DIR *d = opendir(dir.c_str());
  if(!d) return errno == ENOENT ? 0 : -1;
  struct dirent *e;
  int rc = 0;
  while((e = readdir(d)) != 0) {
    std::string n = e->d_name;
    if(n == "." || n == "..") continue;
    std::string p = dir + "/" + n;
    struct stat st;
    if(lstat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) { if(remove_tree(p) != 0) rc = -1; }
    else if(unlink(p.c_str()) != 0) rc = -1;
  }
  closedir(d);
  if(rmdir(dir.c_str()) != 0) rc = -1;
  return rc;
}

// 11/13/2026 (F7 review, LOW): a stage goes away whole or not at all: renamed to a trash name first (stages_of
// lists only "_..." names; RecoverAll removes leftover trash), then removed
static int discard_stage(const std::string &root, const std::string &unit)
{
  std::string sd = stage_dir(root, unit), base = root + "/.gxrepl/stage";
  struct stat st;
  if(lstat(sd.c_str(), &st) != 0) return errno == ENOENT ? 0 : -1;
  static std::atomic<unsigned> seq{0};
  std::string trash = base + "/.trash-" + unit_key(unit) + "-" + std::to_string((long)getpid()) + "-" +
                      std::to_string(seq++);
  if(rename(sd.c_str(), trash.c_str()) != 0) return -1;
  sync_dir(base);
  return remove_tree(trash);
}

// 11/13/2026 (F7 review round 3): a batch apply.jnl still holds goes into its files before a stage is removed
// (the caller holds the apply lock). 0 = done (or nothing held), -1 = it still cannot be applied.
static int recover_journal_locked(const std::string &root)
{
  std::string jname = root + "/.gxrepl/apply.jnl";
  struct stat jb;
  if(stat(jname.c_str(), &jb) != 0 || jb.st_size == 0) return 0;
  unsigned long n = 0;
  if(gxdJournal::Recover(jname.c_str(), &n) != gxDBASE_NO_ERROR) return -1;
  if(truncate(jname.c_str(), 0) != 0) { }
  return 0;
}

// A clone (FICLONE: no data copied) of "from" as "to", its mode and mtime kept; a plain copy where the file
// system cannot clone. 0, -1.
static int clone_file(const std::string &from, const std::string &to)
{
  int in = open(from.c_str(), O_RDONLY | O_CLOEXEC);
  if(in < 0) return -1;
  struct stat st;
  if(fstat(in, &st) != 0) { close(in); return -1; }
  int out = open(to.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, st.st_mode & 07777);
  if(out < 0) { close(in); return -1; }
  int rc = 0;
  if(ioctl(out, FICLONE, in) != 0) {
    std::vector<unsigned char> buf(1 << 20);
    off_t off = 0;
    while(true) {
      ssize_t n = pread(in, buf.data(), buf.size(), off);
      if(n < 0) { rc = -1; break; }
      if(n == 0) break;
      if(pwrite(out, buf.data(), (size_t)n, off) != n) { rc = -1; break; }
      off += n;
    }
  }
  if(rc == 0 && fdatasync(out) != 0) rc = -1;
  struct timespec ts[2] = {st.st_atim, st.st_mtim};
  futimens(out, ts);
  fchmod(out, st.st_mode & 07777);
  close(out);
  close(in);
  return rc;
}

// The unit's replicated files (unit_files) and its commit block, as names in the unit folder
static std::vector<std::string> unit_names(const gxReplSetConfig &sc, const std::string &root, const std::string &unit)
{
  std::vector<std::string> out;
  for(auto &r : unit_files(sc, root, unit)) out.push_back(base_of(r));
  std::string cb = (unit.empty() ? root : root + "/" + unit) + "/" + sc.commit_block;
  struct stat st;
  if(stat(cb.c_str(), &st) == 0 && S_ISREG(st.st_mode)) out.push_back(sc.commit_block);
  return out;
}

// Open (or open again: a new attempt) the unit's stage, cloned from the unit; the odd generation goes in the
// stage's commit block, the unit itself is not touched. 0 = staged, 1 = not staged ("why": copy in place),
// -1 error.
static int stage_swap(const gxReplSetConfig &sc, const std::string &root, const std::string &unit, std::string &err);
static std::map<std::string, std::string> stages_of(const std::string &root);

// 11/13/2026 (F7 review round 2, 7): the set roots that have staged (for the shared free-space count) and whether
// each one's file system clones (FICLONE), probed once
static std::mutex g_stage_mx;
static std::set<std::string> g_stage_roots;
static std::map<std::string, bool> g_can_clone;

static bool can_clone(const std::string &root)
{
  {
    std::lock_guard<std::mutex> g(g_stage_mx);
    g_stage_roots.insert(root);
    auto it = g_can_clone.find(root);
    if(it != g_can_clone.end()) return it->second;
  }
  std::string base = root + "/.gxrepl/stage", a = base + "/.probe-a", b = base + "/.probe-b";
  bool ok = false;
  if(mkdirs(base) == 0) {
    int in = open(a.c_str(), O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if(in >= 0) {
      char page[4096];
      memset(page, 0x5a, sizeof(page));
      if(pwrite(in, page, sizeof(page), 0) == (ssize_t)sizeof(page)) {
	int out = open(b.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
	if(out >= 0) { ok = ioctl(out, FICLONE, in) == 0; close(out); }
      }
      close(in);
    }
    unlink(a.c_str());
    unlink(b.c_str());
  }
#ifdef GXREPL_TEST_HOOKS
  if(getenv("GXREPL_TEST_NO_REFLINK")) ok = false;                  // test builds: a file system with no reflink
#endif
  std::lock_guard<std::mutex> g(g_stage_mx);
  g_can_clone[root] = ok;
  return ok;
}

// What the other open stages on root's file system may still need: each one's unit size (".stage" bytes)
static unsigned long long stages_bytes_on_dev(const std::string &root, const std::string &unit)
{
  struct stat me;
  if(stat(root.c_str(), &me) != 0) return 0;
  std::set<std::string> roots;
  {
    std::lock_guard<std::mutex> g(g_stage_mx);
    roots = g_stage_roots;
  }
  roots.insert(root);
  unsigned long long sum = 0;
  for(auto &r : roots) {
    struct stat st;
    if(stat(r.c_str(), &st) != 0 || st.st_dev != me.st_dev) continue;
    for(auto &e : stages_of(r)) {
      if(r == root && e.first == unit) continue;
      std::vector<std::string> b = stage_lines(e.second + "/.stage", "bytes ");
      if(!b.empty()) sum += strtoull(b[0].c_str(), 0, 10);
    }
  }
  return sum;
}

static int stage_begin(const gxReplSetConfig &sc, const std::string &root, const std::string &unit, uint64_t gen,
		       std::string &why)
{
  std::string live = unit.empty() ? root : root + "/" + unit, sd = stage_dir(root, unit);
  // 11/13/2026 (F7 review round 2, 1): the caller holds the apply lock. A batch the journal still holds (its pages
  // could not be written: an ENOSPC) goes into its files first; a stage is never removed or reused under it. If it
  // still cannot be applied, nothing is changed: this copy fails and the next pass tries again.
  {
    std::string jname = root + "/.gxrepl/apply.jnl";
    struct stat jb;
    if(stat(jname.c_str(), &jb) == 0 && jb.st_size > 0) {
      unsigned long n = 0;
      if(gxdJournal::Recover(jname.c_str(), &n) != gxDBASE_NO_ERROR) {
	why = "the apply journal holds a batch that cannot be applied yet (" + jname + "): the stage is kept, "
	      "tried again next pass";
	return -1;
      }
      if(truncate(jname.c_str(), 0) != 0) { }
    }
  }
  // a swap that began is finished first (it was whole)
  {
    struct stat sw;
    if(stat((sd + "/.swap").c_str(), &sw) == 0) {
      std::string e;
      if(stage_swap(sc, root, unit, e) != 0) { why = e; return -1; }
    }
  }
  // Can this copy be staged? (11/13/2026, F7 review 2 and round 2: every check first; a copy in place never
  // leaves a stage behind, and an old stage's own space is not counted against it)
  std::vector<std::string> names;
  unsigned long long total = 0, others = 0;
  int fb = 0;
  struct stat cst;
  if(stat((live + "/" + sc.commit_block).c_str(), &cst) != 0) fb = 2;   // no copy yet (a first copy, a folder
                                                                       // only the source has): in place, no WARNING
  if(!fb) {
    std::string seal = read_small(root + "/.gxrepl/units/" + unit_key(unit));
    uint64_t g = 0;
    if(seal.empty() || seal.compare(0, 7, "copying") == 0 || !read_unit_gen(sc, live, g) || (g & 1)) {
      why = "the copy is not whole now (no seal, a copy in place in progress, or an odd generation)";
      fb = 1;
    }
  }
  if(!fb) {
    names = unit_names(sc, root, unit);
    for(auto &n : names) {
      struct stat st;
      if(stat((live + "/" + n).c_str(), &st) == 0) total += (unsigned long long)st.st_size;
    }
    if(sc.stage_max_bytes && total > sc.stage_max_bytes) {
      why = "the unit is " + std::to_string(total) + " bytes, over stage_max_bytes " + std::to_string(sc.stage_max_bytes);
      fb = 1;
    }
  }
  // 11/13/2026 (F7 review round 2, 7): a stage is a reflink clone; where the file system cannot clone (probed once
  // per set root), a stage would be a full copy made under the locks: in place instead
  if(!fb && !can_clone(root)) {
    why = "the file system of " + root + " cannot clone files (no reflink): a stage would be a full copy";
    fb = 1;
  }
  if(!fb) {
    struct statvfs vf;
    // a clone shares the unit's blocks, but the new commit may rewrite all of them: room for the whole unit + 256 MB,
    // plus what every other open stage on the same file system may still need (all the sets' stages share it)
    bool known = statvfs(root.c_str(), &vf) == 0;
    unsigned long long avail = known ? (unsigned long long)vf.f_bavail * vf.f_frsize : 0;
    others = stages_bytes_on_dev(root, unit);
#ifdef GXREPL_TEST_HOOKS
    if(const char *fbs = getenv("GXREPL_TEST_FREE_BYTES")) { known = true; avail = strtoull(fbs, 0, 10); }   // test builds
#endif
    if(known && avail < total + others + 256ULL * 1024 * 1024) {
      why = "the disk has " + std::to_string(avail) + " bytes free, the unit needs " + std::to_string(total) +
            " + 256 MB" + (others ? " (+ " + std::to_string(others) + " for the other open stages there)" : std::string(""));
      fb = 1;
    }
  }
  if(fb) {
    if(discard_stage(root, unit) != 0) { why = "cannot clear " + sd; return -1; }
    return fb;
  }
  // 11/13/2026 (F7 review round 2, 7): a retry of the same commit reuses its stage (valid, no swap begun, begun
  // for this generation): the pages it has are not sent again
  if(stage_valid(sc, root, unit)) {
    std::vector<std::string> g0 = stage_lines(sd + "/.stage", "gen ");
    if(!g0.empty() && strtoull(g0[0].c_str(), 0, 10) == gen) {
      glog("set %s: %s: a retry of the same commit: its stage is reused", sc.name.c_str(), unit.empty() ? "." : unit.c_str());
      return 0;
    }
  }
  if(discard_stage(root, unit) != 0) { why = "cannot clear " + sd; return -1; }
  if(mkdirs(sd) != 0) { why = "cannot make " + sd; return -1; }
  std::string info = "unit " + (unit.empty() ? std::string(".") : unit) + "\nsince " + std::to_string((long long)time(0)) + "\n";
  uint64_t was = 0;
  {
    int fd = open((live + "/" + sc.commit_block).c_str(), O_RDONLY);
    if(fd >= 0) { if(pread(fd, &was, 8, (off_t)sc.commit_gen_offset) != 8) was = 0; close(fd); }
  }
  info += "from " + std::to_string((unsigned long long)was) + "\n";
  info += "gen " + std::to_string((unsigned long long)gen) + "\n";
  info += "bytes " + std::to_string(total) + "\n";
  for(auto &n : names) {
    if(clone_file(live + "/" + n, sd + "/" + n) != 0) { discard_stage(root, unit); why = "cannot clone " + n; return -1; }
    info += "file " + n + "\n";
  }
  {
    std::string cb = sd + "/" + sc.commit_block;
    int fd = open(cb.c_str(), O_WRONLY);
    if(fd >= 0) {
      ssize_t n = pwrite(fd, &gen, sizeof(gen), (off_t)sc.commit_gen_offset);
      int ok = n == (ssize_t)sizeof(gen) && fdatasync(fd) == 0;
      close(fd);
      if(!ok) { discard_stage(root, unit); why = "cannot mark the staged " + sc.commit_block; return -1; }
    }
  }
  if(write_atomic(sd + "/.stage", info) != 0) { discard_stage(root, unit); why = "cannot write " + sd + "/.stage"; return -1; }
  sync_dir(sd);
  sync_dir(root + "/.gxrepl/stage");
  return 0;
}

static std::vector<std::string> stage_lines(const std::string &path, const char *prefix)
{
  std::vector<std::string> out;
  FILE *f = fopen(path.c_str(), "r");
  if(!f) return out;
  char b[4096];
  size_t pl = strlen(prefix);
  while(fgets(b, sizeof(b), f)) {
    std::string l = b;
    while(!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
    if(l.compare(0, pl, prefix) == 0) out.push_back(l.substr(pl));
  }
  fclose(f);
  return out;
}

// The stage's files now (no dot files)
static std::vector<std::string> stage_files(const std::string &sd)
{
  std::vector<std::string> out;
  DIR *d = opendir(sd.c_str());
  if(!d) return out;
  struct dirent *e;
  while((e = readdir(d)) != 0) {
    std::string n = e->d_name;
    if(n.empty() || n[0] == '.') continue;
    out.push_back(n);
  }
  closedir(d);
  std::sort(out.begin(), out.end());
  return out;
}

// Swap a whole stage into its unit, or finish a swap that had begun (".swap" there): the files renamed into the
// unit (the commit block last), files the stage dropped (removed by the source meanwhile) removed from the unit,
// then the new seal, then the stage removed. Every step can be done again. The caller holds the apply lock.
static int stage_swap(const gxReplSetConfig &sc, const std::string &root, const std::string &unit, std::string &err)
{
  std::string sd = stage_dir(root, unit), live = unit.empty() ? root : root + "/" + unit;
  std::vector<std::string> list;
  struct stat st;
  // 11/13/2026 (F7 review round 2): a swap whose commit block already moved (a stop after the renames, before the
  // seal): the unit is the new commit. Its new seal is written only while the unit is still as the swap left it
  // (its old seal there, not dropped by a writer that took the folder, and its generation the new seal's); then
  // the stage goes. Never a seal over a folder a writer owns.
  if(stat((sd + "/.swap").c_str(), &st) == 0 && stat((sd + "/" + sc.commit_block).c_str(), &st) != 0) {
    std::string seal = read_small(sd + "/.seal"), old = read_small(root + "/.gxrepl/units/" + unit_key(unit));
    uint64_t g = 0;
    if(seal.compare(0, 4, "gen ") == 0 && !old.empty() && old.compare(0, 7, "copying") != 0 &&
       read_unit_gen(sc, live, g) && !(g & 1) && g == strtoull(seal.c_str() + 4, 0, 10)) {
      if(mkdirs(root + "/.gxrepl/units") != 0 || write_atomic(root + "/.gxrepl/units/" + unit_key(unit), seal) != 0) {
	err = "cannot write the seal of " + unit; return -1;
      }
    }
    if(discard_stage(root, unit) != 0) { err = "cannot remove " + sd; return -1; }
    return 0;
  }
  if(stat((sd + "/.swap").c_str(), &st) == 0) list = stage_lines(sd + "/.swap", "file ");
  else {
    if(stat((sd + "/.seal").c_str(), &st) != 0) { err = "the stage of " + unit + " has no seal yet"; return -1; }
    std::string body;
    list = stage_files(sd);
    for(auto &n : list) body += "file " + n + "\n";
    if(write_atomic(sd + "/.swap", body) != 0) { err = "cannot write " + sd + "/.swap"; return -1; }
    sync_dir(sd);
  }
  // test hook (built only with -DGXREPL_TEST_HOOKS, the test builds): GXREPL_TEST_SWAP_CRASH=N ends the process
  // after N renames (the swap is finished at the next start)
  int crash_after = -1;
#ifdef GXREPL_TEST_HOOKS
  if(const char *c = getenv("GXREPL_TEST_SWAP_CRASH")) crash_after = atoi(c);
#endif
  // A reader of the copy that takes no lock checks the commit block: make the live one odd before any data file
  // moves, so new files beside the old even commit block are never read as a whole commit (the stage's even
  // commit block moves in last and ends the window).
  if(stat((sd + "/" + sc.commit_block).c_str(), &st) == 0) {
    int fd = open((live + "/" + sc.commit_block).c_str(), O_RDWR);
    if(fd >= 0) {
      uint64_t g = 0;
      if(pread(fd, &g, 8, (off_t)sc.commit_gen_offset) == 8 && (g & 1) == 0) {
	g += 1;
	if(pwrite(fd, &g, 8, (off_t)sc.commit_gen_offset) != 8 || fdatasync(fd) != 0) {
	  close(fd); err = "cannot mark " + live + "/" + sc.commit_block; return -1;
	}
      }
      close(fd);
    }
  }
  if(mkdirs(live) != 0) { err = "cannot make " + live; return -1; }
  int renamed = 0;
  std::set<std::string> keep(list.begin(), list.end());
  for(int pass = 0; pass < 2; pass++) {               // data files, then the commit block
    for(auto &n : list) {
      if((n == sc.commit_block) != (pass == 1)) continue;
      std::string from = sd + "/" + n;
      if(stat(from.c_str(), &st) != 0) continue;       // moved already (a swap finished again)
      if(rename(from.c_str(), (live + "/" + n).c_str()) != 0) { err = "cannot move " + from; return -1; }
      if(++renamed == crash_after) _exit(9);
#ifdef GXREPL_TEST_HOOKS
      if(pass == 1 && getenv("GXREPL_TEST_SWAP_CRASH_AFTER_CB")) _exit(9);   // test builds: renames done, no seal yet
#endif
    }
    if(pass == 0) {
      for(auto &n : stage_lines(sd + "/.stage", "file "))
	if(!keep.count(n)) unlink((live + "/" + n).c_str());
      sync_dir(live);
    }
  }
  sync_dir(live);
  std::string seal;
  {
    FILE *f = fopen((sd + "/.seal").c_str(), "rb");
    if(f) { char b[65536]; size_t n; while((n = fread(b, 1, sizeof(b), f)) > 0) seal.append(b, n); fclose(f); }
  }
  if(!seal.empty()) {
    if(mkdirs(root + "/.gxrepl/units") != 0 || write_atomic(root + "/.gxrepl/units/" + unit_key(unit), seal) != 0) {
      err = "cannot write the seal of " + unit; return -1;
    }
  }
  if(discard_stage(root, unit) != 0) { err = "cannot remove " + sd; return -1; }
  return 0;
}

// The stages of a set: unit -> its stage folder
static std::map<std::string, std::string> stages_of(const std::string &root)
{
  std::map<std::string, std::string> out;
  std::string base = root + "/.gxrepl/stage";
  DIR *d = opendir(base.c_str());
  if(!d) return out;
  struct dirent *e;
  while((e = readdir(d)) != 0) {
    std::string n = e->d_name;
    if(n.empty() || n[0] != '_') continue;
    std::vector<std::string> u = stage_lines(base + "/" + n + "/.stage", "unit ");
    std::string unit;
    if(!u.empty()) unit = u[0] == "." ? std::string() : u[0];
    else if(n != "_root") {                    // no .stage (a crash before it was written): unit_key decoded
      unit = n.substr(1);
      for(auto &c : unit) if(c == '#') c = '/';
    }
    out[unit] = base + "/" + n;
  }
  closedir(d);
  return out;
}

// "": sealed and the same; else why not
// 11/14/2026: the hashing half of a "gen G" seal's check: the unit's files against the seal text "have"
static std::string unit_seal_hash_check(const gxReplSetConfig &sc, const std::string &rel, uint64_t gen,
					const std::string &have)
{
  bool ok = true;
  std::string now = unit_manifest(sc, sc.root, rel, gen, ok);
  if(!ok) return "a file cannot be read";
  if(now != have) return "its files differ from the seal of generation " + std::to_string((unsigned long long)gen);
  return "";
}

// "defer" (11/14/2026, promote-check under the apply lock): a "gen G" seal at the right generation is not hashed
// here: its text goes in *defer (cleared otherwise) and "" is returned; the caller runs unit_seal_hash_check once
// it let go of the lock
static std::string unit_seal_check(const gxReplSetConfig &sc, const std::string &rel, uint64_t gen,
				   std::string *defer = 0)
{
  if(defer) defer->clear();
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
  if(defer) { *defer = have; return ""; }
  return unit_seal_hash_check(sc, rel, gen, have);
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
    std::string re;
    gxdShipLogReader *rd = s->reader(n, re);
    if(!rd) continue;
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

int gxReplLocal::ReadLogOf(const std::string &set, const std::string &stream, const std::string &log_id,
			   unsigned long long from, size_t max_bytes, std::vector<unsigned char> &out,
			   unsigned long long &next, int &status, std::string &err)
{
  out.clear();
  status = 0;
  next = from;
  SetState_ *s = set_state(set, err);
  if(!s) return -1;
  std::lock_guard<std::mutex> g(s->m);              // the check and the read on one reader, under one lock
  gxdShipLogReader *rd = s->reader(stream, err);
  if(!rd) return -1;
  if(!log_id.empty()) {
    gxdShipLogReader::Info info;
    if(rd->GetInfo(info) != 0) { err = rd->Error(); return -1; }
    if(info.log_id != log_id) { status = 1; return 0; }   // another log now: the position is gone
  }
  unsigned long count = 0;
  int rc = rd->Read(from, max_bytes, out, next, count);
  if(rc < 0) { err = rd->Error(); return -1; }
  if(rc == 1) status = 1;
  if(rc == 2) status = 2;
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
  gxdShipLogReader *rd = s->reader(stream, err);
  if(!rd) return -1;
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
    gxdShipLogReader *rd = s->reader(cover->name, err);
    if(!rd) return -1;
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
#ifdef GXREPL_TEST_HOOKS
    // 11/14/2026: test builds (the drain-verify tests):
    // GXREPL_TEST_DIFF_STATUS2=1: the first behind-the-log request answers "apply the log first" (status 2), so the
    // client's sync_file() drains the records (drain()'s own part of the fix, alone);
    // GXREPL_TEST_DIFF_HOLD=F: the first request of a covered data file, after the position and records were read
    // (the journal read lock held), writes F.held and waits (20 s at most) for F (pieces against a racing commit)
    if(cs.name == u.stream && cs.log_id == u.log_id && cs.lsn < u.lsn && getenv("GXREPL_TEST_DIFF_STATUS2")) {
      static std::atomic<int> once(0);
      if(once.exchange(1) == 0) { u.records.clear(); res.status = 2; return 0; }
    }
    if(const char *hold = getenv("GXREPL_TEST_DIFF_HOLD")) {
      static std::atomic<int> once_h(0);
      if(base_of(rq.path) == "data.gxd" && once_h.exchange(1) == 0) {
	write_atomic(std::string(hold) + ".held", "held\n");
	for(int w = 0; w < 200 && access(hold, F_OK) != 0; w++) usleep(100000);
      }
    }
#endif
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

static std::string derived_dir(const gxReplSetConfig &sc);   // 11/15/2026: the stream holds (see hold_path)
static std::string hold_path(const gxReplSetConfig &sc, const std::string &stream);
static bool read_hold(const gxReplSetConfig &sc, const std::string &stream, std::string &log_id, unsigned long long &lsn);

int gxReplLocal::SetState(const std::string &set, const gxReplStreamState &st,
			  std::string &err)
{
  SetState_ *s = set_state(set, err);
  if(!s) return -1;
  std::lock_guard<std::mutex> g(s->m);
  WriterHold wh;                             // 11/10/2026: held until this change is done
  if(hold_writer_locks(s->sc, wh, err)) return -1;
  // 11/15/2026 (the recovery gap, review 2): the client sends the copy's own position after a stream's verify is
  // done (an older copy just writes the same position again). Nothing to write then; the stream's hold goes when the
  // verify reached the hold's record (or verified another log, whole).
  // 11/16/2026 (beta4 review H1): ... and only when the client says how many files the verify compared and that is
  // more than none. The same position alone is no proof: a verify that compared no file (a new log that lists none
  // yet) sends it, and so does an older client (the end of a 0-file verify, its log end, is the copy's position
  // too). Such a hold stays until a verify that compared files, and the derived status shows it.
  {
    std::map<std::string, gxReplStreamState> cur;
    std::string e2;
    read_states(s, cur, e2);
    auto c = cur.find(st.name);
    if(c != cur.end() && c->second.log_id == st.log_id && c->second.lsn == st.lsn && st.log_id != "resync") {
      std::string hid;
      unsigned long long hl = 0;
      bool held = read_hold(s->sc, st.name, hid, hl);
      if(held && st.verified_files <= 0) {
	static std::set<std::string> said;     // once per stream and hold
	static std::mutex said_mx;
	std::lock_guard<std::mutex> sg(said_mx);
	if(said.insert(s->sc.name + "|" + st.name + "|" + hid + "|" + std::to_string(hl)).second)
	  glog("set %s: stream %s: %s: its hold (an open or gap record at %s) stays until a verify compares its files",
	       s->sc.name.c_str(), st.name.c_str(), st.verified_files < 0 ? "the same position without a verify's count "
	       "(an older client)" : "a verify that compared no file", hl == ULLONG_MAX ? "?" : std::to_string(hl).c_str());
      }
      if(held && st.verified_files > 0 && hl != ULLONG_MAX && (hid != st.log_id || st.lsn >= hl)) {
	int lk = open((s->root + "/.gxrepl/apply.jnl").c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
	if(lk >= 0 && lock_apply_bounded(lk) == 0) {          // under the apply lock, as the hold is written
	  if(unlink(hold_path(s->sc, st.name).c_str()) == 0) {
	    sync_dir(derived_dir(s->sc));
	    glog("set %s: stream %s verified past its open or gap record at %llu: object by object again",
		 s->sc.name.c_str(), st.name.c_str(), hl);
	  }
	  flock(lk, LOCK_UN);
	}
	if(lk >= 0) close(lk);
      }
      return 0;
    }
  }
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
  full = staged_path(s->sc, s->root, path, full, s->staged.count(dir_of(path)) > 0);   // 11/12/2026 (F7): a staged unit's files as the copy has them
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
  {
    std::lock_guard<std::mutex> g(s->m);          // s->staged (11/13/2026)
    full = staged_path(s->sc, s->root, path, full, s->staged.count(dir_of(path)) > 0);   // F7: its stage, if mine
  }
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
  // 11/12/2026 (F7): the copy goes into a stage; the unit stays whole and sealed until the stage is swapped in.
  // In place (as before) for a set fed by a ship log (its records apply in the unit, with their seals), a unit
  // over stage_max_bytes or a disk short of room: a WARNING once per unit and reason.
  // 11/13/2026 (F7 review 3): the stage is begun (or the unit marked in place) under the apply lock, so gxrepl
  // promote never removes a stage while it is cloned, and an apply's choice of stage or place stays true
  struct ApplyLock { int fd = -1; ~ApplyLock() { if(fd >= 0) { flock(fd, LOCK_UN); close(fd); } } } alk;
  if(!s->sc.commit_block.empty()) {
    std::string jdir = s->root + "/.gxrepl", jname = jdir + "/apply.jnl";
    if(mkdirs(jdir) != 0) { err = "cannot make " + jdir; return -1; }
    alk.fd = open(jname.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if(alk.fd < 0) { err = "cannot lock " + jname; return -1; }
    if(int le = lock_apply_bounded(alk.fd)) {
      close(alk.fd); alk.fd = -1;
      err = le == EWOULDBLOCK ? "busy: " + jname + " is held (a derived index rebuild or a reader of the copy); tried "
                                "again next pass" : "cannot lock " + jname;
      return -1;
    }
  }
  s->staged.erase(dir_of(path));
  if(s->sc.staged_fill) {
    std::map<std::string, gxReplStreamState> streams;
    std::string se;
    read_states(s, streams, se);
    std::string why;
    int rc = streams.empty() ? stage_begin(s->sc, s->root, dir_of(path), gen, why) : 1;
    if(rc == 0) { s->staged.insert(dir_of(path)); return 0; }
    if(rc < 0) { err = why; return -1; }
    if(rc == 2) why = "";
    else if(why.empty()) why = "the set is fed by a ship log";
    static std::set<std::string> warned;
    static std::mutex warned_mx;              // 11/13/2026 (F7 review round 2, 6): the sets' threads share it
    std::string key = s->sc.name + "|" + dir_of(path) + "|" + why.substr(0, 20);
    bool first;
    {
      std::lock_guard<std::mutex> wg(warned_mx);
      first = !why.empty() && warned.insert(key).second;
    }
    if(first)
      glog("WARNING set %s: %s: copied in place, not staged (%s)", s->sc.name.c_str(),
	   dir_of(path).empty() ? "." : dir_of(path).c_str(), why.c_str());
  }
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

// 11/12/2026 (F7): the seal of a staged unit, from its stage's files (unit_manifest's filter and order). 0, -1.
static int stage_manifest(const gxReplSetConfig &sc, const std::string &sd, const std::string &folder, uint64_t gen,
			  std::string &m, std::string &err)
{
  m = "gen " + std::to_string((unsigned long long)gen) + "\n";
  for(auto &n : stage_files(sd)) {
    std::string r = folder.empty() ? n : folder + "/" + n;
    if(n == sc.commit_block || !is_db_file(sc, r) || excluded(sc, r)) continue;
    unsigned long long size = 0; uint64_t h = 0;
    if(file_hash(sd + "/" + n, size, h) != 0) { err = "cannot read the staged " + r; return -1; }
    char hx[17]; snprintf(hx, sizeof(hx), "%016llx", (unsigned long long)h);
    m += r + "\t" + std::to_string(size) + "\t" + hx + "\n";
  }
  return 0;
}

// 11/13/2026 (F7 review, LOW): a stage whose last commit arrived whole (its commit block even, at its seal's
// generation, its files those the seal names) but whose swap had not begun (a stop between the two)
static bool stage_complete(const gxReplSetConfig &sc, const std::string &sd, const std::string &folder)
{
  std::string seal = read_small(sd + "/.seal");
  if(seal.compare(0, 4, "gen ") != 0) return false;
  uint64_t want = strtoull(seal.c_str() + 4, 0, 10), g = 0;
  if(!read_unit_gen(sc, sd, g) || (g & 1) || g != want) return false;
  std::string m, e;
  return stage_manifest(sc, sd, folder, g, m, e) == 0 && m == seal;
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
  // 11/13/2026 (F7 review 3): a staged copy whose stage went away (gxrepl promote) or went stale stops here
  if(s->staged.count(folder) && !stage_valid(s->sc, s->root, folder)) {
    s->staged.erase(folder);
    err = "the stage of " + (folder.empty() ? std::string(".") : folder) + " was discarded (gxrepl promote) or went "
          "stale: this copy stops; the next pass copies the unit again";
    return -1;
  }
  if(s->staged.count(folder) && stage_valid(s->sc, s->root, folder)) {
    // 11/12/2026 (F7): the new seal is the stage's (its files' hashes); it reaches the unit with the swap
    std::string sd = stage_dir(s->root, folder), m;
    if(stage_manifest(s->sc, sd, folder, gen, m, err) != 0) return -1;
    if(write_atomic(sd + "/.seal", m) != 0) { err = "cannot write the staged seal of " + folder; return -1; }
    return 0;
  }
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

// 11/14/2026: stress measurements: GXREPL_STATS_FILE gets one line per apply lock hold and per derived rebuild,
// "<unix ms> <kind> <set> <held ms> <detail>" (off unless the variable is set)
static void stats_line(const std::string &kind, const std::string &set, double ms, const std::string &detail)
{
  static const char *path = getenv("GXREPL_STATS_FILE");
  if(!path || !*path) return;
  static std::mutex m;
  std::lock_guard<std::mutex> g(m);
  FILE *f = fopen(path, "a");
  if(!f) return;
  struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
  fprintf(f, "%lld %s %s %.3f %s\n", (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000, kind.c_str(), set.c_str(), ms,
	  detail.c_str());
  fclose(f);
}

static double ms_since(const struct timespec &t0)
{
  struct timespec t1; clock_gettime(CLOCK_MONOTONIC, &t1);
  return (t1.tv_sec - t0.tv_sec) * 1000.0 + (t1.tv_nsec - t0.tv_nsec) / 1e6;
}

static void drop_stamps(const gxReplSetConfig &sc, const std::string &rel, const std::string &why, bool log_it = true,
			bool all = false);   // 11/14/2026 (review 6)
static void derived_folders(const gxReplSetConfig &sc, const std::string &rel, std::vector<std::string> &out);

int gxReplLocal::apply_batch(SetState_ *s,
  std::map<std::string, std::pair<unsigned long long,
  std::map<unsigned long long, const unsigned char *> > > &pages,
  std::string &err, const std::function<int(std::string &)> &in_lock,
  const std::function<int(std::string &)> &pre_lock)
{
  // 11/15/2026 (the recovery gap): a batch with no pages and a pre-lock step (records with no pages, such as a
  // writer's lone open record, on an incremental set) still takes the apply lock: the step drops the stamps of the
  // stream's folders there, and in_lock writes the stream's position under the same lock
  // review 6 (11/15/2026): and with an in_lock step (the stream's position), so a lone open record's position is
  // written under the lock in rebuild mode too
  if(pages.empty() && !pre_lock && !in_lock) return 0;
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
  // 11/14/2026: stress measurements (GXREPL_STATS_FILE): the apply lock hold, written as the lock goes
  struct timespec held_t0; clock_gettime(CLOCK_MONOTONIC, &held_t0);
  size_t held_pages = 0;
  struct HeldStats { const std::string &set; struct timespec &t0; size_t &pages; double &peak;
    ~HeldStats() { stats_line("apply", set, ms_since(t0), std::to_string(pages) + " " + std::to_string(peak)); } }
    hst{s->sc.name, held_t0, held_pages, derived_peak_mb};   // (peak: the most of any batch so far)
  if(pre_lock && pre_lock(err) != 0) return -1;   // 11/13/2026 (F7 review 3): checked under the lock
  // 11/14/2026: the file list is made after pre_lock, which may add pages (the incremental derived apply)
  // 11/10/2026: commit blocks after the files they describe, the unit seals
  // last (one transaction; the order is the order the pages reach the files)
  // 11/14/2026 (review 6): data files, then the derived indexes, then their stamps (<index>.stamp, derived.stamp,
  // the derived state), then the commit blocks, then the seals
  const std::string units_dir = s->root + "/.gxrepl/units/", dstate_dir = s->root + "/.gxrepl/derived/";
  auto kind_of = [&](const std::string &path) -> int {
    if(path.compare(0, units_dir.size(), units_dir) == 0) return 4;
    if(path.compare(0, dstate_dir.size(), dstate_dir) == 0) return 2;
    std::string rel = path.compare(0, s->root.size() + 1, s->root + "/") == 0 ? path.substr(s->root.size() + 1) : path;
    if(!s->sc.commit_block.empty() && base_of(rel) == s->sc.commit_block) return 3;
    if(gxdDerivedStamp::IsStamp(base_of(rel)) || base_of(rel) == "derived.stamp") return 2;
    if(is_derived(s->sc, rel)) return 1;
    return 0;
  };
  // 11/14/2026 (review 6): one transaction holds at most part_files files (the journal's limit is 1024). A batch
  // past it is applied in parts, which are not one transaction: a crash between them may leave data pages without
  // the index pages made from them. So such a batch carries no index pages: they are dropped, with the folders'
  // stamps, derived.stamp and derived state (removed before any page is written), and the derived pass rebuilds them.
  size_t part_files = 1000;
#ifdef GXREPL_TEST_HOOKS
  if(const char *pf = getenv("GXREPL_TEST_APPLY_PART_FILES")) part_files = (size_t)strtoull(pf, 0, 10);   // tests only
#endif
  if(pages.size() > part_files) {
    std::set<std::string> folders;
    for(auto it = pages.begin(); it != pages.end(); ) {
      int k = kind_of(it->first);
      if(k == 1 || k == 2) {
	std::string rel = it->first.compare(0, s->root.size() + 1, s->root + "/") == 0 ? it->first.substr(s->root.size() + 1) : it->first;
	if(it->first.compare(0, dstate_dir.size(), dstate_dir) != 0) folders.insert(dir_of(rel));
	it = pages.erase(it);
      }
      else ++it;
    }
    for(auto &u : folders) drop_stamps(s->sc, u, "the batch has more than " + std::to_string(part_files) +
				       " files (applied in parts: no index pages in it)", true, true);
  }
  std::vector<gxdApplyFile> af;
  for(int pass = 0; pass < 5; pass++) {
    for(auto &e : pages) {
      if(kind_of(e.first) != pass) continue;
      std::string dir = e.first.substr(0, e.first.find_last_of('/'));
      if(mkdirs(dir) != 0) { err = "cannot make " + dir; return -1; }
      gxdApplyFile f;
      f.path = e.first;
      f.size = e.second.first;
      f.pages = e.second.second;
      af.push_back(f);
    }
  }
  for(auto &f : af) held_pages += f.pages.size();
  // At most 1024 files a transaction
  for(size_t i = 0; i < af.size(); i += part_files) {
    std::vector<gxdApplyFile> part(af.begin() + i, af.begin() + std::min(af.size(), i + part_files));
    unsigned long long n = 0;
#if defined(GXREPL_TEST_HOOKS)
    test_crash_in_apply();
#endif
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
  if(in_lock && in_lock(err) != 0) return -1;     // still under the apply lock
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
// - a unit whose commit block came (allow()): "log G L S C" (generation, lsn,
//   stream, and C, 11/14/2026: the last record that changed this unit, here L);
// - every other unit sealed "log G L S C" by this stream: L moves to "lsn" (an
//   open or gap record changes no data), so a seal's L always equals the
//   stream's applied lsn and promote-check can compare them; C stays (11/14/2026,
//   seal per folder: the unit's derived indexes, stamped at C, stay current
//   while the stream's other folders change). Readers before C ignore it; a seal
//   without C reads as C = L.
// fresh (11/14/2026, seal per folder review 2): units not in step with the stream (a first fill, a new log id:
// its lsn numbering starts again): their last change is this record, never one kept from an older numbering
static int add_unit_seals(const gxReplSetConfig &sc, const std::string &root, PageBatch &batch,
			  unsigned long long lsn, const std::string &stream,
			  const std::map<std::string, bool> &unit_cb,
			  std::deque<std::string> &seals, std::string &err,
			  const std::function<bool(const std::string &)> &allow,
			  const std::set<std::string> *fresh = 0)
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
    // 11/14/2026 (seal per folder): the 4th field is the last record that changed THIS folder (here: this batch),
    // unless the batch only re-sent the folder as it was sealed (a verify after a writer's open or a gap: the same
    // commit block generation, sealed by this stream, and no page of the folder's other files): then its last
    // change stays, and so do its derived indexes
    unsigned long long changed_at = lsn;
    {
      const std::string unit = dir_of(rel);
      FILE *sf = fopen((root + "/.gxrepl/units/" + unit_key(unit)).c_str(), "rb");
      char sname[256] = {0}; unsigned long long og = 0, ol = 0, oc = 0;
      int got = sf ? fscanf(sf, "log %llu %llu %255s %llu", &og, &ol, sname, &oc) : 0;
      if(sf) fclose(sf);
      unsigned long long kept = got >= 4 ? oc : ol;
      if(got >= 3 && og == (unsigned long long)g && stream == sname && ol <= lsn && kept <= lsn &&
	 !(fresh && fresh->count(unit))) {
	bool pages = false;
	for(auto &o : batch) {
	  if(o.first.compare(0, root.size() + 1, root + "/") != 0) continue;
	  std::string orel = o.first.substr(root.size() + 1);
	  if(dir_of(orel) != unit || base_of(orel) == sc.commit_block || is_derived(sc, orel)) continue;
	  if(!o.second.second.empty()) { pages = true; break; }
	}
	if(!pages) changed_at = kept;
      }
    }
    add.push_back(std::make_pair(dir_of(rel), "log " + std::to_string((unsigned long long)g) + " " +
				 std::to_string(lsn) + " " + stream + " " + std::to_string(changed_at) + "\n"));
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
      char sname[256] = {0}; unsigned long long g = 0, l = 0, c = 0;
      int got = fscanf(f, "log %llu %llu %255s %llu", &g, &l, sname, &c);
      fclose(f);
      if(got < 3 || stream != sname || l == lsn) continue;
      if(got < 4) c = l;          // 11/14/2026: a seal from before the 4th field: changed at most at its lsn
      if(c > lsn) c = lsn;        // review 2: never past the stream's position (another numbering)
      std::string unit = n == "_root" ? std::string() : n.substr(1);
      for(auto &ch : unit) if(ch == '#') ch = '/';
      if(done.count(unit)) continue;
      // 11/14/2026 (seal per folder): the stream's position moves on, the folder's own last change stays
      add.push_back(std::make_pair(unit, "log " + std::to_string(g) + " " + std::to_string(lsn) + " " + stream + " " +
				   std::to_string(c) + "\n"));
    }
    if(d) closedir(d);
  }
  if(!add.empty() && mkdirs(root + "/.gxrepl/units") != 0) { err = "cannot make the seals folder"; return -1; }
  for(auto &a2 : add) put_seal(batch, root, a2.first, a2.second, seals);
  return 0;
}

// ---------------- Incremental derived apply (11/14/2026, 7.4) ----------------
// A set with derived_apply = incremental keeps each derived index of a folder object by object: the objects records
// (gxdShipObjects) of a batch say which objects of the folder's derived_source file were added, changed and removed;
// the extractor (gxrextract.h) removes the old bytes and inserts the new ones inside a staged scope (gxdstage.h:
// nothing it writes reaches a file), and its changed pages, each index's <index>.stamp, the folder's derived.stamp
// and derived state go into the batch with the data pages, the seal and (right after, F6) the position: one atomic
// apply. Anything the copy cannot do exactly is "rebuild needed": the index stamps go, the derived pass rebuilds the
// folder (derived_rebuild) and writes fresh stamps.
static unsigned long long unit_log_lsn(const gxReplSetConfig &sc, const std::string &rel, unsigned long long *gen);
static unsigned long long unit_changed_lsn(const gxReplSetConfig &sc, const std::string &rel);   // 11/14/2026
static bool is_derived_index(const gxReplSetConfig &sc, const std::string &rel);
static std::string derived_state_path(const gxReplSetConfig &sc, const std::string &rel);

struct gxrExtractor_ {
  void *so = 0;
  const gxrExtractorOps *ops = 0;
  std::string err;
};

// 11/14/2026: built-in extractors, linked into the program (gxrRegisterExtractor()), found by name first; only a
// path ending in ".so" is loaded with dlopen (the program must export DataReel's symbols to it: ENABLE_EXPORTS)
static std::mutex &extractor_mutex() { static std::mutex m; return m; }
static std::map<std::string, const gxrExtractorOps *> &extractor_registry()
{
  // 11/14/2026 (7.4 merge): the library's own extractor, "gxvmember" (gxvmember.h: vector indexes derived
  // from the records' vector members), is always there
  static std::map<std::string, const gxrExtractorOps *> r = { { "gxvmember", gxvMemberExtractor() } };
  return r;
}

int gxrRegisterExtractor(const char *name, const gxrExtractorOps *ops)
{
  if(!name || !*name || !ops || ops->abi_version != GXR_EXTRACTOR_ABI) return -1;
  std::lock_guard<std::mutex> g(extractor_mutex());
  extractor_registry()[name] = ops;
  return 0;
}

static const gxrExtractorOps *load_extractor(const std::string &spec, std::string &err)
{
  static std::map<std::string, gxrExtractor_> loaded;
  std::lock_guard<std::mutex> g(extractor_mutex());
  auto r = extractor_registry().find(spec);
  if(r != extractor_registry().end()) { err.clear(); return r->second; }
  auto it = loaded.find(spec);
  if(it != loaded.end()) { err = it->second.err; return it->second.ops; }
  gxrExtractor_ &x = loaded[spec];
  bool so = spec.size() > 3 && spec.compare(spec.size() - 3, 3, ".so") == 0;
  if(spec.empty()) x.err = "no derived_extractor";
  else if(!so) x.err = "no built-in extractor named " + spec + " (a shared library path must end in .so)";
  else {
    x.so = dlopen(spec.c_str(), RTLD_NOW | RTLD_LOCAL);
    if(!x.so) x.err = std::string("cannot load ") + spec + ": " + dlerror();
    else {
      gxrExtractorEntry e = (gxrExtractorEntry)dlsym(x.so, "gxr_extractor");
      const gxrExtractorOps *o = e ? e() : 0;
      if(!o) x.err = spec + " has no gxr_extractor()";
      else if(o->abi_version != GXR_EXTRACTOR_ABI) x.err = spec + ": extractor ABI " + std::to_string(o->abi_version) +
						     ", gxrepl needs " + std::to_string(GXR_EXTRACTOR_ABI);
      else if(!o->Open || !o->Remove || !o->Insert || !o->Flush || !o->Close || !o->Count) x.err = spec + ": missing calls";
      else x.ops = o;
    }
  }
  err = x.err;
  return x.ops;
}

static std::string derived_dir(const gxReplSetConfig &sc) { return sc.root + "/.gxrepl/derived"; }
// 11/15/2026 (the recovery gap, review 2): a stream's hold. An open or a gap record of an incremental set's stream
// writes "LOG_ID LSN" here; while it is there, every batch of the stream sends the folders it changes back to a
// rebuild (a rebuild between a lone open and the stream's verify may come from data that still lacks the recovered
// pages: the commits after it would then go on object by object from that rebuild). The stream's verify, once done,
// clears it (SetState with the copy's own position, see gxReplLocal::SetState).
static std::string hold_path(const gxReplSetConfig &sc, const std::string &stream)
{
  std::string k = stream;
  for(auto &c : k) if(c == '/') c = '#';
  return derived_dir(sc) + "/hold_" + k;
}
static bool read_hold(const gxReplSetConfig &sc, const std::string &stream, std::string &log_id, unsigned long long &lsn)
{
  FILE *f = fopen(hold_path(sc, stream).c_str(), "r");
  if(!f) return false;
  char id[128] = {0};
  if(fscanf(f, "%127s %llu", id, &lsn) != 2) { id[0] = 0; lsn = ULLONG_MAX; }   // unreadable: held until a verify
  fclose(f);
  log_id = id;
  return true;
}
// 11/15/2026 (review 5): the folders whose seals name this stream ("log G L STREAM C"), from the seals folder (no
// walk of the set's tree under the apply lock)
static std::vector<std::string> stream_folders(const gxReplSetConfig &sc, const std::string &stream)
{
  std::vector<std::string> out;
  std::string dir = sc.root + "/.gxrepl/units";
  DIR *d = opendir(dir.c_str());
  struct dirent *e;
  while(d && (e = readdir(d)) != 0) {
    std::string n = e->d_name;
    if(n.size() < 2 || n[0] != '_' || n.find(".gxrtmp") != std::string::npos) continue;
    FILE *f = fopen((dir + "/" + n).c_str(), "r");
    if(!f) continue;
    char b[512] = {0};
    size_t got = fread(b, 1, sizeof(b) - 1, f);
    fclose(f);
    b[got] = 0;
    char sname[256] = {0};
    unsigned long long g = 0, l = 0;
    if(sscanf(b, "log %llu %llu %255s", &g, &l, sname) != 3 || stream != sname) continue;
    std::string rel = n == "_root" ? std::string() : n.substr(1);
    for(auto &c : rel) if(c == '#') c = '/';
    out.push_back(rel);
  }
  if(d) closedir(d);
  return out;
}
static std::string breaker_path(const gxReplSetConfig &sc) { return derived_dir(sc) + "/incremental.breaker"; }
static std::string try_path(const gxReplSetConfig &sc) { return derived_dir(sc) + "/incremental.try"; }
// 11/14/2026 (review 4): the times (unix seconds) the index step stopped gxrepl, any batch, the last hour's kept
static std::string crashes_path(const gxReplSetConfig &sc) { return derived_dir(sc) + "/incremental.crashes"; }
static const int breaker_window_s = 3600, breaker_crashes = 3;
// 11/14/2026 (review 10): written when a batch's commits came without objects records (the source's object notes
// off: every batch is rebuilt), removed when a folder is applied object by object again
static std::string notes_off_path(const gxReplSetConfig &sc) { return derived_dir(sc) + "/notes_off"; }

static bool breaker_on(const gxReplSetConfig &sc, std::string *why = 0)
{
  FILE *f = fopen(breaker_path(sc).c_str(), "r");
  if(!f) return false;
  char b[512] = {0};
  size_t n = fread(b, 1, sizeof(b) - 1, f);
  fclose(f);
  if(why) { *why = std::string(b, n); while(!why->empty() && why->back() == '\n') why->pop_back(); }
  return true;
}

// The index files of a folder (derived, not stamps), sorted
static std::vector<std::string> folder_indexes(const gxReplSetConfig &sc, const std::string &rel)
{
  std::vector<std::string> out;
  std::string dir = rel.empty() ? sc.root : sc.root + "/" + rel;
  DIR *d = opendir(dir.c_str());
  struct dirent *e;
  while(d && (e = readdir(d)) != 0) {
    std::string n = e->d_name;
    if(n[0] == '.') continue;
    if(is_derived_index(sc, rel.empty() ? n : rel + "/" + n)) out.push_back(n);
  }
  if(d) closedir(d);
  std::sort(out.begin(), out.end());
  return out;
}

// 11/14/2026 (review 2): the index whose count is derived.stamp's keys (derived_count, else the first by name)
static bool counted_index(const gxReplSetConfig &sc, const std::vector<std::string> &ixs, size_t k)
{
  if(sc.derived_count.empty()) return false;          // re-review B: incremental refuses a set without it
  return fnmatch(sc.derived_count.c_str(), ixs[k].c_str(), 0) == 0;
}

// Rebuild needed: the stamps of a folder's indexes go (under the apply lock), so nothing applies to them
// object by object until the derived pass rebuilt them and stamped them again
// 11/14/2026 (review 8): all = a fill by files (either mode): derived.stamp and the folder's derived state go too, so
// the derived pass sees the indexes as missing and rebuilds them (before, a verify fill left derived.stamp and the
// state "ok": no rebuild ever came and no stamps came back, the 9,680-commit burst hung 10 minutes)
static void drop_stamps(const gxReplSetConfig &sc, const std::string &rel, const std::string &why, bool log_it,
			bool all)
{
  std::string dir = rel.empty() ? sc.root : sc.root + "/" + rel;
  int n = 0;
  // 11/15/2026 (review 3): the derived state and derived.stamp first, then the per-index stamps, and synced before
  // the caller writes the stream's position: a crash part way leaves the folder rebuilt, never stamps that look good
  if(all) {
    if(unlink(derived_state_path(sc, rel).c_str()) == 0) n++;
    if(unlink((dir + "/derived.stamp").c_str()) == 0) n++;
    sync_dir(derived_dir(sc));
    sync_dir(dir);
#ifdef GXREPL_TEST_HOOKS
    if(n && getenv("GXREPL_TEST_KILL_IN_DROP")) _exit(9);   // test builds: killed between the two
#endif
  }
  for(auto &ix : folder_indexes(sc, rel))
    if(unlink(gxdDerivedStamp::PathFor(dir + "/" + ix).c_str()) == 0) n++;
  sync_dir(dir);
  if(n && log_it) glog("set %s: %s: derived indexes: rebuild needed: %s", sc.name.c_str(),
		       rel.empty() ? "." : rel.c_str(), why.c_str());
}

// After a rebuild of an incremental set's folder (the derived pass, under the apply lock): fresh stamps at the
// folder's seal (its stream's log id, the lsn it was sealed at)
static int stamp_after_rebuild(const gxReplSetConfig &sc, const std::string &rel, std::string &why)
{
  std::string seal;
  {
    FILE *f = fopen((sc.root + "/.gxrepl/units/" + unit_key(rel)).c_str(), "rb");
    if(f) { char b[512]; size_t n = fread(b, 1, sizeof(b), f); seal.assign(b, n); fclose(f); }
  }
  unsigned long long g = 0, l = 0, c = 0;
  char sname[256] = {0};
  int sgot = sscanf(seal.c_str(), "log %llu %llu %255s %llu", &g, &l, sname, &c);
  if(sgot < 3) { why = "not sealed by a ship log record"; return 1; }
  if(sgot < 4) c = l;     // 11/14/2026 (seal per folder): stamped at the folder's last change, not the stream's position
  FILE *sf = fopen((sc.root + "/.gxrepl/state/" + sname + ".state").c_str(), "r");
  if(!sf) { why = std::string("no position for stream ") + sname; return 1; }
  char id[64] = {0}; unsigned long long al = 0;
  int got = fscanf(sf, "%63s %llu", id, &al);
  fclose(sf);
  if(got != 2) { why = "a damaged stream position"; return 1; }
  std::string dir = rel.empty() ? sc.root : sc.root + "/" + rel;
  // 11/14/2026 (review 2): each index's own count, read by the extractor in a throwaway staged scope (nothing it
  // does reaches a file); without the extractor the count stays 0 (unknown)
  std::string lerr;
  const gxrExtractorOps *ops = sc.derived_extractor.empty() ? 0 : load_extractor(sc.derived_extractor, lerr);
  std::string src_full;
  if(ops && !sc.derived_source.empty()) {
    DIR *dd = opendir(dir.c_str());
    struct dirent *de;
    while(dd && (de = readdir(dd)) != 0)
      if(de->d_name[0] != '.' && fnmatch(sc.derived_source.c_str(), de->d_name, 0) == 0) { src_full = dir + "/" + de->d_name; break; }
    if(dd) closedir(dd);
  }
  for(auto &ix : folder_indexes(sc, rel)) {
    gxdStampData d;
    d.log_id = id; d.lsn = c; d.time_us = (unsigned long long)time(0) * 1000000ULL;
    d.digest_kind = ix.size() > 4 && ix.compare(ix.size() - 4, 4, ".btx") == 0 ? gxdStampDigestBtree : gxdStampDigestBytes;
    if(ops && !src_full.empty()) {
      gxdStagedScope scope;
      char e[512] = {0};
      void *h = ops->Open((dir + "/" + ix).c_str(), src_full.c_str(), sc.derived_extractor_config.c_str(), e, sizeof(e));
      if(h) { d.records = ops->Count(h); ops->Close(h); }
      else glog("set %s: %s: %s: stamped with no count (0): the extractor did not open it: %s", sc.name.c_str(),   // re-review G
		rel.empty() ? "." : rel.c_str(), ix.c_str(), e);
    }
    if(gxdDerivedStamp::WriteFresh(gxdDerivedStamp::PathFor(dir + "/" + ix), d) != 0) { why = "cannot stamp " + ix; return -1; }
  }
  return 0;
}

// One object's change across a batch
struct ObjChange_ { unsigned char op = 0; uint32_t old_len = 0, new_len = 0; };

// Merge a later change of the same object into the batch's one (the gxdObjectNotes rules); false: cannot be exact
static bool merge_change(std::map<unsigned long long, ObjChange_> &m, const gxdShipObject &o)
{
  auto it = m.find(o.address);
  if(it == m.end()) { ObjChange_ c; c.op = o.op; c.old_len = o.old_len; c.new_len = o.new_len; m[o.address] = c; return true; }
  ObjChange_ &c = it->second;
  if(c.op == gxdObjectAdd && o.op == gxdObjectUpdate) { c.new_len = o.new_len; return true; }
  if(c.op == gxdObjectAdd && o.op == gxdObjectRemove) { m.erase(it); return true; }
  if(c.op == gxdObjectUpdate && o.op == gxdObjectUpdate) { c.new_len = o.new_len; return true; }
  if(c.op == gxdObjectUpdate && o.op == gxdObjectRemove) { c.op = gxdObjectRemove; c.new_len = 0; return true; }
  if(c.op == gxdObjectRemove && o.op == gxdObjectAdd) { c.op = gxdObjectUpdate; c.new_len = o.new_len; return true; }
  return false;
}

// Bytes [addr, addr + len) of a file as it will be after the batch: the batch's pages over the file
static bool bytes_after(int fd, const std::map<unsigned long long, const unsigned char *> *pages,
			unsigned long long addr, uint32_t len, std::vector<unsigned char> &out)
{
  out.assign(len, 0);
  unsigned long long done = 0;
  while(done < len) {
    unsigned long long a = addr + done, pg = a / gxdPageSize, in = a % gxdPageSize;
    unsigned long long n = std::min<unsigned long long>(gxdPageSize - in, len - done);
    const unsigned char *src = 0;
    if(pages) { auto it = pages->find(pg); if(it != pages->end()) src = it->second; }
    if(src) memcpy(out.data() + done, src + in, n);
    else if(fd >= 0) {
      ssize_t r = pread(fd, out.data() + done, n, (off_t)a);
      if(r < 0) return false;          // short: past the end of the file, zeros (as the batch extends it)
    }
    done += n;
  }
  return true;
}

static bool bytes_before(int fd, unsigned long long addr, uint32_t len, std::vector<unsigned char> &out)
{
  out.assign(len, 0);
  return fd >= 0 && pread(fd, out.data(), len, (off_t)addr) == (ssize_t)len;
}

// 11/14/2026: gxrReadAfterBatch() (gxrextract.h): the batch of this thread's extractor calls
static thread_local const PageBatch *extract_batch = 0;

extern "C" int gxrReadAfterBatch(const char *path, uint64_t offset, void *buf, uint32_t len)
{
  if(!extract_batch || !path || !buf) return -1;
  const std::map<unsigned long long, const unsigned char *> *pages = 0;
  auto e = extract_batch->find(path);
  if(e != extract_batch->end()) pages = &e->second.second;
  int fd = open(path, O_RDONLY | O_CLOEXEC);
  std::vector<unsigned char> b;
  bool ok = bytes_after(fd, pages, offset, len, b);
  if(fd >= 0) close(fd);
  if(!ok) return -1;
  memcpy(buf, b.data(), len);
  return (int)len;
}

// Index file identity, to catch an extractor that wrote outside the staged scope
static std::string file_id(const std::string &path)
{
  struct stat st;
  if(stat(path.c_str(), &st) != 0) return "none";
  return std::to_string((unsigned long long)st.st_size) + ":" + std::to_string((long long)st.st_mtim.tv_sec) + "." +
    std::to_string((long long)st.st_mtim.tv_nsec);
}

// 11/14/2026 (review 5): the hard watchdog of one batch's index step: a call into the extractor that never returns
// holds the apply lock for good, so past the limit gxrepl stops (ALERT, exit 8; the try file left behind counts the
// crash for the breaker) and its restart applies the batch again, by rebuild once the breaker trips
struct StepWatchdog_ {
  std::mutex m;
  std::condition_variable cv;
  bool done = false;
  std::thread t;
  StepWatchdog_(const std::string &set, unsigned long long limit_s) {
    if(!limit_s) return;
    t = std::thread([this, set, limit_s] {
      std::unique_lock<std::mutex> l(m);
      if(!cv.wait_for(l, std::chrono::seconds(limit_s), [this] { return done; })) {
	glog("ALERT set %s: the derived index step has run %llu s (an extractor call that never returns): gxrepl stops",
	     set.c_str(), limit_s);
	_exit(8);
      }
    });
  }
  ~StepWatchdog_() {
    if(!t.joinable()) return;
    { std::lock_guard<std::mutex> l(m); done = true; }
    cv.notify_all();
    t.join();
  }
};

// The incremental step of one batch, under the apply lock, before a page is written. Adds the index pages and
// stamps to "batch" (their bytes in "store"). Never fails the batch: a folder it cannot do is "rebuild needed".
// peak_mb: the most staged MB seen (stress tests).
static void incremental_apply(const gxReplSetConfig &sc, const std::string &root, const std::string &stream_name,
			      const std::string &stream_log_id, const std::vector<gxdShipRecordView> &recs, PageBatch &batch,
			      std::deque<std::vector<unsigned char> > &store, std::deque<std::string> &texts,
			      const std::map<std::string, bool> &unit_cb, unsigned long long last, bool &tried,
			      double *peak_mb = 0)
{
  tried = false;
  if(recs.empty()) return;
  unsigned long long first = recs.front().lsn;
  // 11/14/2026 (review 5): the cap is for the whole batch (every folder and index), and the step has a time limit
  unsigned long long batch_bytes = 0;
  struct timespec step_t0; clock_gettime(CLOCK_MONOTONIC, &step_t0);
  auto over_time = [&]() { return sc.derived_apply_max_s && ms_since(step_t0) > (double)sc.derived_apply_max_s * 1000.0; };
  unsigned long long hard = sc.derived_apply_max_s ? std::max(sc.derived_apply_max_s * 4, sc.derived_apply_max_s + 60) : 0;
#ifdef GXREPL_TEST_HOOKS
  if(const char *hw = getenv("GXREPL_TEST_WATCHDOG_S")) hard = strtoull(hw, 0, 10);   // test builds only
#endif
  StepWatchdog_ dog(sc.name, hard);
  // The folders this batch changes (their non-derived files)
  std::set<std::string> units;
  bool orphan = false;
  for(size_t i = 0; i < recs.size(); i++) {
    if(gxdShipObjectsOrphan(recs, i)) orphan = true;
    const gxdShipRecordView &r = recs[i];
    if(r.type != gxdShipCommit && r.type != gxdShipCommitZ) continue;
    std::vector<unsigned char> st;
    std::vector<gxdShipCommitFile> fl;
    if(gxdParseShipCommitRecord(r, st, fl) != 0) continue;
    for(auto &f : fl) if(!is_derived(sc, f.name)) units.insert(dir_of(f.name));
  }
  // 11/15/2026 (the recovery gap): an open record or a gap record in the stream means pages may have reached the
  // files with no object notes: a writer that died after its journal commit and before the ship log has it, whose
  // batch the next writer's Open() recovers into the files (only that writer's open record says so), or records
  // lost (a gap). The next commits can carry those pages (the same tail page, the file's header) without naming the
  // objects in them, and a verify then finds the files equal, so an index kept object by object would miss them
  // with its stamps in order. The stream's folders go back to a rebuild (their stamps, derived.stamp and derived
  // state), and (review 2) the stream is held off object by object until its verify is done: every batch of it
  // until then sends the folders it changes back to a rebuild too.
  // review 10 (moved 11/15/2026): commits without their objects record mean the source's object notes are off; said
  // here, before the open/gap rule and the hold (which return early), so the WARN shows whatever this batch holds
  for(size_t i = 0; i < recs.size(); i++) {
    if(recs[i].type != gxdShipCommit && recs[i].type != gxdShipCommitZ) continue;
    gxdShipObjectsRecord o;
    if(gxdShipObjectsFor(recs, i, o) == 0) {
      write_atomic(notes_off_path(sc), std::to_string((long long)time(0)) + " a commit without its objects record (lsn " +
		   std::to_string(recs[i].lsn) + ")\n");
      break;
    }
  }
  {
    unsigned long long og = 0;
    std::string ogwhy;
    for(auto &r : recs)
      // 11/15/2026 (hardening): an open record whose payload says its journal recovered nothing is a clean restart:
      // no pages without records, so no rebuild and no hold (an open with no payload, an older writer's, still is)
      if((r.type == gxdShipOpen && !gxdShipOpenIsClean(r.payload, r.len)) || r.type == gxdShipGap) {
	og = r.lsn;
	ogwhy = r.type == gxdShipOpen ? "a writer's open record (its journal may have recovered a batch with no record)"
				      : "a gap record (records lost)";
      }
    if(og) {
      mkdirs(derived_dir(sc));
      if(write_atomic(hold_path(sc, stream_name), stream_log_id + " " + std::to_string(og) + "\n") != 0)
	glog("set %s: cannot write %s", sc.name.c_str(), hold_path(sc, stream_name).c_str());
      std::set<std::string> fs(units.begin(), units.end());
      for(auto &u : stream_folders(sc, stream_name)) fs.insert(u);
      for(auto &u : fs) if(!folder_indexes(sc, u).empty()) drop_stamps(sc, u, ogwhy + " at " + std::to_string(og), true, true);
      return;
    }
    std::string hid;
    unsigned long long hl = 0;
    if(read_hold(sc, stream_name, hid, hl)) {
      for(auto &u : units)
	if(!folder_indexes(sc, u).empty())
	  drop_stamps(sc, u, "stream " + stream_name + " waits for its verify (an open or gap record at " +
		      (hl == ULLONG_MAX ? std::string("?") : std::to_string(hl)) + ")", true, true);
      return;
    }
  }
  std::string mode_why;
  const gxrExtractorOps *ops = 0;
  if(sc.commit_block.empty()) mode_why = "incremental needs commit_block";
  else if(sc.derived_source.empty()) mode_why = "no derived_source";
  else if(sc.derived_count.empty()) mode_why = "incremental needs derived_count (the index with one entry per record)";   // re-review B
  else if(breaker_on(sc, &mode_why)) mode_why = "the breaker is on (" + mode_why + "; gxrepl derived rebuild clears it)";
  else ops = load_extractor(sc.derived_extractor, mode_why);
  for(auto &u : units) {
    std::string dir = u.empty() ? root : root + "/" + u;
    std::vector<std::string> ixs = folder_indexes(sc, u);
    if(ixs.empty()) continue;                         // the derived pass makes them
    auto need = [&](const std::string &why) { drop_stamps(sc, u, why); };
    if(!ops) { need(mode_why); continue; }
    if(over_time()) { need("the index step passed derived_apply_max_s"); continue; }   // 11/14/2026 (review 5)
    if(orphan) { need("an objects record without its commit (a writer crash between them)"); continue; }
    auto cb = unit_cb.find(u);
    if(cb == unit_cb.end() || !cb->second) { need("the batch changed the folder without its commit block"); continue; }
    // Every index stamped at the folder's last change (the seal's 4th field, 11/14/2026 seal per folder: not the
    // stream's position, which records of the stream's other folders move on), of this stream's log
    unsigned long long seal_lsn = unit_changed_lsn(sc, u);
    bool skip = false, bad = false;
    std::string why;
    for(auto &ix : ixs) {
      gxdStampData d;
      int rc = gxdDerivedStamp::ReadFile(gxdDerivedStamp::PathFor(dir + "/" + ix), d);
      if(rc == 1) { bad = true; why = ix + " has no stamp"; break; }
      if(rc != 0) { bad = true; why = ix + " has a damaged stamp"; break; }
      if(d.log_id != stream_log_id) { bad = true; why = ix + " is stamped for another ship log"; break; }
      if(d.lsn >= last) { skip = true; continue; }      // a batch applied again after a crash: the index has it
      if(d.lsn >= first) { bad = true; why = ix + " is stamped inside this batch"; break; }
      if(d.lsn != seal_lsn) { bad = true; why = ix + " is stamped at " + std::to_string(d.lsn) + ", the folder at " +
			      std::to_string(seal_lsn); break; }
    }
    if(bad) { need(why); continue; }
    if(skip) continue;
    // The folder's data file and its object changes, commit by commit
    std::string src_rel;
    std::map<unsigned long long, ObjChange_> changes;
    for(size_t i = 0; i < recs.size() && !bad; i++) {
      const gxdShipRecordView &r = recs[i];
      if(r.type != gxdShipCommit && r.type != gxdShipCommitZ) continue;
      std::vector<unsigned char> st;
      std::vector<gxdShipCommitFile> fl;
      if(gxdParseShipCommitRecord(r, st, fl) != 0) { bad = true; why = "a damaged commit record"; break; }
      std::string here;
      for(auto &f : fl)
	if(dir_of(f.name) == u && fnmatch(sc.derived_source.c_str(), base_of(f.name).c_str(), 0) == 0) here = f.name;
      if(here.empty()) continue;                      // this commit left the data file alone
      if(!src_rel.empty() && src_rel != here) { bad = true; why = "two data files match derived_source"; break; }
      src_rel = here;
      gxdShipObjectsRecord o;
      int orc = gxdShipObjectsFor(recs, i, o);
      if(orc != 1) { bad = true; why = orc < 0 ? "a damaged objects record" : "a commit without its objects record (lsn " +
		     std::to_string(r.lsn) + ")";
	if(orc == 0) write_atomic(notes_off_path(sc), std::to_string((long long)time(0)) + " " + why + "\n");   // review 10
	break; }
      const gxdShipObjectFile *of = 0;
      for(auto &f : o.files) if(f.name == here) of = &f;
      if(!of || !of->complete) { bad = true; why = "the objects of " + here + " are not all known (lsn " + std::to_string(r.lsn) + ")"; break; }
      for(auto &ob : of->objects) if(!merge_change(changes, ob)) { bad = true; why = "an object changed in a way that cannot be followed"; break; }
    }
    if(bad) { need(why); continue; }
    // derived.stamp now: the object count goes on from it
    unsigned long long dg = 0, dl = 0, dn = 0, dk = 0;
    std::string dtail;   // 11/14/2026: the rebuild's own fields after "keys K" (e.g. OSA's keys_version): kept
    {
      FILE *f = fopen((dir + "/derived.stamp").c_str(), "r");
      int got = f ? fscanf(f, "gen %llu lsn %llu records %llu keys %llu", &dg, &dl, &dn, &dk) : 0;
      if(f && got == 4) { char b[256] = {0}; if(fgets(b, sizeof(b), f)) dtail = b; }
      if(f) fclose(f);
      if(got != 4) { need("no derived.stamp to count from"); continue; }
      while(!dtail.empty() && (dtail.back() == '\n' || dtail.back() == '\r')) dtail.pop_back();
    }
    // The generation the batch's commit block gives the folder
    uint64_t gen = 0;
    {
      std::string cbfull = dir + "/" + sc.commit_block;
      auto e = batch.find(cbfull);
      auto pg = e == batch.end() ? e->second.second.end() : e->second.second.find(0);
      if(e == batch.end() || pg == e->second.second.end()) { need("the batch has no commit block page"); continue; }
      memcpy(&gen, pg->second + sc.commit_gen_offset, 8);
      if(gen & 1) { need("the commit block is odd"); continue; }
    }
    std::string src_full = root + "/" + src_rel;
    const std::map<unsigned long long, const unsigned char *> *src_pages = 0;
    {
      auto e = batch.find(src_full);
      if(e != batch.end()) src_pages = &e->second.second;
    }
    long long adds = 0, removes = 0;
    for(auto &c : changes) { if(c.second.op == gxdObjectAdd) adds++; else if(c.second.op == gxdObjectRemove) removes++; }
    if(!src_rel.empty() && (long long)dn + adds - removes < 0) { need("the object count would go below zero"); continue; }
    // The extractor, in a staged scope, index by index
    tried = true;
    int fd = src_rel.empty() ? -1 : open(src_full.c_str(), O_RDONLY | O_CLOEXEC);
    std::vector<std::pair<std::string, std::string> > ids;
    for(auto &ix : ixs) ids.push_back(std::make_pair(dir + "/" + ix, file_id(dir + "/" + ix)));
    std::vector<gxdStagedFile> staged;
    unsigned long long count = 0;
    bool counted = false;
    double mb_cap = (double)sc.derived_apply_max_mb;
    {
      gxdStagedScope scope;
      struct BatchSet { BatchSet(const PageBatch *b) { extract_batch = b; } ~BatchSet() { extract_batch = 0; } } bset(&batch);
      char e[512] = {0};
      std::vector<unsigned char> b;
      for(size_t k = 0; k < ixs.size() && !bad; k++) {
	if(over_time()) { bad = true; why = "the index step passed derived_apply_max_s"; break; }   // re-review E
	std::string ip = dir + "/" + ixs[k];
	void *h = ops->Open(ip.c_str(), src_full.c_str(), sc.derived_extractor_config.c_str(), e, sizeof(e));
	if(!h) { bad = true; why = ixs[k] + ": extractor open: " + e; break; }
	for(auto &c : changes) {
	  if(c.second.op == gxdObjectAdd) continue;
	  if(!bytes_before(fd, c.first, c.second.old_len, b)) { bad = true; why = "cannot read an old object"; break; }
	  if(ops->Remove(h, c.first, b.data(), c.second.old_len, e, sizeof(e)) != 0) { bad = true; why = ixs[k] + ": extractor remove: " + e; break; }
	  double mb = (batch_bytes + scope.StagedBytes()) / 1048576.0;
	  if(peak_mb && mb > *peak_mb) *peak_mb = mb;
	  if(mb > mb_cap) { bad = true; why = "the batch's staged index pages passed derived_apply_max_mb"; break; }
	  if(over_time()) { bad = true; why = "the index step passed derived_apply_max_s"; break; }
	}
	for(auto it = changes.begin(); !bad && it != changes.end(); ++it) {
	  if(it->second.op == gxdObjectRemove) continue;
	  if(!bytes_after(fd, src_pages, it->first, it->second.new_len, b)) { bad = true; why = "cannot read a new object"; break; }
	  if(ops->Insert(h, it->first, b.data(), it->second.new_len, e, sizeof(e)) != 0) { bad = true; why = ixs[k] + ": extractor insert: " + e; break; }
	  double mb = (batch_bytes + scope.StagedBytes()) / 1048576.0;
	  if(peak_mb && mb > *peak_mb) *peak_mb = mb;
	  if(mb > mb_cap) { bad = true; why = "the batch's staged index pages passed derived_apply_max_mb"; break; }
	  if(over_time()) { bad = true; why = "the index step passed derived_apply_max_s"; break; }
	}
	if(!bad && ops->Flush(h, last, e, sizeof(e)) != 0) { bad = true; why = ixs[k] + ": extractor flush: " + e; }
	if(!bad && over_time()) { bad = true; why = "the index step passed derived_apply_max_s"; }   // re-review E
	if(!bad) {   // the pages Flush() wrote count too
	  double mb = (batch_bytes + scope.StagedBytes()) / 1048576.0;
	  if(peak_mb && mb > *peak_mb) *peak_mb = mb;
	  if(mb > mb_cap) { bad = true; why = "the batch's staged index pages passed derived_apply_max_mb"; }
	}
	// 11/14/2026 (review 2): each index is stamped with its own count; the counted index gives derived.stamp's keys
	unsigned long long ix_count = 0;
	if(!bad) ix_count = ops->Count(h);
	if(!bad && counted_index(sc, ixs, k)) { count = ix_count; counted = true; }
	// the stamp, in the same scope (its pages go with the index's)
	if(!bad) {
	  gxdDerivedStamp st;
	  gxdStampData sd;
	  if(st.Open(gxdDerivedStamp::PathFor(ip).c_str(), 0) != gxDBASE_NO_ERROR || st.Read(sd) != 0) { bad = true; why = ixs[k] + ": cannot open its stamp"; }
	  else {
	    sd.lsn = last; sd.records = ix_count; sd.time_us = (unsigned long long)time(0) * 1000000ULL;
	    if(st.Write(sd) != gxDBASE_NO_ERROR) { bad = true; why = ixs[k] + ": cannot stamp"; }
	  }
	  st.Close();
	}
	// 11/14/2026 (review 3): the index is closed before the capture (what its close writes, a header, is
	// captured too: the scope keeps a closed file's pages), and each index starts from a clean scope
	ops->Close(h);
	if(!bad && scope.Capture(staged) != 0) { bad = true; why = ixs[k] + ": the extractor opened the index again after closing it with changes"; }
	// 11/14/2026 (review 7): the scope may hold only this index and its stamp; any other file the extractor wrote
	// (another index, the data file, anything) is never applied: rebuild needed, with an ALERT
	if(!bad) {
	  const std::string sp = gxdDerivedStamp::PathFor(ip);
	  for(auto &f : staged)
	    if(f.path != ip && f.path != sp) {
	      glog("ALERT set %s: %s: the extractor %s wrote %s while it made %s: not applied (rebuild needed)",
		   sc.name.c_str(), u.empty() ? "." : u.c_str(), sc.derived_extractor.c_str(), f.path.c_str(), ixs[k].c_str());
	      bad = true; why = "the extractor wrote " + f.path + ", not only " + ixs[k] + " and its stamp";
	      break;
	    }
	}
	if(!bad) {
#ifdef GXREPL_TEST_HOOKS
	  if(getenv("GXREPL_TEST_CRASH_IN_SCRATCH")) _exit(9);   // test builds: killed with the index pages in memory
#endif
	  for(auto &f : staged) {
	    batch_bytes += f.data.size();                // 11/14/2026 (review 5): the batch's total
	    store.emplace_back(std::move(f.data));
	    auto &ent = batch[f.path];
	    if(f.size > ent.first) ent.first = f.size;
	    for(size_t p = 0; p < f.page_no.size(); p++) ent.second[f.page_no[p]] = store.back().data() + p * gxdPageSize;
	  }
	}
	scope.Rollback();
	if(bad) break;
      }
    }
    if(fd >= 0) close(fd);
    if(!bad && !counted) { bad = true; why = "no index of the folder matches derived_count " + sc.derived_count; }
    for(auto &id : ids)
      if(file_id(id.first) != id.second) {
	glog("ALERT set %s: %s: the extractor %s changed %s outside the staged scope", sc.name.c_str(),
	     u.empty() ? "." : u.c_str(), sc.derived_extractor.c_str(), id.first.c_str());
	bad = true; why = "the extractor wrote " + id.first + " directly";
      }
    if(bad) {
      // drop what this folder added to the batch (its index pages and stamps)
      for(auto &ix : ixs) { batch.erase(dir + "/" + ix); batch.erase(gxdDerivedStamp::PathFor(dir + "/" + ix)); }
      need(why);
      continue;
    }
    // derived.stamp and the derived state for the new generation, as a rebuild would leave them
    unsigned long long n_now = src_rel.empty() ? dn : (unsigned long long)((long long)dn + adds - removes);
    char line[512];
    snprintf(line, sizeof(line), "gen %llu lsn %llu records %llu keys %llu%s\n", (unsigned long long)gen, last, n_now,
	     (unsigned long long)count, dtail.c_str());
    auto put_text = [&](const std::string &path, const std::string &body) {
      texts.push_back(body);
      texts.back().resize(gxdPageSize, '\0');
      auto &ent = batch[path];
      ent.first = body.size();
      ent.second.clear();
      ent.second[0] = (const unsigned char *)texts.back().data();
    };
    unlink(notes_off_path(sc).c_str());                // 11/14/2026 (review 10): object by object again
    put_text(dir + "/derived.stamp", line);
    put_text(derived_state_path(sc, u), std::to_string((unsigned long long)gen) + " ok incremental lsn " + std::to_string(last) + "\n");
  }
}

// 11/14/2026 (review A1): the index step of one batch's records with its crash guard (incremental.try, the crash
// log and the breaker), under the apply lock before a page is written: the same for ApplyLog's records and for the
// records a verify's file update brings (ApplyFiles)
static void index_step(const gxReplSetConfig &sc, const std::string &root, const std::string &stream_name,
		       const std::string &log_id, const std::vector<gxdShipRecordView> &recs, PageBatch &batch,
		       std::deque<std::vector<unsigned char> > &store, std::deque<std::string> &texts,
		       const std::map<std::string, bool> &unit_cb, unsigned long long last, double &peak_mb)
{
  mkdirs(derived_dir(sc));
  // the breaker: a batch whose index step crashed gxrepl twice, or 3 crashes of the index step in an hour (any
  // batches, 11/14/2026 review 4: a different batch each time never tripped it), puts the set back on rebuild
  unsigned long long tf = 0, tn = 0;
  bool left = false;
  if(FILE *tfp = fopen(try_path(sc).c_str(), "r")) { left = true; if(fscanf(tfp, "%llu %llu", &tf, &tn) != 2) tn = 0; fclose(tfp); }
  std::vector<long long> when;
  {
	long long now = (long long)time(0), t = 0;
	if(FILE *cf = fopen(crashes_path(sc).c_str(), "r")) { while(fscanf(cf, "%lld", &t) == 1) if(t > now - breaker_window_s && t <= now) when.push_back(t); fclose(cf); }
	if(left) {                                     // the last index step never finished: gxrepl stopped in it
	  when.push_back(now);
	  std::string body;
	  for(auto x : when) body += std::to_string(x) + "\n";
	  write_atomic(crashes_path(sc), body);
	}
  }
  if(!breaker_on(sc)) {
	std::string why;
	if(tf == recs.front().lsn && tn >= 2)
	  why = "the index step of ship log record " + std::to_string(tf) + " stopped gxrepl " + std::to_string(tn) + " times";
	else if((int)when.size() >= breaker_crashes)
	  why = "the index step stopped gxrepl " + std::to_string(when.size()) + " times in the last hour";
	if(!why.empty()) {
	  write_atomic(breaker_path(sc), why + "\n");
	  glog("ALERT set %s: derived_apply back on rebuild: %s (gxrepl derived rebuild --set %s clears it)",
	       sc.name.c_str(), why.c_str(), sc.name.c_str());
	}
  }
  write_atomic(try_path(sc), std::to_string(recs.front().lsn) + " " + std::to_string(tf == recs.front().lsn ? tn + 1 : 1) + "\n");
  bool tried = false;
  double pk = 0;
  struct timespec it0; clock_gettime(CLOCK_MONOTONIC, &it0);
  incremental_apply(sc, root, stream_name, log_id, recs, batch, store, texts, unit_cb, last, tried, &pk);
  if(pk > peak_mb) peak_mb = pk;
  if(tried) stats_line("incremental", sc.name, ms_since(it0), std::to_string(pk) + " " + std::to_string(recs.size()));
  unlink(try_path(sc).c_str());
}

int gxReplLocal::ApplyLog(const std::string &set, const std::string &stream,
			  const std::string &log_id,
			  const std::vector<unsigned char> &records,
			  unsigned long long &lsn, std::string &err)
{
#if defined(GXREPL_TEST_HOOKS)
  apply_kind = "log";
#endif
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
  // 11/15/2026 (the late-pass hollow seal, b): records only change a file the copy has. A file the copy does not
  // have, whose first page (its header) the records do not hold, cannot be made from them: its other pages would be
  // zeros, and the unit's commit block in the same records would seal it whole. The copy is not in step: nothing is
  // applied or sealed, and the client resyncs the stream (its files by pieces, then the records).
  for(auto &e : batch) {
    if(e.second.second.count(0)) continue;
    struct stat fst;
    if(stat(e.first.c_str(), &fst) == 0) continue;
    std::string rel = e.first.compare(0, s->root.size() + 1, s->root + "/") == 0 ? e.first.substr(s->root.size() + 1) : e.first;
    err = "not in step: " + rel + " is not on the copy and the records do not hold it whole (resynced)";
    return -1;
  }
  std::map<std::string, bool> unit_cb_records = unit_cb;   // 11/14/2026: for the incremental derived apply
  // 11/10/2026: the record's commit block (gxdShipLog::SetCommitBlock) makes
  // its unit whole at this record: its seal goes in the same transaction
  std::deque<std::string> seals;
  // (a unit a verify is filling, "copying", stays so: only its commit block, sent after its files, seals it)
  if(add_unit_seals(s->sc, s->root, batch, last, stream, unit_cb, seals, err,
		    [&](const std::string &u) { return !fill_copying(read_seal(s->root, u)); }) != 0) return -1;
  // 11/12/2026: the stream's applied position is written under the same apply lock as the records' pages and their
  // seal, not after it is let go: a reader holding the lock never sees a seal at record L with the position at L-1
  gxReplStreamState st = it->second;
  st.lsn = last;
  // 11/14/2026: incremental derived apply, under the apply lock before a page is written (see incremental_apply)
  std::deque<std::vector<unsigned char> > istore;
  std::deque<std::string> itexts;
  std::function<int(std::string &)> pre = nullptr;
  std::vector<gxdShipRecordView> recs;
  if(s->sc.derived_apply == "incremental" && !s->sc.derived.empty() &&
     gxdParseShipRecords(records.data(), records.size(), recs) == 0 && !recs.empty()) {
    pre = [&](std::string &) -> int {
      index_step(s->sc, s->root, stream, it->second.log_id, recs, batch, istore, itexts, unit_cb_records, last, derived_peak_mb);
      return 0;
    };
  }
  if(apply_batch(s, batch, err, [&](std::string &e) { test_position_delay(); return write_state(s, st, e); }, pre) != 0)
    return -1;
  applied_records += count;
  lsn = last;
  return 0;
}

int gxReplLocal::ApplyFiles(const std::string &set,
			    const std::vector<gxReplFileUpdate> &v,
			    std::vector<int> &results, std::string &err)
{
#if defined(GXREPL_TEST_HOOKS)
  apply_kind = "files";
#endif
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
  struct FileRecs_ { std::string stream, log_id; const std::vector<unsigned char> *records; unsigned long long last;
		     std::map<std::string, bool> unit_cb; };
  std::vector<FileRecs_> file_recs;         // 11/14/2026: records applied here (see below)
  std::deque<std::vector<unsigned char> > fr_store;
  std::deque<std::string> fr_texts;
  std::map<std::string, bool> staged_now;   // 11/13/2026 (F7 review 3): unit -> written in its stage
  std::set<std::string> stale_units;        // round 3: a stage not mine: discarded before this apply
  // 11/15/2026 (stamp-keeping fill): the pages an in-step update's pieces lay into the derived source file, with
  // the page the update's own records had put there (0: none), for the check below (under the apply lock)
  struct FillPage_ { size_t i; unsigned long long pg; const unsigned char *piece; const unsigned char *rec; size_t len; };
  struct FillSize_ { size_t i; bool had_rec; unsigned long long rec_size; };
  std::vector<FillPage_> fill_pages;
  std::vector<FillSize_> fill_sizes;
  std::set<std::string> fill_unexplained;   // folders whose fill cannot be checked page by page (not in step, ...)
  for(size_t i = 0; i < v.size(); i++) {
    const gxReplFileUpdate &u = v[i];
    if(SafePath(s->sc, u.path, fulls[i], err) != 0) return -1;
    if(!s->sc.commit_block.empty() && !staged_now.count(dir_of(u.path))) {
      bool valid = stage_valid(s->sc, s->root, dir_of(u.path)), mine = s->staged.count(dir_of(u.path)) > 0;
      // 11/13/2026 (F7 focused review, 1): a set fed by a ship log is never staged (MarkGen's rule): its records
      // apply in place, so a stage of the unit (a staged copy cut before the log began) is stale: discarded under
      // the apply lock, and this update goes in place with the records, never half into the stage
      if(mine && (!states.empty() || !u.stream.empty())) { mine = false; s->staged.erase(dir_of(u.path)); }
      staged_now[dir_of(u.path)] = valid && mine;
      // 11/13/2026 (F7 review round 3, 3): a stage no copy of this process is filling (left by a lost link, its
      // unit now updated otherwise, e.g. by the ship log): discarded under the apply lock, the update goes in place
      if(valid && !mine) stale_units.insert(dir_of(u.path));
    }
    fulls[i] = staged_path(s->sc, s->root, u.path, fulls[i], staged_now[dir_of(u.path)]);   // 11/12/2026 (F7): into the unit's stage while one is open
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
	// 11/14/2026 (replay finding): records a verify brings (an in-step update after a writer's open) change the
	// folders like ApplyLog's do: on an incremental set they go through the same index step, under the apply lock
	// (below, in check), or their folders' stamps would look behind and be rebuilt
	if(s->sc.derived_apply == "incremental" && !s->sc.derived.empty())
	{
	  // review A2: one entry per stream's records (two files of one folder can carry the same records)
	  bool dup = false;
	  for(auto &fr : file_recs) if(fr.log_id == u.log_id && fr.last == e) dup = true;
	  if(!dup) file_recs.push_back(FileRecs_{u.stream, u.log_id, &u.records, e, unit_cb});
	}
      }
      gxReplStreamState ns;
      ns.name = u.stream; ns.log_id = u.log_id; ns.lsn = u.lsn;
      states[u.stream] = ns;
      changed[u.stream] = ns;
    }
    // 11/15/2026 (stamp-keeping fill): the derived source's pieces of an incremental set are noted page by page
    bool note_fill = !u.pieces.empty() && s->sc.derived_apply == "incremental" && !s->sc.derived.empty() &&
      !s->sc.derived_source.empty() && fnmatch(s->sc.derived_source.c_str(), base_of(u.path).c_str(), 0) == 0;
    if(note_fill) {
      bool in_step = !u.stream.empty() && !not_in_step.count(dir_of(u.path)) && !staged_now[dir_of(u.path)];
      if(!in_step) fill_unexplained.insert(dir_of(u.path));
      auto had = batch.find(fulls[i]);
      fill_sizes.push_back(FillSize_{i, had != batch.end(), had != batch.end() ? had->second.first : 0ULL});
    }
    auto &ent = batch[fulls[i]];
    ent.first = u.size;
    for(auto &p : u.pieces) {
      if(p.offset % gxdPageSize != 0) { err = "piece not page aligned"; return -1; }
      size_t n = p.data.size();
      for(size_t k = 0; k < n; k += gxdPageSize) {
	unsigned long long pg = (p.offset + k) / gxdPageSize;
	const unsigned char *prev = 0;
	if(note_fill) { auto pp = ent.second.find(pg); if(pp != ent.second.end()) prev = pp->second; }
	if(n - k >= gxdPageSize) ent.second[pg] = p.data.data() + k;
	else {
	  padded.push_back(std::vector<unsigned char>(gxdPageSize, 0));
	  memcpy(padded.back().data(), p.data.data() + k, n - k);
	  ent.second[pg] = padded.back().data();
	}
	if(note_fill) fill_pages.push_back(FillPage_{i, pg, ent.second[pg], prev, n - k >= gxdPageSize ? (size_t)gxdPageSize : n - k});
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
  std::map<std::string, std::string> seal_before_touch;   // 11/15/2026 (review MEDIUM): see explained_fill's caller
  if(!s->sc.commit_block.empty()) {
    std::set<std::string> filling, cb_units;
    for(size_t i = 0; i < v.size(); i++) {
      if(results[i] != 0) continue;
      if(base_of(v[i].path) == s->sc.commit_block) { if(!v[i].stream.empty()) cb_units.insert(dir_of(v[i].path)); }
      else if(!v[i].pieces.empty() && (v[i].stream.empty() || not_in_step.count(dir_of(v[i].path))))
	filling.insert(dir_of(v[i].path));
    }
    if(!filling.empty() && mkdirs(s->root + "/.gxrepl/units") != 0) { err = "cannot make the seals folder"; return -1; }
    for(auto &u : filling)
      if(!staged_now[u]) put_seal(batch, s->root, u, "copying\n", seals);   // a stage keeps the unit's seal
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
	  }, &not_in_step) != 0) return -1;
    }
    // 11/14/2026 (seal per folder): a verify that repairs an in-step unit's data (pieces of its other files, in
    // step with its stream) changed the folder at the stream's position, even when its commit block (the same
    // generation) comes in another apply: the seal's 4th field moves there, so its derived indexes are rebuilt
    // review 1: a size change alone (the copy's file longer than the source's: cut) is a change too
    // review 3: the position is of the update's stream; a unit the seal says another stream feeds (or two streams
    // here) gets C = L, never an lsn of another numbering
    struct Touch { unsigned long long lsn = 0; std::string stream; bool mixed = false; };
    std::map<std::string, Touch> touched;
    for(size_t i = 0; i < v.size(); i++) {
      if(results[i] != 0 || v[i].stream.empty()) continue;
      if(base_of(v[i].path) == s->sc.commit_block || is_derived(s->sc, v[i].path)) continue;
      std::string u = dir_of(v[i].path);
      if(filling.count(u) || cb_units.count(u) || staged_now[u]) continue;
      bool change = !v[i].pieces.empty();
      if(!change) {
	struct stat fst;
	change = stat(fulls[i].c_str(), &fst) == 0 && (unsigned long long)fst.st_size != v[i].size;
      }
      if(!change) continue;
      Touch &t = touched[u];
      if(!t.stream.empty() && t.stream != v[i].stream) t.mixed = true;
      t.stream = v[i].stream;
      if(v[i].lsn > t.lsn) t.lsn = v[i].lsn;
    }
    for(auto &tt : touched) {
      auto &t = tt;
      std::string key = s->root + "/.gxrepl/units/" + unit_key(t.first), body;
      auto pend = batch.find(key);
      if(pend != batch.end() && pend->second.second.count(0))
	body.assign((const char *)pend->second.second.at(0), pend->second.first);
      else body = read_small(key);
      char sname[256] = {0}; unsigned long long g = 0, l = 0, c = 0;
      if(sscanf(body.c_str(), "log %llu %llu %255s %llu", &g, &l, sname, &c) < 3) continue;   // not log sealed
      unsigned long long at = t.second.lsn;
      if(t.second.mixed || t.second.stream != sname || at > l) at = l;
      seal_before_touch[t.first] = body;
      put_seal(batch, s->root, t.first, "log " + std::to_string(g) + " " + std::to_string(l) + " " + sname + " " +
	       std::to_string(at) + "\n", seals);
    }
  }
  // 11/12/2026 (F7): a staged unit whose commit block arrives here, even (one whole commit), is swapped in, in the
  // same apply lock as that commit block: the unit goes from its old sealed commit to the new one at once
  std::set<std::string> finish;
  if(!s->sc.commit_block.empty()) {
    for(size_t i = 0; i < v.size(); i++) {
      // round 3: only a stage this process is filling, and whose seal is written (SealUnit), is swapped in
      struct stat sst;
      if(results[i] != 0 || base_of(v[i].path) != s->sc.commit_block || !staged_now[dir_of(v[i].path)] ||
	 stat((stage_dir(s->root, dir_of(v[i].path)) + "/.seal").c_str(), &sst) != 0) continue;
      auto e = batch.find(fulls[i]);
      if(e == batch.end()) continue;
      auto pg = e->second.second.find(0);
      if(pg == e->second.second.end() || e->second.first < s->sc.commit_gen_offset + 8) continue;
      uint64_t g = 0;
      memcpy(&g, pg->second + s->sc.commit_gen_offset, 8);
      if(!(g & 1)) finish.insert(dir_of(v[i].path));
    }
  }
  // 11/15/2026 (stamp-keeping fill): the folders of "filled" whose derived source pieces change nothing beyond what
  // this update's records write: each page the pieces lay in (and that no later record replaced) holds the bytes the
  // records had put there, or, where no record wrote it, the bytes the copy holds now; and the file's size is the
  // records' or the copy's. Called in check(), under the apply lock, before a page is written.
  auto explained_fill = [&](const std::set<std::string> &filled) -> std::set<std::string> {
    std::set<std::string> ok;
    // review (HIGH): only an incremental set keeps stamps through a fill; in rebuild mode a fill of the derived
    // source always drops them (and derived.stamp and the state), as before
    if(s->sc.derived_apply != "incremental") return ok;
    // review (HIGH): never across an open or a gap record (a batch the writer's journal recovered, records lost):
    // the recovery-gap rule (incremental_apply) sends those folders to a rebuild
    for(auto &fr : file_recs) {
      std::vector<gxdShipRecordView> rv;
      if(gxdParseShipRecords(fr.records->data(), fr.records->size(), rv) != 0) return ok;
      for(auto &r : rv)
	if((r.type == gxdShipOpen && !gxdShipOpenIsClean(r.payload, r.len)) || r.type == gxdShipGap) return ok;
    }
    std::set<std::string> noted;                 // folders whose derived source pieces were noted page by page
    for(auto &fs : fill_sizes) noted.insert(dir_of(v[fs.i].path));
    for(auto &u : filled) if(!fill_unexplained.count(u) && noted.count(u)) ok.insert(u);
    if(ok.empty()) return ok;
    auto bad = [&](size_t i) { ok.erase(dir_of(v[i].path)); };
    for(auto &fs : fill_sizes) {
      size_t i = fs.i;
      if(!ok.count(dir_of(v[i].path))) continue;
      auto e = batch.find(fulls[i]);
      if(e == batch.end()) { bad(i); continue; }
      if(e->second.first != v[i].size) continue;             // a later record set the size: the pieces' does not count
      if(fs.had_rec) { if(fs.rec_size != v[i].size) bad(i); continue; }
      struct stat st;
      if(stat(fulls[i].c_str(), &st) != 0 || (unsigned long long)st.st_size != v[i].size) bad(i);
    }
    std::map<size_t, int> fds;
    std::vector<unsigned char> disk(gxdPageSize);
    for(auto &fp : fill_pages) {
      size_t i = fp.i;
      if(!ok.count(dir_of(v[i].path))) continue;
      auto e = batch.find(fulls[i]);
      if(e == batch.end()) { bad(i); continue; }
      auto pe = e->second.second.find(fp.pg);
      if(pe == e->second.second.end() || pe->second != fp.piece) continue;   // replaced by a later record
      unsigned long long off = fp.pg * (unsigned long long)gxdPageSize;
      // review (LOW): the whole page the batch writes (a short piece is padded with zeros to a page), up to the
      // file's end, not only the piece's own bytes
      size_t c = v[i].size > off ? (size_t)std::min<unsigned long long>(gxdPageSize, v[i].size - off) : 0;
      if(c == 0) continue;                                     // past the file's end: never read
      if(fp.rec) { if(memcmp(fp.rec, fp.piece, c) != 0) bad(i); continue; }
      auto fd = fds.find(i);
      if(fd == fds.end()) fd = fds.emplace(i, open(fulls[i].c_str(), O_RDONLY | O_CLOEXEC)).first;
      if(fd->second < 0 || pread(fd->second, disk.data(), c, (off_t)off) != (ssize_t)c || memcmp(disk.data(), fp.piece, c) != 0)
	bad(i);
    }
    for(auto &fd : fds) if(fd.second >= 0) close(fd.second);
    return ok;
  };
  auto times = [&](size_t i) {
    chmod(fulls[i].c_str(), (mode_t)(v[i].mode & 07777));
    struct timespec ts[2];
    ts[0].tv_sec = time(0); ts[0].tv_nsec = 0;
    ts[1].tv_sec = (time_t)(v[i].mtime_ns / 1000000000LL);
    ts[1].tv_nsec = (long)(v[i].mtime_ns % 1000000000LL);
    utimensat(AT_FDCWD, fulls[i].c_str(), ts, 0);
  };
  // 11/13/2026 (F7 review 3): under the apply lock, before a page is written: each unit's stage is as it was when
  // its paths were chosen, and a unit this process began staging still has its stage (gxrepl promote, a stale
  // stage: the batch is dropped, the copy starts again next pass; never in place under an even commit block)
  auto check = [&](std::string &e) -> int {
    // 11/14/2026: a copy by files (a first fill, a resync, a unit copy) is not object by object: the folder's
    // derived index stamps go (rebuild needed)
    // 11/14/2026 (review 8): in both modes, and derived.stamp and the derived state go too. Re-review A: only when
    // the fill wrote the file the indexes are made from (derived_source, e.g. events.gxd): a piece of any other
    // file (a journal, the commit block a verify re-sends) changes no index, and dropping the stamp would force a
    // full rebuild under the apply lock for nothing. Without derived_source (a rebuild set) a fill drops nothing:
    // the stamp's generation and the seal's last change say when the indexes are behind.
    if(!s->sc.derived.empty() && !s->sc.derived_source.empty()) {
      std::set<std::string> filled;
      for(size_t i = 0; i < v.size(); i++)
	if(results[i] == 0 && !v[i].pieces.empty() &&
	   fnmatch(s->sc.derived_source.c_str(), base_of(v[i].path).c_str(), 0) == 0)
	  filled.insert(dir_of(v[i].path));
      // 11/15/2026 (stamp-keeping fill): a verify that races the next commit sends the derived source's pieces
      // with the records of that commit (Diff(): the records from the copy's position to the pieces'). When every
      // page the pieces write is what those records write, or what the copy already holds, the pieces change nothing
      // the records do not, and the index step below applies the records to the stamped indexes: the stamps stay.
      // Anything else (a first fill, a resync, a stage, pages no record explains: a recovered batch) drops them.
      std::set<std::string> explained = explained_fill(filled);
      for(auto &u : filled)
	if(!explained.count(u)) drop_stamps(s->sc, u, "copied by files, not by the ship log", true, true);
      // review (MEDIUM): an explained fill changed nothing the records did not, so the folder's last change (the
      // seal's 4th field) is the records' and not the update's position: the seal from before the touch goes back
      for(auto &u : explained) {
	auto sb = seal_before_touch.find(u);
	if(sb != seal_before_touch.end() && !sb->second.empty()) put_seal(batch, s->root, u, sb->second, seals);
      }
    }
    // 11/14/2026: the index step for the records this update applies (an incremental set), as ApplyLog runs it
    for(auto &fr : file_recs) {
      std::vector<gxdShipRecordView> recs;
      if(gxdParseShipRecords(fr.records->data(), fr.records->size(), recs) != 0 || recs.empty()) continue;
      // a folder this update also fills by files has no stamps now: its index step finds that (rebuild needed)
      index_step(s->sc, s->root, fr.stream, fr.log_id, recs, batch, fr_store, fr_texts, fr.unit_cb, fr.last, derived_peak_mb);
    }
    if(!stale_units.empty()) {
      if(recover_journal_locked(s->root) != 0) {
	e = "the apply journal holds a batch that cannot be applied yet: tried again next pass";
	return -1;
      }
      for(auto &u : stale_units) {
	if(discard_stage(s->root, u) != 0) { e = "cannot remove the stage of " + u; return -1; }
	glog("set %s: %s: discarded a stage no copy is filling (updated in place)", s->sc.name.c_str(),
	     u.empty() ? "." : u.c_str());
      }
    }
    for(auto &kv : staged_now) {
      std::string un = kv.first.empty() ? std::string(".") : kv.first;
      if((stage_valid(s->sc, s->root, kv.first) && s->staged.count(kv.first)) != kv.second) {
	e = "the stage of " + un + " changed during this apply (gxrepl promote?): batch dropped, copied again next pass";
	return -1;
      }
      if(!kv.second && s->staged.count(kv.first)) {
	s->staged.erase(kv.first);
	e = "the stage of " + un + " was discarded (gxrepl promote) or went stale: this copy stops; the next pass "
	    "copies the unit again";
	return -1;
      }
    }
    return 0;
  };
  // 11/14/2026: the streams' applied positions are written in the apply lock too, after the batch (its "log G L"
  // seals): they were written after the lock was let go, so a reader holding the lock (promote-check) could see a
  // seal at record L with the position at L-1 (the record path does this since 11/12)
  auto positions = [&](std::string &e) -> int {
    test_position_delay();
    for(auto &c : changed) if(write_state(s, c.second, e) != 0) return -1;
    return 0;
  };
  // (apply_batch does nothing then, not even its in_lock). 11/15/2026: records with no pages (a lone open or gap
  // record) on an incremental set still go through check() and its index step, under the apply lock
  if(batch.empty() && file_recs.empty()) return positions(err);
  int arc = finish.empty() ? apply_batch(s, batch, err, positions, check) : apply_batch(s, batch, err, [&](std::string &e) {
      for(size_t i = 0; i < v.size(); i++)
	if(results[i] == 0 && v[i].last && finish.count(dir_of(v[i].path))) times(i);
#ifdef GXREPL_TEST_HOOKS
      if(getenv("GXREPL_TEST_CRASH_BEFORE_SWAP")) _exit(9);   // test builds: a stage complete, its swap not begun
#endif
      for(auto &u : finish) { if(stage_swap(s->sc, s->root, u, e) != 0) return -1; s->staged.erase(u); }
      return positions(e);
    }, check);
  if(arc != 0) return -1;
  for(size_t i = 0; i < v.size(); i++) {
    if(results[i] != 0 || !v[i].last) continue;
    if(finish.count(dir_of(v[i].path))) continue;       // set before its swap
    times(i);
  }
  return 0;
}

// 11/13/2026 (dead copy folders): a folder of a commit_block set that the source no longer has (osa_ingest archives
// a day every night): gxrepl deleted the files it copies (the data files, the commit block), but the files it never
// copies stayed (derived indexes by_*.btx, derived.stamp, the writer's journal), and promote-check saw the folder
// UNSEALED (database files and no commit block), refusing every take over until it was moved by hand (11/12, 11/13).
static std::string derived_state_path(const gxReplSetConfig &sc, const std::string &rel);

// A file of a dead copy folder that may go (review): only what is rebuilt or safe to lose, i.e. a derived index (the set's
// derived patterns), gxrepl's default excludes (journals, locks, temp files) and the derived rebuild's stamp, never a
// configured writer lock. Anything else (a node's own excluded database file, a user's file) keeps the folder.
static bool dead_removable(const gxReplSetConfig &sc, const std::string &rel, const std::string &name, bool quiet)
{
  // review: for the sweeps (quiet), a file changed in the last 10 minutes (a writer at work, e.g. after a take over)
  // keeps the folder; a Delete (the source just dropped the folder, its writer locks held) needs no wait
  struct stat fst;
  if(quiet && stat((sc.root + "/" + rel).c_str(), &fst) == 0 && time(0) - fst.st_mtime < dead_quiet_sec()) return false;
  for(auto &w : sc.writer_locks) if(w == rel) return false;
  if(is_derived(sc, rel) || name == "derived.stamp") return true;
  for(int i = 0; default_excludes[i]; i++) if(fnmatch(default_excludes[i], name.c_str(), 0) == 0) return true;
  return false;
}

// A dead copy folder goes whole: its stage, the files gxrepl does not copy, the folder, then its seal and its derived
// state (a stop in between leaves the seal: the next sweep finishes). Only a folder with no commit block, no file
// gxrepl copies and no sub-folder, never the set's root; any_seal false (the sweeps): only a folder sealed whole once
// (not "copying"). The caller holds the set's apply lock and its writer locks and recovered the journal.
// 1 = removed, 0 = left alone, -1 = error.
static int sweep_dead_unit(const gxReplSetConfig &sc, const std::string &root, const std::string &rel, bool any_seal,
			   std::string &what)
{
  if(rel.empty() || sc.commit_block.empty()) return 0;
  std::string dir = root + "/" + rel, seal_path = root + "/.gxrepl/units/" + unit_key(rel);
  std::string seal = read_small(seal_path);
  struct stat st;
  bool exists = lstat(dir.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
  if(seal.empty()) return 0;                                             // never a folder gxrepl did not seal (review)
  if(!any_seal && seal.compare(0, 8, "copying\n") == 0 && !copying_abandoned(seal_path))
    return 0;                                                            // a copy in progress ("copying-log": not)
  std::vector<std::string> files;
  if(exists) {
    DIR *d = opendir(dir.c_str());
    if(!d) return 0;
    struct dirent *e;
    bool keep = false;
    while((e = readdir(d)) != 0) {
      std::string n = e->d_name;
      if(n == "." || n == "..") continue;
      std::string rp = rel + "/" + n;
      if(lstat((dir + "/" + n).c_str(), &st) != 0) continue;
      if(S_ISDIR(st.st_mode)) { keep = true; break; }                    // sub-folders: not this sweep's
      if(n == sc.commit_block || (is_db_file(sc, rp) && !excluded(sc, rp))) { keep = true; break; }   // still a copy
      if(!dead_removable(sc, rp, n, !any_seal)) { keep = true; break; }  // a file that is not ours to remove
      files.push_back(n);
    }
    closedir(d);
    if(keep) return 0;
  }
  if(discard_stage(root, rel) != 0) { what = "cannot remove the stage of " + rel; return -1; }
  for(auto &n : files)
    if(unlink((dir + "/" + n).c_str()) != 0 && errno != ENOENT) { what = "cannot remove " + rel + "/" + n; return -1; }
  if(exists) {
    if(rmdir(dir.c_str()) != 0 && errno != ENOENT) { what = "cannot remove the folder " + rel; return -1; }
    std::string up = dir.substr(0, dir.find_last_of('/'));
    while(up.size() > root.size() && rmdir(up.c_str()) == 0) up = up.substr(0, up.find_last_of('/'));
    sync_dir(up.size() >= root.size() ? up : root);
  }
  unlink(derived_state_path(sc, rel).c_str());                          // before the seal (review): never a stale "ok"
  if(unlink(seal_path.c_str()) != 0 && errno != ENOENT) { what = "cannot remove the seal of " + rel; return -1; }
  sync_dir(root + "/.gxrepl/units");
  if(exists) {
    std::string names;
    for(size_t i = 0; i < files.size() && i < 6; i++) names += (i ? ", " : "") + files[i];
    if(files.size() > 6) names += ", ...";
    what = "removed the copy's folder " + rel + ": gone from the source (" + std::to_string(files.size()) +
           " file(s) gxrepl does not copy" + (names.empty() ? std::string("") : ": " + names) + ")";
  } else what = "removed the seal of " + rel + ": the folder is gone";
  return 1;
}

// A folder that would be swept (no locks taken: the sweep checks again under them)
static bool dead_unit_candidate(const gxReplSetConfig &sc, const std::string &root, const std::string &rel,
				bool quiet = true)
{
  if(rel.empty()) return false;
  std::string dir = root + "/" + rel;
  struct stat st;
  if(lstat(dir.c_str(), &st) != 0) return true;                          // a seal of a folder that is gone
  if(!S_ISDIR(st.st_mode)) return false;
  DIR *d = opendir(dir.c_str());
  if(!d) return false;
  struct dirent *e;
  bool dead = true;
  while(dead && (e = readdir(d)) != 0) {
    std::string n = e->d_name;
    if(n == "." || n == "..") continue;
    std::string rp = rel + "/" + n;
    if(lstat((dir + "/" + n).c_str(), &st) != 0) continue;
    if(S_ISDIR(st.st_mode) || n == sc.commit_block || (is_db_file(sc, rp) && !excluded(sc, rp)) ||
       !dead_removable(sc, rp, n, quiet)) dead = false;
  }
  closedir(d);
  return dead;
}

int gxReplSweepDeadCopies(const gxReplSetConfig &sc, std::vector<std::string> &log)
{
  if(sc.commit_block.empty()) return 0;
  std::string root = sc.root, ub = root + "/.gxrepl/units";
  std::vector<std::string> cand;
  if(DIR *d = opendir(ub.c_str())) {
    struct dirent *e;
    while((e = readdir(d)) != 0) {
      std::string n = e->d_name;
      if(n.size() < 2 || n[0] != '_' || n == "_root") continue;
      bool tmp = false;                                                   // review: a seal being written (.gxrtmp)
      for(int i = 0; default_excludes[i]; i++) if(fnmatch(default_excludes[i], n.c_str(), 0) == 0) tmp = true;
      if(tmp) continue;
      std::string rel = n.substr(1);
      // review: unit_key writes '/' as '#', so a folder with '#' in its name has the same key as a nested path;
      // a seal whose literal name exists as a folder is never decoded (left alone)
      struct stat lst;
      if(rel.find('#') != std::string::npos && lstat((root + "/" + rel).c_str(), &lst) == 0) continue;
      for(auto &c : rel) if(c == '#') c = '/';
      std::string seal = read_small(ub + "/" + n);
      // a copy in progress: never swept; 11/15/2026: unless abandoned (its seal unchanged for the quiet time)
      if(seal.empty() || (seal.compare(0, 8, "copying\n") == 0 && !copying_abandoned(ub + "/" + n))) continue;
      if(dead_unit_candidate(sc, root, rel)) cand.push_back(rel);
    }
    closedir(d);
  }
  if(cand.empty()) return 0;
  WriterHold wh;
  std::string we;
  if(hold_writer_locks(sc, wh, we)) return 0;                             // the folders' writer runs: its data
  std::string jname = root + "/.gxrepl/apply.jnl";
  int lk = open(jname.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
  if(lk < 0) return 0;
  struct LockGuard { int fd; ~LockGuard() { flock(fd, LOCK_UN); close(fd); } } lkg{lk};
  if(lock_apply_bounded(lk) != 0) return 0;                               // busy: the next sweep
  if(recover_journal_locked(root) != 0) return 0;                         // a batch pending: never under it
  int n = 0;
  for(auto &rel : cand) {
    std::string what;
    int rc = sweep_dead_unit(sc, root, rel, false, what);
    if(rc == 1) { n++; log.push_back("set " + sc.name + ": " + what); }
    else if(rc < 0) log.push_back("WARNING set " + sc.name + ": " + what);
  }
  return n;
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
  // 11/13/2026 (F7 review round 3, 4): a delete goes into a stage only while this process's staged copy fills it;
  // a unit whose commit block is deleted (the unit is gone from the source), or a stage nobody fills, has its
  // stage discarded (under the apply lock, after the journal) and the delete goes in place
  if(!s->sc.commit_block.empty()) {
    std::set<std::string> drop;
    for(auto &p : paths) {
      std::string u = dir_of(p);
      struct stat sst;
      if(stat((stage_dir(s->root, u) + "/.stage").c_str(), &sst) != 0) continue;
      if(base_of(p) == s->sc.commit_block || !s->staged.count(u)) drop.insert(u);
    }
    if(!drop.empty()) {
      std::string jname = s->root + "/.gxrepl/apply.jnl";
      int lk = open(jname.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
      if(lk < 0) { err = "cannot lock " + jname; return -1; }
      struct LockGuard { int fd; ~LockGuard() { flock(fd, LOCK_UN); close(fd); } } lkg{lk};
      if(lock_apply_bounded(lk) != 0) { err = "busy: " + jname + " is held; tried again next pass"; return -1; }
      if(recover_journal_locked(s->root) != 0) { err = "the apply journal holds a batch that cannot be applied yet"; return -1; }
      for(auto &u : drop) {
	if(discard_stage(s->root, u) != 0) { err = "cannot remove the stage of " + u; return -1; }
	s->staged.erase(u);
	glog("set %s: %s: its stage was discarded (deleted on the source, or no copy fills it)", s->sc.name.c_str(),
	     u.empty() ? "." : u.c_str());
      }
    }
  }
  // 11/14/2026 (seal per folder review 1, re-review): a file of a log-sealed folder deleted (the same generation):
  // the folder changed at the stream's position, so C = L (its derived indexes are rebuilt). The reseal is written
  // under the apply lock (after the journal's recovery) BEFORE any unlink, and the lock is held until the unlinks are
  // done: a crash between them leaves C = L with the file still there (one rebuild for nothing, never a stale
  // index accepted), and no reader sees the folder half way.
  int del_lk = -1;
  struct DelLock { int &fd; ~DelLock() { if(fd >= 0) { flock(fd, LOCK_UN); close(fd); } } } del_lock{del_lk};
  if(!s->sc.commit_block.empty()) {
    std::vector<std::pair<std::string, std::string> > reseal;      // seal file, new body
    std::set<std::string> units;
    for(auto &p : paths)
      if(base_of(p) != s->sc.commit_block && !is_derived(s->sc, p) && !s->staged.count(dir_of(p))) units.insert(dir_of(p));
    for(auto &u : units) {
      std::string key = s->root + "/.gxrepl/units/" + unit_key(u), body = read_small(key);
      char sname[256] = {0}; unsigned long long g = 0, l = 0, c = 0;
      int got = sscanf(body.c_str(), "log %llu %llu %255s %llu", &g, &l, sname, &c);
      if(got < 3 || (got >= 4 && c == l)) continue;
      reseal.push_back(std::make_pair(key, "log " + std::to_string(g) + " " + std::to_string(l) + " " + sname + " " +
				      std::to_string(l) + "\n"));
    }
    if(!reseal.empty()) {
      std::string jname = s->root + "/.gxrepl/apply.jnl";
      del_lk = open(jname.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
      if(del_lk < 0) { err = "cannot lock " + jname; return -1; }
      if(lock_apply_bounded(del_lk) != 0) { close(del_lk); del_lk = -1; err = "busy: " + jname + " is held; tried again next pass"; return -1; }
      if(recover_journal_locked(s->root) != 0) { err = "the apply journal holds a batch that cannot be applied yet"; return -1; }
      for(auto &r : reseal)
	if(write_atomic(r.first, r.second) != 0) { err = "cannot write " + r.first; return -1; }
      sync_dir(s->root + "/.gxrepl/units");
#ifdef GXREPL_TEST_HOOKS
      if(getenv("GXREPL_TEST_DELETE_CRASH")) _exit(9);   // test builds: killed after the reseal, before the unlinks
#endif
    }
  }
  for(auto &p : paths) {
    std::string full;
    if(SafePath(s->sc, p, full, err) != 0) return -1;
    std::string sp = staged_path(s->sc, s->root, p, full, s->staged.count(dir_of(p)) > 0);   // F7: in a stage until it is swapped in
    if(sp != full) {
      if(unlink(sp.c_str()) != 0 && errno != ENOENT) { err = "cannot remove " + p + " from its stage"; return -1; }
      continue;
    }
    if(unlink(full.c_str()) != 0 && errno != ENOENT) { err = "cannot remove " + p; return -1; }
    std::string dir = full.substr(0, full.find_last_of('/'));
    while(dir.size() > s->root.size() && rmdir(dir.c_str()) == 0)
      dir = dir.substr(0, dir.find_last_of('/'));
    dirs.insert(dir.size() >= s->root.size() ? dir : s->root);
  }
  for(auto &d : dirs) sync_dir(d);
  if(del_lk >= 0) { flock(del_lk, LOCK_UN); close(del_lk); del_lk = -1; }   // the unlinks are done and synced
  // 11/13/2026: a unit whose last copied file went (its source folder is gone): the rest of its folder goes too
  // (only on a node that is purely the set's copy: review)
  bool pure_copy = cfg.role == "server" ? s->sc.accept == "target" : s->sc.direction == "pull";
  if(!s->sc.commit_block.empty() && pure_copy) {
    std::set<std::string> units;
    for(auto &p : paths) if(!dir_of(p).empty() && dead_unit_candidate(s->sc, s->root, dir_of(p), false)) units.insert(dir_of(p));
    if(!units.empty()) {
      std::string jname = s->root + "/.gxrepl/apply.jnl";
      int lk = open(jname.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
      if(lk >= 0) {
	struct LockGuard { int fd; ~LockGuard() { flock(fd, LOCK_UN); close(fd); } } lkg{lk};
	if(lock_apply_bounded(lk) == 0 && recover_journal_locked(s->root) == 0) {
	  for(auto &u : units) {
	    std::string what;
	    int rc = sweep_dead_unit(s->sc, s->root, u, true, what);
	    if(rc == 1) { s->staged.erase(u); glog("set %s: %s", s->sc.name.c_str(), what.c_str()); }
	    else if(rc < 0) glog("WARNING set %s: %s", s->sc.name.c_str(), what.c_str());
	  }
	}
      }
    }
  }
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
  // 11/12/2026 (F7): the stages. 11/13 (review): under the apply lock, and never while the set's writer holds its
  // locks (the writer owns the folder: a begun swap is left for gxrepl promote, and shows TORN). A swap that began
  // is finished (it was whole); a stage whose last commit arrived whole is swapped in; a stale one (the unit
  // changed since it began, or a writer took the folder) is discarded; any other waits for its copy.
  for(auto &sc : cfg.sets) {
    std::string base = sc.root + "/.gxrepl/stage";
    if(DIR *d = opendir(base.c_str())) {           // a discard that stopped half way
      std::vector<std::string> trash;
      struct dirent *e;
      while((e = readdir(d)) != 0) if(strncmp(e->d_name, ".trash-", 7) == 0) trash.push_back(base + "/" + e->d_name);
      closedir(d);
      for(auto &t : trash) remove_tree(t);
    }
    std::map<std::string, std::string> st = stages_of(sc.root);
    if(st.empty()) continue;
    WriterHold wh;
    std::string we;
    bool writer = hold_writer_locks(sc, wh, we) != 0;
    std::string jname = sc.root + "/.gxrepl/apply.jnl";
    int lk = open(jname.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if(lk < 0) { err = "cannot lock " + jname; return -1; }
    struct LockGuard { int fd; ~LockGuard() { flock(fd, LOCK_UN); close(fd); } } lkg{lk};
    if(flock(lk, LOCK_EX) != 0) { err = "cannot lock " + jname; return -1; }
    for(auto &e : st) {
      const char *un = e.first.empty() ? "." : e.first.c_str();
      struct stat sb;
      bool swap = stat((e.second + "/.swap").c_str(), &sb) == 0;
      if(writer) {
	if(swap) glog("WARNING set %s: %s: the swap of a staged copy is not finished: %s (promote-check shows it TORN; "
		      "gxrepl promote --apply finishes it once the writer stops)", sc.name.c_str(), un, we.c_str());
	continue;
      }
      std::string e2;
      // 11/13/2026 (F7 review round 3, 1): on a node that is only this set's source (a client that pushes, a
      // server that only serves), a stage is never filled again (one left by a failback pull): discarded
      bool target = cfg.role == "server" ? sc.accept != "source" : sc.direction != "push";
      if(!swap && !target) {
	if(discard_stage(sc.root, e.first) != 0) { err = "set " + sc.name + ": cannot remove " + e.second; return -1; }
	glog("set %s: %s: discarded a stage on this set's source side (left by a failback pull?)", sc.name.c_str(), un);
	continue;
      }
      // 11/13/2026 (F7 review round 2, 3): a stale stage is discarded before anything else: a complete but stale
      // one never goes over the folder's newer data
      if(!swap && !stage_valid(sc, sc.root, e.first)) {
	if(discard_stage(sc.root, e.first) != 0) { err = "set " + sc.name + ": cannot remove " + e.second; return -1; }
	glog("set %s: %s: discarded a stale stage (the unit changed since it began, or a writer took the folder)",
	     sc.name.c_str(), un);
      } else if(swap || stage_complete(sc, e.second, e.first)) {
	if(stage_swap(sc, sc.root, e.first, e2) != 0) { err = "set " + sc.name + ": " + e2; return -1; }
	glog("set %s: %s: finished the swap of a staged copy", sc.name.c_str(), un);
      }
    }
  }
  // 11/13/2026: dead copy folders (their source folder is gone), on a node that copies into the set
  for(auto &sc : cfg.sets) {
    // only a node that is purely the set's copy (review: never accept "both", which a source may also have)
    bool target = cfg.role == "server" ? sc.accept == "target" : sc.direction == "pull";
    if(!target) continue;
    std::vector<std::string> lines;
    gxReplSweepDeadCopies(sc, lines);
    for(auto &l : lines) glog("%s", l.c_str());
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

int gxReplRemote::ReadLogOf(const std::string &set, const std::string &stream_name, const std::string &log_id,
			    unsigned long long from, size_t max_bytes, std::vector<unsigned char> &out,
			    unsigned long long &next, int &status, bool &checked, std::string &err)
{
  // 11/15/2026 (pull-mode F1): the log id goes after the four fields an older server reads (it ignores the rest);
  // a 7.4 server answers a u32 1 after the records when it checked the id
  WBuf w; w.str(set); w.str(stream_name); w.u64(from); w.u64(max_bytes); w.str(log_id);
  std::vector<unsigned char> rep;
  if(call(mkreq(OP_READLOG, w), rep, err)) return -1;
  RBuf r(rep.data(), rep.size());
  status = (int)r.u32(); next = r.u64(); r.bytes(out);
  if(!r.ok) { err = "bad reply"; if(mux) { chan_broken = true; mux->CloseChannel(mux_ch, err); } return -1; }
  checked = r.n - r.off >= 4 && r.u32() == 1;
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
  // 11/16/2026 (beta4 review H1): a verify's end says so after the state; an older server ignores the extra bytes
  if(s.verified_files >= 0) { w.u32(0x56444F4E); w.u32((uint32_t)s.verified_files); }
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
    objsvc->SetVectorLimits(cfg.vector_threads, cfg.vector_max_concurrent);   // 11/14/2026 (7.4)
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

// 11/14/2026 (review 10): a set's derived index state for status.json: ", \"derived\": {...}" ("" without derived)
static void derived_folders(const gxReplSetConfig &sc, const std::string &rel, std::vector<std::string> &out);
static std::string derived_status_json_uncached(const gxReplSetConfig &sc);
// re-review C: only incremental sets (a rebuild set's state is promote-check's), and walked at most every 30 s
static std::string derived_status_json(const gxReplSetConfig &sc)
{
  if(sc.derived.empty() || sc.derived_apply != "incremental") return "";
  static std::mutex m;
  static std::map<std::string, std::pair<long long, std::string> > cache;
  std::lock_guard<std::mutex> g(m);
  long long now = (long long)time(0);
  auto &c = cache[sc.name];
  if(c.second.empty() || now - c.first >= 30) { c.second = derived_status_json_uncached(sc); c.first = now; }
  return c.second;
}

static std::string derived_status_json_uncached(const gxReplSetConfig &sc)
{
  std::vector<std::string> folders;
  derived_folders(sc, "", folders);
  int failed = 0, behind = 0, unstamped = 0;
  for(auto &rel : folders) {
    std::string st;
    if(FILE *f = fopen(derived_state_path(sc, rel).c_str(), "r")) { char b[256] = {0}; if(fgets(b, sizeof(b), f)) {} fclose(f); st = b; }
    if(st.find(" failed") != std::string::npos) failed++;
    else if(st.empty()) behind++;
    if(sc.derived_apply == "incremental") {
      std::string dir = rel.empty() ? sc.root : sc.root + "/" + rel;
      for(auto &ix : folder_indexes(sc, rel)) {
	gxdStampData d;
	if(gxdDerivedStamp::ReadFile(gxdDerivedStamp::PathFor(dir + "/" + ix), d) != 0) { unstamped++; break; }
      }
    }
  }
  std::string why, warn;
  bool brk = breaker_on(sc, &why);
  int crashes = 0;
  { long long now = (long long)time(0), t = 0;
    if(FILE *cf = fopen(crashes_path(sc).c_str(), "r")) { while(fscanf(cf, "%lld", &t) == 1) if(t > now - breaker_window_s && t <= now) crashes++; fclose(cf); } }
  if(sc.derived_apply == "incremental") {
    std::string no;
    if(FILE *f = fopen(notes_off_path(sc).c_str(), "r")) { char b[512] = {0}; if(fgets(b, sizeof(b), f)) {} fclose(f); no = b; }
    while(!no.empty() && no.back() == '\n') no.pop_back();
    if(!no.empty()) warn = "derived_apply = incremental, but the source's object notes are off (commits without objects "
		      "records: every batch is rebuilt; turn them on in the writer): " + no;
    else if(brk) warn = "the breaker is on: " + why;
  }
  return std::string(", \"derived\": {\"mode\": ") + json_str(sc.derived_apply) + ", \"folders\": " +
    std::to_string(folders.size()) + ", \"failed\": " + std::to_string(failed) + ", \"not_built\": " +
    std::to_string(behind) + ", \"rebuild_needed\": " + std::to_string(unstamped) + ", \"breaker\": " +
    (brk ? "true" : "false") + ", \"crashes_1h\": " + std::to_string(crashes) + ", \"warn\": " + json_str(warn) + "}";
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
      ", \"root\": " + json_str(sc.root) + ", \"last\": " + json_str(set_last[sc.name]) +
      derived_status_json(sc) + "}";
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
    std::map<std::string, long long> swept;   // 11/13/2026: the dead copy folder sweep, once a minute a set
    while(!stop) {
      for(auto &sc : cfg.sets) {
	if(stop || sc.accept == "source") continue;
	if(!sc.commit_block.empty() && sc.accept == "target" && (long long)time(0) - swept[sc.name] >= sweep_every()) {
	  swept[sc.name] = (long long)time(0);
	  std::vector<std::string> lines;
	  gxReplSweepDeadCopies(sc, lines);
	  for(auto &l : lines) glog("%s", l.c_str());
	}
	if(sc.derived.empty()) continue;
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
      // 11/15/2026 (pull-mode F1): a 7.4 client sends the copy's log id after these: read only that log
      bool with_id = r.ok && r.n - r.off >= 4;
      std::string lid = with_id ? r.str() : std::string();
      if(with_id && !r.ok) { e = "bad request"; rc = -1; break; }
      if(mx > cfg.max_message) mx = cfg.max_message;
      std::vector<unsigned char> out; unsigned long long next = 0; int status = 0;
      if(!role_ok(set, false) ||
	 (with_id ? local.ReadLogOf(set, st, lid, from, (size_t)mx, out, next, status, e)
		  : local.ReadLog(set, st, from, (size_t)mx, out, next, status, e)) != 0) { rc = -1; break; }
      w.u32((uint32_t)status); w.u64(next); w.bytes(out);
      if(with_id) w.u32(1);                             // checked
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
      if(r.ok && r.n - r.off >= 8) {                   // 11/16/2026 (H1): "VDON" N, a verify done with N files
	uint32_t mg = r.u32(), nf = r.u32();
	if(mg == 0x56444F4E && nf <= 0x7FFFFFFF) st.verified_files = (int)nf;
      }
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
      std::to_string(st.ship_log_bytes) + ", \"warn\": " + json_str(st.warn) + ", \"waiting\": " + json_str(st.waiting) +
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

// 11/14/2026: test builds only: switch one part of the drain-verify fix off (the tests prove each part alone)
static bool test_off(const char *name)
{
#ifdef GXREPL_TEST_HOOKS
  return getenv(name) != 0;
#else
  (void)name;
  return false;
#endif
}

int gxReplClient::drain(gxReplSource &src, gxReplTarget &tgt, const std::string &set,
			const gxReplStream &s, gxReplStreamState &ts,
			gxReplSetStatus &st, bool &verify, std::string &err, unsigned long long *flag_lsn)
{
  // 11/14/2026 (review): the highest record lsn that set "verify" (ULLONG_MAX: the copy's position is gone)
  if(flag_lsn) *flag_lsn = 0;
  for(int rounds = 0; rounds < 100000 && !stop; rounds++) {
    std::vector<unsigned char> out;
    unsigned long long next = 0;
    int status = 0;
    // 11/14/2026 (review F1): a local source reads only the log the copy's position is on (ts.log_id): a log made
    // again (the reader reopens on it) answers "position gone", never its records as if they continued the old one.
    // A remote source (pull) checks its own log id on the server side as before.
    int rrc = 0;
    if(static_cast<gxReplSource *>(&local) == &src)
      rrc = local.ReadLogOf(set, s.name, ts.log_id, ts.lsn + 1, cfg.max_message, out, next, status, err);
    else if(gxReplRemote *rs = dynamic_cast<gxReplRemote *>(&src)) {
#ifdef GXREPL_TEST_HOOKS
      // test builds: GXREPL_TEST_READLOG_HOLD=F: once F.arm exists, the next remote read writes F.held and waits
      // (60 s at most) for F, so a test can make the source's log again between the pass's listing and the read
      if(const char *h = getenv("GXREPL_TEST_READLOG_HOLD")) {
	static std::atomic<int> once(0);
	struct stat ast;
	if(stat((std::string(h) + ".arm").c_str(), &ast) == 0 && once.exchange(1) == 0) {
	  write_atomic(std::string(h) + ".held", "held\n");
	  for(int k = 0; k < 600 && access(h, F_OK) != 0; k++) usleep(100000);
	}
      }
#endif
      // 11/15/2026 (pull-mode F1): the remote read says which log the copy is on; an older server does not check
      // it, so then the stream's log id is read again after the records: another log, and they are not used
      // (status 1: the copy's position is gone, a verify)
      bool checked = false;
      rrc = rs->ReadLogOf(set, s.name, ts.log_id, ts.lsn + 1, cfg.max_message, out, next, status, checked, err);
      if(rrc == 0 && !checked && status == 0 && !out.empty() && !ts.log_id.empty()) {
	std::vector<gxReplStream> sv;
	if(src.Streams(set, sv, err) != 0) return -1;
	for(auto &x : sv)
	  if(x.name == s.name && x.log_id != ts.log_id) {
	    glog("set %s: stream %s: the source (an older gxrepl) now has log %s, the copy is on %s: not used",
		 set.c_str(), s.name.c_str(), x.log_id.c_str(), ts.log_id.c_str());
	    out.clear(); status = 1;
	  }
      }
    }
    else rrc = src.ReadLog(set, s.name, ts.lsn + 1, cfg.max_message, out, next, status, err);
    if(rrc != 0)
      return -1;
#ifdef GXREPL_TEST_HOOKS
    // test builds: GXREPL_TEST_STATUS2_ALWAYS=1: every read with records answers "bigger than max_message"
    if(status == 0 && !out.empty() && getenv("GXREPL_TEST_STATUS2_ALWAYS")) { out.clear(); status = 2; }
#endif
    if(status == 0 && !out.empty()) big_resyncs.erase(set + "\n" + s.name);   // records drained: the bound starts over
    if(status == 1) {
      verify = true;
      if(flag_lsn) *flag_lsn = ULLONG_MAX;
      // 11/14/2026 (review): the copy's next records are gone from the log: its position is reset as for a record
      // bigger than a message (status 2 below), so a file sync goes on by pieces and the stream's verify completes
      // (before, a file sync inside the verify read "apply the log first" again on every try and gave up)
      {
	gxReplStreamState rs;
	rs.name = s.name; rs.log_id = "resync"; rs.lsn = 0;
	if(tgt.SetState(set, rs, err) != 0) return -1;
	ts = rs;
      }
      // (review): ULLONG_MAX: always, even during the stream's own verify
      if(!test_off("GXREPL_TEST_NO_DRAIN_VERIFY")) note_verify(set, s.name, "the copy's position is gone from the log", ULLONG_MAX);
      return 0;
    }
    if(status == 2) {
      // 11/15/2026 (hardening): the status-2 bound. Resyncs with no record drained since are counted per stream;
      // past GXREPL_STATUS2_MAX (10) the stream is not resynced again (one ALERT, the set's last_error says why)
      // until a record drains: a copy that cannot get past a record never loops on whole-file resyncs.
      {
	int &n = big_resyncs[set + "\n" + s.name];
	int maxn = 10;
	if(const char *m = getenv("GXREPL_STATUS2_MAX")) maxn = std::max(1, atoi(m));
	if(++n > maxn) {
	  std::string why = "stream " + s.name + ": " + std::to_string(n - 1) + " resyncs for records bigger than max_message (" +
	    std::to_string(cfg.max_message) + ") and none drained since: not resynced again (raise max_message on both nodes)";
	  if(n == maxn + 1) glog("ALERT set %s: %s", set.c_str(), why.c_str());
	  { std::lock_guard<std::mutex> lg(st.m); st.last_error = why; }
	  return 0;
	}
      }
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
      // 11/15/2026 (F3): through note_verify(), always (ULLONG_MAX), as status 1: during the stream's own verify the
      // stream was already on the list and no second verify was asked for; and the caller's flag_lsn is set
      if(flag_lsn) *flag_lsn = ULLONG_MAX;
      if(!test_off("GXREPL_TEST_NO_DRAIN_VERIFY")) note_verify(set, s.name, "a record bigger than max_message", ULLONG_MAX);
      return 0;
    }
    if(out.empty()) return 0;
    std::vector<gxdShipRecordView> recs;
    if(gxdParseShipRecords(out.data(), out.size(), recs) != 0) { err = "damaged log records"; return -1; }
    for(auto &r : recs) {
      // 11/14/2026: the stream goes on the verify list here, whoever called drain() (sync_file()'s drains
      // used to drop the flag: a writer's open after a journal recovery never got its verify)
      if(r.type == gxdShipOpen || r.type == gxdShipGap) {
	verify = true;
	if(flag_lsn && r.lsn > *flag_lsn) *flag_lsn = r.lsn;
	if(!test_off("GXREPL_TEST_NO_DRAIN_VERIFY"))
	  note_verify(set, s.name, r.type == gxdShipOpen ? "a writer's open record" : "a gap record", r.lsn, s.log_id);
      }
    }
    unsigned long long lsn = 0;
    if(tgt.ApplyLog(set, s.name, s.log_id, out, lsn, err) != 0) {
      // 11/15/2026 (the late-pass hollow seal, b): the copy is not in step with these records (a file they would
      // only part build): reset its position and verify the stream, as for a position gone from the log
      if(err.compare(0, 12, "not in step:") != 0) return -1;
      glog("set %s: stream %s: %s", set.c_str(), s.name.c_str(), err.c_str());
      err.clear();
      gxReplStreamState rs;
      rs.name = s.name; rs.log_id = "resync"; rs.lsn = 0;
      if(tgt.SetState(set, rs, err) != 0) return -1;
      ts = rs;
      verify = true;
      if(flag_lsn) *flag_lsn = ULLONG_MAX;
      note_verify(set, s.name, "the copy is not in step with the records", ULLONG_MAX);
      return 0;
    }
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

// 11/14/2026: a drain inside sync_file() can apply a writer's open or gap record (or find the copy's position
// gone): the stream still needs its verify. An open record says the files may have changed with no record (the
// new writer's journal recovery replays a batch its killed writer committed to the journal but never shipped).
// Before this, those drains dropped the flag, the verify never ran and the copy kept stale pages at the source's
// generation.
// A stream already on the list and being verified right now (a drain inside its own verify's file syncs) is
// verified once more after this one: files compared earlier in this verify may predate the recovery.
// 11/15/2026 (F3): ULLONG_MAX as an lsn means "always" (a verify whatever its position): said so in log lines
static std::string lsn_text(unsigned long long l) { return l == ULLONG_MAX ? std::string("always") : std::to_string(l); }

void gxReplClient::note_verify(const std::string &set, const std::string &stream, const char *why,
			       unsigned long long lsn, const std::string &log_id)
{
  auto &vl = verify_streams[set];
  if(std::find(vl.begin(), vl.end(), stream) == vl.end()) {
    glog("set %s: stream %s needs a verify (%s)", set.c_str(), stream.c_str(), why);
    vl.push_back(stream);
  }
  // 11/14/2026 (review): during the stream's own verify, the records up to the position it started from are the
  // ones it is catching up on (a burst of writer restarts): only a record past it asks for another verify
  // (review): lsn 0 = not known, and a record of another log (remade mid-verify, lsns from 1 again): always
  else if(verifying[set] == stream &&
	  (lsn == 0 || log_id.empty() || log_id != verify_log_id[set] || lsn > verify_from[set]) &&
	  verify_again[set].insert(stream).second)
    glog("set %s: stream %s needs a verify again after this one (%s at record %s)", set.c_str(), stream.c_str(), why,
	 lsn_text(lsn).c_str());
}

int gxReplClient::sync_file(gxReplSource &src, gxReplTarget &tgt, const std::string &set,
			    const std::string &path, const gxReplStream *cover,
			    gxReplSetStatus &st, std::string &err)
{
#ifdef GXREPL_TEST_HOOKS
  // test builds: GXREPL_TEST_SYNC_FILE_HOLD=F: the first file sync of a covered file writes F.held and waits (60 s
  // at most) for F, so a test can make the writer's records arrive while it runs (the drain-verify test)
  static std::atomic<int> sync_held(0);
  // (GXREPL_TEST_SYNC_FILE_HOLD_ON=NAME: only the sync of the file whose name ends with NAME)
  const char *hold_on = getenv("GXREPL_TEST_SYNC_FILE_HOLD_ON");
  bool hold_this = !hold_on || (path.size() >= strlen(hold_on) && path.compare(path.size() - strlen(hold_on), std::string::npos, hold_on) == 0);
  if(cover && hold_this) if(const char *hold = getenv("GXREPL_TEST_SYNC_FILE_HOLD")) if(sync_held.exchange(1) == 0) {
    write_atomic(std::string(hold) + ".held", "held\n");
    for(int w = 0; w < 600 && access(hold, F_OK) != 0 && !stop; w++) usleep(100000);
  }
#endif
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
      // 11/15/2026: a file the ship log covers gone while it is synced (its folder archived and reopened at once, as
      // osa_ingest's late pass does) is not "synced": the verify fails for it, so the folder's commit block is not
      // sent in this pass and the stream is verified again (before: the later files' updates moved the copy to the
      // new log's position, the commit block followed, and the copy was "in step" with a stale data file)
      if(res.status == 1) {
	// (review F1): a commit block the log no longer carries is answered "gone" on purpose (the copy keeps its own
	// and its seal): synced, as before. Only a data file that really vanished fails.
	const gxReplSetConfig *scg = cover ? cfg.Set(set) : 0;
	bool is_cb = scg && !scg->commit_block.empty() && base_of(path) == scg->commit_block;
	if(cover && !is_cb) { err = path + " was gone while it was synced (verified again next pass)"; return 1; }
	return 0;                   // Gone from the source
      }
      if(res.status == 3) { retry = true; usleep(200000); break; } // 11/08/2026: changed while read
      if(res.status == 2 || res.update.stream.empty() != (cover == 0)) {
	if(cover && cs.log_id == cover->log_id) {
	  bool v = false;
	  unsigned long long vl = 0;
	  if(drain(src, tgt, set, *cover, cs, st, v, err, &vl) != 0) return -1;
	  if(v && !test_off("GXREPL_TEST_NO_CALLER_VERIFY"))
	    note_verify(set, cover->name, "an open or gap record drained by a file sync", vl, cover->log_id);
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
      // 11/15/2026: the copy's position would move to another log here (the stream's log made again while its files
      // were synced: osa_ingest's late pass archives a day, purges its log and reopens it at once). The records of the
      // old log that the files synced before this one still lack can never come from the new log: the stream is
      // verified again, and the seal guard: the folder's commit block is never applied across such a jump, so the
      // copy is never sealed or "in step" with a stale data file (before: 1 night in 3 in the night-1 test, .53 too)
      // (not when this is the stream's own verify of that very log: moving the copy onto it is what the verify does)
      bool own_log = verifying[set] == (cover ? cover->name : std::string()) && res.update.log_id == verify_log_id[set];
      if(cover && !own_log && !cs.log_id.empty() && cs.log_id != "resync" && !res.update.log_id.empty() &&
	 cs.log_id != res.update.log_id && !test_off("GXREPL_TEST_NO_LOGJUMP_REQUEUE")) {
	note_verify(set, cover->name, "the copy's position moved to another log during a file sync", 0, res.update.log_id);
	const gxReplSetConfig *scp = cfg.Set(set);
	if(scp && !scp->commit_block.empty() && base_of(path) == scp->commit_block) {
	  err = path + ": the commit block is not applied across a log made again (verified again)";
	  return 2;
	}
      }
      // 11/15/2026 (the seal guard): the folder's commit block is applied only when its stream needs no other
      // verify: queued to verify again (above, or an open record during this verify), or queued while another
      // pass is not verifying it: the copy stays unsealed (never "in step" with a stale data file) until it is
      if(cover) {
	const gxReplSetConfig *scb = cfg.Set(set);
	if(scb && !scb->commit_block.empty() && base_of(path) == scb->commit_block) {
	  const auto &vl = verify_streams[set];
	  bool queued = std::find(vl.begin(), vl.end(), cover->name) != vl.end();
	  // (also on its own: during the stream's verify, a commit block of another log than the one the verify started
	  // on: the files synced before it may be the old log's)
	  bool other_log = verifying[set] == cover->name && !verify_log_id[set].empty() && !res.update.log_id.empty() &&
	    res.update.log_id != verify_log_id[set];
	  if(!test_off("GXREPL_TEST_NO_SEAL_GUARD") &&
	     (verify_again[set].count(cover->name) || (queued && verifying[set] != cover->name) || other_log)) {
	    err = path + ": the commit block waits for the stream's verify";
	    return 2;               // (review) expected: not an error (no last_error, no log line)
	  }
	}
      }
      std::vector<gxReplFileUpdate> ups(1, res.update);
      std::vector<int> results;
      if(tgt.ApplyFiles(set, ups, results, err) != 0) return -1;
      if(results.empty() || results[0] != 0) {
	// Stale: the copy is not at the stream position of the pieces
	if(cover && cs.log_id == cover->log_id) {
	  bool v = false;
	  unsigned long long vl = 0;
	  if(drain(src, tgt, set, *cover, cs, st, v, err, &vl) != 0) return -1;
	  if(v && !test_off("GXREPL_TEST_NO_CALLER_VERIFY"))
	    note_verify(set, cover->name, "an open or gap record drained by a file sync", vl, cover->log_id);
	}
	retry = true;
	break;
      }
      // 11/14/2026: the update carried the log records from the copy's position to the pieces' (Diff()); an open or
      // gap record among them asks for a verify, as in drain() (a writer's open after a journal recovery: the
      // recovered pages have no record, only the verify brings them)
      // 11/14/2026 (review): the log made again during the stream's own verify (a new log id in this update): files
      // compared earlier in the verify belong to the old log; verify again
      if(cover && verifying[set] == cover->name && !verify_log_id[set].empty() && !res.update.log_id.empty() &&
	 res.update.log_id != verify_log_id[set])
	note_verify(set, cover->name, "the stream's log was made again during its verify", 0, res.update.log_id);
      if(cover && !res.update.records.empty() && !test_off("GXREPL_TEST_NO_CALLER_VERIFY")) {
	std::vector<gxdShipRecordView> rv;
	if(gxdParseShipRecords(res.update.records.data(), res.update.records.size(), rv) == 0) {
	  for(auto &r : rv)
	    if((r.type == gxdShipOpen || r.type == gxdShipGap) &&
	       (verifying[set] != cover->name || res.update.log_id != verify_log_id[set] || r.lsn > verify_from[set])) {
	      note_verify(set, cover->name, r.type == gxdShipOpen ? "a writer's open record in a file update" : "a gap record in a file update",
			  r.lsn, res.update.log_id);
	      break;
	    }
	}
	// (by design: during the stream's own verify, an update at or below the position it started from is ignored)
	else note_verify(set, cover->name, "a file update's records could not be read", res.update.lsn, res.update.log_id);
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
  { std::lock_guard<std::mutex> lg(st.m); st.waiting.clear(); }   // 11/15/2026: set again by this pass's waits
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
  std::map<std::string, int> reverified;      // 11/14/2026: verified again in this pass (new files)
  for(auto it = vlist.begin(); it != vlist.end() && !stop; ) {
    const gxReplStream *s = 0;
    for(auto &x : streams) if(x.name == *it) s = &x;
    if(!s) { verify_again[sc.name].erase(*it); it = vlist.erase(it); continue; }   // (review: no stale flag)
#ifdef GXREPL_TEST_HOOKS
    // test builds: GXREPL_TEST_VERIFY_WAIT=F: a queued stream verify waits (the pass goes on) until F exists
    if(const char *vw = getenv("GXREPL_TEST_VERIFY_WAIT")) { struct stat vst; if(stat(vw, &vst) != 0) { ++it; continue; } }
#endif
    if(!have_sm) { if(src.Manifest(sc.name, sm, err) != 0) return -1; have_sm = true; }
    verifying[sc.name] = s->name;                 // 11/14/2026: see note_verify()
    verify_again[sc.name].erase(s->name);         // 11/15/2026: this verify is the one asked for
    verify_from[sc.name] = s->next_lsn ? s->next_lsn - 1 : 0;
    verify_log_id[sc.name] = s->log_id;
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
      if(rc == 2) { failed++; std::lock_guard<std::mutex> lg(st.m); st.waiting = "waiting for verify: " + s->name; }   // 11/15/2026: expected
      else if(rc > 0) { failed++; { std::lock_guard<std::mutex> lg(st.m); st.last_error = e; } glog("set %s: %s", sc.name.c_str(), e.c_str()); }
      else synced++;
    }
    if(failed) { ++it; continue; }
    // 11/15/2026 (the late-pass hollow seal, a): a new log names its files only with its first commit. A verify that
    // found none while the stream's folder holds database files on the source (a day reopened by a plain copy:
    // events.gxd is there, the new log does not list it yet) is not done: taking the log's end then let the next
    // records build that file from their pages alone on a copy that does not have it (a hollow file, sealed whole).
    // The stream stays on the list until its files are listed.
    if(synced == 0 && !s->journal.empty()) {
      std::string folder = dir_of(s->journal), have;
      for(auto &m : sm) {
	if(dir_of(m.path) != folder || m.path == s->journal || !is_db_file(sc, m.path) || excluded(sc, m.path) ||
	   is_derived(sc, m.path)) continue;
	have = m.path;
	break;
      }
      if(!have.empty()) {
	// 11/16/2026 (beta4 review): said once per log, not every pass
	if(files_wait_noted.insert(sc.name + "|" + s->name + "|" + s->log_id).second)
	  glog("set %s: stream %s lists no file yet, and its folder has %s: verified when it lists its files",
	       sc.name.c_str(), s->name.c_str(), have.c_str());
	verifying[sc.name].clear();
	++it;
	continue;
      }
    }
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
#ifdef GXREPL_TEST_HOOKS
    // test builds: GXREPL_TEST_VERIFY_HOLD=F: after a stream's verify, write F.held and wait (60 s at most) for F
    if(const char *hold = getenv("GXREPL_TEST_VERIFY_HOLD")) {
      write_atomic(std::string(hold) + ".held", "held\n");
      for(int w = 0; w < 600 && access(hold, F_OK) != 0 && !stop; w++) usleep(100000);
    }
#endif
    // 11/14/2026 (the verify race): the stream's file list was read once at the start of the pass; a writer whose
    // next record names a new file (its first record named only the data file, the next adds the commit block)
    // leaves that file unverified, and its unit "copying" until the next verify (the records that carry the commit
    // block never clear a fill's mark). So the list is read again: the same log with new files is verified again
    // in this pass (3 times at most), else it waits for the next pass.
    {
      int &again = reverified[sc.name + "\n" + s->name];
      std::vector<gxReplStream> now_streams;
      std::string e2;
      bool redo = false;
      if(again < 3 && src.Streams(sc.name, now_streams, e2) == 0)
	for(auto &x : now_streams)
	  if(x.name == s->name && x.log_id == s->log_id)
	    for(auto &f : x.files)
	      if(std::find(s->files.begin(), s->files.end(), f) == s->files.end()) { redo = true; break; }
      if(redo) {
	for(auto &x : now_streams)
	  if(x.name == s->name)
	    for(auto &y : streams) if(y.name == x.name) y = x;
	have_sm = false;                              // the source's manifest again too: the new file is in it
	again++;
	glog("set %s: stream %s names new file(s) since the pass began: verified again", sc.name.c_str(), s->name.c_str());
	continue;
      }
      reverified.erase(sc.name + "\n" + s->name);
    }
    verifying[sc.name].clear();
    // 11/14/2026: an open or gap record drained during this verify: the stream stays on the list (next pass)
    if(verify_again[sc.name].erase(s->name)) { ++it; continue; }
    // 11/15/2026 (the recovery gap, review 2): the verify is done: the copy's own position back to it, which tells
    // a 7.4 copy that the stream's hold may go (an older copy writes the same position again)
    {
      auto tsx = tstates.find(s->name);
      if(tsx != tstates.end()) {
	// 11/16/2026 (beta4 review H1): with the number of files the verify compared (the hold goes only after > 0)
	gxReplStreamState done = tsx->second;
	done.verified_files = synced;
#ifdef GXREPL_TEST_HOOKS
	if(getenv("GXREPL_TEST_OLD_VERIFY_DONE")) done.verified_files = -1;   // test builds: as a 7.4 beta3 client
#endif
	if(tgt.SetState(sc.name, done, err) != 0) return -1;
      }
    }
    it = vlist.erase(it);
  }
  verifying[sc.name].clear();

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
      if(rc == 2) { std::lock_guard<std::mutex> lg(st.m); st.waiting = "waiting for verify: " + cover->name; continue; }
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
      if(rc == 2) { std::lock_guard<std::mutex> lg(st.m); st.waiting = "waiting for verify: " + cv->second->name; continue; }
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
// 11/14/2026 (seal per folder): the last ship log record that changed the folder (the seal's 4th field; a seal
// from before it: its lsn), 0 if the folder is not sealed by the log. Its derived indexes are current when they
// were built at this record, whatever records of the stream's other folders came after.
static unsigned long long unit_changed_lsn(const gxReplSetConfig &sc, const std::string &rel)
{
  FILE *f = fopen((sc.root + "/.gxrepl/units/" + unit_key(rel)).c_str(), "rb");
  if(!f) return 0;
  unsigned long long g = 0, l = 0, c = 0;
  char sname[256] = {0};
  int got = fscanf(f, "log %llu %llu %255s %llu", &g, &l, sname, &c);
  fclose(f);
  if(got < 2) return 0;
  return got >= 4 ? c : l;
}

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
  unsigned long long sl = unit_changed_lsn(sc, rel);   // 11/14/2026 (seal per folder): not the stream's position
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
  while(d && (e = readdir(d)) != 0) if(e->d_name[0] != '.' && is_derived_index(sc, rel.empty() ? std::string(e->d_name) : rel + "/" + e->d_name)) found++;
  if(d) closedir(d);
  if(!found) { why = "missing (no derived file in the folder)"; return 1; }
  if(derived_stamp_ok(sc, rel, gen, why)) return 0;
  // a stamp that does not say this generation / record is behind; one that is
  // damaged or holds the wrong key count is a bad rebuild
  return (why.compare(0, 10, "stamped at") == 0 || why.compare(0, 11, "not stamped") == 0) ? 1 : 2;
}

// 11/14/2026: the derived indexes of a folder whose <index>.stamp is missing or damaged
static void derived_stamp_gaps(const gxReplSetConfig &sc, const std::string &rel, std::vector<std::string> &out)
{
  std::string dir = rel.empty() ? sc.root : sc.root + "/" + rel;
  DIR *d = opendir(dir.c_str());
  struct dirent *e;
  while(d && (e = readdir(d)) != 0) {
    std::string n = e->d_name;
    if(n[0] == '.' || !is_derived_index(sc, rel.empty() ? n : rel + "/" + n)) continue;
    gxdStampData sd;
    int rc = gxdDerivedStamp::ReadFile(gxdDerivedStamp::PathFor(dir + "/" + n), sd);
    if(rc == 1) out.push_back(n + " has no stamp");
    else if(rc != 0) out.push_back(n + " has a damaged stamp");
  }
  if(d) closedir(d);
  std::sort(out.begin(), out.end());
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
    struct timespec rb_t0; clock_gettime(CLOCK_MONOTONIC, &rb_t0);   // 11/14/2026: GXREPL_STATS_FILE
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
    std::string ls = std::to_string(unit_changed_lsn(sc, rel));   // 11/14/2026 (seal per folder)
    for(size_t at; (at = cmd.find("{lsn}")) != std::string::npos; ) cmd.replace(at, 5, ls);
    int rc = run_quiet(cmd);
    // 11/14/2026: an incremental set's rebuilt folder gets fresh stamps, under the same lock
    if(rc == 0 && sc.derived_apply == "incremental") {
      std::string sw;
      drop_stamps(sc, rel, "", false);
      if(stamp_after_rebuild(sc, rel, sw) != 0)
	log.push_back("set " + sc.name + ": " + (rel.empty() ? std::string(".") : rel) + ": rebuilt, not stamped: " + sw);
    }
    stats_line("rebuild", sc.name, ms_since(rb_t0), rel.empty() ? std::string(".") : rel);
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

// 11/14/2026: the timed re-reads of an unlocked promote-check (GXREPL_TORN_RECHECK_SEC, 10 s)
static double torn_recheck_sec()
{
  double wait = 10;
  if(const char *e = getenv("GXREPL_TORN_RECHECK_SEC")) wait = atof(e);
  return wait;
}

// 11/14/2026: promote-check's shared lock on a set's apply.jnl, taken per unit. 11/14 (a promote-check rc 1 under
// load): a unit's commit block, its seal, its stream's applied position and its derived state are separate files;
// read unlocked in the middle of an apply they disagree ("sealed at ship log record 77, the copy applied up to 76")
// and the timed re-reads (10 s) did not always outlast an apply's position write under heavy load. Every apply,
// staged swap, derived rebuild and dead folder sweep holds the lock exclusively, so under the shared lock nothing
// is half applied and no re-read is needed. It is taken for one unit's small reads only and let go before the next
// unit (and before any file is hashed): an apply waits (its F4 bounded wait, 5 s) for one unit's reads at most. The
// waits share one budget per set, GXREPL_TORN_LOCK_WAIT (15 s): a derived rebuild holding the lock for minutes
// stops the locking for the rest of the set, which is then read unlocked with the timed re-reads, and said.
// No apply.jnl (or no .gxrepl): nothing was ever applied there; read unlocked (with the re-reads), nothing made.
struct TornLock {
  int fd = -1;
  bool held = false;
  std::string jname;
  gxReplTornLockStats *ls = 0;
  double budget_ms = 60000;                // the waits' budget (the walk's own time does not count)
  std::chrono::steady_clock::time_point t0;

  TornLock(const std::string &root, gxReplTornLockStats &st) : ls(&st) {
    jname = root + "/.gxrepl/apply.jnl";
    if(!st.use_lock) return;
    double wait = 15;   // 11/14/2026: 15 s, so promote (480 s) + this stays well inside the DRCM gate's 600 s
    if(const char *e = getenv("GXREPL_TORN_LOCK_WAIT")) { double v = atof(e); if(v >= 0) wait = v; }
    budget_ms = wait * 1000;
    fd = open(jname.c_str(), O_RDONLY | O_CLOEXEC);
    if(fd < 0 && errno != ENOENT && errno != ENOTDIR) st.fell_back = true;   // there and not readable: said
  }
  ~TornLock() { release(); if(fd >= 0) close(fd); }
  // true: held (no re-reads needed); false: read unlocked, with the timed re-reads
  bool acquire() {
    if(fd < 0) { if(ls->fell_back) ls->unlocked_units++; return false; }
    if(ls->fell_back) { ls->unlocked_units++; return false; }
    auto w0 = std::chrono::steady_clock::now();
    auto end = w0 + std::chrono::microseconds((long long)(std::max(0.0, budget_ms - ls->waited_ms) * 1000));
    while(flock(fd, LOCK_SH | LOCK_NB) != 0) {
      auto now = std::chrono::steady_clock::now();
      if((errno != EWOULDBLOCK && errno != EINTR) || now >= end) {
	ls->waited_ms += std::chrono::duration<double, std::milli>(now - w0).count();
	ls->fell_back = true;
	ls->unlocked_units++;
	return false;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    t0 = std::chrono::steady_clock::now();
    ls->waited_ms += std::chrono::duration<double, std::milli>(t0 - w0).count();
    ls->acquisitions++;
    held = true;
    return true;
  }
  void release() {
    if(!held) return;
    flock(fd, LOCK_UN);
    held = false;
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    ls->total_hold_ms += ms;
    if(ms > ls->max_hold_ms) ls->max_hold_ms = ms;
  }
};

static bool read_cb_gen(const gxReplSetConfig &sc, const std::string &dir, uint64_t &g)
{
  int fd = open((dir + "/" + sc.commit_block).c_str(), O_RDONLY | O_CLOEXEC);
  ssize_t n = fd >= 0 ? pread(fd, &g, sizeof(g), (off_t)sc.commit_gen_offset) : -1;
  if(fd >= 0) close(fd);
  return n == (ssize_t)sizeof(g);
}

// 11/10/2026: see gxrepl.h
static void torn_walk(const gxReplSetConfig &sc, const std::string &root, const std::string &rel,
		      int &checked, std::vector<std::string> &torn, std::vector<std::string> &warn, TornLock &tl)
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
      // 11/14/2026: this unit's small reads under the shared apply lock (no re-reads then: nothing is in flight)
      bool locked = tl.acquire();
      struct Rel { TornLock &t; ~Rel() { t.release(); } } rel_guard{tl};
      const double recheck = locked ? 0 : torn_recheck_sec();
      uint64_t g = 0;
      bool got = read_cb_gen(sc, dir, g);
      // 11/12/2026 (F7): a staged swap marks the live commit block odd for its renames (milliseconds, in the apply
      // lock): an odd unit whose stage has its .swap list is read again for up to GXREPL_TORN_RECHECK_SEC (10) s;
      // a swap that stopped stays odd and is reported torn (11/14: under the lock a .swap is always a stopped one)
      if(got && (g & 1)) {
	struct stat sst;
	if(stat((stage_dir(root, rel) + "/.swap").c_str(), &sst) == 0) {
	  auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds((long)(recheck * 1000));
	  while((g & 1) && std::chrono::steady_clock::now() < end) {
	    std::this_thread::sleep_for(std::chrono::milliseconds(100));
	    uint64_t g2 = g;
	    if(read_cb_gen(sc, dir, g2)) g = g2;
	  }
	}
      }
      if(!got) torn.push_back(where + ": " + sc.commit_block + " is short");
      else if(g & 1) torn.push_back(where + ": generation " + std::to_string((unsigned long long)g) +
				    " is odd (a copy in progress, or one that stopped half way)");
      else {
	// 11/10/2026: an even generation is not enough: the files must be the sealed ones. 11/14: a "gen G" seal's
	// hashes are left for after the lock ("deferred")
	std::string deferred;
	std::string why = unit_seal_check(sc, rel, g, &deferred);
	// 11/12/2026: the commit block, the seal and its stream's applied position are three files: read in the middle
	// of an apply they can disagree by one record or generation ("sealed at ship log record L, the copy applied up
	// to L-1", "sealed by ship log record L at generation G, the commit block says G-n"). That is an apply in
	// flight, not a torn copy: read them all again for up to GXREPL_TORN_RECHECK_SEC (10) s; a real mismatch stays
	// and is reported. (11/14: unlocked only; under the lock what is read is what is there)
	auto in_flight = [](const std::string &w) { return w.compare(0, 7, "sealed ") == 0; };
	if(in_flight(why)) {
	  auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds((long)(recheck * 1000));
	  while(in_flight(why) && std::chrono::steady_clock::now() < end) {
	    std::this_thread::sleep_for(std::chrono::milliseconds(100));
	    uint64_t g2 = 0;
	    if(!read_cb_gen(sc, dir, g2) || (g2 & 1)) continue;
	    g = g2;
	    why = unit_seal_check(sc, rel, g, &deferred);
	  }
	}
	// 11/13/2026 (F7 review round 3, 6): files that do not match the seal while the generation moved during the
	// check (a swap or an apply in between, no lock taken): read again for up to GXREPL_TORN_RECHECK_SEC; only a
	// mismatch at an unchanged generation is reported. 11/14: the hashing is never under the lock, so a deferred
	// hash check that fails is read again this way (unlocked) too.
	auto moved_recheck = [&](double wait) {
	  auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds((long)(wait * 1000));
	  while(!why.empty() && std::chrono::steady_clock::now() < end) {
	    uint64_t g3 = 0;
	    bool ok3 = read_cb_gen(sc, dir, g3);
	    if(ok3 && g3 == g) break;                                  // the same generation: a real mismatch
	    std::this_thread::sleep_for(std::chrono::milliseconds(100));
	    if(!ok3 || (g3 & 1)) continue;
	    g = g3;
	    why = unit_seal_check(sc, rel, g);
	  }
	};
	if(!why.empty()) moved_recheck(recheck);
	// the derived state (small files) while still locked, for this generation
	int ds = 0;
	std::string dwhy;
	if(why.empty() && !sc.derived.empty()) ds = derived_status(sc, rel, g, dwhy);
	std::vector<std::string> nostamp;                              // 11/14/2026: the stamps too, under the lock
	if(why.empty() && sc.derived_apply == "incremental") derived_stamp_gaps(sc, rel, nostamp);
	tl.release();                                                  // nothing slow under the lock
	if(!deferred.empty() && why.empty()) {
	  uint64_t g0 = g;
	  why = unit_seal_hash_check(sc, rel, g, deferred);
	  if(!why.empty()) moved_recheck(torn_recheck_sec());
	  if(why.empty() && g != g0 && !sc.derived.empty()) ds = derived_status(sc, rel, g, dwhy);
	  if(why.empty() && g != g0 && sc.derived_apply == "incremental") { nostamp.clear(); derived_stamp_gaps(sc, rel, nostamp); }   // A3
	}
	if(!why.empty()) torn.push_back(where + ": " + why);
	// and its derived indexes: a bad rebuild is torn (a take over's would
	// fail too); indexes behind a busy day are a warning: the take over
	// rebuilds them (11/11/2026, r4e)
	else if(!sc.derived.empty()) {
	  if(ds == 2) torn.push_back(where + ": derived indexes " + dwhy);
	  else if(ds == 1) warn.push_back(where + ": derived indexes " + dwhy + " (rebuilt by the take over's writer open)");
	  // 11/14/2026: incremental sets: an index without a good stamp is in rebuild mode (a copy on older code has
	  // none): a warning, never torn (the stamps were read under the unit's lock, with the derived state)
	  for(auto &w : nostamp) warn.push_back(where + ": " + w + " (rebuild mode)");
	}
      }
    }
  }
  for(auto &s : subs) torn_walk(sc, root, s, checked, torn, warn, tl);
}

// ---------------- Derived index tools (11/14/2026, 7.4) ----------------

namespace {
struct DigestKey_ : public DatabaseKeyB {   // any B-tree's keys as raw bytes (gxrepl digest)
  std::vector<char> b;
  DigestKey_(size_t n) : DatabaseKeyB(0), b(n ? n : 1, 0) { db_key = b.data(); }
  size_t KeySize() { return b.size(); }
  int CompareKey(const DatabaseKeyB &o) const { return memcmp(b.data(), o.db_key, b.size()); }
  int operator==(const DatabaseKeyB &o) const { return CompareKey(o) == 0; }
  int operator>(const DatabaseKeyB &o) const { return CompareKey(o) > 0; }
};
}

int gxReplDigest(const std::string &path, const std::string &kind_in, uint64_t &hash, uint64_t &count, std::string &err)
{
  hash = 0; count = 0;
  std::string kind = kind_in;
  if(kind.empty()) kind = path.size() > 4 && path.compare(path.size() - 4, 4, ".btx") == 0 ? "btree" : "bytes";
  if(kind == "bytes") {
    int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if(fd < 0) { err = "cannot open " + path; return -1; }
    std::vector<unsigned char> b(1 << 20);
    ssize_t n;
    uint64_t h = 0;
    while((n = read(fd, b.data(), b.size())) > 0) { h = gxdReplHash(b.data(), (size_t)n, h); count += (uint64_t)n; }
    close(fd);
    hash = h;
    return n < 0 ? -1 : 0;
  }
  if(kind == "objects") {                    // the live objects, in any order (a multiset of their bytes)
    gxDatabase db;
    if(db.Open(path.c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) { err = "cannot open " + path; return -1; }
    std::vector<uint64_t> hs;
    std::vector<unsigned char> b;
    for(FAU_t a = db.FindFirstObject(); a != (FAU_t)0; a = db.FindNextObject(a)) {
      __ULWORD__ n = db.ObjectLength(a);
      b.resize(n);
      if(n && db.Read(b.data(), n, a) != gxDBASE_NO_ERROR) { err = "cannot read an object"; return -1; }
      hs.push_back(gxdReplHash(b.data(), b.size(), 0));
    }
    std::sort(hs.begin(), hs.end());
    uint64_t h = 0;
    for(auto x : hs) h = gxdReplHash(&x, sizeof(x), h);
    hash = h; count = hs.size();
    return 0;
  }
  if(kind == "btree") {                      // the keys in order: the same for any node layout
    gxBtreeHeader bh;
    {
      gxDatabase probe;
      if(probe.Open(path.c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) { err = "cannot open " + path; return -1; }
      if(probe.Read(&bh, sizeof(bh), (FAU_t)probe.FileHeaderSize()) != gxDBASE_NO_ERROR) { err = "no B-tree header"; return -1; }
    }
    // The header's key size is SizeOfDatabaseKey(): the key and its right child pointer (an FAU)
    if(bh.key_size <= sizeof(FAU) || bh.key_size > 65536 || bh.node_order < 2) { err = "not a B-tree index"; return -1; }
    size_t ks = (size_t)bh.key_size - sizeof(FAU);
    DigestKey_ k(ks);
    gxBtree bt(k, bh.node_order);
    if(bt.Open(path.c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) { err = "cannot open the B-tree " + path; return -1; }
    // 11/15/2026: walk the nodes in order (left child, then each key and its right child) instead of
    // FindFirst()/FindNext(). FindNext() finds the next key by searching with the key's compare, and the raw byte
    // compare here is not the tree's own (a key whose name repeats and whose record address breaks the tie sorts by
    // the address's value, not its little-endian bytes): the search lost its place and skipped keys. The node walk
    // needs no compare and gives the tree's own order; where the two orders agree, the digest is unchanged.
    struct Pos { int next; unsigned keys; FAU_t left; std::vector<char> entries; };   // next -1: the left child to walk
    std::vector<Pos> stack;                                    // one read node per level, copied out of the BtreeNode
    const size_t max_depth = 64, entry = (size_t)bh.key_size;
    // Every node holds at least one entry, so a valid tree has at most (file size / entry size) nodes; a damaged
    // file whose children point back up the tree stops there (and at max_depth levels) instead of looping.
    uint64_t max_reads = (uint64_t)gxDatabase::FileSize(path.c_str()) / entry + 2, reads = 0;
    uint64_t h = 0;
    BtreeNode n(bh.key_size, bh.node_order);
    auto push = [&](FAU_t a) -> bool {
      if(a == (FAU_t)0) return true;
      if(stack.size() >= max_depth || ++reads > max_reads) { err = "the B-tree's nodes loop or go too deep: " + path; return false; }
      if(bt.ReadNode(n, a) != gxDBASE_NO_ERROR || (unsigned)n.key_count > (unsigned)n.node_order) {   // the node's own limit: order - 1 keys (an order below 5 is 5)
	err = "cannot read a B-tree node of " + path; return false;
      }
      Pos p;
      p.next = -1; p.keys = (unsigned)n.key_count; p.left = (FAU_t)n.left_child;
      p.entries.assign(n.key_entries, n.key_entries + entry * p.keys);
      stack.push_back(std::move(p));
      return true;
    };
    bool ok = push((FAU_t)bt.Root());
    while(ok && !stack.empty()) {
      Pos &p = stack.back();
      if(p.next == -1) { p.next = 0; FAU_t lc = p.left; ok = push(lc); continue; }
      if(p.next >= (int)p.keys) { stack.pop_back(); continue; }
      const char *e = p.entries.data() + entry * (size_t)p.next;   // an entry: its right child (an FAU), then the key
      FAU rc;
      memcpy(&rc, e, sizeof(FAU));
      memcpy(k.b.data(), e + sizeof(FAU), ks);
      p.next++;
      h = gxdReplHash(k.b.data(), k.b.size(), h); count++;
      ok = push((FAU_t)rc);
    }
    bt.Close();
    if(!ok) { hash = 0; count = 0; return -1; }
    // 11/15/2026 (review): the walk must see every key the header counts
    if(count != (uint64_t)bh.n_keys) {
      err = "the B-tree's walk found " + std::to_string(count) + " keys, its header counts " +
	    std::to_string((unsigned long long)bh.n_keys) + ": " + path;
      hash = 0; count = 0; return -1;
    }
    hash = h;
    return 0;
  }
  err = "unknown digest kind " + kind + " (btree, bytes or objects)";
  return -1;
}

int gxReplDerivedStatus(const gxReplSetConfig &sc, std::vector<std::string> &lines)
{
  lines.clear();
  lines.push_back("set " + sc.name + ": derived_apply " + sc.derived_apply +
		  (sc.derived_apply == "incremental" ? ", extractor " + (sc.derived_extractor.empty() ? std::string("(none)") : sc.derived_extractor) +
		   ", source " + (sc.derived_source.empty() ? std::string("(none)") : sc.derived_source) +
		   ", max " + std::to_string(sc.derived_apply_max_mb) + " MB" : std::string("")));
  std::string why;
  if(breaker_on(sc, &why)) lines.push_back("  BREAKER on: " + why);
  if(FILE *nf = fopen(notes_off_path(sc).c_str(), "r")) {   // 11/14/2026 (review 10)
    char b[512] = {0}; if(fgets(b, sizeof(b), nf)) {} fclose(nf);
    std::string t(b); while(!t.empty() && t.back() == '\n') t.pop_back();
    lines.push_back("  WARN: the source's object notes look off (every batch is rebuilt): " + t);
  }
  {                                                  // 11/14/2026 (review 4)
    long long now = (long long)time(0), t = 0; int n = 0;
    if(FILE *cf = fopen(crashes_path(sc).c_str(), "r")) { while(fscanf(cf, "%lld", &t) == 1) if(t > now - breaker_window_s && t <= now) n++; fclose(cf); }
    if(n) lines.push_back("  index step crashes in the last hour: " + std::to_string(n) + " (the breaker trips at " +
			  std::to_string(breaker_crashes) + ")");
  }
  if(FILE *f = fopen(try_path(sc).c_str(), "r")) { char b[128] = {0}; if(fgets(b, sizeof(b), f)) {} fclose(f);
    std::string t(b); while(!t.empty() && t.back() == '\n') t.pop_back(); lines.push_back("  index step in progress or stopped: " + t); }
  if(sc.derived.empty()) return 0;
  std::vector<std::string> folders;
  derived_folders(sc, "", folders);
  for(auto &rel : folders) {
    std::string where = rel.empty() ? "." : rel, dir = rel.empty() ? sc.root : sc.root + "/" + rel;
    unsigned long long g = 0, l = unit_log_lsn(sc, rel, &g), c = unit_changed_lsn(sc, rel);   // 11/14/2026
    std::string state;
    if(FILE *f = fopen(derived_state_path(sc, rel).c_str(), "r")) { char b[256] = {0}; if(fgets(b, sizeof(b), f)) {} fclose(f); state = b; }
    while(!state.empty() && state.back() == '\n') state.pop_back();
    lines.push_back("  " + where + ": sealed at record " + std::to_string(l) + " generation " + std::to_string(g) +
		    (c != l ? ", last changed at record " + std::to_string(c) : std::string("")) +
		    ", state " + (state.empty() ? std::string("(none)") : state));
    for(auto &ix : folder_indexes(sc, rel)) {
      gxdStampData d;
      int rc = gxdDerivedStamp::ReadFile(gxdDerivedStamp::PathFor(dir + "/" + ix), d);
      std::string st = rc == 1 ? "no stamp (rebuild mode)" : rc != 0 ? "a damaged stamp (rebuild mode)" :
	"stamped at record " + std::to_string(d.lsn) + " of log " + d.log_id + ", " + std::to_string(d.records) + " entries" +
	(d.lsn == c ? "" : " (behind the folder's last change: rebuild needed)");
      lines.push_back("    " + ix + ": " + st);
    }
  }
  return 0;
}

int gxReplDerivedReset(const gxReplSetConfig &sc, const std::string &folder, std::string &err)
// The breaker and the step marker go; the folder's (or every folder's) stamps and derived state go, under the
// apply lock: the derived pass rebuilds them and, on an incremental set, stamps them again
{
  std::string jname = sc.root + "/.gxrepl/apply.jnl";
  mkdirs(sc.root + "/.gxrepl");
  int lk = open(jname.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
  if(lk < 0) { err = "cannot open " + jname; return -1; }
  if(lock_apply_bounded(lk) != 0) { close(lk); err = "busy: " + jname + " is held; try again"; return -1; }
  unlink(breaker_path(sc).c_str());
  unlink(try_path(sc).c_str());
  unlink(crashes_path(sc).c_str());   // 11/14/2026 (review 4)
  std::vector<std::string> folders;
  if(folder.empty()) derived_folders(sc, "", folders);
  else folders.push_back(folder == "." ? std::string() : folder);
  for(auto &rel : folders) {
    drop_stamps(sc, rel, "", false);
    unlink(derived_state_path(sc, rel).c_str());
  }
  flock(lk, LOCK_UN);
  close(lk);
  return 0;
}

int gxReplTornCopies(const gxReplSetConfig &sc, std::vector<std::string> &torn, std::vector<std::string> &warn,
		     gxReplTornLockStats &ls)
{
  torn.clear();
  warn.clear();
  ls.acquisitions = ls.unlocked_units = 0;
  ls.max_hold_ms = ls.total_hold_ms = ls.waited_ms = 0;
  ls.fell_back = false;
  int checked = 0;
  if(sc.commit_block.empty()) return 0;
  TornLock tl(sc.root, ls);
  torn_walk(sc, sc.root, "", checked, torn, warn, tl);
  // 11/12/2026 (F7): the stages. A stage leaves its unit whole at its sealed commit (checked above): a newer copy
  // in progress or stopped is a warning (gxrepl promote discards it). A swap that began and did not finish leaves
  // the unit half old, half new: torn until it is finished (the next gxrepl start, or gxrepl promote).
  for(auto &st : stages_of(sc.root)) {
    std::string where = st.first.empty() ? "." : st.first;
    // 11/14/2026: under the shared lock (a swap runs in the apply lock: a .swap seen then is a stopped one)
    bool locked = tl.acquire();
    struct Rel { TornLock &t; ~Rel() { t.release(); } } rel_guard{tl};
    struct stat sb;
    if(stat((st.second + "/.swap").c_str(), &sb) == 0) {
      // a swap in flight (the server's apply lock, milliseconds) ends with the stage removed: wait for it as above
      double wait = locked ? 0 : torn_recheck_sec();
      auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds((long)(wait * 1000));
      while(stat((st.second + "/.swap").c_str(), &sb) == 0 && std::chrono::steady_clock::now() < end)
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if(stat((st.second + "/.swap").c_str(), &sb) == 0)
      torn.push_back(where + ": a staged copy was being swapped in and did not finish (gxrepl promote, or the next "
		     "gxrepl start, finishes it)");
    else if(!stage_valid(sc, sc.root, st.first))
      warn.push_back(where + ": a stale stage (the unit changed since it began, or a writer took the folder): never "
		     "used; the next gxrepl start or gxrepl promote discards it");
    else {
      std::vector<std::string> since = stage_lines(st.second + "/.stage", "since ");
      std::string t = since.empty() ? "?" : since[0];
      warn.push_back(where + ": a copy of a newer commit is staged since " + t + " (in progress, or stopped by a lost "
		     "link); the copy is whole at its sealed commit; gxrepl promote discards the stage");
    }
  }
  if(ls.fell_back)
    warn.push_back(std::string("the check ran without the apply lock: ") + tl.jname + " could not be opened, or "
		   "stayed locked past GXREPL_TORN_LOCK_WAIT (a derived index rebuild?); " + std::to_string(ls.unlocked_units) +
		   " unit(s) were read unlocked, with the timed re-reads (GXREPL_TORN_RECHECK_SEC)");
  return checked;
}

int gxReplTornCopies(const gxReplSetConfig &sc, std::vector<std::string> &torn, std::vector<std::string> &warn)
{
  gxReplTornLockStats ls;
  return gxReplTornCopies(sc, torn, warn, ls);
}

bool gxReplTestHooks()
{
#ifdef GXREPL_TEST_HOOKS
  return true;
#else
  return false;
#endif
}

// 11/12/2026 (F7): see gxrepl.h
int gxReplPromote(const gxReplSetConfig &sc, bool apply, std::vector<std::string> &lines, std::string &err)
{
  std::map<std::string, std::string> st = stages_of(sc.root);
  if(st.empty()) return 0;
  int lk = -1;
  if(apply) {
    std::string jname = sc.root + "/.gxrepl/apply.jnl";
    lk = open(jname.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if(lk < 0) { err = "cannot open " + jname; return -1; }
    // 11/13/2026 (F7 review round 2, 5): a derived index rebuild can hold the apply lock for minutes: wait up to
    // GXREPL_PROMOTE_LOCK_WAIT (480 s, inside the DRCM gate's 600 s); then leave the stages as they are and let
    // promote-check decide (a stage leaves its unit whole; a writer's open makes it stale, never used)
    double wait = 480;
    if(const char *w = getenv("GXREPL_PROMOTE_LOCK_WAIT")) { double v = atof(w); if(v >= 0) wait = v; }
    auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds((long)(wait * 1000));
    while(flock(lk, LOCK_EX | LOCK_NB) != 0) {
      if(std::chrono::steady_clock::now() >= end) {
	close(lk);
	lines.push_back("set " + sc.name + ": " + jname + " stayed locked for " + std::to_string((long)wait) +
			" s (a derived index rebuild?): the stages were left as they are; promote-check decides");
	return 0;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
  // 11/13/2026 (F7 review round 3, 2): the set's writer first. While it runs, its folders are never changed: no
  // journal replay over its files, no swap; a stage is only discarded (the writer never reads a stage).
  WriterHold wh;
  std::string we;
  bool writer = hold_writer_locks(sc, wh, we) != 0;
  if(apply) {
    std::string jname = sc.root + "/.gxrepl/apply.jnl";
    // 11/13/2026 (F7 review 4): a batch committed to the apply journal and not yet applied (a server that stopped
    // inside an apply) goes into its files first: a stage it writes into is never removed under it
    struct stat jb;
    if(stat(jname.c_str(), &jb) == 0 && jb.st_size > 0 && writer) {
      lines.push_back("set " + sc.name + ": " + jname + " holds an apply batch and the set's writer runs (" + we +
		      "): it is not replayed over the writer's files and the stages are left as they are");
      flock(lk, LOCK_UN); close(lk);
      return -1;
    }
    if(stat(jname.c_str(), &jb) == 0 && jb.st_size > 0) {
      unsigned long n = 0;
      if(gxdJournal::Recover(jname.c_str(), &n) != gxDBASE_NO_ERROR) {
	flock(lk, LOCK_UN); close(lk); err = "cannot recover " + jname; return -1;
      }
      if(n) lines.push_back("set " + sc.name + ": finished " + std::to_string(n) + " unfinished apply batch(es)");
      if(truncate(jname.c_str(), 0) != 0) { }
    }
  }
  int rc = 0;
  for(auto &e : st) {
    std::string unit = e.first.empty() ? std::string(".") : e.first, sd = e.second;
    struct stat sb;
    std::vector<std::string> since = stage_lines(sd + "/.stage", "since "), from = stage_lines(sd + "/.stage", "from ");
    std::string when = since.empty() ? "?" : since[0];
    if(!since.empty()) {
      time_t t = (time_t)atoll(since[0].c_str());
      char b[64];
      struct tm tmv;                        // 11/15/2026: localtime_r (localtime() shares its buffer across threads)
      localtime_r(&t, &tmv);
      strftime(b, sizeof(b), "%Y-%m-%d %H:%M:%S", &tmv);
      when = b;
    }
    bool swap = stat((sd + "/.swap").c_str(), &sb) == 0;
    if(writer && swap) {                      // the folder's writer runs: a begun swap is not finished under it
      lines.push_back("set " + sc.name + ": " + unit + ": the swap of a staged copy is not finished: " + we +
		      " (promote-check shows it TORN)");
      rc = -1;
      continue;
    }
    if(!writer && (swap || (stage_valid(sc, sc.root, e.first) && stage_complete(sc, sd, e.first)))) {   // round 2, 3
      lines.push_back("set " + sc.name + ": " + unit + ": " + (apply ? "finished" : "would finish") +
		      " the swap of a whole staged copy (staged since " + when + ")");
      if(apply) {
	std::string e2;
	if(stage_swap(sc, sc.root, e.first, e2) != 0) { lines.push_back("set " + sc.name + ": " + unit + ": " + e2); rc = -1; }
      }
      continue;
    }
    // what is lost: the staged copy of a newer commit; which files it had changed
    std::vector<std::string> changed;
    std::string live = e.first.empty() ? sc.root : sc.root + "/" + e.first;
    for(auto &n : stage_files(sd)) {
      if(n == sc.commit_block) continue;
      unsigned long long s1 = 0, s2 = 0; uint64_t h1 = 0, h2 = 0;
      int a = file_hash(sd + "/" + n, s1, h1), b = file_hash(live + "/" + n, s2, h2);
      if(a != 0 || b != 0 || s1 != s2 || h1 != h2) changed.push_back(n);
    }
    std::string ch;
    for(size_t i = 0; i < changed.size() && i < 8; i++) ch += (i ? ", " : "") + changed[i];
    if(changed.size() > 8) ch += ", ... (" + std::to_string(changed.size()) + " files)";
    if(!stage_valid(sc, sc.root, e.first))
      lines.push_back("set " + sc.name + ": " + unit + ": " + (apply ? "discarded" : "would discard") +
		      " a stale stage (the unit changed since it began, or a writer took the folder): nothing in it is "
		      "used");
    else {
      // what the copy is left at: whole at its sealed commit, unless it is not (promote-check then says TORN)
      uint64_t lg = 0;
      std::string lseal = read_small(sc.root + "/.gxrepl/units/" + unit_key(e.first));
      bool whole = read_unit_gen(sc, live, lg) && !(lg & 1) && !lseal.empty() && lseal.compare(0, 7, "copying") != 0;
      lines.push_back("set " + sc.name + ": " + unit + ": " + (apply ? "discarded" : "would discard") +
		      " an unfinished copy of a newer commit staged since " + when + "; " +
		      (whole ? "the copy stays whole at its sealed commit (generation " +
		       (from.empty() ? std::string("?") : from[0]) + ")" :
		       std::string("the copy is NOT whole (odd, unsealed or copied in place): promote-check reports it")) +
		      "; LOST: the source's changes to this unit after that commit" + (ch.empty() ? std::string("") :
		      std::string(" (files changed in the stage: ") + ch + ")"));
    }
    if(apply && discard_stage(sc.root, e.first) != 0) { lines.push_back("set " + sc.name + ": " + unit + ": cannot remove " + sd); rc = -1; }
  }
  if(lk >= 0) { flock(lk, LOCK_UN); close(lk); }
  if(rc != 0) err = "a stage could not be promoted (see the lines)";
  return rc;
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
      verifying[sc.name];        // 11/14/2026 (review F1): likewise, so no thread inserts a key while others look
      verify_again[sc.name];
      verify_from[sc.name];
      verify_log_id[sc.name];
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
