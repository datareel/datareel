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

DataReel 6.4 example and test: a DataReel 6.x program searching the
vectors a DataReel 7.x database object server serves (op 7
VECTOR_SEARCH, op 8 VECTOR_INFO) with gxv7ObjectClient (dr7features.h),
over mutual TLS. See README.txt and run_vector_client.sh.

  testprog make DIR
      make DIR/news.vixset (3 day files of 16-float vectors, tag 2 = the
      day) and DIR/news.gxd (an empty 6.x database for the server's
      [db news] path)
  testprog check HOST PORT SERVER_NAME CERT KEY CA DB_NAME DIR
      connect, VECTOR_INFO, 6 searches, a day range search and a tag
      filter search; every remote answer must equal a local search of
      DIR/news.vixset (ids, distances, order and file names)
  testprog refused HOST PORT SERVER_NAME CERT KEY CA DB_NAME
      returns 0 if the connection or the channel is refused

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

const unsigned DIM = 16;
const unsigned DAYS = 3;
const unsigned PER_DAY = 300;

static void day_vector(unsigned day, unsigned i, float *v)
{
  for(unsigned d = 0; d < DIM; d++) v[d] = (float)sin(day * 1.7 + i * 0.13 + d * 0.9);
}

static void query_vector(unsigned q, float *v)
{
  for(unsigned d = 0; d < DIM; d++) v[d] = (float)cos(q * 0.77 + d * 0.41);
}

static int make(const char *dir)
{
  gxString manifest = dir; manifest << "/news.vixset";
  gxv7VectorSet set;
  int rv = set.Create(manifest.c_str(), "news", DIM, gxv7FLOAT32, gxv7L2, "news-test");
  check(rv == gxv7_NO_ERROR, "vector set created");
  for(unsigned day = 0; day < DAYS; day++) {
    char rel[32]; sprintf(rel, "day%u.vix", day);
    gxString path = dir; path << "/" << rel;
    gxv7VectorIndex v;
    int ok = v.Create(path.c_str(), DIM, gxv7FLOAT32, gxv7L2, 16, "news-test") == gxv7_NO_ERROR;
    float vec[DIM];
    for(unsigned i = 0; i < PER_DAY && ok; i++) {
      day_vector(day, i, vec);
      unsigned tags[gxv7TagCount] = { i % 5, 0, 20261110 + day, 0 };
      if(v.Insert((long long)(day * 100000 + i), vec, tags) != gxv7_NO_ERROR) ok = 0;
    }
    v.Close();
    if(ok) ok = set.AddFile(rel) == gxv7_NO_ERROR;
    check(ok, "day file written and added");
  }
  set.Close();
  gxString dbname = dir; dbname << "/news.gxd";
  gxDatabase db;
  check(db.Create(dbname.c_str()) == gxDBASE_NO_ERROR, "6.x database news.gxd created");
  db.Close();
  return failures ? 1 : 0;
}

// One line per result: id distance file
static gxString lines(const gxv7Result *r, unsigned n, const gxString *names)
{
  gxString s;
  char buf[128];
  for(unsigned i = 0; i < n; i++) {
    sprintf(buf, "%lld %.9g ", r[i].id, r[i].distance);
    s << buf << names[i] << "\n";
  }
  return s;
}

