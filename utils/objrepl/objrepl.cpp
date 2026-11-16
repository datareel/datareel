// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: objrepl.cpp
// Compiler Used: GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/16/2026
// Date Last Modified: 11/16/2026
// Copyright (c) 2001-2026 DataReel Software Development
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

PROTOTYPE (11/16/2026): object replication. Only objects travel over
the wire, never file pages. The primary and the copy are independent
databases: the copy's file addresses and indexes are its own.

Primary (one process: the writer and its sender):
  - DIR/data.gxd   the objects: { id (u64), version (u32), length (u32),
                   bytes }. An object is found by its logical id.
  - DIR/outbox.gxd the object cache: one object per commit, holding the
                   commit's changes (add / update / remove, id, bytes),
                   numbered by a commit sequence (seq) and the primary's
                   epoch. Its static area holds next_seq, acked_seq and
                   the epoch.
  - DIR/writer.jnl the journal: each commit changes data.gxd, by_id.btx
                   and outbox.gxd in ONE transaction, so a commit and its
                   outbox entry are durable together or not at all.
  The sender sends every outbox entry past the copy's applied seq, in
  order, and trims the outbox up to the seq the copy acknowledges.

Copy:
  - DIR/data.gxd   its own objects; static area: applied_seq, epoch.
  - DIR/by_id.btx  its own index of (id, its own address).
  - DIR/copy.jnl   each received commit is applied, with the new
                   applied_seq, in ONE transaction, then acknowledged.
  A copy refuses a primary whose epoch is older than its own.

Usage:
  objrepl primary DIR --copy HOST:PORT --ca CA --cert CERT --key KEY
                  [--commits N] [--per N] [--sleep MS] [--seed N] [--drain]
  objrepl copy DIR --listen PORT --ca CA --cert CERT --key KEY --allow-cn CN
  The wire is gxStream (gxstream.h): TLS, mutual certificates, one block per
  commit.
  objrepl digest DIR          ids and content hashes, address free
  objrepl takeover DIR        a copy becomes primary: epoch + 1
  objrepl status DIR
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <map>
#include <string>
#include <vector>
#include <algorithm>
#include "gxdbase.h"
#include "gxbtree.h"
#include "gxdjrnl.h"
#include "gxstream.h"

using namespace datareel;

// ---------- records ----------
struct ObjHead { uint64_t id; uint32_t version; uint32_t len; };
const uint32_t STATE_MAGIC = 0x3150524fu;   // "ORP1"
struct State { uint32_t magic; uint32_t pad; uint64_t next_seq; uint64_t acked_seq; uint64_t epoch; uint64_t applied_seq; };
enum { OP_ADD = 1, OP_UPDATE = 2, OP_REMOVE = 3 };

struct IdKeyData { uint64_t id; FAU addr; };
class IdKey : public DatabaseKeyB {
public:
  IdKey() : DatabaseKeyB((char *)&d) { memset(&d, 0, sizeof(d)); }
  IdKey(uint64_t id, FAU_t a) : DatabaseKeyB((char *)&d) { d.id = id; d.addr = (FAU)a; }
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
const BtreeNodeOrder_t ORDER = 16;

static uint64_t fnv(const void *p, size_t n, uint64_t h = 14695981039346656037ULL)
{
  const unsigned char *b = (const unsigned char *)p;
  for(size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ULL; }
  return h;
}

// ---------- byte buffers ----------
struct Buf {
  std::vector<unsigned char> b;
  void u8(uint8_t v) { b.push_back(v); }
  void u32(uint32_t v) { for(int i = 0; i < 4; i++) b.push_back((unsigned char)(v >> (8 * i))); }
  void u64(uint64_t v) { for(int i = 0; i < 8; i++) b.push_back((unsigned char)(v >> (8 * i))); }
  void raw(const void *p, size_t n) { const unsigned char *c = (const unsigned char *)p; b.insert(b.end(), c, c + n); }
};
struct Rd {
  const unsigned char *p; size_t n, o = 0; bool ok = true;
  Rd(const unsigned char *p_, size_t n_) : p(p_), n(n_) {}
  bool need(size_t k) { if(o + k > n) ok = false; return ok; }
  uint8_t u8() { if(!need(1)) return 0; return p[o++]; }
  uint32_t u32() { if(!need(4)) return 0; uint32_t v = 0; for(int i = 0; i < 4; i++) v |= (uint32_t)p[o + i] << (8 * i); o += 4; return v; }
  uint64_t u64() { if(!need(8)) return 0; uint64_t v = 0; for(int i = 0; i < 8; i++) v |= (uint64_t)p[o + i] << (8 * i); o += 8; return v; }
  const unsigned char *raw(size_t k) { if(!need(k)) return 0; const unsigned char *r = p + o; o += k; return r; }
};

// ---------- a store: data.gxd + by_id.btx (+ outbox.gxd on a primary) in one journal ----------
struct Store {
  gxdJournal j;
  gxDatabase db, ob;
  IdKey type;
  gxBtree bt{type, ORDER};
  bool primary = false;
  std::map<uint64_t, FAU_t> where;        // id -> address, this database's own (rebuilt at open)
  State st;
  std::string dir;

