// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: gxdbobject.cpp
// C++ Compiler Used: GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/08/2026
// Date Last Modified: 11/08/2026
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

#if !defined (__WIN32__)

#include "gxdbobject.h"
#include "gxdbase.h"
#include "gxdjrnl.h"
#include "gxstream.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

namespace datareel {

struct gxObjectService::Db {
  gxObjectDbConfig cfg;
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
  std::string r = root;
  while(r.size() > 1 && r.back() == '/') r.pop_back();
  for(auto &e : dbs) {
    Db *d = e.second.get();
    // Compare real paths (a symlink or ".." in the config cannot hide a copy)
    std::string p = d->cfg.path;
    size_t k = p.find_last_of('/');
    char buf[PATH_MAX];
    if(k != std::string::npos && realpath(p.substr(0, k).c_str(), buf)) p = std::string(buf) + p.substr(k);
    if(p.compare(0, r.size() + 1, r + "/") == 0) {
      d->write = false;              // A copy only changes by replication
      d->apply_journal = apply_journal;
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
    if(!d->apply_journal.empty() && access(d->apply_journal.c_str(), F_OK) == 0 &&
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

#endif // !__WIN32__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
