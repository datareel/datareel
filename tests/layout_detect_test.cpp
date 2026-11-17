// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: layout_detect_test.cpp
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

DataReel 7.4: gxDatabase::Open() and the file layout detection.

  engine     A file of the other database engine ("GXDBASE64" for a
             32-bit file address build, "GXDBASE" for a 64-bit one) is
             refused with gxDBASE_FILE_ENGINE_ERROR. Before, the 32-bit
             engine compared only "GXDBASE" and opened 64-bit files.
  damaged    A file of this build whose first block is damaged (its
             length zero or random, or the high half of its check word
             random) is still read in this build's layout, never taken
             for the old 4-byte layout.
  old        With OLD_LAYOUT_DIR (tests/data/old_layout/win64), a sound
             old-layout file is still found to be the old layout.

Usage: layout_detect_test WORK_DIR [OLD_LAYOUT_DIR]
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#include "gxdbase.h"
#include "dbugmgr.h"
#include "../utils/db_debug/db_walk.h"

using namespace datareel;

static int failures = 0;
#define CHECK(cond, what) do { if(!(cond)) { std::cout << "FAIL " << what << "\n"; failures++; } } while(0)

static std::vector<unsigned char> read_file(const std::string &f)
{
  std::ifstream in(f, std::ios::binary);
  return std::vector<unsigned char>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}
static void write_file(const std::string &f, const std::vector<unsigned char> &v)
{
  std::ofstream out(f, std::ios::binary | std::ios::trunc);
  out.write((const char *)v.data(), (std::streamsize)v.size());
}
static unsigned long long get_le(const unsigned char *p, int n)
{
  unsigned long long v = 0;
  for(int i = n - 1; i >= 0; i--) v = (v << 8) | p[i];
  return v;
}
static void put_le(unsigned char *p, int n, unsigned long long v)
{
  for(int i = 0; i < n; i++) p[i] = (unsigned char)(v >> (8 * i));
}