  bool open(const std::string &d, bool prim, const char *jname) {
    dir = d; primary = prim;
    if(j.Open((dir + "/" + jname).c_str()) != gxDBASE_NO_ERROR) { fprintf(stderr, "objrepl: cannot open the journal\n"); return false; }
    if(db.Open((dir + "/data.gxd").c_str()) != gxDBASE_NO_ERROR && db.Create((dir + "/data.gxd").c_str(), (FAU_t)sizeof(State)) != gxDBASE_NO_ERROR) return false;
    if(bt.Open((dir + "/by_id.btx").c_str()) != gxDBASE_NO_ERROR && bt.Create((dir + "/by_id.btx").c_str()) != gxDBASE_NO_ERROR) return false;
    if(primary && ob.Open((dir + "/outbox.gxd").c_str()) != gxDBASE_NO_ERROR && ob.Create((dir + "/outbox.gxd").c_str()) != gxDBASE_NO_ERROR) return false;
    if(j.Attach(&db) != gxDBASE_NO_ERROR || j.Attach(&bt) != gxDBASE_NO_ERROR) return false;
    if(primary && j.Attach(&ob) != gxDBASE_NO_ERROR) return false;
    if(db.Read(&st, sizeof(st), (FAU_t)db.FileHeaderSize()) != gxDBASE_NO_ERROR || st.magic != STATE_MAGIC) {
      memset(&st, 0, sizeof(st)); st.magic = STATE_MAGIC; st.next_seq = 1; st.epoch = 1;
      j.Begin(); db.Write(&st, sizeof(st), (FAU_t)db.FileHeaderSize()); if(j.Commit() != gxDBASE_NO_ERROR) return false;
    }
    for(FAU_t a = db.FindFirstObject(); a != (FAU_t)0; a = db.FindNextObject(a)) {
      ObjHead h;
      if(db.Read(&h, sizeof(h), a) == gxDBASE_NO_ERROR) where[h.id] = a;
    }
    return true;
  }
  void close() { j.Close(); if(primary) ob.Close(); bt.Close(); db.Close(); }
  // the state lives in data.gxd's static area, which starts right after the file header (StaticArea() is its size)
  bool put_state() { return db.Write(&st, sizeof(st), (FAU_t)db.FileHeaderSize()) == gxDBASE_NO_ERROR; }

