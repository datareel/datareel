// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: digest_test.cpp
// Compiler Used: MSVC, GCC
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

DataReel 7.4: gxDatabaseDigest() (gxddigest.h) walks B-trees of node
order 3 (made 5: 4 keys a node) and 16 and counts every key: 1000
random 64-bit keys in each (from the 7.4 beta "gxrepl digest" check
gxrepl_digest_node_orders, review of c59a264). Also the bytes and
objects digests of a small gxDatabase file.

Usage: digest_test WORK_DIR
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include "gxdbase.h"
#include "gxbtree.h"
#include "gxddigest.h"

using namespace datareel;

struct U64Key : public DatabaseKeyB {
  uint64_t v;
  U64Key(uint64_t x = 0) : DatabaseKeyB((char *)&v), v(x) { }
  size_t KeySize() { return sizeof(v); }
  int CompareKey(const DatabaseKeyB &o) const { uint64_t w; memcpy(&w, o.db_key, sizeof(w)); return v < w ? -1 : v > w ? 1 : 0; }
  int operator==(const DatabaseKeyB &o) const { return CompareKey(o) == 0; }
  int operator>(const DatabaseKeyB &o) const { return CompareKey(o) > 0; }
};

static int failures = 0;
static void check(bool ok, const std::string &what)
{
  printf("%s %s\n", ok ? "PASS" : "FAIL", what.c_str());
  if(!ok) failures++;
}

int main(int argc, char **argv)
{
  if(argc < 2) { fprintf(stderr, "Usage: digest_test WORK_DIR\n"); return 2; }
  std::string work = argv[1];
  for(unsigned order : {3u, 16u}) {
    U64Key t, c;
    gxBtree bt(t, (BtreeNodeOrder_t)order);
    std::string f = work + "/keys" + std::to_string(order) + ".btx";
    remove(f.c_str());
    if(bt.Create(f.c_str()) != gxDBASE_NO_ERROR) { check(false, "create " + f); continue; }
    unsigned long long x = 88172645463325252ULL;
    for(unsigned long i = 0; i < 1000; i++) {
      x ^= x << 13; x ^= x >> 7; x ^= x << 17;
      U64Key k(x);
      if(!bt.Insert(k, c)) { check(false, "insert"); break; }
    }
    bt.Close();
    uint64_t h = 0, n = 0;
    std::string err;
    int rc = gxDatabaseDigest(f, "btree", h, n, err);
    check(rc == 0 && n == 1000, "btree order " + std::to_string(order) + ": " + std::to_string(n) + " keys " + err);
    remove(f.c_str());
  }
  {
    std::string f = work + "/objects.gxd";
    remove(f.c_str());
    gxDatabase db;
    if(db.Create(f.c_str()) == gxDBASE_NO_ERROR) {
      for(int i = 0; i < 25; i++) {
	char rec[64];
	memset(rec, 'a' + i % 26, sizeof(rec));
	FAU_t a = db.Alloc(sizeof(rec));
	if(a) db.Write(rec, sizeof(rec), a);
      }
      db.Close();
    }
    uint64_t h1 = 0, n1 = 0, h2 = 0, n2 = 0;
    std::string err;
    check(gxDatabaseDigest(f, "objects", h1, n1, err) == 0 && n1 == 25, "objects: " + std::to_string(n1) + " " + err);
    check(gxDatabaseDigest(f, "bytes", h2, n2, err) == 0 && n2 > 0 && h2 != 0, "bytes: " + std::to_string(n2) + " " + err);
    check(gxDatabaseDigest(f, "nothing", h2, n2, err) != 0, "an unknown kind is refused");
    remove(f.c_str());
  }
  printf(failures ? "%d digest checks failed\n" : "All digest checks passed\n", failures);
  return failures ? 1 : 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
