// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: dr_bench.cpp
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

The numbers on docs/benchmarks.html (DataReel 7.4): one program, one
work folder, fixed random seeds. Single threaded unless a line says
otherwise.

  1. gxDatabase: fixed 128-byte records appended (Alloc + Write), then
     read back at random addresses.
  2. gxdJournal: commits of 10 records, durable (Commit(1): the journal
     and the data synced before it returns) and not (Commit(0)).
  3. gxBtree: 64-bit keys inserted in random order, found at random,
     walked in order.
  4. gxVectorIndex: clustered vectors of dimension 96 (cosine) in
     float32, float16 and int8 files: build rate, file size, Exact()
     time, and Search() latency and recall@10 at three search widths.
  5. gxVectorSet: ten day files searched as one (1 and 4 threads), and
     one day of them (the other nine skipped from the manifest).

  dr_bench [WORK_FOLDER] [--quick]
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <random>
#include <set>
#include <string>
#include <vector>
#include "gxdbase.h"
#include "gxbtree.h"
#include "gxdjrnl.h"
#include "gxvindex.h"
#include "gxvset.h"

using namespace datareel;
typedef std::chrono::steady_clock Clock;

static double ms_since(Clock::time_point t0)
{
  return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

static double pct(std::vector<double> v, double p)
{
  if(v.empty()) return 0;
  std::sort(v.begin(), v.end());
  size_t i = (size_t)(p * (double)(v.size() - 1) + 0.5);
  return v[std::min(i, v.size() - 1)];
}

static unsigned long long fsize(const std::string &f)
{
  std::error_code ec;
  return (unsigned long long)std::filesystem::file_size(f, ec);
}

// A 64-bit key in numeric order
class U64Key : public DatabaseKeyB {
public:
  U64Key(uint64_t x = 0) : DatabaseKeyB((char *)&v), v(x) { }
  size_t KeySize() { return sizeof(v); }
  int CompareKey(const DatabaseKeyB &o) const {
    uint64_t w;
    memcpy(&w, o.db_key, sizeof(w));
    return v < w ? -1 : v > w ? 1 : 0;
  }
  int operator==(const DatabaseKeyB &o) const { return CompareKey(o) == 0; }
  int operator>(const DatabaseKeyB &o) const { return CompareKey(o) > 0; }
  uint64_t v;
};

struct Rec { uint64_t id; uint32_t day, kind; char text[112]; };   // 128 bytes

// Clustered points: centers on the unit sphere, points around them
struct Clusters {
  std::mt19937 g;
  std::vector<std::vector<float> > c;
  unsigned dim;
  Clusters(unsigned d, unsigned n, unsigned seed) : g(seed), dim(d) {
    std::normal_distribution<float> nd(0.0f, 1.0f);
    c.assign(n, std::vector<float>(d));
    for(auto &x : c) for(auto &y : x) y = nd(g);
  }
  void Point(float *out) {
    std::normal_distribution<float> nd(0.0f, 0.35f);
    const std::vector<float> &x = c[g() % c.size()];
    for(unsigned i = 0; i < dim; i++) out[i] = x[i] + nd(g);
  }
};

static void line() { printf("  ------------------------------------------------------------------------\n"); }

int main(int argc, char **argv)
{
  std::string dir = "dr_bench_work";
  bool quick = false;
  for(int i = 1; i < argc; i++) {
    if(strcmp(argv[i], "--quick") == 0) quick = true;
    else dir = argv[i];
  }
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir, ec);
  const unsigned NREC = quick ? 20000 : 200000, NCOMMIT = quick ? 100 : 500, NKEY = quick ? 20000 : 200000;
  const unsigned NVEC = quick ? 5000 : 50000, DIM = 96, NQ = quick ? 100 : 500;
  const unsigned DAYS = 10, PER_DAY = quick ? 1000 : 10000;
  printf("DataReel 7.4 benchmarks (%s run), work folder %s\n", quick ? "quick" : "full", dir.c_str());

  // 1. gxDatabase ----------------------------------------------------------
  {
    printf("\n1. gxDatabase: %u fixed 128-byte records\n", NREC);
    line();
    std::string f = dir + "/records.gxd";
    gxDatabase db;
    db.Create(f.c_str());
    std::vector<FAU_t> addr(NREC);
    Rec r;
    memset(&r, 0, sizeof(r));
    auto t0 = Clock::now();
    for(unsigned i = 0; i < NREC; i++) {
      r.id = i; r.day = 20261110 + i % 5; r.kind = i % 7;
      snprintf(r.text, sizeof(r.text), "record %u", i);
      addr[i] = db.Alloc(sizeof(r));
      db.Write(&r, sizeof(r), addr[i]);
    }
    double w = ms_since(t0);
    db.Flush();
    std::mt19937 g(3);
    t0 = Clock::now();
    unsigned bad = 0;
    for(unsigned i = 0; i < NREC; i++) {
      unsigned k = g() % NREC;
      db.Read(&r, sizeof(r), addr[k]);
      if(r.id != k) bad++;
    }
    double rd = ms_since(t0);
    db.Close();
    printf("  append (Alloc + Write)      %10.0f records/s   %7.0f ms\n", NREC * 1000.0 / w, w);
    printf("  random read                 %10.0f records/s   %7.0f ms  (%u wrong)\n", NREC * 1000.0 / rd, rd, bad);
    printf("  file                        %10.1f MB (%.0f bytes a record with its block header)\n",
	   fsize(f) / 1048576.0, (double)fsize(f) / NREC);
  }

  // 2. gxdJournal ----------------------------------------------------------
  {
    printf("\n2. gxdJournal: commits of 10 records of 128 bytes\n");
    line();
    for(int durable = 1; durable >= 0; durable--) {
      std::string base = dir + (durable ? "/jd" : "/jn");
      gxdJournal j;
      gxDatabase db;
      j.Open((base + ".jnl").c_str());
      db.Create((base + ".gxd").c_str());
      j.Attach(&db);
      unsigned n = durable ? NCOMMIT : NCOMMIT * 4;
      std::vector<double> lat;
      Rec r;
      memset(&r, 0, sizeof(r));
      auto t0 = Clock::now();
      for(unsigned c = 0; c < n; c++) {
	auto t1 = Clock::now();
	j.Begin();
	for(unsigned k = 0; k < 10; k++) {
	  r.id = c * 10 + k;
	  FAU_t a = db.Alloc(sizeof(r));
	  db.Write(&r, sizeof(r), a);
	}
	j.Commit(durable);
	lat.push_back(ms_since(t1));
      }
      double t = ms_since(t0);
      j.Close();
      db.Close();
      printf("  %-26s  %8.0f commits/s  p50 %6.2f ms  p99 %6.2f ms  (%u commits)\n",
	     durable ? "Commit(1), durable" : "Commit(0), not durable", n * 1000.0 / t, pct(lat, 0.5), pct(lat, 0.99), n);
    }
  }

  // 3. gxBtree -------------------------------------------------------------
  {
    printf("\n3. gxBtree: %u 64-bit keys, node order 32\n", NKEY);
    line();
    std::string f = dir + "/keys.btx";
    U64Key type, cmp;
    gxBtree bt(type, 32);
    bt.Create(f.c_str());
    std::vector<uint64_t> keys(NKEY);
    std::mt19937_64 g(5);
    std::set<uint64_t> seen;
    for(auto &k : keys) { do k = g(); while(!seen.insert(k).second); }
    auto t0 = Clock::now();
    for(auto k : keys) { U64Key x(k); bt.Insert(x, cmp); }
    double ins = ms_since(t0);
    std::mt19937 g2(6);
    unsigned miss = 0;
    t0 = Clock::now();
    for(unsigned i = 0; i < NKEY; i++) { U64Key x(keys[g2() % NKEY]); if(!bt.Find(x, cmp)) miss++; }
    double fnd = ms_since(t0);
    unsigned long long walked = 0;
    t0 = Clock::now();
    U64Key x;
    for(int ok = bt.FindFirst(x); ok; ok = bt.FindNext(x, cmp)) walked++;
    double wk = ms_since(t0);
    bt.Close();
    printf("  insert (random order)       %10.0f keys/s      %7.0f ms\n", NKEY * 1000.0 / ins, ins);
    printf("  find (random)               %10.0f keys/s      %7.0f ms  (%u missed)\n", NKEY * 1000.0 / fnd, fnd, miss);
    printf("  walk in order               %10.0f keys/s      %7.0f ms  (%llu keys)\n", walked * 1000.0 / wk, wk, walked);
    printf("  file                        %10.1f MB\n", fsize(f) / 1048576.0);
  }

  // 4. gxVectorIndex -------------------------------------------------------
  {
    printf("\n4. gxVectorIndex: %u vectors of dimension %u (cosine, 100 clusters), M 16, ef_construction 200,\n"
	   "   %u queries, recall@10 against Exact()\n", NVEC, DIM, NQ);
    line();
    Clusters cl(DIM, 100, 9);
    std::vector<std::vector<float> > data(NVEC, std::vector<float>(DIM)), qs(NQ, std::vector<float>(DIM));
    for(auto &v : data) cl.Point(v.data());
    for(auto &q : qs) cl.Point(q.data());
    const gxvElementType types[3] = { gxvFLOAT32, gxvFLOAT16, gxvINT8 };
    const char *names[3] = { "float32", "float16", "int8" };
    for(int t = 0; t < 3; t++) {
      std::string f = dir + "/vectors_" + names[t] + ".vix";
      gxVectorIndex vix;
      vix.Create(f.c_str(), DIM, types[t], gxvCOSINE, 16, "bench/v1", 200);
      auto t0 = Clock::now();
      for(unsigned i = 0; i < NVEC; i++) {
	unsigned tags[gxvTagCount] = { i % 3, 0, 20261110 + i % 5, 0 };
	vix.Insert((long long)i, data[i].data(), tags);
      }
      double build = ms_since(t0);
      unsigned node = vix.NodeSize();
      vix.Close();
      vix.Open(f.c_str(), gxDBASE_READONLY);
      std::vector<std::vector<long long> > truth(NQ);
      std::vector<gxvResult> r;
      t0 = Clock::now();
      for(unsigned q = 0; q < NQ; q++) {
	vix.Exact(qs[q].data(), 10, r);
	for(auto &h : r) truth[q].push_back(h.id);
      }
      double ex = ms_since(t0) / NQ;
      printf("  %-7s build %7.0f vectors/s  file %6.1f MB (%u bytes a node)  Exact() %6.2f ms a query\n",
	     names[t], NVEC * 1000.0 / build, fsize(f) / 1048576.0, node, ex);
      const unsigned efs[3] = { 32, 64, 128 };
      for(unsigned e : efs) {
	std::vector<double> lat;
	unsigned found = 0, want = 0;
	for(unsigned q = 0; q < NQ; q++) {
	  auto t1 = Clock::now();
	  vix.Search(qs[q].data(), 10, r, e);
	  lat.push_back(ms_since(t1) * 1000.0);
	  std::set<long long> s(truth[q].begin(), truth[q].end());
	  for(auto &h : r) if(s.count(h.id)) found++;
	  want += (unsigned)truth[q].size();
	}
	printf("          Search() ef %3u   p50 %7.0f us  p99 %7.0f us  recall@10 %.3f\n", e, pct(lat, 0.5),
	       pct(lat, 0.99), want ? (double)found / want : 0.0);
      }
      vix.Close();
    }
  }

  // 5. gxVectorSet ---------------------------------------------------------
  {
    printf("\n5. gxVectorSet: %u day files of %u vectors (dimension %u, cosine), %u queries, k 10, ef 64\n",
	   DAYS, PER_DAY, DIM, NQ);
    line();
    Clusters cl(DIM, 100, 11);
    std::string manifest = dir + "/days.vixset";
    gxVectorSet set;
    set.Create(manifest.c_str(), "days", DIM, gxvFLOAT32, gxvCOSINE, "bench/days");
    std::vector<float> v(DIM);
    long long id = 1;
    for(unsigned d = 0; d < DAYS; d++) {
      unsigned day = 20261101 + d;
      std::string rel = "day_" + std::to_string(day) + ".vix", full = dir + "/" + rel;
      gxVectorIndex vix;
      vix.Create(full.c_str(), DIM, gxvFLOAT32, gxvCOSINE, 16, "bench/days", 100);
      for(unsigned i = 0; i < PER_DAY; i++) {
	cl.Point(v.data());
	unsigned tags[gxvTagCount] = { 0, 0, day, 0 };
	vix.Insert(id++, v.data(), tags);
      }
      vix.Close();
      set.AddFile(rel.c_str());
    }
    set.Close();
    std::vector<std::vector<float> > qs(NQ, std::vector<float>(DIM));
    for(auto &q : qs) cl.Point(q.data());
    struct Case { const char *what; unsigned threads; int range; };
    const Case cases[3] = { { "every day, 1 thread", 1, 0 }, { "every day, 4 threads", 4, 0 },
			    { "one day (9 files skipped)", 1, 1 } };
    for(auto &c : cases) {
      gxVectorSet s;
      s.Open(manifest.c_str());
      gxvSetSearch o;
      o.k = 10; o.ef = 64; o.threads = c.threads;
      if(c.range) { o.day_range = 1; o.day_lo = o.day_hi = 20261105; }
      std::vector<gxvSetResult> r;
      s.Search(qs[0].data(), o, r);                      // the files opened before timing
      std::vector<double> lat;
      for(unsigned q = 0; q < NQ; q++) {
	auto t1 = Clock::now();
	s.Search(qs[q].data(), o, r);
	lat.push_back(ms_since(t1));
      }
      printf("  %-28s p50 %6.2f ms  p99 %6.2f ms  (%llu file(s) opened)\n", c.what, pct(lat, 0.5), pct(lat, 0.99),
	     s.Opened());
      s.Close();
    }
  }
  std::filesystem::remove_all(dir, ec);
  printf("\nDone\n");
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
