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
the wire (gxStream blocks), never file pages. The primary and the copy
are independent databases: the copy's file addresses and indexes are
its own.

A node (primary or copy) is a folder ROOT:
  ROOT/<name>.gxd       a database of objects { id (u64), version (u32),
                        length (u32), bytes }, found by logical id
  ROOT/<name>.id.btx    that database's index of (id, its own address)
  ROOT/state.gxd        the node's state in its static area: epoch,
                        next_seq (primary), applied_seq (copy), a
                        snapshot in progress (copy)
  ROOT/outbox.gxd       primary only: one object per commit, holding
                        the commit's changes (database name, op, id,
                        bytes), numbered by commit sequence and epoch
  ROOT/node.jnl         one gxdJournal over every file above: a commit
                        (with its outbox entry on a primary, its
                        applied_seq on a copy) is durable as a whole or
                        not at all
A commit may change several databases; a copy opens or creates them as
they appear.

The primary streams the outbox in order from the copy's applied_seq and
trims it as the copy acknowledges. A copy whose next commit is no
longer in the outbox (a new copy, or one that was away while the outbox
was trimmed), or that stopped in the middle of a snapshot, gets a
snapshot: every object of every database as of a known commit, then the
outbox from there. A copy refuses a primary whose epoch is older than
its own.

Usage:
  objrepl primary ROOT --copy HOST:PORT --ca CA --cert CERT --key KEY
                  [--commits N] [--per N] [--dbs N] [--sleep MS]
                  [--seed N] [--drain] [--no-trim]
  objrepl copy ROOT --listen PORT --ca CA --cert CERT --key KEY
                  --allow-cn CN
  objrepl digest ROOT       every database: ids and content hashes
  objrepl status ROOT
  objrepl takeover ROOT     a copy becomes primary: epoch + 1
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
#include <dirent.h>
#include <time.h>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <tuple>
#include "gxdbase.h"
#include "gxbtree.h"
#include "gxdjrnl.h"
#include "gxstream.h"

using namespace datareel;

// ---------- records ----------
struct ObjHead { uint64_t id; uint32_t version; uint32_t len; };
const uint32_t STATE_MAGIC = 0x3350524fu;   // "ORP3"
enum { ROLE_NONE = 0, ROLE_PRIMARY = 1, ROLE_COPY = 2 };
// The running digest: a hash chained over every commit frame in order (the primary when it records a commit, the
// copy when it applies it), so two nodes at the same seq with different histories never look alike.
const uint64_t DIGEST0 = 14695981039346656037ULL;
struct State {
  uint32_t magic; uint32_t in_snapshot;
  uint64_t next_seq; uint64_t acked_seq; uint64_t epoch; uint64_t applied_seq; uint64_t snapshot_base;
  uint32_t role; uint32_t pad;
  uint64_t cur_digest;      // primary: after next_seq - 1
  uint64_t acked_digest;    // primary: after acked_seq
  uint64_t applied_digest;  // copy: after applied_seq
};
enum { OP_ADD = 1, OP_UPDATE = 2, OP_REMOVE = 3 };
const uint64_t DANGLING_ID = ~0ULL;     // a link to an object that is gone, on the wire
const uint64_t DANGLING_ADDR = 1;       // and in a database: inside the file header, never an object
enum { F_COMMIT = 1, F_SNAP_BEGIN = 2, F_SNAP_BATCH = 3, F_SNAP_END = 4 };
const size_t SNAP_BATCH_BYTES = 1u << 20;

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
  void u16(uint16_t v) { b.push_back((unsigned char)v); b.push_back((unsigned char)(v >> 8)); }
  void u32(uint32_t v) { for(int i = 0; i < 4; i++) b.push_back((unsigned char)(v >> (8 * i))); }
  void u64(uint64_t v) { for(int i = 0; i < 8; i++) b.push_back((unsigned char)(v >> (8 * i))); }
  void raw(const void *p, size_t n) { const unsigned char *c = (const unsigned char *)p; b.insert(b.end(), c, c + n); }
  void str(const std::string &s) { u16((uint16_t)s.size()); raw(s.data(), s.size()); }
};
struct Rd {
  const unsigned char *p; size_t n, o = 0; bool ok = true;
  Rd(const unsigned char *p_, size_t n_) : p(p_), n(n_) {}
  bool need(size_t k) { if(o + k > n) ok = false; return ok; }
  uint8_t u8() { if(!need(1)) return 0; return p[o++]; }
  uint16_t u16() { if(!need(2)) return 0; uint16_t v = (uint16_t)(p[o] | (p[o + 1] << 8)); o += 2; return v; }
  uint32_t u32() { if(!need(4)) return 0; uint32_t v = 0; for(int i = 0; i < 4; i++) v |= (uint32_t)p[o + i] << (8 * i); o += 4; return v; }
  uint64_t u64() { if(!need(8)) return 0; uint64_t v = 0; for(int i = 0; i < 8; i++) v |= (uint64_t)p[o + i] << (8 * i); o += 8; return v; }
  const unsigned char *raw(size_t k) { if(!need(k)) return 0; const unsigned char *r = p + o; o += k; return r; }
  std::string str() { uint16_t k = u16(); const unsigned char *r = raw(k); return r ? std::string((const char *)r, k) : std::string(); }
};

// a database name: letters, digits, '_' and '-' only (it becomes a file name)
static bool good_name(const std::string &s)
{
  if(s.empty() || s.size() > 64) return false;
  for(char c : s) if(!(isalnum((unsigned char)c) || c == '_' || c == '-')) return false;
  return s != "state" && s != "outbox";
}

// ---------- link map ----------
// 11/16/2026: an object may hold the file address of another object (a link field). Addresses are each database's
// own, so a link travels as the target's object id: the primary turns the address into the id when it records the
// commit, and the copy turns the id into its own address when it applies it. The map says where the link fields
// are: "DB:OFFSET:TARGET_DB" entries (OFFSET into the object's bytes, an 8-byte address; 0 = no link). Both sides
// use the primary's map, which it sends when it connects.
struct LinkDef { std::string db; uint32_t off; std::string target; };
static std::vector<LinkDef> links;
static bool parse_links(const std::string &spec)
{
  links.clear();
  size_t i = 0;
  while(i < spec.size()) {
    size_t j = spec.find(',', i); if(j == std::string::npos) j = spec.size();
    std::string e = spec.substr(i, j - i);
    size_t a = e.find(':'), b = e.find(':', a == std::string::npos ? 0 : a + 1);
    if(a == std::string::npos || b == std::string::npos) return false;
    LinkDef d{e.substr(0, a), (uint32_t)atol(e.substr(a + 1, b - a - 1).c_str()), e.substr(b + 1)};
    links.push_back(d);
    i = j + 1;
  }
  return true;
}
static std::string links_spec()
{
  std::string s;
  for(auto &d : links) s += (s.empty() ? "" : ",") + d.db + ":" + std::to_string(d.off) + ":" + d.target;
  return s;
}

