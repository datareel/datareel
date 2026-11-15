// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: testprog.cpp
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
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
USA

DataReel 6.4 example and test: a DataReel 6.x program that keeps its
records in a 6.x gxDatabase file and finds them by vector similarity
with the DataReel 7.x vector index (gxv7VectorIndex, dr7features.h).
The vector index id of each record is its 6.x block address, so a
search result is read straight from the 6.x database.

Build with the 6.x CMake build and DATAREEL7_FEATURES_DIR (see the
README). Usage:

  testprog                      demo + checks in the current folder
  testprog --phase-c DIR        also open the DataReel 7.4 phase C
                                .vix files in DIR (read only) and
                                compare 10 searches each with DIR's
                                phase_c_expected.txt

Returns 0 if every check passes.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "gxdbase.h"
#include "gxstring.h"
#include "dr7features.h"

static int failures = 0;

static void check(int ok, const char *what)
{
  printf("%s %s\n", ok ? "PASS" : "FAIL", what);
  fflush(stdout);
  if(!ok) failures++;
}

// A small repeatable random generator (the same on every platform)
static unsigned long rng_state = 12345;
static float rnd()
{
  rng_state = rng_state * 1103515245UL + 12345UL;
  return (float)((rng_state >> 16) & 0x7fff) / 32768.0f - 0.5f;
}

static float l2(const float *a, const float *b, unsigned dim)
{
  float s = 0;
  for(unsigned i = 0; i < dim; i++) { float d = a[i] - b[i]; s += d * d; }
  return s;
}

// The record kept in the 6.x database
struct Item {
  char name[32];
  float vec[16];
};
const unsigned DIM = 16;

