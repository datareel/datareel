// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: vector_basics.cpp
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

The vector index (gxvindex.h, DataReel 7.4), step by step:

  1. Three .vix files of the same 4,000 clustered vectors (dimension 32),
     one per element type: float32, float16 and int8 (a scale per vector).
     Their sizes show what each type costs on disk.
  2. Exact() against Search() (the HNSW graph): recall@10 of the graph
     search for three search widths (ef), and how long each took.
  3. The three metrics on one small index: l2 (squared distance),
     cosine (the vectors kept at unit length) and ip (inner product).
  4. Tags and a gxvFilter: tag 2 holds a day (yyyymmdd), tag 0 a
     category; a search that keeps one category in a range of days.
  5. Update, Delete (a tombstone: Count() and Deleted()), Repair() (the
     graph made again from the live nodes) and Verify().
  6. A gxdJournal transaction rolled back: the index's file is attached
     to the journal; after Rollback() the program calls Reload() so the
     nodes in memory match the file again.
  7. The phase D format fields of a file: the required and optional
     feature bits, the codec, the graph layout, the tag 2 range and the
     logical digest (what the index holds, whatever its layout).

The files go in the work folder (default vector_basics_work). Exit code 0
when every check passes.

  vector_basics [WORK_FOLDER]
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <chrono>
#include <filesystem>
#include <random>
#include <set>
#include <string>
#include <vector>
#include "gxdjrnl.h"
#include "gxvindex.h"

using namespace datareel;

static const unsigned DIM = 32, N = 4000, CLUSTERS = 40;
static int failures = 0;

static void check(bool ok, const char *what)
{
  printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
  if(!ok) failures++;
}

// Clustered vectors: CLUSTERS centers, each vector a center plus noise
struct Data {
  std::vector<std::vector<float> > centers, vecs;
  Data(unsigned n, unsigned seed) {
    std::mt19937 g(seed);
    std::normal_distribution<float> c(0.0f, 1.0f), noise(0.0f, 0.15f);
    centers.resize(CLUSTERS, std::vector<float>(DIM));
    for(auto &v : centers) for(auto &x : v) x = c(g);
    vecs.resize(n, std::vector<float>(DIM));
    for(unsigned i = 0; i < n; i++)
      for(unsigned d = 0; d < DIM; d++) vecs[i][d] = centers[i % CLUSTERS][d] + noise(g);
  }
};

static const char *type_name(gxvElementType t)
{
  return t == gxvFLOAT32 ? "float32" : t == gxvFLOAT16 ? "float16" : "int8";
}

