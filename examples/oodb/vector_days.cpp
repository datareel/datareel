// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: vector_days.cpp
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

A vector set across day files (gxvset.h, DataReel 7.4): one .vix per day
in YYYY/MM/DD folders, searched as one through a .vixset manifest.

  1. Five day files (2026-11-09 to 2026-11-13), 500 vectors each, tag 2 =
     the day. A manifest lists them (AddFile): each line keeps the file's
     node counts, its tag 2 range and a CRC of its header.
  2. One search across all the days, with up to 4 threads: every result
     names its file, and the results are the same for any thread count.
  3. A day-range search (11-11 to 11-12): the other days are skipped from
     the manifest alone, without opening them (Skipped(), Opened()).
  4. A day archived (its file moved away): the set skips it and counts it
     in Missing(); with strict set, the search is an error instead.
  5. The live day: a writer keeps its own index open and puts it in the
     set (AttachOpen): searches see its newest vectors at once, and
     Refresh() writes the new counts and range into the manifest.

  vector_days [WORK_FOLDER]
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <string.h>
#include <filesystem>
#include <random>
#include <string>
#include <vector>
#include "gxvindex.h"
#include "gxvset.h"

using namespace datareel;

static const unsigned DIM = 24, PER_DAY = 500;
static int failures = 0;

static void check(bool ok, const char *what)
{
  printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
  if(!ok) failures++;
}

static std::string day_rel(unsigned day)    // 20261111 -> "2026/11/11/vectors.vix"
{
  char b[64];
  snprintf(b, sizeof(b), "%04u/%02u/%02u/vectors.vix", day / 10000, day / 100 % 100, day % 100);
  return b;
}

static void make_vec(std::mt19937 &g, std::vector<float> &v)
{
  std::normal_distribution<float> n(0.0f, 1.0f);
  for(auto &x : v) x = n(g);
}