// ---------- one database: NAME.gxd + NAME.id.btx ----------
struct Db {
  std::string name;
  gxDatabase db;
  IdKey type;
  gxBtree bt{type, ORDER};
  std::map<uint64_t, FAU_t> where;        // id -> address, this database's own (rebuilt at open)
  std::map<FAU_t, uint64_t> who;          // address -> id

  bool open(const std::string &root) {
    std::string g = root + "/" + name + ".gxd", x = root + "/" + name + ".id.btx";
    if(db.Open(g.c_str()) != gxDBASE_NO_ERROR && db.Create(g.c_str()) != gxDBASE_NO_ERROR) return false;
    if(bt.Open(x.c_str()) != gxDBASE_NO_ERROR && bt.Create(x.c_str()) != gxDBASE_NO_ERROR) return false;
    for(FAU_t a = db.FindFirstObject(); a != (FAU_t)0; a = db.FindNextObject(a)) {
      ObjHead h;
      if(db.Read(&h, sizeof(h), a) == gxDBASE_NO_ERROR) { where[h.id] = a; who[a] = h.id; }
    }
    return true;
  }
  void close() { bt.Close(); db.Close(); }
  bool put(uint64_t id, uint32_t ver, const unsigned char *p, uint32_t n) {
    std::vector<unsigned char> rec(sizeof(ObjHead) + n);
    ObjHead h; memset(&h, 0, sizeof(h)); h.id = id; h.version = ver; h.len = n;
    memcpy(rec.data(), &h, sizeof(h)); if(n) memcpy(rec.data() + sizeof(h), p, n);
    auto old = where.find(id);
    if(old != where.end()) {                        // the same size: in place, so links to it stay right
      if(db.ObjectLength(old->second) == (__ULWORD__)rec.size())
	return db.Write(rec.data(), (__ULWORD__)rec.size(), old->second) == gxDBASE_NO_ERROR;
      if(!remove(id)) return false;
    }
    FAU_t a = db.Alloc((__ULWORD__)rec.size());
    if(!a || db.Write(rec.data(), (__ULWORD__)rec.size(), a) != gxDBASE_NO_ERROR) return false;
    IdKey k(id, a), c;
    if(!bt.Insert(k, c)) return false;
    where[id] = a; who[a] = id;
    return true;
  }
  bool remove(uint64_t id) {
    auto it = where.find(id);
    if(it == where.end()) return true;              // already gone: removes are idempotent
    IdKey k(id, it->second), c;
    bt.Delete(k, c);
    if(!db.Delete(it->second)) return false;
    who.erase(it->second);
    where.erase(it);
    return true;
  }
  bool clear() {                                    // every object and key (a snapshot starts from empty)
    std::vector<uint64_t> ids;
    for(auto &w : where) ids.push_back(w.first);
    for(auto id : ids) if(!remove(id)) return false;
    return true;
  }
};

// ---------- a node: its databases, state and (primary) outbox under one journal ----------
struct Node {
  gxdJournal j;
  gxDatabase sdb, ob;
  bool primary = false;
  std::map<std::string, std::unique_ptr<Db>> dbs;
  State st;
  std::string root;

  bool open(const std::string &r, bool prim) {
    root = r; primary = prim;
    if(j.Open((root + "/node.jnl").c_str()) != gxDBASE_NO_ERROR) { fprintf(stderr, "objrepl: cannot open the journal: %s\n", gxdJournal::RecoverError()); return false; }
    std::string s = root + "/state.gxd";
    if(sdb.Open(s.c_str()) != gxDBASE_NO_ERROR && sdb.Create(s.c_str(), (FAU_t)sizeof(State)) != gxDBASE_NO_ERROR) return false;
    if(j.Attach(&sdb) != gxDBASE_NO_ERROR) return false;
    if(primary) {
      std::string o = root + "/outbox.gxd";
      if(ob.Open(o.c_str()) != gxDBASE_NO_ERROR && ob.Create(o.c_str()) != gxDBASE_NO_ERROR) return false;
      if(j.Attach(&ob) != gxDBASE_NO_ERROR) return false;
    }
    // the databases already here
    if(DIR *d = opendir(root.c_str())) {
      while(struct dirent *e = readdir(d)) {
	std::string n = e->d_name;
	if(n.size() > 4 && n.compare(n.size() - 4, 4, ".gxd") == 0) {
	  std::string nm = n.substr(0, n.size() - 4);
	  if(good_name(nm) && !get(nm)) { closedir(d); return false; }
	}
      }
      closedir(d);
    }
    if(sdb.Read(&st, sizeof(st), (FAU_t)sdb.FileHeaderSize()) != gxDBASE_NO_ERROR || st.magic != STATE_MAGIC) {
      memset(&st, 0, sizeof(st)); st.magic = STATE_MAGIC; st.next_seq = 1; st.epoch = 1;
      st.cur_digest = st.acked_digest = st.applied_digest = DIGEST0;
      j.Begin(); put_state(); if(j.Commit() != gxDBASE_NO_ERROR) return false;
    }
    return true;
  }
  // the database NAME, opened (or created) and attached; never inside a transaction
  Db *get(const std::string &name) {
    auto it = dbs.find(name);
    if(it != dbs.end()) return it->second.get();
    if(!good_name(name) || j.InTransaction()) return 0;
    std::unique_ptr<Db> d(new Db);
    d->name = name;
    if(!d->open(root)) return 0;
    if(j.Attach(&d->db) != gxDBASE_NO_ERROR || j.Attach(&d->bt) != gxDBASE_NO_ERROR) return 0;
    Db *p = d.get();
    dbs[name] = std::move(d);
    return p;
  }
  void close() { j.Close(); for(auto &d : dbs) d.second->close(); if(primary) ob.Close(); sdb.Close(); }
  // the state lives in state.gxd's static area, right after the file header (StaticArea() is its size)
  bool put_state() { return sdb.Write(&st, sizeof(st), (FAU_t)sdb.FileHeaderSize()) == gxDBASE_NO_ERROR; }
  // link fields of DB's object bytes: to ids (to_ids) or to this node's addresses. A link whose target is gone
  // (removed: a dangling link) travels as DANGLING_ID and is stored as DANGLING_ADDR, an address inside the file
  // header that is never an object: dangling on both sides alike, never re-mapped
  void translate(const std::string &db, unsigned char *p, uint32_t n, bool to_ids) {
    for(auto &d : links) {
      if(d.db != db || (size_t)d.off + 8 > n) continue;
      uint64_t v; memcpy(&v, p + d.off, 8);
      uint64_t out = 0;
      Db *t = get(d.target);
      if(v && t) {
	if(to_ids) { auto it = t->who.find((FAU_t)v); out = it == t->who.end() ? DANGLING_ID : it->second; }
	else { auto it = t->where.find(v); out = it == t->where.end() ? DANGLING_ADDR : (uint64_t)it->second; }
      } else if(v) out = to_ids ? DANGLING_ID : DANGLING_ADDR;
      memcpy(p + d.off, &out, 8);
    }
  }
};