static double ms_since(std::chrono::steady_clock::time_point t0)
{
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

int main(int argc, char **argv)
{
  std::string dir = argc > 1 ? argv[1] : "vector_basics_work";
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir, ec);
  Data data(N, 2026);

  // 1. Three element types --------------------------------------------------
  printf("1. %u vectors of dimension %u, three element types\n", N, DIM);
  const gxvElementType types[3] = { gxvFLOAT32, gxvFLOAT16, gxvINT8 };
  for(gxvElementType t : types) {
    std::string f = dir + "/basics_" + type_name(t) + ".vix";
    gxVectorIndex vix;
    if(vix.Create(f.c_str(), DIM, t, gxvL2, 16, "example/basics", 200) != gxv_NO_ERROR) {
      printf("cannot create %s\n", f.c_str()); return 1;
    }
    auto t0 = std::chrono::steady_clock::now();
    for(unsigned i = 0; i < N; i++) {
      unsigned tags[gxvTagCount] = { i % 3, 0, 20261110 + i % 5, 0 };   // tag 0: category, tag 2: day
      if(vix.Insert((long long)i, data.vecs[i].data(), tags) != gxv_NO_ERROR) { printf("insert failed\n"); return 1; }
    }
    double build = ms_since(t0);
    unsigned node = vix.NodeSize(), code = vix.CodeSize();   // read before Close()
    vix.Close();
    printf("  %-7s  %6.0f ms to build, %7llu bytes, %u bytes a node (%u of them the vector)\n", type_name(t), build,
	   (unsigned long long)std::filesystem::file_size(f, ec), node, code);
  }

  // 2. Exact against the graph ----------------------------------------------
  printf("2. Exact() against Search() (float32), 100 queries, recall@10\n");
  {
    gxVectorIndex vix;
    if(vix.Open((dir + "/basics_float32.vix").c_str(), gxDBASE_READONLY) != gxv_NO_ERROR) { printf("open failed\n"); return 1; }
    Data queries(100, 77);
    std::vector<std::vector<gxvResult> > truth(100);
    auto t0 = std::chrono::steady_clock::now();
    for(unsigned q = 0; q < 100; q++) vix.Exact(queries.vecs[q].data(), 10, truth[q]);
    printf("  exact:   %6.1f ms\n", ms_since(t0));
    double best = 0;
    for(unsigned ef : { 16u, 64u, 200u }) {
      unsigned hit = 0;
      auto t1 = std::chrono::steady_clock::now();
      for(unsigned q = 0; q < 100; q++) {
	std::vector<gxvResult> r;
	vix.Search(queries.vecs[q].data(), 10, r, ef);
	std::set<long long> want;
	for(auto &x : truth[q]) want.insert(x.id);
	for(auto &x : r) hit += want.count(x.id);
      }
      double recall = hit / 1000.0;
      if(recall > best) best = recall;
      printf("  ef %3u:  %6.1f ms, recall %.3f\n", ef, ms_since(t1), recall);
    }
    check(best >= 0.95, "the graph search finds at least 95% of the exact top 10 at the widest ef");
  }

  // 3. Three metrics --------------------------------------------------------
  printf("3. Metrics on four 2-D vectors, query (1, 0)\n");
  {
    const float pts[4][2] = { { 1, 0 }, { 10, 0 }, { 0, 1 }, { 0.7f, 0.7f } };
    const gxvMetric metrics[3] = { gxvL2, gxvCOSINE, gxvIP };
    const char *names[3] = { "l2", "cosine", "ip" };
    for(int m = 0; m < 3; m++) {
      std::string f = dir + "/metric_" + names[m] + ".vix";
      gxVectorIndex vix;
      vix.Create(f.c_str(), 2, gxvFLOAT32, metrics[m]);
      for(int i = 0; i < 4; i++) vix.Insert(i, pts[i]);
      float q[2] = { 1, 0 };
      std::vector<gxvResult> r;
      vix.Exact(q, 4, r);
      printf("  %-6s  order:", names[m]);
      for(auto &x : r) printf(" %lld (%.3f)", x.id, x.distance);
      printf("\n");
      // l2: (1,0) first; cosine: (1,0) and (10,0) tie at the top (same direction); ip: (10,0) first (largest dot)
      if(m == 0) check(r[0].id == 0, "l2: the nearest point is (1, 0)");
      if(m == 1) check((r[0].id == 0 || r[0].id == 1) && fabsf(r[0].distance - r[1].distance) < 1e-5f,
		       "cosine: (1, 0) and (10, 0) are the same direction");
      if(m == 2) check(r[0].id == 1, "ip: (10, 0) has the largest inner product");
      vix.Close();
    }
  }

  // 4. Tags and a filter ----------------------------------------------------
  printf("4. A filter: category 1, days 2026-11-11 to 2026-11-12\n");
  {
    gxVectorIndex vix;
    vix.Open((dir + "/basics_float32.vix").c_str(), gxDBASE_READONLY);
    gxvFilter f;
    f.Tag(0, 1);
    f.Range(2, 20261111, 20261112);
    std::vector<gxvResult> r;
    vix.Search(data.vecs[7].data(), 10, r, 64, &f);
    bool all = !r.empty();
    for(auto &x : r) all = all && x.tags[0] == 1 && x.tags[2] >= 20261111 && x.tags[2] <= 20261112;
    printf("  %zu results, first id %lld (category %u, day %u)\n", r.size(), r.empty() ? -1LL : r[0].id,
	   r.empty() ? 0 : r[0].tags[0], r.empty() ? 0 : r[0].tags[2]);
    check(all, "every result is category 1 in the two days");
  }

  // 5. Update, Delete, Repair, Verify ---------------------------------------
  printf("5. Update, Delete, Repair and Verify\n");
  {
    gxVectorIndex vix;
    vix.Open((dir + "/basics_float32.vix").c_str());
    unsigned tags[gxvTagCount] = { 2, 0, 20261114, 0 };
    vix.Update(5, data.vecs[6].data(), tags);                      // a new vector: the old node is replaced
    for(long long id = 100; id < 200; id++) vix.Delete(id);         // tombstones
    printf("  live %llu, deleted %llu\n", vix.Count(), vix.Deleted());
    check(!vix.Contains(150) && vix.Contains(5), "a deleted id is gone, an updated one is there");
    check(vix.Repair() == gxv_NO_ERROR, "Repair() makes the graph again from the live nodes");
    std::vector<long long> bad;
    std::string report;
    check(vix.Verify(0, 0, &bad, &report) == 0, "Verify() finds no problem");
    vix.Close();
  }

  // 6. A rolled back transaction and Reload() -------------------------------
  printf("6. A gxdJournal transaction rolled back, then Reload()\n");
  {
    gxdJournal j;
    gxVectorIndex vix;
    j.Open((dir + "/basics.jnl").c_str());
    vix.Open((dir + "/basics_float16.vix").c_str());
    j.Attach(vix.Database());
    unsigned long long before = vix.Count();
    j.Begin();
    for(long long id = 50000; id < 50010; id++) vix.Insert(id, data.vecs[id % N].data());
    printf("  in the transaction: %llu live\n", vix.Count());
    j.Rollback();
    vix.Reload();                                                   // the file was restored: read it again
    printf("  after Rollback() and Reload(): %llu live\n", vix.Count());
    check(vix.Count() == before && !vix.Contains(50003), "the rolled back inserts are gone");
    j.Close();
    vix.Close();
  }

  // 7. Format fields and the logical digest ---------------------------------
  printf("7. Format fields (phase D)\n");
  {
    gxVectorIndex a, b;
    a.Open((dir + "/basics_int8.vix").c_str(), gxDBASE_READONLY);
    unsigned lo = 0, hi = 0;
    int has = a.TagRange(2, lo, hi);
    printf("  required 0x%x, optional 0x%x, codec %u, graph layout %u, tag 2 %s %u..%u\n", a.FeaturesRequired(),
	   a.FeaturesOptional(), a.Codec(), a.GraphLayout(), has ? "range" : "none", lo, hi);
    check(has && lo == 20261110 && hi == 20261114, "the file keeps the range of tag 2 (a set can skip it)");
    // the same nodes in another file, inserted in reverse order: another graph, the same logical digest
    std::string f = dir + "/basics_int8_reverse.vix";
    b.Create(f.c_str(), DIM, gxvINT8, gxvL2, 16, "example/basics", 200);
    for(unsigned i = N; i-- > 0; ) {
      unsigned tags[gxvTagCount] = { i % 3, 0, 20261110 + i % 5, 0 };
      b.Insert((long long)i, data.vecs[i].data(), tags);
    }
    unsigned long long na = 0, nb = 0, da = a.LogicalDigest(&na), db = b.LogicalDigest(&nb);
    printf("  logical digest %016llx (%llu nodes), reverse order %016llx (%llu nodes)\n", da, na, db, nb);
    check(da == db && na == nb, "the same nodes give the same logical digest, whatever the graph");
    a.Close();
    b.Close();
  }

  printf(failures ? "%d check(s) failed\n" : "All checks pass\n", failures);
  return failures ? 1 : 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
