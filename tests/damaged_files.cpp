// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: damaged_files.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 10/26/2026
// Date Last Modified: 10/26/2026
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

Damaged file test. Builds a database file, a B-tree index and an
InfoHog database, then many times over flips random bytes in copies
of them and opens, walks and reads the damaged copies with the
uncached and cached B-tree functions, gxDatabase checksums and
InfoHog lookups. Every run must end with an error code or good data,
never a crash or an endless loop. Build it with -fsanitize=address
to find memory errors as well.

Usage: damaged_files [iterations] [seed]
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include "gxbtree.h"
#include "infohog.h"

namespace fs = std::filesystem;

class NameKey : public DatabaseKeyB
{
public:
  NameKey() : DatabaseKeyB((char *)name) { memset(name, 0, sizeof(name)); }
  NameKey(const char *s) : DatabaseKeyB((char *)name) {
    memset(name, 0, sizeof(name));
    strncpy(name, s, sizeof(name) - 1);
  }
  size_t KeySize() { return sizeof(name); }
  int operator==(const DatabaseKeyB &k) const {
    return strncmp(name, (const char *)((const NameKey &)k).name, sizeof(name)) == 0;
  }
  int operator>(const DatabaseKeyB &k) const {
    return strncmp(name, (const char *)((const NameKey &)k).name, sizeof(name)) > 0;
  }
  int CompareKey(const DatabaseKeyB &k) const {
    return strncmp(name, (const char *)((const NameKey &)k).name, sizeof(name));
  }
  char name[24];
};

struct HogName
{
  HogName() { memset(s, 0, sizeof(s)); }
  HogName(const char *p) { memset(s, 0, sizeof(s)); strncpy(s, p, sizeof(s) - 1); }
  int operator==(const HogName &o) const { return strncmp(s, o.s, sizeof(s)) == 0; }
  int operator<(const HogName &o) const { return strncmp(s, o.s, sizeof(s)) < 0; }
  int operator>(const HogName &o) const { return strncmp(s, o.s, sizeof(s)) > 0; }
  char s[32];
};

static std::string name_of(int i) { return "key" + std::to_string(i * 7919 % 1000); }

static void build_files()
{
  gxDatabase db;
  db.Create("t.gxd", (FAU_t)0, 'D'); // Revision D has block checksums
  for(int i = 0; i < 60; i++) {
    std::string rec = "record " + std::to_string(i) + std::string((size_t)(i % 13), '*');
    FAU_t a = db.Alloc((__ULWORD__)rec.size());
    db.Write(rec.data(), (__ULWORD__)rec.size(), a);
    db.WriteObjectChecksum(a);
  }
  db.Close();

  NameKey k;
  gxBtree btx(k, 5);
  btx.Create("t.btx");
  NameKey compare;
  for(int i = 0; i < 300; i++) {
    NameKey key(name_of(i).c_str());
    btx.Insert(key, compare);
  }
  btx.Close();

  POD pod;
  InfoHogKey<HogName> key_type;
  pod.Open("t.ihd", "t.ihx", key_type, InfoHogNodeOrder, gxDBASE_READWRITE,
	   InfoHogUseIndexFile, InfoHogStaticArea, InfoHogNumTrees);
  for(int i = 0; i < 40; i++) {
    HogName n(name_of(i).c_str());
    InfoHog<HogName> h(&pod, n, 4);
    std::string m = "member data " + std::to_string(i);
    h.SetMember(m.c_str(), (unsigned)m.size() + 1, 1);
    h.SetMember(m.c_str(), (unsigned)m.size() + 1, 3);
    h.WriteObject(0);
  }
  pod.Close();
}

static void damage(const char *from, const char *to, std::mt19937 &rng)
{
  std::ifstream in(from, std::ios::binary);
  std::vector<char> data((std::istreambuf_iterator<char>(in)), 
			 std::istreambuf_iterator<char>());
  int flips = 1 + (int)(rng() % 8);
  for(int i = 0; i < flips && !data.empty(); i++) {
    size_t pos = rng() % data.size();
    switch(rng() % 3) {
      case 0: data[pos] = (char)(rng() & 0xFF); break;
      case 1: data[pos] = (char)0xFF; break;
      default: data[pos] ^= (char)(1 << (rng() % 8)); break;
    }
  }
  std::ofstream out(to, std::ios::binary | std::ios::trunc);
  out.write(data.data(), (std::streamsize)data.size());
}