static void demo_and_checks()
{
  const char *dbname = "vector_demo.gxd";
  const char *vixname = "vector_demo.vix";
  remove(dbname); remove(vixname);

  // 1. Records in a 6.x database, vectors in a 7.x vector index
  gxDatabase db;
  check(db.Create(dbname) == gxDBASE_NO_ERROR, "6.x database created");
  gxv7VectorIndex vix;
  int rv = vix.Create(vixname, DIM, gxv7FLOAT32, gxv7L2, 16, "vector-demo-16");
  check(rv == gxv7_NO_ERROR, "7.x vector index created");
  if(rv != gxv7_NO_ERROR) printf("  %s\n", gxv7VectorIndex::ErrorMessage(rv));
  printf("  DataReel 7.x library: %s\n", gxv7VectorIndex::LibraryVersion());
  check(strncmp(gxv7VectorIndex::LibraryVersion(), "7.", 2) == 0, "7.x library version");

  const int N = 2000;
  FAU_t *addr = new FAU_t[N];
  Item *items = new Item[N];
  int ok = 1;
  for(int i = 0; i < N; i++) {
    sprintf(items[i].name, "Item %04d", i);
    for(unsigned d = 0; d < DIM; d++) items[i].vec[d] = rnd();
    addr[i] = db.Alloc(sizeof(Item));
    if(addr[i] == (FAU_t)0 || db.Write(&items[i], sizeof(Item), addr[i]) != gxDBASE_NO_ERROR) ok = 0;
    unsigned tags[gxv7TagCount] = { (unsigned)(i % 10), 0, (unsigned)(i / 100), 0 };
    if(vix.Insert((long long)addr[i], items[i].vec, tags) != gxv7_NO_ERROR) ok = 0;
  }
  check(ok, "2000 records written to the 6.x database and indexed");
  check(vix.Count() == (unsigned long long)N && vix.Dim() == DIM, "Count() and Dim()");

  // 2. Find a record from a noisy copy of its vector and read it back
  float q[DIM];
  for(unsigned d = 0; d < DIM; d++) q[d] = items[1234].vec[d] + 0.001f * rnd();
  gxv7Result res[10];
  unsigned count = 0;
  rv = vix.Search(q, 5, res, count);
  Item found;
  memset(&found, 0, sizeof(found));
  if(rv == gxv7_NO_ERROR && count > 0) db.Read(&found, sizeof(Item), (FAU_t)res[0].id);
  printf("  nearest to a copy of Item 1234: %s (distance %g)\n", found.name, count ? res[0].distance : -1.0f);
  check(rv == gxv7_NO_ERROR && count == 5 && strcmp(found.name, "Item 1234") == 0,
	"Search() finds the record; its address reads it from the 6.x database");

  // 3. Exact() against a brute force search
  int exact_ok = 1;
  for(int t = 0; t < 10 && exact_ok; t++) {
    for(unsigned d = 0; d < DIM; d++) q[d] = rnd();
    float best = 1e30f; int besti = -1;
    for(int i = 0; i < N; i++) { float s = l2(q, items[i].vec, DIM); if(s < best) { best = s; besti = i; } }
    if(vix.Exact(q, 1, res, count) != gxv7_NO_ERROR || count != 1 || res[0].id != (long long)addr[besti]) exact_ok = 0;
  }
  check(exact_ok, "Exact() equals a brute force search (10 queries)");

  // 4. Recall of the approximate search
  unsigned hits = 0, total = 0;
  for(int t = 0; t < 20; t++) {
    for(unsigned d = 0; d < DIM; d++) q[d] = rnd();
    gxv7Result ex[10], ap[10];
    unsigned ne = 0, na = 0;
    vix.Exact(q, 10, ex, ne);
    vix.Search(q, 10, ap, na, 128);
    for(unsigned a = 0; a < ne; a++) {
      total++;
      for(unsigned b = 0; b < na; b++) if(ap[b].id == ex[a].id) { hits++; break; }
    }
  }
  printf("  HNSW recall at 10 (ef 128): %.3f\n", total ? (double)hits / total : 0.0);
  check(total && hits >= total * 9 / 10, "Search() recall >= 0.9");

  // 5. Tag filter, Get(), Delete(), Update()
  gxv7Filter flt;
  flt.Tag(0, 3);
  rv = vix.Search(items[0].vec, 10, res, count, 64, &flt);
  int tags_ok = (rv == gxv7_NO_ERROR && count == 10);
  for(unsigned i = 0; i < count; i++) if(res[i].tags[0] != 3) tags_ok = 0;
  check(tags_ok, "tag filter: only tag 0 = 3");
  float back[DIM];
  unsigned btags[gxv7TagCount];
  check(vix.Get((long long)addr[7], back, btags) == gxv7_NO_ERROR &&
	memcmp(back, items[7].vec, sizeof(back)) == 0 && btags[0] == 7, "Get() returns the vector and tags");
  check(vix.Delete((long long)addr[1234]) == gxv7_NO_ERROR && !vix.Contains((long long)addr[1234]) &&
	vix.Count() == (unsigned long long)N - 1, "Delete()");
  vix.Exact(items[1234].vec, 1, res, count);
  check(count == 1 && res[0].id != (long long)addr[1234], "a deleted vector is not found");
  float moved[DIM];
  for(unsigned d = 0; d < DIM; d++) moved[d] = items[1234].vec[d];
  check(vix.Update((long long)addr[42], moved) == gxv7_NO_ERROR, "Update()");
  vix.Exact(moved, 1, res, count);
  check(count == 1 && res[0].id == (long long)addr[42], "an updated vector is found by its new value");
  check(vix.Insert((long long)addr[42], moved) == gxv7_DUPLICATE_ID, "a duplicate id is refused");
  float bad[DIM];
  for(unsigned d = 0; d < DIM; d++) bad[d] = 0;
  bad[3] = (float)HUGE_VAL;
  check(vix.Insert(999999, bad) == gxv7_NOT_FINITE, "a vector with infinity is refused");

  // 6. Close, reopen read only, Verify()
  check(vix.Close() == gxv7_NO_ERROR, "Close()");
  gxv7VectorIndex ro;
  check(ro.Open(vixname, 1, "vector-demo-16") == gxv7_NO_ERROR && ro.Count() == (unsigned long long)N - 1,
	"reopened read only with the spec");
  check(ro.Insert(1, items[0].vec) == gxv7_READ_ONLY, "a read only index refuses inserts");
  gxString report;
  unsigned long problems = ro.Verify(report);
  check(problems == 0, "Verify() finds no problems");
  if(problems) printf("%s\n", report.c_str());
  ro.Close();
  gxv7VectorIndex wrong;
  check(wrong.Open(vixname, 1, "another-spec") == gxv7_SPEC_MISMATCH, "a different spec is refused");
  db.Close();
  delete[] addr;
  delete[] items;

  // 7. float16 and int8 vectors, cosine and inner product
  struct { const char *f; gxv7ElementType t; gxv7Metric m; } kinds[] = {
    { "vector_f16_cos.vix", gxv7FLOAT16, gxv7COSINE },
    { "vector_i8_ip.vix", gxv7INT8, gxv7IP } };
  for(int k = 0; k < 2; k++) {
    remove(kinds[k].f);
    gxv7VectorIndex v;
    int kok = v.Create(kinds[k].f, 8, kinds[k].t, kinds[k].m) == gxv7_NO_ERROR;
    float vv[8];
    for(int i = 0; i < 300 && kok; i++) {
      for(int d = 0; d < 8; d++) vv[d] = (float)sin(i * 0.37 + d * 1.3) + 0.01f * d;
      if(v.Insert(1000 + i, vv) != gxv7_NO_ERROR) kok = 0;
    }
    for(int d = 0; d < 8; d++) vv[d] = (float)sin(57 * 0.37 + d * 1.3) + 0.01f * d;
    gxv7Result r[3]; unsigned n = 0;
    if(kok) kok = v.Exact(vv, 3, r, n) == gxv7_NO_ERROR && n == 3;
    // Quantised types and the inner product: the vector itself is among the closest
    int near = 0;
    for(unsigned i = 0; i < n; i++) if(r[i].id == 1057) near = 1;
    char what[80];
    sprintf(what, "%s: create, 300 inserts, search", kinds[k].t == gxv7FLOAT16 ? "float16 cosine" : "int8 inner product");
    check(kok && (near || kinds[k].m == gxv7IP), what);
    v.Close();
  }

  // 8. A vector set of two .vix files (two "days")
  const char *setname = "vector_demo.vixset";
  remove(setname); remove("vector_day1.vix"); remove("vector_day2.vix");
  int sok = 1;
  for(int day = 1; day <= 2; day++) {
    gxv7VectorIndex v;
    char f[32]; sprintf(f, "vector_day%d.vix", day);
    if(v.Create(f, 8, gxv7FLOAT32, gxv7L2, 16, "vector-set-demo") != gxv7_NO_ERROR) sok = 0;
    float vv[8];
    for(int i = 0; i < 100; i++) {
      for(int d = 0; d < 8; d++) vv[d] = (float)(day * 10 + (i % 7)) + 0.01f * d;
      unsigned tg[gxv7TagCount] = { 0, 0, (unsigned)(20261100 + day), 0 };
      if(v.Insert(day * 1000 + i, vv, tg) != gxv7_NO_ERROR) sok = 0;
    }
    v.Close();
  }
  check(sok, "two day files written");
  gxv7VectorSet set;
  int cr = set.Create(setname, "demo days", 8, gxv7FLOAT32, gxv7L2, "vector-set-demo");
  check(cr == gxv7_NO_ERROR && set.AddFile("vector_day1.vix") == gxv7_NO_ERROR &&
	set.AddFile("vector_day2.vix") == gxv7_NO_ERROR && set.FileCount() == 2, "vector set created with 2 files");
  if(cr != gxv7_NO_ERROR) printf("  %s %s\n", gxv7VectorIndex::ErrorMessage(cr), set.Error().c_str());
  set.Close();
  gxv7VectorSet set2;
  float sq[8];
  for(int d = 0; d < 8; d++) sq[d] = 20.0f + 0.01f * d;
  gxv7Result sr[5]; unsigned sn = 0;
  rv = set2.Open(setname);
  if(rv == gxv7_NO_ERROR) rv = set2.Search(sq, 5, sr, sn);
  int from_day2 = (sn > 0);
  for(unsigned i = 0; i < sn; i++) if(sr[i].id < 2000 || set2.FilePath(sr[i].file) != gxString("vector_day2.vix")) from_day2 = 0;
  check(rv == gxv7_NO_ERROR && sn == 5 && from_day2, "set search finds the nearest day's vectors");
  rv = set2.Search(sq, 5, sr, sn, 64, 0, 1, 20261101, 20261101);
  int day1_only = (rv == gxv7_NO_ERROR && sn == 5);
  for(unsigned i = 0; i < sn; i++) if(sr[i].id >= 2000) day1_only = 0;
  check(day1_only, "day range: only day 1");
  set2.Close();
}

