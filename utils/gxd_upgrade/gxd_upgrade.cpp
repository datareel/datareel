// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: gxd_upgrade.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/15/2026
// Date Last Modified: 11/15/2026
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

gxd_upgrade (DataReel 7.4): a gxDatabase data file in the old 4-byte layout (written by 64-bit Windows or a 32-bit
build before 7.4, gxdbase.h) rewritten as a new file in the canonical layout. Only when it is run: DataReel opens,
reads and writes old-layout files in their own layout, and never converts one by itself.

  gxd_upgrade IN.gxd OUT.gxd [--map MAP.txt] [--records raw|infohog]

- Every object is copied in address order into OUT (a new file: refused if it exists), with IN's revision letter
  and static area (its bytes as they are). The block headers are canonical, so object addresses change: MAP.txt
  (default OUT.gxd.map) lists "OLD NEW" for every object. Indexes that hold record addresses (B-trees, vector
  indexes) are rebuilt from OUT afterwards (the application's own rebuild; vix_rebuild for vector members), or the
  application remaps them through MAP.txt.
- --records raw (the default): each object's bytes as they are. --records infohog: the objects are InfoHog
  records (POD data files), whose object header, member count and lengths are fields of the file's layout:
  those are rewritten canonical, the members' bytes as they are.
- After the copy every object is read back from OUT and compared with IN's (the member bytes for infohog).
- A B-tree index (.btx) or a vector index (.vix) is refused: its nodes hold another file's record addresses.
- IN is opened read only and never changed. Exit 0: done (or IN was already canonical: nothing written); 1: an
  error; 2: refused (usage, an index file, OUT exists).
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include "gxdbase.h"

using namespace datareel;

static bool ends_with(const std::string &s, const char *e)
{
  size_t n = strlen(e);
  return s.size() >= n && s.compare(s.size() - n, n, e) == 0;
}

static unsigned long long get_le(const unsigned char *p, size_t w)
{
  unsigned long long v = 0;
  for(size_t i = w; i-- > 0; ) v = (v << 8) | p[i];
  return v;
}

static void put_le(unsigned char *p, size_t w, unsigned long long v)
{
  for(size_t i = 0; i < w; i++) p[i] = (unsigned char)(v >> (8 * i));
}

// An InfoHog record of layout (L longs, F FAUs) into the canonical one: the object header (two FAUs), the member
// count, then each member's length and bytes. 0 and why when it is not an InfoHog record.
static int infohog_canonical(const std::vector<unsigned char> &in, size_t L, size_t F, std::vector<unsigned char> &out,
			     std::vector<std::vector<unsigned char> > &members, std::string &why)
{
  size_t off = 2 * F;
  if(in.size() < off + L) { why = "too short for an InfoHog record"; return 0; }
  unsigned long long cls = get_le(in.data(), F), oid = get_le(in.data() + F, F);
  if(F == 4) { cls = (unsigned long long)(long long)(int32_t)(uint32_t)cls; oid = (unsigned long long)(long long)(int32_t)(uint32_t)oid; }
  unsigned long long num = get_le(in.data() + off, L);
  off += L;
  if(num == 0 || num > 65536) { why = "a member count of " + std::to_string(num); return 0; }
  members.clear();
  for(unsigned long long i = 0; i < num; i++) {
    if(in.size() - off < L) { why = "a member length past the record"; return 0; }
    unsigned long long len = get_le(in.data() + off, L);
    off += L;
    if(len > in.size() - off) { why = "a member past the record"; return 0; }
    members.emplace_back(in.begin() + (long)off, in.begin() + (long)(off + len));
    off += (size_t)len;
  }
  out.assign(16 + 8, 0);
  put_le(out.data(), 8, cls);
  put_le(out.data() + 8, 8, oid);
  put_le(out.data() + 16, 8, num);
  for(auto &m : members) {
    size_t at = out.size();
    out.resize(at + 8 + m.size());
    put_le(out.data() + at, 8, m.size());
    if(!m.empty()) memcpy(out.data() + at + 8, m.data(), m.size());
  }
  return 1;
}

int main(int argc, char **argv)
{
  if(argc < 3) {
    fprintf(stderr, "usage: gxd_upgrade IN.gxd OUT.gxd [--map MAP.txt] [--records raw|infohog]\n");
    return 2;
  }
  std::string in_path = argv[1], out_path = argv[2], map_path = out_path + ".map", records = "raw";
  for(int i = 3; i < argc; i++) {
    std::string a = argv[i];
    if(a == "--map" && i + 1 < argc) map_path = argv[++i];
    else if(a == "--records" && i + 1 < argc) records = argv[++i];
    else { fprintf(stderr, "gxd_upgrade: unknown option %s\n", a.c_str()); return 2; }
  }
  if(records != "raw" && records != "infohog") { fprintf(stderr, "gxd_upgrade: --records raw or infohog\n"); return 2; }
  if(ends_with(in_path, ".btx") || ends_with(in_path, ".vix")) {
    fprintf(stderr, "gxd_upgrade: %s is an index (%s): its nodes hold the addresses of another file's records, which "
	    "an upgrade of that file changes. Upgrade the data file, then rebuild this index from it (the "
	    "application's rebuild; vix_rebuild for vector members).\n", in_path.c_str(),
	    ends_with(in_path, ".btx") ? "a B-tree" : "a vector index");
    return 2;
  }
  if(gxDatabase::Exists(out_path.c_str())) {
    fprintf(stderr, "gxd_upgrade: %s exists (never overwritten)\n", out_path.c_str());
    return 2;
  }
  gxDatabase in;
  if(in.Open(in_path.c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) {
    fprintf(stderr, "gxd_upgrade: cannot open %s: %s\n", in_path.c_str(), in.DatabaseExceptionMessage());
    return 1;
  }
  {
    unsigned char m[8];                          // a vector index has its magic at the static area
    if(in.StaticArea() >= 8 && in.Read(m, 8, (FAU_t)in.FileHeaderSize()) == gxDBASE_NO_ERROR &&
       memcmp(m, "GXVIX74", 7) == 0) {
      fprintf(stderr, "gxd_upgrade: %s is a vector index: rebuild it from the upgraded data file (vix_rebuild)\n",
	      in_path.c_str());
      return 2;
    }
  }
  if(!in.IsOldLayout()) {
    printf("%s is in the canonical layout already: nothing to do\n", in_path.c_str());
    return 0;
  }
  const size_t L = (size_t)in.DiskLongSize(), F = (size_t)in.DiskFAUSize();
  FAU_t static_size = in.StaticArea();
  gxDatabase out;
  if(out.Create(out_path.c_str(), static_size, in.GetRevLetter()) != gxDBASE_NO_ERROR) {
    fprintf(stderr, "gxd_upgrade: cannot create %s: %s\n", out_path.c_str(), out.DatabaseExceptionMessage());
    return 1;
  }
  if(static_size > 0) {                          // the application's static area, its bytes as they are
    std::vector<unsigned char> sa((size_t)static_size);
    if(in.Read(sa.data(), (__ULWORD__)sa.size(), (FAU_t)in.FileHeaderSize()) != gxDBASE_NO_ERROR ||
       out.Write(sa.data(), (__ULWORD__)sa.size(), (FAU_t)out.FileHeaderSize()) != gxDBASE_NO_ERROR) {
      fprintf(stderr, "gxd_upgrade: cannot copy the static area\n");
      return 1;
    }
  }
  FILE *mf = fopen(map_path.c_str(), "w");
  if(!mf) { fprintf(stderr, "gxd_upgrade: cannot write %s\n", map_path.c_str()); return 1; }
  fprintf(mf, "# gxd_upgrade %s -> %s: OLD NEW object addresses\n", in_path.c_str(), out_path.c_str());
  std::vector<std::pair<FAU_t, FAU_t> > map;
  std::vector<unsigned char> b, conv;
  std::vector<std::vector<unsigned char> > members;
  int crc = in.GetRevLetter() >= 'A' && in.GetRevLetter() <= 'D';
  for(FAU_t a = in.FindFirstObject(); a != (FAU_t)0; a = in.FindNextObject(a)) {
    __ULWORD__ n = in.ObjectLength(a);
    b.resize(n);
    if(n && in.Read(b.data(), n, a) != gxDBASE_NO_ERROR) {
      fprintf(stderr, "gxd_upgrade: cannot read the object at %lld\n", (long long)a); return 1;
    }
    const std::vector<unsigned char> *w = &b;
    if(records == "infohog") {
      std::string why;
      if(!infohog_canonical(b, L, F, conv, members, why)) {
	fprintf(stderr, "gxd_upgrade: the object at %lld is not an InfoHog record (%s)\n", (long long)a, why.c_str());
	return 1;
      }
      // the object id is the record's address: the new one
      w = &conv;
    }
    FAU_t na = out.Alloc((__ULWORD__)w->size());
    if(na == (FAU_t)0) { fprintf(stderr, "gxd_upgrade: cannot allocate in %s\n", out_path.c_str()); return 1; }
    if(records == "infohog") put_le(conv.data() + 8, 8, (unsigned long long)(long long)na);
    if(!w->empty() && out.Write(w->data(), (__ULWORD__)w->size(), na) != gxDBASE_NO_ERROR) {
      fprintf(stderr, "gxd_upgrade: cannot write %s\n", out_path.c_str()); return 1;
    }
    if(crc) out.WriteObjectChecksum(na);
    map.push_back(std::make_pair(a, na));
    fprintf(mf, "%lld %lld\n", (long long)a, (long long)na);
  }
  fclose(mf);
  out.Close();
  // the byte check: every object of OUT against IN's (the members, for InfoHog records)
  gxDatabase chk;
  if(chk.Open(out_path.c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR || chk.IsOldLayout()) {
    fprintf(stderr, "gxd_upgrade: %s does not open as a canonical file\n", out_path.c_str()); return 1;
  }
  unsigned long long objects = 0, bytes = 0;
  std::vector<unsigned char> x;
  std::vector<std::vector<unsigned char> > m2;
  for(auto &p : map) {
    __ULWORD__ n = in.ObjectLength(p.first);
    b.resize(n);
    if(n && in.Read(b.data(), n, p.first) != gxDBASE_NO_ERROR) { fprintf(stderr, "gxd_upgrade: check: cannot read IN\n"); return 1; }
    __ULWORD__ n2 = chk.ObjectLength(p.second);
    x.resize(n2);
    if(n2 && chk.Read(x.data(), n2, p.second) != gxDBASE_NO_ERROR) { fprintf(stderr, "gxd_upgrade: check: cannot read OUT\n"); return 1; }
    bool same;
    if(records == "infohog") {
      std::string why;
      same = infohog_canonical(b, L, F, conv, members, why) && infohog_canonical(x, 8, 8, conv, m2, why) && members == m2;
    }
    else same = b == x;
    if(!same) {
      fprintf(stderr, "gxd_upgrade: check: the object at %lld (now %lld) differs\n", (long long)p.first, (long long)p.second);
      return 1;
    }
    objects++;
    bytes += n;
  }
  chk.Close();
  in.Close();
  printf("%s (old %u-byte layout) -> %s (canonical): %llu objects, %llu bytes, byte check ok; addresses in %s\n",
	 in_path.c_str(), (unsigned)L, out_path.c_str(), objects, bytes, map_path.c_str());
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
