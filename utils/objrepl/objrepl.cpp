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
  objrepl copy ROOT --listen [ADDR:]PORT --ca CA --cert CERT --key KEY
                  --allow-cn CN
  objrepl digest ROOT       every database: ids and content hashes
  objrepl ixcheck ROOT --index ...   each index against the objects
  objrepl check ROOT        OK / WARN / FAIL (exit 0 / 1 / 2)
  objrepl clear-alarm ROOT
  objrepl status ROOT
  Any option may come from ROOT/objrepl.conf (or --config FILE), one
  "key value" line each (the option without "--"); the command line wins.
  Certificates: utils/objrepl/objrepl_certs.sh.
  objrepl takeover ROOT     a copy becomes primary: epoch + 1
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
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
#include <set>
#include "gxdbase.h"
#include "gxbtree.h"
#include "gxdjrnl.h"
#include "gxstream.h"
#include "gxvindex.h"
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <sys/stat.h>

using namespace datareel;

// ---------- records ----------
// 11/16/2026: a record is a head and its bytes, padded to the record's capacity (its block). flags: TOMB = the object
// was removed (or is not known yet): the record keeps the object's id and address so links to it still resolve and
// the address is never reused; STUB = a forwarding stub left where an object had to move on this node only (fwd =
// the object's address now), followed in one hop by a read
struct ObjHead { uint64_t id; uint32_t version; uint32_t len; uint32_t flags; uint32_t reason; uint64_t seen_seq; uint64_t fwd; };
enum { OF_TOMB = 1, OF_STUB = 2 };
enum { TR_REMOVED = 1, TR_UNKNOWN_TARGET = 2, TR_UNKNOWN_REMOVE = 3 };   // why a tombstone exists
const uint32_t PLACEHOLDER_RESERVE = 512;   // the capacity of a tombstone made for an object not known yet
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
enum { OP_ADD = 1, OP_UPDATE = 2, OP_REMOVE = 3, OP_PURGE = 4 };   // every op names (database, object id)
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