// ---------- the wire: one gxStream block per frame (TLS, mutual certificates) ----------
static volatile sig_atomic_t stopping = 0;
struct Tls { std::string ca, cert, key, allow_cn; };
static Tls tls;
static std::string my_name = "node", peer_name;     // --name, --peer-name (empty: any)
static std::string alarm_root;

// a reason to stop streaming: said on stderr and kept in ROOT/objrepl.alarm until someone looks
static void alarm_say(const std::string &why)
{
  fprintf(stderr, "objrepl ALARM: %s\n", why.c_str());
  if(!alarm_root.empty()) if(FILE *f = fopen((alarm_root + "/objrepl.alarm").c_str(), "a")) {
    time_t t = time(0); char ts[32]; strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&t));
    fprintf(f, "%s %s\n", ts, why.c_str()); fclose(f);
  }
}

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
  st.SetTimeOut(30);
  return st.Connect(hostport.substr(0, c), atoi(hostport.substr(c + 1).c_str())) == 0;
}

// ---------- primary ----------
static unsigned rng = 2026;
static unsigned rnd() { rng = rng * 1103515245u + 12345u; return rng >> 8; }

// the outbox entries in seq order: (seq, address)
static std::vector<std::pair<uint64_t, FAU_t>> outbox_entries(Node &s)
{
  std::vector<std::pair<uint64_t, FAU_t>> v;
  for(FAU_t a = s.ob.FindFirstObject(); a != (FAU_t)0; a = s.ob.FindNextObject(a)) {
    unsigned char h[9];
    if(s.ob.Read(h, 9, a) != gxDBASE_NO_ERROR) continue;
    Rd r(h, 9); r.u8(); v.push_back({r.u64(), a});
  }
  std::sort(v.begin(), v.end());
  return v;
}

struct Link { gxStream *st = 0; uint64_t copy_applied = 0, copy_digest = 0; bool copy_in_snapshot = false; bool refused = false;
  long snapshots = 0;
  void drop() { if(st) { st->Close(); delete st; st = 0; } } };

static bool hello(Node &s, Link &L, const std::string &copy)
{
  if(L.st) return true;
  L.st = new gxStream;
  if(!connect_to(*L.st, copy)) { L.drop(); return false; }
  // 11/16/2026: the reconnect check, after every connect: both sides say who they are, their role, epoch, seq and
  // the running digest at that seq; any change of roles or histories stops the stream with an alarm
  Buf h; h.u32(STATE_MAGIC); h.u64(s.st.epoch); h.str(links_spec());
  h.str(my_name); h.u8(ROLE_PRIMARY); h.u64(s.st.next_seq - 1); h.u64(s.st.cur_digest);
  std::vector<unsigned char> rep;
  if(!send_frame(*L.st, h.b) || !recv_frame(*L.st, rep)) { L.drop(); return false; }
  Rd r(rep.data(), rep.size());
  uint8_t ok = r.u8(); uint64_t applied = r.u64(), cep = r.u64(); uint8_t snap = r.u8();
  std::string cname = r.str(); uint8_t crole = r.u8(); uint64_t cdig = r.u64(); std::string why = r.str();
  if(!r.ok || !ok) {
    alarm_say("the copy " + cname + " refused this primary (epoch " + std::to_string(s.st.epoch) + ", its epoch " +
	      std::to_string(cep) + "): " + (why.empty() ? "refused" : why));
    L.drop(); L.refused = true; return false;
  }
  if(crole != ROLE_COPY) { alarm_say("the peer " + cname + " says it is not a copy (role " + std::to_string(crole) + ")"); L.drop(); L.refused = true; return false; }
  if(!peer_name.empty() && cname != peer_name) { alarm_say("the peer is " + cname + ", not the expected copy " + peer_name); L.drop(); L.refused = true; return false; }
  L.copy_applied = applied; L.copy_in_snapshot = snap != 0; L.copy_digest = cdig;
  // the copy's history must be this primary's: the digest at the copy's seq, where this primary still knows it
  if(!L.copy_in_snapshot && applied > 0) {
    bool known = false; uint64_t mine = 0;
    if(applied == s.st.next_seq - 1) { known = true; mine = s.st.cur_digest; }
    else if(applied == s.st.acked_seq) { known = true; mine = s.st.acked_digest; }
    else for(auto &e : outbox_entries(s)) if(e.first == applied) {
      __ULWORD__ n = s.ob.ObjectLength(e.second); std::vector<unsigned char> b(n);
      if(n >= 8 && s.ob.Read(b.data(), n, e.second) == gxDBASE_NO_ERROR) { known = true; memcpy(&mine, b.data() + n - 8, 8); }
    }
    if(applied > s.st.next_seq - 1) {
      alarm_say("the copy " + cname + " is at seq " + std::to_string(applied) + ", past this primary's last commit " +
		std::to_string(s.st.next_seq - 1) + ": another primary wrote to it");
      L.drop(); L.refused = true; return false;
    }
    if(known && mine != cdig) {
      alarm_say("the copy " + cname + " has another history at seq " + std::to_string(applied) + " (digest " +
		std::to_string(cdig) + ", this primary " + std::to_string(mine) + ")");
      L.drop(); L.refused = true; return false;
    }
  }
  return true;
}

static bool exchange(Link &L, const std::vector<unsigned char> &f, uint64_t &acked)
{
  std::vector<unsigned char> ack;
  if(!send_frame(*L.st, f) || !recv_frame(*L.st, ack) || ack.size() < 8) { L.drop(); return false; }
  Rd r(ack.data(), ack.size()); acked = r.u64();
  return true;
}

