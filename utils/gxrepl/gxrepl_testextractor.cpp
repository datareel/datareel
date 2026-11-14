// ----------------------------------------------------------- //
// C++ Source Code File Name: gxrepl_testextractor.cpp
// Produced By: DataReel Software Development Team
// File Creation Date: 11/14/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- //
/*
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.

The test extractor of gxrepl's suite (gxrextract.h): it keeps the
derived index.gxd of a gxrepl_testwriter folder (one IndexObj: the
objects' count, the sum of id * 1000003 + version, the commits, the
next id) object by object. Built two ways: into the test build of
gxrepl as the built-in "gxrepl_test" (gxrRegisterExtractor()), and as
gxrepl_testextractor.so (dlopen; it takes DataReel from gxrepl, which
exports it, so its writes go through gxrepl's staged scope).

GXREPL_TEST_EXTRACTOR_FAIL=insert|remove|flush makes that call fail;
GXREPL_TEST_EXTRACTOR_CRASH=1 kills the process in Insert();
GXREPL_TEST_EXTRACTOR_SLOW=MS sleeps MS ms in each Insert() (11/14/2026);
GXREPL_TEST_EXTRACTOR_HANG=1 never returns from Insert() (11/14/2026);
GXREPL_TEST_EXTRACTOR_STRAY=1 also changes the data file's first object in Flush()
(inside the scope: gxrepl must refuse it, 11/14/2026);
GXREPL_TEST_EXTRACTOR_DIRECT=1 writes index.gxd with its own file
handle (outside any staged scope: gxrepl must notice).
*/
// ----------------------------------------------------------- //
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include "gxdbase.h"
#include "gxuint64.h" // gxUINT64; the 32-bit database headers do not include it (as gxrepl_testwriter.cpp)
#include "gxrextract.h"

using namespace datareel;

