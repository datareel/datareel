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
Tests of the vector index (gxvindex.h), phases A to C:

  vix_test [work_dir] [--crash-rounds N] [--bench]
  vix_test [work_dir] --gate N [--dim D] [--type f32|f16|i8] [--efc E]
           [--live SECONDS] [--uniform]

Searches, graphs, Merge, Repair and crashes run for each element type
(float32, float16, int8); the float16 and int8 runs also check recall
against the exact answer on the float vectors, and that Merge() and
Repair() rebuild the same graph the inserts built. The element tests
convert all 63488 finite float16 values both ways and check rounding,
subnormals, the 65504 limit and the int8 error bound.

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
#include <map>
#include <set>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <random>
#include <thread>
#include <atomic>
#include "gxvindex.h"
#include "gxvset.h"
#include "gxvmember.h"
#include "gxvkern.h"
#include "gxdjrnl.h"
#include "infohog.h"

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

// Element types under test (phase C). The float32 runs print the same
// lines as before; the others say their type.
struct ElemType { gxvElementType type; const char *tag; };
static const ElemType TYPES[3] = { { gxvFLOAT32, "f32" }, { gxvFLOAT16, "f16" }, { gxvINT8, "i8" } };
static std::string label(const ElemType &et, const char *name)
{
  return et.type == gxvFLOAT32 ? std::string(name) : std::string(et.tag) + " " + name;
}
static std::string tagged(const ElemType &et, const std::string &file)
{
  return et.type == gxvFLOAT32 ? path(file.c_str()) : path((std::string(et.tag) + "_" + file).c_str());
}
static long long file_size(const std::string &f)
{
  FILE *fp = fopen(f.c_str(), "rb");
  if(!fp) return -1;
  fseek(fp, 0, SEEK_END);
  long long n = ftell(fp);
  fclose(fp);
  return n;
}

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

// v: the vector given; d (float16 / int8 indexes): the decoded vector the
// index stores, already normalized for cosine
static unsigned short f16(float x) { return gxv_f16_from_float(x); }

// Phase C element conversions and the index's handling of them
static void test_elements()
{
  // float16: every finite half value goes to float and back unchanged
  int bad = 0, finite = 0;
  for(unsigned h = 0; h < 65536; h++) {
    if(((h >> 10) & 0x1Fu) == 0x1Fu) continue; // infinity and NaN
    finite++;
    if(gxv_f16_from_float(gxv_float_from_f16((unsigned short)h)) != h) bad++;
  }
  CHECK(bad == 0 && finite == 63488, "f16 round trip: %d of %d differ", bad, finite);
  CHECK(gxv_float_from_f16(0x3C00) == 1.0f && gxv_float_from_f16(0x7BFF) == 65504.0f &&
	gxv_float_from_f16(0x0001) == ldexpf(1.0f, -24) && gxv_float_from_f16(0x0400) == ldexpf(1.0f, -14) &&
	gxv_float_from_f16(0xC000) == -2.0f && signbit(gxv_float_from_f16(0x8000)), "f16 known values");
  CHECK(isinf(gxv_float_from_f16(0x7C00)) && isnan(gxv_float_from_f16(0x7E00)), "f16 infinity and NaN");
  // Round to nearest, ties to even
  CHECK(f16(1.0f + ldexpf(1.0f, -11)) == 0x3C00, "f16 tie to even (down)");
  CHECK(f16(1.0f + 3.0f * ldexpf(1.0f, -11)) == 0x3C02, "f16 tie to even (up)");
  CHECK(f16(nextafterf(1.0f + ldexpf(1.0f, -11), 2.0f)) == 0x3C01, "f16 above a tie rounds up");
  CHECK(f16(nextafterf(1.0f + ldexpf(1.0f, -11), 0.0f)) == 0x3C00, "f16 below a tie rounds down");
  // Subnormals: kept, with ties to even, and the carry into the normals
  CHECK(f16(ldexpf(1.0f, -25)) == 0x0000, "f16 2^-25 ties to zero");
  CHECK(f16(nextafterf(ldexpf(1.0f, -25), 1.0f)) == 0x0001, "f16 just above 2^-25");
  CHECK(f16(3.0f * ldexpf(1.0f, -25)) == 0x0002 && f16(5.0f * ldexpf(1.0f, -25)) == 0x0002,
	"f16 subnormal ties to even");
  CHECK(f16(1023.5f * ldexpf(1.0f, -24)) == 0x0400, "f16 largest subnormal rounds up to 2^-14");
  CHECK(f16(-ldexpf(1.0f, -20)) == 0x8010, "f16 negative subnormal");
  // The largest value: 65504; 65519 rounds to it, 65520 would be infinity
  CHECK(f16(65504.0f) == 0x7BFF && f16(65519.0f) == 0x7BFF && f16(65520.0f) == 0x7C00 &&
	f16(-65504.0f) == 0xFBFF, "f16 limits");

  // The table decode gives the same bits as the scalar decoder for all
  // 65536 patterns (infinities and NaNs included)
  {
    std::vector<unsigned short> all(65536);
    std::vector<float> tab(65536);
    for(unsigned h = 0; h < 65536; h++) all[h] = (unsigned short)h;
    gxv_f16_decode(all.data(), tab.data(), 65536);
    int diff = 0;
    for(unsigned h = 0; h < 65536; h++) {
      float one = gxv_float_from_f16((unsigned short)h);
      if(memcmp(&one, &tab[h], 4) != 0) diff++;
    }
    CHECK(diff == 0, "f16 table decode differs from the scalar one for %d patterns", diff);
  }
  // Fused float16 distances = decode, then the float kernel, bit for bit
  {
    std::mt19937 fg(29);
    int diff = 0, tried = 0;
    std::vector<unsigned> dims;
    for(unsigned d = 1; d <= 96; d++) dims.push_back(d);
    dims.push_back(4096);
    for(unsigned d : dims) {
      for(int rep = 0; rep < 4; rep++) {
	std::vector<float> a(d), b(d), bd(d);
	std::vector<unsigned short> h(d);
	rand_vec(fg, a.data(), d);
	rand_vec(fg, b.data(), d);
	for(unsigned i = 0; i < d; i++) h[i] = gxv_f16_from_float(b[i] * 8.0f);
	gxv_f16_decode(h.data(), bd.data(), d);
	float l1 = gxv_l2sq(a.data(), bd.data(), d), l2 = gxv_l2sq_f16(a.data(), h.data(), d);
	float d1 = gxv_dot(a.data(), bd.data(), d), d2 = gxv_dot_f16(a.data(), h.data(), d);
	if(memcmp(&l1, &l2, 4) != 0 || memcmp(&d1, &d2, 4) != 0) diff++;
	tried++;
      }
    }
    CHECK(diff == 0, "fused f16 distances differ in %d of %d vectors", diff, tried);
  }

  // The index refuses values above 65504 with gxv_OUT_OF_RANGE
  std::string f = path("elem_f16.vix");
  {
    gxVectorIndex vix;
    CHECK(vix.Create(f.c_str(), 4, gxvFLOAT16) == gxv_NO_ERROR && vix.NodeSize() == 48 + 8 * 32 + 8,
	  "f16 create, node size %u", vix.NodeSize());
    float ok[4] = { 1.0f, -2.5f, 65504.0f, -65504.0f }, out[4];
    float big[4] = { 1.0f, 2.0f, 65504.5f, 0.0f }, neg[4] = { 0.0f, -70000.0f, 0.0f, 0.0f };
    CHECK(vix.Insert(1, ok) == gxv_NO_ERROR, "f16 insert at the limit");
    CHECK(vix.Insert(2, big) == gxv_OUT_OF_RANGE && vix.Insert(3, neg) == gxv_OUT_OF_RANGE,
	  "f16 out of range refused");
    CHECK(vix.Update(1, neg) == gxv_OUT_OF_RANGE && vix.Count() == 1, "f16 update out of range refused");
    CHECK(vix.Get(1, out) == gxv_NO_ERROR && memcmp(ok, out, sizeof(ok)) == 0, "f16 exact values kept");
    float third[4] = { 1.0f / 3.0f, 0.1f, 1e-6f, 3e-8f };
    CHECK(vix.Insert(4, third) == gxv_NO_ERROR && vix.Get(4, out) == gxv_NO_ERROR, "f16 insert");
    int good = 1;
    for(int i = 0; i < 4; i++) good &= out[i] == gxv_float_from_f16(f16(third[i]));
    CHECK(good, "f16 Get() returns the decoded values");
    CHECK(strcmp(gxVectorIndex::ErrorMessage(gxv_OUT_OF_RANGE), "Unknown vector index error") != 0,
	  "OUT_OF_RANGE has a message");
    vix.Close();
    CHECK(vix.Open(f.c_str(), gxDBASE_READONLY) == gxv_NO_ERROR && vix.ElementType() == gxvFLOAT16 &&
	  vix.Count() == 2, "f16 reopen");
    vix.Close();
  }
  {
    // A file with an element type from a later version is refused
    gxDatabase db;
    db.Open(f.c_str());
    unsigned four = 4;
    db.Write(&four, 4, (FAU_t)db.FileHeaderSize() + 16); // The static area follows the file header
    db.Close();
    gxVectorIndex vix;
    CHECK(vix.Open(f.c_str()) == gxv_UNSUPPORTED, "unknown element type in a file");
    remove(f.c_str());
  }

  // int8: s = max|v| / 127, every value within s / 2
  signed char q[128];
  float z[8] = { 0, 0, 0, 0, 0, 0, 0, 0 }, zd[8];
  float s = gxv_i8_encode(z, q, 8);
  gxv_i8_decode(q, s, zd, 8);
  int zeros = s == 0.0f;
  for(int i = 0; i < 8; i++) zeros &= q[i] == 0 && zd[i] == 0.0f;
  CHECK(zeros, "int8 zero vector");
  std::mt19937 g(17);
  int over = 0, ends = 0;
  for(unsigned dim = 1; dim <= 128; dim++) {
    std::vector<float> a(dim), d(dim);
    rand_vec(g, a.data(), dim);
    for(float &x : a) x *= (float)(1 + dim % 7) * 10.0f;
    s = gxv_i8_encode(a.data(), q, dim);
    gxv_i8_decode(q, s, d.data(), dim);
    float mx = 0;
    for(float x : a) mx = fabsf(x) > mx ? fabsf(x) : mx;
    if(s != mx / 127.0f) over++;
    for(unsigned i = 0; i < dim; i++) {
      if(fabsf(d[i] - a[i]) > 0.5f * s * (1.0f + 1e-5f)) over++;
      if(fabsf(a[i]) == mx && (q[i] == 127 || q[i] == -127)) ends++;
    }
  }
  CHECK(over == 0, "int8: %d values off by more than s / 2", over);
  CHECK(ends >= 128, "int8: the largest value maps to +-127 (%d)", ends);
  f = path("elem_i8.vix");
  {
    gxVectorIndex vix;
    CHECK(vix.Create(f.c_str(), 16, gxvINT8) == gxv_NO_ERROR && vix.NodeSize() == 48 + 8 * 32 + 20,
	  "int8 create, node size %u", vix.NodeSize());
    std::vector<float> a(16), out(16), want(16);
    rand_vec(g, a.data(), 16);
    CHECK(vix.Insert(1, a.data()) == gxv_NO_ERROR && vix.Get(1, out.data()) == gxv_NO_ERROR, "int8 insert");
    s = gxv_i8_encode(a.data(), q, 16);
    gxv_i8_decode(q, s, want.data(), 16);
    CHECK(memcmp(out.data(), want.data(), 4 * 16) == 0, "int8 Get() returns the decoded values");
    // A change smaller than half a step stores the same codes: tags only
    std::vector<float> b = a;
    b[0] += s * 0.01f;
    unsigned t[4] = { 9, 9, 9, 9 };
    CHECK(vix.Update(1, b.data(), t) == gxv_NO_ERROR && vix.Deleted() == 0, "int8 same codes: tags in place");
    std::string rep;
    CHECK(vix.Verify(0, 0, 0, &rep) == 0, "int8 verify: %s", rep.c_str());
    vix.Close();
    remove(f.c_str());
  }
}