// every object of every database as of the last commit, then the copy's applied_seq = that commit
static bool snapshot(Node &s, Link &L)
{
  uint64_t base = s.st.next_seq - 1, acked = 0;
  Buf b0; b0.u8(F_SNAP_BEGIN); b0.u64(base); b0.u64(s.st.epoch);
  if(!exchange(L, b0.b, acked)) return false;
  Buf f; f.u8(F_SNAP_BATCH); uint32_t nobj = 0; size_t cnt_at = f.b.size(); f.u32(0);
  auto flush = [&]() -> bool {
    if(!nobj) return true;
    for(int i = 0; i < 4; i++) f.b[cnt_at + i] = (unsigned char)(nobj >> (8 * i));
    if(!exchange(L, f.b, acked)) return false;
    f.b.resize(cnt_at + 4); nobj = 0; for(int i = 0; i < 4; i++) f.b[cnt_at + i] = 0;
    return true;
  };
  for(auto &d : s.dbs) {
    for(auto &w : d.second->where) {
      __ULWORD__ n = d.second->db.ObjectLength(w.second);
      std::vector<unsigned char> rec(n);
      if(d.second->db.Read(rec.data(), n, w.second) != gxDBASE_NO_ERROR || n < sizeof(ObjHead)) return false;
      ObjHead h; memcpy(&h, rec.data(), sizeof(h));
      s.translate(d.first, rec.data() + sizeof(h), h.len, true);
      f.str(d.first); f.u64(h.id); f.u32(h.version); f.u32(h.len); f.raw(rec.data() + sizeof(h), h.len);
      nobj++;
      if(f.b.size() >= SNAP_BATCH_BYTES && !flush()) return false;
    }
  }
  if(!flush()) return false;
  Buf e; e.u8(F_SNAP_END); e.u64(base); e.u64(s.st.cur_digest);
  if(!exchange(L, e.b, acked) || acked != base) return false;
  L.copy_applied = base; L.copy_in_snapshot = false; L.snapshots++;
  return true;
}

// send everything past the copy's applied seq (or a snapshot); trim the outbox up to what the copy has
static bool pump(Node &s, Link &L, const std::string &copy, bool trim)
{
  if(!hello(s, L, copy)) return false;
  auto v = outbox_entries(s);
  bool need_snap = L.copy_in_snapshot || (L.copy_applied + 1 < s.st.next_seq &&
					  (v.empty() || v.front().first > L.copy_applied + 1));
  if(need_snap && !snapshot(s, L)) return false;
  for(auto &e : v) {
    if(e.first <= L.copy_applied) continue;
    __ULWORD__ n = s.ob.ObjectLength(e.second);
    std::vector<unsigned char> b(n);
    if(n < 8 || s.ob.Read(b.data(), n, e.second) != gxDBASE_NO_ERROR) return false;
    b.resize(n - 8);                                           // the frame without the digest trailer
    uint64_t acked = 0;
    if(!exchange(L, b, acked)) return false;
    L.copy_applied = acked;
  }
  if(trim && L.copy_applied > s.st.acked_seq) {
    s.j.Begin();
    for(auto &e : v) if(e.first <= L.copy_applied) {
      if(e.first == L.copy_applied) {                          // keep the digest at the acknowledged seq
	__ULWORD__ n = s.ob.ObjectLength(e.second); std::vector<unsigned char> b(n);
	if(n >= 8 && s.ob.Read(b.data(), n, e.second) == gxDBASE_NO_ERROR) memcpy(&s.st.acked_digest, b.data() + n - 8, 8);
      }
      s.ob.Delete(e.second);
    }
    s.st.acked_seq = L.copy_applied;
    s.put_state();
    if(s.j.Commit() != gxDBASE_NO_ERROR) return false;
  }
  return true;
}

static long drop_linked = 0;   // 11/16/2026 (test): one last commit that removes this many objects that links point to

