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

The folder copy example's records and their derived indexes
(examples/oodb/replication_copy.cpp, Linux and Windows): one record
type and two indexes made from it. 11/16/2026: the "example_records"
extractor of the gxrepl example went with gxrepl (7.4 replication is
gxObjRepl).

A record is a fixed block in data.gxd: a 24-byte name, the day
(yyyymmdd) and a vector member (gxvindex.h, dimension 8, float32). From
the records come two indexes, which a copy rebuilds from its records:
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

// A record in an object's bytes, or 0 if the object is too short
inline const Record *as_record(const unsigned char *b, uint32_t len)
{
  return len >= sizeof(Record) ? (const Record *)b : 0;
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
