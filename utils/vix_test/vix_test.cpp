// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: vix_test.cpp
// Compiler Used: GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/12/2026
// Date Last Modified: 11/12/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- //
// ------------- Program Description and Details ------------- //
// ----------------------------------------------------------- //
/*
Tests of the vector index (gxvindex.h), phase A:

  vix_test [work_dir] [--crash-rounds N] [--bench]

1. Kernels: fixed summation order, SIMD-safe results.
2. Exact search: every metric against a brute force reference, tag
   and range filters, accept callback, order of ties.
3. Insert / Update / Delete / reopen / spec check / bad arguments.
4. Journal: commit, rollback + Reload.
5. Crash: a child inserts in committed batches and is killed with
   SIGKILL at a random moment; the parent recovers the journal and
   checks that every committed batch is in the index and no partial
   batch is (POSIX only).
6. --bench: exact search time over 30k, 150k and 1M vectors.

Exit code 0 when every test passes.
*/
// ----------------------------------------------------------- //
// The kernel test compares gxv_dot with a sum written out by hand, bit
// for bit, so this file must not contract multiply-add either (see
// gxvkern.cpp); utils/CMakeLists.txt also passes -ffp-contract=off.
#if defined(__clang__)
#pragma clang fp contract(off)
#elif defined(__GNUC__)
#pragma GCC optimize ("fp-contract=off")
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include <random>
#include <thread>
#include <atomic>
#include "gxvindex.h"
#include "gxvkern.h"
#include "gxdjrnl.h"

#ifndef _WIN32
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#endif

using namespace datareel;

static int failures = 0;
static int checks = 0;

#define CHECK(cond, ...) do { checks++; if(!(cond)) { failures++; \
  printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while(0)

static std::string dir;
static std::string path(const char *name) { return dir + "/" + name; }

static void rand_vec(std::mt19937 &g, float *v, unsigned dim)
{
  std::normal_distribution<float> nd(0.0f, 1.0f);
  for(unsigned i = 0; i < dim; i++) v[i] = nd(g);
}

// Reference distance in the same order the library documents
static float ref_dist(gxvMetric met, const float *a, const float *b, unsigned dim)
{
  if(met == gxvL2) return gxv_l2sq(a, b, dim);
  if(met == gxvIP) return -gxv_dot(a, b, dim);
  return 1.0f - gxv_dot(a, b, dim);
}