static int run_primary(const std::string &root, const std::string &copy, long commits, long per, long ndbs, long sleep_ms,
		       bool drain, bool trim)
{
  Node s;
  alarm_root = root;
  if(!s.open(root, true)) return 1;
  if(s.st.role == ROLE_COPY) { alarm_say(root + " is a copy: run takeover before it writes"); s.close(); return 4; }
  if(s.st.role != ROLE_PRIMARY) { s.j.Begin(); s.st.role = ROLE_PRIMARY; s.put_state(); s.j.Commit(); }
  for(long i = 0; i < ndbs; i++) if(!s.get("db" + std::to_string(i))) { fprintf(stderr, "cannot open db%ld\n", i); return 1; }
  Link L;
  uint64_t next_id = 1;
  for(auto &d : s.dbs) for(auto &w : d.second->where) next_id = std::max(next_id, w.first + 1);
  for(long c = 0; !drain && c < commits; c++) {
    // one commit: objects in several databases and their outbox entry, in one transaction
    Buf f; uint64_t seq = s.st.next_seq;
    f.u8(F_COMMIT); f.u64(seq); f.u64(s.st.epoch);
    // the objects change first; the commit's frame is made from their final bytes at the end, so a link to an
    // object added later in the same commit (or a cycle of links) is turned into ids once every target exists
    std::vector<std::tuple<uint8_t, std::string, uint64_t>> touched;
    // databases are opened (and attached to the journal) outside a transaction
    Db *lk = links.empty() ? 0 : s.get(links[0].db);
    for(auto &l : links) if(!s.get(l.db) || !s.get(l.target)) return 1;
    s.j.Begin();
    for(long k = 0; k < per; k++) {
      unsigned r = rnd() % 10;
      if(lk && r == 0) {                                       // link objects (32 bytes; the link fields from the map)
	const LinkDef *cyc = 0, *fw = 0;                       // cyc: a field into lk itself; fw: into another database
	for(auto &l : links) if(l.db == lk->name) { if(l.target == lk->name) cyc = &l; else fw = &l; }
	uint64_t a = next_id++, b = next_id++;
	unsigned char z[32]; memset(z, 0, sizeof(z));
	if(!lk->put(a, 1, z, 32)) return 1;
	if(cyc && (!fw || rnd() % 2)) {                        // c: a cycle in one commit, a -> b and b -> a
	  if(!lk->put(b, 1, z, 32)) return 1;
	  uint64_t la = (uint64_t)lk->where[b], lb = (uint64_t)lk->where[a];
	  unsigned char x[32]; memset(x, 0, 32); memcpy(x + cyc->off, &la, 8); x[24] = 1;
	  unsigned char y[32]; memset(y, 0, 32); memcpy(y + cyc->off, &lb, 8); y[24] = 2;
	  if(!lk->put(a, 2, x, 32) || !lk->put(b, 2, y, 32)) return 1;
	  touched.push_back({OP_ADD, lk->name, a}); touched.push_back({OP_ADD, lk->name, b});
	} else if(fw) {                                        // a: a -> c, c added after a in the same commit
	  Db *t = s.get(fw->target);
	  uint32_t n = 40 + rnd() % 400; std::vector<unsigned char> cb(n); for(auto &q : cb) q = (unsigned char)rnd();
	  if(!t || !t->put(b, 1, cb.data(), n)) return 1;
	  uint64_t lc = (uint64_t)t->where[b];
	  unsigned char x[32]; memset(x, 0, 32); memcpy(x + fw->off, &lc, 8); x[24] = 3;
	  if(!lk->put(a, 2, x, 32)) return 1;
	  touched.push_back({OP_ADD, lk->name, a}); touched.push_back({OP_ADD, t->name, b});
	}
	continue;
      }
      Db *d = s.dbs["db" + std::to_string(rnd() % ndbs)].get();
      bool target = false;
      for(auto &l : links) if(l.target == d->name) target = true;
      if(r < 6 || d->where.empty()) {                            // add
	uint32_t n = 40 + rnd() % 400; std::vector<unsigned char> b(n);
	for(auto &x : b) x = (unsigned char)rnd();
	uint64_t id = next_id++;
	if(!d->put(id, 1, b.data(), n)) { fprintf(stderr, "add failed\n"); return 1; }
	touched.push_back({OP_ADD, d->name, id});
      } else {
	auto it = d->where.begin(); std::advance(it, rnd() % d->where.size()); uint64_t id = it->first;
	if(r < 9 || target) {                                   // update: new bytes (a link target keeps its size,
	  uint32_t n = target ? (uint32_t)(d->db.ObjectLength(it->second) - sizeof(ObjHead))   // so it stays in place)
			      : 40 + rnd() % 400;
	  std::vector<unsigned char> b(n);
	  for(auto &x : b) x = (unsigned char)rnd();
	  if(!d->put(id, 2, b.data(), n)) { fprintf(stderr, "update failed\n"); return 1; }
	  touched.push_back({OP_UPDATE, d->name, id});
	} else {                                                // remove
	  if(!d->remove(id)) { fprintf(stderr, "remove failed\n"); return 1; }
	  touched.push_back({OP_REMOVE, d->name, id});
	}
      }
    }
    Buf ops; uint32_t nops = 0;
    for(auto &t : touched) {
      uint8_t op = std::get<0>(t); const std::string &dn = std::get<1>(t); uint64_t id = std::get<2>(t);
      Db *d = s.get(dn);
      if(op == OP_REMOVE) { ops.u8(OP_REMOVE); ops.str(dn); ops.u64(id); ops.u32(0); nops++; continue; }
      auto w = d->where.find(id);
      if(w == d->where.end()) continue;                        // added or changed, then removed in this commit
      __ULWORD__ n = d->db.ObjectLength(w->second);
      std::vector<unsigned char> rec(n);
      if(n < sizeof(ObjHead) || d->db.Read(rec.data(), n, w->second) != gxDBASE_NO_ERROR) return 1;
      ObjHead h; memcpy(&h, rec.data(), sizeof(h));
      s.translate(dn, rec.data() + sizeof(h), h.len, true);
      ops.u8(op); ops.str(dn); ops.u64(id); ops.u32(h.len); ops.raw(rec.data() + sizeof(h), h.len); nops++;
    }
    f.u32(nops); f.raw(ops.b.data(), ops.b.size());
    s.st.cur_digest = fnv(f.b.data(), f.b.size(), s.st.cur_digest);
    f.u64(s.st.cur_digest);                                    // the outbox entry: the frame, then the digest after it
    FAU_t oa = s.ob.Alloc((__ULWORD__)f.b.size());
    if(!oa || s.ob.Write(f.b.data(), (__ULWORD__)f.b.size(), oa) != gxDBASE_NO_ERROR) { fprintf(stderr, "outbox write failed\n"); return 1; }
    s.st.next_seq = seq + 1;
    s.put_state();
    if(s.j.Commit() != gxDBASE_NO_ERROR) { fprintf(stderr, "commit failed\n"); return 1; }
    pump(s, L, copy, trim);                                    // best effort: a copy that is away catches up later
    if(L.refused) { s.close(); return 3; }
    if(sleep_ms) usleep((useconds_t)sleep_ms * 1000);
  }
  if(drop_linked > 0 && !links.empty()) {
    // case b: remove targets of links, leaving the links dangling on the primary; the copy must agree (links to a
    // gone object compare as 0, and both digests count them)
    std::vector<std::pair<std::string, uint64_t>> victims;
    for(auto &l : links) {
      Db *src = s.get(l.db), *tgt = s.get(l.target);
      if(!src || !tgt || l.target == l.db) continue;
      for(auto &w : src->where) {
	if((long)victims.size() >= drop_linked) break;
	__ULWORD__ n = src->db.ObjectLength(w.second); std::vector<unsigned char> rec(n);
	if(n < sizeof(ObjHead) + l.off + 8 || src->db.Read(rec.data(), n, w.second) != gxDBASE_NO_ERROR) continue;
	uint64_t v; memcpy(&v, rec.data() + sizeof(ObjHead) + l.off, 8);
	auto it = tgt->who.find((FAU_t)v);
	if(v && it != tgt->who.end()) victims.push_back({l.target, it->second});
      }
    }
    std::sort(victims.begin(), victims.end()); victims.erase(std::unique(victims.begin(), victims.end()), victims.end());
    Buf f; uint64_t seq = s.st.next_seq; f.u8(F_COMMIT); f.u64(seq); f.u64(s.st.epoch); f.u32((uint32_t)victims.size());
    s.j.Begin();
    for(auto &v : victims) { if(!s.get(v.first)->remove(v.second)) return 1; f.u8(OP_REMOVE); f.str(v.first); f.u64(v.second); f.u32(0); }
    s.st.cur_digest = fnv(f.b.data(), f.b.size(), s.st.cur_digest); f.u64(s.st.cur_digest);
    FAU_t oa = s.ob.Alloc((__ULWORD__)f.b.size());
    if(!oa || s.ob.Write(f.b.data(), (__ULWORD__)f.b.size(), oa) != gxDBASE_NO_ERROR) return 1;
    s.st.next_seq = seq + 1; s.put_state();
    if(s.j.Commit() != gxDBASE_NO_ERROR) return 1;
    printf("objrepl primary: removed %zu linked object(s): their links now dangle\n", victims.size());
  }
  for(int tries = 0; tries < 600; tries++) {                   // drain: until the copy has every commit
    if(pump(s, L, copy, trim) && L.copy_applied + 1 >= s.st.next_seq) break;
    if(L.refused) { s.close(); return 3; }
    usleep(200000);
  }
  bool done = L.copy_applied + 1 >= s.st.next_seq;
  printf("objrepl primary: epoch %llu, last seq %llu, copy applied %llu, %ld snapshot(s), outbox %s\n",
	 (unsigned long long)s.st.epoch, (unsigned long long)(s.st.next_seq - 1), (unsigned long long)L.copy_applied,
	 L.snapshots, done ? "drained" : "NOT drained");
  L.drop();
  s.close();
  return done ? 0 : 2;
}

