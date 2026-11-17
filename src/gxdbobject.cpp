// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: gxdbobject.cpp
// C++ Compiler Used: GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/08/2026
// Date Last Modified: 11/17/2026
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

Database objects over the network (gxdbobject.h).
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

// 11/17/2026 (7.4): Windows too
#include "gxdbobject.h"
#include "gxvset.h"
#include <thread>
#include <chrono>
#include <algorithm>
#include <cmath>
#include "gxdbase.h"
#include "gxdjrnl.h"
#include "gxstream.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <filesystem>
#include <system_error>

namespace datareel {

// 11/17/2026 (7.4): a file's path with its folder made real (symlinks and
// ".." resolved), '/' separators, lower case on Windows (its paths are
// not case sensitive); the path as given if the folder does not exist
static std::string gxdo_real_path(const std::string &path)
{
  std::filesystem::path p(path);
  std::error_code ec;
  std::filesystem::path dir = p.has_parent_path() ? p.parent_path() : std::filesystem::path(".");
  std::filesystem::path real = std::filesystem::canonical(dir, ec);
  std::string s = ec ? p.generic_string() : (real / p.filename()).generic_string();
#if defined (__WIN32__)
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)tolower(c); });
#endif
  return s;
}

static std::string gxdo_root(const std::string &root)
{
  std::error_code ec;
  std::filesystem::path real = std::filesystem::canonical(root, ec);
  std::string r = ec ? std::filesystem::path(root).generic_string() : real.generic_string();
#if defined (__WIN32__)
  std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return (char)tolower(c); });
#endif
  while(r.size() > 1 && r.back() == '/') r.pop_back();
  return r;
}

static bool gxdo_exists(const std::string &f)
{
  std::error_code ec;
  return std::filesystem::exists(f, ec);
}

struct gxObjectService::Db {
  gxObjectDbConfig cfg;
  // 11/14/2026 (7.4): vectors (opened on the first vector request)
  std::mutex vm;
  std::unique_ptr<gxVectorSet> vset;
  std::string vec_apply_journal;   // the vectors are a replicated copy: searched under this read lock
  bool write = false;            // Writes allowed (access write, not a copy)
  std::string apply_journal;     // A replicated copy: read under this journal's read lock
  std::mutex m;
  std::unique_ptr<gxDatabase> db; // Writable databases stay open
  std::unique_ptr<gxdJournal> j;
};

static void p64(std::vector<unsigned char> &v, uint64_t x)
{ unsigned char b[8]; memcpy(b, &x, 8); v.insert(v.end(), b, b + 8); }
static void pbytes(std::vector<unsigned char> &v, const unsigned char *p, size_t n)
{ p64(v, (uint64_t)n); v.insert(v.end(), p, p + n); }
static bool g64(const std::vector<unsigned char> &v, size_t &off, uint64_t &x)
{ if(v.size() - off < 8 || off > v.size()) return false; memcpy(&x, v.data() + off, 8); off += 8; return true; }

gxObjectService::gxObjectService(const std::vector<gxObjectDbConfig> &list, const std::string &legacy_db)
  : legacy(legacy_db)
{
  for(auto &c : list) {
    std::unique_ptr<Db> d(new Db);
    d->cfg = c;
    if(d->cfg.journal.empty()) d->cfg.journal = c.path + ".jnl";
    d->write = c.access == "write";
    dbs[c.name] = std::move(d);
  }
}

gxObjectService::~gxObjectService()
{
  for(auto &e : dbs) {
    Db *d = e.second.get();
    if(d->j) d->j->Close();
    if(d->db) d->db->Close();
  }
}

void gxObjectService::ProtectRoot(const std::string &root, const std::string &apply_journal)
{
  // 11/17/2026 (7.4): real paths on both sides (a symlink or ".." in the
  // config cannot hide a copy; Windows paths compare without case)
  std::string r = gxdo_root(root);
  for(auto &e : dbs) {
    Db *d = e.second.get();
    std::string p = gxdo_real_path(d->cfg.path);
    if(p.compare(0, r.size() + 1, r + "/") == 0) {
      d->write = false;              // A copy only changes by replication
      d->apply_journal = apply_journal;
    }
    // 11/14/2026 (7.4): the vector files too
    if(!d->cfg.vectors.empty()) {
      std::string v = gxdo_real_path(d->cfg.vectors);
      if(v.compare(0, r.size() + 1, r + "/") == 0) d->vec_apply_journal = apply_journal;
    }
  }
}