static void test_kernels()
{
  // Fixed order: compare with the order spelled out by hand
  std::mt19937 g(7);
  for(unsigned dim = 1; dim < 40; dim++) {
    std::vector<float> a(dim), b(dim);
    rand_vec(g, a.data(), dim);
    rand_vec(g, b.data(), dim);
    float s[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    unsigned i = 0;
    for(; i + 8 <= dim; i += 8) for(unsigned j = 0; j < 8; j++) s[j] += a[i + j] * b[i + j];
    float r = ((s[0] + s[4]) + (s[2] + s[6])) + ((s[1] + s[5]) + (s[3] + s[7]));
    for(; i < dim; i++) r += a[i] * b[i];
    float k = gxv_dot(a.data(), b.data(), dim);
    CHECK(memcmp(&r, &k, 4) == 0, "dot order differs at dim %u", dim);
    double d = 0;
    for(unsigned j = 0; j < dim; j++) d += (double)(a[j] - b[j]) * (a[j] - b[j]);
    CHECK(fabs(d - gxv_l2sq(a.data(), b.data(), dim)) <= 1e-4 * (1 + d), "l2 off at dim %u", dim);
  }
  float z[5] = {0, 0, 0, 0, 0};
  CHECK(gxv_normalize(z, 5) == 0.0f && z[0] == 0.0f, "zero vector normalize");
  float n[2] = {3, 4};
  CHECK(fabsf(gxv_normalize(n, 2) - 5.0f) < 1e-6f && fabsf(n[0] - 0.6f) < 1e-6f, "normalize");
  float bad[2] = {1, NAN};
  CHECK(!gxv_finite(bad, 2), "NaN not caught");
}

struct RefNode { long long id; std::vector<float> v; unsigned t[4]; int live; };

static std::vector<gxvResult> brute(gxvMetric met, const std::vector<RefNode> &nodes,
				    const float *q, unsigned dim, unsigned k,
				    const gxvFilter *f)
{
  std::vector<float> qq(q, q + dim);
  if(met == gxvCOSINE) gxv_normalize(qq.data(), dim);
  std::vector<gxvResult> all;
  for(const RefNode &n : nodes) {
    if(!n.live) continue;
    if(f && !f->Match(n.id, n.t)) continue;
    std::vector<float> v = n.v;
    if(met == gxvCOSINE) gxv_normalize(v.data(), dim);
    gxvResult r;
    r.id = n.id;
    r.distance = ref_dist(met, qq.data(), v.data(), dim);
    memcpy(r.tags, n.t, sizeof(r.tags));
    all.push_back(r);
  }
  std::sort(all.begin(), all.end(), [](const gxvResult &a, const gxvResult &b) {
    if(a.distance != b.distance) return a.distance < b.distance;
    return a.id < b.id;
  });
  if(all.size() > k) all.resize(k);
  return all;
}

static int same(const std::vector<gxvResult> &a, const std::vector<gxvResult> &b)
{
  if(a.size() != b.size()) return 0;
  for(size_t i = 0; i < a.size(); i++) {
    if(a[i].id != b[i].id || memcmp(&a[i].distance, &b[i].distance, 4) != 0) return 0;
    if(memcmp(a[i].tags, b[i].tags, sizeof(a[i].tags)) != 0) return 0;
  }
  return 1;
}

static int odd_ids(long long id, const unsigned *, void *) { return (id & 1) != 0; }

static void test_search()
{
  const gxvMetric mets[3] = { gxvL2, gxvCOSINE, gxvIP };
  const char *names[3] = { "l2", "cosine", "ip" };
  for(int mi = 0; mi < 3; mi++) {
    unsigned dim = 79;
    std::mt19937 g(100 + mi);
    std::string f = path((std::string("search_") + names[mi] + ".vix").c_str());
    gxVectorIndex vix;
    CHECK(vix.Create(f.c_str(), dim, gxvFLOAT32, mets[mi], 16, "test/v1") == gxv_NO_ERROR,
	  "create %s", names[mi]);
    std::vector<RefNode> nodes;
    for(int i = 0; i < 3000; i++) {
      RefNode n;
      n.id = 1000 + i * 7;
      n.v.resize(dim);
      rand_vec(g, n.v.data(), dim);
      n.t[0] = i % 3;             // session
      n.t[1] = 72 + i % 2;        // kind
      n.t[2] = 20261019 + i % 20; // day
      n.t[3] = 0;
      n.live = 1;
      CHECK(vix.Insert(n.id, n.v.data(), n.t) == gxv_NO_ERROR, "insert");
      nodes.push_back(n);
    }
    // Duplicate values: two nodes with the same vector, so ties are tested
    RefNode dup = nodes[10];
    dup.id = 5;
    CHECK(vix.Insert(dup.id, dup.v.data(), dup.t) == gxv_NO_ERROR, "insert dup vector");
    nodes.push_back(dup);

    for(int qi = 0; qi < 30; qi++) {
      std::vector<float> q(dim);
      if(qi == 0) q = nodes[10].v; else rand_vec(g, q.data(), dim);
      std::vector<gxvResult> got;
      CHECK(vix.Exact(q.data(), 10, got) == gxv_NO_ERROR, "exact");
      CHECK(same(got, brute(mets[mi], nodes, q.data(), dim, 10, 0)), "%s exact != brute (q %d)", names[mi], qi);
      if(qi == 0 && mets[mi] == gxvL2) {
	CHECK(got.size() >= 2 && got[0].id == 5 && got[1].id == nodes[10].id, "tie order by id");
      }
      gxvFilter flt;
      flt.Tag(0, 1);
      flt.Range(2, 0, 20261030);
      CHECK(vix.Search(q.data(), 20, got, 64, &flt) == gxv_NO_ERROR, "search");
      CHECK(same(got, brute(mets[mi], nodes, q.data(), dim, 20, &flt)), "%s filtered != brute", names[mi]);
      for(const gxvResult &r : got) CHECK(r.tags[0] == 1 && r.tags[2] <= 20261030, "filter leak");
      gxvFilter cb;
      cb.accept = odd_ids;
      CHECK(vix.Exact(q.data(), 5, got, &cb) == gxv_NO_ERROR, "callback");
      CHECK(same(got, brute(mets[mi], nodes, q.data(), dim, 5, &cb)), "callback != brute");
    }
    // k larger than the set
    gxvFilter none;
    none.Tag(1, 999);
    std::vector<gxvResult> got;
    CHECK(vix.Exact(nodes[0].v.data(), 10, got, &none) == gxv_NO_ERROR && got.empty(), "empty filter");
    CHECK(vix.Exact(nodes[0].v.data(), 100000, got) == gxv_NO_ERROR && got.size() == nodes.size(), "k > n");
    vix.Close();
  }
}

static void test_crud()
{
  unsigned dim = 16;
  std::string f = path("crud.vix");
  std::mt19937 g(3);
  std::vector<float> a(dim), b(dim), c(dim), out(dim);
  rand_vec(g, a.data(), dim);
  rand_vec(g, b.data(), dim);
  rand_vec(g, c.data(), dim);
  {
    gxVectorIndex vix;
    CHECK(vix.Create(f.c_str(), 0) == gxv_BAD_ARG, "dim 0");
    CHECK(vix.Create(f.c_str(), gxvMaxDim + 1) == gxv_BAD_ARG, "dim too big");
    CHECK(vix.Create(f.c_str(), dim, gxvINT8) == gxv_UNSUPPORTED, "int8 not yet");
    CHECK(vix.Create(f.c_str(), dim, gxvFLOAT32, gxvL2, 16, "anomaly/shock/v1/abc") == gxv_NO_ERROR, "create");
    CHECK(vix.Insert(1, a.data()) == gxv_NO_ERROR, "insert 1");
    CHECK(vix.Insert(1, b.data()) == gxv_DUPLICATE_ID, "duplicate id");
    unsigned t[4] = {1, 2, 3, 4};
    CHECK(vix.Insert(2, b.data(), t) == gxv_NO_ERROR, "insert 2");
    CHECK(vix.Insert(3, c.data()) == gxv_NO_ERROR, "insert 3");
    std::vector<float> nanv = a;
    nanv[3] = NAN;
    CHECK(vix.Insert(4, nanv.data()) == gxv_NOT_FINITE, "NaN refused");
    CHECK(vix.Update(2, c.data()) == gxv_NO_ERROR, "update vector only");
    unsigned tt[4];
    CHECK(vix.Get(2, out.data(), tt) == gxv_NO_ERROR && memcmp(out.data(), c.data(), 4 * dim) == 0 &&
	  tt[3] == 4, "get after update");
    // A new vector replaces the node: the old one is a tombstone
    CHECK(vix.Count() == 3 && vix.Deleted() == 1, "counts after update");
    unsigned t2[4] = {5, 6, 7, 8};
    CHECK(vix.Update(2, c.data(), t2) == gxv_NO_ERROR && vix.Deleted() == 1, "same vector: tags in place");
    CHECK(vix.Get(2, 0, tt) == gxv_NO_ERROR && tt[0] == 5 && tt[3] == 8, "tags after update");
    CHECK(vix.Delete(3) == gxv_NO_ERROR, "delete");
    CHECK(vix.Delete(3) == gxv_ID_NOT_FOUND, "delete twice");
    CHECK(!vix.Contains(3) && vix.Count() == 2 && vix.Deleted() == 2, "counts after delete");
    // A deleted id can be inserted again (a new node)
    CHECK(vix.Insert(3, a.data()) == gxv_NO_ERROR, "reinsert deleted id");
    std::string rep;
    CHECK(vix.Verify(0, 0, 0, &rep) == 0, "verify: %s", rep.c_str());
    vix.Close();
  }
  {
    gxVectorIndex vix;
    CHECK(vix.Open(f.c_str(), gxDBASE_READWRITE, "anomaly/shock/v2/def") == gxv_SPEC_MISMATCH, "spec mismatch");
    CHECK(vix.Open(f.c_str(), gxDBASE_READONLY, "anomaly/shock/v1/abc") == gxv_NO_ERROR, "reopen");
    CHECK(vix.Count() == 3 && vix.Deleted() == 2 && vix.Dim() == dim, "counts after reopen");
    CHECK(vix.GraphSaved(), "graph saved");
    CHECK(vix.Get(3, out.data()) == gxv_NO_ERROR && memcmp(out.data(), a.data(), 4 * dim) == 0, "reinserted vector");
    CHECK(vix.Insert(9, a.data()) == gxv_READ_ONLY, "read only");
    std::vector<gxvResult> r;
    CHECK(vix.Exact(c.data(), 1, r) == gxv_NO_ERROR && r.size() == 1 && r[0].id == 2 && r[0].distance == 0.0f,
	  "nearest after reopen");
    auto resolve = [](long long id, void *) -> int { return id != 2; };
    std::vector<long long> bad;
    CHECK(vix.Verify(resolve, 0, &bad) == 1 && bad.size() == 1 && bad[0] == 2, "verify resolver");
    vix.Close();
  }
  {
    // Not a vector index
    gxDatabase plain;
    std::string pf = path("plain.gxd");
    plain.Create(pf.c_str(), 256);
    plain.Close();
    gxVectorIndex vix;
    CHECK(vix.Open(pf.c_str()) == gxv_BAD_FILE, "plain file refused");
  }
}

static void test_journal()
{
  unsigned dim = 8;
  std::string f = path("jrnl.vix"), jf = path("jrnl.jnl");
  remove(jf.c_str());
  std::mt19937 g(5);
  std::vector<float> v(dim);
  gxdJournal j;
  CHECK(j.Open(jf.c_str()) == gxDBASE_NO_ERROR, "journal open");
  gxVectorIndex vix;
  CHECK(vix.Create(f.c_str(), dim) == gxv_NO_ERROR, "create");
  CHECK(j.Attach(vix.Database()) == gxDBASE_NO_ERROR, "attach");
  j.Begin();
  for(int i = 0; i < 50; i++) { rand_vec(g, v.data(), dim); vix.Insert(i, v.data()); }
  CHECK(j.Commit() == gxDBASE_NO_ERROR, "commit");
  j.Begin();
  for(int i = 50; i < 80; i++) { rand_vec(g, v.data(), dim); vix.Insert(i, v.data()); }
  vix.Delete(3);
  CHECK(j.Rollback() == gxDBASE_NO_ERROR, "rollback");
  CHECK(vix.Reload() == gxv_NO_ERROR, "reload after rollback");
  CHECK(vix.Count() == 50 && vix.Contains(3) && !vix.Contains(60), "rolled back: count %llu", vix.Count());
  std::string rep;
  CHECK(vix.Verify(0, 0, 0, &rep) == 0, "verify after rollback: %s", rep.c_str());
  j.Close();
  vix.Close();
  CHECK(vix.Open(f.c_str()) == gxv_NO_ERROR && vix.Count() == 50, "reopen after journal");
  vix.Close();
}

// Readers search while one writer inserts: every search sees a
// consistent index (results sorted, ids present, never more than k).
static void test_concurrent()
{
  const unsigned dim = 24;
  std::string f = path("conc.vix");
  gxVectorIndex vix;
  CHECK(vix.Create(f.c_str(), dim) == gxv_NO_ERROR, "create");
  std::atomic<int> done(0), bad(0);
  std::atomic<long> searches(0);
  std::vector<std::thread> readers;
  for(int t = 0; t < 3; t++) {
    readers.emplace_back([&, t]() {
      std::mt19937 g(50 + t);
      std::vector<float> q(dim);
      std::vector<gxvResult> r;
      while(!done.load()) {
	rand_vec(g, q.data(), dim);
	if(vix.Search(q.data(), 10, r) != gxv_NO_ERROR) bad++;
	if(r.size() > 10) bad++;
	for(size_t i = 1; i < r.size(); i++) if(r[i].distance < r[i - 1].distance) bad++;
	searches++;
      }
    });
  }
  std::mt19937 g(9);
  std::vector<float> v(dim);
  for(int i = 0; i < 20000; i++) {
    rand_vec(g, v.data(), dim);
    if(vix.Insert(i, v.data()) != gxv_NO_ERROR) bad++;
    if(i % 7 == 0 && i > 0) vix.Delete(i - 1);
  }
  done = 1;
  for(auto &t : readers) t.join();
  CHECK(bad.load() == 0, "concurrent: %d bad results", bad.load());
  CHECK(searches.load() > 0, "concurrent: no searches ran");
  printf("concurrent: %ld searches during 20000 inserts\n", searches.load());
  vix.Close();
}

// Points around a set of centers, like real feature vectors (which
// are never spread evenly over the whole space)
struct Clusters {
  Clusters(unsigned dim_, unsigned count, unsigned seed) : dim(dim_), g(seed) {
    centers.resize((size_t)count * dim);
    std::normal_distribution<float> nd(0.0f, 3.0f);
    for(float &c : centers) c = nd(g);
  }
  void Point(float *v) {
    std::uniform_int_distribution<size_t> pick(0, centers.size() / dim - 1);
    std::normal_distribution<float> nd(0.0f, 1.0f);
    const float *c = &centers[pick(g) * dim];
    for(unsigned i = 0; i < dim; i++) v[i] = c[i] + nd(g);
  }
  unsigned dim;
  std::mt19937 g;
  std::vector<float> centers;
};

static double recall(const std::vector<gxvResult> &got, const std::vector<gxvResult> &exact)
{
  if(exact.empty()) return got.empty() ? 1.0 : 0.0;
  size_t hit = 0;
  for(const gxvResult &e : exact) {
    for(const gxvResult &r : got) if(r.id == e.id) { hit++; break; }
  }
  return (double)hit / exact.size();
}

static int sorted_ok(const std::vector<gxvResult> &r)
{
  for(size_t i = 1; i < r.size(); i++) {
    if(r[i].distance < r[i - 1].distance) return 0;
    if(r[i].distance == r[i - 1].distance && r[i].id < r[i - 1].id) return 0;
  }
  return 1;
}

// Mean recall of Search() against Exact() over the queries
static double mean_recall(gxVectorIndex &vix, const std::vector<std::vector<float> > &qs,
			  unsigned k, unsigned ef, const gxvFilter *f = 0, int *bad = 0)
{
  double sum = 0;
  std::vector<gxvResult> got, ex;
  for(const auto &q : qs) {
    vix.Search(q.data(), k, got, ef, f);
    vix.Exact(q.data(), k, ex, f);
    sum += recall(got, ex);
    if(bad && (!sorted_ok(got) || got.size() > k)) (*bad)++;
    if(bad && f) for(const gxvResult &r : got) if(!f->Match(r.id, r.tags)) (*bad)++;
  }
  return qs.empty() ? 1.0 : sum / qs.size();
}

static void test_graph()
{
  const unsigned dim = 32, N = 20000;
  const gxvMetric mets[3] = { gxvL2, gxvCOSINE, gxvIP };
  const char *names[3] = { "l2", "cosine", "ip" };
  for(int mi = 0; mi < 3; mi++) {
    std::string f = path((std::string("graph_") + names[mi] + ".vix").c_str());
    Clusters cl(dim, 100, 200 + mi);
    std::vector<std::vector<float> > pts(N, std::vector<float>(dim));
    std::vector<std::vector<float> > qs(200, std::vector<float>(dim));
    for(auto &p : pts) cl.Point(p.data());
    for(auto &q : qs) cl.Point(q.data());
    auto tags_of = [](unsigned i, unsigned *t) {
      t[0] = i % 3; t[1] = 72 + i % 10; t[2] = 20261001 + i % 30; t[3] = i;
    };
    std::vector<std::vector<gxvResult> > first;
    {
      gxVectorIndex vix;
      CHECK(vix.Create(f.c_str(), dim, gxvFLOAT32, mets[mi], 16, "graph/v1") == gxv_NO_ERROR, "create");
      int ins = 0;
      for(unsigned i = 0; i < N; i++) {
	unsigned t[4];
	tags_of(i, t);
	if(vix.Insert(10 + 3 * (long long)i, pts[i].data(), t) != gxv_NO_ERROR) ins++;
      }
      CHECK(ins == 0, "%s: %d inserts failed", names[mi], ins);
      CHECK(vix.TopLevel() > 0, "%s: graph has upper levels", names[mi]);
      int bad = 0;
      double r64 = mean_recall(vix, qs, 10, 64, 0, &bad);
      double r200 = mean_recall(vix, qs, 10, 200, 0, &bad);
      printf("graph %s: %u nodes, top level %u, recall@10 ef 64 %.4f, ef 200 %.4f\n",
	     names[mi], N, vix.TopLevel(), r64, r200);
      CHECK(r64 >= 0.95, "%s: recall %.4f at ef 64", names[mi], r64);
      CHECK(r200 >= r64 - 0.005 && r200 >= 0.98, "%s: recall %.4f at ef 200", names[mi], r200);
      gxvFilter flt;
      flt.Tag(0, 1);
      flt.Range(2, 0, 20261020);
      double rf = mean_recall(vix, qs, 10, 64, &flt, &bad);
      printf("graph %s: filtered (22%% of nodes) recall@10 %.4f\n", names[mi], rf);
      CHECK(rf >= 0.90, "%s: filtered recall %.4f", names[mi], rf);
      gxvFilter one;
      one.Tag(1, 75);
      one.Range(3, 0, 1999); // 200 nodes: below 5%, so exact
      std::vector<gxvResult> got, ex;
      for(int i = 0; i < 20; i++) {
	vix.Search(qs[i].data(), 10, got, 64, &one);
	vix.Exact(qs[i].data(), 10, ex, &one);
	CHECK(same(got, ex), "%s: selective filter not exact", names[mi]);
      }
      CHECK(bad == 0, "%s: %d unsorted or filtered-out results", names[mi], bad);
      std::string rep;
      CHECK(vix.Verify(0, 0, 0, &rep) == 0, "%s: verify: %.300s", names[mi], rep.c_str());
      for(const auto &q : qs) { vix.Search(q.data(), 10, got); first.push_back(got); }
    }
    {
      // The graph read back from the file gives the same answers
      gxVectorIndex vix;
      CHECK(vix.Open(f.c_str(), gxDBASE_READONLY, "graph/v1") == gxv_NO_ERROR, "reopen");
      CHECK(vix.GraphSaved(), "%s: graph loaded from the file", names[mi]);
      int diff = 0;
      std::vector<gxvResult> got;
      for(size_t i = 0; i < qs.size(); i++) { vix.Search(qs[i].data(), 10, got); if(!same(got, first[i])) diff++; }
      CHECK(diff == 0, "%s: %d searches differ after reopen", names[mi], diff);
    }
    if(mi == 0) {
      // The same inserts in the same order build the same graph
      std::string f2 = path("graph_l2_again.vix");
      gxVectorIndex vix;
      vix.Create(f2.c_str(), dim, gxvFLOAT32, mets[mi], 16, "graph/v1");
      for(unsigned i = 0; i < N; i++) {
	unsigned t[4];
	tags_of(i, t);
	vix.Insert(10 + 3 * (long long)i, pts[i].data(), t);
      }
      int diff = 0;
      std::vector<gxvResult> got;
      for(size_t i = 0; i < qs.size(); i++) { vix.Search(qs[i].data(), 10, got); if(!same(got, first[i])) diff++; }
      CHECK(diff == 0, "rebuild: %d searches differ", diff);
      vix.Close();
      remove(f2.c_str());
    }
    {
      // Deletes and updates
      gxVectorIndex vix;
      CHECK(vix.Open(f.c_str(), gxDBASE_READWRITE, "graph/v1") == gxv_NO_ERROR, "reopen rw");
      for(unsigned i = 0; i < N; i += 3) vix.Delete(10 + 3 * (long long)i);
      std::mt19937 g(77);
      std::vector<float> nv(dim), got_v(dim);
      int upd = 0, found = 0, want = 0;
      for(unsigned i = 1; i < N; i += 200) {
	if(i % 3) want++; // every third node was just deleted
	cl.Point(nv.data());
	if(vix.Update(10 + 3 * (long long)i, nv.data()) == gxv_NO_ERROR) upd++;
	std::vector<gxvResult> r;
	vix.Search(nv.data(), 1, r);
	if(!(i % 3) || mets[mi] == gxvIP || (r.size() == 1 && r[0].id == 10 + 3 * (long long)i)) found++;
      }
      CHECK(upd == want, "%s: updates %d of %d", names[mi], upd, want);
      CHECK(found == (int)((N - 1 + 199) / 200), "%s: updated vector found %d of %d", names[mi], found, upd);
      int bad = 0, leaked = 0;
      double rd = mean_recall(vix, qs, 10, 64, 0, &bad);
      std::vector<gxvResult> got;
      for(const auto &q : qs) {
	vix.Search(q.data(), 10, got);
	for(const gxvResult &r : got) if(!vix.Contains(r.id)) leaked++;
      }
      printf("graph %s: after deleting a third, recall@10 %.4f\n", names[mi], rd);
      CHECK(leaked == 0, "%s: %d deleted nodes returned", names[mi], leaked);
      CHECK(rd >= 0.90, "%s: recall after deletes %.4f", names[mi], rd);
      std::string rep;
      CHECK(vix.Verify(0, 0, 0, &rep) == 0, "%s: verify after deletes: %.300s", names[mi], rep.c_str());
      unsigned long long live = vix.Count(), dead = vix.Deleted();
      CHECK(vix.Repair() == gxv_NO_ERROR, "%s: repair", names[mi]);
      CHECK(vix.Count() == live && vix.Deleted() == dead, "%s: repair kept the counts", names[mi]);
      rep.clear();
      CHECK(vix.Verify(0, 0, 0, &rep) == 0, "%s: verify after repair: %.300s", names[mi], rep.c_str());
      double rr = mean_recall(vix, qs, 10, 64);
      printf("graph %s: after Repair(), recall@10 %.4f\n", names[mi], rr);
      CHECK(rr >= 0.95, "%s: recall after repair %.4f", names[mi], rr);
      vix.Close();
      CHECK(vix.Open(f.c_str(), gxDBASE_READONLY) == gxv_NO_ERROR && vix.GraphSaved() &&
	    vix.Count() == live, "%s: reopen after repair", names[mi]);
      rep.clear();
      CHECK(vix.Verify(0, 0, 0, &rep) == 0, "%s: verify after repair reopen: %.300s", names[mi], rep.c_str());
      vix.Close();
    }
    remove(f.c_str());
  }
}

// Take the graph out of a file, as phase A wrote it
static void strip_graph(const std::string &f)
{
  gxDatabase db;
  db.Open(f.c_str());
  std::vector<FAU_t> uppers, nodes;
  for(FAU_t a = db.FindFirstObject(); a; a = db.FindNextObject(a)) {
    unsigned char h[16];
    db.Read(h, 16, a);
    unsigned magic;
    memcpy(&magic, h + 8, 4);
    if(magic == 0x55565847u) uppers.push_back(a); else nodes.push_back(a);
  }
  unsigned zero = 0;
  long long zero64 = 0;
  for(FAU_t a : nodes) {
    db.Write(&zero, 4, a + 12);
    db.Write(&zero64, 8, a + 32);
    db.Write(&zero, 4, a + 40);
  }
  for(FAU_t a : uppers) db.Delete(a);
  FAU_t sa = db.StaticArea();
  db.Write(&zero, 4, sa + 136);
  db.Write(&zero, 4, sa + 44);
  db.Write(&zero64, 8, sa + 48);
  db.Close();
}

static void test_phase_a_files()
{
  const unsigned dim = 24, N = 6000;
  std::string f = path("phase_a.vix");
  Clusters cl(dim, 40, 9);
  std::vector<std::vector<float> > qs(100, std::vector<float>(dim));
  for(auto &q : qs) cl.Point(q.data());
  std::vector<float> v(dim);
  {
    gxVectorIndex vix;
    vix.Create(f.c_str(), dim);
    for(unsigned i = 0; i < N; i++) { cl.Point(v.data()); vix.Insert(i, v.data()); }
    vix.Close();
  }
  strip_graph(f);
  {
    gxVectorIndex vix;
    CHECK(vix.Open(f.c_str()) == gxv_NO_ERROR, "open phase A file");
    CHECK(!vix.GraphSaved() && vix.Count() == N, "phase A: graph built in memory");
    double r = mean_recall(vix, qs, 10, 64);
    CHECK(r >= 0.95, "phase A file: recall %.4f", r);
    std::string rep;
    CHECK(vix.Verify(0, 0, 0, &rep) == 0, "phase A verify: %.300s", rep.c_str());
    for(unsigned i = 0; i < 10; i++) { cl.Point(v.data()); vix.Insert(N + i, v.data()); }
    vix.Close();
    CHECK(vix.Open(f.c_str()) == gxv_NO_ERROR && !vix.GraphSaved() && vix.Count() == N + 10,
	  "phase A file after inserts");
    CHECK(vix.Repair() == gxv_NO_ERROR && vix.GraphSaved(), "repair phase A file");
    vix.Close();
    CHECK(vix.Open(f.c_str()) == gxv_NO_ERROR && vix.GraphSaved(), "graph saved by Repair()");
    rep.clear();
    CHECK(vix.Verify(0, 0, 0, &rep) == 0, "verify after repair: %.300s", rep.c_str());
    r = mean_recall(vix, qs, 10, 64);
    CHECK(r >= 0.95, "repaired file: recall %.4f", r);
    vix.Close();
  }
  {
    // A link to a block that is not a node: the graph is rebuilt at Open()
    gxDatabase db;
    db.Open(f.c_str());
    FAU_t a = db.FindFirstObject();
    unsigned c0;
    db.Read(&c0, 4, a + 40);
    long long bogus = 12345;
    db.Write(&bogus, 8, a + 48);
    db.Close();
    gxVectorIndex vix;
    CHECK(c0 > 0 && vix.Open(f.c_str()) == gxv_NO_ERROR && !vix.GraphSaved(),
	  "damaged link: graph rebuilt in memory");
    double r = mean_recall(vix, qs, 10, 64);
    CHECK(r >= 0.95, "damaged link: recall %.4f", r);
    CHECK(vix.Repair() == gxv_NO_ERROR, "repair damaged link");
    vix.Close();
    CHECK(vix.Open(f.c_str()) == gxv_NO_ERROR && vix.GraphSaved(), "damaged link repaired");
    vix.Close();
  }
  remove(f.c_str());
}

static void test_merge()
{
  const unsigned dim = 32, per = 3000;
  Clusters cl(dim, 60, 31);
  std::vector<std::string> days;
  std::vector<float> v(dim);
  long long next = 1;
  for(int d = 0; d < 3; d++) {
    std::string f = path((std::string("day") + (char)('1' + d) + ".vix").c_str());
    days.push_back(f);
    gxVectorIndex vix;
    vix.Create(f.c_str(), dim, gxvFLOAT32, gxvCOSINE, 16, "graph/v1");
    for(unsigned i = 0; i < per; i++) {
      cl.Point(v.data());
      unsigned t[4] = { i % 3, 72, 20261101u + d, 0 };
      vix.Insert(next++, v.data(), t);
    }
    if(d == 1) for(long long id = per + 1; id < per + 101; id++) vix.Delete(id);
    vix.Close();
  }
  std::string out = path("month.vix"), rep;
  CHECK(gxVectorIndex::Merge(out.c_str(), days, "graph/v1", 0, 200, &rep) == gxv_NO_ERROR,
	"merge: %s", rep.c_str());
  {
    gxVectorIndex vix;
    CHECK(vix.Open(out.c_str(), gxDBASE_READONLY, "graph/v1") == gxv_NO_ERROR, "open merged");
    CHECK(vix.Count() == 3 * per - 100 && vix.Deleted() == 0 && vix.GraphSaved() &&
	  vix.Metric() == gxvCOSINE, "merged counts %llu", vix.Count());
    CHECK(!vix.Contains(per + 50) && vix.Contains(per + 200), "merged: tombstones left out");
    std::vector<std::vector<float> > qs(100, std::vector<float>(dim));
    for(auto &q : qs) cl.Point(q.data());
    double r = mean_recall(vix, qs, 10, 64);
    printf("merge: %llu nodes from 3 day files, recall@10 %.4f\n", vix.Count(), r);
    CHECK(r >= 0.95, "merged recall %.4f", r);
    gxvFilter day2;
    day2.Tag(2, 20261102);
    double rf = mean_recall(vix, qs, 10, 64, &day2);
    CHECK(rf >= 0.90, "merged, one day: recall %.4f", rf);
    std::string vr;
    CHECK(vix.Verify(0, 0, 0, &vr) == 0, "merged verify: %.300s", vr.c_str());
  }
  std::string out2 = path("month2.vix");
  CHECK(gxVectorIndex::Merge(out2.c_str(), days, "graph/v2") == gxv_SPEC_MISMATCH, "merge: spec mismatch");
  std::vector<std::string> twice = { days[0], days[0] };
  CHECK(gxVectorIndex::Merge(out2.c_str(), twice) == gxv_DUPLICATE_ID, "merge: duplicate ids");
  {
    std::string other = path("day_other.vix");
    gxVectorIndex vix;
    vix.Create(other.c_str(), 16, gxvFLOAT32, gxvCOSINE, 16, "graph/v1");
    vix.Close();
    std::vector<std::string> mixed = { days[0], other };
    gxvError me = gxVectorIndex::Merge(out2.c_str(), mixed);
    CHECK(me == gxv_BAD_ARG, "merge: other dimension: %s", gxVectorIndex::ErrorMessage(me));
    remove(other.c_str());
  }
  for(const std::string &d : days) remove(d.c_str());
  remove(out.c_str());
  remove(out2.c_str());
}

#ifndef _WIN32
static void write_progress(const std::string &pf, long batches)
{
  // Write a new note and rename it over the old one, so a kill never
  // leaves the note empty or half written
  std::string tmp = pf + ".tmp";
  int fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if(fd < 0) return;
  char b[32];
  int n = snprintf(b, sizeof(b), "%ld\n", batches);
  if(write(fd, b, n) != n) { }
  fsync(fd);
  close(fd);
  rename(tmp.c_str(), pf.c_str());
}

static long read_progress(const std::string &pf)
{
  FILE *fp = fopen(pf.c_str(), "r");
  if(!fp) return -1;
  long n = 0;
  if(fscanf(fp, "%ld", &n) != 1) n = -1;
  fclose(fp);
  return n;
}

static void test_crash(int rounds)
{
  const unsigned dim = 32;
  const int batch = 37;
  std::string f = path("crash.vix"), jf = path("crash.jnl"), pf = path("crash.progress");
  std::mt19937 g(11);
  int ok_rounds = 0, after_commit = 0;
  long total_committed = 0;
  for(int round = 0; round < rounds; round++) {
    remove(f.c_str());
    remove(jf.c_str());
    remove(pf.c_str());
    remove((pf + ".tmp").c_str());
    pid_t pid = fork();
    if(pid == 0) {
      gxdJournal j;
      if(j.Open(jf.c_str()) != gxDBASE_NO_ERROR) _exit(2);
      gxVectorIndex vix;
      if(vix.Create(f.c_str(), dim) != gxv_NO_ERROR) _exit(3);
      vix.Database()->Sync();
      j.Attach(vix.Database());
      write_progress(pf, 0); // created and synced: the parent starts its kill timer now
      std::mt19937 cg(round);
      std::vector<float> v(dim);
      for(long b = 0;; b++) {
	j.Begin();
	for(int i = 0; i < batch; i++) {
	  long long id = b * batch + i;
	  for(unsigned d = 0; d < dim; d++) v[d] = (float)((id * 31 + d) % 97) / 7.0f;
	  if(vix.Insert(id, v.data()) != gxv_NO_ERROR) _exit(4);
	  if(i == batch / 2 && b > 0) vix.Delete(b * batch - 1); // delete inside the batch too
	}
	if(j.Commit() != gxDBASE_NO_ERROR) _exit(5);
	write_progress(pf, b + 1);
      }
    }
    for(int w = 0; w < 30000 && read_progress(pf) < 0; w++) usleep(1000);
    CHECK(read_progress(pf) >= 0, "round %d: child did not create the file in 30 s", round);
    std::uniform_int_distribution<int> wait_us(20000, 400000);
    usleep(wait_us(g));
    kill(pid, SIGKILL);
    int st = 0;
    waitpid(pid, &st, 0);
    long committed = read_progress(pf);
    CHECK(committed >= 0, "round %d: progress note unreadable after the kill", round);
    if(committed < 0) continue;
    unsigned long recovered = 0;
    gxDatabaseError je = gxdJournal::Recover(jf.c_str(), &recovered);
    CHECK(je == gxDBASE_NO_ERROR, "round %d: recover error %d", round, (int)je);
    gxVectorIndex vix;
    gxvError oe = vix.Open(f.c_str());
    CHECK(oe == gxv_NO_ERROR, "round %d: open after crash: %s", round, gxVectorIndex::ErrorMessage(oe));
    if(oe != gxv_NO_ERROR) continue;
    // Total nodes is a whole number of batches, at least the committed ones
    unsigned long long nodes = vix.Count() + vix.Deleted();
    long batches = (long)(nodes / batch);
    CHECK(nodes % batch == 0, "round %d: partial batch in the file (%llu nodes)", round, nodes);
    CHECK(batches >= committed, "round %d: lost a committed batch (%ld < %ld)", round, batches, committed);
    int good = 1;
    for(long long id = 0; id < (long long)batches * batch; id++) {
      int deleted = (id % batch == batch - 1) && (id / batch) < batches - 1;
      if(vix.Contains(id) == deleted) { good = 0; break; }
    }
    CHECK(good, "round %d: ids after recovery are not the committed set", round);
    std::string rep;
    CHECK(vix.Verify(0, 0, 0, &rep) == 0, "round %d: verify: %s", round, rep.c_str());
    if(good) ok_rounds++;
    total_committed += committed;
    if(batches > committed) after_commit++;
    vix.Close();
  }
  printf("crash: %d/%d rounds recovered exactly (%ld committed batches in all; %d rounds killed "
	 "after a commit but before its progress note, and kept it)\n", ok_rounds, rounds,
	 total_committed, after_commit);
}
#endif

static void bench()
{
  const unsigned dim = 79;
  const size_t sizes[3] = { 30000, 150000, 1000000 };
  for(size_t s : sizes) {
    std::string f = path("bench.vix");
    gxVectorIndex vix;
    vix.Create(f.c_str(), dim);
    std::mt19937 g(1);
    std::vector<float> v(dim);
    auto t0 = std::chrono::steady_clock::now();
    gxdJournal j;
    std::string jf = path("bench.jnl");
    remove(jf.c_str());
    j.Open(jf.c_str());
    j.SetWriteMode(gxdJOURNAL_WRITE_BACK);
    j.Attach(vix.Database(), 64 * 1024 * 1024);
    for(size_t i = 0; i < s; i++) {
      if(i % 5000 == 0) j.Begin();
      rand_vec(g, v.data(), dim);
      vix.Insert((long long)i, v.data());
      if(i % 5000 == 4999 || i + 1 == s) j.Commit(0);
    }
    j.Close();
    double ins = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::vector<gxvResult> r;
    std::vector<double> times;
    for(int q = 0; q < 50; q++) {
      rand_vec(g, v.data(), dim);
      auto a = std::chrono::steady_clock::now();
      vix.Exact(v.data(), 20, r);
      times.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - a).count());
    }
    std::sort(times.begin(), times.end());
    printf("bench: %zu x %u: insert %.1f s (%.1f us each), exact k=20 p50 %.2f ms p99 %.2f ms\n",
	   s, dim, ins, 1e6 * ins / s, times[25], times[49]);
    gxvFilter fl;
    fl.Range(2, 0, 0);
    auto a = std::chrono::steady_clock::now();
    vix.Close();
    gxVectorIndex ro;
    ro.Open(f.c_str(), gxDBASE_READONLY);
    double op = std::chrono::duration<double>(std::chrono::steady_clock::now() - a).count();
    printf("bench: %zu: open (load nodes) %.2f s\n", s, op);
    ro.Close();
    remove(f.c_str());
  }
}

