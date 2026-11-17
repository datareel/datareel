// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: gxddigest.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/16/2026
// Date Last Modified: 11/16/2026
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

Database file digests (gxddigest.h). Moved 11/16/2026 from the gxrepl
replication code, unchanged: the same hash and the same digests.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <string>
#include <vector>
#include "gxddigest.h"
#include "gxdbase.h"
#include "gxbtree.h"

namespace datareel {

uint64_t gxDatabaseHash(const void *vp, size_t n, uint64_t h)
{
  const unsigned char *p = (const unsigned char *)vp;
  const uint64_t m = 0x9E3779B97F4A7C15ULL;
  h ^= (uint64_t)n * 0xC2B2AE3D27D4EB4FULL;
  if(n >= 32) {
    uint64_t l0 = h, l1 = h ^ 0x6A09E667F3BCC909ULL,
      l2 = h ^ 0xBB67AE8584CAA73BULL, l3 = h ^ 0x3C6EF372FE94F82BULL;
    while(n >= 32) {
      uint64_t w[4];
      memcpy(w, p, 32);
      l0 = (l0 ^ w[0]) * m; l0 ^= l0 >> 29;
      l1 = (l1 ^ w[1]) * m; l1 ^= l1 >> 29;
      l2 = (l2 ^ w[2]) * m; l2 ^= l2 >> 29;
      l3 = (l3 ^ w[3]) * m; l3 ^= l3 >> 29;
      p += 32; n -= 32;
    }
    h = ((((l0 * m) ^ l1) * m ^ l2) * m ^ l3) * m;
  }
  while(n >= 8) {
    uint64_t w;
    memcpy(&w, p, 8);
    h = (h ^ w) * m;
    h ^= h >> 29;
    p += 8; n -= 8;
  }
  while(n--) {
    h = (h ^ *p++) * m;
    h ^= h >> 29;
  }
  h ^= h >> 32;
  h *= m;
  h ^= h >> 29;
  return h;
}

namespace {
struct DigestKey_ : public DatabaseKeyB {   // any B-tree's keys as raw bytes (the btree digest)
  std::vector<char> b;
  DigestKey_(size_t n) : DatabaseKeyB(0), b(n ? n : 1, 0) { db_key = b.data(); }
  size_t KeySize() { return b.size(); }
  int CompareKey(const DatabaseKeyB &o) const { return memcmp(b.data(), o.db_key, b.size()); }
  int operator==(const DatabaseKeyB &o) const { return CompareKey(o) == 0; }
  int operator>(const DatabaseKeyB &o) const { return CompareKey(o) > 0; }
};
}

int gxDatabaseDigest(const std::string &path, const std::string &kind_in, uint64_t &hash, uint64_t &count, std::string &err)
{
  hash = 0; count = 0;
  std::string kind = kind_in;
  if(kind.empty()) kind = path.size() > 4 && path.compare(path.size() - 4, 4, ".btx") == 0 ? "btree" : "bytes";
  if(kind == "bytes") {
    FILE *f = fopen(path.c_str(), "rb");
    if(!f) { err = "cannot open " + path; return -1; }
    std::vector<unsigned char> b(1 << 20);
    size_t n;
    uint64_t h = 0;
    while((n = fread(b.data(), 1, b.size(), f)) > 0) { h = gxDatabaseHash(b.data(), n, h); count += (uint64_t)n; }
    bool bad = ferror(f) != 0;
    fclose(f);
    hash = h;
    if(bad) { err = "cannot read " + path; return -1; }
    return 0;
  }
  if(kind == "objects") {                    // the live objects, in any order (a multiset of their bytes)
    gxDatabase db;
    if(db.Open(path.c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) { err = "cannot open " + path; return -1; }
    std::vector<uint64_t> hs;
    std::vector<unsigned char> b;
    for(FAU_t a = db.FindFirstObject(); a != (FAU_t)0; a = db.FindNextObject(a)) {
      __ULWORD__ n = db.ObjectLength(a);
      b.resize(n);
      if(n && db.Read(b.data(), n, a) != gxDBASE_NO_ERROR) { err = "cannot read an object"; return -1; }
      hs.push_back(gxDatabaseHash(b.data(), b.size(), 0));
    }
    std::sort(hs.begin(), hs.end());
    uint64_t h = 0;
    for(auto x : hs) h = gxDatabaseHash(&x, sizeof(x), h);
    hash = h; count = hs.size();
    return 0;
  }
  if(kind == "btree") {                      // the keys in order: the same for any node layout
    gxBtreeHeader bh;
    {
      gxDatabase probe;
      if(probe.Open(path.c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) { err = "cannot open " + path; return -1; }
      // 11/15/2026: in the file's layout (gxdbase.h); the key size comes back as it is in memory
      if(gxBtreeReadHeaderDisk(&probe, bh, (FAU_t)probe.FileHeaderSize()) != gxDBASE_NO_ERROR) { err = "no B-tree header"; return -1; }
    }
    // The header's key size is SizeOfDatabaseKey(): the key and its right child pointer (an FAU)
    if(bh.key_size <= sizeof(FAU) || bh.key_size > 65536 || bh.node_order < 2) { err = "not a B-tree index"; return -1; }
    size_t ks = (size_t)bh.key_size - sizeof(FAU);
    DigestKey_ k(ks);
    gxBtree bt(k, bh.node_order);
    if(bt.Open(path.c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) { err = "cannot open the B-tree " + path; return -1; }
    // 11/15/2026: walk the nodes in order (left child, then each key and its right child) instead of
    // FindFirst()/FindNext(). FindNext() finds the next key by searching with the key's compare, and the raw byte
    // compare here is not the tree's own (a key whose name repeats and whose record address breaks the tie sorts by
    // the address's value, not its little-endian bytes): the search lost its place and skipped keys. The node walk
    // needs no compare and gives the tree's own order; where the two orders agree, the digest is unchanged.
    struct Pos { int next; unsigned keys; FAU_t left; std::vector<char> entries; };   // next -1: the left child to walk
    std::vector<Pos> stack;                                    // one read node per level, copied out of the BtreeNode
    const size_t max_depth = 64, entry = (size_t)bh.key_size;
    // Every node holds at least one entry, so a valid tree has at most (file size / entry size) nodes; a damaged
    // file whose children point back up the tree stops there (and at max_depth levels) instead of looping.
    uint64_t max_reads = (uint64_t)gxDatabase::FileSize(path.c_str()) / entry + 2, reads = 0;
    uint64_t h = 0;
    BtreeNode n(bh.key_size, bh.node_order);
    auto push = [&](FAU_t a) -> bool {
      if(a == (FAU_t)0) return true;
      if(stack.size() >= max_depth || ++reads > max_reads) { err = "the B-tree's nodes loop or go too deep: " + path; return false; }
      if(bt.ReadNode(n, a) != gxDBASE_NO_ERROR || (unsigned)n.key_count > (unsigned)n.node_order) {   // the node's own limit: order - 1 keys (an order below 5 is 5)
	err = "cannot read a B-tree node of " + path; return false;
      }
      Pos p;
      p.next = -1; p.keys = (unsigned)n.key_count; p.left = (FAU_t)n.left_child;
      p.entries.assign(n.key_entries, n.key_entries + entry * p.keys);
      stack.push_back(std::move(p));
      return true;
    };
    bool ok = push((FAU_t)bt.Root());
    while(ok && !stack.empty()) {
      Pos &p = stack.back();
      if(p.next == -1) { p.next = 0; FAU_t lc = p.left; ok = push(lc); continue; }
      if(p.next >= (int)p.keys) { stack.pop_back(); continue; }
      const char *e = p.entries.data() + entry * (size_t)p.next;   // an entry: its right child (an FAU), then the key
      FAU rc;
      memcpy(&rc, e, sizeof(FAU));
      memcpy(k.b.data(), e + sizeof(FAU), ks);
      p.next++;
      h = gxDatabaseHash(k.b.data(), k.b.size(), h); count++;
      ok = push((FAU_t)rc);
    }
    bt.Close();
    if(!ok) { hash = 0; count = 0; return -1; }
    // 11/15/2026 (review): the walk must see every key the header counts
    if(count != (uint64_t)bh.n_keys) {
      err = "the B-tree's walk found " + std::to_string(count) + " keys, its header counts " +
	    std::to_string((unsigned long long)bh.n_keys) + ": " + path;
      hash = 0; count = 0; return -1;
    }
    hash = h;
    return 0;
  }
  err = "unknown digest kind " + kind + " (btree, bytes or objects)";
  return -1;
}

} // namespace datareel
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