namespace {

struct ObjHdr {
  gxUINT64 id;
  gxUINT64 version;
  gxUINT32 len;
  gxUINT32 check;
};

struct IndexObj {
  gxUINT64 count;
  gxUINT64 sum;
  gxUINT64 commits;
  gxUINT64 next_id;
};

struct H {
  gxDatabase ix;
  FAU_t addr = 0;
  IndexObj io;
  std::string path, cb, data;
};

gxUINT32 obj_check(const ObjHdr &h, const unsigned char *p)
{
  unsigned long long x = 1469598103934665603ULL;
  const unsigned char *hp = (const unsigned char *)&h;
  for(size_t i = 0; i < 20; i++) { x ^= hp[i]; x *= 1099511628211ULL; }
  for(gxUINT32 i = 0; i < h.len; i++) { x ^= p[i]; x *= 1099511628211ULL; }
  return (gxUINT32)(x ^ (x >> 32));
}

bool fail(const char *what)
{
  const char *e = getenv("GXREPL_TEST_EXTRACTOR_FAIL");
  return e && strcmp(e, what) == 0;
}

void say(char *err, size_t n, const std::string &m) { if(err && n) snprintf(err, n, "%s", m.c_str()); }

// An object of data.gxd: its header, checked
int parse(const unsigned char *b, uint32_t len, ObjHdr &h, char *err, size_t n)
{
  if(len < sizeof(ObjHdr)) { say(err, n, "an object shorter than its header"); return -1; }
  memcpy(&h, b, sizeof(h));
  if(sizeof(h) + (unsigned long long)h.len > len || obj_check(h, b + sizeof(h)) != h.check) {
    say(err, n, "a bad object"); return -1;
  }
  return 0;
}

const char *t_version() { return "gxrepl_test 1 (index.gxd count/sum)"; }

void *t_open(const char *index_path, const char *data_path, const char *, char *err, size_t n)
{
  H *h = new H;
  h->path = index_path;
  std::string d = data_path ? data_path : "";
  h->cb = d.substr(0, d.find_last_of('/') + 1) + "commit.bin";
  h->data = d;
  if(h->ix.Open(index_path) != gxDBASE_NO_ERROR) { say(err, n, std::string("cannot open ") + index_path); delete h; return 0; }
  h->addr = h->ix.FindFirstObject();
  if(!h->addr || h->ix.Read(&h->io, sizeof(h->io), h->addr) != gxDBASE_NO_ERROR) {
    say(err, n, "index.gxd holds no index object"); h->ix.Close(); delete h; return 0;
  }
  return h;
}

int t_remove(void *hv, uint64_t, const unsigned char *b, uint32_t len, char *err, size_t n)
{
  H *h = (H *)hv;
  if(fail("remove")) { say(err, n, "test: remove fails"); return -1; }
  ObjHdr o;
  if(parse(b, len, o, err, n) != 0) return -1;
  h->io.count--;
  h->io.sum -= (unsigned long long)o.id * 1000003ULL + o.version;
  return 0;
}

int t_insert(void *hv, uint64_t, const unsigned char *b, uint32_t len, char *err, size_t n)
{
  H *h = (H *)hv;
  if(fail("insert")) { say(err, n, "test: insert fails"); return -1; }
  if(getenv("GXREPL_TEST_EXTRACTOR_CRASH")) _exit(7);
  if(const char *sl = getenv("GXREPL_TEST_EXTRACTOR_SLOW")) usleep((useconds_t)atoi(sl) * 1000);   // 11/14/2026
  if(getenv("GXREPL_TEST_EXTRACTOR_HANG")) for(;;) sleep(60);                                     // 11/14/2026
  ObjHdr o;
  if(parse(b, len, o, err, n) != 0) return -1;
  h->io.count++;
  h->io.sum += (unsigned long long)o.id * 1000003ULL + o.version;
  if(o.id + 1 > h->io.next_id) h->io.next_id = o.id + 1;
  return 0;
}

int t_flush(void *hv, uint64_t, char *err, size_t n)
{
  H *h = (H *)hv;
  if(fail("flush")) { say(err, n, "test: flush fails"); return -1; }
  // the commit count is the commit block's (as reindex does), as the batch leaves it
  unsigned char blk[24];
  if(gxrReadAfterBatch(h->cb.c_str(), 0, blk, sizeof(blk)) == (int)sizeof(blk)) {
    uint64_t cc; memcpy(&cc, blk + 16, 8); h->io.commits = cc;
  }
  if(getenv("GXREPL_TEST_EXTRACTOR_DIRECT")) {
    int fd = open(h->path.c_str(), O_WRONLY);
    if(fd >= 0) { if(pwrite(fd, &h->io, sizeof(h->io), (off_t)h->addr) < 0) { } close(fd); }
  }
  if(getenv("GXREPL_TEST_EXTRACTOR_STRAY") && !h->data.empty()) {   // 11/14/2026: a write to another file (staged)
    gxDatabase df;
    if(df.Open(h->data.c_str()) == gxDBASE_NO_ERROR) {
      FAU_t a = df.FindFirstObject();
      unsigned char junk[8] = {0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef};
      if(a) df.Write(junk, sizeof(junk), a);           // a real change: gxrepl must never apply it
      df.Close();
    }
  }
  if(h->ix.Write(&h->io, sizeof(h->io), h->addr) != gxDBASE_NO_ERROR || h->ix.WriteFileHdr() != gxDBASE_NO_ERROR) {
    say(err, n, "cannot write index.gxd"); return -1;
  }
  return 0;
}

uint64_t t_count(void *hv) { return ((H *)hv)->io.count; }

void t_close(void *hv)
{
  H *h = (H *)hv;
  h->ix.Close();
  delete h;
}

const gxrExtractorOps ops = { GXR_EXTRACTOR_ABI, t_version, t_open, t_remove, t_insert, t_flush, t_count, t_close };

} // namespace

const gxrExtractorOps *gxrepl_test_extractor_ops() { return &ops; }

extern "C" const gxrExtractorOps *gxr_extractor(void) { return &ops; }