static double pct(std::vector<double> &v, double p)
{
  if(v.empty()) return 0;
  std::sort(v.begin(), v.end());
  size_t i = (size_t)(p * (v.size() - 1));
  return v[i];
}

// Phase B gate: an index of n vectors, searched by 3 threads while one
// thread keeps inserting (committed batches through a journal)
static void gate(size_t n, unsigned dim, int uniform, unsigned efc, int live_sec)
{
  std::string f = path("gate.vix"), jf = path("gate.jnl");
  remove(f.c_str());
  remove(jf.c_str());
  Clusters cl(dim, 1000, 42);
  std::mt19937 ug(43);
  auto point = [&](float *v) { if(uniform) rand_vec(ug, v, dim); else cl.Point(v); };
  gxVectorIndex vix;
  vix.Create(f.c_str(), dim, gxvFLOAT32, gxvL2, 16, "gate/v1", efc);
  gxdJournal j;
  j.Open(jf.c_str());
  j.SetWriteMode(gxdJOURNAL_WRITE_BACK);
  j.Attach(vix.Database(), 64 * 1024 * 1024);
  std::vector<float> v(dim);
  auto t0 = std::chrono::steady_clock::now();
  for(size_t i = 0; i < n; i++) {
    if(i % 1000 == 0) j.Begin();
    point(v.data());
    unsigned t[4] = { (unsigned)(i % 3), 72, 20261101u + (unsigned)(i % 20), 0 };
    vix.Insert((long long)i, v.data(), t);
    if(i % 1000 == 999 || i + 1 == n) j.Commit(0);
    if(i % 100000 == 99999) {
      double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
      printf("gate: %zu inserted, %.0f s\n", i + 1, s);
      fflush(stdout);
    }
  }
  double build = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  printf("gate: %zu x %u (%s), M 16, ef_construction %u: build %.0f s (%.0f us an insert), top level %u\n",
	 n, dim, uniform ? "uniform" : "clustered", efc, build, 1e6 * build / n, vix.TopLevel());
  std::vector<std::vector<float> > qs(200, std::vector<float>(dim));
  for(auto &q : qs) point(q.data());
  for(unsigned ef : { 32u, 64u, 128u }) {
    double r = mean_recall(vix, qs, 20, ef);
    std::vector<double> times;
    std::vector<gxvResult> res;
    for(int rep = 0; rep < 5; rep++) {
      for(const auto &q : qs) {
	auto a = std::chrono::steady_clock::now();
	vix.Search(q.data(), 20, res, ef);
	times.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - a).count());
      }
    }
    printf("gate: quiet, k 20, ef %u: recall %.4f, p50 %.3f ms, p99 %.3f ms\n", ef, r,
	   pct(times, 0.5), pct(times, 0.99));
  }
  gxvFilter fl;
  fl.Tag(0, 1);
  fl.Range(2, 0, 20261110);
  printf("gate: quiet, filtered (17%%), ef 64: recall %.4f\n", mean_recall(vix, qs, 20, 64, &fl));

  // Live: one writer, three searchers
  std::atomic<int> stop(0);
  std::atomic<long> inserted(0);
  std::thread writer([&]() {
    std::vector<float> w(dim);
    Clusters wc(dim, 1000, 42);
    std::mt19937 wg(44);
    long long id = (long long)n;
    while(!stop.load()) {
      j.Begin();
      for(int b = 0; b < 100 && !stop.load(); b++) {
	if(uniform) rand_vec(wg, w.data(), dim); else wc.Point(w.data());
	unsigned t[4] = { (unsigned)(id % 3), 72, 20261121, 0 };
	if(vix.Insert(id++, w.data(), t) == gxv_NO_ERROR) inserted++;
      }
      j.Commit(0);
    }
  });
  std::vector<std::vector<double> > lat(3);
  std::vector<std::thread> readers;
  for(int t = 0; t < 3; t++) {
    readers.emplace_back([&, t]() {
      std::vector<gxvResult> res;
      size_t qi = (size_t)t * 61;
      while(!stop.load()) {
	const auto &q = qs[qi++ % qs.size()];
	auto a = std::chrono::steady_clock::now();
	vix.Search(q.data(), 20, res, 64);
	lat[t].push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - a).count());
      }
    });
  }
  std::this_thread::sleep_for(std::chrono::seconds(live_sec));
  stop = 1;
  writer.join();
  for(auto &t : readers) t.join();
  std::vector<double> all;
  for(auto &l : lat) all.insert(all.end(), l.begin(), l.end());
  double mx = all.empty() ? 0 : *std::max_element(all.begin(), all.end());
  printf("gate: live %d s: %ld inserts (%.0f a second) during %zu searches: p50 %.3f ms, p99 %.3f ms, "
	 "p99.9 %.3f ms, max %.2f ms\n", live_sec, inserted.load(), inserted.load() / (double)live_sec,
	 all.size(), pct(all, 0.5), pct(all, 0.99), pct(all, 0.999), mx);
  double r = mean_recall(vix, qs, 20, 64);
  printf("gate: after the live inserts (%llu nodes): recall@20 ef 64 %.4f\n", vix.Count(), r);
  j.Close();
  auto a = std::chrono::steady_clock::now();
  vix.Close();
  gxVectorIndex ro;
  ro.Open(f.c_str(), gxDBASE_READONLY);
  double op = std::chrono::duration<double>(std::chrono::steady_clock::now() - a).count();
  printf("gate: open (nodes + graph) %.1f s, graph saved %d\n", op, ro.GraphSaved());
  ro.Close();
  remove(f.c_str());
  remove(jf.c_str());
}