static int do_check(int argc, char **argv)
{
  gxv7ObjectClient cli;
  int rv = cli.Connect(argv[2], atoi(argv[3]), argv[4], argv[5], argv[6], argv[7], argv[8]);
  check(rv == 0, "connected over mutual TLS, channel opened");
  if(rv != 0) { printf("  %s\n", cli.Error().c_str()); return 1; }

  gxString manifest = argv[9]; manifest << "/news.vixset";
  gxv7VectorSet local;
  check(local.Open(manifest.c_str()) == gxv7_NO_ERROR, "local vector set opened");

  gxv7VectorInfo info;
  rv = cli.VectorInfo(info);
  printf("  VECTOR_INFO: version %u, dim %u, type %d, metric %d, %u files, %llu nodes, %llu deleted, spec \"%s\"\n",
	 info.version, info.dim, (int)info.type, (int)info.metric, info.files, info.nodes, info.deleted,
	 info.spec.c_str());
  check(rv == 0 && info.version == 1 && info.dim == DIM && info.type == gxv7FLOAT32 &&
	info.metric == gxv7L2 && info.files == DAYS && info.nodes == DAYS * PER_DAY &&
	info.spec == gxString("news-test"), "VECTOR_INFO describes the set");

  float q[DIM];
  gxv7Result rr[10], lr[10];
  gxString rn[10], ln[10];
  int same = 0;
  for(unsigned qi = 0; qi < 6; qi++) {
    query_vector(qi, q);
    unsigned nr = 0, nl = 0;
    int a = cli.VectorSearch(q, DIM, 10, rr, nr);
    int b = local.Search(q, 10, lr, nl);
    for(unsigned i = 0; i < nr; i++) rn[i] = cli.FileName(rr[i].file);
    for(unsigned i = 0; i < nl; i++) ln[i] = local.FilePath(lr[i].file);
    gxString x = lines(rr, nr, rn), y = lines(lr, nl, ln);
    if(a == 0 && b == gxv7_NO_ERROR && nr == 10 && x == y) same++;
    else printf("  query %u differs:\n%s  local:\n%s", qi, x.c_str(), y.c_str());
  }
  check(same == 6, "6 remote searches equal the local searches (ids, distances, order, files)");

  // A day range: only day 1, the other two files skipped
  query_vector(2, q);
  unsigned nr = 0, nl = 0, skipped = 0, missing = 0;
  int a = cli.VectorSearch(q, DIM, 10, rr, nr, 64, 0, 1, 20261111, 20261111, &skipped, &missing);
  int b = local.Search(q, 10, lr, nl, 64, 0, 1, 20261111, 20261111);
  int day_ok = (a == 0 && b == gxv7_NO_ERROR && nr == 10 && nr == nl && skipped == 2 && missing == 0);
  for(unsigned i = 0; i < nr && day_ok; i++) {
    if(rr[i].tags[2] != 20261111 || rr[i].id != lr[i].id || cli.FileName(rr[i].file) != gxString("day1.vix")) day_ok = 0;
  }
  check(day_ok, "day range search: day 1 only, 2 files skipped, equal to local");

  // A tag filter
  gxv7Filter flt;
  flt.Tag(0, 3);
  a = cli.VectorSearch(q, DIM, 10, rr, nr, 64, &flt);
  b = local.Search(q, 10, lr, nl, 64, &flt);
  int flt_ok = (a == 0 && b == gxv7_NO_ERROR && nr == 10 && nr == nl);
  for(unsigned i = 0; i < nr && flt_ok; i++) if(rr[i].tags[0] != 3 || rr[i].id != lr[i].id) flt_ok = 0;
  check(flt_ok, "tag filter search equal to local");

  // Refused requests: a wrong dimension, k too large
  float big[DIM + 1];
  for(unsigned d = 0; d <= DIM; d++) big[d] = 0.1f;
  check(cli.VectorSearch(big, DIM + 1, 10, rr, nr) != 0, "a query of the wrong dimension is refused");
  gxv7Result *many = new gxv7Result[1001];
  check(cli.VectorSearch(q, DIM, 1001, many, nr) != 0, "k over 1000 is refused");
  delete[] many;
  check(cli.VectorSearch(q, DIM, 3, rr, nr) == 0 && nr == 3, "the channel still answers after refusals");
  cli.Close();
  local.Close();
  return failures ? 1 : 0;
}

int main(int argc, char **argv)
{
  if(argc == 3 && strcmp(argv[1], "make") == 0) return make(argv[2]);
  if(argc == 10 && strcmp(argv[1], "check") == 0) {
    int rv = do_check(argc, argv);
    if(failures) printf("%d check(s) FAILED\n", failures);
    else printf("All checks passed\n");
    return rv;
  }
  if(argc == 9 && strcmp(argv[1], "refused") == 0) {
    gxv7ObjectClient cli;
    int rv = cli.Connect(argv[2], atoi(argv[3]), argv[4], argv[5], argv[6], argv[7], argv[8]);
    if(rv == 0) {
      gxv7VectorInfo info;
      rv = cli.VectorInfo(info);
    }
    printf("%s: %s\n", rv == 0 ? "ACCEPTED" : "REFUSED", rv == 0 ? "" : cli.Error().c_str());
    return rv == 0 ? 1 : 0;
  }
  printf("Usage: %s make DIR | check HOST PORT SERVER_NAME CERT KEY CA DB_NAME DIR |\n"
	 "       refused HOST PORT SERVER_NAME CERT KEY CA DB_NAME\n", argv[0]);
  return 2;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