struct RefNode { long long id; std::vector<float> v; std::vector<float> d; unsigned t[4]; int live; };

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
    std::vector<float> v = n.d.empty() ? n.v : n.d;
    if(met == gxvCOSINE && n.d.empty()) gxv_normalize(v.data(), dim);
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

static void test_search(const ElemType &et)
{
  const gxvMetric mets[3] = { gxvL2, gxvCOSINE, gxvIP };
  const char *names0[3] = { "l2", "cosine", "ip" };
  std::string lab[3] = { label(et, "l2"), label(et, "cosine"), label(et, "ip") };
  const char *names[3] = { lab[0].c_str(), lab[1].c_str(), lab[2].c_str() };
  for(int mi = 0; mi < 3; mi++) {
    unsigned dim = 79;
    std::mt19937 g(100 + mi);
    std::string f = tagged(et, std::string("search_") + names0[mi] + ".vix");
    gxVectorIndex vix;
    CHECK(vix.Create(f.c_str(), dim, et.type, mets[mi], 16, "test/v1") == gxv_NO_ERROR,
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
      if(et.type != gxvFLOAT32) {
	n.d.resize(dim);
	CHECK(vix.Get(n.id, n.d.data()) == gxv_NO_ERROR, "get decoded");
      }
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
    CHECK(vix.Create(f.c_str(), dim, (gxvElementType)4) == gxv_BAD_ARG, "unknown element type");
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

static std::vector<std::string> size_lines;

static void test_graph(const ElemType &et)
{
  const unsigned dim = 32, N = 20000;
  const gxvMetric mets[3] = { gxvL2, gxvCOSINE, gxvIP };
  const char *names0[3] = { "l2", "cosine", "ip" };
  std::string lab[3] = { label(et, "l2"), label(et, "cosine"), label(et, "ip") };
  const char *names[3] = { lab[0].c_str(), lab[1].c_str(), lab[2].c_str() };
  for(int mi = 0; mi < 3; mi++) {
    std::string f = tagged(et, std::string("graph_") + names0[mi] + ".vix");
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
      CHECK(vix.Create(f.c_str(), dim, et.type, mets[mi], 16, "graph/v1") == gxv_NO_ERROR, "create");
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
      if(et.type != gxvFLOAT32) {
	// Recall against the exact answer on the original float vectors
	std::vector<RefNode> truth(N);
	for(unsigned i = 0; i < N; i++) {
	  truth[i].id = 10 + 3 * (long long)i;
	  truth[i].v = pts[i];
	  truth[i].live = 1;
	  tags_of(i, truth[i].t);
	}
	double sum = 0;
	for(const auto &q : qs) {
	  vix.Search(q.data(), 10, got, 200);
	  sum += recall(got, brute(mets[mi], truth, q.data(), dim, 10, 0));
	}
	double rt = sum / qs.size();
	// int8 cosine needs the decode to rescale to unit length (0.947
	// without it, 0.980 with it, on this set)
	double need = et.type == gxvFLOAT16 ? 0.99 : 0.95;
	printf("graph %s: recall@10 at ef 200 against float32 exact %.4f\n", names[mi], rt);
	CHECK(rt >= need, "%s: recall against float32 %.4f below %.2f", names[mi], rt, need);
      }
    }
    if(mi == 0) {
      char line[160];
      snprintf(line, sizeof(line), "%s: %u x %u, %lld bytes (node %u bytes)", et.tag, N, dim,
	       file_size(f), 48 + 8 * 32 + (et.type == gxvFLOAT32 ? 4 * dim : et.type == gxvFLOAT16 ? 2 * dim : dim + 4));
      size_lines.push_back(line);
    }
    {
      // Determinism: Merge() of the file and Repair() rebuild the graph
      // from the stored (decoded) vectors and give the same answers as
      // the inserts did
      std::string mf = tagged(et, std::string("graph_merged_") + names0[mi] + ".vix");
      std::vector<std::string> one = { f };
      CHECK(gxVectorIndex::Merge(mf.c_str(), one, "graph/v1") == gxv_NO_ERROR, "%s: merge one file", names[mi]);
      gxVectorIndex mv;
      CHECK(mv.Open(mf.c_str(), gxDBASE_READONLY, "graph/v1") == gxv_NO_ERROR, "%s: open merged", names[mi]);
      int diff = 0;
      std::vector<gxvResult> got;
      for(size_t i = 0; i < qs.size(); i++) { mv.Search(qs[i].data(), 10, got); if(!same(got, first[i])) diff++; }
      CHECK(diff == 0, "%s: %d searches differ after Merge()", names[mi], diff);
      mv.Close();
      remove(mf.c_str());
      gxVectorIndex rv;
      CHECK(rv.Open(f.c_str(), gxDBASE_READWRITE, "graph/v1") == gxv_NO_ERROR && rv.Repair() == gxv_NO_ERROR,
	    "%s: repair before deletes", names[mi]);
      diff = 0;
      for(size_t i = 0; i < qs.size(); i++) { rv.Search(qs[i].data(), 10, got); if(!same(got, first[i])) diff++; }
      CHECK(diff == 0, "%s: %d searches differ after Repair()", names[mi], diff);
      rv.Close();
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
      std::string f2 = tagged(et, "graph_l2_again.vix");
      gxVectorIndex vix;
      vix.Create(f2.c_str(), dim, et.type, mets[mi], 16, "graph/v1");
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

// 11/14/2026 (phase D): the format fields, and files made by the phase A-C library
#ifndef VIX_TEST_DATA
#define VIX_TEST_DATA "utils/vix_test/data"
#endif
static std::string data_dir = VIX_TEST_DATA;

static unsigned get32_at(const std::string &f, long off)
{
  gxDatabase db;
  unsigned v = 0;
  if(db.Open(f.c_str(), gxDBASE_READONLY) == gxDBASE_NO_ERROR) {
    db.Read(&v, 4, (FAU_t)db.FileHeaderSize() + off);
    db.Close();
  }
  return v;
}

static void put32_at(const std::string &f, long off, unsigned v)
{
  gxDatabase db;
  if(db.Open(f.c_str()) == gxDBASE_NO_ERROR) {
    db.Write(&v, 4, (FAU_t)db.FileHeaderSize() + off);
    db.Close();
  }
}

static void test_format_fields()
{
  const unsigned dim = 8;
  std::string f = path("fields.vix");
  remove(f.c_str());
  std::vector<float> v(dim);
  {
    gxVectorIndex vix;
    CHECK(vix.Create(f.c_str(), dim) == gxv_NO_ERROR, "create");
    unsigned lo, hi;
    CHECK(vix.FeaturesRequired() == 0 && (vix.FeaturesOptional() & gxvOptTagRanges) && vix.Codec() == 0 &&
	  vix.GraphLayout() == 0 && vix.PartitionId() == 0 && vix.PartitionCount() == 0, "new file: defaults");
    CHECK(!vix.TagRange(0, lo, hi) && vix.TagRangeExcludes(2, 0, 0xFFFFFFFFu), "empty index: no range, excludes all");
    for(unsigned i = 0; i < 50; i++) {
      for(unsigned d = 0; d < dim; d++) v[d] = (float)((i * 7 + d * 3) % 11);
      unsigned t[4] = { i % 5, 1000 + i, 20261101 + i % 9, 7 };
      CHECK(vix.Insert(i, v.data(), t) == gxv_NO_ERROR, "insert %u", i);
    }
    CHECK(vix.TagRange(1, lo, hi) && lo == 1000 && hi == 1049, "tag 1 range %u..%u", lo, hi);
    CHECK(vix.TagRange(2, lo, hi) && lo == 20261101 && hi == 20261109, "tag 2 range");
    CHECK(vix.TagRangeExcludes(2, 20261110, 20261130) && !vix.TagRangeExcludes(2, 20261105, 20261130),
	  "range test");
    // Delete never narrows; an in-place tag rewrite widens
    CHECK(vix.Delete(49) == gxv_NO_ERROR && vix.TagRange(1, lo, hi) && hi == 1049, "delete keeps the range");
    for(unsigned d = 0; d < dim; d++) v[d] = (float)((3 * 7 + d * 3) % 11);
    unsigned t2[4] = { 3, 5000, 20261103, 7 };
    CHECK(vix.Update(3, v.data(), t2) == gxv_NO_ERROR && vix.TagRange(1, lo, hi) && hi == 5000, "update widens");
    CHECK(vix.SetPartition(2, 4) == gxv_NO_ERROR && vix.SetFromMembers(1) == gxv_NO_ERROR, "partition, members");
    CHECK(vix.SetPartition(4, 4) == gxv_BAD_ARG && vix.SetPartition(1, 0) == gxv_BAD_ARG, "bad partitions");
    vix.Close();
  }
  {
    gxVectorIndex vix;
    unsigned lo, hi;
    CHECK(vix.Open(f.c_str()) == gxv_NO_ERROR, "reopen");
    CHECK(vix.PartitionId() == 2 && vix.PartitionCount() == 4 && (vix.FeaturesOptional() & gxvOptFromMembers),
	  "fields kept");
    CHECK(vix.TagRange(1, lo, hi) && lo == 1000 && hi == 5000, "ranges kept (%u..%u)", lo, hi);
    CHECK(vix.Repair() == gxv_NO_ERROR && vix.TagRange(1, lo, hi) && hi == 5000 && lo == 1000, "repair: live nodes");
    CHECK(vix.Delete(3) == gxv_NO_ERROR && vix.Repair() == gxv_NO_ERROR && vix.TagRange(1, lo, hi) && hi == 1048,
	  "repair narrows to the live nodes (%u)", hi);
    vix.Close();
  }
  // A later version's required feature, codec or layout: refused, the file unchanged
  for(long off : { 140L, 148L, 152L }) {
    unsigned keep = get32_at(f, off);
    put32_at(f, off, off == 140 ? 0x80u : 1u);
    gxVectorIndex vix;
    CHECK(vix.Open(f.c_str()) == gxv_UNSUPPORTED, "unknown field at %ld: unsupported", off);
    put32_at(f, off, keep);
  }
  {
    // reserved bytes are ignored; an optional bit this library does not know is kept as it is
    put32_at(f, 220, 0x12345678u);
    unsigned opt = get32_at(f, 144);
    put32_at(f, 144, opt | 0x100u);
    gxVectorIndex vix;
    CHECK(vix.Open(f.c_str()) == gxv_NO_ERROR && (vix.FeaturesOptional() & 0x100u), "reserved and optional bits");
    vix.Close();
  }
  CHECK(std::string(gxVectorIndex::ErrorMessage(gxv_INDEX_FULL)).find("4,294,967,280") != std::string::npos,
	"index full message");
  CHECK(gxvMaxNodes == 4294967280ULL, "node cap");
  remove(f.c_str());
}

static void test_phase_c_files()
{
  // Files made by the phase A-C library (utils/vix_test/data/make_phase_c_files.cpp): they open, search the
  // same as there, and have no tag ranges until Repair()
  // file addresses are 32 or 64 bits by the build (DATAREEL_64BIT_DATABASE): a set of files for each
  // (the set whose file header is this build's: the index header starts right after it)
  size_t fhs = 0;
  {
    gxDatabase probe;
    std::string pf = path("fhs_probe.gxd");
    if(probe.Create(pf.c_str()) == gxDBASE_NO_ERROR) { fhs = probe.FileHeaderSize(); probe.Close(); }
    remove(pf.c_str());
  }
  std::string dd = data_dir + "/fau32";
  {
    std::ifstream t(data_dir + "/fau64/phase_c_f32_l2.vix", std::ios::binary);
    std::vector<char> b(fhs + 8, 0);
    if(t.read(b.data(), (std::streamsize)b.size()) && memcmp(b.data() + fhs, "GXVIX74", 7) == 0) dd = data_dir + "/fau64";
  }
  std::ifstream ex(dd + "/phase_c_expected.txt");
  CHECK(ex.good(), "no %s/phase_c_expected.txt", dd.c_str());
  if(!ex.good()) return;
  std::map<std::string, std::vector<std::string> > want;
  std::map<std::string, std::string> counts;
  std::string line;
  while(std::getline(ex, line)) {
    std::istringstream is(line);
    std::string name, q;
    is >> name >> q;
    if(q == "count") counts[name] = line;
    else want[name].push_back(line);
  }
  CHECK(want.size() == 4, "4 files expected");
  for(auto &w : want) {
    std::string src = dd + "/" + w.first, f = path(("old_" + w.first).c_str());
    std::ifstream in(src, std::ios::binary);
    std::ofstream out(f, std::ios::binary | std::ios::trunc);
    out << in.rdbuf();
    out.close();
    gxVectorIndex vix;
    gxvError oe = vix.Open(f.c_str());
    CHECK(oe == gxv_NO_ERROR, "%s opens (%s, copied from %s)", w.first.c_str(), gxVectorIndex::ErrorMessage(oe), src.c_str());
    unsigned lo, hi;
    CHECK(vix.FeaturesRequired() == 0 && vix.FeaturesOptional() == 0 && !vix.TagRange(1, lo, hi) &&
	  !vix.TagRangeExcludes(2, 0, 1), "%s: no tag ranges", w.first.c_str());
    for(size_t qi = 0; qi < w.second.size(); qi++) {
      std::vector<float> q(8);
      for(int d = 0; d < 8; d++) q[d] = (float)cos(qi * 0.91 + d * 0.7);
      std::vector<gxvResult> r;
      vix.Search(q.data(), 10, r, 64);
      char b[64];
      std::string got = w.first + " q" + std::to_string(qi);
      for(auto &h : r) { snprintf(b, sizeof(b), " %lld:%.9g", h.id, h.distance); got += b; }
      CHECK(got == w.second[qi], "%s: the same results as the phase C library (query %zu)", w.first.c_str(), qi);
    }
    char cl[256];
    snprintf(cl, sizeof(cl), "%s count %llu deleted %llu", w.first.c_str(), (unsigned long long)vix.Count(),
	     (unsigned long long)vix.Deleted());
    CHECK(counts[w.first] == cl, "%s: counts", w.first.c_str());
    // written to by this library: still no ranges (inserts do not invent them), then Repair() adds them
    std::vector<float> v(8, 0.5f);
    unsigned t[4] = { 1, 2, 3, 4 };
    CHECK(vix.Insert(999999, v.data(), t) == gxv_NO_ERROR && !vix.TagRange(1, lo, hi), "%s: insert", w.first.c_str());
    CHECK(vix.Repair() == gxv_NO_ERROR && vix.TagRange(1, lo, hi) && lo == 2 && hi >= 399, "%s: repair adds ranges (%u..%u)",
	  w.first.c_str(), lo, hi);
    std::string rep;
    CHECK(vix.Verify(0, 0, 0, &rep) == 0, "%s verify: %.200s", w.first.c_str(), rep.c_str());
    vix.Close();
    CHECK(vix.Open(f.c_str()) == gxv_NO_ERROR && vix.TagRange(1, lo, hi), "%s: ranges kept", w.first.c_str());
    vix.Close();
    remove(f.c_str());
  }
}

// 11/14/2026 (phase D): gxVectorSet and the .vixset manifest
static void test_vector_set()
{
  const unsigned dim = 16, days = 5, per_day = 600;
  std::string sdir = path("set1");
  {                                                   // (std::filesystem: the same on Linux and Windows)
    std::error_code ec;
    std::filesystem::remove_all(sdir, ec);
    std::filesystem::create_directories(sdir + "/by day", ec);
  }
  Clusters cl(dim, 12, 41);
  std::vector<std::vector<float> > all;
  std::vector<std::pair<long long, unsigned> > meta;   // id, day
  std::vector<std::string> rels;
  for(unsigned d = 0; d < days; d++) {
    char rel[64]; snprintf(rel, sizeof(rel), "by day/2026111%u.vix", d);
    rels.push_back(rel);
    gxVectorIndex vix;
    CHECK(vix.Create((sdir + "/" + rel).c_str(), dim, gxvFLOAT32, gxvL2, 8, "set-test", 100) == gxv_NO_ERROR, "create day %u", d);
    std::vector<float> v(dim);
    for(unsigned i = 0; i < per_day; i++) {
      cl.Point(v.data());
      long long id = (long long)d * 100000 + i;
      unsigned t[4] = { i % 3, 0, 20261110 + d, 0 };
      CHECK(vix.Insert(id, v.data(), t) == gxv_NO_ERROR, "insert");
      all.push_back(v); meta.push_back(std::make_pair(id, 20261110 + d));
    }
    vix.Close();
  }
  std::string man = sdir + "/news.vixset";
  {
    gxVectorSet set;
    CHECK(set.Create(man.c_str(), "news", dim, gxvFLOAT32, gxvL2, "set-test") == gxv_NO_ERROR, "create set");
    for(auto &r : rels) CHECK(set.AddFile(r.c_str()) == gxv_NO_ERROR, "add %s: %s", r.c_str(), set.Error().c_str());
    CHECK(set.AddFile(rels[0].c_str()) == gxv_DUPLICATE_ID, "added twice");
    CHECK(set.AddFile("../x.vix") == gxv_BAD_ARG && set.AddFile("/tmp/x.vix") == gxv_BAD_ARG, "unsafe paths refused");
  }
  // Exact answers for checking: brute force over every node (L2), ties by (distance, day file, id)
  auto brute = [&](const float *q, unsigned k, unsigned lo, unsigned hi) {
    std::vector<std::pair<float, std::pair<unsigned, long long> > > d;
    for(size_t n = 0; n < all.size(); n++) {
      if(meta[n].second < lo || meta[n].second > hi) continue;
      float s2 = 0;
      for(unsigned j = 0; j < dim; j++) { float x = q[j] - all[n][j]; s2 += x * x; }
      d.push_back(std::make_pair(s2, std::make_pair((unsigned)(meta[n].first / 100000), meta[n].first)));
    }
    std::sort(d.begin(), d.end());
    if(d.size() > k) d.resize(k);
    return d;
  };
  for(unsigned mo : { 16u, 2u }) {
    gxVectorSet set;
    CHECK(set.Open(man.c_str(), "set-test", mo) == gxv_NO_ERROR, "open set: %s", set.Error().c_str());
    CHECK(set.Files().size() == days && set.Files()[3].has_tag2 && set.Files()[3].tag2_min == 20261113 &&
	  set.Files()[3].nodes == per_day, "manifest fields");
    std::vector<float> q(dim);
    double hit = 0, tot = 0;
    for(int qi = 0; qi < 40; qi++) {
      cl.Point(q.data());
      gxvSetSearch o;
      o.k = 10; o.ef = 128; o.threads = (qi % 2) ? 4 : 1;
      std::vector<gxvSetResult> r, r1;
      CHECK(set.Search(q.data(), o, r) == gxv_NO_ERROR, "search");
      o.threads = (qi % 2) ? 1 : 3;
      CHECK(set.Search(q.data(), o, r1) == gxv_NO_ERROR, "search again");
      bool same = r.size() == r1.size();
      for(size_t i = 0; same && i < r.size(); i++) same = r[i].id == r1[i].id && r[i].file == r1[i].file && r[i].distance == r1[i].distance;
      CHECK(same, "the same results for any number of threads (max_open %u)", mo);
      for(size_t i = 1; i < r.size(); i++)
	CHECK(r[i - 1].distance < r[i].distance || (r[i - 1].distance == r[i].distance &&
	      (r[i - 1].file < r[i].file || (r[i - 1].file == r[i].file && r[i - 1].id < r[i].id))), "order (distance, file, id)");
      auto b = brute(q.data(), 10, 0, 0xFFFFFFFFu);
      std::set<long long> want;
      for(auto &x : b) want.insert(x.second.second);
      for(auto &x : r) { if(want.count(x.id)) hit++; CHECK(set.FullPath(x.file).find(rels[x.id / 100000]) != std::string::npos, "file of a result"); }
      tot += b.size();
      // a day range: files outside it are skipped unopened
      o.day_range = 1; o.day_lo = 20261111; o.day_hi = 20261112; o.threads = 2;
      CHECK(set.Search(q.data(), o, r) == gxv_NO_ERROR && set.Skipped() == 3, "day range: 3 files skipped (%llu)", set.Skipped());
      for(auto &x : r) CHECK(x.tags[2] >= 20261111 && x.tags[2] <= 20261112, "a result outside the day range");
    }
    CHECK(hit / tot >= 0.95, "set recall %.3f (max_open %u)", hit / tot, mo);
    if(mo == 2) CHECK(set.Opened() >= 3, "the LRU reopened files (%llu)", set.Opened());
  }
  // The live file: inserts between two searches are found, read-only (reopened when changed) and attached
  {
    gxVectorSet set;
    CHECK(set.Open(man.c_str(), 0, 8) == gxv_NO_ERROR, "open for live");   // every file stays open: the live one is reloaded
    std::vector<float> q(dim, 0.25f);
    gxvSetSearch o; o.k = 1; o.ef = 64;
    std::vector<gxvSetResult> r;
    CHECK(set.Search(q.data(), o, r) == gxv_NO_ERROR && !r.empty() && r[0].distance > 0, "before the live insert");
    {
      gxVectorIndex w;
      CHECK(w.Open((sdir + "/" + rels[4]).c_str()) == gxv_NO_ERROR, "writer opens the live day");
      unsigned t[4] = { 0, 0, 20261114, 0 };
      CHECK(w.Insert(777777, q.data(), t) == gxv_NO_ERROR, "live insert");
      w.Close();
    }
    gxvError le = set.Search(q.data(), o, r);
    CHECK(le == gxv_NO_ERROR && !r.empty() && r[0].id == 777777 && r[0].distance == 0 &&
	  set.Reloaded() >= 1, "the read-only set sees the live insert (err %d, reloaded %llu, opened %llu, id %lld, %s)",
	  (int)le, set.Reloaded(), set.Opened(), r.empty() ? -1LL : r[0].id, set.Error().c_str());
    gxVectorIndex w;
    CHECK(w.Open((sdir + "/" + rels[4]).c_str()) == gxv_NO_ERROR, "writer opens again");
    CHECK(set.AttachOpen(rels[4].c_str(), &w) == gxv_NO_ERROR, "attach the writer's index");
    std::vector<float> q2(dim, -0.5f);
    unsigned t[4] = { 0, 0, 20261114, 0 };
    CHECK(w.Insert(888888, q2.data(), t) == gxv_NO_ERROR, "insert into the attached index");
    CHECK(set.Search(q2.data(), o, r) == gxv_NO_ERROR && !r.empty() && r[0].id == 888888, "the attached index is searched as it is");
    CHECK(set.AttachOpen(rels[4].c_str(), 0) == gxv_NO_ERROR, "detach");
    w.Close();
    CHECK(set.Refresh() == gxv_NO_ERROR && set.Files()[4].nodes == per_day + 2, "refresh counts (%llu)", set.Files()[4].nodes);
  }
  // A missing file is skipped and counted, unless strict
  {
    std::string moved = sdir + "/" + rels[0], to = sdir + "/away.vix";
    CHECK(rename(moved.c_str(), to.c_str()) == 0, "move day 0 away");
    gxVectorSet set;
    CHECK(set.Open(man.c_str()) == gxv_NO_ERROR, "open with a file gone");
    std::vector<float> q(dim, 0.1f);
    gxvSetSearch o; o.k = 5;
    std::vector<gxvSetResult> r;
    CHECK(set.Search(q.data(), o, r) == gxv_NO_ERROR && set.Missing() == 1 && r.size() == 5, "missing file skipped");
    for(auto &x : r) CHECK(x.file != 0, "no result from the missing file");
    o.strict = 1;
    CHECK(set.Search(q.data(), o, r) == gxv_NOT_OPEN, "strict: a missing file is an error");
    rename(to.c_str(), moved.c_str());
  }
  // A file replaced by one that does not match its crc is refused
  {
    std::string f = sdir + "/" + rels[1];
    remove(f.c_str());
    gxVectorIndex vix;
    CHECK(vix.Create(f.c_str(), dim, gxvFLOAT32, gxvL2, 12, "set-test", 100) == gxv_NO_ERROR, "replace day 1 (M 12)");
    std::vector<float> v(dim, 0.3f);
    vix.Insert(5, v.data());
    vix.Close();
    gxVectorSet set;
    CHECK(set.Open(man.c_str()) == gxv_NO_ERROR, "open");
    gxvSetSearch o; o.k = 3;
    std::vector<gxvSetResult> r;
    CHECK(set.Search(v.data(), o, r) == gxv_SPEC_MISMATCH, "replaced file refused by its crc: %s", set.Error().c_str());
  }
  // Manifests that are not whole or hold bad lines
  auto bad_manifest = [&](const std::string &body, gxvError want, const char *what) {
    std::string p = sdir + "/bad.vixset";
    FILE *fp = fopen(p.c_str(), "wb");
    fwrite(body.data(), 1, body.size(), fp);
    fclose(fp);
    gxVectorSet set;
    gxvError e = set.Open(p.c_str());
    CHECK(e == want, "%s: %s (%d)", what, set.Error().c_str(), (int)e);
  };
  std::string head = "set x\nversion 1\nspec set-test\ndim 16\nelement f32\nmetric l2\ncodec 0\n";
  bad_manifest(head + "file ../up.vix nodes=1 deleted=0 tag2=none crc=0\n", gxv_BAD_FILE, "a .. path");
  bad_manifest(head + "file /abs.vix nodes=1 deleted=0 tag2=none crc=0\n", gxv_BAD_FILE, "an absolute path");
  bad_manifest(head + "file a.vix deleted=0 tag2=none crc=0\n", gxv_BAD_FILE, "no nodes=");
  bad_manifest(head + "file a.vix nodes=1 deleted=0 tag2=none\n", gxv_BAD_FILE, "no crc");
  bad_manifest(head + "shards 4\n", gxv_BAD_FILE, "an unknown line (a later version)");
  bad_manifest("set x\nversion 2\n", gxv_BAD_VERSION, "a later manifest version");
  bad_manifest(head.substr(0, head.find("dim")), gxv_BAD_FILE, "not whole");
  bad_manifest("set x\nversion 1\nspec set-test\ndim 16\nelement f8\nmetric l2\n", gxv_UNSUPPORTED, "an unknown element");
  bad_manifest(head + "file with space/a b.vix nodes=1 deleted=0 tag2=3..4 crc=0\n", gxv_NO_ERROR, "a path with spaces");
  {
    std::error_code ec;
    std::filesystem::remove_all(sdir, ec);
  }
}

// 11/14/2026 (phase D item 5): the gxvmember extractor and the rebuild from members
static void test_member_extractor()
{
  const unsigned dim = 8;
  std::string dataf = path("members.gxd"), ixa = path("members_a.vix"), ixb = path("members_b.vix");
  remove(dataf.c_str()); remove(ixa.c_str()); remove(ixb.c_str());
  gxVectorIndex shape;
  CHECK(shape.Create(ixa.c_str(), dim, gxvFLOAT32, gxvL2, 8, "mtest", 100) == gxv_NO_ERROR, "create a");
  shape.Close();
  // POD-style records: 8 bytes of text, the member at offset 8, the day at offset 8 + MemberSize()
  gxVectorIndex probe;
  probe.Open(ixa.c_str());
  const unsigned msz = probe.MemberSize();
  gxDatabase db;
  CHECK(db.Create(dataf.c_str()) == gxDBASE_NO_ERROR, "data file");
  std::vector<FAU_t> addr;
  std::vector<unsigned char> mem, rec;
  std::vector<float> v(dim);
  for(unsigned i = 0; i < 120; i++) {
    for(unsigned d = 0; d < dim; d++) v[d] = (float)sin(i * 0.3 + d);
    probe.MakeMember(v.data(), mem);
    rec.assign(8, (unsigned char)i);
    rec.insert(rec.end(), mem.begin(), mem.end());
    unsigned day = 20261101 + i % 7;
    rec.insert(rec.end(), (unsigned char *)&day, (unsigned char *)&day + 4);
    if(i % 10 == 9) memset(rec.data() + 8, 0, 4);      // no member: not in the index
    FAU_t a = db.Alloc((__ULWORD__)rec.size());
    db.Write(rec.data(), (__ULWORD__)rec.size(), a);
    addr.push_back(a);
  }
  probe.Close();
  // the extractor, object by object (as an incremental copy would), then deletes
  const gxrExtractorOps *ops = gxvMemberExtractor();
  CHECK(ops && ops->abi_version == GXR_EXTRACTOR_ABI, "extractor ops");
  char e[256];
  std::string cfg = "member=offset:8 tags=-,-,offset:" + std::to_string(8 + msz) + ",-";
  void *h = ops->Open(ixa.c_str(), dataf.c_str(), cfg.c_str(), e, sizeof(e));
  CHECK(h != 0, "extractor open: %s", e);
  if(!h) return;
  std::vector<unsigned char> b;
  for(FAU_t a : addr) {
    unsigned n = db.ObjectLength(a); b.resize(n); db.Read(b.data(), n, a);
    CHECK(ops->Insert(h, (uint64_t)a, b.data(), n, e, sizeof(e)) == 0, "insert: %s", e);
  }
  CHECK(ops->Count(h) == 108, "12 records without a member are left out (%llu)", (unsigned long long)ops->Count(h));
  for(size_t i = 0; i < addr.size(); i += 5) {
    unsigned n = db.ObjectLength(addr[i]); b.resize(n); db.Read(b.data(), n, addr[i]);
    CHECK(ops->Remove(h, (uint64_t)addr[i], b.data(), n, e, sizeof(e)) == 0, "remove: %s", e);
    db.Delete(addr[i]);
  }
  CHECK(ops->Remove(h, 999999, 0, 0, e, sizeof(e)) == 0, "removing a node that is not there is not an error");
  CHECK(ops->Flush(h, 1, e, sizeof(e)) == 0, "flush");
  ops->Close(h);
  db.Close();
  // a rebuild from the same records: the same live nodes, and two rebuilds are the same bytes
  gxVectorIndex like;
  CHECK(like.Open(ixa.c_str(), gxDBASE_READONLY) == gxv_NO_ERROR, "open a");
  unsigned long long na = 0, nb = 0, recs = 0;
  unsigned long long da = like.LogicalDigest(&na);
  std::string why;
  CHECK(gxvRebuildFromMembers(dataf, ixb, like, cfg, recs, why) == 0, "rebuild: %s", why.c_str());
  std::string ixc = path("members_c.vix");
  CHECK(gxvRebuildFromMembers(dataf, ixc, like, cfg, recs, why) == 0, "rebuild again: %s", why.c_str());
  like.Close();
  gxVectorIndex rb;
  CHECK(rb.Open(ixb.c_str(), gxDBASE_READONLY) == gxv_NO_ERROR, "open b");
  CHECK(rb.LogicalDigest(&nb) == da && nb == na && na > 0, "rebuild = extractor (logical; %llu nodes)", na);
  CHECK(recs == na && (rb.FeaturesOptional() & gxvOptFromMembers), "records and the members flag");
  unsigned lo, hi;
  CHECK(rb.TagRange(2, lo, hi) && lo == 20261101 && hi == 20261107, "day range");
  rb.Close();
  std::ifstream fb(ixb, std::ios::binary), fc(ixc, std::ios::binary);
  std::string sb((std::istreambuf_iterator<char>(fb)), std::istreambuf_iterator<char>()), sc((std::istreambuf_iterator<char>(fc)), std::istreambuf_iterator<char>());
  CHECK(!sb.empty() && sb == sc, "two rebuilds are the same bytes");
  gxvMemberSpec sp;
  CHECK(sp.Parse("member=offset", why) != 0 && sp.Parse("member=infohog:1 tags=a:1", why) != 0 &&
	sp.Parse("tags=-", why) != 0 && sp.Parse("member=infohog:1 color=red", why) != 0, "bad configs refused");
  remove(dataf.c_str()); remove(ixa.c_str()); remove(ixb.c_str()); remove(ixc.c_str());
}

static void test_merge(const ElemType &et)
{
  const unsigned dim = 32, per = 3000;
  Clusters cl(dim, 60, 31);
  std::vector<std::string> days;
  std::vector<float> v(dim);
  long long next = 1;
  for(int d = 0; d < 3; d++) {
    std::string f = tagged(et, std::string("day") + (char)('1' + d) + ".vix");
    days.push_back(f);
    gxVectorIndex vix;
    vix.Create(f.c_str(), dim, et.type, gxvCOSINE, 16, "graph/v1");
    for(unsigned i = 0; i < per; i++) {
      cl.Point(v.data());
      unsigned t[4] = { i % 3, 72, 20261101u + d, 0 };
      vix.Insert(next++, v.data(), t);
    }
    if(d == 1) for(long long id = per + 1; id < per + 101; id++) vix.Delete(id);
    vix.Close();
  }
  std::string out = tagged(et, "month.vix"), rep;
  CHECK(gxVectorIndex::Merge(out.c_str(), days, "graph/v1", 0, 200, &rep) == gxv_NO_ERROR,
	"merge: %s", rep.c_str());
  {
    gxVectorIndex vix;
    CHECK(vix.Open(out.c_str(), gxDBASE_READONLY, "graph/v1") == gxv_NO_ERROR, "open merged");
    CHECK(vix.Count() == 3 * per - 100 && vix.Deleted() == 0 && vix.GraphSaved() &&
	  vix.Metric() == gxvCOSINE && vix.ElementType() == et.type, "merged counts %llu", vix.Count());
    CHECK(!vix.Contains(per + 50) && vix.Contains(per + 200), "merged: tombstones left out");
    std::vector<std::vector<float> > qs(100, std::vector<float>(dim));
    for(auto &q : qs) cl.Point(q.data());
    double r = mean_recall(vix, qs, 10, 64);
    printf("%s: %llu nodes from 3 day files, recall@10 %.4f\n", label(et, "merge").c_str(), vix.Count(), r);
    CHECK(r >= 0.95, "merged recall %.4f", r);
    gxvFilter day2;
    day2.Tag(2, 20261102);
    double rf = mean_recall(vix, qs, 10, 64, &day2);
    CHECK(rf >= 0.90, "merged, one day: recall %.4f", rf);
    std::string vr;
    CHECK(vix.Verify(0, 0, 0, &vr) == 0, "merged verify: %.300s", vr.c_str());
  }
  std::string out2 = tagged(et, "month2.vix");
  CHECK(gxVectorIndex::Merge(out2.c_str(), days, "graph/v2") == gxv_SPEC_MISMATCH, "merge: spec mismatch");
  std::vector<std::string> twice = { days[0], days[0] };
  CHECK(gxVectorIndex::Merge(out2.c_str(), twice) == gxv_DUPLICATE_ID, "merge: duplicate ids");
  {
    std::string other = tagged(et, "day_other.vix");
    gxVectorIndex vix;
    vix.Create(other.c_str(), 16, et.type, gxvCOSINE, 16, "graph/v1");
    vix.Close();
    std::vector<std::string> mixed = { days[0], other };
    gxvError me = gxVectorIndex::Merge(out2.c_str(), mixed);
    CHECK(me == gxv_BAD_ARG, "merge: other dimension: %s", gxVectorIndex::ErrorMessage(me));
    // Another element type, same dimension: refused too
    vix.Create(other.c_str(), dim, et.type == gxvFLOAT32 ? gxvINT8 : gxvFLOAT32, gxvCOSINE, 16, "graph/v1");
    vix.Close();
    me = gxVectorIndex::Merge(out2.c_str(), mixed);
    CHECK(me == gxv_BAD_ARG, "merge: other element type: %s", gxVectorIndex::ErrorMessage(me));
    remove(other.c_str());
  }
  for(const std::string &d : days) remove(d.c_str());
  remove(out.c_str());
  remove(out2.c_str());
}

// ----------------------------------------------------------- //
// Vector members (phase C part 2): InfoHog records that carry their
// vector, kept in step with a gxVectorIndex under one journal
// ----------------------------------------------------------- //
struct ItemName {
  ItemName() { memset(name, 0, sizeof(name)); }
  ItemName(const char *s) { memset(name, 0, sizeof(name)); strncpy(name, s, sizeof(name) - 1); }
  int operator==(const ItemName &o) const { return strcmp(name, o.name) == 0; }
  int operator<(const ItemName &o) const { return strcmp(name, o.name) < 0; }
  int operator>(const ItemName &o) const { return strcmp(name, o.name) > 0; }
  char name[32];
};
typedef InfoHog<ItemName> Item;
typedef InfoHogKey<ItemName> ItemKey;
enum { IT_NAME = 0, IT_VEC = 1, IT_TEXT = 2, IT_MEMBERS = 3 };
static const unsigned MEM_DIM = 12;

static int open_items(POD &pod, const std::string &data, const std::string &index)
{
  ItemKey key_type;
  return pod.Open(data.c_str(), index.c_str(), key_type, InfoHogNodeOrder, gxDBASE_READWRITE,
		  InfoHogUseIndexFile, InfoHogStaticArea, InfoHogNumTrees) == gxDBASE_NO_ERROR;
}

static void item_vec(long long i, float *v)
{
  for(unsigned d = 0; d < MEM_DIM; d++) v[d] = (float)((i * 37 + d * 11) % 101) / 9.0f - 5.0f;
}

// Add one record with its vector member and put the member in the index
static long long add_item(POD &pod, gxVectorIndex &vix, long long i, const std::string &text)
{
  char nm[32];
  snprintf(nm, sizeof(nm), "item%06lld", i);
  ItemName key(nm);
  Item it(&pod, key, IT_MEMBERS);
  std::vector<float> v(MEM_DIM);
  item_vec(i, v.data());
  std::vector<unsigned char> m;
  if(vix.MakeMember(v.data(), m) != gxv_NO_ERROR) return -1;
  it.SetMember(m.data(), (unsigned)m.size(), IT_VEC);
  it.SetMember(text.c_str(), (unsigned)text.size() + 1, IT_TEXT);
  if(!it.WriteObject(0)) return -1;
  unsigned t[4] = { (unsigned)(i % 5), 0, 0, 0 };
  if(vix.InsertMember((long long)it.ObjectAddress(), m.data(), (unsigned)m.size(), t) != gxv_NO_ERROR) return -1;
  return (long long)it.ObjectAddress();
}

// Every record has a live node holding its member, and nothing else is
// in the index. Returns the number of problems.
static int items_match(POD &pod, gxVectorIndex &vix, unsigned long long *records = 0)
{
  int bad = 0;
  unsigned long long n = 0;
  ItemKey key, compare;
  gxBtree *btx = pod.Index();
  if(btx->FindFirst(key)) {
    do {
      Item it(&pod, IT_MEMBERS);
      n++;
      if(!it.ReadObject(key.ObjectID())) { bad++; continue; }
      if(!vix.MemberMatches((long long)key.ObjectID(), it.GetMember(IT_VEC), it.GetMemberLen(IT_VEC))) bad++;
    } while(btx->FindNext(key, compare));
  }
  if(n != vix.Count()) bad++;
  if(records) *records = n;
  return bad;
}

struct ItemResolve { POD *pod; gxVectorIndex *vix; };
static int item_exists(long long id, void *data)
{
  ItemResolve *r = (ItemResolve *)data;
  Item it(r->pod, IT_MEMBERS);
  if(!it.ReadObject((FAU_t)id)) return 0;
  return r->vix->MemberMatches(id, it.GetMember(IT_VEC), it.GetMemberLen(IT_VEC));
}

static void test_members(const ElemType &et)
{
  std::string data = tagged(et, "items.ihd"), index = tagged(et, "items.ihx");
  std::string vf = tagged(et, "items.vix"), jf = tagged(et, "items.jnl");
  remove(data.c_str()); remove(index.c_str()); remove(vf.c_str()); remove(jf.c_str());
  std::string L = label(et, "members");
  const char *lab = L.c_str();
  gxdJournal j;
  CHECK(j.Open(jf.c_str()) == gxDBASE_NO_ERROR, "%s: journal", lab);
  POD pod;
  CHECK(open_items(pod, data, index), "%s: open POD", lab);
  gxVectorIndex vix;
  CHECK(vix.Create(vf.c_str(), MEM_DIM, et.type, gxvL2, 16, "items/v1") == gxv_NO_ERROR, "%s: create", lab);
  vix.Database()->Sync();
  pod.OpenDataFile()->Sync();
  pod.IndexDBPtr()->Sync();
  CHECK(j.Attach(pod.OpenDataFile()) == gxDBASE_NO_ERROR && j.Attach(pod.Index()) == gxDBASE_NO_ERROR &&
	j.Attach(vix.Database()) == gxDBASE_NO_ERROR, "%s: attach data, index and vix", lab);
  CHECK(vix.MemberSize() == gxvMemberHead + vix.CodeSize(), "%s: member size", lab);

  // 1. Records and nodes in one transaction
  j.Begin();
  int fails = 0;
  for(long long i = 0; i < 60; i++) if(add_item(pod, vix, i, "first text") < 0) fails++;
  CHECK(j.Commit() == gxDBASE_NO_ERROR && fails == 0, "%s: 60 records committed (%d failed)", lab, fails);
  unsigned long long recs = 0;
  CHECK(items_match(pod, vix, &recs) == 0 && recs == 60, "%s: index matches the records (%llu)", lab, recs);
  {
    // The nearest node to a record's vector is that record
    Item it(&pod, IT_MEMBERS);
    ItemName k("item000007");
    Item probe(&pod, k, IT_MEMBERS);
    std::vector<float> q(MEM_DIM);
    std::vector<gxvResult> r;
    CHECK(probe.FindObject() && it.ReadObject(probe.ObjectAddress()) &&
	  vix.MemberVector(it.GetMember(IT_VEC), it.GetMemberLen(IT_VEC), q.data()) == gxv_NO_ERROR &&
	  vix.Exact(q.data(), 1, r) == gxv_NO_ERROR && r.size() == 1 &&
	  r[0].id == (long long)probe.ObjectAddress() && r[0].distance == 0.0f, "%s: nearest is the record", lab);
  }

  // 2. A record that grows moves (new address): Rename() follows it. A new
  // vector: UpdateMember(). A deleted record: Delete().
  j.Begin();
  {
    ItemName k3("item000003");
    Item a(&pod, k3, IT_MEMBERS);
    Item it(&pod, IT_MEMBERS);
    CHECK(a.FindObject() && it.ReadObject(a.ObjectAddress()), "%s: find item 3", lab);
    long long old_addr = (long long)it.ObjectAddress();
    std::string longer(400, 'x');
    CHECK(it.ChangeMember(longer.c_str(), (unsigned)longer.size() + 1, IT_TEXT), "%s: grow item 3", lab);
    long long new_addr = (long long)it.ObjectAddress();
    CHECK(new_addr != old_addr && vix.Rename(old_addr, new_addr) == gxv_NO_ERROR &&
	  !vix.Contains(old_addr) && vix.Contains(new_addr), "%s: moved record renamed (%lld -> %lld)", lab,
	  old_addr, new_addr);
    ItemName k4("item000004");
    Item b(&pod, k4, IT_MEMBERS);
    Item it4(&pod, IT_MEMBERS);
    CHECK(b.FindObject() && it4.ReadObject(b.ObjectAddress()), "%s: find item 4", lab);
    std::vector<float> nv(MEM_DIM);
    item_vec(999, nv.data());
    std::vector<unsigned char> m;
    CHECK(vix.MakeMember(nv.data(), m) == gxv_NO_ERROR &&
	  it4.ChangeMember(m.data(), (unsigned)m.size(), IT_VEC), "%s: change item 4's vector", lab);
    CHECK(vix.UpdateMember((long long)it4.ObjectAddress(), m.data(), (unsigned)m.size()) == gxv_NO_ERROR,
	  "%s: update member", lab);
    ItemName k5("item000005");
    Item c(&pod, k5, IT_MEMBERS);
    long long addr5 = c.FindObject() ? (long long)c.ObjectAddress() : -1;
    CHECK(addr5 > 0 && c.DeleteObject() && vix.Delete(addr5) == gxv_NO_ERROR, "%s: delete item 5", lab);
  }
  CHECK(j.Commit() == gxDBASE_NO_ERROR, "%s: commit changes", lab);
  CHECK(items_match(pod, vix, &recs) == 0 && recs == 59, "%s: index matches after changes (%llu)", lab, recs);

  // 3. Rolled back: neither the records nor the nodes stay
  j.Begin();
  for(long long i = 100; i < 110; i++) add_item(pod, vix, i, "rolled back");
  CHECK(j.Rollback() == gxDBASE_NO_ERROR && vix.Reload() == gxv_NO_ERROR, "%s: rollback", lab);
  CHECK(items_match(pod, vix, &recs) == 0 && recs == 59, "%s: index matches after rollback (%llu)", lab, recs);

  // 4. Members of another index are refused
  {
    std::string of = tagged(et, "items_other.vix");
    gxVectorIndex other;
    other.Create(of.c_str(), MEM_DIM, et.type, gxvL2, 16, "items/v2");
    std::vector<float> v(MEM_DIM);
    item_vec(1, v.data());
    std::vector<unsigned char> m;
    other.MakeMember(v.data(), m);
    CHECK(vix.InsertMember(424242, m.data(), (unsigned)m.size()) == gxv_SPEC_MISMATCH, "%s: other spec refused", lab);
    CHECK(vix.InsertMember(424242, m.data(), (unsigned)m.size() - 1) == gxv_BAD_ARG, "%s: short member refused", lab);
    other.Close();
    remove(of.c_str());
    gxVectorIndex odim;
    odim.Create(of.c_str(), MEM_DIM + 1, et.type, gxvL2, 16, "items/v1");
    std::vector<float> v2(MEM_DIM + 1, 1.0f);
    odim.MakeMember(v2.data(), m);
    CHECK(vix.InsertMember(424242, m.data(), (unsigned)m.size()) == gxv_BAD_ARG, "%s: other dimension refused", lab);
    odim.Close();
    remove(of.c_str());
  }
  ItemResolve res = { &pod, &vix };
  std::string rep;
  CHECK(vix.Verify(item_exists, &res, 0, &rep) == 0, "%s: verify with the records: %.300s", lab, rep.c_str());
  j.Close();
  vix.Close();
  pod.Close();

  // 5. A POD record with a fixed vector field
  {
    std::string pf = tagged(et, "parts.gxd"), pv = tagged(et, "parts.vix");
    gxVectorIndex px;
    px.Create(pv.c_str(), MEM_DIM, et.type, gxvCOSINE, 16, "parts/v1");
    struct Part { char name[16]; unsigned char vec[gxvMemberHead + 4 * MEM_DIM]; } part;
    CHECK(px.MemberSize() <= sizeof(part.vec), "%s: member fits the field", lab);
    gxDatabase db;
    db.Create(pf.c_str());
    memset(&part, 0, sizeof(part));
    strcpy(part.name, "bolt");
    std::vector<float> v(MEM_DIM), back(MEM_DIM), dec(MEM_DIM);
    item_vec(42, v.data());
    std::vector<unsigned char> m;
    px.MakeMember(v.data(), m);
    memcpy(part.vec, m.data(), m.size());
    FAU_t a = db.Alloc(sizeof(part));
    db.Write(&part, sizeof(part), a);
    CHECK(px.InsertMember((long long)a, part.vec, px.MemberSize()) == gxv_NO_ERROR, "%s: POD insert member", lab);
    Part rd;
    db.Read(&rd, sizeof(rd), a);
    CHECK(px.MemberMatches((long long)a, rd.vec, px.MemberSize()) &&
	  px.MemberVector(rd.vec, px.MemberSize(), back.data()) == gxv_NO_ERROR &&
	  px.Get((long long)a, dec.data()) == gxv_NO_ERROR && memcmp(back.data(), dec.data(), 4 * MEM_DIM) == 0,
	  "%s: POD record and node hold the same vector", lab);
    db.Close();
    px.Close();
    remove(pf.c_str()); remove(pv.c_str());
  }
  // 11/14/2026 (phase D item 5): the InfoHog records as InfoHog writes them (its count and lengths are
  // sizeof(gxUINT32) wide, 8 bytes on a 64-bit native build): a rebuild from their members finds every node
  // the index holds, with the same vector
  {
    gxVectorIndex src;
    CHECK(src.Open(vf.c_str(), gxDBASE_READONLY) == gxv_NO_ERROR, "%s: reopen for the rebuild", lab);
    std::string rb = tagged(et, "items_rebuilt.vix"), why;
    unsigned long long recs = 0;
    CHECK(gxvRebuildFromMembers(data, rb, src, "member=infohog:" + std::to_string((int)IT_VEC), recs, why) == 0,
	  "%s: rebuild from the InfoHog members: %s", lab, why.c_str());
    CHECK(recs == src.Count() && recs > 0, "%s: rebuild found %llu records with members, the index holds %llu", lab,
	  recs, (unsigned long long)src.Count());
    gxVectorIndex out;
    CHECK(out.Open(rb.c_str(), gxDBASE_READONLY) == gxv_NO_ERROR, "%s: open the rebuild", lab);
    gxDatabase d;
    int same = 1, checked = 0;
    if(d.Open(data.c_str(), gxDBASE_READONLY) == gxDBASE_NO_ERROR) {
      std::vector<float> va(MEM_DIM), vb(MEM_DIM);
      for(FAU_t a = d.FindFirstObject(); a != (FAU_t)0; a = d.FindNextObject(a)) {
	if(!src.Contains((long long)a)) continue;
	checked++;
	if(!out.Contains((long long)a) || src.Get((long long)a, va.data()) != gxv_NO_ERROR ||
	   out.Get((long long)a, vb.data()) != gxv_NO_ERROR || memcmp(va.data(), vb.data(), 4 * MEM_DIM) != 0) same = 0;
      }
      d.Close();
    }
    CHECK(same && checked == (int)src.Count(), "%s: every node rebuilt from its InfoHog record (%d)", lab, checked);
    out.Close();
    src.Close();
    remove(rb.c_str());
  }
  remove(data.c_str()); remove(index.c_str()); remove(vf.c_str()); remove(jf.c_str());
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

static void test_crash(int rounds, const ElemType &et)
{
  const unsigned dim = 32;
  const int batch = 37;
  std::string f = tagged(et, "crash.vix"), jf = tagged(et, "crash.jnl"), pf = tagged(et, "crash.progress");
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
      if(vix.Create(f.c_str(), dim, et.type) != gxv_NO_ERROR) _exit(3);
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
  printf("%s: %d/%d rounds recovered exactly (%ld committed batches in all; %d rounds killed "
	 "after a commit but before its progress note, and kept it)\n", label(et, "crash").c_str(),
	 ok_rounds, rounds, total_committed, after_commit);
  CHECK(ok_rounds == rounds, "%s: %d of %d rounds recovered exactly", label(et, "crash").c_str(),
	ok_rounds, rounds);
}
#endif

#ifndef _WIN32
// Records and their index nodes under one journal: a writer killed at any
// moment leaves records and nodes that still match after Recover()
static void test_members_crash(int rounds, const ElemType &et)
{
  const int batch = 20;
  std::string data = tagged(et, "mcrash.ihd"), index = tagged(et, "mcrash.ihx");
  std::string vf = tagged(et, "mcrash.vix"), jf = tagged(et, "mcrash.jnl"), pf = tagged(et, "mcrash.progress");
  std::mt19937 g(23);
  int ok_rounds = 0;
  long total = 0;
  std::string L = label(et, "members crash");
  const char *lab = L.c_str();
  for(int round = 0; round < rounds; round++) {
    remove(data.c_str()); remove(index.c_str()); remove(vf.c_str()); remove(jf.c_str());
    remove(pf.c_str()); remove((pf + ".tmp").c_str());
    pid_t pid = fork();
    if(pid == 0) {
      gxdJournal j;
      if(j.Open(jf.c_str()) != gxDBASE_NO_ERROR) _exit(2);
      POD pod;
      if(!open_items(pod, data, index)) _exit(3);
      gxVectorIndex vix;
      if(vix.Create(vf.c_str(), MEM_DIM, et.type, gxvL2, 16, "items/v1") != gxv_NO_ERROR) _exit(4);
      vix.Database()->Sync();
      pod.OpenDataFile()->Sync();
      pod.IndexDBPtr()->Sync();
      j.Attach(pod.OpenDataFile());
      j.Attach(pod.Index());
      j.Attach(vix.Database());
      write_progress(pf, 0);
      for(long b = 0;; b++) {
	j.Begin();
	for(int i = 0; i < batch; i++) if(add_item(pod, vix, b * batch + i, "crash test record") < 0) _exit(5);
	if(j.Commit() != gxDBASE_NO_ERROR) _exit(6);
	write_progress(pf, b + 1);
      }
    }
    for(int w = 0; w < 30000 && read_progress(pf) < 0; w++) usleep(1000);
    CHECK(read_progress(pf) >= 0, "%s round %d: child did not start in 30 s", lab, round);
    std::uniform_int_distribution<int> wait_us(20000, 300000);
    usleep(wait_us(g));
    kill(pid, SIGKILL);
    int st = 0;
    waitpid(pid, &st, 0);
    long committed = read_progress(pf);
    CHECK(committed >= 0, "%s round %d: progress note unreadable", lab, round);
    if(committed < 0) continue;
    CHECK(gxdJournal::Recover(jf.c_str()) == gxDBASE_NO_ERROR, "%s round %d: recover", lab, round);
    POD pod;
    gxVectorIndex vix;
    int opened = open_items(pod, data, index) && vix.Open(vf.c_str()) == gxv_NO_ERROR;
    CHECK(opened, "%s round %d: open after the crash", lab, round);
    if(!opened) continue;
    unsigned long long recs = 0;
    int bad = items_match(pod, vix, &recs);
    CHECK(bad == 0, "%s round %d: %d records and nodes differ", lab, round, bad);
    CHECK(recs % batch == 0 && (long)(recs / batch) >= committed, "%s round %d: %llu records, %ld batches committed",
	  lab, round, recs, committed);
    std::string rep;
    ItemResolve res = { &pod, &vix };
    CHECK(vix.Verify(item_exists, &res, 0, &rep) == 0, "%s round %d: verify: %.200s", lab, round, rep.c_str());
    if(bad == 0) ok_rounds++;
    total += committed;
    vix.Close();
    pod.Close();
  }
  printf("%s: %d/%d rounds: records and index nodes match after Recover() (%ld committed batches of %d)\n",
	 lab, ok_rounds, rounds, total, batch);
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
static long rss_mb()
{
  long kb = 0;
#ifndef _WIN32
  FILE *fp = fopen("/proc/self/status", "r");
  if(fp) {
    char line[256];
    while(fgets(line, sizeof(line), fp)) {
      if(strncmp(line, "VmRSS:", 6) == 0) { kb = atol(line + 6); break; }
    }
    fclose(fp);
  }
#endif
  return kb / 1024;
}

static void gate(size_t n, unsigned dim, int uniform, unsigned efc, int live_sec, const ElemType &et,
		 gxvMetric met = gxvL2)
{
  std::string f = path("gate.vix"), jf = path("gate.jnl");
  remove(f.c_str());
  remove(jf.c_str());
  Clusters cl(dim, 1000, 42);
  std::mt19937 ug(43);
  auto point = [&](float *v) { if(uniform) rand_vec(ug, v, dim); else cl.Point(v); };
  gxVectorIndex vix;
  long rss0 = rss_mb();
  vix.Create(f.c_str(), dim, et.type, met, 16, "gate/v1", efc);
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
  printf("gate: %zu x %u (%s, %s, %s), M 16, ef_construction %u: build %.0f s (%.0f us an insert), top level %u\n",
	 n, dim, uniform ? "uniform" : "clustered", et.tag, met == gxvL2 ? "l2" : met == gxvCOSINE ? "cosine" : "ip",
	 efc, build, 1e6 * build / n, vix.TopLevel());
  printf("gate: memory after the build %ld MB (RSS; %ld MB before), file %lld MB\n", rss_mb(), rss0,
	 file_size(f) / (1024 * 1024));
  fflush(stdout);
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
  if(et.type != gxvFLOAT32) {
    // Against the exact answer on the float vectors. The points are made
    // again from the same seeds (Clusters 42, uniform 43), so this needs
    // no copy of the n vectors in memory: a streaming top-k per query.
    Clusters tc(dim, 1000, 42);
    std::mt19937 tg(43);
    std::vector<std::vector<std::pair<float, long long> > > best(qs.size());
    std::vector<float> p(dim), pn(dim);
    std::vector<std::vector<float> > qn = qs; // Queries as the index sees them
    if(met == gxvCOSINE) for(auto &q : qn) gxv_normalize(q.data(), dim);
    for(size_t i = 0; i < n; i++) {
      if(uniform) rand_vec(tg, p.data(), dim); else tc.Point(p.data());
      pn = p;
      if(met == gxvCOSINE) gxv_normalize(pn.data(), dim);
      for(size_t qi = 0; qi < qs.size(); qi++) {
	auto &b = best[qi];
	float d = ref_dist(met, qn[qi].data(), pn.data(), dim);
	std::pair<float, long long> e(d, (long long)i);
	if(b.size() < 20) { b.push_back(e); std::push_heap(b.begin(), b.end()); }
	else if(e < b.front()) { std::pop_heap(b.begin(), b.end()); b.back() = e; std::push_heap(b.begin(), b.end()); }
      }
    }
    for(unsigned ef : { 64u, 200u }) {
      double sum = 0;
      std::vector<gxvResult> res;
      for(size_t qi = 0; qi < qs.size(); qi++) {
	vix.Search(qs[qi].data(), 20, res, ef);
	size_t hit = 0;
	for(const auto &e : best[qi]) for(const gxvResult &r : res) if(r.id == e.second) { hit++; break; }
	sum += (double)hit / best[qi].size();
      }
      printf("gate: recall@20 against float32 exact, ef %u: %.4f\n", ef, sum / qs.size());
    }
  }

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
  printf("gate: open (nodes + graph) %.1f s, graph saved %d, memory %ld MB\n", op, ro.GraphSaved(), rss_mb());
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
  const ElemType *gate_type = &TYPES[0];
  gxvMetric gate_metric = gxvL2;
  for(int i = 1; i < argc; i++) {
    if(!strcmp(argv[i], "--crash-rounds") && i + 1 < argc) rounds = atoi(argv[++i]);
    else if(!strcmp(argv[i], "--bench")) do_bench = 1;
    else if(!strcmp(argv[i], "--gate") && i + 1 < argc) gate_n = atol(argv[++i]);
    else if(!strcmp(argv[i], "--dim") && i + 1 < argc) gate_dim = (unsigned)atoi(argv[++i]);
    else if(!strcmp(argv[i], "--efc") && i + 1 < argc) efc = (unsigned)atoi(argv[++i]);
    else if(!strcmp(argv[i], "--live") && i + 1 < argc) live_sec = atoi(argv[++i]);
    else if(!strcmp(argv[i], "--uniform")) uniform = 1;
    else if(!strcmp(argv[i], "--data") && i + 1 < argc) data_dir = argv[++i];   // 11/14/2026
    else if(!strcmp(argv[i], "--metric") && i + 1 < argc) {
      const char *m = argv[++i];
      if(!strcmp(m, "l2")) gate_metric = gxvL2;
      else if(!strcmp(m, "cosine")) gate_metric = gxvCOSINE;
      else if(!strcmp(m, "ip")) gate_metric = gxvIP;
      else { fprintf(stderr, "--metric l2, cosine or ip\n"); return 2; }
    }
    else if(!strcmp(argv[i], "--type") && i + 1 < argc) {
      const char *t = argv[++i];
      gate_type = 0;
      for(const ElemType &et : TYPES) if(!strcmp(t, et.tag)) gate_type = &et;
      if(!gate_type) { fprintf(stderr, "--type f32, f16 or i8\n"); return 2; }
    }
    else dir = argv[i];
  }
#ifndef _WIN32
  std::string mk = "mkdir -p '" + dir + "'";
  if(system(mk.c_str()) != 0) { }
#endif
  if(gate_n > 0) {
    gate((size_t)gate_n, gate_dim, uniform, efc, live_sec, *gate_type, gate_metric);
    return 0;
  }
  test_kernels();
  test_elements();
  for(const ElemType &et : TYPES) test_search(et);
  test_crud();
  test_journal();
  for(const ElemType &et : TYPES) test_graph(et);
  test_phase_a_files();
  test_format_fields();    // 11/14/2026 (phase D)
  test_phase_c_files();
  test_vector_set();
  test_member_extractor();
  for(const ElemType &et : TYPES) test_merge(et);
  test_concurrent();
  for(const ElemType &et : TYPES) test_members(et);
#ifndef _WIN32
  if(rounds > 0) for(const ElemType &et : TYPES) test_crash(rounds, et);
  if(rounds > 0) for(const ElemType &et : TYPES) test_members_crash(rounds < 10 ? rounds : 10, et);
#endif
  for(const std::string &s : size_lines) printf("file size %s\n", s.c_str());
  if(do_bench) bench();
  printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