int main(int argc, char **argv)
{
  dir = "vix_test_work";
  int rounds = 20, do_bench = 0, uniform = 0, live_sec = 20;
  long gate_n = 0;
  unsigned gate_dim = 96, efc = 200;
  for(int i = 1; i < argc; i++) {
    if(!strcmp(argv[i], "--crash-rounds") && i + 1 < argc) rounds = atoi(argv[++i]);
    else if(!strcmp(argv[i], "--bench")) do_bench = 1;
    else if(!strcmp(argv[i], "--gate") && i + 1 < argc) gate_n = atol(argv[++i]);
    else if(!strcmp(argv[i], "--dim") && i + 1 < argc) gate_dim = (unsigned)atoi(argv[++i]);
    else if(!strcmp(argv[i], "--efc") && i + 1 < argc) efc = (unsigned)atoi(argv[++i]);
    else if(!strcmp(argv[i], "--live") && i + 1 < argc) live_sec = atoi(argv[++i]);
    else if(!strcmp(argv[i], "--uniform")) uniform = 1;
    else dir = argv[i];
  }
#ifndef _WIN32
  std::string mk = "mkdir -p '" + dir + "'";
  if(system(mk.c_str()) != 0) { }
#endif
  if(gate_n > 0) {
    gate((size_t)gate_n, gate_dim, uniform, efc, live_sec);
    return 0;
  }
  test_kernels();
  test_search();
  test_crud();
  test_journal();
  test_graph();
  test_phase_a_files();
  test_merge();
  test_concurrent();
#ifndef _WIN32
  if(rounds > 0) test_crash(rounds);
#endif
  if(do_bench) bench();
  printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