// Read the DataReel 7.4 phase C files and compare with the 7.4 results
static void phase_c(const char *dir)
{
  gxString exp_name = dir;
  exp_name << "/phase_c_expected.txt";
  FILE *fp = fopen(exp_name.c_str(), "r");
  check(fp != 0, "phase C: phase_c_expected.txt found");
  if(!fp) return;
  char line[4096];
  int lines = 0, same = 0;
  while(fgets(line, sizeof(line), fp)) {
    char fname[128], what[32];
    int pos = 0;
    if(sscanf(line, "%127s %31s %n", fname, what, &pos) != 2) continue;
    gxString path = dir;
    path << "/" << fname;
    gxv7VectorIndex v;
    if(v.Open(path.c_str(), 1) != gxv7_NO_ERROR) { lines++; continue; }
    lines++;
    if(strcmp(what, "count") == 0) {
      unsigned long long c = 0, d = 0;
      sscanf(line + pos, "%llu deleted %llu", &c, &d);
      if(v.Count() == c && v.Deleted() == d) same++;
      else printf("  %s count %llu/%llu deleted %llu/%llu\n", fname, v.Count(), c, v.Deleted(), d);
      continue;
    }
    int qi = atoi(what + 1);
    float vec[8];
    for(int d = 0; d < 8; d++) vec[d] = (float)cos(qi * 0.91 + d * 0.7);
    gxv7Result r[10]; unsigned n = 0;
    v.Search(vec, 10, r, n, 64);
    gxString got;
    char buf[64];
    for(unsigned i = 0; i < n; i++) { sprintf(buf, " %lld:%.9g", r[i].id, r[i].distance); got << buf; }
    gxString g = got;
    g.TrimTrailingSpaces();
    // The expected line holds " id:dist ..." after the query name
    gxString w2 = " ";
    w2 << (line + pos);
    w2.TrimTrailingSpaces();
    while(w2.length() && (w2[w2.length()-1] == '\n' || w2[w2.length()-1] == '\r')) w2.DeleteAt(w2.length()-1, 1);
    if(g == w2) same++;
    else printf("  %s %s differs\n   got %s\n  want %s\n", fname, what, g.c_str(), w2.c_str());
  }
  fclose(fp);
  printf("  phase C: %d of %d lines the same as DataReel 7.4\n", same, lines);
  check(lines > 0 && same == lines, "phase C: 6.x program reading DataReel 7.4 .vix files gets the 7.4 results");
}

int main(int argc, char **argv)
{
  demo_and_checks();
  for(int i = 1; i < argc; i++) {
    if(strcmp(argv[i], "--phase-c") == 0 && i + 1 < argc) phase_c(argv[++i]);
  }
  if(failures) printf("%d check(s) FAILED\n", failures);
  else printf("All checks passed\n");
  return failures ? 1 : 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