int main(int argc, char **argv)
{
  std::string dir = argc > 1 ? argv[1] : "vector_days_work";
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir, ec);
  const std::string manifest = dir + "/days.vixset";
  std::mt19937 g(5);
  std::vector<float> v(DIM);

  // 1. Day files and the manifest --------------------------------------------
  printf("1. Five day files and the manifest\n");
  gxVectorSet set;
  if(set.Create(manifest.c_str(), "days", DIM, gxvFLOAT32, gxvL2, "example/days") != gxv_NO_ERROR) {
    printf("cannot create the manifest\n"); return 1;
  }
  long long next_id = 1;
  for(unsigned day = 20261109; day <= 20261113; day++) {
    std::string rel = day_rel(day), full = dir + "/" + rel;
    std::filesystem::create_directories(std::filesystem::path(full).parent_path(), ec);
    gxVectorIndex vix;
    vix.Create(full.c_str(), DIM, gxvFLOAT32, gxvL2, 16, "example/days", 100);
    for(unsigned i = 0; i < PER_DAY; i++) {
      make_vec(g, v);
      unsigned tags[gxvTagCount] = { 0, 0, day, 0 };
      vix.Insert(next_id++, v.data(), tags);
    }
    vix.Close();
    if(set.AddFile(rel.c_str()) != gxv_NO_ERROR) { printf("AddFile: %s\n", set.Error().c_str()); return 1; }
  }
  set.Close();
  printf("  %s:\n", manifest.c_str());
  {
    FILE *f = fopen(manifest.c_str(), "r");
    char line[512];
    while(f && fgets(line, sizeof(line), f)) printf("    %s", line);
    if(f) fclose(f);
  }
  if(set.Open(manifest.c_str(), "example/days") != gxv_NO_ERROR) { printf("open: %s\n", set.Error().c_str()); return 1; }
  check(set.Files().size() == 5, "five files listed");

  // 2. One search across the days ---------------------------------------------
  printf("2. The 8 nearest across all days, 1 and 4 threads\n");
  std::vector<float> q(DIM);
  make_vec(g, q);
  std::vector<gxvSetResult> r1, r4;
  gxvSetSearch o;
  o.k = 8;
  o.threads = 1;
  set.Search(q.data(), o, r1);
  o.threads = 4;
  set.Search(q.data(), o, r4);
  for(auto &x : r1) printf("    id %5lld  %.3f  %s\n", x.id, x.distance, set.Files()[x.file].path.c_str());
  bool same = r1.size() == r4.size();
  for(size_t i = 0; same && i < r1.size(); i++) same = r1[i].id == r4[i].id && r1[i].file == r4[i].file;
  check(same, "the same results with 1 and 4 threads");

  // 3. A day range: files outside it are skipped unopened ---------------------
  printf("3. Days 2026-11-11 to 2026-11-12 only\n");
  {
    gxVectorSet fresh;                               // a new reader: nothing opened yet
    fresh.Open(manifest.c_str());
    gxvSetSearch d;
    d.k = 8;
    d.day_range = 1;
    d.day_lo = 20261111;
    d.day_hi = 20261112;
    std::vector<gxvSetResult> r;
    gxvSetStats st;
    fresh.Search(q.data(), d, r, &st);
    bool in_range = !r.empty();
    for(auto &x : r) in_range = in_range && x.tags[2] >= 20261111 && x.tags[2] <= 20261112;
    printf("  %zu results, %llu file(s) skipped from the manifest, %llu opened\n", r.size(), st.skipped,
	   fresh.Opened());
    check(in_range, "every result is from the two days");
    check(st.skipped == 3, "the other three days were never opened");
  }

  // 4. An archived day ----------------------------------------------------------
  printf("4. 2026-11-09 archived (moved away)\n");
  {
    set.Close();                                     // Windows cannot move a file that is open: close the set first
    std::filesystem::create_directories(dir + "/archive", ec);
    std::filesystem::rename(dir + "/" + day_rel(20261109), dir + "/archive/20261109.vix", ec);
    if(ec) printf("  cannot move the day away: %s\n", ec.message().c_str());
    gxVectorSet reader;
    reader.Open(manifest.c_str());
    gxvSetSearch s;
    s.k = 8;
    std::vector<gxvSetResult> r;
    gxvSetStats st;
    gxvError e = reader.Search(q.data(), s, r, &st);
    printf("  search: %s, %zu results, %llu missing\n", gxVectorIndex::ErrorMessage(e), r.size(), st.missing);
    check(e == gxv_NO_ERROR && st.missing == 1, "the missing day is skipped and counted");
    s.strict = 1;
    e = reader.Search(q.data(), s, r, &st);
    check(e != gxv_NO_ERROR, "with strict set, a missing day is an error");
    reader.Close();
    std::filesystem::rename(dir + "/archive/20261109.vix", dir + "/" + day_rel(20261109), ec);
    if(set.Open(manifest.c_str(), "example/days") != gxv_NO_ERROR) { printf("reopen: %s\n", set.Error().c_str()); return 1; }
  }

  // 5. The live day: a writer's own index in the set -------------------------
  printf("5. The live day 2026-11-14, written while the set searches it\n");
  {
    std::string rel = day_rel(20261114), full = dir + "/" + rel;
    std::filesystem::create_directories(std::filesystem::path(full).parent_path(), ec);
    gxVectorIndex live;
    live.Create(full.c_str(), DIM, gxvFLOAT32, gxvL2, 16, "example/days", 100);
    set.AddFile(rel.c_str());
    set.AttachOpen(rel.c_str(), &live);              // searched as it is, never reopened by the set
    std::vector<float> target(DIM, 3.0f);            // far from the other days' vectors
    unsigned tags[gxvTagCount] = { 0, 0, 20261114, 0 };
    live.Insert(999999, target.data(), tags);
    gxvSetSearch s;
    s.k = 1;
    std::vector<gxvSetResult> r;
    set.Search(target.data(), s, r);
    check(!r.empty() && r[0].id == 999999, "the writer's newest vector is found at once");
    set.Refresh();                                   // the new counts and tag 2 range into the manifest
    const gxvSetFile &lf = set.Files().back();
    printf("  manifest line for %s: nodes %llu, tag2 %u..%u\n", lf.path.c_str(), lf.nodes, lf.tag2_min, lf.tag2_max);
    check(lf.nodes == 1 && lf.tag2_min == 20261114, "Refresh() wrote the live day's counts and range");
    set.AttachOpen(rel.c_str(), 0);
    live.Close();
  }
  set.Close();

  printf(failures ? "%d check(s) failed\n" : "All checks pass\n", failures);
  return failures ? 1 : 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