static long walk_database()
{
  gxDatabase db;
  if(db.Open("d.gxd", gxDBASE_READONLY) != gxDBASE_NO_ERROR) return -1;
  long blocks = 0;
  FAU_t addr = db.FindFirstBlock();
  while(addr != (FAU_t)0 && blocks < 100000) {
    FAU_t obj = addr + (FAU_t)db.BlockHeaderSize();
    __ULWORD__ crc1 = 0, crc2 = 0;
    db.ReadObjectChecksum(obj, &crc1, &crc2);
    __ULWORD__ len = db.ObjectLength(obj);
    if(len > 0 && len < 1024*1024) {
      std::vector<char> buf(len);
      db.Read(buf.data(), len, obj);
    }
    blocks++;
    FAU_t next = db.FindFirstBlock(addr + 1);
    if(next <= addr) break;
    addr = next;
  }
  db.Close();
  return blocks;
}

static long walk_btree(bool cached)
{
  NameKey k;
  gxBtree btx(k, 5);
  if(btx.Open("d.btx", gxDBASE_READONLY) != gxDBASE_NO_ERROR) return -1;
  long keys = 0;
  NameKey key, compare;
  if(cached) {
    BtreeCache cache(8, &btx);
    if(btx.FindFirst(key, &cache)) {
      keys++;
      while(btx.FindNext(key, compare, &cache) && keys < 100000) keys++;
    }
    NameKey probe(name_of(17).c_str());
    btx.Find(probe, compare, &cache);
  }
  else {
    if(btx.FindFirst(key)) {
      keys++;
      while(btx.FindNext(key, compare) && keys < 100000) keys++;
    }
    NameKey probe(name_of(17).c_str());
    btx.Find(probe, compare);
    btx.FindLast(key);
  }
  btx.Close();
  return keys;
}

static long walk_infohog()
{
  POD pod;
  InfoHogKey<HogName> key_type;
  if(pod.Open("d.ihd", "d.ihx", key_type, InfoHogNodeOrder, 
	      gxDBASE_READONLY, InfoHogUseIndexFile, InfoHogStaticArea, 
	      InfoHogNumTrees) != gxDBASE_NO_ERROR) return -1;
  long found = 0;
  for(int i = 0; i < 40; i++) {
    HogName n(name_of(i).c_str());
    InfoHog<HogName> h(&pod, n, 4);
    if(h.FindObject()) {
      InfoHog<HogName> r(&pod, 4);
      if(r.ReadObject(h.ObjectAddress())) found++;
    }
  }
  pod.Close();
  return found;
}

int main(int argc, char **argv)
{
  int iterations = argc > 1 ? std::atoi(argv[1]) : 300;
  unsigned seed = argc > 2 ? (unsigned)std::atoi(argv[2]) : 7;
  std::mt19937 rng(seed);

  build_files();
  long base_db = walk_database(), base_bt = 0, base_ih = 0;
  fs::copy_file("t.btx", "d.btx", fs::copy_options::overwrite_existing);
  fs::copy_file("t.gxd", "d.gxd", fs::copy_options::overwrite_existing);
  fs::copy_file("t.ihd", "d.ihd", fs::copy_options::overwrite_existing);
  fs::copy_file("t.ihx", "d.ihx", fs::copy_options::overwrite_existing);
  base_db = walk_database();
  base_bt = walk_btree(false);
  base_ih = walk_infohog();
  std::cout << "Undamaged: " << base_db << " blocks, " << base_bt 
	    << " keys, " << base_ih << " InfoHog objects\n";
  long base_cached = walk_btree(true);
  std::cout << "Cached walk: " << base_cached << " keys\n";
  if(base_db != 60 || base_bt != 300 || base_cached != 300 || base_ih != 40) {
    std::cout << "FAIL: the undamaged files did not read back\n";
    return 1;
  }

  for(int i = 0; i < iterations; i++) {
    damage("t.gxd", "d.gxd", rng);
    damage("t.btx", "d.btx", rng);
    damage("t.ihd", "d.ihd", rng);
    damage("t.ihx", "d.ihx", rng);
    walk_database();
    walk_btree(false);
    walk_btree(true);
    walk_infohog();
  }
  std::cout << "PASS: " << iterations << " damaged copies handled\n";
  return 0;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