// ---------- indexes: each node builds its own ----------
// 11/16/2026: an index is this node's business, like its file addresses: it is configured on each node (--index),
// never sent on the wire, and built from the object bytes in the same transaction as the object (so a crash leaves
// both or neither). "NAME:DB:B:OFF:LEN" is a B-tree on LEN bytes (at most 32) at OFF; "NAME:DB:V:OFF:DIM" is a
// vector index (gxVectorIndex, float32, L2) on DIM floats at OFF. An object too short for the field, or with a
// vector that is not finite, is not in that index. Index fields must not overlap link fields (links hold addresses).
// A new index on a database that already has objects is built from them; ROOT/<file>.building marks an unfinished
// build, which the next open starts again.
struct IxDef { std::string name, db; char kind; uint32_t off, len; };
static std::vector<IxDef> ixdefs;
static bool parse_ix(const std::string &spec)
{
  ixdefs.clear();                                     // the last --index wins (the command line over the config)
  size_t i = 0;
  while(i < spec.size()) {
    size_t j = spec.find(',', i); if(j == std::string::npos) j = spec.size();
    std::vector<std::string> f; std::string e = spec.substr(i, j - i);
    for(size_t a = 0;;) { size_t b = e.find(':', a); f.push_back(e.substr(a, b == std::string::npos ? b : b - a)); if(b == std::string::npos) break; a = b + 1; }
    if(f.size() != 5 || (f[2] != "B" && f[2] != "V")) return false;
    IxDef d{f[0], f[1], f[2][0], (uint32_t)atol(f[3].c_str()), (uint32_t)atol(f[4].c_str())};
    if(d.name.empty() || d.name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != std::string::npos) return false;
    if(d.len == 0 || (d.kind == 'B' && d.len > 32) || (d.kind == 'V' && d.len > 4096)) return false;
    ixdefs.push_back(d);
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
  std::string file(const std::string &root) const { return root + "/" + def.db + "." + def.name + (def.kind == 'B' ? ".ix.btx" : ".vix"); }
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
  std::vector<Ix *> ixs;                  // this node's indexes on this database (owned by the Node)
  // the indexes: a live object's entries out (before it changes or goes) and in (after)
  bool ix_drop(uint64_t id) {
    if(ixs.empty() || !live(id)) return true;
    FAU_t a = where[id];
    __ULWORD__ n = db.ObjectLength(a);
    std::vector<unsigned char> rec(n);
    if(n < sizeof(ObjHead) || db.Read(rec.data(), n, a) != gxDBASE_NO_ERROR) return false;
    ObjHead h; memcpy(&h, rec.data(), sizeof(h));
    for(Ix *x : ixs) if(!x->del(id, rec.data() + sizeof(h), h.len)) return false;
    return true;
  }
  bool ix_put(uint64_t id, const unsigned char *p, uint32_t n) {
    for(Ix *x : ixs) if(!x->add(id, p, n)) return false;
    return true;
  }

  bool open(const std::string &root) {
    std::string g = root + "/" + name + ".gxd", x = root + "/" + name + ".id.btx";
    if(db.Open(g.c_str()) != gxDBASE_NO_ERROR && db.Create(g.c_str()) != gxDBASE_NO_ERROR) return false;
    if(bt.Open(x.c_str()) != gxDBASE_NO_ERROR && bt.Create(x.c_str()) != gxDBASE_NO_ERROR) return false;
    for(FAU_t a = db.FindFirstObject(); a != (FAU_t)0; a = db.FindNextObject(a)) {
      ObjHead h;
      if(db.Read(&h, sizeof(h), a) != gxDBASE_NO_ERROR) continue;
      who[a] = h.id;
      if(h.flags & OF_STUB) { stubs[a] = h.id; continue; }
      where[h.id] = a;
      if(h.flags & OF_TOMB) tombs.insert(h.id);
    }
    return true;
  }
  void close() { bt.Close(); db.Close(); }
  bool live(uint64_t id) const { return where.count(id) && !tombs.count(id); }
  __ULWORD__ capacity(uint64_t id) { auto it = where.find(id); return it == where.end() ? 0 : db.ObjectLength(it->second); }
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
  // an add or an update (also of a tombstone: the object comes back at its address). In place when the bytes fit
  // the record and force_move is off; otherwise the object moves, with capacity want_cap (on a copy: the primary's).
  // free_old: the old block is freed (the primary; a copy mirroring a move the primary made); otherwise a forwarding
  // stub stays at the old address so links to it stay right (a move on this copy only). moved: this call moved it.
  bool put(uint64_t id, uint32_t ver, const unsigned char *p, uint32_t n, __ULWORD__ want_cap, bool force_move,
	   bool free_old, bool &moved) {
    moved = false;
    if(!ix_drop(id)) return false;
    ObjHead h; memset(&h, 0, sizeof(h)); h.id = id; h.version = ver; h.len = n;
    __ULWORD__ need = (__ULWORD__)(sizeof(ObjHead) + n), cap = std::max(need, want_cap);
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
    else {                                                   // this node only: a stub names the new address
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
    who.erase(it->second); where.erase(it); tombs.erase(id);
    for(auto st = stubs.begin(); st != stubs.end();) {
      if(st->second == id) { db.Delete(st->first); who.erase(st->first); st = stubs.erase(st); } else ++st;
    }
    return true;
  }
  bool clear() {                                    // every record and stub (a full send starts from empty)
    for(auto &w : where) if(!ix_drop(w.first)) return false;
    for(auto &w : where) { IdKey k(w.first, w.second), c; bt.Delete(k, c); if(!db.Delete(w.second)) return false; }
    for(auto &st : stubs) if(!db.Delete(st.first)) return false;
    where.clear(); who.clear(); tombs.clear(); stubs.clear();
    return true;
  }
  // read a record by address, following a forwarding stub once
  bool read(FAU_t a, std::vector<unsigned char> &rec, ObjHead &h) {
    for(int hop = 0; hop < 2; hop++) {
      __ULWORD__ n = db.ObjectLength(a);
      rec.resize(n);
      if(n < sizeof(ObjHead) || db.Read(rec.data(), n, a) != gxDBASE_NO_ERROR) return false;
      memcpy(&h, rec.data(), sizeof(h));
      if(!(h.flags & OF_STUB)) return true;
      a = (FAU_t)h.fwd;
    }
    return false;
  }
};

// ---------- a node: its databases, state and (primary) outbox under one journal ----------
struct Node {
  gxdJournal j;
  gxDatabase sdb, ob;
  bool primary = false;
  std::map<std::string, std::unique_ptr<Db>> dbs;
  std::vector<std::unique_ptr<Ix>> ixs;
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
    for(auto &def : ixdefs) if(def.db == name && !open_ix(p, def)) { fprintf(stderr, "objrepl: cannot open index %s\n", def.name.c_str()); return 0; }
    // an index file of NAME this node is not configured for would go stale while the objects change: it is
    // dropped (an index is derived data), so configuring it again builds it from the objects
    if(DIR *dd = opendir(root.c_str())) {
      std::string pre = name + ".";
      while(struct dirent *e = readdir(dd)) {
	std::string f = e->d_name, ix;
	if(f.compare(0, pre.size(), pre) != 0) continue;
	if(f.size() > pre.size() + 7 && f.compare(f.size() - 7, 7, ".ix.btx") == 0) ix = f.substr(pre.size(), f.size() - pre.size() - 7);
	else if(f.size() > pre.size() + 4 && f.compare(f.size() - 4, 4, ".vix") == 0) ix = f.substr(pre.size(), f.size() - pre.size() - 4);
	else continue;
	bool configured = false;
	for(auto &def : ixdefs) if(def.db == name && def.name == ix) configured = true;
	if(!configured) { unlink((root + "/" + f).c_str()); fprintf(stderr, "objrepl: index %s on %s is not configured here: dropped\n", ix.c_str(), name.c_str()); }
      }
      closedir(dd);
    }
    return p;
  }
  // an index of D: opened, or created and built from the objects already here (in one transaction)
  bool open_ix(Db *d, const IxDef &def) {
    std::unique_ptr<Ix> x(new Ix); x->def = def;
    std::string f = x->file(root), mark = f + ".building";
    struct stat sb;
    bool fresh = stat(f.c_str(), &sb) != 0 || stat(mark.c_str(), &sb) == 0;
    if(fresh) {
      if(FILE *m = fopen(mark.c_str(), "w")) fclose(m); else return false;
      unlink(f.c_str());
    }
    if(def.kind == 'B') {
      if((fresh ? x->bt.Create(f.c_str()) : x->bt.Open(f.c_str())) != gxDBASE_NO_ERROR) return false;
      if(j.Attach(&x->bt) != gxDBASE_NO_ERROR) return false;
    } else {
      gxvError e = fresh ? x->vx.Create(f.c_str(), def.len, gxvFLOAT32, gxvL2, 16, x->spec().c_str())
			 : x->vx.Open(f.c_str(), gxDBASE_READWRITE, x->spec().c_str());
      if(e != gxv_NO_ERROR || j.Attach(x->vx.Database()) != gxDBASE_NO_ERROR) return false;
    }
    if(fresh) {
      size_t built = 0;
      j.Begin();
      for(auto &w : d->where) {
	if(d->tombs.count(w.first)) continue;
	__ULWORD__ n = d->db.ObjectLength(w.second);
	std::vector<unsigned char> rec(n);
	if(n < sizeof(ObjHead) || d->db.Read(rec.data(), n, w.second) != gxDBASE_NO_ERROR) { j.Rollback(); return false; }
	ObjHead h; memcpy(&h, rec.data(), sizeof(h));
	if(!x->add(w.first, rec.data() + sizeof(h), h.len)) { j.Rollback(); return false; }
	built++;
	if(built == 50 && getenv("OBJREPL_TEST_KILL_IN_IXBUILD")) _exit(9);   // test only: killed inside the build
      }
      if(j.Commit() != gxDBASE_NO_ERROR) return false;
      unlink(mark.c_str());
      if(built) fprintf(stderr, "objrepl: index %s on %s built from %zu object(s)\n", def.name.c_str(), def.db.c_str(), built);
    }
    d->ixs.push_back(x.get());
    ixs.push_back(std::move(x));
    return true;
  }
  void close() {
    j.Close();
    for(auto &x : ixs) x->close();
    for(auto &d : dbs) d.second->close();
    if(primary) ob.Close();
    sdb.Close();
    ixs.clear(); dbs.clear();
  }
  // the state lives in state.gxd's static area, right after the file header (StaticArea() is its size)
  bool put_state() { return sdb.Write(&st, sizeof(st), (FAU_t)sdb.FileHeaderSize()) == gxDBASE_NO_ERROR; }
  // link fields of DB's object bytes: to ids (to_ids) or to this node's addresses. A removed target is a
  // tombstone, so its address still names its id. An id this node does not hold yet gets a placeholder tombstone
  // (inside the open transaction), so a link is never dangling or 0; a later add of that id replaces it in place
  uint64_t seen_seq = 0;            // the commit being applied (placeholders record it)
  bool translate(const std::string &db, unsigned char *p, uint32_t n, bool to_ids) {
    for(auto &d : links) {
      if(d.db != db || (size_t)d.off + 8 > n) continue;
      uint64_t v; memcpy(&v, p + d.off, 8);
      uint64_t out = 0;
      Db *t = get(d.target);
      if(v && !t) return false;
      if(v && to_ids) { auto it = t->who.find((FAU_t)v); if(it == t->who.end()) return false; out = it->second; }
      else if(v) {
	auto it = t->where.find(v);
	if(it == t->where.end()) {
	  if(!j.InTransaction() || !t->tombstone(v, TR_UNKNOWN_TARGET, seen_seq, PLACEHOLDER_RESERVE)) return false;
	  fprintf(stderr, "objrepl: placeholder tombstone %s id %llu (a link at seq %llu names an id not here yet)\n",
		  t->name.c_str(), (unsigned long long)v, (unsigned long long)seen_seq);
	  it = t->where.find(v);
	}
	out = (uint64_t)it->second;
      }
      memcpy(p + d.off, &out, 8);
    }
    return true;
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

// ---------- status and monitoring ----------
// 11/16/2026: a running node keeps ROOT/objrepl.status, "key value" lines replaced whole (a temp file and a rename),
// at most once a second and at once on a connect, a drop, a refusal and the end. "objrepl check ROOT" reads it with
// the alarm file and the state and says OK, WARN or FAIL in one line (exit 0, 1, 2) for DRSM or cron.
struct Mon {
  std::map<std::string, std::string> v;
  time_t last_write = 0;
  time_t behind_since = 0;            // primary: since when the copy has been behind (0 = current)
  void set(const std::string &k, const std::string &x) { v[k] = x; }
  void set(const std::string &k, unsigned long long x) { v[k] = std::to_string(x); }
  void write(bool force) {
    time_t now = time(0);
    if(alarm_root.empty() || (!force && now == last_write)) return;
    last_write = now;
    set("updated", (unsigned long long)now); set("pid", (unsigned long long)getpid());
    std::string f = alarm_root + "/objrepl.status", t = f + ".tmp";
    FILE *o = fopen(t.c_str(), "w");
    if(!o) return;
    for(auto &e : v) fprintf(o, "%s %s\n", e.first.c_str(), e.second.c_str());
    if(fclose(o) == 0) rename(t.c_str(), f.c_str());
  }
};
static Mon mon;

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
// ---------- certificates ----------
// 11/16/2026: checked when a primary or copy starts, so a bad setup stops there with a plain reason instead of a
// TLS handshake failing later: the CA, certificate and key are set and readable; the key is not readable by group
// or others; the certificate is within its dates, signed by the CA, and matches the key. The days left go in the
// status file; objrepl check says WARN below --min-cert-days (30).
static std::string listen_addr = "127.0.0.1";
static long cert_days_left = -1;
static bool check_tls(std::string &why)
{
  if(tls.ca.empty() || tls.cert.empty() || tls.key.empty()) { why = "ca, cert and key are required"; return false; }
  for(auto *f : {&tls.ca, &tls.cert, &tls.key}) if(access(f->c_str(), R_OK) != 0) { why = "cannot read " + *f; return false; }
  struct stat sb;
  if(stat(tls.key.c_str(), &sb) != 0 || (sb.st_mode & 077)) { why = "the key " + tls.key + " is readable by group or others (chmod 600)"; return false; }
  X509 *cert = 0, *ca = 0; EVP_PKEY *key = 0; X509_STORE *store = 0; X509_STORE_CTX *ctx = 0;
  bool ok = false;
  do {
    FILE *f = fopen(tls.cert.c_str(), "r"); if(f) { cert = PEM_read_X509(f, 0, 0, 0); fclose(f); }
    if(!cert) { why = "no certificate in " + tls.cert; break; }
    f = fopen(tls.ca.c_str(), "r"); if(f) { ca = PEM_read_X509(f, 0, 0, 0); fclose(f); }
    if(!ca) { why = "no CA certificate in " + tls.ca; break; }
    f = fopen(tls.key.c_str(), "r"); if(f) { key = PEM_read_PrivateKey(f, 0, 0, 0); fclose(f); }
    if(!key) { why = "no private key in " + tls.key + " (or it has a passphrase)"; break; }
    if(X509_cmp_current_time(X509_get0_notBefore(cert)) > 0) { why = tls.cert + " is not valid yet"; break; }
    if(X509_cmp_current_time(X509_get0_notAfter(cert)) < 0) { why = tls.cert + " has expired"; break; }
    int days = 0, secs = 0;
    if(ASN1_TIME_diff(&days, &secs, 0, X509_get0_notAfter(cert))) cert_days_left = days;
    if(X509_check_private_key(cert, key) != 1) { why = "the key " + tls.key + " does not match " + tls.cert; break; }
    store = X509_STORE_new(); ctx = X509_STORE_CTX_new();
    if(!store || !ctx || X509_STORE_add_cert(store, ca) != 1 || X509_STORE_CTX_init(ctx, store, cert, 0) != 1) { why = "cannot check the certificate"; break; }
    if(X509_verify_cert(ctx) != 1) { why = tls.cert + " is not signed by the CA " + tls.ca + " (" + X509_verify_cert_error_string(X509_STORE_CTX_get_error(ctx)) + ")"; break; }
    ok = true;
  } while(false);
  if(ctx) X509_STORE_CTX_free(ctx);
  if(store) X509_STORE_free(store);
  if(key) EVP_PKEY_free(key);
  if(ca) X509_free(ca);
  if(cert) X509_free(cert);
  return ok;
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
  long snapshots = 0; time_t last_contact = 0;
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
  L.last_contact = time(0);
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
    for(auto &w : d.second->where) {                         // live objects and tombstones (never stubs)
      __ULWORD__ n = d.second->db.ObjectLength(w.second);
      std::vector<unsigned char> rec(n);
      if(d.second->db.Read(rec.data(), n, w.second) != gxDBASE_NO_ERROR || n < sizeof(ObjHead)) return false;
      ObjHead h; memcpy(&h, rec.data(), sizeof(h));
      if(!(h.flags & OF_TOMB) && !s.translate(d.first, rec.data() + sizeof(h), h.len, true)) return false;
      f.str(d.first); f.u64(h.id); f.u32(h.version); f.u32(h.flags); f.u32(h.reason); f.u64(h.seen_seq); f.u32((uint32_t)n);
      f.u32(h.len); f.raw(rec.data() + sizeof(h), h.len);
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

static bool pump1(Node &s, Link &L, const std::string &copy, bool trim);
// the primary's status: its seq, the copy's applied seq as last acknowledged, the lag and how long it has lasted
static void primary_status(Node &s, Link &L, bool force, const std::string &state)
{
  uint64_t last = s.st.next_seq - 1, lag = last > L.copy_applied ? last - L.copy_applied : 0;
  time_t now = time(0);
  if(!lag) mon.behind_since = 0; else if(!mon.behind_since) mon.behind_since = now;
  if(!force && now == mon.last_write) return;                  // at most once a second (the outbox scan is not free)
  size_t n = 0; unsigned long long bytes = 0, oldest = 0;
  for(auto &e : outbox_entries(s)) {
    __ULWORD__ len = s.ob.ObjectLength(e.second);
    n++; bytes += len;
    if(!oldest && e.first > L.copy_applied && len >= 16) {     // the oldest commit the copy does not have yet
      uint64_t t = 0; if(s.ob.Read(&t, 8, e.second + (FAU_t)(len - 16)) == gxDBASE_NO_ERROR && t && (time_t)t <= now) oldest = now - (time_t)t;
    }
  }
  unsigned long long tombs = 0; for(auto &d : s.dbs) tombs += d.second->tombs.size();
  mon.set("oldest_unacked_secs", oldest); mon.set("last_contact", (unsigned long long)L.last_contact); mon.set("tombstones", tombs);
  mon.set("role", "primary"); mon.set("name", my_name); mon.set("epoch", (unsigned long long)s.st.epoch);
  mon.set("state", state); mon.set("connected", L.st ? 1ULL : 0ULL);
  mon.set("last_seq", (unsigned long long)last); mon.set("copy_applied", (unsigned long long)L.copy_applied);
  mon.set("lag_seq", (unsigned long long)lag); mon.set("behind_secs", (unsigned long long)(mon.behind_since ? now - mon.behind_since : 0));
  mon.set("outbox_entries", (unsigned long long)n); mon.set("outbox_bytes", bytes);
  mon.set("snapshots", (unsigned long long)L.snapshots); mon.set("cert_days_left", std::to_string(cert_days_left));
  mon.write(force);
}
// send everything past the copy's applied seq (or a snapshot); trim the outbox up to what the copy has
static bool pump(Node &s, Link &L, const std::string &copy, bool trim)
{
  bool was = L.st != 0;
  bool ok = pump1(s, L, copy, trim);
  bool is = L.st != 0;
  if(!ok && L.st) L.drop();                                    // a failed exchange: reconnect (and check) next time
  primary_status(s, L, was != is || L.refused || !ok, L.refused ? "refused" : L.st ? "streaming" : "copy away");
  return ok;
}
static bool pump1(Node &s, Link &L, const std::string &copy, bool trim)
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
    if(n < 16 || s.ob.Read(b.data(), n, e.second) != gxDBASE_NO_ERROR) return false;
    b.resize(n - 16);                                          // the frame without the time and digest trailer
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

static long drop_linked = 0;    // 11/16/2026 (test): one commit that removes this many objects that links point to
static long unknown_removes = 0; // (test): one commit that removes this many ids that never existed
static bool purge_unlinked = false;   // a commit that purges every tombstone no link points to
static bool defer_target = false;     // (test): a link target's add is held back to the next commit (an unknown id)

typedef std::tuple<uint8_t, std::string, uint64_t> Touch;

// one commit's record from the objects' final bytes (links as ids), its outbox entry and its state, in the open
// transaction, then Commit(). moved: the (database, id) pairs this primary moved in the commit.
static bool record_commit(Node &s, const std::vector<Touch> &touched, const std::set<std::pair<std::string, uint64_t>> &moved)
{
  Buf f; uint64_t seq = s.st.next_seq;
  f.u8(F_COMMIT); f.u64(seq); f.u64(s.st.epoch);
  Buf ops; uint32_t nops = 0;
  for(auto &t : touched) {
    uint8_t op = std::get<0>(t); const std::string &dn = std::get<1>(t); uint64_t id = std::get<2>(t);
    Db *d = s.get(dn);
    ops.u8(op); ops.str(dn); ops.u64(id); nops++;
    if(op == OP_REMOVE || op == OP_PURGE) continue;
    auto w = d->where.find(id);
    std::vector<unsigned char> rec; ObjHead h; memset(&h, 0, sizeof(h));
    if(w == d->where.end() || !d->read(w->second, rec, h) || (h.flags & OF_TOMB)) {
      ops.u32(0); ops.u8(0); ops.u32(0);                       // changed, then removed in this commit: the remove follows
      continue;
    }
    if(!s.translate(dn, rec.data() + sizeof(h), h.len, true)) { fprintf(stderr, "objrepl primary: a link in %s %llu names no object\n", dn.c_str(), (unsigned long long)id); return false; }
    ops.u32((uint32_t)d->db.ObjectLength(w->second)); ops.u8(moved.count({dn, id}) ? 1 : 0);
    ops.u32(h.len); ops.raw(rec.data() + sizeof(h), h.len);
  }
  f.u32(nops); f.raw(ops.b.data(), ops.b.size());
  s.st.cur_digest = fnv(f.b.data(), f.b.size(), s.st.cur_digest);
  f.u64((uint64_t)time(0));                                    // the outbox entry: the frame, its commit time and
  f.u64(s.st.cur_digest);                                      // the running digest after it (the last 8 bytes)
  FAU_t oa = s.ob.Alloc((__ULWORD__)f.b.size());
  if(!oa || s.ob.Write(f.b.data(), (__ULWORD__)f.b.size(), oa) != gxDBASE_NO_ERROR) { fprintf(stderr, "outbox write failed\n"); return false; }
  s.st.next_seq = seq + 1;
  s.put_state();
  if(s.j.Commit() != gxDBASE_NO_ERROR) { fprintf(stderr, "commit failed\n"); return false; }
  return true;
}

// the live ids of a database
static std::vector<uint64_t> live_ids(Db *d)
{
  std::vector<uint64_t> v;
  for(auto &w : d->where) if(!d->tombs.count(w.first)) v.push_back(w.first);
  return v;
}

// (test data) index fields that mean something: a B-tree key from 64 values (so ids share keys), a vector of
// floats in [-1, 1]. A field the object is too short for is left as random bytes (that object is not indexed)
static void ix_fields(const std::string &db, std::vector<unsigned char> &b)
{
  for(auto &x : ixdefs) {
    if(x.db != db) continue;
    if(x.kind == 'B' && (size_t)x.off + x.len <= b.size()) { memset(b.data() + x.off, 0, x.len); b[x.off] = (unsigned char)(rnd() % 64); }
    if(x.kind == 'V' && (size_t)x.off + 4 * (size_t)x.len <= b.size())
      for(uint32_t i = 0; i < x.len; i++) { float f = (float)((int)(rnd() % 2001) - 1000) / 1000.0f; memcpy(b.data() + x.off + 4 * i, &f, 4); }
  }
}

static int run_primary(const std::string &root, const std::string &copy, long commits, long per, long ndbs, long sleep_ms,
		       bool drain, bool trim)
{
  Node s;
  alarm_root = root;
  if(!s.open(root, true)) return 1;
  if(s.st.role == ROLE_COPY) { alarm_say(root + " is a copy: run takeover before it writes"); s.close(); return 4; }
  { std::string why; if(!check_tls(why)) { fprintf(stderr, "objrepl primary: %s\n", why.c_str()); s.close(); return 64; } }
  if(s.st.role != ROLE_PRIMARY) { s.j.Begin(); s.st.role = ROLE_PRIMARY; s.put_state(); s.j.Commit(); }
  for(long i = 0; i < ndbs; i++) if(!s.get("db" + std::to_string(i))) { fprintf(stderr, "cannot open db%ld\n", i); return 1; }
  for(auto &l : links) if(!s.get(l.db) || !s.get(l.target)) return 1;   // opened outside a transaction
  Link L;
  uint64_t next_id = 1;
  for(auto &d : s.dbs) for(auto &w : d.second->where) next_id = std::max(next_id, w.first + 1);
  std::vector<Touch> held;                                     // defer_target: an add held back to the next commit
  for(long c = 0; !drain && c < commits; c++) {
    // the objects change first; the commit's record is made from their final bytes at the end, so a link to an
    // object added later in the same commit (or a cycle of links) is turned into ids once every target exists
    std::vector<Touch> touched;
    std::set<std::pair<std::string, uint64_t>> moved;
    Db *lk = links.empty() ? 0 : s.get(links[0].db);
    s.j.Begin();
    for(auto &h : held) touched.push_back(h);
    held.clear();
    bool held_one = false;
    for(long k = 0; k < per; k++) {
      unsigned r = rnd() % 10;
      bool mv = false;
      if(lk && r == 0) {                                       // link objects (32 bytes; the link fields from the map)
	const LinkDef *cyc = 0, *fw = 0;                       // cyc: a field into lk itself; fw: into another database
	for(auto &l : links) if(l.db == lk->name) { if(l.target == lk->name) cyc = &l; else fw = &l; }
	uint64_t a = next_id++, b = next_id++;
	unsigned char z[32]; memset(z, 0, sizeof(z));
	if(!lk->put(a, 1, z, 32, 0, false, false, mv)) return 1;
	if(cyc && (!fw || rnd() % 2)) {                        // a cycle in one commit, a -> b and b -> a
	  if(!lk->put(b, 1, z, 32, 0, false, false, mv)) return 1;
	  uint64_t la = (uint64_t)lk->where[b], lb = (uint64_t)lk->where[a];
	  unsigned char x[32]; memset(x, 0, 32); memcpy(x + cyc->off, &la, 8); x[24] = 1;
	  unsigned char y[32]; memset(y, 0, 32); memcpy(y + cyc->off, &lb, 8); y[24] = 2;
	  if(!lk->put(a, 2, x, 32, 0, false, false, mv) || !lk->put(b, 2, y, 32, 0, false, false, mv)) return 1;
	  touched.push_back(Touch(OP_ADD, lk->name, a)); touched.push_back(Touch(OP_ADD, lk->name, b));
	} else if(fw) {                                        // a -> c, c added after a in the same commit
	  Db *t = s.get(fw->target);
	  uint32_t n = 40 + rnd() % 400; std::vector<unsigned char> cb(n); for(auto &q : cb) q = (unsigned char)rnd();
	  if(!t || !t->put(b, 1, cb.data(), n, 0, false, false, mv)) return 1;
	  uint64_t lc = (uint64_t)t->where[b];
	  unsigned char x[32]; memset(x, 0, 32); memcpy(x + fw->off, &lc, 8); x[24] = 3;
	  if(!lk->put(a, 2, x, 32, 0, false, false, mv)) return 1;
	  touched.push_back(Touch(OP_ADD, lk->name, a));
	  // defer_target (test): c's add goes in the NEXT commit, so the copy meets a link to an id it does not hold
	  if(defer_target && !held_one) { held.push_back(Touch(OP_ADD, t->name, b)); held_one = true; }
	  else touched.push_back(Touch(OP_ADD, t->name, b));
	}
	continue;
      }
      Db *d = s.dbs["db" + std::to_string(rnd() % ndbs)].get();
      std::vector<uint64_t> lv = live_ids(d);
      if(r < 6 || lv.empty()) {                                  // add
	uint32_t n = 40 + rnd() % 400; std::vector<unsigned char> b(n);
	for(auto &x : b) x = (unsigned char)rnd();
	ix_fields(d->name, b);
	uint64_t id = next_id++;
	if(!d->put(id, 1, b.data(), n, 0, false, false, mv)) { fprintf(stderr, "add failed\n"); return 1; }
	touched.push_back(Touch(OP_ADD, d->name, id));
      } else {
	uint64_t id = lv[rnd() % lv.size()];
	if(r < 9) {                                             // update: new bytes, a new size (in place when it fits)
	  uint32_t n = 40 + rnd() % 400; std::vector<unsigned char> b(n);
	  for(auto &x : b) x = (unsigned char)rnd();
	  ix_fields(d->name, b);
	  if(!d->put(id, 2, b.data(), n, 0, false, false, mv)) { fprintf(stderr, "update failed\n"); return 1; }
	  if(mv) moved.insert({d->name, id});
	  touched.push_back(Touch(OP_UPDATE, d->name, id));
	} else {                                                // remove: a tombstone at the object's address
	  if(!d->tombstone(id, TR_REMOVED, s.st.next_seq)) { fprintf(stderr, "remove failed\n"); return 1; }
	  touched.push_back(Touch(OP_REMOVE, d->name, id));
	}
      }
    }
    if(!record_commit(s, touched, moved)) return 1;
    pump(s, L, copy, trim);                                    // best effort: a copy that is away catches up later
    if(L.refused) { s.close(); return 3; }
    if(sleep_ms) usleep((useconds_t)sleep_ms * 1000);
  }
  if(!held.empty()) {                                          // a held-back add still owed: its own commit
    s.j.Begin();
    if(!record_commit(s, held, {})) return 1;
    held.clear();
  }
  if(drop_linked > 0 && !links.empty()) {
    // removes of objects that links point to: the targets become tombstones, the links keep naming them (by id
    // on the wire, by address on each side) on the primary and the copy alike
    std::vector<Touch> touched;
    for(auto &l : links) {
      Db *src = s.get(l.db), *tgt = s.get(l.target);
      if(!src || !tgt || l.target == l.db) continue;
      for(auto &w : src->where) {
	if((long)touched.size() >= drop_linked) break;
	std::vector<unsigned char> rec; ObjHead h;
	if(!src->read(w.second, rec, h) || (h.flags & OF_TOMB) || h.len < l.off + 8) continue;
	uint64_t v; memcpy(&v, rec.data() + sizeof(ObjHead) + l.off, 8);
	auto it = tgt->who.find((FAU_t)v);
	if(v && it != tgt->who.end() && tgt->live(it->second)) touched.push_back(Touch(OP_REMOVE, l.target, it->second));
      }
    }
    std::sort(touched.begin(), touched.end()); touched.erase(std::unique(touched.begin(), touched.end()), touched.end());
    s.j.Begin();
    for(auto &t : touched) if(!s.get(std::get<1>(t))->tombstone(std::get<2>(t), TR_REMOVED, s.st.next_seq)) return 1;
    if(!record_commit(s, touched, {})) return 1;
    printf("objrepl primary: removed %zu linked object(s): links now name their tombstones\n", touched.size());
  }
  if(unknown_removes > 0 && !s.dbs.empty()) {
    // removes of ids that never existed: a tombstone on each side
    std::vector<Touch> touched;
    Db *d = s.dbs.begin()->second.get();
    s.j.Begin();
    for(long i = 0; i < unknown_removes; i++) {
      uint64_t id = (1ULL << 40) + next_id++;
      if(!d->tombstone(id, TR_UNKNOWN_REMOVE, s.st.next_seq)) return 1;
      touched.push_back(Touch(OP_REMOVE, d->name, id));
    }
    if(!record_commit(s, touched, {})) return 1;
    printf("objrepl primary: removed %ld unknown id(s)\n", unknown_removes);
  }
  if(purge_unlinked) {
    // a purge: every tombstone that no link points to is freed (a scan of the link fields, no reverse index)
    std::set<std::pair<std::string, uint64_t>> linked;
    for(auto &l : links) {
      Db *src = s.get(l.db), *tgt = s.get(l.target);
      if(!src || !tgt) continue;
      for(auto &w : src->where) {
	std::vector<unsigned char> rec; ObjHead h;
	if(!src->read(w.second, rec, h) || (h.flags & OF_TOMB) || h.len < l.off + 8) continue;
	uint64_t v; memcpy(&v, rec.data() + sizeof(ObjHead) + l.off, 8);
	auto it = tgt->who.find((FAU_t)v);
	if(v && it != tgt->who.end()) linked.insert({l.target, it->second});
      }
    }
    std::vector<Touch> touched;
    for(auto &d : s.dbs) for(auto id : d.second->tombs) if(!linked.count({d.first, id})) touched.push_back(Touch(OP_PURGE, d.first, id));
    s.j.Begin();
    for(auto &t : touched) if(!s.get(std::get<1>(t))->purge(std::get<2>(t))) return 1;
    if(!record_commit(s, touched, {})) return 1;
    printf("objrepl primary: purged %zu tombstone(s), %zu kept (links point to them)\n", touched.size(), linked.size());
  }
  for(int tries = 0; tries < 600; tries++) {                   // drain: until the copy has every commit
    if(pump(s, L, copy, trim) && L.copy_applied + 1 >= s.st.next_seq) break;
    if(L.refused) { s.close(); return 3; }
    usleep(200000);
  }
  bool done = L.copy_applied + 1 >= s.st.next_seq;
  L.drop();
  primary_status(s, L, true, done ? "stopped, drained" : "stopped, NOT drained");
  printf("objrepl primary: epoch %llu, last seq %llu, copy applied %llu, %ld snapshot(s), outbox %s\n",
	 (unsigned long long)s.st.epoch, (unsigned long long)(s.st.next_seq - 1), (unsigned long long)L.copy_applied,
	 L.snapshots, done ? "drained" : "NOT drained");
  L.drop();
  s.close();
  return done ? 0 : 2;
}

// ---------- copy ----------
// gxSocket::Accept() retries accept() on EINTR, so a TERM while waiting for a primary would never be seen: in there
// (no transaction is open) the copy leaves at once; anywhere else it finishes the frame in hand and stops
static volatile sig_atomic_t in_accept = 0;
static void on_term(int) { stopping = 1; if(in_accept) _exit(0); }

// the databases named in a commit's ops or a snapshot batch, opened before the transaction begins
static bool open_named(Node &s, const unsigned char *p, size_t n, bool snapshot_batch)
{
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

static int run_copy(const std::string &root, int port)
{
  Node s;
  alarm_root = root;
  if(!s.open(root, false)) return 1;
  if(s.st.role == ROLE_PRIMARY) { alarm_say(root + " is a primary: it will not run as a copy (two primaries)"); s.close(); return 4; }
  { std::string why; if(!check_tls(why)) { fprintf(stderr, "objrepl copy: %s\n", why.c_str()); s.close(); return 64; } }
  if(s.st.role != ROLE_COPY) { s.j.Begin(); s.st.role = ROLE_COPY; s.put_state(); s.j.Commit(); }
  struct sigaction sa_t; memset(&sa_t, 0, sizeof(sa_t)); sa_t.sa_handler = on_term;   // no SA_RESTART
  sigaction(SIGTERM, &sa_t, 0); sigaction(SIGINT, &sa_t, 0); signal(SIGPIPE, SIG_IGN);
  gxStreamServer srv;
  srv.SetClientCAFile(tls.ca);
  if(srv.AllowClientCN(tls.allow_cn) != 0) { fprintf(stderr, "objrepl copy: --allow-cn is required\n"); return 1; }
  if(srv.Listen(port, tls.cert, tls.key, std::string(), listen_addr) != 0) { fprintf(stderr, "objrepl copy: cannot listen on %s:%d\n", listen_addr.c_str(), port); return 1; }
  printf("objrepl copy: listening on %s:%d (gxStream, TLS, client CN %s), applied %llu, epoch %llu%s\n", listen_addr.c_str(), port,
	 tls.allow_cn.c_str(), (unsigned long long)s.st.applied_seq, (unsigned long long)s.st.epoch,
	 s.st.in_snapshot ? ", a snapshot was cut" : "");
  fflush(stdout);
  bool rolled = false;      // a rolled-back apply: the maps and index memory are stale, the node is opened again
  unsigned long long frames_total = 0, rollbacks = 0, refusals = 0;
  time_t last_frame = 0, snap_since = 0;
  std::string primary_now;
  auto status = [&](bool force, const std::string &state, bool connected) {
    mon.set("role", "copy"); mon.set("name", my_name); mon.set("epoch", (unsigned long long)s.st.epoch);
    mon.set("state", state); mon.set("connected", connected ? 1ULL : 0ULL); mon.set("primary", primary_now.empty() ? "-" : primary_now);
    mon.set("applied_seq", (unsigned long long)s.st.applied_seq); mon.set("in_snapshot", (unsigned long long)s.st.in_snapshot);
    if(s.st.in_snapshot) { if(!snap_since) snap_since = time(0); } else snap_since = 0;
    mon.set("snapshot_secs", (unsigned long long)(snap_since ? time(0) - snap_since : 0));
    mon.set("frames", frames_total); mon.set("last_frame", (unsigned long long)last_frame);
    mon.set("rollbacks", rollbacks); mon.set("refusals", refusals); mon.set("last_contact", (unsigned long long)last_frame);
    unsigned long long tombs = 0; for(auto &d : s.dbs) tombs += d.second->tombs.size();
    mon.set("tombstones", tombs); mon.set("cert_days_left", std::to_string(cert_days_left));
    mon.write(force);
  };
  status(true, "listening", false);
  while(!stopping) {
    gxStream st;
    st.SetTimeOut(3600);
    in_accept = 1;
    if(stopping) _exit(0);
    int arc = srv.Accept(st);
    in_accept = 0;
    if(arc != 0) { if(stopping) break; continue; }
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
    if(!ok) { alarm_say("refused " + pname + ": " + why); refusals++; status(true, "refused " + pname, false); }
    else { primary_now = pname; status(true, "streaming", true); }
    Buf rep;
    rep.u8(ok ? 1 : 0); rep.u64(s.st.applied_seq); rep.u64(s.st.epoch); rep.u8(s.st.in_snapshot ? 1 : 0);
    rep.str(my_name); rep.u8(ROLE_COPY); rep.u64(s.st.applied_digest); rep.str(why);
    send_frame(st, rep.b);
    if(!ok) { st.Close(); continue; }
    for(auto &l : links) { s.get(l.db); s.get(l.target); }   // outside any transaction: placeholders may go there
    if(pep > s.st.epoch) { s.j.Begin(); s.st.epoch = pep; s.put_state(); s.j.Commit(); }
    long frames = 0, drop_every = getenv("OBJREPL_TEST_DROP_EVERY") ? atol(getenv("OBJREPL_TEST_DROP_EVERY")) : 0;
    while(!stopping && recv_frame(st, b)) {
      if(drop_every && ++frames % drop_every == 0) break;      // test only: the connection drops (no restart)
      frames_total++; last_frame = time(0);
      status(false, "streaming", true);
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
	s.seen_seq = seq;
	for(uint32_t i = 0; i < nops && good; i++) {
	  uint8_t op = r.u8(); std::string name = r.str(); uint64_t id = r.u64();
	  Db *d = s.get(name);
	  if(!r.ok || !d) { good = false; break; }
	  if(op == OP_REMOVE) { good = d->tombstone(id, d->where.count(id) ? TR_REMOVED : TR_UNKNOWN_REMOVE, seq); continue; }
	  if(op == OP_PURGE) { good = d->purge(id); continue; }
	  uint32_t cap = r.u32(); uint8_t pmoved = r.u8(); uint32_t n = r.u32(); const unsigned char *p = r.raw(n);
	  if(!r.ok || (op != OP_ADD && op != OP_UPDATE)) { good = false; break; }
	  if(!cap) continue;                                   // changed then removed in the commit: the remove follows
	  bool mv = false;    // a move never frees the old block: a stub there keeps every link to it right
	  good = d->put(id, op == OP_ADD ? 1 : 2, p, n, cap, pmoved != 0, false, mv);
	  if(good) wrote.push_back({name, id});
	}
	std::sort(wrote.begin(), wrote.end()); wrote.erase(std::unique(wrote.begin(), wrote.end()), wrote.end());   // each once
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
	  good = s.translate(w.first, rec.data() + sizeof(oh), oh.len, false) &&
		 d->db.Write(rec.data(), n, it->second) == gxDBASE_NO_ERROR;
	}
	if(!good) { { s.j.Rollback(); rolled = true; } fprintf(stderr, "objrepl copy: commit %llu did not apply\n", (unsigned long long)seq); break; }
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
	if(!good || s.j.Commit() != gxDBASE_NO_ERROR) { if(s.j.InTransaction()) { s.j.Rollback(); rolled = true; } break; }
	Buf a; a.u64(0);
	if(!send_frame(st, a.b)) break;
      } else if(kind == F_SNAP_BATCH) {
	if(!s.st.in_snapshot || !open_named(s, b.data() + r.o, b.size() - r.o, true)) break;
	uint32_t nobj = r.u32();
	s.j.Begin();
	for(uint32_t i = 0; i < nobj && good; i++) {
	  std::string name = r.str(); uint64_t id = r.u64(); uint32_t ver = r.u32(), flags = r.u32(), reason = r.u32();
	  uint64_t seen = r.u64(); uint32_t cap = r.u32(); uint32_t n = r.u32(); const unsigned char *p = r.raw(n);
	  Db *d = s.get(name);
	  if(!r.ok || !d) { good = false; break; }
	  bool mv = false;
	  good = (flags & OF_TOMB) ? d->tombstone(id, reason, seen, cap) : d->put(id, ver, p, n, cap, false, true, mv);
	}
	if(!good) { { s.j.Rollback(); rolled = true; } break; }
	if(s.j.Commit() != gxDBASE_NO_ERROR) break;
	Buf a; a.u64(0);
	if(!send_frame(st, a.b)) break;
      } else if(kind == F_SNAP_END) {
	uint64_t base = r.u64(), sdig = r.u64();
	if(!r.ok || !s.st.in_snapshot || base != s.st.snapshot_base) break;
	s.j.Begin();
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
	    if(!s.translate(d.first, rec.data() + sizeof(oh), oh.len, false)) { good = false; break; }
	    if(d.second->db.Write(rec.data(), n, w.second) != gxDBASE_NO_ERROR) { good = false; break; }
	    if(getenv("OBJREPL_TEST_KILL_IN_FIXUP")) _exit(9);   // test only: killed inside the link fix-up's transaction
	  }
	}
	if(!good) { { s.j.Rollback(); rolled = true; } break; }
	s.st.in_snapshot = 0; s.st.applied_seq = base; s.st.applied_digest = sdig;
	s.put_state();
	if(s.j.Commit() != gxDBASE_NO_ERROR) break;
	Buf a; a.u64(base);
	if(!send_frame(st, a.b)) break;
      } else break;
    }
    st.Close();
    if(rolled) { rollbacks++; s.close(); if(!s.open(root, false)) return 1; rolled = false; }
    status(true, "listening", false);
  }
  status(true, "stopped", false);
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
	ObjHead oh; if(db.Read(&oh, sizeof(oh), a) == gxDBASE_NO_ERROR) ids[nm][a] = oh.id;   // records and stubs
      }
      db.Close();
    }
    closedir(d);
  }
  std::map<std::string, std::map<uint64_t, uint64_t>> all;
  size_t dangling = 0, tombs = 0, stubs = 0;   // dangling: a link to no record at all (never expected now)
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
	if(oh.flags & OF_STUB) { stubs++; continue; }          // this node's own forwarding stubs: not compared
	if(h.count(oh.id)) { printf("DUPLICATE id %llu in %s\n", (unsigned long long)oh.id, nm.c_str()); closedir(d); return 1; }
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
    closedir(d);
  }
  uint64_t x = 14695981039346656037ULL; size_t count = 0;
  for(auto &d : all) {
    if(d.second.empty()) continue;                             // an empty database and a missing one are the same
    x = fnv(d.first.data(), d.first.size(), x);
    for(auto &e : d.second) { x = fnv(&e.first, 8, x); x = fnv(&e.second, 8, x); count++; }
  }
  printf("objects %zu digest %016llx\n", count - tombs, (unsigned long long)x);
  fprintf(stderr, "objrepl digest: %zu tombstone(s) %zu stub(s) %zu dangling\n", tombs, stubs, dangling);
  return 0;
}

