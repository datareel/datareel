// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: gxobjrepl.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/16/2026
// Date Last Modified: 11/16/2026
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

gxObjRepl, object replication (see gxobjrepl.h). The wire format:

  hello (primary)  u32 magic, u64 epoch, str link map, str name,
                   u8 role, u64 last seq, u64 running digest
  reply (copy)     u8 ok, u64 applied seq, u64 epoch, u8 in full send,
                   str name, u8 role, u64 applied digest, str why
  F_COMMIT         u8 1, u64 seq, u64 epoch, u32 ops, then each op:
                   u8 op, str db, u64 id; ADD/UPDATE add u32 capacity,
                   u8 moved, u32 length, bytes (links as ids)
  F_SNAP_BEGIN     u8 2, u64 base seq, u64 epoch
  F_SNAP_BATCH     u8 3, u32 records, each: str db, u64 id, u32 version,
                   u32 flags, u32 reason, u64 seen seq, u32 capacity,
                   u32 length, bytes
  F_SNAP_DLOG      u8 6, u32 entries, each: u64 seq, u64 digest (the
                   primary's digest log up to the base, before F_SNAP_END)
  F_SNAP_END       u8 4, u64 base seq, u64 digest, u64 records + digest-log
                   entries sent, u64 hash chained over them (as sent)
  F_EPOCH          u8 5, u64 epoch: sent once the primary has accepted the
                   copy (after the hello); the copy saves the epoch
  ack (copy)       u64 seq applied
7.5 beta 3, the incremental rejoin (appended fields: older nodes ignore
them, and a node that does not send them gets the older behavior):
  hello (primary)  ... then u32 RJ_MAGIC, u64 epoch base seq, u64 digest
                   there, str schema fingerprint
  reply (copy)     ... then u32 RJ_MAGIC, u8 incremental ok, str schema
                   fingerprint
  F_RJ_SET         u8 7, u64 base seq, u64 up-to seq; the answer: u8 ok,
                   u32 entries, each: str db, u64 id (what the copy changed
                   after the base)
  F_RJ_BATCH       u8 8, u32 records, each: u8 1 and a full-send record,
                   or u8 2, str db, u64 id (absent on the primary)
  F_RJ_END         u8 9, u64 up-to seq, u64 digest, u64 records + digest-
                   log entries sent, u64 hash chained over them (as sent)
  (the primary's digest log after the base goes as F_SNAP_DLOG frames)
Every number is little endian; every length read is checked against
the frame.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <map>
#include <set>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <tuple>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <condition_variable>
#include "gxobjrepl.h"
#include "gxdbase.h"
#include "gxbtree.h"
#include "gxdjrnl.h"
#include "gxstream.h"
#include "gxvindex.h"

#if !defined(__USE_GX_SCHANNEL__)
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#endif

#if defined(_WIN32)
#include <process.h>
#include <windows.h>
#else
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <poll.h>
#endif

#ifdef __BCC32__
#pragma warn -8080
#endif

namespace fs = std::filesystem;

namespace datareel {

namespace {

// ---------- records ----------
// A record is a head and its bytes, padded to the record's capacity (its block). flags: TOMB = the object was removed
// (or is not known yet): the record keeps the object's id and address so links to it still resolve and the address
// is never reused; STUB = a forwarding stub left where an object moved (fwd = its address now), followed in one hop
struct ObjHead { uint64_t id; uint32_t version; uint32_t len; uint32_t flags; uint32_t reason; uint64_t seen_seq; uint64_t fwd; };
enum { OF_TOMB = 1, OF_STUB = 2 };
enum { TR_REMOVED = 1, TR_UNKNOWN_TARGET = 2, TR_UNKNOWN_REMOVE = 3 };   // why a tombstone exists
const uint32_t PLACEHOLDER_RESERVE = 512;   // the capacity of a tombstone made for an object not known yet
const uint32_t STATE_MAGIC = 0x3550524fu;   // "ORP5" (11/16/2026: max_id, rejoin_dropped, the full-send count and hash)
enum { ROLE_NONE = 0, ROLE_PRIMARY = 1, ROLE_COPY = 2 };
// The running digest: a hash chained over every commit frame in order (the primary when it records a commit, the
// copy when it applies it), so two nodes at the same seq with different histories never look alike
const uint64_t DIGEST0 = 14695981039346656037ULL;
struct State {
  uint32_t magic; uint32_t in_snapshot;
  uint64_t next_seq; uint64_t acked_seq; uint64_t epoch; uint64_t applied_seq; uint64_t snapshot_base;
  uint32_t role; uint32_t pad;
  uint64_t cur_digest;      // primary: after next_seq - 1
  uint64_t acked_digest;    // primary: after acked_seq
  uint64_t applied_digest;  // copy: after applied_seq
  uint64_t max_id;          // the highest object id ever written here (NewId() never hands out an old one)
  uint64_t rejoin_dropped;  // commits this node had made as a primary past its copy's last ack, at its last Rejoin()
  uint64_t snap_count;      // copy, in a full send: the records and digest-log entries received so far
  uint64_t snap_hash;       // and a hash chained over them, checked against the primary's at F_SNAP_END
};
// NewId(): epoch << ID_EPOCH_SHIFT plus a counter, so a primary at a new epoch never hands out an id an old primary
// may have used in commits that never reached it (11/16/2026, design review)
const int ID_EPOCH_SHIFT = 40;
// 11/16/2026 (review): sizes. A commit travels as one gxStream block (gxSTREAM_DEF_MAX_BLOCK, 64 MB): Commit()
// refuses a bigger one rather than leaving it in the outbox where it could never be delivered
const uint32_t MAX_OBJECT = 32u * 1024 * 1024;                 // the bytes of one object
const size_t MAX_FRAME = 60u * 1024 * 1024;                     // a commit frame
const uint64_t MAX_RECORD = MAX_OBJECT + 64;                    // a record's capacity (head + bytes), from the wire too
enum { OP_ADD = 1, OP_UPDATE = 2, OP_REMOVE = 3, OP_PURGE = 4 };   // every op names (database, object id)
enum { F_COMMIT = 1, F_SNAP_BEGIN = 2, F_SNAP_BATCH = 3, F_SNAP_END = 4, F_EPOCH = 5, F_SNAP_DLOG = 6,
       F_RJ_SET = 7, F_RJ_BATCH = 8, F_RJ_END = 9 };
// 7.5 beta 3: the incremental rejoin's hello fields, and State::in_snapshot 2 = a former primary waiting for one
// (its old data still here; reads are refused as in a full send, and a primary that cannot do it sends a full send)
const uint32_t RJ_MAGIC = 0x31304a52u;   // "RJ01"
const uint32_t SNAP_INCR = 2;
// the digest log keeps the last DLOG_KEEP commits: a copy further behind than that cannot be checked and must rejoin
const uint64_t DLOG_KEEP = 1000000;
const uint32_t DLOG_BATCH = 65536;                              // entries per F_SNAP_DLOG frame
const size_t SNAP_BATCH_BYTES = 1u << 20;
// the journal is checkpointed once it holds CKPT_SOFT bytes, with the database files synced before the node's lock
// is taken for it (gxObjRepl::Impl::checkpoint_soon()); a commit checkpoints it in the lock only at CKPT_HARD
const size_t CKPT_SOFT = 8u * 1024 * 1024, CKPT_HARD = 64u * 1024 * 1024;
const BtreeNodeOrder_t ORDER = 16;

struct IdKeyData { uint64_t id; FAU addr; };
class IdKey : public DatabaseKeyB {
public:
  IdKey() : DatabaseKeyB((char *)&d) { memset(&d, 0, sizeof(d)); }
  IdKey(uint64_t id, FAU_t a) : DatabaseKeyB((char *)&d) { memset(&d, 0, sizeof(d)); d.id = id; d.addr = (FAU)a; }
  size_t KeySize() { return sizeof(d); }
  int CompareKey(const DatabaseKeyB &o) const {
    const IdKeyData *k = (const IdKeyData *)o.db_key;
    if(d.id != k->id) return d.id < k->id ? -1 : 1;
    return d.addr < k->addr ? -1 : d.addr > k->addr ? 1 : 0;
  }
  int operator==(const DatabaseKeyB &o) const { return CompareKey(o) == 0; }
  int operator>(const DatabaseKeyB &o) const { return CompareKey(o) > 0; }
  IdKeyData d;
};

uint64_t fnv(const void *p, size_t n, uint64_t h = 14695981039346656037ULL)
{
  const unsigned char *b = (const unsigned char *)p;
  for(size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ULL; }
  return h;
}

std::string hex16(uint64_t v) { char b[24]; snprintf(b, sizeof(b), "%016llx", (unsigned long long)v); return b; }

// ---------- byte buffers ----------
struct Buf {
  std::vector<unsigned char> b;
  void u8(uint8_t v) { b.push_back(v); }
  void u16(uint16_t v) { b.push_back((unsigned char)v); b.push_back((unsigned char)(v >> 8)); }
  void u32(uint32_t v) { for(int i = 0; i < 4; i++) b.push_back((unsigned char)(v >> (8 * i))); }
  void u64(uint64_t v) { for(int i = 0; i < 8; i++) b.push_back((unsigned char)(v >> (8 * i))); }
  void raw(const void *p, size_t n) { const unsigned char *c = (const unsigned char *)p; b.insert(b.end(), c, c + n); }
  void str(const std::string &s) { u16((uint16_t)s.size()); raw(s.data(), s.size()); }
};
struct Rd {
  const unsigned char *p; size_t n, o = 0; bool ok = true;
  Rd(const unsigned char *p_, size_t n_) : p(p_), n(n_) {}
  bool need(size_t k) { if(k > n || o > n - k) ok = false; return ok; }
  uint8_t u8() { if(!need(1)) return 0; return p[o++]; }
  uint16_t u16() { if(!need(2)) return 0; uint16_t v = (uint16_t)(p[o] | (p[o + 1] << 8)); o += 2; return v; }
  uint32_t u32() { if(!need(4)) return 0; uint32_t v = 0; for(int i = 0; i < 4; i++) v |= (uint32_t)p[o + i] << (8 * i); o += 4; return v; }
  uint64_t u64() { if(!need(8)) return 0; uint64_t v = 0; for(int i = 0; i < 8; i++) v |= (uint64_t)p[o + i] << (8 * i); o += 8; return v; }
  const unsigned char *raw(size_t k) { if(!need(k)) return 0; const unsigned char *r = p + o; o += k; return r; }
  std::string str() { uint16_t k = u16(); const unsigned char *r = raw(k); return r ? std::string((const char *)r, k) : std::string(); }
};

// a database name: letters, digits, '_' and '-' only (it becomes a file name)
bool good_name(const std::string &s)
{
  if(s.empty() || s.size() > 64) return false;
  for(char c : s) if(!(isalnum((unsigned char)c) || c == '_' || c == '-')) return false;
  return s != "state" && s != "outbox";
}

std::string path(const std::string &root, const std::string &f) { return (fs::path(root) / f).string(); }
bool exists(const std::string &f) { std::error_code ec; return fs::exists(f, ec); }
void remove_file(const std::string &f) { std::error_code ec; fs::remove(f, ec); }
std::vector<std::string> list_dir(const std::string &root)
{
  std::vector<std::string> v; std::error_code ec;
  for(fs::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) v.push_back(it->path().filename().string());
  std::sort(v.begin(), v.end());
  return v;
}
std::string read_file(const std::string &f)
{
  std::string s; FILE *in = fopen(f.c_str(), "rb");
  if(!in) return s;
  char b[4096]; size_t n;
  while((n = fread(b, 1, sizeof(b), in)) > 0) s.append(b, n);
  fclose(in);
  return s;
}

bool ends_with(const std::string &s, const std::string &t) { return s.size() > t.size() && s.compare(s.size() - t.size(), t.size(), t) == 0; }

// ---------- link map ----------
// An object may hold the file address of another object (a link field). Addresses are each database's own, so a link
// travels as the target's object id: the primary turns the address into the id when it records the commit, and the
// copy turns the id into its own address when it applies it. "DB:OFFSET:TARGET_DB" entries (OFFSET into the object's
// bytes, an 8-byte address; 0 = no link). Both sides use the primary's map, which it sends when it connects
struct LinkDef { std::string db; uint32_t off; std::string target; };
bool parse_links(const std::string &spec, std::vector<LinkDef> &out)
{
  out.clear();
  size_t i = 0;
  while(i < spec.size()) {
    size_t j = spec.find(',', i); if(j == std::string::npos) j = spec.size();
    std::string e = spec.substr(i, j - i);
    size_t a = e.find(':'), b = a == std::string::npos ? a : e.find(':', a + 1);
    if(a == std::string::npos || b == std::string::npos) return false;
    LinkDef d{e.substr(0, a), (uint32_t)atol(e.substr(a + 1, b - a - 1).c_str()), e.substr(b + 1)};
    if(!good_name(d.db) || !good_name(d.target) || d.off > (1u << 30)) return false;
    out.push_back(d);
    i = j + 1;
  }
  return true;
}
std::string links_spec(const std::vector<LinkDef> &v)
{
  std::string s;
  for(auto &d : v) s += (s.empty() ? "" : ",") + d.db + ":" + std::to_string(d.off) + ":" + d.target;
  return s;
}
// the map in one order (by database, offset, target) and its hash: two nodes' maps compare equal whatever order
// each was configured in (11/16/2026: each node configures its own map; a copy refuses a primary whose map differs)
std::string links_canon(std::vector<LinkDef> v)
{
  std::sort(v.begin(), v.end(), [](const LinkDef &a, const LinkDef &b) {
    return a.db != b.db ? a.db < b.db : a.off != b.off ? a.off < b.off : a.target < b.target; });
  return links_spec(v);
}

// ---------- indexes: each node builds its own ----------
// Configured on each node, never sent on the wire, and built from the object bytes in the same transaction as the
// object. "NAME:DB:B:OFF:LEN" is a B-tree on LEN bytes (at most 32) at OFF; "NAME:DB:V:OFF:DIM" a vector index
// (gxVectorIndex, float32, L2) on DIM floats at OFF. An object too short for the field, or with a vector that is not
// finite, is not in that index. A new index on a database that already has objects is built from them;
// ROOT/<file>.building marks an unfinished build, which the next open starts again
struct IxDef { std::string name, db; char kind; uint32_t off, len; };
bool parse_ix(const std::string &spec, std::vector<IxDef> &out)
{
  out.clear();
  size_t i = 0;
  while(i < spec.size()) {
    size_t j = spec.find(',', i); if(j == std::string::npos) j = spec.size();
    std::vector<std::string> f; std::string e = spec.substr(i, j - i);
    for(size_t a = 0;;) { size_t b = e.find(':', a); f.push_back(e.substr(a, b == std::string::npos ? b : b - a)); if(b == std::string::npos) break; a = b + 1; }
    if(f.size() != 5 || (f[2] != "B" && f[2] != "V")) return false;
    IxDef d{f[0], f[1], f[2][0], (uint32_t)atol(f[3].c_str()), (uint32_t)atol(f[4].c_str())};
    if(d.name.empty() || d.name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != std::string::npos) return false;
    if(!good_name(d.db) || d.off > (1u << 30)) return false;
    if(d.len == 0 || (d.kind == 'B' && d.len > 32) || (d.kind == 'V' && d.len > 4096)) return false;
    for(auto &o : out) if(o.name == d.name && o.db == d.db) return false;
    out.push_back(d);
    i = j + 1;
  }
  return true;
}
struct IxKeyData { unsigned char k[32]; uint64_t id; };
class IxKey : public DatabaseKeyB {
public:
  IxKey() : DatabaseKeyB((char *)&d) { memset(&d, 0, sizeof(d)); }
  size_t KeySize() { return sizeof(d); }
  int CompareKey(const DatabaseKeyB &o) const {
    const IxKeyData *k = (const IxKeyData *)o.db_key;
    int c = memcmp(d.k, k->k, sizeof(d.k));
    if(c) return c < 0 ? -1 : 1;
    return d.id < k->id ? -1 : d.id > k->id ? 1 : 0;
  }
  int operator==(const DatabaseKeyB &o) const { return CompareKey(o) == 0; }
  int operator>(const DatabaseKeyB &o) const { return CompareKey(o) > 0; }
  IxKeyData d;
};
struct Ix {
  IxDef def;
  IxKey type;
  gxBtree bt{type, 16};
  gxVectorIndex vx;
  std::string file(const std::string &root) const { return path(root, def.db + "." + def.name + (def.kind == 'B' ? ".ix.btx" : ".vix")); }
  std::string spec() const { return "objrepl/" + def.name + "/" + std::to_string(def.off) + "/" + std::to_string(def.len); }
  bool key(uint64_t id, const unsigned char *p, uint32_t n, IxKey &k) const {
    if((size_t)def.off + def.len > n) return false;
    memset(&k.d, 0, sizeof(k.d)); memcpy(k.d.k, p + def.off, def.len); k.d.id = id;
    return true;
  }
  bool vec(const unsigned char *p, uint32_t n, std::vector<float> &v) const {
    if((size_t)def.off + 4 * (size_t)def.len > n) return false;
    v.resize(def.len); memcpy(v.data(), p + def.off, 4 * (size_t)def.len);
    for(float x : v) if(!(x == x) || x > 3.0e38f || x < -3.0e38f) return false;
    return true;
  }
  bool add(uint64_t id, const unsigned char *p, uint32_t n) {
    if(def.kind == 'B') { IxKey k, c; return !key(id, p, n, k) || bt.Insert(k, c) != 0; }
    std::vector<float> v; return !vec(p, n, v) || vx.Insert((long long)id, v.data()) == gxv_NO_ERROR;
  }
  bool del(uint64_t id, const unsigned char *p, uint32_t n) {
    if(def.kind == 'B') { IxKey k, c; return !key(id, p, n, k) || bt.Delete(k, c) != 0; }
    std::vector<float> v; return !vec(p, n, v) || vx.Delete((long long)id) == gxv_NO_ERROR;
  }
  void close() { if(def.kind == 'B') bt.Close(); else vx.Close(); }
};

// the node's lock: a recursive mutex that knows how deep its owner holds it (Once(): held once, by the calling thread,
// so it can be given up for a while: the network I/O of Pump(), a file sync before a checkpoint). A test build
// (GXREPL_TEST_HOOKS) with OBJREPL_TEST_HOLD_MS=N set says every hold of N ms or more on stderr, by phase (at() names
// the phase from there on), so the waits readers see can be traced to what held the lock (11/16/2026: reader latency)
class ObjMutex {
public:
  void lock() { m.lock(); if(depth++ == 0) begin_hold(); }
  bool try_lock() { if(!m.try_lock()) return false; if(depth++ == 0) begin_hold(); return true; }
  void unlock() { if(--depth == 0) end_hold(); m.unlock(); }
  bool Once() const { return depth == 1; }  // only meaningful to the thread that holds it
  std::atomic<int> windows{0};              // threads inside an Unlocked window (the lock given up for a while)
#ifdef GXREPL_TEST_HOOKS
  void at(const char *s) { if(depth <= 0) return; seg_end(); site = s; seg0 = clk::now(); }
  static long thresh() { static long t = getenv("OBJREPL_TEST_HOLD_MS") ? atol(getenv("OBJREPL_TEST_HOLD_MS")) : -1; return t; }
  static void note(const char *what, double ms) {
    if(thresh() >= 0 && ms >= thresh()) { fprintf(stderr, "objrepl hold: %s %.1f ms\n", what, ms); fflush(stderr); }
  }
private:
  typedef std::chrono::steady_clock clk;
  clk::time_point t0, seg0; const char *site = 0, *first = 0;
  void begin_hold() { t0 = seg0 = clk::now(); site = first = 0; }
  void seg_end() {
    if(!first) first = site;
    if(thresh() < 0 || !site) return;
    note(site, std::chrono::duration<double, std::milli>(clk::now() - seg0).count());
  }
  void end_hold() {
    if(thresh() < 0) return;
    seg_end();
    double ms = std::chrono::duration<double, std::milli>(clk::now() - t0).count();
    if(ms >= thresh()) { std::string w = std::string("total (from ") + (first ? first : "?") + ")"; note(w.c_str(), ms); }
  }
#else
  void at(const char *) {}
  static void note(const char *, double) {}
private:
  void begin_hold() {}
  void end_hold() {}
#endif
  int depth = 0;                    // changed only by the owner, with m held
  std::recursive_mutex m;
};
// the lock given up for a scope by a thread that holds it once (see ObjMutex::Once()), and taken again
struct Unlocked {
  ObjMutex &m; bool did;
  Unlocked(ObjMutex &x) : m(x), did(x.Once()) { if(did) { m.windows++; m.unlock(); } }
  ~Unlocked() { if(did) { m.lock(); m.windows--; } }
};

bool sync_files(const std::vector<std::string> &paths);   // below: sync by path
int sync_path(const std::string &f, bool data_only);

// 7.5 beta 3: ROOT/epochs, the epoch history: "EPOCH BASE_SEQ BASE_DIGEST" lines, one appended at each Takeover()
// (the seq and running digest the node took over at). Written and synced before the takeover's state commit, so a
// line may name an epoch the state never reached; only the line of the node's current epoch is used
bool epoch_line_add(const std::string &root, uint64_t epoch, uint64_t seq, uint64_t dig)
{
  std::string f = path(root, "epochs");
  FILE *o = fopen(f.c_str(), "ab");
  if(!o) return false;
  char line[96]; snprintf(line, sizeof(line), "%llu %llu %016llx\n", (unsigned long long)epoch, (unsigned long long)seq, (unsigned long long)dig);
  bool ok = fputs(line, o) >= 0 && fflush(o) == 0;
  fclose(o);
  return ok && sync_files({f});
}
bool epoch_base(const std::string &root, uint64_t epoch, uint64_t &seq, uint64_t &dig)
{
  std::string t = read_file(path(root, "epochs"));
  bool found = false; size_t i = 0;
  while(i < t.size()) {
    size_t j = t.find('\n', i); if(j == std::string::npos) j = t.size();
    unsigned long long e = 0, q = 0, d = 0;
    if(sscanf(t.substr(i, j - i).c_str(), "%llu %llu %llx", &e, &q, &d) == 3 && e == epoch) { seq = q; dig = d; found = true; }
    i = j + 1;
  }
  return found;
}

// ---------- one database: NAME.gxd + NAME.id.btx ----------
struct Db {
  std::string name;
  gxDatabase db;
  IdKey type;
  gxBtree bt{type, ORDER};
  std::map<uint64_t, FAU_t> where;        // id -> the record's address (a live object or a tombstone)
  std::map<FAU_t, uint64_t> who;          // address -> id (records and forwarding stubs)
  std::set<uint64_t> tombs;               // ids whose record is a tombstone
  std::map<FAU_t, uint64_t> stubs;        // forwarding stubs: address -> id
  std::map<uint64_t, uint32_t> versions;  // id -> version (counts the changes of an object here)
  std::vector<Ix *> ixs;                  // this node's indexes on this database (owned by the Node)

  // the indexes: a live object's entries out (before it changes or goes) and in (after)
  bool ix_drop(uint64_t id) {
    if(ixs.empty() || !live(id)) return true;
    FAU_t a = where[id];
    __ULWORD__ n = db.ObjectLength(a);
    std::vector<unsigned char> rec(n);
    if(n < sizeof(ObjHead) || db.Read(rec.data(), n, a) != gxDBASE_NO_ERROR) return false;
    ObjHead h; memcpy(&h, rec.data(), sizeof(h));
    if(h.len > n - sizeof(h)) return false;
    for(Ix *x : ixs) if(!x->del(id, rec.data() + sizeof(h), h.len)) return false;
    return true;
  }
  bool ix_put(uint64_t id, const unsigned char *p, uint32_t n) {
    for(Ix *x : ixs) if(!x->add(id, p, n)) return false;
    return true;
  }
  bool open(const std::string &root) {
    std::string g = path(root, name + ".gxd"), x = path(root, name + ".id.btx");
    if(db.Open(g.c_str()) != gxDBASE_NO_ERROR && db.Create(g.c_str()) != gxDBASE_NO_ERROR) return false;
    if(bt.Open(x.c_str()) != gxDBASE_NO_ERROR && bt.Create(x.c_str()) != gxDBASE_NO_ERROR) return false;
    for(FAU_t a = db.FindFirstObject(); a != (FAU_t)0; a = db.FindNextObject(a)) {
      ObjHead h;
      if(db.Read(&h, sizeof(h), a) != gxDBASE_NO_ERROR) continue;
      who[a] = h.id;
      if(h.flags & OF_STUB) { stubs[a] = h.id; continue; }
      where[h.id] = a;
      versions[h.id] = h.version;
      if(h.flags & OF_TOMB) tombs.insert(h.id);
    }
    return true;
  }
  void close() { bt.Close(); db.Close(); }
  bool live(uint64_t id) const { return where.count(id) && !tombs.count(id); }
  bool write_at(FAU_t a, const ObjHead &h, const unsigned char *p, uint32_t n, __ULWORD__ cap) {
    std::vector<unsigned char> rec(cap, 0);
    memcpy(rec.data(), &h, sizeof(h)); if(n) memcpy(rec.data() + sizeof(h), p, n);
    return db.Write(rec.data(), cap, a) == gxDBASE_NO_ERROR;
  }
  FAU_t alloc_at(uint64_t id, const ObjHead &h, const unsigned char *p, uint32_t n, __ULWORD__ cap) {
    FAU_t a = db.Alloc(cap);
    if(!a || !write_at(a, h, p, n, cap)) return 0;
    IdKey k(id, a), c;
    if(!bt.Insert(k, c)) return 0;
    where[id] = a; who[a] = id;
    return a;
  }
  // an add or an update (also of a tombstone: the object comes back at its address). In place when the bytes fit the
  // record and force_move is off; otherwise the object moves, with capacity want_cap (on a copy: the primary's), and
  // a forwarding stub stays at the old address so links to it stay right (free_old: the old block is freed instead).
  // moved: this call moved it
  bool put(uint64_t id, uint32_t ver, const unsigned char *p, uint32_t n, __ULWORD__ want_cap, bool force_move,
	   bool free_old, bool &moved) {
    moved = false;
    if(!ix_drop(id)) return false;
    ObjHead h; memset(&h, 0, sizeof(h)); h.id = id; h.version = ver; h.len = n;
    __ULWORD__ need = (__ULWORD__)(sizeof(ObjHead) + n), cap = std::max(need, want_cap);
    versions[id] = ver;
    auto old = where.find(id);
    if(old == where.end()) return alloc_at(id, h, p, n, cap) != 0 && ix_put(id, p, n);
    FAU_t oa = old->second;
    __ULWORD__ ocap = db.ObjectLength(oa);
    if(need <= ocap && !force_move) {                        // in place
      if(!write_at(oa, h, p, n, ocap)) return false;
      tombs.erase(id);
      return ix_put(id, p, n);
    }
    IdKey ok(id, oa), c;                                     // the move
    bt.Delete(ok, c);
    where.erase(id); tombs.erase(id);
    FAU_t na = alloc_at(id, h, p, n, cap);
    if(!na) return false;
    for(auto &st : stubs) if(st.second == id) {             // older stubs of this id: straight to the new address
      ObjHead sh; memset(&sh, 0, sizeof(sh)); sh.id = id; sh.flags = OF_STUB; sh.fwd = (uint64_t)na;
      if(!write_at(st.first, sh, 0, 0, db.ObjectLength(st.first))) return false;
    }
    if(free_old) { who.erase(oa); if(!db.Delete(oa)) return false; }
    else {
      ObjHead sh; memset(&sh, 0, sizeof(sh)); sh.id = id; sh.flags = OF_STUB; sh.fwd = (uint64_t)na;
      if(!write_at(oa, sh, 0, 0, ocap)) return false;
      stubs[oa] = id; who[oa] = id;
    }
    moved = true;
    return ix_put(id, p, n);
  }
  // a remove: the record becomes a tombstone at the same address (an unknown id gets a new one)
  bool tombstone(uint64_t id, uint32_t reason, uint64_t seq, __ULWORD__ cap = 0) {
    ObjHead h; memset(&h, 0, sizeof(h)); h.id = id; h.flags = OF_TOMB; h.reason = reason; h.seen_seq = seq;
    auto it = where.find(id);
    if(it != where.end()) {
      if(tombs.count(id)) return true;                       // already one
      if(!ix_drop(id)) return false;
      if(!write_at(it->second, h, 0, 0, db.ObjectLength(it->second))) return false;
      tombs.insert(id);
      return true;
    }
    if(!alloc_at(id, h, 0, 0, std::max((__ULWORD__)sizeof(ObjHead), cap))) return false;
    tombs.insert(id);
    return true;
  }
  // a purge: a tombstone (and stubs naming its id) freed for good; a live object is never purged
  bool purge(uint64_t id) {
    auto it = where.find(id);
    if(it == where.end() || !tombs.count(id)) return true;
    IdKey k(id, it->second), c;
    bt.Delete(k, c);
    if(!db.Delete(it->second)) return false;
    who.erase(it->second); where.erase(it); tombs.erase(id); versions.erase(id);
    for(auto st = stubs.begin(); st != stubs.end();) {
      if(st->second == id) { db.Delete(st->first); who.erase(st->first); st = stubs.erase(st); } else ++st;
    }
    return true;
  }
  bool clear() {                                    // every record and stub (a full send starts from empty)
    for(auto &w : where) if(!ix_drop(w.first)) return false;
    for(auto &w : where) { IdKey k(w.first, w.second), c; bt.Delete(k, c); if(!db.Delete(w.second)) return false; }
    for(auto &st : stubs) if(!db.Delete(st.first)) return false;
    where.clear(); who.clear(); tombs.clear(); stubs.clear(); versions.clear();
    return true;
  }
  // read a record by address, following a forwarding stub once
  bool read(FAU_t a, std::vector<unsigned char> &rec, ObjHead &h) {
    for(int hop = 0; hop < 2; hop++) {
      __ULWORD__ n = db.ObjectLength(a);
      rec.resize(n);
      if(n < sizeof(ObjHead) || db.Read(rec.data(), n, a) != gxDBASE_NO_ERROR) return false;
      memcpy(&h, rec.data(), sizeof(h));
      if(h.len > n - sizeof(h)) return false;
      if(!(h.flags & OF_STUB)) return true;
      a = (FAU_t)h.fwd;
    }
    return false;
  }
};

// the digest log: the running digest after each commit, by seq, kept when the outbox is trimmed (11/16/2026, review),
// so a reconnecting copy's history can always be checked, however far behind it is
struct DKeyData { uint64_t seq; uint64_t digest; };
class DKey : public DatabaseKeyB {
public:
  DKey(uint64_t s = 0, uint64_t d = 0) : DatabaseKeyB((char *)&k) { k.seq = s; k.digest = d; }
  size_t KeySize() { return sizeof(k); }
  int CompareKey(const DatabaseKeyB &o) const {
    const DKeyData *x = (const DKeyData *)o.db_key;
    return k.seq < x->seq ? -1 : k.seq > x->seq ? 1 : 0;      // by seq only
  }
  int operator==(const DatabaseKeyB &o) const { return CompareKey(o) == 0; }
  int operator>(const DatabaseKeyB &o) const { return CompareKey(o) > 0; }
  DKeyData k;
};

// ---------- a node: its databases, state and (primary) outbox under one journal ----------
struct Node {
  gxdJournal j;
  gxDatabase sdb, ob;
  // 7.5 beta 3: chg (ROOT/objrepl.changes.gxd) holds, for each commit this node made as a primary since its last
  // takeover, the (database, id) of its ops, so an incremental rejoin knows what changed past the takeover even after
  // the outbox was trimmed; rjs (ROOT/objrepl.rejoin.gxd), on a former primary waiting for an incremental rejoin, the
  // same for its commits its copy never acknowledged. One block per commit: u64 seq, u32 n, n x (str db, u64 id)
  gxDatabase chg, rjs;
  bool primary = false, is_open = false;
  std::map<std::string, std::unique_ptr<Db>> dbs;
  std::vector<std::unique_ptr<Ix>> ixs;
  std::vector<std::unique_ptr<Db>> dead_dbs;   // attached, then failed: kept until the journal is closed
  std::vector<std::unique_ptr<Ix>> dead_ixs;
  DKey dtype;
  gxBtree dlog{dtype, ORDER};                  // the digest log (ROOT/digests.btx)
  State st;
  std::string root, err;
#if defined(_WIN32)
  HANDLE lock_h = INVALID_HANDLE_VALUE;
#else
  int lock_fd = -1;
#endif
  const std::vector<LinkDef> *links = 0;
  std::set<std::string> opening;          // OpenDatabase(): databases being prepared without the lock (11/16/2026)
  const std::vector<IxDef> *ixdefs = 0;
  std::function<void(const std::string &)> log;
  uint64_t seen_seq = 0;            // the commit being applied (placeholders record it)

  bool fail(const std::string &w) { err = w; return false; }
  // ROOT/objrepl.lock, held while the node is open: one process per node (11/16/2026, review: a takeover run
  // against a serving copy was overwritten by the copy's next state write)
  // wait_ms: how long to keep trying while the lock is held (review 4: a Status() replaying the journal held it for
  // a moment and a node starting then failed); a node held by a running process is still refused after that
  int lock_wait_ms = 5000;          // 7.5: gxObjReplConfig::lock_wait (Open() of a root another process holds)
  bool lock(int wait_ms = 5000) {
    std::string f = path(root, "objrepl.lock");
    for(int waited = 0;; waited += 50) {
#if defined(_WIN32)
      lock_h = CreateFileA(f.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
      if(lock_h != INVALID_HANDLE_VALUE) return true;
      if(waited >= wait_ms) return fail(root + " is in use by another process (or " + f + " cannot be opened)");
#else
      lock_fd = ::open(f.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);   // not inherited by child processes (review 2)
      if(lock_fd < 0) return fail("cannot open " + f);
      if(flock(lock_fd, LOCK_EX | LOCK_NB) == 0) return true;
      ::close(lock_fd); lock_fd = -1;
      if(waited >= wait_ms) return fail(root + " is in use by another process");
#endif
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  }
  void unlock() {
#if defined(_WIN32)
    if(lock_h != INVALID_HANDLE_VALUE) { CloseHandle(lock_h); lock_h = INVALID_HANDLE_VALUE; }
#else
    if(lock_fd >= 0) { flock(lock_fd, LOCK_UN); ::close(lock_fd); lock_fd = -1; }
#endif
  }
  bool begin() { if(j.Begin() != gxDBASE_NO_ERROR) return fail("cannot begin a transaction"); return true; }
  // a commit that failed leaves the transaction open: it is rolled back (the caller reopens the node)
  bool commit() {
#ifdef GXREPL_TEST_HOOKS
    auto t0 = std::chrono::steady_clock::now(); unsigned long ck = j.checkpoints;
    gxDatabaseError e = j.Commit();
    if(e == gxDBASE_NO_ERROR) e = j.WriteCommitted();
    ObjMutex::note(j.checkpoints != ck ? "journal commit with a checkpoint" : "journal commit",
		   std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
    if(e == gxDBASE_NO_ERROR) return true;
#else
    // durable, then the pages to the files (write-back mode: as a write-through commit)
    if(j.Commit() == gxDBASE_NO_ERROR && j.WriteCommitted() == gxDBASE_NO_ERROR) return true;
#endif
    if(j.InTransaction()) j.Rollback(); return fail("a commit failed");
  }
  // have_lock: the caller holds ROOT/objrepl.lock already (a reopen keeps it, so nothing can change the node between)
  unsigned long opens = 0;          // open() calls: a reopen changes it
  bool open(const std::string &r, bool prim, bool have_lock = false) {
    root = r; primary = prim; opens++;
    std::error_code ec; fs::create_directories(root, ec);
    if(!have_lock && !lock(lock_wait_ms)) return false;
    if(j.Open(path(root, "node.jnl").c_str()) != gxDBASE_NO_ERROR) { unlock(); return fail(std::string("cannot open the journal: ") + gxdJournal::RecoverError()); }
    j.SetCheckpointSize(CKPT_HARD);                            // the checkpoints come at CKPT_SOFT (checkpoint_soon())
    // write-back: a commit's pages reach the files when commit() (or gxObjRepl::Impl::jcommit()) writes them, after
    // the journal is synced, so a commit can be synced without the node's lock (11/16/2026)
    j.SetWriteMode(gxdJOURNAL_WRITE_BACK);
    // 7.4.1: page writes under the journal's write lock, so gxObjReplReader in another process never reads a batch
    // half written
    j.SetWriteLock(1);
    is_open = true;                                            // from here close() undoes what was opened
    // an interrupted set-aside (review 4): the files still here are originals not moved yet; they go into prev.tmp
    // (never over a file already there) and prev.tmp becomes prev, before this open makes any node file
    {
      std::string tmp = path(root, "prev.tmp"), prev = path(root, "prev");
      if(exists(tmp) && !exists(prev)) {
	std::error_code ec;
	for(auto &f : list_dir(root)) {
	  if(!node_file(f)) continue;
	  if(exists((fs::path(tmp) / f).string())) continue;
	  fs::rename(path(root, f), (fs::path(tmp) / f), ec);
	  if(ec) return fail("cannot finish setting " + f + " aside: " + ec.message());
	}
	fs::rename(tmp, prev, ec);
	if(ec) return fail("cannot rename " + tmp + ": " + ec.message());
      }
    }
    std::string dl = path(root, "digests.btx");
    if(dlog.Open(dl.c_str()) != gxDBASE_NO_ERROR && dlog.Create(dl.c_str()) != gxDBASE_NO_ERROR) return fail("cannot open " + dl);
    if(j.Attach(&dlog) != gxDBASE_NO_ERROR) return fail("cannot attach " + dl);
    // a node is new (and may get a fresh state) only if it holds no database and no digest log yet: a lost or
    // unwritten state.gxd on a node with data is an error, never a fresh start (review 2)
    bool has_data = false;
    { DKey k; if(dlog.FindFirst(k)) has_data = true; }
    for(auto &n : list_dir(root))
      if(ends_with(n, ".gxd")) { std::string nm = n.substr(0, n.size() - 4); if(good_name(nm)) has_data = true; }
    std::string s = path(root, "state.gxd");
    bool new_state = !exists(s);
    if(new_state && has_data) return fail(s + " is missing but the node holds data: refused (nothing changed)");
    if(sdb.Open(s.c_str()) != gxDBASE_NO_ERROR) {
      if(has_data) return fail("cannot open " + s);
      remove_file(s); new_state = true;                        // a node that stopped inside its first open
      if(sdb.Create(s.c_str(), (FAU_t)sizeof(State)) != gxDBASE_NO_ERROR) return fail("cannot create " + s);
    }
    if(j.Attach(&sdb) != gxDBASE_NO_ERROR) return fail("cannot attach " + s);
    for(auto fx : {std::make_pair(&chg, "objrepl.changes.gxd"), std::make_pair(&rjs, "objrepl.rejoin.gxd")}) {
      std::string f = path(root, fx.second);
      if(fx.first->Open(f.c_str()) != gxDBASE_NO_ERROR && fx.first->Create(f.c_str()) != gxDBASE_NO_ERROR) return fail("cannot open " + f);
      if(j.Attach(fx.first) != gxDBASE_NO_ERROR) return fail("cannot attach " + f);
    }
    if(primary) {
      std::string o = path(root, "outbox.gxd");
      if(ob.Open(o.c_str()) != gxDBASE_NO_ERROR && ob.Create(o.c_str()) != gxDBASE_NO_ERROR) return fail("cannot open " + o);
      if(j.Attach(&ob) != gxDBASE_NO_ERROR) return fail("cannot attach " + o);
    }
    // the state: a new node gets a fresh one; an existing state that cannot be read (or is from another version) is
    // an error, never silently replaced (11/16/2026, review)
    if(sdb.Read(&st, sizeof(st), (FAU_t)sdb.FileHeaderSize()) != gxDBASE_NO_ERROR || st.magic != STATE_MAGIC) {
      if(has_data) return fail(s + " cannot be read or is from another objrepl version: refused (nothing changed)");
      memset(&st, 0, sizeof(st)); st.magic = STATE_MAGIC; st.next_seq = 1; st.epoch = 1;
      st.cur_digest = st.acked_digest = st.applied_digest = DIGEST0;
      if(!begin()) return false;
      put_state();
      if(!commit()) return fail("cannot write the state");
    }
    for(auto &n : list_dir(root))                              // the databases already here
      if(ends_with(n, ".gxd")) {
	std::string nm = n.substr(0, n.size() - 4);
	// one OpenDatabase() is preparing without the lock (a reopen() from another thread meanwhile): it attaches it
	// (review: a reopen built the same index over the preparer's)
	if(opening.count(nm)) continue;
	if(good_name(nm) && !get(nm)) return false;
      }
    // old data kept from a full send that has ended (the removal failed or was cut short): it goes now (review 4)
    if(!st.in_snapshot && (exists(path(root, "prev")) || exists(path(root, "prev.tmp")))) drop_aside();
    return true;
  }
  // the digest log: the entry for SEQ (replaced if there), or the lookup
  bool log_digest(uint64_t seq, uint64_t d) { DKey k(seq, d), c; DKey old(seq); dlog.Delete(old, c); return dlog.Insert(k, c) != 0; }
  bool digest_at(uint64_t seq, uint64_t &d) { DKey k(seq), c; if(!dlog.Find(k, c)) return false; d = k.k.digest; return true; }
  bool clear_digests() {
    DKey k, c;
    std::vector<uint64_t> all;
    for(int ok = dlog.FindFirst(k); ok; ok = dlog.FindNext(k, c)) all.push_back(k.k.seq);
    for(uint64_t q : all) { DKey d(q); if(!dlog.Delete(d, c)) return false; }
    return true;
  }
  // the database NAME, opened (or created) and attached; never inside a transaction
  // READY: NAME's files opened and scanned already (prepare_db(), without the node's lock), or 0 to open them here
  Db *get(const std::string &name, std::unique_ptr<Db> ready = nullptr) {
    auto it = dbs.find(name);
    if(it != dbs.end()) { if(ready) ready->close(); return it->second.get(); }
    if(!good_name(name)) { if(ready) ready->close(); err = "a bad database name: " + name; return 0; }
    if(j.InTransaction()) { if(ready) ready->close(); err = "the database " + name + " is not open (open it before Begin())"; return 0; }
    // a database whose open failed keeps its objects attached until close: it is not opened a second time over them
    // (review 3: the dead objects' stale headers were written at close and emptied an index)
    for(auto &d : dead_dbs) if(d->name == name) { if(ready) ready->close(); err = "the database " + name + " failed to open earlier: Close() and Open() again"; return 0; }
    std::unique_ptr<Db> d(ready ? std::move(ready) : std::unique_ptr<Db>(new Db));
    d->name = name;
    if(!d->db.IsOpen() && !d->open(root)) { d->close(); err = "cannot open the database " + name; return 0; }
    if(j.Attach(&d->db) != gxDBASE_NO_ERROR || j.Attach(&d->bt) != gxDBASE_NO_ERROR) {
      dead_dbs.push_back(std::move(d)); err = "cannot attach " + name; return 0;   // the journal may hold one of them
    }
    Db *p = d.get();
    if(ixdefs) for(auto &def : *ixdefs) if(def.db == name && !open_ix(p, def)) {
      // the database is not kept open without its index (review 2): it and the indexes opened for it so far stay
      // alive (the journal holds them) but out of use until close
      for(auto it = ixs.begin(); it != ixs.end();) { if((*it)->def.db == name) { dead_ixs.push_back(std::move(*it)); it = ixs.erase(it); } else ++it; }
      dead_dbs.push_back(std::move(d));
      if(err.empty()) err = "cannot open the index " + def.name;
      return 0;
    }
    dbs[name] = std::move(d);
    // an index file of NAME this node is not configured for would go stale while the objects change: it is dropped
    // (an index is derived data), so configuring it again builds it from the objects
    std::string pre = name + ".";
    if(ixdefs) for(auto &f : list_dir(root)) {
      if(f.compare(0, pre.size(), pre) != 0) continue;
      std::string ix;
      if(ends_with(f, ".ix.btx") && f.size() > pre.size() + 7) ix = f.substr(pre.size(), f.size() - pre.size() - 7);
      else if(ends_with(f, ".vix") && f.size() > pre.size() + 4) ix = f.substr(pre.size(), f.size() - pre.size() - 4);
      else continue;
      bool configured = false;
      if(ixdefs) for(auto &def : *ixdefs) if(def.db == name && def.name == ix) configured = true;
      if(!configured) {
	remove_file(path(root, f)); remove_file(path(root, f) + ".def"); remove_file(path(root, f) + ".building");
	say("index " + ix + " on " + name + " is not configured here: dropped");
      }
    }
    return p;
  }
  void say(const std::string &m) { if(log) log(m); }
  // 11/16/2026 (lock holds): OpenDatabase() on a primary opens NAME's files, reads its objects' headers and builds any
  // index that must be built (new, unfinished or of another definition) here, WITHOUT the node's lock, so readers do
  // not wait for it (a new primary held the lock 74 s building indexes). Nobody else can reach NAME meanwhile: it is
  // not in dbs (Put() and the other writers need it there, readers find nothing) and OpenDatabase() lets one caller
  // prepare it at a time. The index is built outside the journal: ROOT/<index file>.building marks it unfinished (a
  // crash in the middle builds it again at the next open, as before), the files are synced before the .def is written
  // and the mark removed. get() under the lock then finds the index ready (opened and attached, not built again).
  // DEFS, ROOT: copies taken under the lock. STOP: Close() has begun (the build gives up)
  static bool prepare_db(const std::string &root, const std::string &name, const std::vector<IxDef> &defs,
			 std::unique_ptr<Db> &out, std::string &e, std::function<bool()> stop, std::function<void(const std::string &)> log) {
    std::unique_ptr<Db> d(new Db);
    d->name = name;
    bool was = exists(path(root, name + ".gxd"));
    if(!d->open(root)) { d->close(); e = "cannot open the database " + name; return false; }
    // a new database's files and their directory entries are durable before the journal holds pages for them
    // (review: after a power loss Recover() could not open them and the node did not open)
    if(!was && (!sync_files({path(root, name + ".gxd"), path(root, name + ".id.btx")}) || sync_path(root, false) == 0)) {
      d->close(); e = "cannot sync the new database " + name; return false;
    }
    for(auto &def : defs) {
      if(def.db != name) continue;
      Ix x; x.def = def;
      std::string f = x.file(root), mark = f + ".building", deff = f + ".def";
      std::string want = std::string(1, def.kind) + ":" + std::to_string(def.off) + ":" + std::to_string(def.len) + "\n";
      bool fresh = !exists(f) || exists(mark) || read_file(deff) != want;
      if(!fresh) {
	bool ok = def.kind == 'B' ? x.bt.Open(f.c_str()) == gxDBASE_NO_ERROR : x.vx.Open(f.c_str(), gxDBASE_READWRITE, x.spec().c_str()) == gxv_NO_ERROR;
	if(ok) { x.close(); continue; }                        // ready: get() opens and attaches it
	fresh = true;                                          // one that will not open is built again
      }
      if(FILE *m = fopen(mark.c_str(), "w")) fclose(m); else { d->close(); e = "cannot write " + mark; return false; }
      remove_file(f); remove_file(deff);
      Ix b; b.def = def;
      if(def.kind == 'B' ? b.bt.Create(f.c_str()) != gxDBASE_NO_ERROR
			 : b.vx.Create(f.c_str(), def.len, gxvFLOAT32, gxvL2, 16, b.spec().c_str()) != gxv_NO_ERROR) { d->close(); e = "cannot create " + f; return false; }
      size_t built = 0;
      for(auto &w : d->where) {
	if(d->tombs.count(w.first)) continue;
	std::vector<unsigned char> rec; ObjHead h;
	if(!d->read(w.second, rec, h) || !b.add(w.first, rec.data() + sizeof(h), h.len)) { b.close(); d->close(); e = "cannot build " + f; return false; }
	if(++built % 1024 == 0 && stop()) { b.close(); d->close(); e = "the node was closed while " + name + " was opened"; return false; }
#ifdef GXREPL_TEST_HOOKS
	if(built == 50 && getenv("OBJREPL_TEST_KILL_IN_IXBUILD")) _exit(9);   // test only: killed inside the build
#endif
      }
      b.close();
      // durable before it is marked finished: a crash after the mark is gone must find a whole index
      if(!sync_files({f})) { d->close(); e = "cannot sync " + f; return false; }
      FILE *df = fopen(deff.c_str(), "wb");
      if(!df || fputs(want.c_str(), df) < 0 || fclose(df) != 0) { d->close(); e = "cannot write " + deff; return false; }
      if(!sync_files({deff})) { d->close(); e = "cannot sync " + deff; return false; }
      remove_file(mark);
      sync_files({root});
      if(built) log("index " + def.name + " on " + def.db + " built from " + std::to_string(built) + " object(s) (without the lock)");
    }
    out = std::move(d);
    return true;
  }
  // an index of D: opened, or created and built from the objects already here (in one transaction)
  // ROOT/<index file>.def holds the definition the file was built for: another offset, length or kind under the same
  // name rebuilds it (11/16/2026, review: a changed definition was reopened as it was and every write then failed)
  bool open_ix(Db *d, const IxDef &def) {
    std::unique_ptr<Ix> x(new Ix); x->def = def;
    std::string f = x->file(root), mark = f + ".building", deff = f + ".def";
    std::string want = std::string(1, def.kind) + ":" + std::to_string(def.off) + ":" + std::to_string(def.len) + "\n";
    bool fresh = !exists(f) || exists(mark) || read_file(deff) != want;
    if(!fresh) {                                               // an existing index that will not open is built again
      if(def.kind == 'B' ? x->bt.Open(f.c_str()) != gxDBASE_NO_ERROR : x->vx.Open(f.c_str(), gxDBASE_READWRITE, x->spec().c_str()) != gxv_NO_ERROR) {
	say("index " + def.name + " on " + def.db + " does not open: built again");
	x.reset(new Ix); x->def = def; fresh = true;
      }
    }
    if(fresh) {
      if(FILE *m = fopen(mark.c_str(), "w")) fclose(m); else return fail("cannot write " + mark);
      remove_file(f); remove_file(deff);
      if(def.kind == 'B' ? x->bt.Create(f.c_str()) != gxDBASE_NO_ERROR
			 : x->vx.Create(f.c_str(), def.len, gxvFLOAT32, gxvL2, 16, x->spec().c_str()) != gxv_NO_ERROR) return fail("cannot create " + f);
    }
    gxDatabaseError ae = def.kind == 'B' ? j.Attach(&x->bt) : j.Attach(x->vx.Database());
    if(ae != gxDBASE_NO_ERROR) { dead_ixs.push_back(std::move(x)); return fail("cannot attach " + f); }
    if(fresh) {
      size_t built = 0;
      if(!begin()) { dead_ixs.push_back(std::move(x)); return false; }
      for(auto &w : d->where) {
	if(d->tombs.count(w.first)) continue;
	std::vector<unsigned char> rec; ObjHead h;
	if(!d->read(w.second, rec, h) || !x->add(w.first, rec.data() + sizeof(h), h.len)) { j.Rollback(); dead_ixs.push_back(std::move(x)); return fail("cannot build " + f); }
	built++;
#ifdef GXREPL_TEST_HOOKS
	if(built == 50 && getenv("OBJREPL_TEST_KILL_IN_IXBUILD")) _exit(9);   // test only: killed inside the build
#endif
      }
      if(!commit()) { dead_ixs.push_back(std::move(x)); return fail("cannot commit the build of " + f); }
      FILE *df = fopen(deff.c_str(), "wb");                    // binary: the compare is byte for byte (Windows wrote \r\n)
      if(!df || fputs(want.c_str(), df) < 0 || fclose(df) != 0) { dead_ixs.push_back(std::move(x)); return fail("cannot write " + deff); }
      remove_file(mark);
      if(built) say("index " + def.name + " on " + def.db + " built from " + std::to_string(built) + " object(s)");
    }
    d->ixs.push_back(x.get());
    ixs.push_back(std::move(x));
    return true;
  }
  void close(bool keep_lock = false) {
    if(!is_open) { if(!keep_lock) unlock(); return; }
    if(j.InTransaction()) j.Rollback();
    j.Close();
    for(auto &x : ixs) x->close();
    for(auto &x : dead_ixs) x->close();
    for(auto &d : dbs) d.second->close();
    for(auto &d : dead_dbs) d->close();
    dlog.Close();
    if(primary) ob.Close();
    chg.Close(); rjs.Close();
    sdb.Close();
    ixs.clear(); dbs.clear(); dead_ixs.clear(); dead_dbs.clear();
    is_open = false;
    if(!keep_lock) unlock();
  }
  // a full send starts from empty: the node's databases, indexes and digest log are moved aside into ROOT/prev as one
  // unit (ROOT/prev.tmp while it fills, then a rename) and the node opens again empty, instead of deleting the
  // objects one by one (11/16/2026: a 1M-object rejoin spent most of its 24 minutes clearing). ROOT/prev is removed
  // once the full send has ended (SNAP_END committed). After a crash in the middle: a complete ROOT/prev holds the old
  // data and what is here now is a partial full send (deleted); an incomplete ROOT/prev.tmp is completed first
  static bool node_file(const std::string &f) {
    if(f == "digests.btx") return true;
    size_t dot = f.find('.');
    if(dot == std::string::npos || !good_name(f.substr(0, dot))) return false;
    for(const char *e : {".gxd", ".id.btx", ".ix.btx", ".vix", ".def", ".building"}) if(ends_with(f, e)) return true;
    return false;
  }
  bool set_aside() {
    std::string prev = path(root, "prev"), tmp = path(root, "prev.tmp");
    bool done_before = exists(prev);
    close(true);                                               // the lock is kept
    std::error_code ec;
    if(!done_before) fs::create_directories(tmp, ec);
    for(auto &f : list_dir(root)) {
      if(!node_file(f)) continue;
      if(done_before) remove_file(path(root, f));              // a partial full send: the old data is in prev already
      else if(exists((fs::path(tmp) / f).string())) remove_file(path(root, f));   // the original is in prev.tmp already
      else { fs::rename(path(root, f), (fs::path(tmp) / f), ec); if(ec) return fail("cannot move " + f + " aside: " + ec.message()); }
    }
    if(!done_before) { fs::rename(tmp, prev, ec); if(ec) return fail("cannot rename " + tmp + ": " + ec.message()); }
    return open(root, false, true);
  }
  bool drop_aside() {
    std::error_code ec; fs::remove_all(path(root, "prev"), ec); fs::remove_all(path(root, "prev.tmp"), ec);
    if(exists(path(root, "prev")) || exists(path(root, "prev.tmp"))) { say("cannot remove " + path(root, "prev") + ": remove it by hand"); return false; }
    return true;
  }
  // the digest log: entries below CUTOFF out (the window of DLOG_KEEP commits)
  bool trim_digests(uint64_t cutoff, size_t limit = 10000) {   // at most LIMIT entries a call (a backlog goes in steps)
    DKey k, c;
    std::vector<uint64_t> old;
    for(int ok = dlog.FindFirst(k); ok && k.k.seq < cutoff && old.size() < limit; ok = dlog.FindNext(k, c)) old.push_back(k.k.seq);
    for(uint64_t q : old) { DKey d(q); if(!dlog.Delete(d, c)) return false; }
    return true;
  }
  // 7.5 beta 3: the change lists (chg, rjs): one block per commit (u64 seq, u32 n, n x (str db, u64 id))
  typedef std::set<std::pair<std::string, uint64_t>> IdSet;
  static bool list_add(gxDatabase &f, uint64_t seq, const IdSet &ids) {
    Buf b; b.u64(seq); b.u32((uint32_t)ids.size());
    for(auto &x : ids) { b.str(x.first); b.u64(x.second); }
    FAU_t a = f.Alloc((__ULWORD__)b.b.size());
    return a && f.Write(b.b.data(), (__ULWORD__)b.b.size(), a) == gxDBASE_NO_ERROR;
  }
  // every (database, id) of the blocks past AFTER; false when a block cannot be read
  static bool list_since(gxDatabase &f, uint64_t after, IdSet &out, uint64_t *first = 0) {
    if(first) *first = 0;
    for(FAU_t a = f.FindFirstObject(); a != (FAU_t)0; a = f.FindNextObject(a)) {
      __ULWORD__ n = f.ObjectLength(a);
      std::vector<unsigned char> b(n);
      if(n < 12 || f.Read(b.data(), n, a) != gxDBASE_NO_ERROR) return false;
      Rd r(b.data(), b.size()); uint64_t q = r.u64(); uint32_t k = r.u32();
      if(first && (!*first || q < *first)) *first = q;
      if(q <= after) continue;
      for(uint32_t i = 0; i < k; i++) { std::string d = r.str(); uint64_t id = r.u64(); if(!r.ok) return false; out.insert({d, id}); }
      if(!r.ok) return false;
    }
    return true;
  }
  // blocks with a seq below CUTOFF out (every block when CUTOFF is UINT64_MAX), at most LIMIT
  static bool list_trim(gxDatabase &f, uint64_t cutoff, size_t limit = (size_t)-1) {
    std::vector<FAU_t> old;
    for(FAU_t a = f.FindFirstObject(); a != (FAU_t)0 && old.size() < limit; a = f.FindNextObject(a)) {
      unsigned char h[8];
      if(f.Read(h, 8, a) != gxDBASE_NO_ERROR) return false;
      Rd r(h, 8);
      if(cutoff == UINT64_MAX || r.u64() < cutoff) old.push_back(a);
    }
    for(FAU_t a : old) if(!f.Delete(a)) return false;
    return true;
  }
  // digest log entries past SEQ out (a former primary's own commits, replaced in an incremental rejoin)
  bool dlog_drop_after(uint64_t seq) {
    DKey k(seq + 1), c; std::vector<uint64_t> v;
    for(int ok = dlog.PartialFind(k, c); ok; ok = dlog.FindNext(k, c)) if(k.k.seq > seq) v.push_back(k.k.seq);
    for(uint64_t q : v) { DKey d(q); if(!dlog.Delete(d, c)) return false; }
    return true;
  }
  // the state lives in state.gxd's static area, right after the file header
  bool put_state() { return sdb.Write(&st, sizeof(st), (FAU_t)sdb.FileHeaderSize()) == gxDBASE_NO_ERROR; }
  // link fields of DB's object bytes: to ids (to_ids) or to this node's addresses. A removed target is a tombstone,
  // so its address still names its id. An id this node does not hold yet gets a placeholder tombstone (inside the
  // open transaction), so a link is never dangling or 0; a later add of that id replaces it in place
  bool translate(const std::string &db, unsigned char *p, uint32_t n, bool to_ids) {
    for(auto &d : *links) {
      if(d.db != db || (size_t)d.off + 8 > n) continue;
      uint64_t v; memcpy(&v, p + d.off, 8);
      uint64_t out = 0;
      Db *t = get(d.target);
      if(v && !t) return false;
      if(v && to_ids) { auto it = t->who.find((FAU_t)v); if(it == t->who.end()) return fail("a link in " + db + " names no object in " + d.target); out = it->second; }
      else if(v) {
	auto it = t->where.find(v);
	if(it == t->where.end()) {
	  if(!j.InTransaction() || !t->tombstone(v, TR_UNKNOWN_TARGET, seen_seq, PLACEHOLDER_RESERVE)) return false;
	  say("placeholder tombstone " + t->name + " id " + std::to_string(v) + " (a link at seq " + std::to_string(seen_seq) + " names an id not here yet)");
	  it = t->where.find(v);
	}
	out = (uint64_t)it->second;
      }
      memcpy(p + d.off, &out, 8);
    }
    return true;
  }
};

// a node that is not running (its lock is free): the journal's committed transactions are written to the files first,
// so Digest(), IndexCheck() and Status() read what an Open() would see (property test: Status() after a SIGKILL read
// applied 1 where the node, opened, was at 2). A running node is read as it is
void settle(const std::string &root)
{
  if(!exists(path(root, "node.jnl"))) return;
  Node n; n.root = root;
  if(!n.lock(0)) return;                                      // running (or another settle): read as it is
  gxdJournal::Recover(path(root, "node.jnl").c_str(), 0, 0);
  n.unlock();
}

// one file synced to disk by name, by its own handle (nothing of an open node is used, so it runs without the node's
// lock): 1 synced, 0 failed, -1 gone, 2 (Windows) not opened for the sync (another handle denies write sharing) (the node was closed or set aside meanwhile: the caller checks that). DATA_ONLY:
// fdatasync where there is one. Windows (11/16/2026): a second handle with write access (DataReel opens its files
// sharing read and write) and FlushFileBuffers, which flushes the file whichever handle wrote it; a directory needs
// no flush there (NTFS logs its metadata) and counts as synced
int sync_path(const std::string &f, bool data_only)
{
#if !defined(_WIN32)
  int fd = ::open(f.c_str(), O_RDONLY | O_CLOEXEC);
  if(fd < 0) return errno == ENOENT ? -1 : 0;
#if defined(__linux__)
  int r = data_only ? fdatasync(fd) : fsync(fd);
#else
  (void)data_only; int r = fsync(fd);
#endif
  ::close(fd);
  return r == 0 ? 1 : 0;
#else
  (void)data_only;
  DWORD at = GetFileAttributesA(f.c_str());
  if(at == INVALID_FILE_ATTRIBUTES) { DWORD e = GetLastError(); return e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND ? -1 : 0; }
  if(at & FILE_ATTRIBUTE_DIRECTORY) return 1;
  HANDLE h = CreateFileA(f.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING,
			 FILE_ATTRIBUTE_NORMAL, 0);
  if(h == INVALID_HANDLE_VALUE) {
    DWORD e = GetLastError();
    if(e == ERROR_SHARING_VIOLATION) return 2;               // another program's handle denies write sharing: not synced,
    return e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND ? -1 : 0;   // nothing failed (the caller syncs another way)
  }
  BOOL ok = FlushFileBuffers(h);
  CloseHandle(h);
  return ok ? 1 : 0;
#endif
}

// files synced to disk by name (see sync_path()). Returns false if one cannot be synced (one that is gone meanwhile
// counts as synced: the node was closed or set aside, and the caller checks that)
bool sync_files(const std::vector<std::string> &paths)
{
  bool ok = true;
  for(auto &f : paths) { int r = sync_path(f, false); if(r == 0 || r == 2) ok = false; }
  return ok;
}

// ---------- the wire: one gxStream block per frame ----------
bool send_frame(gxStream &st, const std::vector<unsigned char> &b)
{
  return st.WriteBlock(b.data(), (int)b.size()) == 0;
}
bool recv_frame(gxStream &st, std::vector<unsigned char> &b)
{
  gxBlockHeader gx;
  if(st.ReadHeader(gx) != 0) return false;
  return st.ReadBlock(b, gx) == 0;
}

long long my_pid()
{
#if defined(_WIN32)
  return (long long)_getpid();
#else
  return (long long)getpid();
#endif
}
bool pid_alive(long long pid)
{
  if(pid <= 0) return false;
#if defined(_WIN32)
  HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
  if(!h) return false;
  DWORD code = 0; BOOL ok = GetExitCodeProcess(h, &code); CloseHandle(h);
  return ok && code == STILL_ACTIVE;
#else
  return kill((pid_t)pid, 0) == 0;
#endif
}

// ---------- status ----------
// ROOT/objrepl.status, "key value" lines replaced whole (a temp file and a rename), at most once a second and at once
// on a connect, a drop, a refusal and the end
struct Mon {
  std::string root;
  std::map<std::string, std::string> v;
  time_t last_write = 0;
  time_t behind_since = 0;            // primary: since when the copy has been behind (0 = current)
  void set(const std::string &k, const std::string &x) { v[k] = x; }
  void set(const std::string &k, unsigned long long x) { v[k] = std::to_string(x); }
  void write(bool force) {
    time_t now = time(0);
    if(root.empty() || (!force && now == last_write)) return;
    last_write = now;
    set("updated", (unsigned long long)now); set("pid", (unsigned long long)my_pid());
    std::string f = path(root, "objrepl.status"), t = f + ".tmp";
    FILE *o = fopen(t.c_str(), "w");
    if(!o) return;
    for(auto &e : v) fprintf(o, "%s %s\n", e.first.c_str(), e.second.c_str());
    if(fclose(o) == 0) { std::error_code ec; fs::rename(t, f, ec); }
  }
};

// ---------- certificates ----------
// Checked when a node opens, so a bad setup stops there with a plain reason instead of a TLS handshake failing later
bool check_tls(const gxObjReplConfig &c, long &days_left, std::string &why)
{
  days_left = -1;
  if(c.ca.empty() || c.cert.empty() || c.key.empty()) { why = "ca, cert and key are required"; return false; }
  for(auto *f : {&c.ca, &c.cert, &c.key}) { FILE *t = fopen(f->c_str(), "rb"); if(!t) { why = "cannot read " + *f; return false; } fclose(t); }
#if !defined(_WIN32)
  struct stat sb;
  if(stat(c.key.c_str(), &sb) != 0 || (sb.st_mode & 077)) { why = "the key " + c.key + " is readable by group or others (chmod 600)"; return false; }
#endif
#if !defined(__USE_GX_SCHANNEL__)
  X509 *cert = 0, *ca = 0; EVP_PKEY *key = 0; X509_STORE *store = 0; X509_STORE_CTX *ctx = 0;
  bool ok = false;
  do {
    FILE *f = fopen(c.cert.c_str(), "r"); if(f) { cert = PEM_read_X509(f, 0, 0, 0); fclose(f); }
    if(!cert) { why = "no certificate in " + c.cert; break; }
    f = fopen(c.ca.c_str(), "r"); if(f) { ca = PEM_read_X509(f, 0, 0, 0); fclose(f); }
    if(!ca) { why = "no CA certificate in " + c.ca; break; }
    f = fopen(c.key.c_str(), "r"); if(f) { key = PEM_read_PrivateKey(f, 0, 0, 0); fclose(f); }
    if(!key) { why = "no private key in " + c.key + " (or it has a passphrase)"; break; }
    if(X509_cmp_current_time(X509_get0_notBefore(cert)) > 0) { why = c.cert + " is not valid yet"; break; }
    if(X509_cmp_current_time(X509_get0_notAfter(cert)) < 0) { why = c.cert + " has expired"; break; }
    int days = 0, secs = 0;
    if(ASN1_TIME_diff(&days, &secs, 0, X509_get0_notAfter(cert))) days_left = days;
    if(X509_check_private_key(cert, key) != 1) { why = "the key " + c.key + " does not match " + c.cert; break; }
    store = X509_STORE_new(); ctx = X509_STORE_CTX_new();
    if(!store || !ctx || X509_STORE_add_cert(store, ca) != 1 || X509_STORE_CTX_init(ctx, store, cert, 0) != 1) { why = "cannot check the certificate"; break; }
    if(X509_verify_cert(ctx) != 1) { why = c.cert + " is not signed by the CA " + c.ca + " (" + X509_verify_cert_error_string(X509_STORE_CTX_get_error(ctx)) + ")"; break; }
    ok = true;
  } while(false);
  if(ctx) X509_STORE_CTX_free(ctx);
  if(store) X509_STORE_free(store);
  if(key) EVP_PKEY_free(key);
  if(ca) X509_free(ca);
  if(cert) X509_free(cert);
  return ok;
#else
  return true;                              // Schannel: the handshake checks them (no days left in the status)
#endif
}

// 11/19/2026 (7.4.1): the primary's bound on one exchange (a frame out and the copy's answer in). The socket timeouts
// bound each send and each read, but not the whole: a copy stalled on a slow disk held a Pump() in a socket read for
// 10+ minutes. Past the deadline this thread shuts the socket down (it does not close it), which ends a blocked send
// or read at once on every platform; the exchange then fails and the connection is dropped. One thread per node,
// started at the first exchange. The socket is closed only after Disarm(), so the shutdown never hits a reused one
struct ExchangeGuard {
  std::mutex m;
  std::condition_variable c;
  std::thread t;
  bool quit = false, armed = false, fired = false;
  gxsSocket_t fd = gxsINVALID_SOCKET;
  std::chrono::steady_clock::time_point deadline;
  void Arm(gxsSocket_t f, int seconds) {
    std::lock_guard<std::mutex> g(m);
    if(!t.joinable()) t = std::thread([this] { run(); });
    fd = f; deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds); armed = true; fired = false;
    c.notify_one();
  }
  bool Disarm() {                                  // true: the deadline passed and the socket was shut down
    std::lock_guard<std::mutex> g(m);
    armed = false; bool f = fired; fired = false; fd = gxsINVALID_SOCKET;
    return f;
  }
  void run() {
    std::unique_lock<std::mutex> l(m);
    while(!quit) {
      if(!armed) { c.wait(l); continue; }
      if(std::chrono::steady_clock::now() >= deadline) {
	if(fd != gxsINVALID_SOCKET) shutdown(fd, 2);   // SHUT_RDWR / SD_BOTH
	fired = true; armed = false;
	continue;
      }
      c.wait_until(l, deadline);
    }
  }
  ~ExchangeGuard() {
    { std::lock_guard<std::mutex> g(m); quit = true; c.notify_one(); }
    if(t.joinable()) t.join();
  }
};

// 11/19/2026 (7.4.1): waits up to MS for socket A or the listener B to be readable. Returns 1 (A readable, whether
// or not B is), 2 (only B: a connection waits to be accepted), 0 (neither) or -1 (an error)
int wait_two(gxsSocket_t a, gxsSocket_t b, int ms)
{
#if defined(_WIN32)
  fd_set fds; FD_ZERO(&fds); FD_SET(a, &fds); FD_SET(b, &fds);
  timeval tv; tv.tv_sec = ms / 1000; tv.tv_usec = (ms % 1000) * 1000;
  int rv = select(0, &fds, 0, 0, &tv);
  if(rv < 0) return -1;
  if(rv == 0) return 0;
  return FD_ISSET(a, &fds) ? 1 : 2;
#else
  pollfd p[2];
  p[0].fd = a; p[0].events = POLLIN; p[0].revents = 0;
  p[1].fd = b; p[1].events = POLLIN; p[1].revents = 0;
  int rv;
  do { rv = poll(p, 2, ms); } while(rv < 0 && errno == EINTR);
  if(rv < 0) return -1;
  if(rv == 0) return 0;
  if(p[0].revents) return 1;                       // data, a close or an error on A: its read says which
  return (p[1].revents & POLLIN) ? 2 : -1;
#endif
}

struct Link {
  gxStream *st = 0; uint64_t copy_applied = 0, copy_digest = 0; bool copy_in_snapshot = false; bool refused = false;
  long snapshots = 0; time_t last_contact = 0;
  bool fresh = false;                 // connected, the hello not yet done
  long exchanges = 0;                 // frames answered on this link
  void drop() { if(st) { st->Close(); delete st; st = 0; } }
};

typedef std::tuple<uint8_t, std::string, uint64_t> Touch;

} // anonymous namespace

// ---------- the config ----------
bool gxObjReplConfig::Set(const std::string &k, const std::string &v, std::string *why)
{
  auto bad = [&](const std::string &w) { if(why) *why = w; return false; };
  auto num = [&](unsigned long long &out) { char *e = 0; out = strtoull(v.c_str(), &e, 10); return !v.empty() && e && !*e; };
  unsigned long long u = 0;
  if(k == "name") name = v;
  else if(k == "peer-name") peer_name = v;
  else if(k == "copy") {
    // 11/18/2026 (7.4.1): HOST[:PORT], port 2073 (gxsDATA_PORT) when none is given
    size_t c = v.rfind(':');
    std::string pv = c == std::string::npos ? std::string("2073") : v.substr(c + 1);
    char *e = 0; long port = strtol(pv.c_str(), &e, 10);
    if(v.empty() || c == 0 || pv.empty() || !e || *e || port <= 0 || port > 65535) return bad("copy is HOST[:PORT]");
    copy = v;
  }
  else if(k == "listen") {
    // 11/18/2026 (7.4.1): [ADDR:]PORT, or ADDR alone (the port stays 2073 unless set)
    if(v.empty()) return bad("listen is [ADDR:]PORT or ADDR");
    std::string pv = v; size_t c = v.rfind(':');
    if(c != std::string::npos) { listen_addr = v.substr(0, c); pv = v.substr(c + 1); }
    else if(!v.empty() && v.find_first_not_of("0123456789") != std::string::npos) { listen_addr = v; pv.clear(); }
    if(!pv.empty() || c != std::string::npos) {
      char *e = 0; long port = strtol(pv.c_str(), &e, 10);
      if(pv.empty() || !e || *e || port <= 0 || port > 65535) return bad("listen is [ADDR:]PORT or ADDR");
      listen_port = (int)port;
    }
    if(listen_addr.empty()) return bad("listen is [ADDR:]PORT or ADDR");
  }
  else if(k == "ca") ca = v;
  else if(k == "cert") cert = v;
  else if(k == "key") key = v;
  else if(k == "allow-cn") allow_cn = v;
  else if(k == "links") { std::vector<LinkDef> t; if(!parse_links(v, t)) return bad("bad links (DB:OFFSET:TARGET,...)"); links = v; }
  else if(k == "index") { std::vector<IxDef> t; if(!parse_ix(v, t)) return bad("bad index (NAME:DB:B|V:OFFSET:LEN,...)"); index = v; }
  else if(k == "max-lag") { if(!num(u)) return bad("max-lag is a number"); max_lag = u; }
  else if(k == "max-behind") { if(!num(u)) return bad("max-behind is a number"); max_behind = u; }
  else if(k == "min-cert-days") { if(!num(u)) return bad("min-cert-days is a number"); min_cert_days = (long)u; }
  else if(k == "trim") { if(v != "0" && v != "1") return bad("trim is 0 or 1"); trim = v == "1"; }
  else if(k == "idle-timeout") { if(!num(u) || u > 86400) return bad("idle-timeout is seconds (0 = none)"); idle_timeout = (int)u; }
  else if(k == "schema") { if(v.size() > 200 || v.find_first_of(";\r\n") != std::string::npos) return bad("schema is text of at most 200 characters, no ';'"); schema = v; }
  else if(k == "rejoin-max-share") { if(!num(u) || u < 1 || u > 100) return bad("rejoin-max-share is a percent (1 to 100)"); rejoin_max_share = (int)u; }
  else if(k == "lock-wait") { if(!num(u) || u > 3600) return bad("lock-wait is seconds (0 to 3600)"); lock_wait = (int)u; }
  else if(k == "ack-timeout") { if(!num(u) || u < 1 || u > 86400) return bad("ack-timeout is seconds (1 to 86400)"); ack_timeout = (int)u; }
  else return bad("unknown key " + k);
  return true;
}

bool gxObjReplConfig::Load(const std::string &fname, std::string *why)
{
  FILE *f = fopen(fname.c_str(), "r");
  if(!f) { if(why) *why = "cannot read the config " + fname; return false; }
  char line[4096]; int ln = 0;
  while(fgets(line, sizeof(line), f)) {
    ln++;
    std::string l(line);
    size_t h = l.find('#'); if(h != std::string::npos) l.resize(h);
    while(!l.empty() && isspace((unsigned char)l.back())) l.pop_back();
    size_t b = l.find_first_not_of(" \t"); if(b == std::string::npos) continue;
    l = l.substr(b);
    size_t sp = l.find_first_of(" \t");
    std::string k = l.substr(0, sp), v = sp == std::string::npos ? "" : l.substr(l.find_first_not_of(" \t", sp));
    std::string w;
    if(v.empty()) w = "no value for " + k;
    if(w.empty() && !Set(k, v, &w)) {}
    if(!w.empty()) { fclose(f); if(why) *why = fname + ":" + std::to_string(ln) + ": " + w; return false; }
  }
  fclose(f);
  return true;
}

// ---------- the node ----------
// the last error: its own small lock, so a reader can say why it failed without the node's lock (a copy in a full send)
struct ErrStr {
  mutable std::mutex m; std::string s;
  ErrStr &operator=(const std::string &v) { std::lock_guard<std::mutex> g(m); s = v; return *this; }
  operator std::string() const { std::lock_guard<std::mutex> g(m); return s; }
  bool empty() const { std::lock_guard<std::mutex> g(m); return s.empty(); }
  void clear() { std::lock_guard<std::mutex> g(m); s.clear(); }
};

struct gxObjRepl::Impl {
  gxObjReplConfig cfg;
  Node s;
  Link L;
  Mon mon;
  std::vector<LinkDef> links;
  std::vector<LinkDef> links_own;      // copy: the map this node was configured with (the hello must match it)
  std::vector<IxDef> ixdefs;
  ErrStr err;
  // 7.5 beta 3: the schema fingerprint: "index=...;links=...;schema=..." (each part in canonical order). The copy
  // compares the links part only when it has a link map of its own (otherwise it takes the primary's)
  std::string fingerprint(const std::vector<LinkDef> &lk) const {
    std::vector<std::string> v;
    for(auto &d : ixdefs) v.push_back(d.name + ":" + d.db + ":" + std::string(1, d.kind) + ":" + std::to_string(d.off) + ":" + std::to_string(d.len));
    std::sort(v.begin(), v.end());
    std::string ix;
    for(auto &x : v) ix += (ix.empty() ? "" : ",") + x;
    return "index=" + ix + ";links=" + links_canon(lk) + ";schema=" + cfg.schema;
  }
  // what differs between two fingerprints, in words ("" when they agree)
  static std::string fp_diff(const std::string &mine, const std::string &theirs) {
    auto parts = [](const std::string &f) {
      std::map<std::string, std::string> m; size_t i = 0;
      while(i <= f.size()) {
	size_t j = f.find(';', i); if(j == std::string::npos) j = f.size();
	std::string p = f.substr(i, j - i); size_t e = p.find('=');
	if(e != std::string::npos) m[p.substr(0, e)] = p.substr(e + 1);
	i = j + 1;
      }
      return m;
    };
    auto a = parts(mine), b = parts(theirs);
    std::string d;
    for(const char *k : {"index", "links", "schema"}) {
      std::string x = a.count(k) ? a[k] : "?", y = b.count(k) ? b[k] : "?";
      if(x != y) d += (d.empty() ? "" : "; ") + std::string(k == std::string("index") ? "index spec" : k == std::string("links") ? "link map" : "schema") +
		      " differs: [" + y + "] here, [" + x + "] on the other node";
    }
    return d;
  }
  bool copy_incr = false;              // primary: the copy said it waits for an incremental rejoin we can do
  std::string copy_fp;                 // primary: the copy's schema fingerprint (empty: an older copy)
  uint64_t last_rejoin_objects = 0; std::string last_rejoin = "none";
  // a copy in a full send (s.st.in_snapshot), as of the last time the node's lock was given up: reads fail at once
  // then, without waiting for the batch being applied (11/16/2026)
  std::atomic<int> snapflag{0};
  std::atomic<int> opening_n{0};
  std::set<std::string> failed_dbs;       // OpenDatabase(): prepared without success (refused until Close()/Open())
  void sync_snapflag() { snapflag = role == COPY && s.is_open && s.st.in_snapshot ? 1 : 0; }
  Role role = PRIMARY;
  long cert_days = -1;
  std::atomic<int> stop{0};
  // every public call takes it; Serve() holds it only while it handles a hello or a frame, never while it waits, so
  // reader threads (Get, FindKey, Nearest ...) run between frames (11/16/2026)
  ObjMutex mu;
  std::atomic<int> serving{0};       // Serve() is running (Close() stops it and waits)
  std::atomic<int> waiting{0};       // readers waiting for the lock: Serve() lets them in before it applies a frame
  // Pump() gives the lock up while a frame travels and the copy applies it (11/16/2026: before, it held it through
  // each round trip and gave it up only between frames when a reader waited; the sim day's readers still waited
  // seconds). While the lock is given up, nothing that pump1() relies on may change: Pump()/Drain() are serialized
  // (pumpmu), Close() and Open() wait for a running Pump() (pumping) and raise closing first (Open() before it takes
  // the lock), and the generation counter changes at every Open()/Close(). io() fails when any of that happened.
  // What the application may do meanwhile (commit, roll back) is safe: the outbox only grows (Pump() alone trims
  // it), a catch-up holds addresses of outbox entries only, and a full send keeps what it has not sent yet as it was
  // at its base (snap_keep())
  std::mutex pumpmu;
  ExchangeGuard guard;                // primary: the deadline of one exchange (cfg.ack_timeout)
  std::atomic<int> pumping{0}, closing{0};
  std::atomic<unsigned long> gen{0};
  std::vector<LinkDef> pending_links; // copy: the link map of a primary not yet accepted (F_EPOCH adopts it)
  // primary: the open transaction's ops, and the moves
  std::vector<Touch> touched;
  std::set<std::pair<std::string, uint64_t>> moved;
  std::vector<Touch> held;
  uint64_t next_id = 1;
  // Purge(): the (database, id) pairs live links name, worked out once per transaction until the next change
  std::set<std::pair<std::string, uint64_t>> linked;
  bool linked_valid = false;
  std::string last_alarm;
  time_t last_alarm_said = 0; unsigned long alarm_repeats = 0;   // the log line of a repeated alarm: once a minute

  void say(const std::string &m) {
    if(cfg.log) cfg.log(m); else { fprintf(stderr, "objrepl: %s\n", m.c_str()); fflush(stderr); }
  }
  // a reason to stop streaming: said and kept in ROOT/objrepl.alarm until someone clears it
  void alarm(const std::string &why) {
    time_t now = time(0);
    if(why == last_alarm) {                                    // the same reason again: not written again, and said
      alarm_repeats++;                                         // once a minute with a count (11/16/2026: an old
      if(now - last_alarm_said >= 60) {                        // primary's retries gave 3.5 log lines a second)
	say("ALARM (again, " + std::to_string(alarm_repeats) + " time(s) since it was last logged): " + why);
	alarm_repeats = 0; last_alarm_said = now;
      }
      return;
    }
    say("ALARM: " + why);
    last_alarm = why; last_alarm_said = now; alarm_repeats = 0;
    if(FILE *f = fopen(path(s.root, "objrepl.alarm").c_str(), "a")) {
      time_t t = time(0); char ts[32]; strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&t));
      fprintf(f, "%s %s\n", ts, why.c_str()); fclose(f);
    }
  }
  int fail(const std::string &w) { err = w; return -1; }

  // a commit whose journal sync runs without the node's lock (11/16/2026: the sync, once per commit, was made with the
  // lock held: tens of ms, seconds on a busy disk, and every reader waited). With the lock held once: Commit(0) puts
  // the batch in the journal (and makes it visible here), the journal file is synced by its own descriptor with the
  // lock given up, then the batch's pages are written to the files with it held again (write-back mode: the files
  // change only after their batch is durable, as in a write-through commit; with another thread's transaction open
  // by then they wait for the next commit or checkpoint). durable_seq: the commits made durable (Pump() sends no
  // other). Otherwise (held deeper, Windows) the plain commit. Returns false (s.err) when the commit failed
  uint64_t durable_seq = 0;
  // 11/16/2026 (review of the unlocked sync): commits whose journal sync is running without the lock. Pages reach the
  // files only when none is (WriteCommitted() writes every committed page, a later commit's too: its record must be
  // durable first, and it is once its own committer is back), a soft checkpoint waits for none, Close() and Begin()
  // wait for none. jcommit_gen_ok: the node was not closed or opened again while the last jcommit() was unlocked
  std::atomic<int> commits_inflight{0};
  bool jcommit_gen_ok = true;
  bool said_sharing = false;              // the sharing-violation fallback was logged (once a node)
  // with the lock held once: wait (lock given up) until no commit's sync is running
  void wait_commits() { while(commits_inflight && mu.Once()) { Unlocked u(mu); std::this_thread::sleep_for(std::chrono::microseconds(200)); } }
  bool jcommit() {
    jcommit_gen_ok = true;
    if(mu.Once()) {
#ifdef GXREPL_TEST_HOOKS
      auto t0 = std::chrono::steady_clock::now();
#endif
      if(s.j.Commit(0) != gxDBASE_NO_ERROR) { if(s.j.InTransaction()) s.j.Rollback(); return s.fail("a commit failed"); }
      unsigned long g = gen; std::string jn = path(s.root, "node.jnl");
      bool synced = false; int sr = 0;
      commits_inflight++;
      { Unlocked u(mu); sr = sync_path(jn, true); synced = sr == 1; }
      commits_inflight--;
#ifdef GXREPL_TEST_HOOKS
      ObjMutex::note("journal sync (unlocked)", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
#endif
      mu.at("commit: the pages to the files");
      if(g != gen || !s.is_open) { jcommit_gen_ok = false; return true; }   // closed meanwhile: the close checkpointed the batch
      // Windows: the journal could not be opened a second time (another program's handle denies write sharing): its own
      // handle syncs it, with the lock held, as before (a first sync, not a retry)
      if(sr == 2 && !said_sharing) { said_sharing = true; say("the journal could not be opened again for its sync (another program's handle denies write sharing): synced through its own handle, with the lock held"); }
      if(sr == 2) { if(s.j.InTransaction() || commits_inflight || s.j.Checkpoint() != gxDBASE_NO_ERROR) return s.fail("the journal sync failed"); return true; }
      // a failed sync fails the commit and the caller reopens the node. The close syncs the journal once more through
      // its own descriptor, which still reports the earlier error (Linux), so the journal is not emptied, and the
      // recovery at the open replays what is in it: the commit the application was told failed may still be there
      // and then reaches the copy (review: documented, not "dropped")
      if(!synced) return s.fail("the journal sync failed");
      if(!commits_inflight && !s.j.InTransaction() && s.j.WriteCommitted() != gxDBASE_NO_ERROR) return s.fail("a commit's pages cannot be written");
      return true;
    }
    return s.commit();
  }

  // the journal's bytes since its last checkpoint (about: the pages logged since)
  unsigned long ck_seen = ~0UL; unsigned long long ck_pages = 0;
  size_t journal_bytes() {
    if(s.j.checkpoints != ck_seen || s.j.pages_logged < ck_pages) { ck_seen = s.j.checkpoints; ck_pages = s.j.pages_logged; }
    return (size_t)(s.j.pages_logged - ck_pages) * (size_t)gxdPageSize;
  }
  // a checkpoint once the journal holds CKPT_SOFT bytes, with the files synced while the lock is given up (11/16/2026:
  // the checkpoint synced about 30 files with the lock held, seconds on a busy disk): CheckpointBegin() writes the
  // committed pages to the files, they are synced by name without the lock, and CheckpointEnd() empties the journal
  // if no commit came meanwhile (else the next try does it; the journal's own limit, CKPT_HARD, checkpoints inside a
  // commit if the soft one never gets its turn). Called with the lock held once, between transactions
  void checkpoint_soon() {
    if(!s.is_open || s.j.InTransaction() || commits_inflight || journal_bytes() < CKPT_SOFT || !mu.Once()) return;
    checkpoint_unlocked();
  }
  // the checkpoint with the files synced while the lock is given up: true if the journal was emptied
  bool checkpoint_unlocked() {
    if(!s.is_open || s.j.InTransaction() || commits_inflight || !mu.Once()) return false;
    std::vector<std::string> paths; unsigned long c0 = 0;
    if(s.j.CheckpointBegin(paths, c0) != gxDBASE_NO_ERROR) return false;
    unsigned long g = gen, o = s.opens;
    bool synced;
    { Unlocked u(mu); synced = sync_files(paths); }
    mu.at("checkpoint");
    if(!synced || g != gen || o != s.opens || !s.is_open) return false;
    return s.j.CheckpointEnd(c0) == gxDBASE_NO_ERROR;
  }


  std::vector<std::pair<uint64_t, FAU_t>> outbox_entries() {
    std::vector<std::pair<uint64_t, FAU_t>> v;
    for(FAU_t a = s.ob.FindFirstObject(); a != (FAU_t)0; a = s.ob.FindNextObject(a)) {
      unsigned char h[9];
      if(s.ob.Read(h, 9, a) != gxDBASE_NO_ERROR) continue;
      Rd r(h, 9); r.u8(); v.push_back({r.u64(), a});
    }
    std::sort(v.begin(), v.end());
    return v;
  }

  // ---------- primary ----------
  bool connect_to(gxStream &st) {
    // 11/18/2026 (7.4.1): HOST[:PORT], port 2073 (gxsDATA_PORT) when none is given
    size_t c = cfg.copy.rfind(':');
    std::string host = c == std::string::npos ? cfg.copy : cfg.copy.substr(0, c);
    int port = c == std::string::npos ? gxsDATA_PORT : atoi(cfg.copy.substr(c + 1).c_str());
    st.Connection().SetCAFile(cfg.ca);
    st.SetClientCert(cfg.cert, cfg.key);
    int ack = cfg.ack_timeout > 0 ? cfg.ack_timeout : 300;
    st.SetTimeOut(ack < 30 ? ack : 30);                       // the connect and handshake
    if(st.Connect(host, port) != 0) return false;
    // an ack may follow a long step on the copy (clearing for a full send, a 60 MB commit) (review 2). 7.4.1: the
    // whole exchange is bounded by ack-timeout too (io())
    st.SetTimeOut(ack);
    // a frame is a header write and a block write, answered by a small ack: without TCP_NODELAY Nagle holds the
    // block for the peer's delayed ack (about 40 ms per frame on Linux)
    st.Connection().Socket().SetNoDelay(true);
    return true;
  }
  bool hello() {
    // a connection idle past the idle limit may be gone (the copy drops it then): a fresh one, with the check
    mu.at("hello");
    if(L.st && !L.fresh && cfg.idle_timeout > 0 && time(0) - L.last_contact > cfg.idle_timeout / 2) L.drop();
    if(L.st && !L.fresh) return true;
    L.refused = false;                                         // each connect is judged afresh
    if(!L.st) { L.st = new gxStream; if(!connect_to(*L.st)) { L.drop(); return false; } }
    L.fresh = false;
    // the reconnect check, after every connect: both sides say who they are, their role, epoch, seq and the running
    // digest at that seq; any change of roles or histories stops the stream with an alarm
    Buf h; h.u32(STATE_MAGIC); h.u64(s.st.epoch); h.str(links_spec(links));
    h.str(cfg.name); h.u8(ROLE_PRIMARY); h.u64(s.st.next_seq - 1); h.u64(s.st.cur_digest);
    {                                                          // 7.5 beta 3 (older copies ignore these)
      uint64_t bseq = 0, bdig = 0;
      if(!epoch_base(s.root, s.st.epoch, bseq, bdig)) bseq = bdig = 0;
      h.u32(RJ_MAGIC); h.u64(bseq); h.u64(bdig); h.str(fingerprint(links));
    }
    std::vector<unsigned char> rep;
    if(!io(h.b, rep)) {
      if(io_net_failed && io_old_header) { alarm("the copy sends the gxStream block header with 4-byte words (DataReel before 7.4 beta 5 on Windows or a 32-bit build): refused"); L.refused = true; }
      else if(io_net_failed) say("the copy closed the connection without answering the hello (a DataReel before 7.4 beta 5 on Windows or a "
				 "32-bit build, which cannot read this gxStream header, or a copy that stopped)");   // not an alarm: a restarting copy looks the same
      return false;
    }
    Rd r(rep.data(), rep.size());
    uint8_t ok = r.u8(); uint64_t applied = r.u64(), cep = r.u64(); uint8_t snap = r.u8();
    std::string cname = r.str(); uint8_t crole = r.u8(); uint64_t cdig = r.u64(); std::string why = r.str();
    copy_incr = false; copy_fp.clear();
    if(r.ok && r.o + 4 <= r.n) {                               // 7.5 beta 3 fields (an older copy has none)
      Rd x(r.p + r.o, r.n - r.o);
      if(x.u32() == RJ_MAGIC) { uint8_t inc = x.u8(); std::string f = x.str(); if(x.ok) { copy_incr = inc != 0; copy_fp = f; } }
    }
    if(!r.ok || !ok) {
      alarm("the copy " + cname + " refused this primary (epoch " + std::to_string(s.st.epoch) + ", its epoch " +
	    std::to_string(cep) + "): " + (why.empty() ? "refused" : why));
      L.drop(); L.refused = true; return false;
    }
    if(crole != ROLE_COPY) { alarm("the peer " + cname + " says it is not a copy (role " + std::to_string(crole) + ")"); L.drop(); L.refused = true; return false; }
    if(!cfg.peer_name.empty() && cname != cfg.peer_name) { alarm("the peer is " + cname + ", not the expected copy " + cfg.peer_name); L.drop(); L.refused = true; return false; }
    L.copy_applied = applied; L.copy_in_snapshot = snap != 0; L.copy_digest = cdig; L.last_contact = time(0);
    if(!L.copy_in_snapshot && applied > 0) {                   // the copy's history must be this primary's
      if(applied > s.st.next_seq - 1) {
	alarm("the copy " + cname + " is at seq " + std::to_string(applied) + ", past this primary's last commit " +
	      std::to_string(s.st.next_seq - 1) + ": another primary wrote to it");
	L.drop(); L.refused = true; return false;
      }
      // the digest log keeps every seq this node committed or applied (since its last full send), so the copy's
      // history is always checked; one this node cannot check is refused too (11/16/2026, review: a stray primary
      // with a trimmed outbox overwrote a copy of another history with a full send). "objrepl rejoin" on the copy
      // makes it wait for a full send from this primary
      uint64_t mine = 0;
      bool known = applied == s.st.next_seq - 1 ? (mine = s.st.cur_digest, true) : s.digest_at(applied, mine);
      if(!known) {
	alarm("the copy " + cname + " is at seq " + std::to_string(applied) + ", which this primary cannot check (not in its digest log): " +
	      "refused; if the copy should follow this primary, run objrepl rejoin on it");
	L.drop(); L.refused = true; return false;
      }
      if(mine != cdig) {
	alarm("the copy " + cname + " has another history at seq " + std::to_string(applied) + " (digest " +
	      hex16(cdig) + ", this primary " + hex16(mine) + ")");
	L.drop(); L.refused = true; return false;
      }
    }
    // accepted: the copy saves this primary's epoch now, before anything else (review 2: a new primary with nothing
    // to send left the copy at the old epoch, and the old primary was then accepted)
    Buf e; e.u8(F_EPOCH); e.u64(s.st.epoch);
    uint64_t a = 0;
    if(!exchange(e.b, a)) return false;
    return true;
  }
  // a frame out and its answer in, with the node's lock given up meanwhile (11/16/2026: readers waited for every
  // round trip, and for the copy's apply of each frame): Pump() holds it once here. L.st belongs to the pumping
  // thread: Close() and Open() wait for a running Pump() before they touch it, so it stays valid without the lock.
  // hold: keep the lock (a full send whose kept records passed their memory cap). Returns false (L dropped) when the
  // exchange failed, or (err set) when Close() or Open() is waiting for this Pump()
  bool io(const std::vector<unsigned char> &f, std::vector<unsigned char> &in, bool hold = false) {
    gxStream *st = L.st;
    unsigned long g = gen;
    bool ok;
    io_net_failed = false; io_old_header = false;
    int ack = cfg.ack_timeout > 0 ? cfg.ack_timeout : 300;
    bool late = false;
    if(hold) {
      guard.Arm(st->Connection().Socket().GetSocket(), ack);
      ok = send_frame(*st, f) && recv_frame(*st, in);
      late = guard.Disarm();
    }
    else {
      Unlocked u(mu);
      guard.Arm(st->Connection().Socket().GetSocket(), ack);
      ok = send_frame(*st, f) && recv_frame(*st, in);
      late = guard.Disarm();
    }
    mu.at("pump");
    if(late) { ok = false; say("no answer from the copy in " + std::to_string(ack) + " s (ack-timeout): connection dropped"); }
    if(closing || g != gen || !s.is_open) { err = "the node was closed while pumping"; return false; }
    if(!ok) { io_net_failed = !late; io_old_header = !late && st->PeerOldHeader() != 0; L.drop(); return false; }
    return true;
  }
  bool io_net_failed = false, io_old_header = false;   // the last io(): the connection failed, the peer's header was old
  bool exchange(const std::vector<unsigned char> &f, uint64_t &acked, bool hold = false) {
    std::vector<unsigned char> ack;
    if(!io(f, ack, hold)) return false;
    if(ack.size() < 8) { L.drop(); return false; }
    L.last_contact = time(0); L.exchanges++;
    Rd r(ack.data(), ack.size()); acked = r.u64();
    return true;
  }
  // ---------- the full send ----------
  // Every object and tombstone of every database as of the last commit (the base), then the copy's applied seq = the
  // base. 11/16/2026 (reader latency): it no longer holds the node's lock for the whole send. It goes in batches, in
  // (database, id) order: the lock is held while a batch is read, never while it travels. Meanwhile the application
  // may commit: before a change to an object the full send has not reached yet (Put, Remove, Purge), the object as
  // it was at the base is kept (snap_keep(): its full-send record, links already as ids, or "absent" for an id the
  // base did not have), and the full send sends that instead. So what the copy gets is the base exactly; the commits
  // after it follow from the outbox (the outbox is trimmed only by Pump(), so they are there).
  struct Kept { bool absent = false; std::vector<unsigned char> rec; };
  struct FullSend {
    bool active = false;
    // sent so far: the databases before db, and in db (once in_db) the ids up to id (has_id); all: every object
    std::string db; bool in_db = false, has_id = false, all = false; uint64_t id = 0;
    std::map<std::pair<std::string, uint64_t>, Kept> kept;
    size_t bytes = 0;                                         // kept records' bytes
    bool hold = false;                                        // past FULLSEND_KEEP_MAX: the rest with the lock held
    bool bad = false; std::string why;                        // a record that could not be kept
  } fs;
  static const size_t FULLSEND_KEEP_MAX = 256u * 1024 * 1024;
  static const uint32_t DLOG_STEP = 8192;
  static const size_t TRIM_STEP = 256;                         // outbox entries (and digest log entries) trimmed a Pump()                     // digest log entries a frame (at most DLOG_BATCH)
  bool fs_sent(const std::string &db, uint64_t id) const {
    return fs.all || db < fs.db || (db == fs.db && fs.in_db && fs.has_id && id <= fs.id);
  }
  // one full-send record (database, id, version, flags, reason, seen seq, capacity, the bytes with links as ids)
  bool snap_entry(Db *d, FAU_t a, Buf &f) {
    __ULWORD__ n = d->db.ObjectLength(a);
    std::vector<unsigned char> rec; ObjHead h;
    if(!d->read(a, rec, h)) return false;
    if(!(h.flags & OF_TOMB) && !s.translate(d->name, rec.data() + sizeof(h), h.len, true)) return false;
    f.str(d->name); f.u64(h.id); f.u32(h.version); f.u32(h.flags); f.u32(h.reason); f.u64(h.seen_seq); f.u32((uint32_t)n);
    f.u32(h.len); f.raw(rec.data() + sizeof(h), h.len);
    return true;
  }
  // before the application changes (D, ID): kept as it is now if a full send is under way and has not sent it yet
  void snap_keep(Db *d, uint64_t id) {
    if(!fs.active || fs_sent(d->name, id)) return;
    auto key = std::make_pair(d->name, id);
    if(fs.kept.count(key)) return;                            // kept already: that is the base
    Kept k;
    auto w = d->where.find(id);
    if(w == d->where.end()) k.absent = true;
    else {
      Buf b;
      if(!snap_entry(d, w->second, b)) { fs.bad = true; fs.why = "cannot keep " + d->name + " " + std::to_string(id) + " for the full send"; }
      k.rec.swap(b.b);
    }
    fs.bytes += k.rec.size() + 64;
    if(fs.bytes > FULLSEND_KEEP_MAX) fs.hold = true;
    fs.kept[key] = std::move(k);
  }
  struct FsActive { FullSend &f; ~FsActive() { f = FullSend(); } };
  bool fs_waits = false;            // snapshot() did not start: a transaction stayed open
  bool snapshot() {
    mu.at("full send");
    fs_waits = false;
    // the base must be a commit: a transaction another thread has open is waited for (its changes are in the files
    // already and would be sent as if committed); still open after 2 s: the full send waits for the next Pump()
    for(int i = 0; s.j.InTransaction() || durable_seq + 1 < s.st.next_seq; i++) {
      if(i >= 2000 || !mu.Once()) { err = "a transaction stays open: the full send waits"; fs_waits = true; return false; }
      unsigned long g = gen;
      { Unlocked u(mu); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
      if(closing || g != gen || !s.is_open) { err = "the node was closed while pumping"; return false; }
    }
    uint64_t base = s.st.next_seq - 1, base_digest = s.st.cur_digest, acked = 0;
    uint64_t scount = 0, shash = DIGEST0;                      // what this full send carries, checked by the copy
    fs = FullSend(); fs.active = true;
    FsActive fa{fs};                                           // ended (and the kept records freed) on every return
    Buf b0; b0.u8(F_SNAP_BEGIN); b0.u64(base); b0.u64(s.st.epoch);
    if(!exchange(b0.b, acked, fs.hold)) return false;
    Buf f;
    for(;;) {
      // one batch, read with the lock held: from where the last one stopped (the maps may have changed since; the
      // ids changed since the base come from fs.kept)
      f.b.clear(); f.u8(F_SNAP_BATCH); size_t cnt_at = f.b.size(); f.u32(0); uint32_t nobj = 0;
      bool done = false;
      while(f.b.size() < SNAP_BATCH_BYTES) {
	if(fs.bad) { err = fs.why; return false; }
	auto di = fs.in_db ? s.dbs.find(fs.db) : s.dbs.lower_bound(fs.db);
	if(di == s.dbs.end()) { done = true; break; }
	if(!fs.in_db) { fs.db = di->first; fs.in_db = true; fs.has_id = false; fs.id = 0; }
	Db *d = di->second.get();
	// the next id: live here (unchanged since the base unless kept) or kept
	auto w = fs.has_id ? d->where.upper_bound(fs.id) : d->where.begin();
	auto k = fs.has_id ? fs.kept.upper_bound(std::make_pair(fs.db, fs.id)) : fs.kept.lower_bound(std::make_pair(fs.db, (uint64_t)0));
	bool has_w = w != d->where.end(), has_k = k != fs.kept.end() && k->first.first == fs.db;
	if(!has_w && !has_k) {                                 // this database is sent: the next one
	  auto nx = s.dbs.upper_bound(fs.db);
	  if(nx == s.dbs.end()) { done = true; break; }
	  fs.db = nx->first; fs.in_db = false; fs.has_id = false; fs.id = 0;
	  continue;
	}
	uint64_t id = has_w && (!has_k || w->first <= k->first.second) ? w->first : k->first.second;
	size_t at0 = f.b.size();
	auto kk = fs.kept.find(std::make_pair(fs.db, id));
	if(kk != fs.kept.end()) {
	  if(!kk->second.absent) f.raw(kk->second.rec.data(), kk->second.rec.size());
	  fs.bytes -= std::min(fs.bytes, kk->second.rec.size() + 64);
	  fs.kept.erase(kk);
	} else if(!snap_entry(d, w->second, f)) { err = "cannot read " + fs.db + " " + std::to_string(id) + " for the full send"; return false; }
	fs.id = id; fs.has_id = true;
	if(f.b.size() > at0) { shash = fnv(f.b.data() + at0, f.b.size() - at0, shash); scount++; nobj++; }
      }
      if(nobj) {
	for(int i = 0; i < 4; i++) f.b[cnt_at + i] = (unsigned char)(nobj >> (8 * i));
	if(!exchange(f.b, acked, fs.hold)) return false;
      }
      if(done) { fs.all = true; break; }
    }
    // the digest log up to the base, so the copy (if it ever takes over) can check other copies' histories (review 2);
    // read in batches too (entries up to the base do not change: only Pump() trims the log)
    {
      uint64_t next = 0;
      for(;;) {
	Buf g; g.u8(F_SNAP_DLOG); size_t at = g.b.size(); g.u32(0); uint32_t n = 0;
	DKey k(next), c;
	for(int ok = s.dlog.PartialFind(k, c); ok && k.k.seq <= base; ok = s.dlog.FindNext(k, c)) {
	  g.u64(k.k.seq); g.u64(k.k.digest); n++; next = k.k.seq + 1;
	  shash = fnv(g.b.data() + g.b.size() - 16, 16, shash); scount++;
	  if(n == DLOG_STEP) break;
	}
	if(!n) break;
	for(int i = 0; i < 4; i++) g.b[at + i] = (unsigned char)(n >> (8 * i));
	if(!exchange(g.b, acked, fs.hold)) return false;
	if(n < DLOG_STEP) break;
      }
    }
    Buf e; e.u8(F_SNAP_END); e.u64(base); e.u64(base_digest); e.u64(scount); e.u64(shash);
    if(!exchange(e.b, acked, fs.hold) || acked != base) return false;
    L.copy_applied = base; L.copy_in_snapshot = false; L.snapshots++;
    return true;
  }
  // ---------- 7.5 beta 3: the incremental rejoin ----------
  // A former primary waiting for one (the copy said so at the hello, and its history matches this node's at the seq
  // this node took over at): it lists what its own commits changed past that base (F_RJ_SET), this node adds what it
  // changed since (its change list), and sends each of those objects as it is now (or "absent"), then its digest log
  // past the base and the end with the count and hash of what was sent. The node's lock is held throughout (the set
  // is small by design: past rejoin-max-share of the objects a full send goes instead). Returns 1 done, 0 failed
  // (the connection dropped, or a transaction stays open: fs_waits), 2 a full send instead
  int repair() {
    mu.at("incremental rejoin");
    fs_waits = false;
    for(int i = 0; s.j.InTransaction() || durable_seq + 1 < s.st.next_seq; i++) {
      if(i >= 2000 || !mu.Once()) { err = "a transaction stays open: the rejoin waits"; fs_waits = true; return 0; }
      unsigned long g = gen;
      { Unlocked u(mu); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
      if(closing || g != gen || !s.is_open) { err = "the node was closed while pumping"; return 0; }
    }
    uint64_t base = 0, bdig = 0, d0 = 0;
    if(!epoch_base(s.root, s.st.epoch, base, bdig) || !base) { say("primary: no epoch base for epoch " + std::to_string(s.st.epoch) + ": a full send instead of an incremental rejoin"); return 2; }
    if(!s.digest_at(base, d0) || d0 != bdig) { say("primary: seq " + std::to_string(base) + " is no longer in the digest log: a full send instead of an incremental rejoin"); return 2; }
    uint64_t upto = s.st.next_seq - 1, updig = s.st.cur_digest, acked = 0;
    Node::IdSet u;
    if(!Node::list_since(s.chg, base, u)) { err = "cannot read the change list"; return 0; }
    size_t own = u.size();
    Buf q; q.u8(F_RJ_SET); q.u64(base); q.u64(upto);
    std::vector<unsigned char> in;
    if(!io(q.b, in, true)) return 0;
    L.last_contact = time(0); L.exchanges++;
    Rd r(in.data(), in.size()); uint8_t ok = r.u8(); uint32_t n = r.u32();
    if(!r.ok) { L.drop(); return 0; }
    if(!ok) { say("primary: the copy changed too many objects to list: a full send instead of an incremental rejoin"); return 2; }
    for(uint32_t i = 0; i < n && r.ok; i++) { std::string d = r.str(); uint64_t id = r.u64(); if(r.ok) u.insert({d, id}); }
    if(!r.ok) { L.drop(); return 0; }
    size_t total = 0; for(auto &d : s.dbs) total += d.second->where.size();
    if(total && (unsigned long long)u.size() * 100 > (unsigned long long)total * (unsigned)cfg.rejoin_max_share) {
      say("primary: an incremental rejoin would send " + std::to_string(u.size()) + " of " + std::to_string(total) + " objects (over rejoin-max-share " +
	  std::to_string(cfg.rejoin_max_share) + "%): a full send instead");
      return 2;
    }
    uint64_t scount = 0, shash = DIGEST0;
    auto it = u.begin();
    while(it != u.end()) {
      Buf f; f.u8(F_RJ_BATCH); size_t cnt_at = f.b.size(); f.u32(0); uint32_t nobj = 0;
      for(; it != u.end() && f.b.size() < SNAP_BATCH_BYTES; ++it) {
	size_t at0 = f.b.size();
	auto di = s.dbs.find(it->first);
	Db *d = di == s.dbs.end() ? 0 : di->second.get();
	auto w = d ? d->where.find(it->second) : std::map<uint64_t, FAU_t>::iterator();
	if(d && w != d->where.end()) { f.u8(1); if(!snap_entry(d, w->second, f)) { err = "cannot read " + it->first + " " + std::to_string(it->second) + " for the rejoin"; return 0; } }
	else { f.u8(2); f.str(it->first); f.u64(it->second); }
	shash = fnv(f.b.data() + at0, f.b.size() - at0, shash); scount++; nobj++;
      }
      for(int i = 0; i < 4; i++) f.b[cnt_at + i] = (unsigned char)(nobj >> (8 * i));
      if(!exchange(f.b, acked, true)) return 0;
    }
    for(uint64_t next = base + 1;;) {                          // the digest log past the base
      Buf g; g.u8(F_SNAP_DLOG); size_t at = g.b.size(); g.u32(0); uint32_t k = 0;
      DKey dk(next), c;
      for(int ok2 = s.dlog.PartialFind(dk, c); ok2 && dk.k.seq <= upto; ok2 = s.dlog.FindNext(dk, c)) {
	if(dk.k.seq < next) continue;
	g.u64(dk.k.seq); g.u64(dk.k.digest); k++; next = dk.k.seq + 1;
	shash = fnv(g.b.data() + g.b.size() - 16, 16, shash); scount++;
	if(k == DLOG_STEP) break;
      }
      if(!k) break;
      for(int i = 0; i < 4; i++) g.b[at + i] = (unsigned char)(k >> (8 * i));
      if(!exchange(g.b, acked, true)) return 0;
      if(k < DLOG_STEP) break;
    }
    Buf e; e.u8(F_RJ_END); e.u64(upto); e.u64(updig); e.u64(scount); e.u64(shash);
    if(!exchange(e.b, acked, true) || acked != upto) return 0;
    L.copy_applied = upto; L.copy_in_snapshot = false; copy_incr = false;
    last_rejoin_objects = u.size(); last_rejoin = "incremental";
    say("primary: incremental rejoin to seq " + std::to_string(upto) + " from base " + std::to_string(base) + ": " + std::to_string(u.size()) +
	" object(s) sent (" + std::to_string(own) + " changed here, " + std::to_string(n) + " on the rejoining node)");
    return 1;
  }
  void tombstone_count(unsigned long long &t) { t = 0; for(auto &d : s.dbs) t += d.second->tombs.size(); }
  // the primary's status: its seq, the copy's applied seq as last acknowledged, the lag and how long it has lasted
  void primary_status(bool force, const std::string &state) {
    uint64_t last = s.st.next_seq - 1, lag = last > L.copy_applied ? last - L.copy_applied : 0;
    time_t now = time(0);
    if(!lag) mon.behind_since = 0; else if(!mon.behind_since) mon.behind_since = now;
    if(!force && now == mon.last_write) return;                // at most once a second (the outbox scan is not free)
    mu.at("primary status");
    size_t n = 0; unsigned long long bytes = 0, oldest = 0, tombs = 0;
    for(auto &e : outbox_entries()) {
      __ULWORD__ len = s.ob.ObjectLength(e.second);
      n++; bytes += len;
      if(!oldest && e.first > L.copy_applied && len >= 16) {   // the oldest commit the copy does not have yet
	uint64_t t = 0; if(s.ob.Read(&t, 8, e.second + (FAU_t)(len - 16)) == gxDBASE_NO_ERROR && t && (time_t)t <= now) oldest = now - (time_t)t;
      }
    }
    tombstone_count(tombs);
    mon.set("oldest_unacked_secs", oldest); mon.set("last_contact", (unsigned long long)L.last_contact); mon.set("tombstones", tombs);
    mon.set("role", "primary"); mon.set("name", cfg.name); mon.set("epoch", (unsigned long long)s.st.epoch);
    mon.set("state", state); mon.set("connected", L.st ? 1ULL : 0ULL);
    mon.set("last_seq", (unsigned long long)last); mon.set("copy_applied", (unsigned long long)L.copy_applied);
    // how long the copy has been behind: since this node first saw the lag, or the age of the oldest commit the copy
    // does not have, whichever is longer (11/16/2026: on Windows a connect to a closed port takes ~2 s, so the first
    // status with the lag came late and said "for 0s" with commits seconds old)
    unsigned long long behind = mon.behind_since ? (unsigned long long)(now - mon.behind_since) : 0;
    if(lag && oldest > behind) behind = oldest;
    mon.set("lag_seq", (unsigned long long)lag); mon.set("behind_secs", behind);
    mon.set("outbox_entries", (unsigned long long)n); mon.set("outbox_bytes", bytes);
    mon.set("snapshots", (unsigned long long)L.snapshots); mon.set("cert_days_left", std::to_string(cert_days));
    mon.set("last_rejoin", last_rejoin); mon.set("last_rejoin_objects", (unsigned long long)last_rejoin_objects);
    mon.write(force);
  }
  bool pump1() {
    if(!hello()) return false;
    auto v = outbox_entries();
    bool need_snap = L.copy_in_snapshot || (L.copy_applied + 1 < s.st.next_seq &&
					    (v.empty() || v.front().first > L.copy_applied + 1));
    if(need_snap) {
      int rj = L.copy_in_snapshot && copy_incr ? repair() : 2;   // 7.5 beta 3: an incremental rejoin when it can
      if(rj == 0) return fs_waits;
      if(rj == 2) {
	if(!snapshot()) return fs_waits;                       // a transaction stayed open: the next Pump() sends it
	last_rejoin = "full send"; last_rejoin_objects = 0;
      }
      v = outbox_entries();                                    // the commits made while it was sent follow
#ifdef GXREPL_TEST_HOOKS
      if(getenv("OBJREPL_TEST_ONLY_FULL_SEND")) return true;   // test only: the copy stays at the full send's base
#endif
    }
    mu.at("catch-up");
    for(auto &e : v) {
      if(e.first <= L.copy_applied) continue;
      if(e.first > durable_seq) break;                         // its journal sync is still running (jcommit())
      __ULWORD__ n = s.ob.ObjectLength(e.second);
      std::vector<unsigned char> b(n);
      if(n < 16 || s.ob.Read(b.data(), n, e.second) != gxDBASE_NO_ERROR) return false;
      b.resize(n - 16);                                        // the frame without the time and digest trailer
      uint64_t acked = 0;
      if(!exchange(b, acked)) return false;
      L.copy_applied = acked;
    }
    // the copy's acknowledgement is recorded (acked_seq and its digest) with or without trimming (review 2: with trim
    // off the rejoin count was wrong); trimming drops the outbox entries it has, and the digest log keeps a window
    // (in steps of TRIM_STEP entries a Pump(): one transaction for a whole backlog held the lock for 200 ms)
    bool more = cfg.trim && !v.empty() && v.front().first <= L.copy_applied;
    if((L.copy_applied > s.st.acked_seq || more) && !s.j.InTransaction()) {
      mu.at("trim");
      wait_commits();                                          // no transaction of its own while a commit syncs
      if(closing || !s.is_open) { err = "the node was closed while pumping"; return false; }
      if(s.j.InTransaction()) return true;                     // the application's Begin() came first: trim next time
      if(!s.begin()) { err = s.err; return false; }
      uint64_t d = 0;
      if(L.copy_applied == s.st.next_seq - 1) s.st.acked_digest = s.st.cur_digest;
      else if(s.digest_at(L.copy_applied, d)) s.st.acked_digest = d;
      bool dok = true;
      size_t del = 0;
      if(cfg.trim) for(auto &e : v) if(e.first <= L.copy_applied && del++ < TRIM_STEP && !s.ob.Delete(e.second)) dok = false;
      if(!dok) { s.j.Rollback(); err = "cannot trim the outbox"; reopen(); return false; }
      if(s.st.next_seq > DLOG_KEEP && !s.trim_digests(std::min(s.st.next_seq - DLOG_KEEP, L.copy_applied), TRIM_STEP)) { s.j.Rollback(); err = "cannot trim the digest log"; reopen(); return false; }
      // 7.5 beta 3: the change list keeps the same window as the digest log, so an incremental rejoin whose base is
      // still in the digest log has every change since it
      if(s.st.next_seq > DLOG_KEEP && !Node::list_trim(s.chg, std::min(s.st.next_seq - DLOG_KEEP, L.copy_applied), TRIM_STEP)) { s.j.Rollback(); err = "cannot trim the change list"; reopen(); return false; }
      uint64_t keep_seq = s.st.acked_seq, keep_dig = s.st.acked_digest;
      s.st.acked_seq = L.copy_applied;
      s.put_state();
      if(!jcommit()) { s.st.acked_seq = keep_seq; s.st.acked_digest = keep_dig; err = s.err; reopen(); return false; }
      checkpoint_soon();
    }
    return true;
  }
  // one commit's record from the objects' final bytes (links as ids), its outbox entry and its state, in the open
  // transaction, then Commit()
  bool record_commit() {
    Buf f; uint64_t seq = s.st.next_seq;
    f.u8(F_COMMIT); f.u64(seq); f.u64(s.st.epoch);
    Buf ops; uint32_t nops = 0;
    for(auto &t : touched) {
      uint8_t op = std::get<0>(t); const std::string &dn = std::get<1>(t); uint64_t id = std::get<2>(t);
      Db *d = s.get(dn);
      if(!d) { err = s.err; return false; }
      ops.u8(op); ops.str(dn); ops.u64(id); nops++;
      if(op == OP_REMOVE || op == OP_PURGE) continue;
      auto w = d->where.find(id);
      std::vector<unsigned char> rec; ObjHead h; memset(&h, 0, sizeof(h));
      if(w == d->where.end() || !d->read(w->second, rec, h) || (h.flags & OF_TOMB)) {
	ops.u32(0); ops.u8(0); ops.u32(0);                     // changed, then removed in this commit: the remove follows
	continue;
      }
      if(!s.translate(dn, rec.data() + sizeof(h), h.len, true)) { err = s.err.empty() ? "a link names no object" : s.err; return false; }
      ops.u32((uint32_t)d->db.ObjectLength(w->second)); ops.u8(moved.count({dn, id}) ? 1 : 0);
      ops.u32(h.len); ops.raw(rec.data() + sizeof(h), h.len);
    }
    f.u32(nops); f.raw(ops.b.data(), ops.b.size());
    if(f.b.size() > MAX_FRAME) {
      err = "the commit is " + std::to_string(f.b.size()) + " bytes, over the " + std::to_string(MAX_FRAME) + " a commit may be: split it";
      return false;
    }
    for(auto &t : touched) s.st.max_id = std::max(s.st.max_id, std::get<2>(t));
    s.st.cur_digest = fnv(f.b.data(), f.b.size(), s.st.cur_digest);
    if(!s.log_digest(seq, s.st.cur_digest)) { err = "cannot write the digest log"; return false; }
    f.u64((uint64_t)time(0));                                  // the outbox entry: the frame, its commit time and
    f.u64(s.st.cur_digest);                                    // the running digest after it (the last 8 bytes)
    FAU_t oa = s.ob.Alloc((__ULWORD__)f.b.size());
    if(!oa || s.ob.Write(f.b.data(), (__ULWORD__)f.b.size(), oa) != gxDBASE_NO_ERROR) { err = "the outbox write failed"; return false; }
    {                                                          // 7.5 beta 3: the change list
      Node::IdSet ids;
      for(auto &t : touched) ids.insert({std::get<1>(t), std::get<2>(t)});
      if(!Node::list_add(s.chg, seq, ids)) { err = "the change list write failed"; return false; }
    }
    s.st.next_seq = seq + 1;
    s.put_state();
    mu.at("Commit journal");
    if(!jcommit()) { err = "the commit failed"; return false; }
    if(jcommit_gen_ok) durable_seq = std::max(durable_seq, seq);   // not the old node's seq on a node opened since
    return true;
  }
  // every (database, id) a link field of a live object names (a scan; no reverse index)
  void build_linked() {
    linked.clear();
    for(auto &l : links) {
      auto src = s.dbs.find(l.db), tgt = s.dbs.find(l.target);
      if(src == s.dbs.end() || tgt == s.dbs.end()) continue;
      for(auto &w : src->second->where) {
	if(src->second->tombs.count(w.first)) continue;
	std::vector<unsigned char> rec; ObjHead h;
	if(!src->second->read(w.second, rec, h) || h.len < (size_t)l.off + 8) continue;
	uint64_t v; memcpy(&v, rec.data() + sizeof(h) + l.off, 8);
	auto it = v ? tgt->second->who.find((FAU_t)v) : tgt->second->who.end();
	if(it != tgt->second->who.end()) linked.insert({l.target, it->second});
      }
    }
    linked_valid = true;
  }
  void touch(uint8_t op, const std::string &db, uint64_t id) {
    // a second add or update of an object in the same commit adds nothing: the record holds its final bytes
    if(op == OP_ADD || op == OP_UPDATE)
      for(auto it = touched.rbegin(); it != touched.rend(); ++it)
	if(std::get<1>(*it) == db && std::get<2>(*it) == id) {
	  uint8_t last = std::get<0>(*it);
	  if(last == OP_ADD || last == OP_UPDATE) return;
	  break;
	}
    touched.push_back(Touch(op, db, id));
  }
  // reopened after a rollback: the maps, versions and vector index memory are as the files say
  bool reopen() {
    std::string root = s.root; bool prim = s.primary;
    s.close(true);                                             // the lock is kept: nothing changes the node meanwhile
    if(!s.open(root, prim, true)) { err = s.err; s.close(); return false; }
    uint32_t want = role == PRIMARY ? ROLE_PRIMARY : ROLE_COPY;
    if(s.st.role != want) { err = root + ": the role changed underneath this process"; s.close(); return false; }
    durable_seq = s.st.next_seq - 1;                           // the close checkpointed (synced) the journal
    sync_snapflag();
    return true;
  }

  // ---------- copy ----------
  // the databases named in a commit's ops or a snapshot batch, opened before the transaction begins
  bool open_named(const unsigned char *p, size_t n, bool snapshot_batch) {
    Rd r(p, n);
    uint32_t k = r.u32();
    for(uint32_t i = 0; i < k && r.ok; i++) {
      uint8_t op = snapshot_batch ? OP_ADD : r.u8();
      std::string name = r.str(); r.u64();
      if(snapshot_batch) { r.u32(); r.u32(); r.u32(); r.u64(); }   // version, flags, reason, seen_seq
      if(op == OP_ADD || op == OP_UPDATE) { r.u32(); if(!snapshot_batch) r.u8(); uint32_t len = r.u32(); r.raw(len); }
      if(!r.ok || !good_name(name) || !s.get(name)) return false;
    }
    for(auto &l : links) if(!s.get(l.db) || !s.get(l.target)) return false;   // link targets too (placeholders)
    return r.ok;
  }
  int serve();
};

gxObjRepl::gxObjRepl() : p(new Impl) { }
gxObjRepl::~gxObjRepl() { Close(); delete p; }
std::string gxObjRepl::Error() const { return p->err; }   // its own lock (ErrStr)
int gxObjRepl::IsOpen() const { std::lock_guard<ObjMutex> g_(p->mu); return p->s.is_open; }

int gxObjRepl::Open(const std::string &root, const gxObjReplConfig &cfg, Role role)
{
  // closed first, before the lock is taken: Close() waits for a Pump() running in another thread, which takes the
  // lock again after each frame it sends (11/16/2026)
  Close();
  std::lock_guard<ObjMutex> g_(p->mu);
  p->mu.at("Open");
  // another thread's Open() got here first (review: a second Open() reset the first one's state, then failed on the
  // lock file): not opened twice
  if(p->s.is_open) return p->fail("another Open() of this object is running: Close() it first");
  p->cfg = cfg; p->role = role; p->err.clear(); p->stop = 0;
  p->L = Link(); p->mon = Mon(); p->failed_dbs.clear(); p->touched.clear(); p->moved.clear(); p->held.clear();
  p->next_id = 1; p->linked_valid = false; p->last_alarm.clear(); p->gen++;
  if(!parse_links(cfg.links, p->links)) return p->fail("bad links (DB:OFFSET:TARGET,...)");
  p->links_own = p->links;
  if(!parse_ix(cfg.index, p->ixdefs)) return p->fail("bad index (NAME:DB:B|V:OFFSET:LEN,...)");
  for(auto &x : p->ixdefs) for(auto &l : p->links)             // an index field may not hold a link (an address)
    if(x.db == l.db && l.off < x.off + (x.kind == 'B' ? x.len : 4 * x.len) && x.off < l.off + 8)
      return p->fail("index " + x.name + " overlaps the link field " + l.db + ":" + std::to_string(l.off));
  if(role == PRIMARY && cfg.copy.empty()) return p->fail("copy (HOST[:PORT]) is required");
  if(role == COPY && cfg.listen_port <= 0) return p->fail("listen needs a port (default 2073)");
  if(role == COPY && cfg.allow_cn.empty()) return p->fail("allow-cn is required");
  std::string why;
  if(!check_tls(p->cfg, p->cert_days, why)) return p->fail(why);
  p->s.links = &p->links; p->s.ixdefs = &p->ixdefs;
  p->s.log = [this](const std::string &m) { p->say(m); };
  p->s.lock_wait_ms = cfg.lock_wait * 1000;
  if(!p->s.open(root, role == PRIMARY)) { std::string e = p->s.err; p->s.close(); return p->fail(e); }
  p->mon.root = root;
  p->durable_seq = p->s.st.next_seq - 1;
  p->sync_snapflag();
  if(role == PRIMARY && p->s.st.role == ROLE_COPY) { p->alarm(root + " is a copy: run takeover before it writes"); p->s.close(); p->err = "a copy (takeover first)"; return 4; }
  if(role == COPY && p->s.st.role == ROLE_PRIMARY) { p->alarm(root + " is a primary: it will not run as a copy (two primaries)"); p->s.close(); p->err = "a primary (two primaries)"; return 4; }
  uint32_t want = role == PRIMARY ? ROLE_PRIMARY : ROLE_COPY;
  if(p->s.st.role != want) {
    if(!p->s.begin()) { std::string e = p->s.err; p->s.close(); return p->fail(e); }
    p->s.st.role = want; p->s.put_state();
    if(!p->s.commit()) { std::string e = p->s.err; p->s.close(); return p->fail(e); }
  }
  if(role == PRIMARY) {
    for(auto &l : p->links) if(!p->s.get(l.db) || !p->s.get(l.target)) { std::string e = p->s.err; p->s.close(); return p->fail(e); }
    // never an id used before (even one purged since), nor one of an older epoch; and never into the next epoch's
    // range, where an application's own ids may lie (review 3)
    uint64_t lim = (p->s.st.epoch + 1) << ID_EPOCH_SHIFT;
    p->next_id = std::max(p->next_id, (p->s.st.epoch << ID_EPOCH_SHIFT) + 1);
    if(p->s.st.max_id + 1 < lim) p->next_id = std::max(p->next_id, p->s.st.max_id + 1);
    for(auto &d : p->s.dbs) for(auto &w : d.second->where) if(w.first + 1 < lim) p->next_id = std::max(p->next_id, w.first + 1);
  }
  return 0;
}

void gxObjRepl::Close()
{
  // a Serve() running in another thread is stopped first and waited for (review 3: it went on serving a closed,
  // unlocked node); it leaves within half a second
  if(p->serving) { p->stop = 1; while(p->serving) std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
  // a Pump() running in another thread is told to stop and waited for (review 4)
  p->closing = 1;
  while(p->pumping || p->opening_n || p->commits_inflight) std::this_thread::sleep_for(std::chrono::milliseconds(5));   // and OpenDatabase(), a commit's sync
  struct Unclose { std::atomic<int> &f; ~Unclose() { f = 0; } } uc{p->closing};
  std::lock_guard<ObjMutex> g_(p->mu);
  p->mu.at("Close");
  // a thread that gave the lock up for a while (a commit's sync, a checkpoint's, an OpenDatabase() prepare or wait)
  // takes it back before the node is closed (review: such a thread took a freed lock after ~gxObjRepl)
  while(p->mu.windows > 0 && p->mu.Once()) { p->mu.unlock(); std::this_thread::sleep_for(std::chrono::milliseconds(2)); p->mu.lock(); }
  p->gen++;
  if(!p->s.is_open) return;
  // 11/17/2026: "stopping" (and since when) while the node closes: the journal checkpoint syncs every file, which
  // can take minutes on a saturated disk; "stopped" with the close's time once it is done (objrepl check warns
  // on a long "stopping")
  time_t c0 = time(0);
  if(!p->mon.v.count("stopping_since")) p->mon.set("stopping_since", (unsigned long long)c0);
  std::string final_state = "stopped";
  if(p->role == PRIMARY) {
    bool done = p->L.copy_applied + 1 >= p->s.st.next_seq;
    final_state = done ? "stopped, drained" : "stopped, NOT drained";
    p->L.drop();
    p->primary_status(true, "stopping");
  }
  else { p->mon.set("state", "stopping"); p->mon.write(true); }
  p->L.drop();
  p->s.close();
  p->sync_snapflag();
  p->mon.v.erase("stopping_since");
  p->mon.set("close_secs", (unsigned long long)(time(0) - c0));
  p->mon.set("state", final_state);
  p->mon.write(true);
}

int gxObjRepl::OpenDatabase(const std::string &db)
{
  std::unique_lock<ObjMutex> g_(p->mu);
  p->mu.at("OpenDatabase");
  if(!p->s.is_open || p->role != PRIMARY) return p->fail("not an open primary (a copy opens databases as they arrive)");
  if(p->closing) return p->fail("the node is closing");
  // 11/16/2026 (lock holds): the files are opened and the indexes built without the lock (see Node::prepare_db());
  // only the attach to the journal is done with it. One caller prepares a database at a time; Close() waits for it
  for(;;) {
    if(p->s.dbs.count(db)) return 0;
    if(!p->s.opening.count(db)) break;
    { Unlocked u(p->mu); std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
    if(p->closing || !p->s.is_open) return p->fail("the node was closed while " + db + " was opened");
  }
  if(!good_name(db)) return p->fail("a bad database name: " + db);
  if(p->s.j.InTransaction()) return p->fail("the database " + db + " is not open (open it before Begin())");
  for(auto &d : p->s.dead_dbs) if(d->name == db) return p->fail("the database " + db + " failed to open earlier: Close() and Open() again");
  if(p->failed_dbs.count(db)) return p->fail("the database " + db + " failed to open earlier: Close() and Open() again");
  std::vector<IxDef> defs = p->ixdefs; std::string root = p->s.root;
  unsigned long g0 = p->gen;
  p->s.opening.insert(db); p->opening_n++;
  std::unique_ptr<Db> pre; std::string e; bool ok;
  {
    Unlocked u(p->mu);
    gxObjRepl::Impl *q = p;
    ok = Node::prepare_db(root, db, defs, pre, e, [q]() { return q->closing != 0; }, [q](const std::string &m) { q->say(m); });
  }
  p->mu.at("OpenDatabase attach");
  p->s.opening.erase(db); p->opening_n--;
  if(p->closing || g0 != p->gen || !p->s.is_open) { if(pre) pre->close(); return p->fail("the node was closed while " + db + " was opened"); }
  if(!ok) { p->failed_dbs.insert(db); return p->fail(e); }
  // gxdJournal::Attach() checkpoints the journal, with the lock held: the journal is emptied first with the files
  // synced without the lock, so that checkpoint has little or nothing left (a new primary held the lock 74 s here)
  for(int i = 0; i < 3; i++) {
    p->wait_commits();
    if(p->closing || g0 != p->gen || !p->s.is_open) { pre->close(); return p->fail("the node was closed while " + db + " was opened"); }
    if(p->checkpoint_unlocked()) break;
  }
  p->mu.at("OpenDatabase attach");
  if(p->s.j.InTransaction()) { pre->close(); return p->fail("the database " + db + " is not open (open it before Begin())"); }
  return p->s.get(db, std::move(pre)) ? 0 : p->fail(p->s.err);
}

int gxObjRepl::Begin()
{
  std::lock_guard<ObjMutex> g_(p->mu);
  p->mu.at("Begin");
  if(!p->s.is_open || p->role != PRIMARY) return p->fail("not an open primary");
  // a transaction begins once no commit's sync is running (review: a Begin() during another thread's unlocked sync
  // had its touched list cleared by that Commit() when it took the lock back, and its operations left out of the
  // outbox frame)
  p->wait_commits();
  if(!p->s.is_open) return p->fail("not an open primary");
  if(p->s.j.InTransaction()) return p->fail("a transaction is open");
  if(!p->s.begin()) return p->fail(p->s.err);
  p->touched = p->held; p->held.clear(); p->moved.clear(); p->linked_valid = false;
  return 0;
}

uint64_t gxObjRepl::NewId() { std::lock_guard<ObjMutex> g_(p->mu); return p->next_id++; }

int gxObjRepl::Put(const std::string &db, uint64_t id, const void *bytes, uint32_t n)
{
  std::lock_guard<ObjMutex> g_(p->mu);
  p->mu.at("Put");
  if(!p->s.j.InTransaction() || p->role != PRIMARY) return p->fail("Put() outside a transaction");
  if(!id) return p->fail("id 0 is not an object id");
  if(n > MAX_OBJECT) return p->fail("an object is at most " + std::to_string(MAX_OBJECT) + " bytes");
  Db *d = p->s.get(db);
  if(!d) return p->fail(p->s.err);
  bool was = d->live(id), mv = false;
  p->snap_keep(d, id);                                         // a full send under way keeps it as it was
  uint32_t ver = d->versions.count(id) ? d->versions[id] + 1 : 1;
  if(!d->put(id, ver, (const unsigned char *)bytes, n, 0, false, false, mv)) return p->fail("cannot write " + db + " " + std::to_string(id));
  if(mv) p->moved.insert({db, id});
  p->touch(was ? OP_UPDATE : OP_ADD, db, id);
  p->linked_valid = false;
  // an id the application chose moves NewId() past it, but never into the next epoch's range (review 2)
  if(id >= p->next_id && id < ((p->s.st.epoch + 1) << ID_EPOCH_SHIFT)) p->next_id = id + 1;
  return 0;
}

int gxObjRepl::Remove(const std::string &db, uint64_t id)
{
  std::lock_guard<ObjMutex> g_(p->mu);
  p->mu.at("Remove");
  if(!p->s.j.InTransaction() || p->role != PRIMARY) return p->fail("Remove() outside a transaction");
  Db *d = p->s.get(db);
  if(!d) return p->fail(p->s.err);
  p->snap_keep(d, id);
  if(!d->tombstone(id, d->where.count(id) ? TR_REMOVED : TR_UNKNOWN_REMOVE, p->s.st.next_seq)) return p->fail("cannot remove " + db + " " + std::to_string(id));
  p->touch(OP_REMOVE, db, id);
  p->linked_valid = false;
  return 0;
}

int gxObjRepl::Purge(const std::string &db, uint64_t id)
{
  std::lock_guard<ObjMutex> g_(p->mu);
  p->mu.at("Purge");
  if(!p->s.j.InTransaction() || p->role != PRIMARY) return p->fail("Purge() outside a transaction");
  Db *d = p->s.get(db);
  if(!d) return p->fail(p->s.err);
  if(d->live(id)) return p->fail("a live object is never purged (remove it first)");
  if(!p->links.empty()) {
    if(!p->linked_valid) p->build_linked();
    if(p->linked.count({db, id})) return p->fail(db + " " + std::to_string(id) + " is still named by a link: purge refused");
  }
  p->snap_keep(d, id);
  if(!d->purge(id)) return p->fail("cannot purge " + db + " " + std::to_string(id));
  p->touch(OP_PURGE, db, id);
  return 0;
}

int gxObjRepl::Commit()
{
  std::lock_guard<ObjMutex> g_(p->mu);
  p->mu.at("Commit");
  if(!p->s.j.InTransaction() || p->role != PRIMARY) return p->fail("Commit() outside a transaction");
  bool ok = p->record_commit();
  p->touched.clear(); p->moved.clear();
  if(!ok) { if(p->s.j.InTransaction()) p->s.j.Rollback(); std::string e = p->err; p->reopen(); return p->fail(e); }
  p->checkpoint_soon();
  return 0;
}

int gxObjRepl::Rollback()
{
  std::lock_guard<ObjMutex> g_(p->mu);
  p->mu.at("Rollback");
  if(!p->s.j.InTransaction()) return 0;
  p->s.j.Rollback();
  p->touched.clear(); p->moved.clear();
  return p->reopen() ? 0 : -1;
}

void gxObjRepl::TestHoldAdd(const std::string &db, uint64_t id)
{
  std::lock_guard<ObjMutex> g_(p->mu);
  for(auto it = p->touched.begin(); it != p->touched.end(); ++it)
    if(std::get<0>(*it) == OP_ADD && std::get<1>(*it) == db && std::get<2>(*it) == id) {
      p->held.push_back(*it); p->touched.erase(it); return;
    }
}

int gxObjRepl::Pump()
{
  std::lock_guard<std::mutex> pg(p->pumpmu);                 // one Pump() at a time (review 4)
  struct Pumping { std::atomic<int> &f; Pumping(std::atomic<int> &x) : f(x) { f++; } ~Pumping() { f--; } } pm(p->pumping);
  if(p->closing) return p->fail("closing");
  Link &L = p->L;
  // the TCP connect and the TLS handshake run without the node's lock (review 3: readers waited up to 30 s on a copy
  // that did not answer); the hello and everything after it run with it
  bool connect = false;
  {
    std::lock_guard<ObjMutex> g_(p->mu);
    p->mu.at("Pump start");
    if(!p->s.is_open || p->role != PRIMARY) return p->fail("not an open primary");
    if(p->s.j.InTransaction()) return p->fail("Pump() inside a transaction");
    if(L.st && !L.fresh && p->cfg.idle_timeout > 0 && time(0) - L.last_contact > p->cfg.idle_timeout / 2) L.drop();
    connect = L.st == 0;
  }
  gxStream *ns = 0;
  if(connect) { ns = new gxStream; if(!p->connect_to(*ns)) { ns->Close(); delete ns; ns = 0; } }
  std::lock_guard<ObjMutex> g_(p->mu);
  p->mu.at("Pump");
  if(!p->s.is_open || p->closing) { if(ns) { ns->Close(); delete ns; } return p->fail("closed"); }   // review: Close() waited a hello
  if(connect && !ns) { p->primary_status(true, "copy away"); return 1; }
  if(ns) { L.drop(); L.st = ns; L.fresh = true; }
  bool was = !connect;
  long ex0 = L.exchanges;
  bool ok = p->pump1();
  // a reused connection with nothing sent proves nothing: the copy may have been replaced or restarted since. One
  // F_EPOCH exchange checks it before 0 is returned (review 3: Drain() returned 0 for an empty new copy)
  if(ok && was && L.exchanges == ex0 && L.st) {
    Buf e; e.u8(F_EPOCH); e.u64(p->s.st.epoch); uint64_t a = 0;
    if(!p->exchange(e.b, a)) { L.drop(); ok = p->pump1(); }
  }
  if(!ok && L.st) L.drop();                                    // a failed exchange: reconnect (and check) next time
  p->primary_status(was != (L.st != 0) || L.refused || !ok, L.refused ? "refused" : L.st ? "streaming" : "copy away");
  if(L.refused) return 3;
  return ok && L.copy_applied + 1 >= p->s.st.next_seq ? 0 : 1;
}

int gxObjRepl::Drain(int seconds)
{
  auto until = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
  for(;;) {
    int rc = Pump();
    if(rc == 0 || rc == 3 || rc < 0) return rc;
    if(std::chrono::steady_clock::now() >= until) return 2;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
}

void gxObjRepl::Stop() { p->stop = 1; }

int gxObjRepl::Serve()
{
  {
    std::lock_guard<ObjMutex> g_(p->mu);
    if(!p->s.is_open || p->role != COPY) return p->fail("not an open copy");
    p->stop = 0;                                               // a Stop() before this Serve() is spent (review 3)
    p->serving = 1;
  }
  int rc = p->serve();
  p->serving = 0;
  return rc;
}

int gxObjRepl::Impl::serve()
{
  gxStreamServer srv;
  srv.SetClientCAFile(cfg.ca);
  if(srv.AllowClientCN(cfg.allow_cn) != 0) return fail("allow-cn is required");
  if(srv.Listen(cfg.listen_port, cfg.cert, cfg.key, std::string(), cfg.listen_addr) != 0)
    return fail("cannot listen on " + cfg.listen_addr + ":" + std::to_string(cfg.listen_port) + ": " + srv.Error());
  std::unique_lock<ObjMutex> sl(mu);             // the start: said and the status written with the node held
  say("copy listening on " + cfg.listen_addr + ":" + std::to_string(cfg.listen_port) + " (gxStream, TLS, client CN " + cfg.allow_cn +
      "), applied " + std::to_string(s.st.applied_seq) + ", epoch " + std::to_string(s.st.epoch) + (s.st.in_snapshot ? ", a snapshot was cut" : ""));
  // ROOT/prev, the data set aside for a full send that has ended: removed without the node held (only this thread
  // sets data aside; one that stays is removed at the next open or full send)
  std::string sroot = s.root;
  auto s_drop_aside = [](const std::string &r) { std::error_code ec; fs::remove_all(path(r, "prev"), ec); fs::remove_all(path(r, "prev.tmp"), ec); };
  bool rolled = false;      // a rolled-back apply: the maps and index memory are stale, the node is opened again
  unsigned long long frames_total = 0, rollbacks = 0, refusals = 0;
  bool rj_ok = false;                                          // 7.5 beta 3: this connection may do an incremental rejoin
  Node::IdSet rj_ids;                                          // and the objects its batches wrote (links fixed at F_RJ_END)
  bool rj_started = false;
  time_t last_frame = 0, snap_since = 0;
  std::string primary_now;
  long drop_every = 0;
#ifdef GXREPL_TEST_HOOKS
  drop_every = getenv("OBJREPL_TEST_DROP_EVERY") ? atol(getenv("OBJREPL_TEST_DROP_EVERY")) : 0;
  bool kill_in_fixup = getenv("OBJREPL_TEST_KILL_IN_FIXUP") != 0;
  // 7.5 beta 3: killed inside an incremental rejoin's batch ("batch") or end ("end") transaction
  std::string kill_in_rejoin = getenv("OBJREPL_TEST_KILL_IN_REJOIN") ? getenv("OBJREPL_TEST_KILL_IN_REJOIN") : "";
  // 7.4.1: a copy that stalls (a slow disk) for SECS seconds before it applies frame AT, once, not reading its socket
  long stall_at = getenv("OBJREPL_TEST_STALL_AT") ? atol(getenv("OBJREPL_TEST_STALL_AT")) : 0;
  long stall_secs = getenv("OBJREPL_TEST_STALL_SECS") ? atol(getenv("OBJREPL_TEST_STALL_SECS")) : 0;
#endif
  auto status = [&](bool force, const std::string &state, bool connected) {
    mon.set("role", "copy"); mon.set("name", cfg.name); mon.set("epoch", (unsigned long long)s.st.epoch);
    mon.set("state", state); mon.set("connected", connected ? 1ULL : 0ULL); mon.set("primary", primary_now.empty() ? "-" : primary_now);
    mon.set("applied_seq", (unsigned long long)s.st.applied_seq); mon.set("in_snapshot", (unsigned long long)s.st.in_snapshot);
    if(s.st.in_snapshot) { if(!snap_since) snap_since = time(0); } else snap_since = 0;
    mon.set("snapshot_secs", (unsigned long long)(snap_since ? time(0) - snap_since : 0));
    mon.set("frames", frames_total); mon.set("last_frame", (unsigned long long)last_frame);
    mon.set("rollbacks", rollbacks); mon.set("refusals", refusals); mon.set("last_contact", (unsigned long long)last_frame);
    unsigned long long tombs = 0; tombstone_count(tombs);
    mon.set("tombstones", tombs); mon.set("cert_days_left", std::to_string(cert_days));
    mon.set("dropped_at_rejoin", (unsigned long long)s.st.rejoin_dropped);
    mon.write(force);
  };
  // waits for the socket in half seconds, so Stop() is seen (the socket calls retry when a signal interrupts them)
  // idle: seconds without a frame after which a connection is dropped (0 = wait for ever: the listener)
  auto wait = [&](gxSocket &sock, int idle) -> int {
    time_t since = time(0);
    for(;;) {
      if(stop) return 0;
      int w = sock.WaitReadable(500);
      if(w != 0) return w > 0 ? 1 : -1;
      if(idle > 0 && time(0) - since >= idle) return -2;
    }
  };
  // 11/19/2026 (7.4.1): a connection accepted while another was being served (see the frame loop), served next
  std::unique_ptr<gxStream> next;
  // the TLS accept, with its client certificate check: 0 = an authenticated connection in X
  auto accept_one = [&](gxStream &x) -> int {
    x.SetTimeOut(30);                                          // the handshake and the hello: a stalled client is dropped
    return srv.Accept(x);
  };
  status(true, "listening", false);
  sl.unlock();
  while(!stop) {
    std::unique_ptr<gxStream> cur;
    if(next) cur = std::move(next);
    else {
      if(wait(srv.Listener(), 0) <= 0) { if(stop) break; continue; }
      cur.reset(new gxStream);
      if(accept_one(*cur) != 0) continue;
    }
    gxStream &st = *cur;
    st.SetTimeOut(30);
    st.Connection().Socket().SetNoDelay(true);                // the acks go out at once (see connect_to)
    std::vector<unsigned char> b;
    if(!recv_frame(st, b)) {
      if(st.PeerOldHeader()) { std::lock_guard<ObjMutex> al(mu); alarm("a primary that sends the gxStream block header with 4-byte words (DataReel before 7.4 beta 5 on Windows or a 32-bit build) was refused"); }
      st.Close(); continue;
    }
    st.SetTimeOut(cfg.idle_timeout > 0 ? cfg.idle_timeout : 3600);   // a frame that stops half way: dropped too
    std::unique_lock<ObjMutex> hl(mu);           // the hello: the node is held while it is answered
    mu.at("copy hello");
    Rd h(b.data(), b.size()); uint32_t magic = h.u32(); uint64_t pep = h.u64(); std::string lspec = h.str();
    std::string pname = h.str(); uint8_t prole = h.u8(); uint64_t plast = h.u64(); h.u64();
    // 7.5 beta 3 fields (an older primary has none): its epoch base and schema fingerprint
    bool has_rj = false; uint64_t rj_base = 0, rj_bdig = 0; std::string pfp;
    if(h.ok && h.o + 4 <= h.n) {
      Rd x(h.p + h.o, h.n - h.o);
      if(x.u32() == RJ_MAGIC) { rj_base = x.u64(); rj_bdig = x.u64(); pfp = x.str(); has_rj = x.ok; }
    }
    std::string why;
    std::vector<LinkDef> plinks;
    if(!h.ok || magic != STATE_MAGIC) why = "not an objrepl primary (format)";
    else if(s.st.role == ROLE_PRIMARY) why = "this node is a primary (taken over at epoch " + std::to_string(s.st.epoch) + ")";
    else if(prole != ROLE_PRIMARY) why = "the peer " + pname + " does not say it is a primary";
    else if(!cfg.peer_name.empty() && pname != cfg.peer_name) why = "the peer is " + pname + ", not the expected primary " + cfg.peer_name;
    else if(pep < s.st.epoch) why = "the primary's epoch " + std::to_string(pep) + " is older than this copy's " + std::to_string(s.st.epoch);
    else if(!s.st.in_snapshot && s.st.applied_seq > plast)
      why = "this copy is at seq " + std::to_string(s.st.applied_seq) + ", past the primary's last commit " + std::to_string(plast);
    else if(!parse_links(lspec, plinks)) why = "a bad link map";
    else if(!cfg.links.empty() && links_canon(plinks) != links_canon(links_own)) {
      std::string a = links_canon(plinks), c = links_canon(links_own);
      char hx[64]; snprintf(hx, sizeof(hx), "%016llx vs %016llx", (unsigned long long)fnv(a.data(), a.size()), (unsigned long long)fnv(c.data(), c.size()));
      why = "the primary's link map [" + a + "] differs from this copy's [" + c + "] (hash " + hx + ")";
    }
    else for(auto &x : ixdefs) for(auto &l : plinks)
      if(why.empty() && x.db == l.db && l.off < x.off + (x.kind == 'B' ? x.len : 4 * x.len) && x.off < l.off + 8)
	why = "the primary's link field " + l.db + ":" + std::to_string(l.off) + " overlaps this copy's index " + x.name;
    // 7.5 beta 3: a former primary waiting for an incremental rejoin takes one only when the primary's history
    // matches its own at the primary's epoch base (the same digest there) and the schemas agree; a schema difference
    // refuses the primary (the node is rebuilt instead); otherwise it waits for a full send
    std::string myfp = fingerprint(cfg.links.empty() ? plinks : links_own);
    rj_ok = false; rj_started = false; rj_ids.clear();
    if(why.empty() && s.st.in_snapshot == SNAP_INCR) {
      // the histories first: when they do not match, a full send replaces this node's data whatever its schema
      uint64_t d = 0;
      std::string fd = has_rj ? fp_diff(pfp, myfp) : std::string();
      if(!has_rj) say("copy: " + pname + " is an older primary: this node waits for a full send instead of an incremental rejoin");
      else if(!rj_base) say("copy: " + pname + " has no epoch base: a full send instead of an incremental rejoin");
      else if(!s.digest_at(rj_base, d) || d != rj_bdig) say("copy: " + pname + "'s history does not match this node's at seq " + std::to_string(rj_base) + ": a full send instead of an incremental rejoin");
      else if(!fd.empty()) why = "the schema differs, so an incremental rejoin is refused (rebuild this node): " + fd;
      else rj_ok = true;
    } else if(why.empty() && has_rj && !s.st.in_snapshot) {
      std::string fd = fp_diff(pfp, myfp);
      if(!fd.empty()) say("copy: warning: the schema of " + pname + " differs from this copy's: " + fd);
    }
    bool ok = why.empty();
    if(ok) pending_links = plinks;                             // adopted when the primary accepts this copy (F_EPOCH)
    if(!ok) { alarm("refused " + pname + ": " + why); refusals++; status(true, "refused " + pname, false); }
    else { primary_now = pname; status(true, "streaming", true); }
    Buf rep;
    rep.u8(ok ? 1 : 0); rep.u64(s.st.applied_seq); rep.u64(s.st.epoch); rep.u8(s.st.in_snapshot ? 1 : 0);
    rep.str(cfg.name); rep.u8(ROLE_COPY); rep.u64(s.st.applied_digest); rep.str(why);
    rep.u32(RJ_MAGIC); rep.u8(rj_ok ? 1 : 0); rep.str(myfp);  // 7.5 beta 3 (older primaries ignore these)
    hl.unlock();                                               // the answer goes out without the node held
    send_frame(st, rep.b);
    if(!ok) { st.Close(); continue; }
    // the primary's epoch is saved with the first frame applied from it, never at the hello: a primary that then
    // refuses this copy must not fence out the real one (11/16/2026, review)
    long frames = 0;
    bool accepted = false;                                     // F_EPOCH seen on this connection
    while(!stop) {
      // 11/19/2026 (7.4.1): the listener is watched too. A primary connects again only when it has given up on this
      // connection (its ack-timeout passed, it restarted, or the network lost this one without a close), so a new
      // connection that passes the TLS client check replaces this one at once. Before, it waited in the listen
      // queue until this one went idle for idle-timeout seconds, and the primary's tries piled up there unaccepted
      // (CLOSE-WAIT, their TLS hello unread). One that fails the check is closed, and this one goes on
      int w;
      {
	time_t since = time(0);
	for(;;) {
	  if(stop) { w = 0; break; }
	  int r = wait_two(st.Connection().Socket().GetSocket(), srv.Listener().GetSocket(), 500);
	  if(r == 1) { w = 1; break; }
	  if(r < 0) { w = -1; break; }
	  if(r == 2) {
	    std::unique_ptr<gxStream> nx(new gxStream);
	    if(accept_one(*nx) == 0) { next = std::move(nx); w = -3; break; }
	    continue;                                          // not authenticated: closed, this one goes on
	  }
	  if(cfg.idle_timeout > 0 && time(0) - since >= cfg.idle_timeout) { w = -2; break; }
	}
      }
      if(w == -2) { say("copy: no frame from " + pname + " in " + std::to_string(cfg.idle_timeout) + " s: connection dropped"); break; }
      if(w == -3) { say("copy: a new connection passed the TLS client check: the connection from " + pname + " is dropped"); break; }
      if(w <= 0 || !recv_frame(st, b)) break;
#ifdef GXREPL_TEST_HOOKS
      if(stall_at && (long)frames_total + 1 == stall_at && stall_secs > 0) {
	say("copy: test stall of " + std::to_string(stall_secs) + " s before frame " + std::to_string(stall_at));
	for(long i = 0; i < stall_secs * 10 && !stop; i++) std::this_thread::sleep_for(std::chrono::milliseconds(100));
	stall_at = 0;
      }
#endif
      if(waiting > 0) std::this_thread::sleep_for(std::chrono::microseconds(300));   // readers first (see step_aside)
      std::unique_lock<ObjMutex> fl(mu);         // the frame: applied with the node held
      // the ack goes out after the lock is given up (11/16/2026: readers waited for the socket write too)
      bool aside = false;                                      // the full send ended: ROOT/prev goes (unlocked)
      auto ack = [&](const Buf &a) -> bool {
	sync_snapflag();
	fl.unlock();
	bool sent = send_frame(st, a.b);
	if(aside) s_drop_aside(sroot);
	if(sent && !rolled) { fl.lock(); checkpoint_soon(); fl.unlock(); }
	return sent;
      };
      mu.at("copy frame");
      if(drop_every && ++frames % drop_every == 0) break;      // test only: the connection drops (no restart)
      frames_total++; last_frame = time(0);
      status(false, "streaming", true);
      Rd r(b.data(), b.size());
      uint8_t kind = r.u8();
      bool good = r.ok;
      // a primary's data comes only after it accepted this copy (F_EPOCH): frames before that are refused (fuzz:
      // commits ahead of F_EPOCH were applied with no link map)
      if(kind != F_EPOCH && !accepted) { alarm("a frame (kind " + std::to_string(kind) + ") from " + pname + " before it accepted this copy: refused"); break; }
      if(kind == F_COMMIT) {
	mu.at("copy F_COMMIT");
	uint64_t seq = r.u64(), ep = r.u64();
	if(!r.ok || ep < s.st.epoch || s.st.in_snapshot) { alarm("a commit from " + pname + " that cannot apply here (malformed, an older epoch, or during a full send): refused"); break; }
	if(seq <= s.st.applied_seq) { Buf a; a.u64(s.st.applied_seq); ack(a); continue; }   // a resend
	if(seq != s.st.applied_seq + 1) { alarm("commit " + std::to_string(seq) + " from " + pname + " after " + std::to_string(s.st.applied_seq) + ": a gap: refused"); break; }
	if(!open_named(b.data() + r.o, b.size() - r.o, false)) { alarm("commit " + std::to_string(seq) + " from " + pname + ": malformed or a bad database name: refused"); break; }
	uint32_t nops = r.u32();
	if(!s.begin()) break;
	if(ep > s.st.epoch) s.st.epoch = ep;                   // saved with this commit
	// pass 1: every object as sent (its links are still ids); pass 2: the links of the objects this commit wrote,
	// to this copy's addresses, once every target of the commit exists (a later add, a cycle)
	std::vector<std::pair<std::string, uint64_t>> wrote;
	// what each object of the commit must be once applied: its bytes as sent (links as ids), a tombstone, or gone
	struct Want { int kind; const unsigned char *p; uint32_t n; };   // kind 1 live, 2 tombstone, 3 purged
	std::map<std::pair<std::string, uint64_t>, Want> want;
	s.seen_seq = seq;
	for(uint32_t i = 0; i < nops && good; i++) {
	  uint8_t op = r.u8(); std::string name = r.str(); uint64_t id = r.u64();
	  Db *d = s.get(name);
	  if(!r.ok || !d) { good = false; break; }
	  s.st.max_id = std::max(s.st.max_id, id);
	  if(op == OP_REMOVE) { want[{name, id}] = {2, 0, 0}; good = d->tombstone(id, d->where.count(id) ? TR_REMOVED : TR_UNKNOWN_REMOVE, seq); continue; }
	  if(op == OP_PURGE) { want[{name, id}] = {3, 0, 0}; good = d->purge(id); continue; }
	  uint32_t cap = r.u32(); uint8_t pmoved = r.u8(); uint32_t n = r.u32(); const unsigned char *pb = r.raw(n);
	  if(!r.ok || (op != OP_ADD && op != OP_UPDATE)) { good = false; break; }
	  if(!cap) continue;                                   // changed then removed in the commit: the remove follows
	  want[{name, id}] = {1, pb, n};
	  if(n > MAX_OBJECT || cap > MAX_RECORD || cap < sizeof(ObjHead) + n) {   // bounds from the wire (review)
	    say("copy: commit " + std::to_string(seq) + " has an object of " + std::to_string(n) + " bytes in " + std::to_string(cap) + ": refused");
	    good = false; break;
	  }
	  bool mv = false;    // a move never frees the old block: a stub there keeps every link to it right
	  uint32_t ver = d->versions.count(id) ? d->versions[id] + 1 : 1;
	  good = d->put(id, ver, pb, n, cap, pmoved != 0, false, mv);
	  if(good) wrote.push_back({name, id});
	}
	std::sort(wrote.begin(), wrote.end()); wrote.erase(std::unique(wrote.begin(), wrote.end()), wrote.end());
	mu.at("copy F_COMMIT links");
	for(auto &w : wrote) {
	  if(!good) break;
	  bool has = false;
	  for(auto &l : links) if(l.db == w.first) has = true;
	  if(!has) continue;
	  Db *d = s.get(w.first);
	  auto it = d->where.find(w.second);
	  if(it == d->where.end() || d->tombs.count(w.second)) continue;
	  __ULWORD__ n = d->db.ObjectLength(it->second);
	  std::vector<unsigned char> rec(n);
	  if(n < sizeof(ObjHead) || d->db.Read(rec.data(), n, it->second) != gxDBASE_NO_ERROR) { good = false; break; }
	  ObjHead oh; memcpy(&oh, rec.data(), sizeof(oh));
	  good = oh.len <= n - sizeof(oh) && s.translate(w.first, rec.data() + sizeof(oh), oh.len, false) &&
		 d->db.Write(rec.data(), n, it->second) == gxDBASE_NO_ERROR;
	}
#ifdef GXREPL_TEST_HOOKS
	{ static bool done = false; const char *c = getenv("OBJREPL_TEST_CORRUPT_APPLY");   // test only: once, after apply
	  if(good && !done && c && strtoull(c, 0, 10) == seq) {
	    for(auto &w : want) if(w.second.kind == 1 && w.second.n > 0) {
	      Db *d = s.get(w.first.first); FAU_t a = d->where[w.first.second];
	      unsigned char x; d->db.Read(&x, 1, a + (FAU_t)sizeof(ObjHead)); x ^= 0x5a; d->db.Write(&x, 1, a + (FAU_t)sizeof(ObjHead));
	      done = true; break;
	    }
	  } }
#endif
	mu.at("copy F_COMMIT read back");
	// read back: every object as stored now, its links turned back into ids, must be what was sent (11/16/2026,
	// design review: the copy checks what it holds, not only what it received)
	for(auto &w : want) {
	  if(!good) break;
	  Db *d = s.get(w.first.first);
	  auto it = d->where.find(w.first.second);
	  bool ok;
	  if(w.second.kind == 3) ok = it == d->where.end();
	  else if(w.second.kind == 2) ok = it != d->where.end() && d->tombs.count(w.first.second);
	  else {
	    std::vector<unsigned char> rec; ObjHead h;
	    ok = it != d->where.end() && !d->tombs.count(w.first.second) && d->read(it->second, rec, h) && h.len == w.second.n &&
		 s.translate(w.first.first, rec.data() + sizeof(h), h.len, true) && memcmp(rec.data() + sizeof(h), w.second.p, w.second.n) == 0;
	  }
	  if(!ok) { alarm("commit " + std::to_string(seq) + ": " + w.first.first + " " + std::to_string(w.first.second) + " read back differently from what was sent: not applied"); good = false; }
	}
	if(good) {
	  s.st.applied_seq = seq;
	  s.st.applied_digest = fnv(b.data(), b.size(), s.st.applied_digest);
	  good = s.log_digest(seq, s.st.applied_digest);
	  if(good && seq > DLOG_KEEP) good = s.trim_digests(seq - DLOG_KEEP, 100);   // the copy keeps the same window (review 3)
	  s.put_state();
	}
	// not applied, or the commit failed: rolled back, the node reopened from the files, and no ack (review)
	if(!good) { s.j.Rollback(); rolled = true; say("copy: commit " + std::to_string(seq) + " did not apply"); break; }
	mu.at("copy F_COMMIT journal");
	if(!jcommit()) { rolled = true; say("copy: commit " + std::to_string(seq) + " failed: " + s.err); break; }
	Buf a; a.u64(seq);
	if(!ack(a)) break;
      } else if(kind == F_EPOCH) {
	mu.at("copy F_EPOCH");
	uint64_t ep = r.u64();
	if(!r.ok || ep < s.st.epoch) { alarm("an epoch frame from " + pname + " older than this copy's: refused"); break; }
	accepted = true;
	// the primary accepted this copy: its link map now, and the link databases (outside any transaction:
	// placeholders may go there) (review 3: they were made at the hello, before the primary's checks)
	links = pending_links;
	for(auto &l : links) { s.get(l.db); s.get(l.target); }
	mu.at("copy F_EPOCH state");
	if(ep > s.st.epoch) {
	  if(!s.begin()) break;
	  s.st.epoch = ep; s.put_state();
	  if(!jcommit()) { rolled = true; break; }
	}
	Buf a; a.u64(s.st.applied_seq);
	if(!ack(a)) break;
      } else if(kind == F_SNAP_DLOG) {
	mu.at("copy F_SNAP_DLOG");
	uint32_t n = r.u32();
	if(!r.ok || !s.st.in_snapshot || n > DLOG_BATCH || (size_t)n * 16 > b.size() - r.o) break;
	if(!s.begin()) break;
	for(uint32_t i = 0; i < n && good; i++) {
	  size_t at0 = r.o;
	  uint64_t q = r.u64(), d = r.u64();
	  good = r.ok && q <= s.st.snapshot_base && s.log_digest(q, d);
	  s.st.snap_hash = fnv(b.data() + at0, 16, s.st.snap_hash); s.st.snap_count++;
	}
	if(good) s.put_state();
	if(!good) { s.j.Rollback(); rolled = true; break; }
	if(!jcommit()) { rolled = true; break; }
	Buf a; a.u64(0);
	if(!ack(a)) break;
      } else if(kind == F_SNAP_BEGIN) {
	mu.at("copy F_SNAP_BEGIN");
	uint64_t base = r.u64(), ep = r.u64();
	if(!r.ok || ep < s.st.epoch) break;
	// first the state says a full send is under way (the old data still here), then the old data goes aside and the
	// node opens again empty (see Node::set_aside). A ROOT/prev left from an ended full send is removed first, so it is
	// never taken for this one's (review 4)
	if(!s.st.in_snapshot && (exists(path(s.root, "prev")) || exists(path(s.root, "prev.tmp"))) && !s.drop_aside()) {
	  alarm("a full send cannot start: " + path(s.root, "prev") + " cannot be removed"); break;
	}
	if(!s.begin()) break;
	if(ep > s.st.epoch) s.st.epoch = ep;
	s.st.in_snapshot = 1; s.st.snapshot_base = base; s.st.applied_seq = 0; s.st.applied_digest = DIGEST0;
	s.st.snap_count = 0; s.st.snap_hash = DIGEST0;
	s.put_state();
	if(!jcommit()) { rolled = true; break; }
	// readers fail at once from here (a full send is under way) instead of waiting for the lock through the set-aside
	// and reopen below (11/16/2026: a copy reader waited out a 40 s SNAP_BEGIN)
	snapflag = 1;
	if(!s.set_aside()) { say("copy: " + s.err); s.close(); return fail(s.err); }
	for(auto &l : links) { s.get(l.db); s.get(l.target); }   // the link databases again (empty)
	Buf a; a.u64(0);
	if(!ack(a)) break;
      } else if(kind == F_SNAP_BATCH) {
	mu.at("copy F_SNAP_BATCH");
	if(!s.st.in_snapshot || !open_named(b.data() + r.o, b.size() - r.o, true)) break;
	uint32_t nobj = r.u32();
	if(!s.begin()) break;
	for(uint32_t i = 0; i < nobj && good; i++) {
	  size_t at0 = r.o;
	  std::string name = r.str(); uint64_t id = r.u64(); uint32_t ver = r.u32(), flags = r.u32(), reason = r.u32();
	  uint64_t seen = r.u64(); uint32_t cap = r.u32(); uint32_t n = r.u32(); const unsigned char *pb = r.raw(n);
	  Db *d = s.get(name);
	  if(!r.ok || !d) { good = false; break; }
	  s.st.snap_hash = fnv(b.data() + at0, r.o - at0, s.st.snap_hash); s.st.snap_count++;
	  if(n > MAX_OBJECT || cap > MAX_RECORD || (!(flags & OF_TOMB) && cap < sizeof(ObjHead) + n)) { say("copy: a full-send record out of bounds: refused"); good = false; break; }
	  s.st.max_id = std::max(s.st.max_id, id);
	  bool mv = false;
	  good = (flags & OF_TOMB) ? d->tombstone(id, reason, seen, cap) : d->put(id, ver, pb, n, cap, false, true, mv);
	}
	if(good) s.put_state();
	if(!good) { s.j.Rollback(); rolled = true; break; }
	if(!jcommit()) { rolled = true; break; }
	Buf a; a.u64(0);
	if(!ack(a)) break;
      } else if(kind == F_SNAP_END) {
	mu.at("copy F_SNAP_END");
	uint64_t base = r.u64(), sdig = r.u64(), scount = r.u64(), shash = r.u64();
	if(!r.ok || !s.st.in_snapshot || base != s.st.snapshot_base) { alarm("a full-send end that does not match the full send under way: refused"); break; }
	// what arrived must be what was sent (fuzz: a lost, repeated or changed batch ended a full send that looked whole)
	if(scount != s.st.snap_count || shash != s.st.snap_hash) {
	  alarm("the full send to seq " + std::to_string(base) + " arrived incomplete or changed (" + std::to_string(s.st.snap_count) + " of " +
		std::to_string(scount) + " records): not ended; the primary sends it again");
	  break;
	}
	if(!s.begin()) break;
	s.seen_seq = base;
	// the links came as ids (targets may have arrived later in the snapshot): now each to this copy's address
	for(auto &d : s.dbs) {
	  bool has = false;
	  for(auto &l : links) if(l.db == d.first) has = true;
	  if(!has) continue;
	  std::vector<std::pair<uint64_t, FAU_t>> recs(d.second->where.begin(), d.second->where.end());   // placeholders may be added
	  for(auto &w : recs) {
	    if(d.second->tombs.count(w.first)) continue;
	    __ULWORD__ n = d.second->db.ObjectLength(w.second);
	    std::vector<unsigned char> rec(n);
	    if(n < sizeof(ObjHead) || d.second->db.Read(rec.data(), n, w.second) != gxDBASE_NO_ERROR) { good = false; break; }
	    ObjHead oh; memcpy(&oh, rec.data(), sizeof(oh));
	    if(oh.len > n - sizeof(oh) || !s.translate(d.first, rec.data() + sizeof(oh), oh.len, false)) { good = false; break; }
	    if(d.second->db.Write(rec.data(), n, w.second) != gxDBASE_NO_ERROR) { good = false; break; }
#ifdef GXREPL_TEST_HOOKS
	    if(kill_in_fixup) _exit(9);                      // test only: killed inside the link fix-up's transaction
#endif
	  }
	}
	if(good) good = s.log_digest(base, sdig);
	if(!good) { s.j.Rollback(); rolled = true; break; }
	s.st.in_snapshot = 0; s.st.applied_seq = base; s.st.applied_digest = sdig;
	s.put_state();
	if(!jcommit()) { rolled = true; break; }
	aside = true;                                          // the full send is whole: the old data goes (after the ack)
	Buf a; a.u64(base);
	if(!ack(a)) break;
      } else if(kind == F_RJ_SET) {
	// 7.5 beta 3, the incremental rejoin: this node's own commits past BASE are dropped from its digest log (the
	// primary's follow as F_SNAP_DLOG up to UPTO), and the answer lists what those commits changed
	mu.at("copy F_RJ_SET");
	uint64_t base = r.u64(), upto = r.u64(), d = 0;
	if(!r.ok || !rj_ok || s.st.in_snapshot != SNAP_INCR || upto < base || !s.digest_at(base, d)) { alarm("an incremental rejoin from " + pname + " that does not match this node: refused"); break; }
	Node::IdSet mine;
	if(!Node::list_since(s.rjs, base, mine)) { alarm("cannot read " + path(s.root, "objrepl.rejoin.gxd")); break; }
	if(!s.begin()) break;
	good = s.dlog_drop_after(base);
	s.st.snapshot_base = upto; s.st.snap_count = 0; s.st.snap_hash = DIGEST0;
	if(good) s.put_state();
	if(!good) { s.j.Rollback(); rolled = true; break; }
	if(!jcommit()) { rolled = true; break; }
	rj_started = true; rj_ids.clear();
	Buf a; a.u8(1);
	size_t sz = 5; for(auto &x : mine) sz += 10 + x.first.size();
	if(sz > MAX_FRAME) { a.b.clear(); a.u8(0); a.u32(0); }  // too many: the primary sends a full send
	else { a.u32((uint32_t)mine.size()); for(auto &x : mine) { a.str(x.first); a.u64(x.second); } }
	say("copy: incremental rejoin from " + pname + " at seq " + std::to_string(base) + ": " + std::to_string(mine.size()) + " object(s) changed here since");
	if(!ack(a)) break;
      } else if(kind == F_RJ_BATCH) {
	mu.at("copy F_RJ_BATCH");
	if(!rj_started || s.st.in_snapshot != SNAP_INCR) break;
	uint32_t nobj = r.u32();
	// the databases first (outside the transaction, as open_named() does for a commit)
	{
	  Rd q(b.data() + r.o, b.size() - r.o);
	  for(uint32_t i = 0; i < nobj && q.ok; i++) {
	    uint8_t k = q.u8(); std::string name = q.str(); q.u64();
	    if(k == 1) { q.u32(); q.u32(); q.u32(); q.u64(); q.u32(); uint32_t n = q.u32(); q.raw(n); }
	    else if(k != 2) q.ok = false;
	    if(q.ok && (!good_name(name) || (k == 1 && !s.get(name)))) q.ok = false;
	  }
	  if(!q.ok) { alarm("a malformed incremental-rejoin batch from " + pname + ": refused"); break; }
	}
	if(!s.begin()) break;
	s.seen_seq = s.st.snapshot_base;
	for(uint32_t i = 0; i < nobj && good; i++) {
	  size_t at0 = r.o;
	  uint8_t k = r.u8(); std::string name = r.str(); uint64_t id = r.u64();
	  if(k == 2) {                                         // absent on the primary: gone here too
	    s.st.snap_hash = fnv(b.data() + at0, r.o - at0, s.st.snap_hash); s.st.snap_count++;
	    auto di = s.dbs.find(name);
	    if(di == s.dbs.end()) continue;
	    Db *d = di->second.get();
	    if(d->where.count(id)) good = (d->tombs.count(id) || d->tombstone(id, TR_REMOVED, s.seen_seq)) && d->purge(id);
	    continue;
	  }
	  uint32_t ver = r.u32(), flags = r.u32(), reason = r.u32(); uint64_t seen = r.u64(); uint32_t cap = r.u32();
	  uint32_t n = r.u32(); const unsigned char *pb = r.raw(n);
	  Db *d = s.get(name);
	  if(!r.ok || !d) { good = false; break; }
	  s.st.snap_hash = fnv(b.data() + at0, r.o - at0, s.st.snap_hash); s.st.snap_count++;
	  if(n > MAX_OBJECT || cap > MAX_RECORD || (!(flags & OF_TOMB) && cap < sizeof(ObjHead) + n)) { say("copy: an incremental-rejoin record out of bounds: refused"); good = false; break; }
	  s.st.max_id = std::max(s.st.max_id, id);
	  bool mv = false;
	  if(flags & OF_TOMB) {
	    // a tombstone: one that is here already is replaced by the primary's (its reason and seq)
	    if(d->where.count(id) && d->tombs.count(id)) good = d->purge(id);
	    if(good) good = d->tombstone(id, reason, seen, cap);
	  } else {
	    good = d->put(id, ver, pb, n, cap, false, false, mv);
	    if(good) rj_ids.insert({name, id});
	  }
	}
	if(good) s.put_state();
#ifdef GXREPL_TEST_HOOKS
	if(good && kill_in_rejoin == "batch") _exit(9);           // test only: killed with the batch's transaction open
#endif
	if(!good) { s.j.Rollback(); rolled = true; rj_started = false; break; }
	if(!jcommit()) { rolled = true; rj_started = false; break; }
	Buf a; a.u64(0);
	if(!ack(a)) break;
      } else if(kind == F_RJ_END) {
	mu.at("copy F_RJ_END");
	uint64_t upto = r.u64(), sdig = r.u64(), scount = r.u64(), shash = r.u64();
	if(!r.ok || !rj_started || s.st.in_snapshot != SNAP_INCR || upto != s.st.snapshot_base) { alarm("an incremental-rejoin end that does not match the one under way: refused"); break; }
	if(scount != s.st.snap_count || shash != s.st.snap_hash) {
	  alarm("the incremental rejoin to seq " + std::to_string(upto) + " arrived incomplete or changed (" + std::to_string(s.st.snap_count) + " of " +
		std::to_string(scount) + " records): not ended; the primary sends it again");
	  break;
	}
	if(!s.begin()) break;
	s.seen_seq = upto;
	// the links of the objects the batches wrote came as ids: now each to this node's address
	for(auto &w : rj_ids) {
	  if(!good) break;
	  bool has = false;
	  for(auto &l : links) if(l.db == w.first) has = true;
	  if(!has) continue;
	  Db *d = s.get(w.first);
	  auto it = d ? d->where.find(w.second) : std::map<uint64_t, FAU_t>::iterator();
	  if(!d || it == d->where.end() || d->tombs.count(w.second)) continue;
	  __ULWORD__ n = d->db.ObjectLength(it->second);
	  std::vector<unsigned char> rec(n);
	  if(n < sizeof(ObjHead) || d->db.Read(rec.data(), n, it->second) != gxDBASE_NO_ERROR) { good = false; break; }
	  ObjHead oh; memcpy(&oh, rec.data(), sizeof(oh));
	  good = oh.len <= n - sizeof(oh) && s.translate(w.first, rec.data() + sizeof(oh), oh.len, false) &&
		 d->db.Write(rec.data(), n, it->second) == gxDBASE_NO_ERROR;
	}
	if(good) good = s.log_digest(upto, sdig) && Node::list_trim(s.rjs, UINT64_MAX);
	if(!good) { s.j.Rollback(); rolled = true; rj_started = false; break; }
	s.st.in_snapshot = 0; s.st.applied_seq = upto; s.st.applied_digest = sdig; s.st.snapshot_base = 0;
	s.put_state();
#ifdef GXREPL_TEST_HOOKS
	if(kill_in_rejoin == "end") _exit(9);                    // test only: killed with the end's transaction open
#endif
	if(!jcommit()) { rolled = true; rj_started = false; break; }
	say("copy: incremental rejoin from " + pname + " ended at seq " + std::to_string(upto) + " (" + std::to_string(scount) + " records)");
	rj_started = false; rj_ok = false; rj_ids.clear();
	Buf a; a.u64(upto);
	if(!ack(a)) break;
      } else { alarm("an unknown frame (kind " + std::to_string(kind) + ") from " + pname + ": refused"); break; }
    }
    st.Close();
    std::lock_guard<ObjMutex> el(mu);
    mu.at("copy end of connection");
    if(rolled) { rollbacks++; if(!reopen()) { sync_snapflag(); return -1; } rolled = false; }
    sync_snapflag();
    status(true, "listening", false);
  }
  std::lock_guard<ObjMutex> el(mu);
  // 11/17/2026: "stopping" until Close() has closed the node: the close's journal checkpoint syncs every file,
  // minutes on a saturated disk, and the status says so (Close() writes "stopped" when it is done)
  if(!mon.v.count("stopping_since")) mon.set("stopping_since", (unsigned long long)time(0));
  status(true, "stopping", false);
  return 0;
}

// ---------- reads ----------
// a reader's lock: counted while it waits, so Serve() and Pump() step aside for it
struct RLock {
  gxObjRepl::Impl *p; std::unique_lock<ObjMutex> l;
  RLock(gxObjRepl::Impl *q) : p(q) {
#ifdef GXREPL_TEST_HOOKS
    auto t0 = std::chrono::steady_clock::now();
#endif
    p->waiting++; l = std::unique_lock<ObjMutex>(p->mu); p->waiting--;
#ifdef GXREPL_TEST_HOOKS
    ObjMutex::note("reader wait", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
#endif
    p->mu.at("read");
  }
};
// a copy in the middle of a full send holds a part of the data, with links not yet translated: reads wait for it to
// end (review 3: readers saw links naming nothing)
#define GXOBJREPL_NO_READS_IN_FULL_SEND(ret) if(p->role == COPY && p->s.is_open && p->s.st.in_snapshot) { p->err = "a full send is under way"; return ret; }
// the same before the lock is taken: a copy in a full send holds the lock for each batch it applies (11/16/2026)
#define GXOBJREPL_FULL_SEND_FAST(ret) if(p->snapflag) { p->err = "a full send is under way"; return ret; }
int gxObjRepl::InSnapshot() { if(p->snapflag) return 1; RLock g_(p); return p->s.is_open && p->s.st.in_snapshot; }
int gxObjRepl::Get(const std::string &db, uint64_t id, std::vector<unsigned char> &bytes)
{
  bytes.clear();
  GXOBJREPL_FULL_SEND_FAST(-1)
  RLock g_(p);
  bytes.clear();
  GXOBJREPL_NO_READS_IN_FULL_SEND(-1)
  auto it = p->s.dbs.find(db);
  if(it == p->s.dbs.end() || !it->second->live(id)) return -1;
  std::vector<unsigned char> rec; ObjHead h;
  if(!it->second->read(it->second->where[id], rec, h)) return -1;
  bytes.assign(rec.begin() + sizeof(h), rec.begin() + sizeof(h) + h.len);
  return 0;
}
int gxObjRepl::IsLive(const std::string &db, uint64_t id)
{
  GXOBJREPL_FULL_SEND_FAST(0)
  RLock g_(p);
  GXOBJREPL_NO_READS_IN_FULL_SEND(0)
  auto it = p->s.dbs.find(db);
  return it != p->s.dbs.end() && it->second->live(id);
}
FAU_t gxObjRepl::AddressOf(const std::string &db, uint64_t id)
{
  GXOBJREPL_FULL_SEND_FAST((FAU_t)0)
  RLock g_(p);
  GXOBJREPL_NO_READS_IN_FULL_SEND((FAU_t)0)
  auto it = p->s.dbs.find(db);
  if(it == p->s.dbs.end()) return 0;
  auto w = it->second->where.find(id);
  return w == it->second->where.end() ? (FAU_t)0 : w->second;
}
uint64_t gxObjRepl::IdAt(const std::string &db, FAU_t a)
{
  GXOBJREPL_FULL_SEND_FAST(0)
  RLock g_(p);
  GXOBJREPL_NO_READS_IN_FULL_SEND(0)
  auto it = p->s.dbs.find(db);
  if(it == p->s.dbs.end()) return 0;
  auto w = it->second->who.find(a);
  return w == it->second->who.end() ? 0 : w->second;
}
std::vector<uint64_t> gxObjRepl::Ids(const std::string &db, Which which)
{
  GXOBJREPL_FULL_SEND_FAST(std::vector<uint64_t>())
  RLock g_(p);
  GXOBJREPL_NO_READS_IN_FULL_SEND(std::vector<uint64_t>())
  std::vector<uint64_t> v;
  auto it = p->s.dbs.find(db);
  if(it == p->s.dbs.end()) return v;
  if(which == TOMBSTONES) v.assign(it->second->tombs.begin(), it->second->tombs.end());
  else for(auto &w : it->second->where) if(!it->second->tombs.count(w.first)) v.push_back(w.first);
  return v;
}
std::vector<std::string> gxObjRepl::Databases()
{
  RLock g_(p);
  std::vector<std::string> v;
  for(auto &d : p->s.dbs) v.push_back(d.first);
  return v;
}
int gxObjRepl::FindKey(const std::string &index, const void *key, unsigned n, std::vector<uint64_t> &ids)
{
  ids.clear();
  GXOBJREPL_FULL_SEND_FAST(-1)
  RLock g_(p);
  GXOBJREPL_NO_READS_IN_FULL_SEND(-1)
  ids.clear();
  for(auto &x : p->s.ixs) {
    if(x->def.name != index || x->def.kind != 'B') continue;
    if(n > x->def.len) return p->fail("the key is longer than the index's");
    // in key order from the first entry at or past KEY (zero padded, id 0: the smallest key that starts with KEY), up
    // to the first key past it (11/16/2026: it scanned from the first key of the index, half the index on average,
    // tens of ms with the node's lock held)
    IxKey k, c;
    memcpy(k.d.k, key, n); k.d.id = 0;
    for(int ok = x->bt.PartialFind(k, c); ok; ok = x->bt.FindNext(k, c)) {
      int cmp = memcmp(k.d.k, key, n);
      if(cmp > 0) break;
      if(cmp == 0) ids.push_back(k.d.id);
    }
    return 0;
  }
  return p->fail("no B-tree index " + index);
}
int gxObjRepl::Nearest(const std::string &index, const float *q, unsigned k, std::vector<gxObjReplHit> &hits)
{
  hits.clear();
  GXOBJREPL_FULL_SEND_FAST(-1)
  RLock g_(p);
  GXOBJREPL_NO_READS_IN_FULL_SEND(-1)
  hits.clear();
  for(auto &x : p->s.ixs) {
    if(x->def.name != index || x->def.kind != 'V') continue;
    std::vector<gxvResult> r;
    if(x->vx.Search(q, k, r) != gxv_NO_ERROR) return p->fail("the search failed");
    for(auto &e : r) hits.push_back({(uint64_t)e.id, e.distance});
    return 0;
  }
  return p->fail("no vector index " + index);
}

uint64_t gxObjRepl::Epoch() const { std::lock_guard<ObjMutex> g_(p->mu); return p->s.st.epoch; }
uint64_t gxObjRepl::LastSeq() const { std::lock_guard<ObjMutex> g_(p->mu); return p->s.st.next_seq - 1; }
uint64_t gxObjRepl::AppliedSeq() const { std::lock_guard<ObjMutex> g_(p->mu); return p->s.st.applied_seq; }
uint64_t gxObjRepl::CopyApplied() const { std::lock_guard<ObjMutex> g_(p->mu); return p->L.copy_applied; }
long gxObjRepl::Snapshots() const { std::lock_guard<ObjMutex> g_(p->mu); return p->L.snapshots; }

// ---------- folders ----------
int gxObjRepl::Digest(const std::string &root, const std::string &lspec, std::string &line, std::string *detail)
{
  settle(root);
  std::vector<LinkDef> links;
  if(!parse_links(lspec, links)) { line = "bad links"; return 1; }
  std::vector<std::string> names;
  for(auto &n : list_dir(root)) if(ends_with(n, ".gxd")) { std::string nm = n.substr(0, n.size() - 4); if(good_name(nm)) names.push_back(nm); }
  // the links are compared as the target's id, never as an address: each database's address -> id map first
  std::map<std::string, std::map<FAU_t, uint64_t>> ids;
  for(auto &nm : names) {
    gxDatabase db;
    if(db.Open(path(root, nm + ".gxd").c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) continue;
    for(FAU_t a = db.FindFirstObject(); a != (FAU_t)0; a = db.FindNextObject(a)) {
      ObjHead oh; if(db.Read(&oh, sizeof(oh), a) == gxDBASE_NO_ERROR) ids[nm][a] = oh.id;   // records and stubs
    }
    db.Close();
  }
  std::map<std::string, std::map<uint64_t, uint64_t>> all;
  size_t dangling = 0, tombs = 0, stubs = 0;
  for(auto &nm : names) {
    gxDatabase db;
    if(db.Open(path(root, nm + ".gxd").c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) { line = "cannot open " + nm; return 1; }
    auto &h = all[nm];
    for(FAU_t a = db.FindFirstObject(); a != (FAU_t)0; a = db.FindNextObject(a)) {
      __ULWORD__ len = db.ObjectLength(a);
      std::vector<unsigned char> b(len);
      if(len < sizeof(ObjHead) || db.Read(b.data(), len, a) != gxDBASE_NO_ERROR) continue;
      ObjHead oh; memcpy(&oh, b.data(), sizeof(oh));
      if(oh.len > len - sizeof(oh)) continue;
      if(oh.flags & OF_STUB) { stubs++; continue; }            // this node's own forwarding stubs: not compared
      if(h.count(oh.id)) { line = "DUPLICATE id " + std::to_string(oh.id) + " in " + nm; db.Close(); return 1; }
      for(auto &l : links) {
	if(l.db != nm || (size_t)l.off + 8 > oh.len) continue;
	uint64_t v; memcpy(&v, b.data() + sizeof(oh) + l.off, 8);
	auto &m = ids[l.target]; auto it = m.find((FAU_t)v);
	uint64_t idv = v && it != m.end() ? it->second : 0;
	if(v && it == m.end()) dangling++;
	memcpy(b.data() + sizeof(oh) + l.off, &idv, 8);
      }
      if(oh.flags & OF_TOMB) { tombs++; static const char T[] = "TOMB"; h[oh.id] = fnv(T, 4, fnv(&oh.flags, 4)); continue; }
      h[oh.id] = fnv(b.data() + sizeof(oh), oh.len, fnv(&oh.len, 4));
    }
    db.Close();
  }
  uint64_t x = DIGEST0; size_t count = 0;
  for(auto &d : all) {
    if(d.second.empty()) continue;                             // an empty database and a missing one are the same
    x = fnv(d.first.data(), d.first.size(), x);
    for(auto &e : d.second) { x = fnv(&e.first, 8, x); x = fnv(&e.second, 8, x); count++; }
  }
  line = "objects " + std::to_string(count - tombs) + " digest " + hex16(x);
  if(detail) *detail = std::to_string(tombs) + " tombstone(s) " + std::to_string(stubs) + " stub(s) " + std::to_string(dangling) + " dangling";
  return 0;
}

int gxObjRepl::IndexCheck(const std::string &root, const std::string &spec, std::string &report)
{
  settle(root);
  std::vector<IxDef> ixdefs;
  report.clear();
  if(!parse_ix(spec, ixdefs)) { report = "bad index\n"; return 5; }
  int rc = 0;
  for(auto &def : ixdefs) {
    Ix x; x.def = def;
    if(exists(x.file(root) + ".building")) {
      report += "index " + def.name + " " + def.db + " " + def.kind + " UNFINISHED BUILD (stopped in the middle; the next start builds it again)\n";
      rc = 5; continue;
    }
    gxDatabase db;
    bool nodb = !exists(path(root, def.db + ".gxd"));
    if(nodb && !exists(x.file(root))) {                        // no database and no index yet: the same as empty
      report += "index " + def.name + " " + def.db + " " + def.kind + " entries 0 digest " + hex16(0) + " consistent\n";
      continue;
    }
    if(!nodb && db.Open(path(root, def.db + ".gxd").c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) { report += "index " + def.name + ": cannot open the database " + def.db + "\n"; rc = 5; continue; }
    std::set<std::pair<std::string, uint64_t>> want_b;
    std::map<uint64_t, std::vector<float>> want_v;
    for(FAU_t a = nodb ? (FAU_t)0 : db.FindFirstObject(); a != (FAU_t)0; a = db.FindNextObject(a)) {
      __ULWORD__ n = db.ObjectLength(a);
      std::vector<unsigned char> rec(n);
      if(n < sizeof(ObjHead) || db.Read(rec.data(), n, a) != gxDBASE_NO_ERROR) continue;
      ObjHead h; memcpy(&h, rec.data(), sizeof(h));
      if((h.flags & (OF_TOMB | OF_STUB)) || h.len > n - sizeof(h)) continue;
      if(def.kind == 'B') { IxKey k; if(x.key(h.id, rec.data() + sizeof(h), h.len, k)) want_b.insert({std::string((char *)k.d.k, 32), h.id}); }
      else { std::vector<float> v; if(x.vec(rec.data() + sizeof(h), h.len, v)) want_v[h.id] = v; }
    }
    if(!nodb) db.Close();
    size_t entries = 0, missing = 0, extra = 0; uint64_t dig = DIGEST0;
    if(def.kind == 'B') {
      if(x.bt.Open(x.file(root).c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) { report += "index " + def.name + ": cannot open\n"; rc = 5; continue; }
      std::set<std::pair<std::string, uint64_t>> have;
      IxKey k, c;
      for(int ok = x.bt.FindFirst(k); ok; ok = x.bt.FindNext(k, c)) { have.insert({std::string((char *)k.d.k, 32), k.d.id}); dig = fnv(&k.d, sizeof(k.d), dig); entries++; }
      x.bt.Close();
      for(auto &w : want_b) if(!have.count(w)) missing++;
      for(auto &h : have) if(!want_b.count(h)) extra++;
    } else {
      if(x.vx.Open(x.file(root).c_str(), gxDBASE_READONLY, x.spec().c_str()) != gxv_NO_ERROR) { report += "index " + def.name + ": cannot open\n"; rc = 5; continue; }
      unsigned long long nodes = 0;
      dig = x.vx.LogicalDigest(&nodes); entries = (size_t)nodes;
      std::vector<float> got(def.len);
      for(auto &w : want_v)
	if(x.vx.Get((long long)w.first, got.data()) != gxv_NO_ERROR || memcmp(got.data(), w.second.data(), 4 * (size_t)def.len) != 0) missing++;
      if(entries > want_v.size() - std::min(missing, want_v.size())) extra = entries - (want_v.size() - missing);
      x.vx.Close();
    }
    bool good = !missing && !extra;
    if(!entries) dig = 0;                                      // empty: the same digest however the file was made
    report += "index " + def.name + " " + def.db + " " + def.kind + " entries " + std::to_string(entries) + " digest " + hex16(dig) +
	      (good ? " consistent" : " MISMATCH (" + std::to_string(missing) + " missing or wrong, " + std::to_string(extra) + " extra)") + "\n";
    if(!good) rc = 5;
  }
  return rc;
}

int gxObjRepl::Check(const std::string &root, const gxObjReplConfig &cfg, std::string &line)
{
  std::map<std::string, std::string> v;
  {
    std::string t = read_file(path(root, "objrepl.status"));
    size_t i = 0;
    while(i < t.size()) {
      size_t j = t.find('\n', i); if(j == std::string::npos) j = t.size();
      std::string l = t.substr(i, j - i); while(!l.empty() && l.back() == '\r') l.pop_back();
      size_t sp = l.find(' '); if(sp != std::string::npos) v[l.substr(0, sp)] = l.substr(sp + 1);
      i = j + 1;
    }
  }
  auto num = [&](const char *k) { return v.count(k) ? strtoull(v[k].c_str(), 0, 10) : 0ULL; };
  std::string last_alarm; unsigned long long alarms = 0;
  {
    std::string t = read_file(path(root, "objrepl.alarm"));
    size_t i = 0;
    while(i < t.size()) {
      size_t j = t.find('\n', i); if(j == std::string::npos) j = t.size();
      if(j > i) { alarms++; last_alarm = t.substr(i, j - i); }
      i = j + 1;
    }
  }
  if(v.empty() && !alarms) { line = "WARN " + root + ": no status (never run here)"; return 1; }
  bool running = pid_alive((long long)num("pid")) && v["state"].compare(0, 7, "stopped") != 0;
  unsigned long long age = (unsigned long long)time(0) - num("updated");
  std::string who = v["role"] + " " + v["name"] + " epoch " + v["epoch"];
  std::string what;
  if(v["role"] == "primary")
    what = "seq " + v["last_seq"] + ", copy applied " + v["copy_applied"] + ", lag " + v["lag_seq"] + " for " + v["behind_secs"] +
	   "s, oldest unacked " + v["oldest_unacked_secs"] + "s, outbox " + v["outbox_entries"] + " (" + v["outbox_bytes"] + " bytes), " +
	   v["tombstones"] + " tombstones, " + v["state"];
  else
    what = "applied " + v["applied_seq"] + " from " + v["primary"] + (num("in_snapshot") ? ", inside a full send for " + v["snapshot_secs"] + "s" : "") +
	   ", " + v["frames"] + " frames, " + v["tombstones"] + " tombstones, " + v["state"];
  what += running ? "" : " (not running, status " + std::to_string(age) + "s old)";
  if(alarms) { line = "FAIL " + who + ": " + what + "; " + std::to_string(alarms) + " alarm(s), last: " + last_alarm; return 2; }
  unsigned long long contact = num("last_contact");
  std::string warn;                                            // every reason, not just the first
  auto add = [&](const std::string &w) { warn += (warn.empty() ? "" : ", ") + w; };
  if(!running) add("not running");
  else if(v["role"] == "primary" && !num("connected") && num("behind_secs") > cfg.max_behind) add("the copy is away");
  else if(v["role"] == "primary" && num("lag_seq") > cfg.max_lag && num("behind_secs") > cfg.max_behind) add("the copy is behind");
  else if(v["role"] == "primary" && num("lag_seq") && num("oldest_unacked_secs") > cfg.max_behind) add("the oldest unacknowledged commit is old");
  if(running && v["role"] == "copy" && num("in_snapshot") && num("snapshot_secs") > cfg.max_behind) add("a full send is taking long");
  // 11/17/2026: a node closing: WARN after 10 s (the close's checkpoint syncs every file: slow on a busy disk, not a
  // failure); a node that died while closing is "not running" (above) and says so
  if(v["state"] == "stopping") {
    unsigned long long since = num("stopping_since"), secs = since ? (unsigned long long)time(0) - since : 0;
    if(running && secs > 10) add("stopping for " + std::to_string(secs) + "s (the close's journal checkpoint syncs every file; a busy disk makes it slow)");
    if(!running) add("it stopped while closing (killed or crashed during the close: the next open recovers)");
  }
  if(v.count("cert_days_left") && atol(v["cert_days_left"].c_str()) >= 0 && atol(v["cert_days_left"].c_str()) < cfg.min_cert_days)
    add("the certificate expires in " + v["cert_days_left"] + " day(s)");
  if(!warn.empty()) { line = "WARN " + who + ": " + warn + "; " + what; return 1; }
  line = "OK " + who + ": " + what + "; last contact " + (contact ? std::to_string((unsigned long long)time(0) - contact) + "s ago" : std::string("never"));
  return 0;
}

int gxObjRepl::ClearAlarm(const std::string &root, std::string &msg)
{
  std::string a = path(root, "objrepl.alarm"), s = a + ".seen";
  if(!exists(a)) { msg = "no alarm"; return 0; }
  std::string t = read_file(a);
  FILE *out = fopen(s.c_str(), "ab");
  if(!out) { msg = "cannot write " + s; return 1; }
  bool ok = fwrite(t.data(), 1, t.size(), out) == t.size();
  if(fclose(out) != 0 || !ok) { msg = "cannot write " + s; return 1; }
  remove_file(a);
  msg = "alarm cleared (kept in " + s + ")";
  return 0;
}

// 7.5 beta 3: NEW in place of OLD after a rebuild, OLD kept as OLD.before-rebuild-YYYYMMDD-HHMMSS
int gxObjRepl::Swap(const std::string &old_root, const std::string &new_root, const std::string &index, std::string &msg)
{
  if(old_root.empty() || new_root.empty() || fs::path(old_root).lexically_normal() == fs::path(new_root).lexically_normal()) { msg = "swap: two different folders"; return 1; }
  if(!exists(path(old_root, "state.gxd"))) { msg = old_root + " is not a node folder"; return 1; }
  std::string line;
  if(Status(new_root, line) != 0) { msg = new_root + " is not a node folder (" + line + ")"; return 1; }
  if(line.find(" in_snapshot 0 ") == std::string::npos) { msg = new_root + " is in a full send or waiting for a rejoin: refused"; return 1; }
  for(auto &r : {old_root, new_root}) {                        // both stopped: each lock taken and given back
    Node n; n.root = r;
    if(!n.lock(0)) { msg = n.err + ": stop it first"; return 1; }
    n.unlock();
  }
  std::string report;
  if(!index.empty() && IndexCheck(new_root, index, report) != 0) { msg = new_root + ": the index check failed:\n" + report; return 1; }
  time_t t = time(0); struct tm tmv;
#if defined(_WIN32)
  localtime_s(&tmv, &t);
#else
  localtime_r(&t, &tmv);
#endif
  char ts[32]; strftime(ts, sizeof(ts), "%Y%m%d-%H%M%S", &tmv);
  std::string kept = fs::path(old_root).lexically_normal().string();
  while(!kept.empty() && (kept.back() == '/' || kept.back() == '\\')) kept.pop_back();
  kept += std::string(".before-rebuild-") + ts;
  if(exists(kept)) { msg = kept + " is there already: refused"; return 1; }
  std::error_code ec;
  fs::rename(old_root, kept, ec);
  if(ec) { msg = "cannot rename " + old_root + ": " + ec.message(); return 1; }
  fs::rename(new_root, old_root, ec);
  if(ec) { std::error_code e2; fs::rename(kept, old_root, e2); msg = "cannot rename " + new_root + ": " + ec.message() + (e2 ? " (and " + old_root + " is at " + kept + ")" : " (nothing changed)"); return 1; }
  msg = new_root + " is now " + old_root + "; the old node is kept at " + kept;
  return 0;
}

int gxObjRepl::Status(const std::string &root, std::string &line)
{
  settle(root);
  gxDatabase db; State st;
  if(db.Open(path(root, "state.gxd").c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) { line = "no state"; return 1; }
  if(db.Read(&st, sizeof(st), (FAU_t)db.FileHeaderSize()) != gxDBASE_NO_ERROR || st.magic != STATE_MAGIC) { db.Close(); line = "no state"; return 1; }
  line = "epoch " + std::to_string(st.epoch) + " next_seq " + std::to_string(st.next_seq) + " acked_seq " + std::to_string(st.acked_seq) +
	 " applied_seq " + std::to_string(st.applied_seq) + " in_snapshot " + std::to_string(st.in_snapshot) + " role " +
	 (st.role == ROLE_PRIMARY ? "primary" : st.role == ROLE_COPY ? "copy" : "none") + " digest " +
	 hex16(st.role == ROLE_PRIMARY ? st.cur_digest : st.applied_digest);
  db.Close();
  return 0;
}

int gxObjRepl::Takeover(const std::string &root, std::string &msg)
{
  Node s;
  std::vector<LinkDef> nolinks; s.links = &nolinks;
  if(!s.open(root, false)) { msg = s.err; s.close(); return 1; }
  if(s.st.role != ROLE_COPY) { msg = root + " is not a copy (role " + std::to_string(s.st.role) + "): takeover refused"; s.close(); return 1; }
  if(s.st.in_snapshot) {
    msg = root + (s.st.in_snapshot == SNAP_INCR ? " is waiting for an incremental rejoin (its data is its old history): refused" :
		  " is in the middle of a full send: refused");
    s.close(); return 1;
  }
  // 7.5 beta 3: the epoch history line (the seq and digest this node takes over at), synced before the state commit
  if(!epoch_line_add(root, s.st.epoch + 1, s.st.applied_seq, s.st.applied_digest)) { msg = "cannot write " + path(root, "epochs"); s.close(); return 1; }
  if(!s.begin()) { msg = s.err; s.close(); return 1; }
  // the change list starts again: it holds this node's own commits since this takeover
  if(!Node::list_trim(s.chg, UINT64_MAX) || !Node::list_trim(s.rjs, UINT64_MAX)) { s.j.Rollback(); msg = "cannot clear the change lists"; s.close(); return 1; }
  s.st.epoch += 1; s.st.next_seq = s.st.applied_seq + 1; s.st.role = ROLE_PRIMARY;
  s.st.cur_digest = s.st.acked_digest = s.st.applied_digest; s.st.acked_seq = s.st.applied_seq; s.put_state();
  int rc = s.commit() ? 0 : 1;
  msg = rc ? s.err : root + " is primary at epoch " + std::to_string(s.st.epoch) + " (applied " + std::to_string(s.st.applied_seq) + ")";
  s.close();
  return rc;
}

int gxObjRepl::Rejoin(const std::string &root, std::string &msg) { return Rejoin(root, msg, true); }
int gxObjRepl::Rejoin(const std::string &root, std::string &msg, bool incremental)
{
  Node s;
  std::vector<LinkDef> nolinks; s.links = &nolinks;
  if(!s.open(root, true)) { msg = s.err; s.close(); return 1; }
  if(s.st.role == ROLE_NONE) { msg = root + " was never a primary or a copy: nothing to rejoin"; s.close(); return 1; }
  bool was_primary = s.st.role == ROLE_PRIMARY;
  uint64_t last = s.st.next_seq - 1, unacked = was_primary && last > s.st.acked_seq ? last - s.st.acked_seq : 0;
  if(!s.begin()) { msg = s.err; s.close(); return 1; }
  // 7.5 beta 3: an incremental rejoin keeps the (database, id) of every op of the commits past the last one its copy
  // acknowledged (all still in the outbox: it is trimmed only by acknowledgements), one block per commit in rjs
  bool fast = was_primary && unacked == 0 && last > 0;
  bool incr = incremental && was_primary && unacked > 0;
  size_t incr_ids = 0;
  if(!Node::list_trim(s.rjs, UINT64_MAX) || !Node::list_trim(s.chg, UINT64_MAX)) { s.j.Rollback(); msg = "cannot clear the change lists"; s.close(); return 1; }
  if(incr) {
    Node::IdSet all;
    for(FAU_t a = s.ob.FindFirstObject(); a != (FAU_t)0 && incr; a = s.ob.FindNextObject(a)) {
      __ULWORD__ n = s.ob.ObjectLength(a);
      std::vector<unsigned char> b(n);
      if(n < 16 || s.ob.Read(b.data(), n, a) != gxDBASE_NO_ERROR) { incr = false; break; }
      Rd r(b.data(), n - 16); uint8_t kind = r.u8(); uint64_t seq = r.u64(); r.u64(); uint32_t nops = r.u32();
      if(!r.ok || kind != F_COMMIT) { incr = false; break; }
      if(seq <= s.st.acked_seq) continue;
      Node::IdSet ids;
      for(uint32_t i = 0; i < nops && r.ok; i++) {
	uint8_t op = r.u8(); std::string db = r.str(); uint64_t id = r.u64();
	if(op == OP_ADD || op == OP_UPDATE) { r.u32(); r.u8(); uint32_t len = r.u32(); r.raw(len); }
	ids.insert({db, id});
      }
      if(!r.ok || !Node::list_add(s.rjs, seq, ids)) { incr = false; break; }
      all.insert(ids.begin(), ids.end());
    }
    if(!incr && !Node::list_trim(s.rjs, UINT64_MAX)) { s.j.Rollback(); msg = "cannot clear " + path(root, "objrepl.rejoin.gxd"); s.close(); return 1; }
    incr_ids = all.size();
  }
  for(FAU_t a = s.ob.FindFirstObject(); a != (FAU_t)0;) {      // the outbox
    FAU_t n = s.ob.FindNextObject(a);
    if(!s.ob.Delete(a)) { s.j.Rollback(); msg = "cannot clear the outbox"; s.close(); return 1; }
    a = n;
  }
  // a former primary whose copy acknowledged every commit (a planned switchover) holds exactly the history the new
  // primary started from: it follows on from its last seq with no full send (the new primary checks the digest at
  // that seq). Otherwise it waits for a full send
  s.st.role = ROLE_COPY; s.st.snapshot_base = 0; s.st.rejoin_dropped = unacked;
  if(fast) { s.st.in_snapshot = 0; s.st.applied_seq = last; s.st.applied_digest = s.st.cur_digest; }
  else { s.st.in_snapshot = incr ? SNAP_INCR : 1; s.st.applied_seq = 0; s.st.applied_digest = DIGEST0; }
  s.put_state();
  int rc = s.commit() ? 0 : 1;
  msg = rc ? s.err : fast ? root + " is a copy at epoch " + std::to_string(s.st.epoch) + ", following on from seq " + std::to_string(last) +
	" (its copy had every commit: no full send)" :
	incr ? root + " is a copy at epoch " + std::to_string(s.st.epoch) + ", waiting for an incremental rejoin: " + std::to_string(unacked) +
	" commit(s) past seq " + std::to_string(s.st.acked_seq) + " (the last its copy acknowledged) changed " + std::to_string(incr_ids) +
	" object(s); the new primary sends those and the objects it changed since it took over, or a full send when it cannot match the "
	"two histories" :
	root + " is a copy at epoch " + std::to_string(s.st.epoch) + ", waiting for a full send" +
	(was_primary ? "; " + std::to_string(unacked) + " commit(s) past seq " + std::to_string(s.st.acked_seq) +
	 " (the last its copy acknowledged) are replaced by what the new primary has" : "");
  s.close();
  return rc;
}

// ---------- gxObjReplReader: read only, from another process (11/19/2026, 7.4.1) ----------
struct gxObjReplReader::Impl {
  std::string root;
  std::vector<IxDef> ixdefs;
  bool is_open = false;
  std::string err;
  State st;
  bool have_state = false;
  struct VIx { IxDef def; gxVectorIndex vx; uint64_t seq = 0, epoch = 0; bool loaded = false; };
  std::map<std::string, std::unique_ptr<VIx>> vix;
  int fail(const std::string &w) { err = w; return -1; }
  uint64_t seq() const { return !have_state ? 0 : st.role == ROLE_PRIMARY ? st.next_seq - 1 : st.applied_seq; }
  bool read_state() {
    gxDatabase db; State s;
    if(db.Open(path(root, "state.gxd").c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) { fail("no state in " + root); return false; }
    bool ok = db.Read(&s, sizeof(s), (FAU_t)db.FileHeaderSize()) == gxDBASE_NO_ERROR && s.magic == STATE_MAGIC;
    db.Close();
    if(!ok) { fail("no state in " + root); return false; }
    st = s; have_state = true;
    return true;
  }
  // the start of every call: a journal left by a node that stopped is replayed (none runs), then the journal's shared
  // lock (held by the caller until it returns: no batch is being written to the files meanwhile) and the state
  int begin(gxdJournalReadLock &rl) {
    err.clear();
    if(!is_open) return fail("not open");
    settle(root);
    std::string jn = path(root, "node.jnl");
    if(exists(jn) && rl.Lock(jn.c_str()) != gxDBASE_NO_ERROR) return fail("cannot lock " + jn + " (gxObjReplReader needs the 64-bit engine)");
    if(!read_state()) return -1;
    if(st.in_snapshot) return fail("a full send is under way");
    return 0;
  }
  const IxDef *ix(const std::string &name, char kind) const {
    for(auto &d : ixdefs) if(d.name == name && d.kind == kind) return &d;
    return 0;
  }
  // the record of ID in DB (db and its id index open): 1 found (rec, h), 0 not there, -1 an error
  int find(gxDatabase &db, gxBtree &bt, uint64_t id, std::vector<unsigned char> &rec, ObjHead &h) {
    IdKey k(id, 0), c;
    for(int ok = bt.PartialFind(k, c); ok; ok = bt.FindNext(k, c)) {
      if(k.d.id != id) break;
      FAU_t a = (FAU_t)k.d.addr;
      __ULWORD__ n = db.ObjectLength(a);
      rec.resize(n);
      if(n < sizeof(ObjHead) || db.Read(rec.data(), n, a) != gxDBASE_NO_ERROR) return fail("cannot read an object");
      memcpy(&h, rec.data(), sizeof(h));
      if(h.id != id || h.len > n - sizeof(h)) return fail("a damaged record");
      if(h.flags & OF_STUB) continue;
      return 1;
    }
    return 0;
  }
  // DB's files, read only: 1 open, 0 no such database, -1 an error
  int open_db(const std::string &name, gxDatabase &db, gxBtree &bt) {
    if(!good_name(name)) return fail("bad database name " + name);
    std::string g = path(root, name + ".gxd"), x = path(root, name + ".id.btx");
    if(!exists(g)) return 0;
    if(db.Open(g.c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) return fail("cannot open " + g);
    if(bt.Open(x.c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) { db.Close(); return fail("cannot open " + x); }
    return 1;
  }
};

gxObjReplReader::gxObjReplReader() : p(new Impl) { }
gxObjReplReader::~gxObjReplReader() { Close(); delete p; }
std::string gxObjReplReader::Error() const { return p->err; }
int gxObjReplReader::IsOpen() const { return p->is_open; }
uint64_t gxObjReplReader::Seq() const { return p->seq(); }
uint64_t gxObjReplReader::Epoch() const { return p->have_state ? p->st.epoch : 0; }
int gxObjReplReader::Role() const { return !p->have_state ? 0 : p->st.role == ROLE_PRIMARY ? gxObjRepl::PRIMARY : p->st.role == ROLE_COPY ? gxObjRepl::COPY : 0; }

// 7.5: the index spec a node wrote beside its index files: DB.NAME.ix.btx.def ("B:OFF:LEN") and DB.NAME.vix.def
// ("V:OFF:DIM"), in name order
static std::string spec_from_defs(const std::string &root)
{
  std::string spec;
  for(auto &f : list_dir(root)) {
    std::string rest; char kind = 0;
    if(ends_with(f, ".ix.btx.def")) { rest = f.substr(0, f.size() - 11); kind = 'B'; }
    else if(ends_with(f, ".vix.def")) { rest = f.substr(0, f.size() - 8); kind = 'V'; }
    else continue;
    size_t dot = rest.find('.');
    if(dot == std::string::npos) continue;
    std::string db = rest.substr(0, dot), name = rest.substr(dot + 1);
    std::string d = read_file(path(root, f));
    while(!d.empty() && (d.back() == '\n' || d.back() == '\r' || d.back() == ' ')) d.pop_back();
    if(d.size() < 2 || d[0] != kind || d[1] != ':') continue;
    std::string entry = name + ":" + db + ":" + d;
    std::vector<IxDef> one;
    if(!parse_ix(entry, one)) continue;
    spec += (spec.empty() ? "" : ",") + entry;
  }
  return spec;
}

int gxObjReplReader::Open(const std::string &root, const std::string &index)
{
  Close();
  p->err.clear();
  // 7.5: no INDEX: the node's own spec, from its .def files
  std::string ix = index.empty() ? spec_from_defs(root) : index;
  if(!parse_ix(ix, p->ixdefs)) return p->fail("bad index (NAME:DB:B|V:OFFSET:LEN,...)");
  if(!exists(path(root, "state.gxd"))) return p->fail("no state in " + root + " (not a node folder)");
  p->root = root; p->is_open = true;
  // a copy in a full send opens (its reads fail until the send ends); a folder with no readable state does not
  if(Refresh() != 0 && !p->have_state) { std::string e = p->err; Close(); p->err = e; return -1; }
  p->err.clear();
  return 0;
}
void gxObjReplReader::Close()
{
  ReleaseVectors();
  p->is_open = false; p->have_state = false; p->ixdefs.clear(); p->root.clear();
}
void gxObjReplReader::ReleaseVectors()
{
  for(auto &v : p->vix) if(v.second->loaded) v.second->vx.Close();
  p->vix.clear();
}
int gxObjReplReader::Refresh()
{
  gxdJournalReadLock rl;
  return p->begin(rl);
}
int gxObjReplReader::Get(const std::string &db, uint64_t id, std::vector<unsigned char> &bytes)
{
  bytes.clear();
  gxdJournalReadLock rl;
  if(p->begin(rl) != 0) return -1;
  gxDatabase d; IdKey type; gxBtree bt(type, ORDER);
  int o = p->open_db(db, d, bt);
  if(o <= 0) return -1;
  std::vector<unsigned char> rec; ObjHead h;
  int f = p->find(d, bt, id, rec, h);
  bt.Close(); d.Close();
  if(f <= 0 || (h.flags & OF_TOMB)) return -1;
  bytes.assign(rec.begin() + sizeof(h), rec.begin() + sizeof(h) + h.len);
  return 0;
}
int gxObjReplReader::IsLive(const std::string &db, uint64_t id)
{
  gxdJournalReadLock rl;
  if(p->begin(rl) != 0) return -1;
  gxDatabase d; IdKey type; gxBtree bt(type, ORDER);
  int o = p->open_db(db, d, bt);
  if(o <= 0) return o;
  std::vector<unsigned char> rec; ObjHead h;
  int f = p->find(d, bt, id, rec, h);
  bt.Close(); d.Close();
  if(f < 0) return -1;
  return f == 1 && !(h.flags & OF_TOMB) ? 1 : 0;
}
// 7.5: a page of ids in id order, from the id index (the lock is held for the page only)
int gxObjReplReader::IdsAfter(const std::string &db, uint64_t after, size_t limit, std::vector<uint64_t> &ids,
			      gxObjRepl::Which which)
{
  ids.clear();
  gxdJournalReadLock rl;
  if(p->begin(rl) != 0) return -1;
  gxDatabase d; IdKey type; gxBtree bt(type, ORDER);
  int o = p->open_db(db, d, bt);
  if(o <= 0) return o;
  IdKey k(after + 1, 0), c;
  uint64_t last = 0;
  for(int ok = after == UINT64_MAX ? 0 : bt.PartialFind(k, c); ok && (!limit || ids.size() < limit); ok = bt.FindNext(k, c)) {
    if(k.d.id <= after || k.d.id == last) continue;
    ObjHead h;
    if(d.Read(&h, sizeof(h), (FAU_t)k.d.addr) != gxDBASE_NO_ERROR || h.id != k.d.id || (h.flags & OF_STUB)) continue;
    bool tomb = (h.flags & OF_TOMB) != 0;
    if(tomb == (which == gxObjRepl::TOMBSTONES)) { ids.push_back(k.d.id); last = k.d.id; }
  }
  bt.Close(); d.Close();
  return 0;
}
// 7.5: ids whose B-tree key K has LO <= K < HI (keys compared as bytes, LO and HI zero padded to the key length;
// an empty HI: no upper bound), in key order, at most LIMIT (0: all)
int gxObjReplReader::FindRange(const std::string &index, const void *lo, unsigned nlo, const void *hi, unsigned nhi,
			       std::vector<uint64_t> &ids, size_t limit)
{
  ids.clear();
  gxdJournalReadLock rl;
  if(p->begin(rl) != 0) return -1;
  const IxDef *d = p->ix(index, 'B');
  if(!d) return p->fail("no B-tree index " + index);
  if(nlo > d->len || nhi > d->len) return p->fail("a bound is longer than the index's key");
  Ix x; x.def = *d;
  std::string f = x.file(p->root);
  if(exists(f + ".building")) return p->fail("the index " + index + " is being built");
  if(!exists(f)) return 0;
  if(x.bt.Open(f.c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) return p->fail("cannot open " + f);
  unsigned char hk[32] = {0};
  if(nhi) memcpy(hk, hi, nhi);
  IxKey k, c;
  if(nlo) memcpy(k.d.k, lo, nlo);
  k.d.id = 0;
  for(int ok = x.bt.PartialFind(k, c); ok && (!limit || ids.size() < limit); ok = x.bt.FindNext(k, c)) {
    if(nhi && memcmp(k.d.k, hk, sizeof(hk)) >= 0) break;
    ids.push_back(k.d.id);
  }
  x.bt.Close();
  return 0;
}
std::string gxObjReplReader::IndexSpec() const
{
  std::string s;
  for(auto &d : p->ixdefs) s += (s.empty() ? "" : ",") + d.name + ":" + d.db + ":" + std::string(1, d.kind) + ":" +
			      std::to_string(d.off) + ":" + std::to_string(d.len);
  return s;
}

int gxObjReplReader::Ids(const std::string &db, std::vector<uint64_t> &ids, gxObjRepl::Which which)
{
  ids.clear();
  gxdJournalReadLock rl;
  if(p->begin(rl) != 0) return -1;
  if(!good_name(db)) return p->fail("bad database name " + db);
  std::string g = path(p->root, db + ".gxd");
  if(!exists(g)) return 0;
  gxDatabase d;
  if(d.Open(g.c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) return p->fail("cannot open " + g);
  for(FAU_t a = d.FindFirstObject(); a != (FAU_t)0; a = d.FindNextObject(a)) {
    ObjHead h;
    if(d.Read(&h, sizeof(h), a) != gxDBASE_NO_ERROR || (h.flags & OF_STUB)) continue;
    bool tomb = (h.flags & OF_TOMB) != 0;
    if(tomb == (which == gxObjRepl::TOMBSTONES)) ids.push_back(h.id);
  }
  d.Close();
  std::sort(ids.begin(), ids.end());
  return 0;
}
int gxObjReplReader::Databases(std::vector<std::string> &dbs)
{
  dbs.clear();
  gxdJournalReadLock rl;
  if(p->begin(rl) != 0) return -1;
  for(auto &n : list_dir(p->root))
    if(ends_with(n, ".gxd")) { std::string nm = n.substr(0, n.size() - 4); if(good_name(nm)) dbs.push_back(nm); }
  return 0;
}
int gxObjReplReader::FindKey(const std::string &index, const void *key, unsigned n, std::vector<uint64_t> &ids)
{
  ids.clear();
  gxdJournalReadLock rl;
  if(p->begin(rl) != 0) return -1;
  const IxDef *d = p->ix(index, 'B');
  if(!d) return p->fail("no B-tree index " + index);
  if(n > d->len) return p->fail("the key is longer than the index's");
  Ix x; x.def = *d;
  std::string f = x.file(p->root);
  if(exists(f + ".building")) return p->fail("the index " + index + " is being built");
  if(!exists(f)) return 0;                                     // no objects yet
  if(x.bt.Open(f.c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) return p->fail("cannot open " + f);
  IxKey k, c;
  memcpy(k.d.k, key, n); k.d.id = 0;
  for(int ok = x.bt.PartialFind(k, c); ok; ok = x.bt.FindNext(k, c)) {
    int cmp = memcmp(k.d.k, key, n);
    if(cmp > 0) break;
    if(cmp == 0) ids.push_back(k.d.id);
  }
  x.bt.Close();
  return 0;
}
int gxObjReplReader::Nearest(const std::string &index, const float *q, unsigned k, std::vector<gxObjReplHit> &hits)
{
  hits.clear();
  gxdJournalReadLock rl;
  if(p->begin(rl) != 0) return -1;
  const IxDef *d = p->ix(index, 'V');
  if(!d) return p->fail("no vector index " + index);
  Ix x; x.def = *d;
  std::string f = x.file(p->root);
  if(exists(f + ".building")) return p->fail("the index " + index + " is being built");
  auto &v = p->vix[index];
  if(!v) { v.reset(new Impl::VIx); v->def = *d; }
  uint64_t now = p->seq();
  // the node moved on: opened again by name (a full send replaces the file, so Reload() of the old one would not do)
  if(v->loaded && (v->seq != now || v->epoch != p->st.epoch)) { v->vx.Close(); v->loaded = false; }
  if(!v->loaded) {
    if(!exists(f)) return 0;                                   // no objects yet
    if(v->vx.Open(f.c_str(), gxDBASE_READONLY, x.spec().c_str()) != gxv_NO_ERROR) return p->fail("cannot open " + f);
    v->loaded = true; v->seq = now; v->epoch = p->st.epoch;
  }
  std::vector<gxvResult> r;
  if(v->vx.Search(q, k, r) != gxv_NO_ERROR) return p->fail("the search failed");
  for(auto &e : r) hits.push_back({(uint64_t)e.id, e.distance});
  return 0;
}

} // namespace datareel

#ifdef __BCC32__
#pragma warn .8080
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