gxObjectService::Db *gxObjectService::find(const std::string &name)
{
  auto it = dbs.find(name);
  return it == dbs.end() ? 0 : it->second.get();
}

bool gxObjectService::OpenAllowed(const std::string &name, const std::string &mode, std::string &why)
{
  Db *d = find(name);
  if(!d) { why = "no database " + name; return false; }
  if(mode != "read" && mode != "write") { why = "bad mode " + mode; return false; }
  if(mode == "write" && !d->write) {
    why = "database " + name + (d->apply_journal.empty() ? " is read-only" : " is a replicated copy (read-only)");
    return false;
  }
  return true;
}

int gxObjectService::Request(const std::string &name, bool write, uint8_t op,
			     const std::vector<unsigned char> &body,
			     std::vector<unsigned char> &reply, std::string &err)
{
  reply.clear();
  Db *d = find(name);
  if(!d) { err = "no database " + name; return -1; }
  // 11/14/2026 (7.4): vector searches never take the database's own lock
  if(op == gxOBJ_VECTOR_SEARCH || op == gxOBJ_VECTOR_INFO) return vector_request(d, op, body, reply, err);
  bool changes = op == gxOBJ_ADD || op == gxOBJ_CHANGE || op == gxOBJ_DELETE;
  if(changes && (!write || !d->write)) { err = "database " + name + " is open for reading"; return -1; }
  std::lock_guard<std::mutex> g(d->m);
  // A writable database stays open with its journal; a read-only one is
  // opened for each request (a replicated copy changes under it), under
  // the copy's apply journal read lock
  gxdJournalReadLock rl;
  gxDatabase rodb;
  gxDatabase *db;
  if(d->write) {
    if(!d->db) {
      d->db.reset(new gxDatabase);
      if(d->db->Open(d->cfg.path.c_str()) != gxDBASE_NO_ERROR &&
	 d->db->Create(d->cfg.path.c_str()) != gxDBASE_NO_ERROR) {
	d->db.reset(); err = "cannot open " + d->cfg.path; return -1;
      }
      d->j.reset(new gxdJournal);
      if(d->j->Open(d->cfg.journal.c_str()) != gxDBASE_NO_ERROR ||
	 d->j->Attach(d->db.get()) != gxDBASE_NO_ERROR) {
	d->j.reset(); d->db->Close(); d->db.reset();
	err = "cannot open the journal " + d->cfg.journal; return -1;
      }
    }
    db = d->db.get();
  }
  else {
    if(!d->apply_journal.empty() && gxdo_exists(d->apply_journal) &&
       rl.Lock(d->apply_journal.c_str()) != gxDBASE_NO_ERROR) {
      err = "cannot lock " + d->apply_journal; return -1;
    }
    if(rodb.Open(d->cfg.path.c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) {
      err = "cannot open " + d->cfg.path; return -1;
    }
    db = &rodb;
  }
  size_t off = 0;
  uint64_t addr = 0;
  int rc = 0;
  if(changes && d->j->Begin() != gxDBASE_NO_ERROR) { err = "cannot begin a transaction"; return -1; }
  switch(op) {
  case gxOBJ_ADD: {
    uint64_t n = 0;
    if(!g64(body, off, n) || body.size() - off != n || n == 0) { err = "bad request"; rc = -1; break; }
    FAU_t a = db->Alloc((__ULWORD__)n);
    if(a == (FAU_t)0 || db->Write(body.data() + off, (__ULWORD__)n, a) != gxDBASE_NO_ERROR) {
      err = "cannot add the object"; rc = -1; break;
    }
    p64(reply, (uint64_t)a);
    break;
  }
  case gxOBJ_REQUEST: {
    if(!g64(body, off, addr) || addr == 0) { err = "bad request"; rc = -1; break; }
    __ULWORD__ n = db->ObjectLength((FAU_t)addr);
    std::vector<unsigned char> buf(n);
    if(n == 0 || db->Read(buf.data(), n, (FAU_t)addr) != gxDBASE_NO_ERROR) { err = "no object at that address"; rc = -1; break; }
    pbytes(reply, buf.data(), n);
    break;
  }
  case gxOBJ_CHANGE: {
    uint64_t n = 0;
    if(!g64(body, off, addr) || !g64(body, off, n) || body.size() - off != n || addr == 0) { err = "bad request"; rc = -1; break; }
    if(db->ObjectLength((FAU_t)addr) != (__ULWORD__)n) { err = "the new object must have the old length"; rc = -1; break; }
    if(db->Write(body.data() + off, (__ULWORD__)n, (FAU_t)addr) != gxDBASE_NO_ERROR) { err = "cannot change the object"; rc = -1; }
    break;
  }
  case gxOBJ_DELETE:
    if(!g64(body, off, addr) || addr == 0) { err = "bad request"; rc = -1; break; }
    if(!db->Delete((FAU_t)addr)) { err = "cannot delete the object"; rc = -1; }
    break;
  case gxOBJ_FIRST:
    p64(reply, (uint64_t)db->FindFirstObject());
    break;
  case gxOBJ_NEXT:
    if(!g64(body, off, addr)) { err = "bad request"; rc = -1; break; }
    p64(reply, (uint64_t)db->FindNextObject((FAU_t)addr));
    break;
  default:
    err = "unknown object operation"; rc = -1;
  }
  if(changes) {
    if(rc == 0 && d->j->Commit() != gxDBASE_NO_ERROR) { err = "commit failed"; rc = -1; }
    if(rc != 0) d->j->Rollback();
  }
  if(!d->write) rodb.Close();
  return rc;
}

// ---------------- Vector searches (11/14/2026, 7.4) ----------------

void gxObjectService::SetVectorLimits(unsigned threads, unsigned max_concurrent)
{
  std::lock_guard<std::mutex> g(vec_m);
  vec_threads = threads ? threads : 1;
  vec_max = max_concurrent ? max_concurrent : 1;
}

static void p32(std::vector<unsigned char> &v, uint32_t x)
{ unsigned char b[4]; memcpy(b, &x, 4); v.insert(v.end(), b, b + 4); }
static bool g32(const std::vector<unsigned char> &v, size_t &off, uint32_t &x)
{ if(off > v.size() || v.size() - off < 4) return false; memcpy(&x, v.data() + off, 4); off += 4; return true; }
static void pstr(std::vector<unsigned char> &v, const std::string &s)
{ p32(v, (uint32_t)s.size()); v.insert(v.end(), s.begin(), s.end()); }

int gxObjectService::vector_request(Db *d, uint8_t op, const std::vector<unsigned char> &body,
				    std::vector<unsigned char> &reply, std::string &err)
{
  if(d->cfg.vectors.empty()) { err = "no vector index"; return -1; }
  // 11/14/2026 (7.4 scan S2): a search takes its slot (at most vec_max at once, one more waits up to 5 s) BEFORE
  // the copy's apply lock: waiting for a slot while holding the read lock let a burst of searches keep the copy's
  // applies "busy"
  struct Slot {
    gxObjectService *s = 0;
    ~Slot() { if(s) { std::lock_guard<std::mutex> g(s->vec_m); s->vec_running--; s->vec_cv.notify_one(); } }
  } slot;
  unsigned slot_threads = 1;
  if(op == gxOBJ_VECTOR_SEARCH) {
    std::unique_lock<std::mutex> g(vec_m);
    if(!vec_cv.wait_for(g, std::chrono::seconds(5), [&] { return vec_running < vec_max; })) { err = "busy"; return -1; }
    vec_running++;
    slot.s = this;
    slot_threads = vec_threads;
  }
  // The copy's apply lock (a reader never sees a half-applied batch), then the set
  gxdJournalReadLock rl;
  if(!d->vec_apply_journal.empty() && gxdo_exists(d->vec_apply_journal) &&
     rl.Lock(d->vec_apply_journal.c_str()) != gxDBASE_NO_ERROR) {
    err = "cannot lock " + d->vec_apply_journal; return -1;
  }
  gxVectorSet *set;
  {
    std::lock_guard<std::mutex> g(d->vm);
    if(!d->vset) {
      std::unique_ptr<gxVectorSet> s(new gxVectorSet);
      const std::string &v = d->cfg.vectors;
      bool manifest = v.size() > 7 && v.compare(v.size() - 7, 7, ".vixset") == 0;
      gxvError e = manifest ? s->Open(v.c_str()) : s->OpenFile(v.c_str());
      if(e != gxv_NO_ERROR) {
	err = "cannot open the vectors " + v + ": " + (s->Error().empty() ? gxVectorIndex::ErrorMessage(e) : s->Error());
	return -1;
      }
      d->vset = std::move(s);
    }
    set = d->vset.get();
  }
  if(op == gxOBJ_VECTOR_INFO) {
    if(!body.empty()) { err = "bad request"; return -1; }
    unsigned long long nodes = 0, deleted = 0;
    for(auto &f : set->Files()) { nodes += f.nodes; deleted += f.deleted; }
    p32(reply, gxObjVectorVersion);
    p32(reply, set->Dim());
    p32(reply, (uint32_t)set->ElementType());
    p32(reply, (uint32_t)set->Metric());
    p32(reply, (uint32_t)set->Files().size());
    p64(reply, nodes);
    p64(reply, deleted);
    pstr(reply, set->Spec());
    return 0;
  }
  // VECTOR_SEARCH
  size_t off = 0;
  uint32_t ver = 0, k = 0, ef = 0, dim = 0, flags = 0, mask = 0, lo[4], hi[4], dlo = 0, dhi = 0;
  bool ok = g32(body, off, ver) && g32(body, off, k) && g32(body, off, ef) && g32(body, off, dim) &&
    g32(body, off, flags) && g32(body, off, mask);
  for(int i = 0; ok && i < 4; i++) ok = g32(body, off, lo[i]);
  for(int i = 0; ok && i < 4; i++) ok = g32(body, off, hi[i]);
  ok = ok && g32(body, off, dlo) && g32(body, off, dhi);
  if(!ok) { err = "bad request: short"; return -1; }
  if(ver != gxObjVectorVersion) { err = "unknown vector request version " + std::to_string(ver); return -1; }
  if(k == 0 || k > gxObjVectorMaxK) { err = "k must be 1 to " + std::to_string(gxObjVectorMaxK); return -1; }
  if(ef > gxObjVectorMaxEf) { err = "ef must be at most " + std::to_string(gxObjVectorMaxEf); return -1; }
  if(dim != set->Dim()) { err = "the query has dimension " + std::to_string(dim) + ", the index " + std::to_string(set->Dim()); return -1; }
  if(flags & ~3u) { err = "unknown flags"; return -1; }
  if(mask & ~15u) { err = "unknown tag bits"; return -1; }
  if(body.size() - off != (size_t)dim * 4) { err = "bad request: the query is not dim floats"; return -1; }
  std::vector<float> q(dim);
  memcpy(q.data(), body.data() + off, (size_t)dim * 4);
  for(float x : q) if(!std::isfinite(x)) { err = "the query is not finite"; return -1; }
  gxvFilter f;
  for(int i = 0; i < 4; i++) if(mask & (1u << i)) f.Range((unsigned)i, lo[i], hi[i]);
  gxvSetSearch o;
  o.k = k;
  o.ef = ef ? ef : 64;
  o.filter = mask ? &f : 0;
  o.day_range = (flags & 1) ? 1 : 0;
  o.day_lo = dlo; o.day_hi = dhi;
  o.strict = (flags & 2) ? 1 : 0;
  o.threads = slot_threads;                       // the slot was taken before the apply lock (S2)
#ifdef GXREPL_TEST_HOOKS
  if(const char *ms = getenv("GXDB_VECTOR_TEST_HOLD_MS")) std::this_thread::sleep_for(std::chrono::milliseconds(atoi(ms)));
#endif
  std::vector<gxvSetResult> r;
  gxvError e;
  unsigned long long skipped, missing;
  std::vector<std::string> paths;
  gxvSetStats st;
  e = set->Search(q.data(), o, r, &st);     // searches of one set run side by side (the set's files never change here)
  skipped = st.skipped;
  missing = st.missing;
  for(auto &fl : set->Files()) paths.push_back(fl.path);
  if(e != gxv_NO_ERROR) err = st.error.empty() ? gxVectorIndex::ErrorMessage(e) : st.error;
  if(e != gxv_NO_ERROR) return -1;
  p32(reply, gxObjVectorVersion);
  p32(reply, (uint32_t)r.size());
  p32(reply, (uint32_t)skipped);
  p32(reply, (uint32_t)missing);
  std::vector<uint32_t> named;
  for(auto &x : r) {
    p64(reply, (uint64_t)x.id);
    unsigned char fb[4]; memcpy(fb, &x.distance, 4); reply.insert(reply.end(), fb, fb + 4);
    for(int i = 0; i < 4; i++) p32(reply, x.tags[i]);
    p32(reply, x.file);
    if(std::find(named.begin(), named.end(), x.file) == named.end()) named.push_back(x.file);
  }
  std::sort(named.begin(), named.end());
  p32(reply, (uint32_t)named.size());
  for(uint32_t fi : named) { p32(reply, fi); pstr(reply, fi < paths.size() ? paths[fi] : std::string()); }
  return 0;
}

void gxObjectService::ServeChannel(gxdMuxSession &mux, uint32_t ch, const std::string &name,
				   const std::string &mode, std::atomic<bool> &stop)
{
  bool write = mode == "write";
  while(!stop) {
    std::vector<unsigned char> msg, reply, out;
    uint64_t seq = 0;
    std::string e;
    int rc = mux.ReceiveMessage(ch, msg, seq, 1000, e);
    if(rc == 1) continue;
    if(rc < 0) break;
    if(msg.empty()) { mux.CloseChannel(ch, "empty request"); break; }
    std::vector<unsigned char> body(msg.begin() + 1, msg.end());
    int r = Request(name, write, msg[0], body, reply, e);
    out.push_back(r == 0 ? 0 : 1);
    if(r == 0) out.insert(out.end(), reply.begin(), reply.end());
    else { uint32_t n = (uint32_t)e.size(); unsigned char b[4]; memcpy(b, &n, 4); out.insert(out.end(), b, b + 4); out.insert(out.end(), e.begin(), e.end()); }
    if(mux.Send(ch, out, gxdMUX_LAST, e) != 0) break;
  }
}

bool gxObjectService::Claims(const gxBlockHeader &gx, const std::vector<unsigned char> &)
{
  if(legacy.empty()) return false;
  __SBYTE__ st = gxStream::BlockStatus(gx);
  return st == gxAddRemoteBlock || st == gxRequestBlock;
}

// A 6.x style client: 'A' adds (answer: a result block with the address),
// 'R' with an address requests (answer: a block with the object), 'L'
// ends. Everything else ends the session.
void gxObjectService::Serve(gxDatabaseSession &ses)
{
  gxStream &s = ses.Stream();
  gxBlockHeader hdr = ses.first_header;
  std::vector<unsigned char> block = ses.first_body;
  bool first = true;
  for(;;) {
    if(!first) {
      if(s.ReadHeader(hdr) != gxSOCKET_NO_ERROR) return;
      __SBYTE__ st0 = gxStream::BlockStatus(hdr);
      if(st0 == gxCloseConnection) return;
      if(s.ReadBlock(block, hdr) != gxSOCKET_NO_ERROR) return;
    }
    first = false;
    __SBYTE__ st = gxStream::BlockStatus(hdr);
    std::vector<unsigned char> body, reply;
    std::string e;
    if(st == gxAddRemoteBlock) {
      p64(body, block.size()); body.insert(body.end(), block.begin(), block.end());
      FAU_t a = (FAU_t)0;
      if(Request(legacy, true, gxOBJ_ADD, body, reply, e) == 0 && reply.size() >= 8) {
	uint64_t x; memcpy(&x, reply.data(), 8); a = (FAU_t)x;
      }
      if(s.WriteResultBlock(a) != gxSOCKET_NO_ERROR) return;
    }
    else if(st == gxRequestBlock) {
      uint64_t x = 0;
      if(block.size() == 8) memcpy(&x, block.data(), 8);
      p64(body, x);
      std::vector<unsigned char> data;
      if(Request(legacy, false, gxOBJ_REQUEST, body, reply, e) == 0 && reply.size() >= 8)
	data.assign(reply.begin() + 8, reply.end());
      if(s.WriteBlock(data.data(), (int)data.size()) != gxSOCKET_NO_ERROR) return;
    }
    else return;
  }
}

} // namespace datareel

// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
