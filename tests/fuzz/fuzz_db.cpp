// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: fuzz_db.cpp
// Compiler Used: Clang (libFuzzer)
// Produced By: DataReel Software Development Team
// File Creation Date: 10/31/2026
// Date Last Modified: 10/31/2026
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

libFuzzer targets for the object database. Each input is written to a
file in the current folder and opened like a damaged file would be:

  FUZZ_GXD  a .gxd data file: open (with and without the page cache),
            walk the blocks and objects, read each object, checksums
  FUZZ_BTX  a .btx index file: open, FindFirst/FindNext walk, Find of
            each key found and of a fixed key, the same with a
            BtreeCache
  FUZZ_JNL  a journal: gxdJournal::Recover() of the input. The journal
            names the files it repairs; run this target only in a
            throwaway container or folder.

Build (clang, see tests/fuzz/build_fuzz.sh):
  clang++ -DFUZZ_GXD -fsanitize=fuzzer,address,undefined ... fuzz_db.cpp
Every run must end with an error code or good data: a crash, hang,
sanitizer report or leak is a finding.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include "gxbtree.h"
#include "btcache.h"
#include "gxdjrnl.h"

using namespace datareel;

static const size_t KSIZE = 16;
struct FzKeyData { char k[KSIZE]; FAU addr; };

class FzKey : public DatabaseKeyB
{
public:
  FzKey() : DatabaseKeyB((char *)&d) { memset(&d, 0, sizeof(d)); }
  size_t KeySize() { return sizeof(d); }
  int CompareKey(const DatabaseKeyB &o) const {
    return memcmp(d.k, ((const FzKeyData *)o.db_key)->k, KSIZE);
  }
  int operator==(const DatabaseKeyB &o) const { return CompareKey(o) == 0; }
  int operator>(const DatabaseKeyB &o) const { return CompareKey(o) > 0; }
  FzKeyData d;
};

static void write_file(const char *name, const uint8_t *data, size_t size)
{
  FILE *f = fopen(name, "wb");
  if(!f) return;
  if(size) fwrite(data, 1, size, f);
  fclose(f);
}

#if defined (FUZZ_GXD)
static void walk_gxd(int cache)
{
  gxDatabase db;
  if(db.Open("fz.gxd", gxDBASE_READONLY) != gxDBASE_NO_ERROR) return;
  if(cache) db.EnablePageCache(64 * 1024);
  char buf[4096];
  int n = 0;
  FAU_t a = db.FindFirstObject();
  while(a && n++ < 2000) {
    __ULWORD__ len = db.ObjectLength(a);
    if(len > 0 && len <= sizeof(buf)) db.Read(buf, len, a);
    db.ReadObjectChecksum(a);
    a = db.FindNextObject(a);
  }
  db.TotalBlocks();
  db.Close();
}
#endif

#if defined (FUZZ_BTX)
static void walk_btx(int cached)
{
  FzKey type, key, cmp;
  gxBtree btx(type, 16);
  if(btx.Open("fz.btx", gxDBASE_READONLY) != gxDBASE_NO_ERROR) return;
  BtreeCache *cache = cached ? new BtreeCache(8, &btx) : 0;
  int n = 0;
  int ok = cached ? btx.FindFirst(key, cache) : btx.FindFirst(key);
  while(ok && n++ < 2000) {
    FzKey f = key, c;
    memcpy(&f.d, &key.d, sizeof(f.d));
    f.db_key = &f.d;
    if(cached) btx.Find(f, c, cache); else btx.Find(f, c);
    ok = cached ? btx.FindNext(key, cmp, cache) : btx.FindNext(key, cmp);
  }
  FzKey probe;
  memcpy(probe.d.k, "0000000000000500", KSIZE);
  if(cached) btx.Find(probe, cmp, cache); else btx.Find(probe, cmp);
  delete cache;
  btx.Close();
}
#endif

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
#if defined (FUZZ_GXD)
  write_file("fz.gxd", data, size);
  walk_gxd(0);
  walk_gxd(1);
#elif defined (FUZZ_BTX)
  write_file("fz.btx", data, size);
  walk_btx(0);
  walk_btx(1);
#elif defined (FUZZ_JNL)
  write_file("fz.jnl", data, size);
  unsigned long recovered = 0;
  gxdJournal::Recover("fz.jnl", &recovered);
#else
#error Define FUZZ_GXD, FUZZ_BTX or FUZZ_JNL
#endif
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