// ---------- copy ----------
static void on_term(int) { stopping = 1; }

// the databases named in a commit's ops or a snapshot batch, opened before the transaction begins
static bool open_named(Node &s, const unsigned char *p, size_t n, bool snapshot_batch)
{
  Rd r(p, n);
  uint32_t k = r.u32();
  for(uint32_t i = 0; i < k && r.ok; i++) {
    if(!snapshot_batch) r.u8();
    std::string name = r.str(); r.u64();
    if(snapshot_batch) r.u32();
    uint32_t len = r.u32(); r.raw(len);
    if(!r.ok || !good_name(name) || !s.get(name)) return false;
  }
  return r.ok;
}

static int run_copy(const std::string &root, int port)
{
  Node s;
  alarm_root = root;
  if(!s.open(root, false)) return 1;
  if(s.st.role == ROLE_PRIMARY) { alarm_say(root + " is a primary: it will not run as a copy (two primaries)"); s.close(); return 4; }
  if(s.st.role != ROLE_COPY) { s.j.Begin(); s.st.role = ROLE_COPY; s.put_state(); s.j.Commit(); }
  struct sigaction sa_t; memset(&sa_t, 0, sizeof(sa_t)); sa_t.sa_handler = on_term;   // no SA_RESTART
  sigaction(SIGTERM, &sa_t, 0); sigaction(SIGINT, &sa_t, 0); signal(SIGPIPE, SIG_IGN);
  gxStreamServer srv;
  srv.SetClientCAFile(tls.ca);
  if(srv.AllowClientCN(tls.allow_cn) != 0) { fprintf(stderr, "objrepl copy: --allow-cn is required\n"); return 1; }
  if(srv.Listen(port, tls.cert, tls.key, std::string(), "127.0.0.1") != 0) { fprintf(stderr, "objrepl copy: cannot listen on %d\n", port); return 1; }
  printf("objrepl copy: listening on 127.0.0.1:%d (gxStream, TLS, client CN %s), applied %llu, epoch %llu%s\n", port,
	 tls.allow_cn.c_str(), (unsigned long long)s.st.applied_seq, (unsigned long long)s.st.epoch,
	 s.st.in_snapshot ? ", a snapshot was cut" : "");
  fflush(stdout);
  while(!stopping) {
    gxStream st;
    st.SetTimeOut(3600);
    if(srv.Accept(st) != 0) { if(stopping) break; continue; }
    std::vector<unsigned char> b;
    if(!recv_frame(st, b)) { st.Close(); continue; }
    Rd h(b.data(), b.size()); uint32_t magic = h.u32(); uint64_t pep = h.u64(); std::string lspec = h.str();
    std::string pname = h.str(); uint8_t prole = h.u8(); uint64_t plast = h.u64(); h.u64();
    std::string why;
    if(!h.ok || magic != STATE_MAGIC) why = "not an objrepl primary (format)";
    else if(s.st.role == ROLE_PRIMARY) why = "this node is a primary (taken over at epoch " + std::to_string(s.st.epoch) + ")";
    else if(prole != ROLE_PRIMARY) why = "the peer " + pname + " does not say it is a primary";
    else if(!peer_name.empty() && pname != peer_name) why = "the peer is " + pname + ", not the expected primary " + peer_name;
    else if(pep < s.st.epoch) why = "the primary's epoch " + std::to_string(pep) + " is older than this copy's " + std::to_string(s.st.epoch);
    else if(!s.st.in_snapshot && s.st.applied_seq > plast)
      why = "this copy is at seq " + std::to_string(s.st.applied_seq) + ", past the primary's last commit " + std::to_string(plast);
    else if(!parse_links(lspec)) why = "a bad link map";
    bool ok = why.empty();
    if(!ok) alarm_say("refused " + pname + ": " + why);
    Buf rep;
    rep.u8(ok ? 1 : 0); rep.u64(s.st.applied_seq); rep.u64(s.st.epoch); rep.u8(s.st.in_snapshot ? 1 : 0);
    rep.str(my_name); rep.u8(ROLE_COPY); rep.u64(s.st.applied_digest); rep.str(why);
    send_frame(st, rep.b);
    if(!ok) { st.Close(); continue; }
    if(pep > s.st.epoch) { s.j.Begin(); s.st.epoch = pep; s.put_state(); s.j.Commit(); }
    long frames = 0, drop_every = getenv("OBJREPL_TEST_DROP_EVERY") ? atol(getenv("OBJREPL_TEST_DROP_EVERY")) : 0;
    while(!stopping && recv_frame(st, b)) {
      if(drop_every && ++frames % drop_every == 0) break;      // test only: the connection drops (no restart)
      Rd r(b.data(), b.size());
      uint8_t kind = r.u8();
      bool good = r.ok;
      if(kind == F_COMMIT) {
	uint64_t seq = r.u64(), ep = r.u64();
	if(!r.ok || ep < s.st.epoch || s.st.in_snapshot) break;
	if(seq <= s.st.applied_seq) { Buf a; a.u64(s.st.applied_seq); send_frame(st, a.b); continue; }   // a resend
	if(seq != s.st.applied_seq + 1) { fprintf(stderr, "objrepl copy: commit %llu after %llu: a gap\n", (unsigned long long)seq, (unsigned long long)s.st.applied_seq); break; }
	if(!open_named(s, b.data() + r.o, b.size() - r.o, false)) { fprintf(stderr, "objrepl copy: a bad database name in commit %llu\n", (unsigned long long)seq); break; }
	uint32_t nops = r.u32();
	s.j.Begin();
	// pass 1: every object as sent (its links are still ids); pass 2: the links of the objects this commit
	// wrote, to this copy's addresses, once every target of the commit exists (a later add, a cycle)
	std::vector<std::pair<std::string, uint64_t>> wrote;
	for(uint32_t i = 0; i < nops && good; i++) {
	  uint8_t op = r.u8(); std::string name = r.str(); uint64_t id = r.u64(); uint32_t n = r.u32(); const unsigned char *p = r.raw(n);
	  if(!r.ok) { good = false; break; }
	  Db *d = s.get(name);
	  good = d && (op == OP_REMOVE ? d->remove(id) : d->put(id, op == OP_ADD ? 1 : 2, p, n));
	  if(good && op != OP_REMOVE) wrote.push_back({name, id});
	}
	std::sort(wrote.begin(), wrote.end()); wrote.erase(std::unique(wrote.begin(), wrote.end()), wrote.end());   // each once
	for(auto &w : wrote) {
	  if(!good) break;
	  bool has = false;
	  for(auto &l : links) if(l.db == w.first) has = true;
	  if(!has) continue;
	  Db *d = s.get(w.first);
	  auto it = d->where.find(w.second);
	  if(it == d->where.end()) continue;
	  __ULWORD__ n = d->db.ObjectLength(it->second);
	  std::vector<unsigned char> rec(n);
	  if(n < sizeof(ObjHead) || d->db.Read(rec.data(), n, it->second) != gxDBASE_NO_ERROR) { good = false; break; }
	  ObjHead oh; memcpy(&oh, rec.data(), sizeof(oh));
	  s.translate(w.first, rec.data() + sizeof(oh), oh.len, false);
	  good = d->db.Write(rec.data(), n, it->second) == gxDBASE_NO_ERROR;
	}
	if(!good) { s.j.Rollback(); fprintf(stderr, "objrepl copy: commit %llu did not apply\n", (unsigned long long)seq); break; }
	s.st.applied_seq = seq;
	s.st.applied_digest = fnv(b.data(), b.size(), s.st.applied_digest);
	s.put_state();
	if(s.j.Commit() != gxDBASE_NO_ERROR) { fprintf(stderr, "objrepl copy: commit failed\n"); break; }
	Buf a; a.u64(seq);
	if(!send_frame(st, a.b)) break;
      } else if(kind == F_SNAP_BEGIN) {
	uint64_t base = r.u64(), ep = r.u64();
	if(!r.ok || ep < s.st.epoch) break;
	s.j.Begin();                                           // from empty: every database cleared
	for(auto &d : s.dbs) if(!d.second->clear()) good = false;
	s.st.in_snapshot = 1; s.st.snapshot_base = base; s.st.applied_seq = 0;
	s.put_state();
	if(!good || s.j.Commit() != gxDBASE_NO_ERROR) { if(s.j.InTransaction()) s.j.Rollback(); break; }
	Buf a; a.u64(0);
	if(!send_frame(st, a.b)) break;
      } else if(kind == F_SNAP_BATCH) {
	if(!s.st.in_snapshot || !open_named(s, b.data() + r.o, b.size() - r.o, true)) break;
	uint32_t nobj = r.u32();
	s.j.Begin();
	for(uint32_t i = 0; i < nobj && good; i++) {
	  std::string name = r.str(); uint64_t id = r.u64(); uint32_t ver = r.u32(); uint32_t n = r.u32(); const unsigned char *p = r.raw(n);
	  if(!r.ok) { good = false; break; }
	  Db *d = s.get(name);
	  good = d && d->put(id, ver, p, n);
	}
	if(!good) { s.j.Rollback(); break; }
	if(s.j.Commit() != gxDBASE_NO_ERROR) break;
	Buf a; a.u64(0);
	if(!send_frame(st, a.b)) break;
      } else if(kind == F_SNAP_END) {
	uint64_t base = r.u64(), sdig = r.u64();
	if(!r.ok || !s.st.in_snapshot || base != s.st.snapshot_base) break;
	s.j.Begin();
	// the links came as ids (targets may have arrived later in the snapshot): now each to this copy's address
	for(auto &d : s.dbs) {
	  bool has = false;
	  for(auto &l : links) if(l.db == d.first) has = true;
	  if(!has) continue;
	  for(auto &w : d.second->where) {
	    __ULWORD__ n = d.second->db.ObjectLength(w.second);
	    std::vector<unsigned char> rec(n);
	    if(n < sizeof(ObjHead) || d.second->db.Read(rec.data(), n, w.second) != gxDBASE_NO_ERROR) { good = false; break; }
	    ObjHead oh; memcpy(&oh, rec.data(), sizeof(oh));
	    s.translate(d.first, rec.data() + sizeof(oh), oh.len, false);
	    if(d.second->db.Write(rec.data(), n, w.second) != gxDBASE_NO_ERROR) { good = false; break; }
	    if(getenv("OBJREPL_TEST_KILL_IN_FIXUP")) _exit(9);   // test only: killed inside the link fix-up's transaction
	  }
	}
	if(!good) { s.j.Rollback(); break; }
	s.st.in_snapshot = 0; s.st.applied_seq = base; s.st.applied_digest = sdig;
	s.put_state();
	if(s.j.Commit() != gxDBASE_NO_ERROR) break;
	Buf a; a.u64(base);
	if(!send_frame(st, a.b)) break;
      } else break;
    }
    st.Close();
  }
  s.close();
  return 0;
}