int main(int argc, char **argv)
{
  if(argc < 2) {
    std::cerr << "Usage: " << argv[0] << " WORK_DIR [OLD_LAYOUT_DIR]\n";
    return 2;
  }
  std::string work = argv[1], base = work + "/layout_base.gxd";
  int fau = 0, lng = 0;
  {
    gxDatabase db;
    CHECK(db.Create(base.c_str()) == gxDBASE_NO_ERROR, "create");
    for(int i = 0; i < 6; i++) {
      char rec[100];
      memset(rec, 'a' + i, sizeof(rec));
      FAU_t a = db.Alloc(sizeof(rec));
      CHECK(a != (FAU_t)0 && db.Write(rec, sizeof(rec), a) == gxDBASE_NO_ERROR, "write");
    }
    fau = db.DiskFAUSize();
    lng = db.DiskLongSize();
    db.Close();
  }
  CHECK(lng == 8, "a new file is not in the canonical layout");
  std::vector<unsigned char> img = read_file(base);
  size_t sig_at = (size_t)(4 * fau);                     // the signature follows 4 file addresses
  CHECK(img.size() > sig_at + 16, "file too small");

  // 1. The other engine's signature
  {
    std::vector<unsigned char> v = img;
    const char *other;
#if defined (__64_BIT_DATABASE_ENGINE__)
    other = "GXDBASEE";                                  // a 32-bit file address engine's file
#else
    other = "GXDBASE64E";                                // a 64-bit engine's file
#endif
    memcpy(v.data() + sig_at, other, strlen(other));
    std::string f = work + "/layout_engine.gxd";
    write_file(f, v);
    gxDatabase db;
    gxDatabaseError e = db.Open(f.c_str(), gxDBASE_READONLY);
    CHECK(e == gxDBASE_FILE_ENGINE_ERROR, std::string("the other engine's file (") + other + ") was not refused as such: " +
	  db.DatabaseExceptionMessage());
    std::cout << (e == gxDBASE_FILE_ENGINE_ERROR ? "PASS" : "FAIL") << " engine: " << other << ": "
	      << db.DatabaseExceptionMessage() << "\n";
  }

  // 2. A damaged first block: this build's layout, never the old one
  {
    size_t hs_at = (size_t)(2 * fau);
    unsigned long long hs = get_le(img.data() + hs_at, fau);
    CHECK(hs > 0 && hs + 32 < img.size(), "heap start");
    struct Damage { const char *what; size_t off; int n; unsigned long long v; };
    const Damage damage[] = {
      { "first block length zero", (size_t)hs + (size_t)lng, lng, 0 },
      { "first block length random", (size_t)hs + (size_t)lng, lng, 0x5a3c1f27ULL },
      { "check word's high half random", (size_t)hs + 4, 4, 0x13579bdfULL },
    };
    for(auto &d : damage) {
      std::vector<unsigned char> v = img;
      put_le(v.data() + d.off, d.n, d.v);
      std::string f = work + "/layout_damaged.gxd";
      write_file(f, v);
      gxDatabase db;
      gxDatabaseError e = db.Open(f.c_str(), gxDBASE_READONLY);
      bool ok = e == gxDBASE_NO_ERROR && db.DiskLongSize() == lng;
      CHECK(ok, std::string(d.what) + ": opened as the " + std::to_string(db.DiskLongSize()) + "-byte layout (" +
	    db.DatabaseExceptionMessage() + ")");
      std::cout << (ok ? "PASS" : "FAIL") << " damaged: " << d.what << ": layout " << db.DiskLongSize()
		<< (db.LayoutGuessed() ? " (guessed)" : "") << "\n";
    }
  }

  // 3. A sound old-layout file is still the old layout
  if(argc > 2) {
    std::string f = std::string(argv[2]) + "/data.gxd";
    gxDatabase db;
    gxDatabaseError e = db.Open(f.c_str(), gxDBASE_READONLY);
    bool ok = e == gxDBASE_NO_ERROR && db.DiskLongSize() == 4 && !db.LayoutGuessed();
    CHECK(ok, std::string("the old-layout file: ") + db.DatabaseExceptionMessage() + ", layout " +
	  std::to_string(db.DiskLongSize()));
    std::cout << (ok ? "PASS" : "FAIL") << " old: " << f << ": layout " << db.DiskLongSize() << "\n";
  }

  // 4. 11/17/2026 (7.4 beta5): db_debug's block search and walk on the old-layout file. BlockSearch() copied
  // DiskBlockHdrSize() bytes into the struct in memory: wrong whenever the two sizes differ (an old file on 64-bit
  // Linux, a canonical file on Windows, where it overran the struct), so no block was found. A copy of the file,
  // opened as db_debug opens it; every block must be found and good, as many as gxDatabase counts
  if(argc > 2) {
    std::string src = std::string(argv[2]) + "/data.gxd", f = work + "/layout_walk.gxd";
    std::ifstream in(src, std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    write_file(f, bytes);
    FAU_t blocks = 0;
    {
      gxDatabase db;
      if(db.Open(f.c_str(), gxDBASE_READONLY) == gxDBASE_NO_ERROR) blocks = db.TotalBlocks();
    }
    gxDatabaseDebugManager dm;
    FAU_t first = 0, good = 0, bad = 0;
    bool walked = false;
    if(dm.BlindOpen(f.c_str()) == gxDBASE_NO_ERROR) {
      first = dm.BlockSearch((FAU_t)0);
      walked = gxdWalkBlocks(&dm, good, bad);
    }
    bool ok = blocks > 0 && first > 0 && walked && good == blocks && bad == 0;
    CHECK(ok, "db_debug walk of the old-layout file: blocks " + std::to_string((long long)blocks) + ", first " +
	  std::to_string((long long)first) + ", good " + std::to_string((long long)good) + ", bad " +
	  std::to_string((long long)bad));
    std::cout << (ok ? "PASS" : "FAIL") << " db_debug walk (old layout): " << (long long)good << " of "
	      << (long long)blocks << " blocks\n";
    dm.Close();
    remove(f.c_str());
  }

  remove(base.c_str());
  remove((work + "/layout_engine.gxd").c_str());
  remove((work + "/layout_damaged.gxd").c_str());
  if(failures) {
    std::cout << failures << " layout detection checks failed\n";
    return 1;
  }
  std::cout << "All layout detection checks passed\n";
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
