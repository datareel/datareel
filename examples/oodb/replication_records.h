// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: replication_records.h
// Compiler Used: GCC, MSVC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/14/2026
// Date Last Modified: 11/14/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- //
// ---------- Include File Description and Details  ---------- //
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

The replication example's records and their derived indexes
(examples/oodb/replication.cpp, and replication_copy.cpp, the folder
copy example that also builds on Windows): one record type, two indexes made from
it, and the "example_records" extractor that keeps both on a copy,
object by object (gxrextract.h).

A record is a fixed block in data.gxd: a 24-byte name, the day
(yyyymmdd) and a vector member (gxvindex.h, dimension 8, float32). From
the records come two indexes, which a copy never receives but keeps
itself:
  by_name.btx   a B-tree of (name, record address)
  vectors.vix   a vector index: id = the record's address, tag 2 = the day
*/
// ----------------------------------------------------------- //
#ifndef __REPLICATION_RECORDS_HPP__
#define __REPLICATION_RECORDS_HPP__

#include "gxdlcode.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <filesystem>
#include <system_error>
#include "gxdbase.h"
#include "gxbtree.h"
#include "gxvindex.h"
#include "gxrextract.h"

namespace example {

using namespace datareel;

const unsigned DIM = 8;
const char *const SPEC = "example/v1";
const size_t MEMBER = gxvMemberHead + 4 * DIM;   // a float32 vector member

struct Record {
  char name[24];
  uint32_t day;
  uint32_t pad;
  unsigned char vec[MEMBER];
};

// The B-tree key: the name, then the record's address (names may repeat)
struct KeyData { char name[24]; FAU addr; };

class NameKey : public DatabaseKeyB {
public:
  NameKey() : DatabaseKeyB((char *)&d) { memset(&d, 0, sizeof(d)); }
  NameKey(const char *name, FAU_t a) : DatabaseKeyB((char *)&d) {
    memset(&d, 0, sizeof(d));
    strncpy(d.name, name, sizeof(d.name) - 1);
    d.addr = (FAU)a;
  }
  size_t KeySize() { return sizeof(d); }
  int CompareKey(const DatabaseKeyB &o) const {
    const KeyData *k = (const KeyData *)o.db_key;
    int c = memcmp(d.name, k->name, sizeof(d.name));
    if(c) return c;
    return d.addr < k->addr ? -1 : d.addr > k->addr ? 1 : 0;
  }
  int operator==(const DatabaseKeyB &o) const { return CompareKey(o) == 0; }
  int operator>(const DatabaseKeyB &o) const { return CompareKey(o) > 0; }
  KeyData d;
};

const BtreeNodeOrder_t ORDER = 16;

inline bool is_btx(const std::string &p) { return p.size() > 4 && p.compare(p.size() - 4, 4, ".btx") == 0; }

// ---------------- The "example_records" extractor ----------------
// One extractor for both indexes: Open() looks at the index's name.

struct Handle {
  bool btx = false;
  NameKey type;
  gxBtree *bt = 0;
  gxVectorIndex vix;
  ~Handle() { if(bt) { bt->Close(); delete bt; } }
};

inline void say(char *err, size_t n, const std::string &m) { if(err && n) snprintf(err, n, "%s", m.c_str()); }

inline const Record *as_record(const unsigned char *b, uint32_t len)
{
  return len >= sizeof(Record) ? (const Record *)b : 0;
}

inline const char *x_version() { return "example_records 1"; }

inline void *x_open(const char *index_path, const char *, const char *, char *err, size_t n)
{
  Handle *h = new Handle;
  h->btx = is_btx(index_path);
  if(h->btx) {
    h->bt = new gxBtree(h->type, ORDER);
    if(h->bt->Open(index_path) != gxDBASE_NO_ERROR) { say(err, n, std::string("cannot open ") + index_path); delete h; return 0; }
  }
  else if(h->vix.Open(index_path) != gxv_NO_ERROR) { say(err, n, std::string("cannot open ") + index_path); delete h; return 0; }
  return h;
}

inline int x_remove(void *hv, uint64_t addr, const unsigned char *b, uint32_t len, char *err, size_t n)
{
  Handle *h = (Handle *)hv;
  const Record *r = as_record(b, len);
  if(!r) return 0;                                  // not one of ours: never in the indexes
  if(h->btx) {
    NameKey k(r->name, (FAU_t)addr), c;
    h->bt->Delete(k, c);                            // not there: not an error
    return 0;
  }
  gxvError e = h->vix.Delete((long long)addr);
  if(e == gxv_NO_ERROR || e == gxv_ID_NOT_FOUND) return 0;
  say(err, n, gxVectorIndex::ErrorMessage(e));
  return -1;
}

inline int x_insert(void *hv, uint64_t addr, const unsigned char *b, uint32_t len, char *err, size_t n)
{
  Handle *h = (Handle *)hv;
  const Record *r = as_record(b, len);
  if(!r) return 0;
  if(h->btx) {
    NameKey k(r->name, (FAU_t)addr), c;
    if(!h->bt->Insert(k, c)) { say(err, n, "cannot insert the key"); return -1; }
    return 0;
  }
  unsigned t[gxvTagCount] = { 0, 0, r->day, 0 };
  gxvError e = h->vix.InsertMember((long long)addr, r->vec, (unsigned)MEMBER, t);
  if(e == gxv_NO_ERROR || e == gxv_BAD_ARG || e == gxv_NOT_FINITE) return 0;   // no valid vector: not indexed
  say(err, n, gxVectorIndex::ErrorMessage(e));
  return -1;
}

inline int x_flush(void *hv, uint64_t, char *err, size_t n)
{
  Handle *h = (Handle *)hv;
  if(h->btx) {
    if(h->bt->WriteBtreeHeader() != gxDBASE_NO_ERROR || h->bt->gxDatabasePtr()->WriteFileHdr() != gxDBASE_NO_ERROR) {
      say(err, n, "cannot write the B-tree header"); return -1;
    }
    return 0;
  }
  return h->vix.Flush() == gxv_NO_ERROR ? 0 : (say(err, n, "cannot flush the vector index"), -1);
}

inline uint64_t x_count(void *hv)
{
  Handle *h = (Handle *)hv;
  return h->btx ? (uint64_t)h->bt->NumKeys() : (uint64_t)h->vix.Count();
}

inline void x_close(void *hv) { delete (Handle *)hv; }

inline const gxrExtractorOps *extractor()
{
  static const gxrExtractorOps ops = { GXR_EXTRACTOR_ABI, x_version, x_open, x_remove, x_insert, x_flush, x_count, x_close };
  return &ops;
}

// A file renamed over another: rename() on POSIX; on Windows rename()
// refuses an existing target, std::filesystem::rename replaces it
inline bool replace_file(const std::string &from, const std::string &to)
{
  std::error_code ec;
  std::filesystem::rename(from, to, ec);
  return !ec;
}

// ---------------- The full rebuild (the fallback) ----------------
// Both indexes made again from every record, in ascending address order,
// written beside them and renamed over them. "records" and "keys" for
// derived.stamp. 0, or -1 with why.
inline int rebuild(const std::string &dir, unsigned long long &records, unsigned long long &keys, std::string &why)
{
  records = keys = 0;
  gxDatabase db;
  if(db.Open((dir + "/data.gxd").c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) { why = "no data.gxd"; return -1; }
  std::string tb = dir + "/.by_name.btx.rebuild", tv = dir + "/.vectors.vix.rebuild";
  remove(tb.c_str()); remove(tv.c_str());
  {
    NameKey type;
    gxBtree bt(type, ORDER);
    gxVectorIndex vix;
    if(bt.Create(tb.c_str()) != gxDBASE_NO_ERROR) { why = "cannot create the B-tree"; return -1; }
    if(vix.Create(tv.c_str(), DIM, gxvFLOAT32, gxvL2, 8, SPEC, 100) != gxv_NO_ERROR) { why = "cannot create the vector index"; return -1; }
    vix.SetFromMembers(1);
    std::vector<unsigned char> b;
    for(FAU_t a = db.FindFirstObject(); a != (FAU_t)0; a = db.FindNextObject(a)) {
      __ULWORD__ n = db.ObjectLength(a);
      b.resize(n);
      if(n == 0 || db.Read(b.data(), n, a) != gxDBASE_NO_ERROR) continue;
      const Record *r = as_record(b.data(), (uint32_t)n);
      if(!r) continue;
      records++;
      NameKey k(r->name, a), c;
      bt.Insert(k, c);
      unsigned t[gxvTagCount] = { 0, 0, r->day, 0 };
      vix.InsertMember((long long)a, r->vec, (unsigned)MEMBER, t);
    }
    keys = bt.NumKeys();
    bt.Close();
    vix.Close();
  }
  db.Close();
  if(!replace_file(tb, dir + "/by_name.btx") || !replace_file(tv, dir + "/vectors.vix")) {
    why = "cannot rename the rebuilt indexes"; return -1;
  }
  return 0;
}

} // namespace example

#endif // __REPLICATION_RECORDS_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