// ---------- digest / status / takeover ----------
static int run_digest(const std::string &root)
{
  // the links are compared as the target's id, never as an address: each database's address -> id map first
  std::map<std::string, std::map<FAU_t, uint64_t>> ids;
  if(DIR *d = opendir(root.c_str())) {
    while(struct dirent *e = readdir(d)) {
      std::string n = e->d_name;
      if(n.size() <= 4 || n.compare(n.size() - 4, 4, ".gxd") != 0) continue;
      std::string nm = n.substr(0, n.size() - 4);
      if(!good_name(nm)) continue;
      gxDatabase db;
      if(db.Open((root + "/" + n).c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) continue;
      for(FAU_t a = db.FindFirstObject(); a != (FAU_t)0; a = db.FindNextObject(a)) {
	ObjHead oh; if(db.Read(&oh, sizeof(oh), a) == gxDBASE_NO_ERROR) ids[nm][a] = oh.id;
      }
      db.Close();
    }
    closedir(d);
  }
  std::map<std::string, std::map<uint64_t, uint64_t>> all;
  size_t dangling = 0;   // 11/16/2026: links (set, not 0) whose target is not an object: reported, compared as 0
  if(DIR *d = opendir(root.c_str())) {
    while(struct dirent *e = readdir(d)) {
      std::string n = e->d_name;
      if(n.size() <= 4 || n.compare(n.size() - 4, 4, ".gxd") != 0) continue;
      std::string nm = n.substr(0, n.size() - 4);
      if(!good_name(nm)) continue;
      gxDatabase db;
      if(db.Open((root + "/" + n).c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) { closedir(d); return 1; }
      auto &h = all[nm];
      for(FAU_t a = db.FindFirstObject(); a != (FAU_t)0; a = db.FindNextObject(a)) {
	__ULWORD__ len = db.ObjectLength(a);
	std::vector<unsigned char> b(len);
	if(db.Read(b.data(), len, a) != gxDBASE_NO_ERROR || len < sizeof(ObjHead)) continue;
	ObjHead oh; memcpy(&oh, b.data(), sizeof(oh));
	if(h.count(oh.id)) { printf("DUPLICATE id %llu in %s\n", (unsigned long long)oh.id, nm.c_str()); closedir(d); return 1; }
	for(auto &l : links) {
	  if(l.db != nm || (size_t)l.off + 8 > oh.len) continue;
	  uint64_t v; memcpy(&v, b.data() + sizeof(oh) + l.off, 8);
	  auto &m = ids[l.target]; auto it = m.find((FAU_t)v);
	  uint64_t idv = v && it != m.end() ? it->second : 0;
	  if(v && it == m.end()) dangling++;                   // a link to an object that is gone: the application's job
	  memcpy(b.data() + sizeof(oh) + l.off, &idv, 8);
	}
	h[oh.id] = fnv(b.data() + sizeof(oh), oh.len, fnv(&oh.len, 4));
      }
      db.Close();
    }
    closedir(d);
  }
  uint64_t x = 14695981039346656037ULL; size_t count = 0;
  for(auto &d : all) {
    if(d.second.empty()) continue;                             // an empty database and a missing one are the same
    x = fnv(d.first.data(), d.first.size(), x);
    for(auto &e : d.second) { x = fnv(&e.first, 8, x); x = fnv(&e.second, 8, x); count++; }
  }
  printf("objects %zu digest %016llx\n", count, (unsigned long long)x);
  if(dangling) fprintf(stderr, "objrepl digest: %zu dangling link(s) (a link to an object that is gone)\n", dangling);
  return 0;
}

static int run_status(const std::string &root)
{
  gxDatabase db; State st;
  if(db.Open((root + "/state.gxd").c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) return 1;
  if(db.Read(&st, sizeof(st), (FAU_t)db.FileHeaderSize()) != gxDBASE_NO_ERROR || st.magic != STATE_MAGIC) { printf("no state\n"); return 1; }
  printf("epoch %llu next_seq %llu acked_seq %llu applied_seq %llu in_snapshot %u role %s digest %016llx\n",
	 (unsigned long long)st.epoch, (unsigned long long)st.next_seq, (unsigned long long)st.acked_seq,
	 (unsigned long long)st.applied_seq, st.in_snapshot,
	 st.role == ROLE_PRIMARY ? "primary" : st.role == ROLE_COPY ? "copy" : "none",
	 (unsigned long long)(st.role == ROLE_PRIMARY ? st.cur_digest : st.applied_digest));
  db.Close();
  return 0;
}

static int run_takeover(const std::string &root)
{
  Node s;
  if(!s.open(root, false)) return 1;
  if(s.st.in_snapshot) { fprintf(stderr, "objrepl takeover: %s is in the middle of a snapshot: refused\n", root.c_str()); s.close(); return 1; }
  s.j.Begin(); s.st.epoch += 1; s.st.next_seq = s.st.applied_seq + 1; s.st.role = ROLE_PRIMARY;
  s.st.cur_digest = s.st.acked_digest = s.st.applied_digest; s.st.acked_seq = s.st.applied_seq; s.put_state();
  int rc = s.j.Commit() == gxDBASE_NO_ERROR ? 0 : 1;
  printf("objrepl takeover: %s is primary at epoch %llu (applied %llu)\n", root.c_str(), (unsigned long long)s.st.epoch,
	 (unsigned long long)s.st.applied_seq);
  s.close();
  return rc;
}

int main(int argc, char **argv)
{
  if(argc < 3) { fprintf(stderr, "usage: objrepl primary|copy|digest|status|takeover ROOT [options]\n"); return 64; }
  std::string cmd = argv[1], root = argv[2], copy;
  long commits = 100, per = 10, ndbs = 3, sleep_ms = 0, port = 0; bool drain = false, trim = true;
  for(int i = 3; i < argc; i++) {
    std::string a = argv[i];
    auto val = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
    if(a == "--copy") copy = val();
    else if(a == "--commits") commits = atol(val().c_str());
    else if(a == "--per") per = atol(val().c_str());
    else if(a == "--dbs") ndbs = std::max(1L, atol(val().c_str()));
    else if(a == "--sleep") sleep_ms = atol(val().c_str());
    else if(a == "--seed") rng = (unsigned)atol(val().c_str());
    else if(a == "--listen") port = atol(val().c_str());
    else if(a == "--drain") drain = true;
    else if(a == "--no-trim") trim = false;
    else if(a == "--drop-linked") drop_linked = atol(val().c_str());
    else if(a == "--ca") tls.ca = val();
    else if(a == "--cert") tls.cert = val();
    else if(a == "--key") tls.key = val();
    else if(a == "--allow-cn") tls.allow_cn = val();
    else if(a == "--name") my_name = val();
    else if(a == "--peer-name") peer_name = val();
    else if(a == "--links") { if(!parse_links(val())) { fprintf(stderr, "bad --links (DB:OFFSET:TARGET,...)\n"); return 64; } }
    else { fprintf(stderr, "unknown option %s\n", a.c_str()); return 64; }
  }
  if(cmd == "primary") return copy.empty() ? 64 : run_primary(root, copy, commits, per, ndbs, sleep_ms, drain, trim);
  if(cmd == "copy") return port ? run_copy(root, (int)port) : 64;
  if(cmd == "digest") return run_digest(root);
  if(cmd == "status") return run_status(root);
  if(cmd == "takeover") return run_takeover(root);
  return 64;
}