// every --index of ROOT: its entries against the objects (rebuilt from the bytes), and a digest of its contents
// (keys and ids, or ids and vectors; never addresses) to compare across nodes. 0 = all consistent, 5 = not
static int run_ixcheck(const std::string &root)
{
  int rc = 0;
  for(auto &def : ixdefs) {
    { Ix t; t.def = def; struct stat sb;
      if(stat((t.file(root) + ".building").c_str(), &sb) == 0) {
	printf("index %s %s %c UNFINISHED BUILD (stopped in the middle; the next start builds it again)\n", def.name.c_str(), def.db.c_str(), def.kind);
	rc = 5; continue;
      } }
    gxDatabase db;
    std::string g = root + "/" + def.db + ".gxd";
    if(db.Open(g.c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) { printf("index %s: no database %s\n", def.name.c_str(), def.db.c_str()); rc = 5; continue; }
    Ix x; x.def = def;
    std::set<std::pair<std::string, uint64_t>> want_b;
    std::map<uint64_t, std::vector<float>> want_v;
    for(FAU_t a = db.FindFirstObject(); a != (FAU_t)0; a = db.FindNextObject(a)) {
      __ULWORD__ n = db.ObjectLength(a);
      std::vector<unsigned char> rec(n);
      if(n < sizeof(ObjHead) || db.Read(rec.data(), n, a) != gxDBASE_NO_ERROR) continue;
      ObjHead h; memcpy(&h, rec.data(), sizeof(h));
      if(h.flags & (OF_TOMB | OF_STUB)) continue;
      if(def.kind == 'B') { IxKey k; if(x.key(h.id, rec.data() + sizeof(h), h.len, k)) want_b.insert({std::string((char *)k.d.k, 32), h.id}); }
      else { std::vector<float> v; if(x.vec(rec.data() + sizeof(h), h.len, v)) want_v[h.id] = v; }
    }
    db.Close();
    size_t entries = 0, missing = 0, extra = 0; uint64_t dig = 14695981039346656037ULL;
    if(def.kind == 'B') {
      if(x.bt.Open(x.file(root).c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) { printf("index %s: cannot open\n", def.name.c_str()); rc = 5; continue; }
      std::set<std::pair<std::string, uint64_t>> have;
      IxKey k, c;
      for(int ok = x.bt.FindFirst(k); ok; ok = x.bt.FindNext(k, c)) { have.insert({std::string((char *)k.d.k, 32), k.d.id}); dig = fnv(&k.d, sizeof(k.d), dig); entries++; }
      x.bt.Close();
      for(auto &w : want_b) if(!have.count(w)) missing++;
      for(auto &h : have) if(!want_b.count(h)) extra++;
    } else {
      if(x.vx.Open(x.file(root).c_str(), gxDBASE_READONLY, x.spec().c_str()) != gxv_NO_ERROR) { printf("index %s: cannot open\n", def.name.c_str()); rc = 5; continue; }
      unsigned long long nodes = 0;
      dig = x.vx.LogicalDigest(&nodes); entries = (size_t)nodes;
      std::vector<float> got(def.len);
      for(auto &w : want_v)
	if(x.vx.Get((long long)w.first, got.data()) != gxv_NO_ERROR || memcmp(got.data(), w.second.data(), 4 * (size_t)def.len) != 0) missing++;
      if(entries > want_v.size() - std::min(missing, want_v.size())) extra = entries - (want_v.size() - missing);
      x.vx.Close();
    }
    bool good = !missing && !extra;
    printf("index %s %s %c entries %zu digest %016llx %s", def.name.c_str(), def.db.c_str(), def.kind, entries, (unsigned long long)dig, good ? "consistent" : "MISMATCH");
    if(!good) printf(" (%zu missing or wrong, %zu extra)", missing, extra);
    printf("\n");
    if(!good) rc = 5;
  }
  return rc;
}

// objrepl check ROOT [--max-lag N] [--max-behind S]: one line and an exit code: 0 OK, 1 WARN, 2 FAIL. FAIL: an alarm
// not yet cleared (objrepl clear-alarm ROOT).
// WARN: the node is not running; a primary's copy is more than max_lag commits behind for more than max_behind
// seconds, or away that long; a copy inside a full send for more than max_behind seconds
static int run_monitor(const std::string &root, unsigned long long max_lag, unsigned long long max_behind, long min_cert_days)
{
  std::map<std::string, std::string> v;
  if(FILE *f = fopen((root + "/objrepl.status").c_str(), "r")) {
    char line[1024];
    while(fgets(line, sizeof(line), f)) {
      std::string l(line); while(!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
      size_t sp = l.find(' '); if(sp != std::string::npos) v[l.substr(0, sp)] = l.substr(sp + 1);
    }
    fclose(f);
  }
  auto num = [&](const char *k) { return v.count(k) ? strtoull(v[k].c_str(), 0, 10) : 0ULL; };
  std::string last_alarm; unsigned long long alarms = 0;
  if(FILE *f = fopen((root + "/objrepl.alarm").c_str(), "r")) {
    char line[1024];
    while(fgets(line, sizeof(line), f)) { alarms++; last_alarm = line; }
    fclose(f);
    while(!last_alarm.empty() && last_alarm.back() == '\n') last_alarm.pop_back();
  }
  if(v.empty() && !alarms) { printf("WARN %s: no status (never run here)\n", root.c_str()); return 1; }
  pid_t pid = (pid_t)num("pid");
  bool running = pid > 0 && kill(pid, 0) == 0 && v["state"].compare(0, 7, "stopped") != 0;
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
  if(alarms) { printf("FAIL %s: %s; %llu alarm(s), last: %s\n", who.c_str(), what.c_str(), alarms, last_alarm.c_str()); return 2; }
  unsigned long long contact = num("last_contact");
  std::string warn;                                            // every reason, not just the first
  auto add = [&](const std::string &w) { warn += (warn.empty() ? "" : ", ") + w; };
  if(!running) add("not running");
  else if(v["role"] == "primary" && !num("connected") && num("behind_secs") > max_behind) add("the copy is away");
  else if(v["role"] == "primary" && num("lag_seq") > max_lag && num("behind_secs") > max_behind) add("the copy is behind");
  else if(v["role"] == "primary" && num("lag_seq") && num("oldest_unacked_secs") > max_behind) add("the oldest unacknowledged commit is old");
  if(running && v["role"] == "copy" && num("in_snapshot") && num("snapshot_secs") > max_behind) add("a full send is taking long");
  if(v.count("cert_days_left") && atol(v["cert_days_left"].c_str()) >= 0 && atol(v["cert_days_left"].c_str()) < min_cert_days)
    add("the certificate expires in " + v["cert_days_left"] + " day(s)");
  if(!warn.empty()) { printf("WARN %s: %s; %s\n", who.c_str(), warn.c_str(), what.c_str()); return 1; }
  printf("OK %s: %s; last contact %s\n", who.c_str(), what.c_str(),
	 contact ? (std::to_string((unsigned long long)time(0) - contact) + "s ago").c_str() : "never");
  return 0;
}
// an alarm read by someone: kept in objrepl.alarm.seen, so monitor says OK again
static int run_clear_alarm(const std::string &root)
{
  std::string a = root + "/objrepl.alarm", s = a + ".seen";
  FILE *in = fopen(a.c_str(), "r");
  if(!in) { printf("no alarm\n"); return 0; }
  FILE *out = fopen(s.c_str(), "a");
  char buf[4096]; size_t n;
  while(out && (n = fread(buf, 1, sizeof(buf), in)) > 0) fwrite(buf, 1, n, out);
  fclose(in);
  if(!out || fclose(out) != 0) return 1;
  unlink(a.c_str());
  printf("alarm cleared (kept in %s)\n", s.c_str());
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
  if(argc < 3) { fprintf(stderr, "usage: objrepl primary|copy|digest|ixcheck|status|check|clear-alarm|takeover ROOT [options]\n"); return 64; }
  std::string cmd = argv[1], root = argv[2], copy;
  long commits = 100, per = 10, ndbs = 3, sleep_ms = 0, port = 0, min_cert_days = 30; bool drain = false, trim = true;
  unsigned long long max_lag = 100, max_behind = 60;
  // 11/16/2026: the config, ROOT/objrepl.conf or --config FILE: "key value" lines (# comments), each key an option
  // without its "--" (name, peer-name, copy, listen, ca, cert, key, allow-cn, links, index, max-lag, max-behind,
  // min-cert-days). The command line comes after it, so it wins
  std::vector<std::string> args;
  std::string conf = root + "/objrepl.conf"; bool conf_given = false;
  for(int i = 3; i + 1 < argc; i++) if(std::string(argv[i]) == "--config") { conf = argv[i + 1]; conf_given = true; }
  if(FILE *f = fopen(conf.c_str(), "r")) {
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
      static const std::set<std::string> keys = {"name", "peer-name", "copy", "listen", "ca", "cert", "key", "allow-cn",
						 "links", "index", "max-lag", "max-behind", "min-cert-days"};
      if(!keys.count(k) || v.empty()) { fprintf(stderr, "%s:%d: unknown key or no value: %s\n", conf.c_str(), ln, k.c_str()); fclose(f); return 64; }
      args.push_back("--" + k); args.push_back(v);
    }
    fclose(f);
  } else if(conf_given) { fprintf(stderr, "cannot read the config %s\n", conf.c_str()); return 64; }
  for(int i = 3; i < argc; i++) args.push_back(argv[i]);
  for(size_t i = 0; i < args.size(); i++) {
    std::string a = args[i];
    auto val = [&]() { return i + 1 < args.size() ? args[++i] : std::string(); };
    if(a == "--copy") copy = val();
    else if(a == "--commits") commits = atol(val().c_str());
    else if(a == "--per") per = atol(val().c_str());
    else if(a == "--dbs") ndbs = std::max(1L, atol(val().c_str()));
    else if(a == "--sleep") sleep_ms = atol(val().c_str());
    else if(a == "--seed") rng = (unsigned)atol(val().c_str());
    else if(a == "--listen") {                           // PORT or ADDRESS:PORT (default address 127.0.0.1)
      std::string v = val(); size_t c = v.rfind(':');
      if(c != std::string::npos) { listen_addr = v.substr(0, c); v = v.substr(c + 1); }
      port = atol(v.c_str());
    }
    else if(a == "--config") val();
    else if(a == "--min-cert-days") min_cert_days = atol(val().c_str());
    else if(a == "--drain") drain = true;
    else if(a == "--no-trim") trim = false;
    else if(a == "--drop-linked") drop_linked = atol(val().c_str());
    else if(a == "--unknown-removes") unknown_removes = atol(val().c_str());
    else if(a == "--purge") purge_unlinked = true;
    else if(a == "--defer-target") defer_target = true;
    else if(a == "--ca") tls.ca = val();
    else if(a == "--cert") tls.cert = val();
    else if(a == "--key") tls.key = val();
    else if(a == "--allow-cn") tls.allow_cn = val();
    else if(a == "--name") my_name = val();
    else if(a == "--peer-name") peer_name = val();
    else if(a == "--max-lag") max_lag = strtoull(val().c_str(), 0, 10);
    else if(a == "--max-behind") max_behind = strtoull(val().c_str(), 0, 10);
    else if(a == "--index") { if(!parse_ix(val())) { fprintf(stderr, "bad --index (NAME:DB:B|V:OFFSET:LEN,...)\n"); return 64; } }
    else if(a == "--links") { if(!parse_links(val())) { fprintf(stderr, "bad --links (DB:OFFSET:TARGET,...)\n"); return 64; } }
    else { fprintf(stderr, "unknown option %s\n", a.c_str()); return 64; }
  }
  if(cmd == "primary") return copy.empty() ? 64 : run_primary(root, copy, commits, per, ndbs, sleep_ms, drain, trim);
  if(cmd == "copy") return port ? run_copy(root, (int)port) : 64;
  for(auto &x : ixdefs) for(auto &l : links)          // an index field may not hold a link (an address)
    if(x.db == l.db && l.off < x.off + (x.kind == 'B' ? x.len : 4 * x.len) && x.off < l.off + 8) {
      fprintf(stderr, "index %s overlaps the link field %s:%u\n", x.name.c_str(), l.db.c_str(), l.off); return 64;
    }
  if(cmd == "digest") return run_digest(root);
  if(cmd == "ixcheck") return run_ixcheck(root);
  if(cmd == "check" || cmd == "monitor") return run_monitor(root, max_lag, max_behind, min_cert_days);
  if(cmd == "clear-alarm") return run_clear_alarm(root);
  if(cmd == "status") return run_status(root);
  if(cmd == "takeover") return run_takeover(root);
  return 64;
}