  // object changes, inside an open transaction
  bool add(uint64_t id, uint32_t ver, const unsigned char *p, uint32_t n) {
    if(where.count(id) && !remove(id)) return false;
    std::vector<unsigned char> rec(sizeof(ObjHead) + n);
    ObjHead h{id, ver, n};
    memcpy(rec.data(), &h, sizeof(h)); if(n) memcpy(rec.data() + sizeof(h), p, n);
    FAU_t a = db.Alloc((__ULWORD__)rec.size());
    if(!a || db.Write(rec.data(), (__ULWORD__)rec.size(), a) != gxDBASE_NO_ERROR) return false;
    IdKey k(id, a), c;
    if(!bt.Insert(k, c)) return false;
    where[id] = a;
    return true;
  }
  bool remove(uint64_t id) {
    auto it = where.find(id);
    if(it == where.end()) return true;              // already gone: removes are idempotent
    IdKey k(id, it->second), c;
    bt.Delete(k, c);
    if(!db.Delete(it->second)) return false;
    where.erase(it);
    return true;
  }
};

// ---------- the wire ----------
static volatile sig_atomic_t stopping = 0;
// 11/16/2026: every frame is one gxStream block (gxstream.h): TLS with mutual certificates, block lengths checked
// against the stream's maximum before any memory is allocated. Raw sockets are not used.
struct Tls { std::string ca, cert, key, allow_cn; };
static Tls tls;

static bool send_frame(gxStream &st, const std::vector<unsigned char> &b)
{
  return st.WriteBlock(b.data(), (int)b.size()) == 0;
}
static bool recv_frame(gxStream &st, std::vector<unsigned char> &b)
{
  gxBlockHeader gx;
  if(st.ReadHeader(gx) != 0) return false;
  return st.ReadBlock(b, gx) == 0;
}
static bool connect_to(gxStream &st, const std::string &hostport)
{
  size_t c = hostport.rfind(':');
  st.Connection().SetCAFile(tls.ca);
  st.SetClientCert(tls.cert, tls.key);
  st.SetTimeOut(10);
  return st.Connect(hostport.substr(0, c), atoi(hostport.substr(c + 1).c_str())) == 0;
}

// ---------- primary ----------
static unsigned rng = 2026;
static unsigned rnd() { rng = rng * 1103515245u + 12345u; return rng >> 8; }

// the outbox entries past "after", in seq order: (seq, address)
static std::vector<std::pair<uint64_t, FAU_t>> outbox_after(Store &s, uint64_t after)
{
  std::vector<std::pair<uint64_t, FAU_t>> v;
  for(FAU_t a = s.ob.FindFirstObject(); a != (FAU_t)0; a = s.ob.FindNextObject(a)) {
    unsigned char h[8];
    if(s.ob.Read(h, 8, a) != gxDBASE_NO_ERROR) continue;
    Rd r(h, 8); uint64_t seq = r.u64();
    if(seq > after) v.push_back({seq, a});
  }
  std::sort(v.begin(), v.end());
  return v;
}

struct Link { gxStream *st = 0; uint64_t copy_applied = 0; bool refused = false;
  void drop() { if(st) { st->Close(); delete st; st = 0; } } };

static bool hello(Store &s, Link &L, const std::string &copy)
{
  if(L.st) return true;
  L.st = new gxStream;
  if(!connect_to(*L.st, copy)) { L.drop(); return false; }
  Buf h; h.u32(STATE_MAGIC); h.u64(s.st.epoch);
  std::vector<unsigned char> rep;
  if(!send_frame(*L.st, h.b) || !recv_frame(*L.st, rep)) { L.drop(); return false; }
  Rd r(rep.data(), rep.size());
  uint8_t ok = r.u8(); uint64_t applied = r.u64(), cep = r.u64();
  if(!r.ok || !ok) {
    fprintf(stderr, "objrepl primary: the copy refused epoch %llu (its epoch %llu): it has taken over\n",
	    (unsigned long long)s.st.epoch, (unsigned long long)cep);
    L.drop(); L.refused = true; return false;
  }
  L.copy_applied = applied;
  return true;
}

// send everything past the copy's applied seq; trim the outbox up to what it acknowledges
static bool pump(Store &s, Link &L, const std::string &copy)
{
  if(!hello(s, L, copy)) return false;
  auto v = outbox_after(s, L.copy_applied);
  for(auto &e : v) {
    __ULWORD__ n = s.ob.ObjectLength(e.second);
    std::vector<unsigned char> b(n);
    if(s.ob.Read(b.data(), n, e.second) != gxDBASE_NO_ERROR) return false;
    std::vector<unsigned char> ack;
    if(!send_frame(*L.st, b) || !recv_frame(*L.st, ack) || ack.size() < 8) { L.drop(); return false; }
    Rd r(ack.data(), ack.size()); L.copy_applied = r.u64();
  }
  // trim: one transaction, the outbox entries at or below the copy's applied seq and acked_seq
  if(L.copy_applied > s.st.acked_seq) {
    s.j.Begin();
    for(FAU_t a = s.ob.FindFirstObject(); a != (FAU_t)0; a = s.ob.FindNextObject(a)) {
      unsigned char h[8];
      if(s.ob.Read(h, 8, a) != gxDBASE_NO_ERROR) continue;
      Rd r(h, 8);
      if(r.u64() <= L.copy_applied) s.ob.Delete(a);
    }
    s.st.acked_seq = L.copy_applied;
    s.put_state();
    if(s.j.Commit() != gxDBASE_NO_ERROR) return false;
  }
  return true;
}

static int run_primary(const std::string &dir, const std::string &copy, long commits, long per, long sleep_ms, bool drain)
{
  Store s;
  if(!s.open(dir, true, "writer.jnl")) return 1;
  Link L;
  uint64_t next_id = 1;
  for(auto &w : s.where) next_id = std::max(next_id, w.first + 1);
  for(long c = 0; !drain && c < commits; c++) {
    // one commit: the objects and their outbox entry in one transaction
    Buf f; uint64_t seq = s.st.next_seq;
    f.u64(seq); f.u64(s.st.epoch);
    std::vector<std::pair<uint8_t, uint64_t>> ops;
    std::vector<std::vector<unsigned char>> datas;
    s.j.Begin();
    for(long k = 0; k < per; k++) {
      unsigned r = rnd() % 10;
      std::vector<uint64_t> ids;
      if(r >= 6 && !s.where.empty()) { auto it = s.where.begin(); std::advance(it, rnd() % s.where.size()); ids.push_back(it->first); }
      if(ids.empty() || r < 6) {                               // add
	uint32_t n = 40 + rnd() % 400; std::vector<unsigned char> d(n);
	for(auto &x : d) x = (unsigned char)rnd();
	uint64_t id = next_id++;
	if(!s.add(id, 1, d.data(), n)) { fprintf(stderr, "add failed\n"); return 1; }
	ops.push_back({OP_ADD, id}); datas.push_back(d);
      } else if(r < 9) {                                       // update (new bytes, new size)
	uint64_t id = ids[0]; uint32_t n = 40 + rnd() % 400; std::vector<unsigned char> d(n);
	for(auto &x : d) x = (unsigned char)rnd();
	if(!s.add(id, 2, d.data(), n)) { fprintf(stderr, "update failed\n"); return 1; }
	ops.push_back({OP_UPDATE, id}); datas.push_back(d);
      } else {                                                 // remove
	uint64_t id = ids[0];
	if(!s.remove(id)) { fprintf(stderr, "remove failed\n"); return 1; }
	ops.push_back({OP_REMOVE, id}); datas.push_back({});
      }
    }
    f.u32((uint32_t)ops.size());
    for(size_t i = 0; i < ops.size(); i++) {
      f.u8(ops[i].first); f.u64(ops[i].second); f.u32((uint32_t)datas[i].size()); f.raw(datas[i].data(), datas[i].size());
    }
    FAU_t oa = s.ob.Alloc((__ULWORD__)f.b.size());
    if(!oa || s.ob.Write(f.b.data(), (__ULWORD__)f.b.size(), oa) != gxDBASE_NO_ERROR) { fprintf(stderr, "outbox write failed\n"); return 1; }
    s.st.next_seq = seq + 1;
    s.put_state();
    if(s.j.Commit() != gxDBASE_NO_ERROR) { fprintf(stderr, "commit failed\n"); return 1; }
    pump(s, L, copy);                                          // best effort: a copy that is down catches up later
    if(L.refused) { s.close(); return 3; }
    if(sleep_ms) usleep((useconds_t)sleep_ms * 1000);
  }
  // drain: until the copy has every commit
  for(int tries = 0; tries < 600; tries++) {
    if(pump(s, L, copy) && L.copy_applied + 1 >= s.st.next_seq) break;
    if(L.refused) { s.close(); return 3; }
    usleep(200000);
  }
  bool done = L.copy_applied + 1 >= s.st.next_seq;
  printf("objrepl primary: epoch %llu, last seq %llu, copy applied %llu, outbox %s\n", (unsigned long long)s.st.epoch,
	 (unsigned long long)(s.st.next_seq - 1), (unsigned long long)L.copy_applied, done ? "drained" : "NOT drained");
  L.drop();
  s.close();
  return done ? 0 : 2;
}

// ---------- copy ----------
static void on_term(int) { stopping = 1; }

static int run_copy(const std::string &dir, int port)
{
  Store s;
  if(!s.open(dir, false, "copy.jnl")) return 1;
  // no SA_RESTART: a TERM interrupts accept() and recv(), so the copy stops between commits
  struct sigaction sa_t; memset(&sa_t, 0, sizeof(sa_t)); sa_t.sa_handler = on_term;
  sigaction(SIGTERM, &sa_t, 0); sigaction(SIGINT, &sa_t, 0); signal(SIGPIPE, SIG_IGN);
  gxStreamServer srv;
  srv.SetClientCAFile(tls.ca);
  if(srv.AllowClientCN(tls.allow_cn) != 0) { fprintf(stderr, "objrepl copy: --allow-cn is required\n"); return 1; }
  if(srv.Listen(port, tls.cert, tls.key, std::string(), "127.0.0.1") != 0) { fprintf(stderr, "objrepl copy: cannot listen on %d\n", port); return 1; }
  printf("objrepl copy: listening on 127.0.0.1:%d (gxStream, TLS, client CN %s), applied %llu, epoch %llu\n", port, tls.allow_cn.c_str(),
	 (unsigned long long)s.st.applied_seq, (unsigned long long)s.st.epoch);
  fflush(stdout);
  while(!stopping) {
    gxStream st;
    st.SetTimeOut(3600);
    if(srv.Accept(st) != 0) { if(stopping) break; continue; }
    std::vector<unsigned char> b;
    if(!recv_frame(st, b)) { st.Close(); continue; }
    Rd h(b.data(), b.size()); uint32_t magic = h.u32(); uint64_t pep = h.u64();
    Buf rep;
    bool ok = h.ok && magic == STATE_MAGIC && pep >= s.st.epoch;
    rep.u8(ok ? 1 : 0); rep.u64(s.st.applied_seq); rep.u64(s.st.epoch);
    send_frame(st, rep.b);
    if(!ok) { st.Close(); continue; }
    if(pep > s.st.epoch) { s.j.Begin(); s.st.epoch = pep; s.put_state(); s.j.Commit(); }
    while(!stopping && recv_frame(st, b)) {
      Rd r(b.data(), b.size());
      uint64_t seq = r.u64(), ep = r.u64(); uint32_t nops = r.u32();
      if(!r.ok || ep < s.st.epoch) break;
      if(seq <= s.st.applied_seq) { Buf a; a.u64(s.st.applied_seq); send_frame(st, a.b); continue; }   // a resend
      if(seq != s.st.applied_seq + 1) { fprintf(stderr, "objrepl copy: seq %llu after %llu: a gap\n", (unsigned long long)seq, (unsigned long long)s.st.applied_seq); break; }
      s.j.Begin();
      bool good = true;
      for(uint32_t i = 0; i < nops && good; i++) {
	uint8_t op = r.u8(); uint64_t id = r.u64(); uint32_t n = r.u32(); const unsigned char *p = r.raw(n);
	if(!r.ok) { good = false; break; }
	good = op == OP_REMOVE ? s.remove(id) : s.add(id, op == OP_ADD ? 1 : 2, p, n);
      }
      if(!good) { s.j.Rollback(); fprintf(stderr, "objrepl copy: commit %llu did not apply\n", (unsigned long long)seq); break; }
      s.st.applied_seq = seq;
      s.put_state();
      if(s.j.Commit() != gxDBASE_NO_ERROR) { fprintf(stderr, "objrepl copy: commit failed\n"); break; }
      Buf a; a.u64(seq);
      if(!send_frame(st, a.b)) break;
    }
    st.Close();
  }
  s.close();
  return 0;
}

// ---------- digest / status / takeover ----------
static int run_digest(const std::string &dir)
{
  gxDatabase db;
  if(db.Open((dir + "/data.gxd").c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) { fprintf(stderr, "cannot open %s/data.gxd\n", dir.c_str()); return 1; }
  std::map<uint64_t, uint64_t> h;
  for(FAU_t a = db.FindFirstObject(); a != (FAU_t)0; a = db.FindNextObject(a)) {
    __ULWORD__ n = db.ObjectLength(a);
    std::vector<unsigned char> b(n);
    if(db.Read(b.data(), n, a) != gxDBASE_NO_ERROR || n < sizeof(ObjHead)) continue;
    ObjHead oh; memcpy(&oh, b.data(), sizeof(oh));
    if(h.count(oh.id)) { printf("DUPLICATE id %llu\n", (unsigned long long)oh.id); return 1; }
    h[oh.id] = fnv(b.data() + sizeof(oh), oh.len, fnv(&oh.len, 4));
  }
  uint64_t all = 14695981039346656037ULL;
  for(auto &e : h) { all = fnv(&e.first, 8, all); all = fnv(&e.second, 8, all); }
  printf("objects %zu digest %016llx\n", h.size(), (unsigned long long)all);
  db.Close();
  return 0;
}

static int run_status(const std::string &dir)
{
  gxDatabase db; State st;
  if(db.Open((dir + "/data.gxd").c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) return 1;
  if(db.Read(&st, sizeof(st), (FAU_t)db.FileHeaderSize()) != gxDBASE_NO_ERROR || st.magic != STATE_MAGIC) { printf("no state\n"); return 1; }
  printf("epoch %llu next_seq %llu acked_seq %llu applied_seq %llu\n", (unsigned long long)st.epoch, (unsigned long long)st.next_seq,
	 (unsigned long long)st.acked_seq, (unsigned long long)st.applied_seq);
  db.Close();
  return 0;
}

static int run_takeover(const std::string &dir)
{
  Store s;
  if(!s.open(dir, false, "copy.jnl")) return 1;
  s.j.Begin(); s.st.epoch += 1; s.st.next_seq = s.st.applied_seq + 1; s.put_state();
  int rc = s.j.Commit() == gxDBASE_NO_ERROR ? 0 : 1;
  printf("objrepl takeover: %s is primary at epoch %llu (applied %llu)\n", dir.c_str(), (unsigned long long)s.st.epoch,
	 (unsigned long long)s.st.applied_seq);
  s.close();
  return rc;
}

int main(int argc, char **argv)
{
  if(argc < 3) { fprintf(stderr, "usage: objrepl primary|copy|digest|status|takeover DIR [options]\n"); return 64; }
  std::string cmd = argv[1], dir = argv[2], copy;
  long commits = 100, per = 10, sleep_ms = 0, port = 0; bool drain = false;
  for(int i = 3; i < argc; i++) {
    std::string a = argv[i];
    auto val = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
    if(a == "--copy") copy = val();
    else if(a == "--commits") commits = atol(val().c_str());
    else if(a == "--per") per = atol(val().c_str());
    else if(a == "--sleep") sleep_ms = atol(val().c_str());
    else if(a == "--seed") rng = (unsigned)atol(val().c_str());
    else if(a == "--listen") port = atol(val().c_str());
    else if(a == "--drain") drain = true;
    else if(a == "--ca") tls.ca = val();
    else if(a == "--cert") tls.cert = val();
    else if(a == "--key") tls.key = val();
    else if(a == "--allow-cn") tls.allow_cn = val();
    else { fprintf(stderr, "unknown option %s\n", a.c_str()); return 64; }
  }
  if(cmd == "primary") return copy.empty() ? 64 : run_primary(dir, copy, commits, per, sleep_ms, drain);
  if(cmd == "copy") return port ? run_copy(dir, (int)port) : 64;
  if(cmd == "digest") return run_digest(dir);
  if(cmd == "status") return run_status(dir);
  if(cmd == "takeover") return run_takeover(dir);
  return 64;
}
