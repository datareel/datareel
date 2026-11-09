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
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
USA

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
#include <set>
#include <thread>
#include "gxdjrnl.h"
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
  OP_DIFF = 13, OP_STATE = 20, OP_TMANIFEST = 21, OP_HASHES = 22,
  OP_APPLYLOG = 23, OP_APPLYFILES = 24, OP_DELETE = 25, OP_SETSTATE = 26,
  OP_OK = 100, OP_ERR = 101
};

static int send_msg(gxStream &s, uint8_t op, const std::vector<unsigned char> &body,
		    int level, unsigned long long *bytes_out, std::string &err)
{
  std::vector<unsigned char> m;
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

int gxReplHasZlib() { return (int)have_zlib; }

// ---------------- Configuration ----------------

int gxReplConfig::Load(const std::string &fname, std::string &err)
{
  FILE *f = fopen(fname.c_str(), "r");
  if(!f) { err = "cannot open " + fname; return -1; }
  char line[8192];
  int ln = 0;
  gxReplSetConfig *cur = 0;
  sets.clear();
  while(fgets(line, sizeof(line), f)) {
    ln++;
    std::string s = trim(line);
    if(s.empty() || s[0] == '#') continue;
    if(s[0] == '[') {
      size_t e = s.find(']');
      std::string sec = trim(s.substr(1, e == std::string::npos ? std::string::npos : e - 1));
      if(sec.compare(0, 4, "set ") != 0 || trim(sec.substr(4)).empty()) {
	fclose(f);
	err = fname + ":" + std::to_string(ln) + ": expected [set NAME]";
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
    if(cur) {
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
      else known = false;
    }
    else {
      if(k == "node") node = v;
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

static bool excluded(const gxReplSetConfig &sc, const std::string &rel)
{
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
    if(n == "." || n == "..") continue;
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
    if(sent >= max_bytes && off < size) { res.more = 1; break; }
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
  if(writer_active(s->sc, err)) return -1;
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

int gxReplLocal::apply_batch(SetState_ *s,
  std::map<std::string, std::pair<unsigned long long,
  std::map<unsigned long long, const unsigned char *> > > &pages,
  std::string &err)
{
  if(pages.empty()) return 0;
  std::vector<gxdApplyFile> af;
  for(auto &e : pages) {
    std::string dir = e.first.substr(0, e.first.find_last_of('/'));
    if(mkdirs(dir) != 0) { err = "cannot make " + dir; return -1; }
    gxdApplyFile f;
    f.path = e.first;
    f.size = e.second.first;
    f.pages = e.second.second;
    af.push_back(f);
  }
  std::string jdir = s->root + "/.gxrepl";
  if(mkdirs(jdir) != 0) { err = "cannot make " + jdir; return -1; }
  std::string jname = jdir + "/apply.jnl";
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
static unsigned long long add_records(gxReplLocal &L, const gxReplSetConfig &sc,
				      const std::vector<unsigned char> &records,
				      unsigned long long expect_first,
				      PageBatch &batch, unsigned long long &count,
				      std::string &err)
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
    if(r.type != gxdShipCommit) continue;
    std::vector<gxdShipCommitFile> files;
    if(gxdParseShipCommit(r.payload, r.len, files) != 0) {
      err = "damaged commit record " + std::to_string(r.lsn); return 0;
    }
    for(auto &f : files) {
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

int gxReplLocal::ApplyLog(const std::string &set, const std::string &stream,
			  const std::string &log_id,
			  const std::vector<unsigned char> &records,
			  unsigned long long &lsn, std::string &err)
{
  SetState_ *s = set_state(set, err);
  if(!s) return -1;
  std::lock_guard<std::mutex> g(s->m);
  if(writer_active(s->sc, err)) return -1;
  std::map<std::string, gxReplStreamState> m;
  read_states(s, m, err);
  auto it = m.find(stream);
  if(it == m.end() || it->second.log_id != log_id) {
    err = "stream " + stream + " is not in step (verify first)"; return -1;
  }
  PageBatch batch;
  unsigned long long count = 0;
  unsigned long long last = add_records(*this, s->sc, records, it->second.lsn + 1,
					batch, count, err);
  if(last == 0) return -1;
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
  if(writer_active(s->sc, err)) return -1;
  std::map<std::string, gxReplStreamState> states;
  read_states(s, states, err);
  std::map<std::string, gxReplStreamState> changed;
  PageBatch batch;
  std::list< std::vector<unsigned char> > padded;
  std::vector<std::string> fulls(v.size());
  for(size_t i = 0; i < v.size(); i++) {
    const gxReplFileUpdate &u = v[i];
    if(SafePath(s->sc, u.path, fulls[i], err) != 0) return -1;
    if(!u.stream.empty()) {
      auto it = states.find(u.stream);
      bool same = it != states.end() && it->second.log_id == u.log_id;
      if(same && it->second.lsn != u.lsn) {
	if(it->second.lsn > u.lsn || u.records.empty()) continue; // Stale
	unsigned long long count = 0, e = 0;
	std::string e2;
	e = add_records(*this, s->sc, u.records, it->second.lsn + 1, batch, count, e2);
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
  if(writer_active(s->sc, err)) return -1;
  for(auto &p : paths) {
    std::string full;
    if(SafePath(s->sc, p, full, err) != 0) return -1;
    if(unlink(full.c_str()) != 0 && errno != ENOENT) { err = "cannot remove " + p; return -1; }
    std::string dir = full.substr(0, full.find_last_of('/'));
    while(dir.size() > s->root.size() && rmdir(dir.c_str()) == 0)
      dir = dir.substr(0, dir.find_last_of('/'));
  }
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
  return stream && stream->Connection().IsOpen();
}

void gxReplRemote::Close()
{
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
  if(!r.ok) { err = "bad reply"; return -1; }
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
  if(!r.ok) { err = "bad reply"; return -1; }
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
  return dec_files(rep, v, err);
}

int gxReplRemote::TargetManifest(const std::string &set, std::vector<gxReplFile> &v,
				 std::string &err)
{
  WBuf w; w.str(set);
  std::vector<unsigned char> rep;
  if(call(mkreq(OP_TMANIFEST, w), rep, err)) return -1;
  return dec_files(rep, v, err);
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
  if(!r.ok) { err = "bad reply"; return -1; }
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
  if(!r.ok) { err = "bad reply"; return -1; }
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
  if(!r.ok) { err = "bad reply"; return -1; }
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
  if(!r.ok) { err = "bad reply"; return -1; }
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

gxReplServer::gxReplServer(const gxReplConfig &c) : cfg(c), local(c) { }

gxReplServer::~gxReplServer() { server.Close(); }

void gxReplServer::write_status()
{
  if(cfg.status_file.empty()) return;
  std::string j = "{\"node\": " + json_str(cfg.node) + ", \"role\": \"server\", \"time\": " +
    std::to_string(now_s()) + ", \"port\": " + std::to_string(cfg.listen_port) +
    ", \"applied_records\": " + std::to_string(local.applied_records) +
    ", \"applied_pages\": " + std::to_string(local.applied_pages) + ", \"clients\": [";
  std::lock_guard<std::mutex> g(status_lock);
  bool first = true;
  for(auto &e : conn_info) { j += (first ? "" : ", ") + json_str(e.second); first = false; }
  j += "], \"sets\": {";
  first = true;
  for(auto &sc : cfg.sets) {
    j += (first ? "" : ", ") + json_str(sc.name) + ": {\"accept\": " + json_str(sc.accept) +
      ", \"root\": " + json_str(sc.root) + ", \"last\": " + json_str(set_last[sc.name]) + "}";
    first = false;
  }
  j += "}}\n";
  write_atomic(cfg.status_file, j);
}

int gxReplServer::Run(std::string &err)
{
  signal(SIGPIPE, SIG_IGN);
  if(local.RecoverAll(err) != 0) return -1;
  server.SetClientCAFile(cfg.ca);
  for(auto &cn : cfg.allow_cn) server.AllowClientCN(cn);
  for(auto &fp : cfg.allow_fingerprint) {
    if(server.AllowClientFingerprint(fp) != 0) { err = "bad fingerprint " + fp; return -1; }
  }
  if(server.Listen(cfg.listen_port, cfg.cert, cfg.key, cfg.key_passphrase,
		   cfg.listen_address) != 0) {
    err = "listen on port " + std::to_string(cfg.listen_port) + ": " + server.Error();
    return -1;
  }
  glog("serving %zu set(s) on %s:%d", cfg.sets.size(),
       cfg.listen_address.empty() ? "*" : cfg.listen_address.c_str(), server.Port());
  write_status();
  int slot = 0;
  long long last_status = 0;
  while(!stop) {
    if(now_s() - last_status >= 5) { write_status(); last_status = now_s(); }
    if(server.Listener().WaitReadable(500) <= 0) continue;
    // 11/07/2026: Only the TCP accept runs here. The TLS handshake runs
    // in its own thread with a short timeout, so a connection that never
    // starts or finishes TLS (a scanner on an open port) cannot hold up
    // the workstation's connection.
    gxSocket c = server.Listener().Accept();
    if(!c.IsOpen()) continue;
    if(handshaking >= cfg.max_handshakes) {
      glog("dropped %s: %d handshakes already running", c.PeerIPAddress().c_str(), (int)handshaking);
      c.Close();
      continue;
    }
    handshaking++;
    int id = ++slot;
    std::thread(&gxReplServer::handshake, this, id, std::make_shared<gxSocket>(std::move(c))).detach();
  }
  server.Close();
  for(int i = 0; i < 100 && (clients > 0 || handshaking > 0); i++) usleep(100000);
  write_status();
  return 0;
}

void gxReplServer::handshake(int slot, std::shared_ptr<gxSocket> c)
{
  std::string ip = c->PeerIPAddress();
  auto s = std::make_shared<gxStream>();
  s->SetTimeOut(cfg.handshake_timeout);
  s->SetMaxBlockSize((gxUINT32)(cfg.max_message * 4 + 1024 * 1024));
  int rc = server.Handshake(s->Connection(), std::move(*c));
  if(rc != 0) {
    glog("refused %s: %s", ip.c_str(), s->Connection().Error().c_str());
    s->Close();
    handshaking--;
    return;
  }
  // Only clients that passed the certificate checks count against max_clients
  if(clients.fetch_add(1) >= cfg.max_clients) {
    clients--;
    glog("refused %s: %d clients already", s->Connection().PeerCN().c_str(), cfg.max_clients);
    s->Close();
    handshaking--;
    return;
  }
  handshaking--;
  s->SetTimeOut(cfg.timeout);
  serve(slot, s);
}

void gxReplServer::serve(int slot, std::shared_ptr<gxStream> s)
{
  std::string who = s->Connection().PeerCN() + "@" + s->Connection().Host();
  glog("client %s connected", who.c_str());
  {
    std::lock_guard<std::mutex> g(status_lock);
    conn_info[slot] = who + " since " + std::to_string(now_s());
  }
  bool hello = false;
  bool peer_zlib = false;
  std::string err;
  while(!stop) {
    uint8_t op = 0;
    std::vector<unsigned char> body;
    if(recv_msg(*s, op, body, cfg.max_message * 4 + 1024 * 1024, 0, err) != 0) break;
    RBuf r(body.data(), body.size());
    WBuf w;
    std::string e;
    int rc = 0;
    auto role_ok = [&](const std::string &set, bool as_target) -> bool {
      const gxReplSetConfig *sc = cfg.Set(set);
      if(!sc) { e = "no set " + set + " on " + cfg.node; return false; }
      if(as_target && sc->accept == "source") { e = "set " + set + " is a source on " + cfg.node; return false; }
      if(!as_target && sc->accept == "target") { e = "set " + set + " is a copy on " + cfg.node; return false; }
      return true;
    };
    if(!hello && op != OP_HELLO) { glog("client %s: no hello", who.c_str()); break; }
    switch(op) {
    case OP_HELLO: {
      std::string magic = r.str(); uint32_t proto = r.u32(); std::string node = r.str();
      uint32_t fau = r.u32(), page = r.u32(), endian = r.u32();
      peer_zlib = r.u32() != 0 && have_zlib;
      if(!r.ok || magic != "GXREPL" || proto != (uint32_t)gxREPL_PROTOCOL ||
	 fau != sizeof(FAU) || page != gxdPageSize || endian != 0x01020304) {
	e = "incompatible client (protocol, build or platform)"; rc = -1; break;
      }
      hello = true;
      who = s->Connection().PeerCN() + " (" + node + ")@" + s->Connection().Host();
      { std::lock_guard<std::mutex> g(status_lock); conn_info[slot] = who + " since " + std::to_string(now_s()); }
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
    int sent;
    if(rc == 0) sent = send_msg(*s, OP_OK, w.b, peer_zlib ? cfg.compress : 0, 0, err);
    else {
      WBuf we; we.str(e);
      sent = send_msg(*s, OP_ERR, we.b, 0, 0, err);
      if(op == OP_HELLO) { glog("client %s refused: %s", who.c_str(), e.c_str()); break; }
    }
    if(sent != 0) break;
  }
  s->Close();
  glog("client %s disconnected%s%s", who.c_str(), err.empty() ? "" : ": ", err.c_str());
  {
    std::lock_guard<std::mutex> g(status_lock);
    conn_info.erase(slot);
  }
  clients--;
}

// ---------------- Client ----------------

gxReplClient::gxReplClient(const gxReplConfig &c) : cfg(c), local(c) { }

gxReplClient::~gxReplClient() { }

void gxReplClient::write_status(bool connected, const std::string &note)
{
  if(cfg.status_file.empty()) return;
  last_status_write = now_s();
  std::string j = "{\"node\": " + json_str(cfg.node) + ", \"role\": \"client\", \"time\": " +
    std::to_string(now_s()) + ", \"connected\": " + (connected ? "true" : "false") +
    ", \"peer\": " + json_str(peer) + ", \"since\": " + std::to_string(connected_since) +
    ", \"via\": " + json_str(cfg.ssh_tunnel.empty() ? "direct" : "ssh " + cfg.ssh_tunnel) +
    ", \"note\": " + json_str(note) + ", \"sets\": {";
  bool first = true;
  for(auto &sc : cfg.sets) {
    auto &st = status[sc.name];
    j += (first ? "" : ", ") + json_str(sc.name) + ": {\"direction\": " + json_str(sc.direction) +
      ", \"ok\": " + (st.ok ? "true" : "false") + ", \"last_ok\": " + std::to_string(st.last_ok) +
      ", \"last_scan\": " + std::to_string(st.last_scan) +
      ", \"last_verify\": " + std::to_string(st.last_verify) +
      ", \"last_full_verify\": " + std::to_string(st.last_full_verify) +
      ", \"records\": " + std::to_string(st.records) + ", \"files_sent\": " + std::to_string(st.files_sent) +
      ", \"pieces_sent\": " + std::to_string(st.pieces_sent) + ", \"bytes_sent\": " + std::to_string(st.bytes_sent) +
      ", \"deletes\": " + std::to_string(st.deletes) + ", \"last_error\": " + json_str(st.last_error) +
      ", \"streams\": {";
    bool f2 = true;
    for(auto &e : st.streams) {
      long long lag = (long long)e.second.first - (long long)e.second.second;
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
    if(out.empty()) return 0;
    std::vector<gxdShipRecordView> recs;
    if(gxdParseShipRecords(out.data(), out.size(), recs) != 0) { err = "damaged log records"; return -1; }
    for(auto &r : recs) {
      if(r.type == gxdShipOpen || r.type == gxdShipGap) verify = true;
    }
    unsigned long long lsn = 0;
    if(tgt.ApplyLog(set, s.name, s.log_id, out, lsn, err) != 0) return -1;
    ts.lsn = lsn;
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

  bool full = once || verify_now || (sc.full_verify_interval > 0 && now - st.last_full_verify >= sc.full_verify_interval);
  bool verify = full || (sc.verify_interval > 0 && now - st.last_verify >= sc.verify_interval);
  // 11/08/2026: After a connect, verify the files the ship-log streams cover (the commit feed may have
  // moved on while the link was down). Plain files only get the size/mtime scan: hashing every one of
  // them on each connect read all of a 22 GB osa_data at idle I/O (13 hours) for nothing.
  bool cverify = connect_verify;
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
    st.streams[s.name] = std::make_pair(s.next_lsn ? s.next_lsn - 1 : 0, ts.lsn);
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
    for(auto &f : s->files) {
      bool on_source = false;
      for(auto &m : sm) if(m.path == f) { on_source = true; break; }
      if(!on_source) continue;
      std::string e;
      int rc = sync_file(src, tgt, sc.name, f, s, st, e);
      if(rc < 0) { err = e; return -1; }
      if(rc > 0) { failed++; st.last_error = e; glog("set %s: %s", sc.name.c_str(), e.c_str()); }
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
      st.streams[s->name] = std::make_pair(s->next_lsn ? s->next_lsn - 1 : 0, x.lsn);
    }
    st.verifies++;
    glog("set %s: stream %s verified (%d file(s))", sc.name.c_str(), s->name.c_str(), synced);
    it = vlist.erase(it);
  }

  // 3. Scan, and verify when due
  if(verify || cverify || now - st.last_scan >= sc.scan_interval) {
    if(!have_sm) { if(src.Manifest(sc.name, sm, err) != 0) return -1; have_sm = true; }
    std::vector<gxReplFile> tm;
    if(tgt.Manifest(sc.name, tm, err) != 0) return -1;
    std::map<std::string, const gxReplFile *> tmap;
    for(auto &t : tm) tmap[t.path] = &t;
    std::map<std::string, const gxReplStream *> cover_of;
    for(auto &s : streams) for(auto &f : s.files) cover_of[f] = &s;
    long long recent_ns = now_ns() - (long long)sc.verify_recent_hours * 3600LL * 1000000000LL;
    int failures = 0;
    for(auto &f : sm) {
      if(stop) break;
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
      if(rc > 0) { failures++; st.last_error = e; glog("set %s: %s", sc.name.c_str(), e.c_str()); }
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
      st.ok = true; st.last_ok = now_s(); st.last_error.clear();
    }
    else {
      if(!e.empty()) {
	if(e != st.last_error) glog("set %s: %s", sc.name.c_str(), e.c_str());
	st.last_error = e;
      }
      st.ok = false;
      if(rc > worst) worst = rc;
    }
    if(rc < 0 && !remote.IsOpen()) { err = e; return -1; }
    if(rc < 0) worst = 2;
  }
  verify_now = false;
  connect_verify = false;
  return worst;
}

int gxReplClient::Run(bool once, const std::string &only_set, std::string &err)
{
  signal(SIGPIPE, SIG_IGN);
  if(local.RecoverAll(err) != 0) return 2;
  int backoff = 1;
  while(!stop) {
    gxReplRemote remote(cfg);
    std::string e;
    if(remote.Connect(e) != 0) {
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
    connect_verify = true; // 11/08/2026: was verify_now (a full verify of every file on each connect)
    pass_connected = true;
    long long last_status = 0;
    while(!stop) {
      std::string pe;
      unsigned long long before = work_count();
      int rc = pass(remote, once, only_set, pe);
      // Write the status after a pass that moved data, else every 2 s
      if(work_count() != before || now_s() - last_status >= 2 || rc != 0 || once) {
	write_status(remote.IsOpen(), pe);
	last_status = now_s();
      }
      if(rc < 0 || !remote.IsOpen()) {
	glog("connection lost: %s", pe.c_str());
	if(once) { err = pe; return 2; }
	break;
      }
      if(once) {
	if(rc == 0) {
	  // A second pass finds nothing to do when every set is the same
	  rc = pass(remote, false, only_set, pe);
	  write_status(remote.IsOpen(), pe);
	}
	if(rc != 0) err = pe.empty() ? "a set is not in step (see the status)" : pe;
	return rc == 0 ? 0 : 1;
      }
      interruptible_sleep(cfg.poll_ms, stop);
    }
    pass_connected = false;
    write_status(false, "disconnected");
  }
  return 0;
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
