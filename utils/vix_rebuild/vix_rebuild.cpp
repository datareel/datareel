// ----------------------------------------------------------- //
// C++ Source Code File Name: vix_rebuild.cpp
// Produced By: DataReel Software Development Team
// File Creation Date: 11/14/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- //
/*
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.

vix_rebuild (DataReel 7.4): rebuilds a vector index (.vix) from the
vector members of a data file's records (gxvmember.h), the derived
rebuild of a copy, and prints digests to compare indexes.

  vix_rebuild --data DATA.gxd --index OUT.vix --member infohog:N|offset:N
              [--tags A,B,C,D] [--like OLD.vix]
              [--dim D --element f32|f16|i8 --metric l2|cosine|ip
               --m M --efc N --spec TEXT]
  vix_rebuild --digest FILE.vix [--digest-kind logical|bytes]

The header (dimension, element type, metric, M, ef_construction, spec)
comes from --like, or OUT.vix itself when it exists, else from the
options. (11/16/2026: --stamp-dir, --lsn and --commit-block, which wrote
gxrepl's derived stamp, went with gxrepl.)

Digests: "logical" (default) hashes the live nodes in id order (id,
stored code, tags): a primary's index and a copy's rebuild agree on it;
"bytes" hashes the file: two rebuilds of the same data agree on it.
Exit 0 ok, 1 failed, 2 usage.
*/
// ----------------------------------------------------------- //
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include "gxvmember.h"
#include "gxddigest.h"

using namespace datareel;

static int usage()
{
  fprintf(stderr, "usage: vix_rebuild --data DATA.gxd --index OUT.vix --member infohog:N|offset:N [--tags A,B,C,D]\n"
	  "         [--like OLD.vix | --dim D --element f32|f16|i8 --metric l2|cosine|ip --m M --efc N --spec TEXT]\n"
	  "       vix_rebuild --digest FILE.vix [--digest-kind logical|bytes]\n");
  return 2;
}

int main(int argc, char **argv)
{
  std::string data, index, like, member, tags, spec, digest, kind = "logical";
  unsigned dim = 0, m = 16, efc = 200;
  gxvElementType et = gxvFLOAT32;
  gxvMetric met = gxvL2;
  for(int i = 1; i < argc; i++) {
    std::string a = argv[i];
    auto val = [&](std::string &v) { if(i + 1 >= argc) return false; v = argv[++i]; return true; };
    std::string v;
    if(a == "--data" && val(v)) data = v;
    else if(a == "--index" && val(v)) index = v;
    else if(a == "--like" && val(v)) like = v;
    else if(a == "--member" && val(v)) member = v;
    else if(a == "--tags" && val(v)) tags = v;
    else if(a == "--spec" && val(v)) spec = v;
    else if(a == "--dim" && val(v)) dim = (unsigned)atoi(v.c_str());
    else if(a == "--m" && val(v)) m = (unsigned)atoi(v.c_str());
    else if(a == "--efc" && val(v)) efc = (unsigned)atoi(v.c_str());
    else if(a == "--element" && val(v)) { if(v == "f32") et = gxvFLOAT32; else if(v == "f16") et = gxvFLOAT16; else if(v == "i8") et = gxvINT8; else return usage(); }
    else if(a == "--metric" && val(v)) { if(v == "l2") met = gxvL2; else if(v == "cosine") met = gxvCOSINE; else if(v == "ip") met = gxvIP; else return usage(); }
    else if(a == "--digest" && val(v)) digest = v;
    else if(a == "--digest-kind" && val(v)) { kind = v; if(kind != "logical" && kind != "bytes") return usage(); }
    else return usage();
  }
  if(!digest.empty()) {
    if(kind == "bytes") {
      FILE *f = fopen(digest.c_str(), "rb");
      if(!f) { fprintf(stderr, "vix_rebuild: cannot open %s\n", digest.c_str()); return 1; }
      std::vector<unsigned char> b(1 << 20);
      uint64_t h = 0;
      unsigned long long total = 0;
      size_t n;
      while((n = fread(b.data(), 1, b.size(), f)) > 0) { h = gxDatabaseHash(b.data(), n, h); total += n; }
      fclose(f);
      printf("bytes %016llx %llu %s\n", (unsigned long long)h, total, digest.c_str());
      return 0;
    }
    gxVectorIndex vix;
    gxvError e = vix.Open(digest.c_str(), gxDBASE_READONLY);
    if(e != gxv_NO_ERROR) { fprintf(stderr, "vix_rebuild: %s: %s\n", digest.c_str(), gxVectorIndex::ErrorMessage(e)); return 1; }
    unsigned long long nodes = 0;
    unsigned long long h = vix.LogicalDigest(&nodes);
    printf("logical %016llx %llu %s opt %u\n", h, nodes, digest.c_str(), vix.FeaturesOptional());
    return 0;
  }
  if(data.empty() || index.empty() || member.empty()) return usage();
  std::string config = "member=" + member + (tags.empty() ? std::string() : " tags=" + tags);
  // The header: --like, the old index, or the options (a copy never has the old index)
  gxVectorIndex lk;
  std::string tmp_like;
  std::string src = !like.empty() ? like : index;
  FILE *t = fopen(src.c_str(), "rb");
  if(t) {
    fclose(t);
    gxvError e = lk.Open(src.c_str(), gxDBASE_READONLY);
    if(e != gxv_NO_ERROR) {
      if(!like.empty() || dim == 0) { fprintf(stderr, "vix_rebuild: %s: %s\n", src.c_str(), gxVectorIndex::ErrorMessage(e)); return 1; }
      t = 0;                         // a damaged old index: the options say what to build
    }
  }
  if(!t) {
    if(dim == 0) { fprintf(stderr, "vix_rebuild: %s does not exist: give --dim (and --element, --metric, --m, --spec)\n", index.c_str()); return 1; }
    tmp_like = index + ".like";
    remove(tmp_like.c_str());
    gxvError e = lk.Create(tmp_like.c_str(), dim, et, met, m, spec.empty() ? 0 : spec.c_str(), efc);
    if(e != gxv_NO_ERROR) { fprintf(stderr, "vix_rebuild: %s\n", gxVectorIndex::ErrorMessage(e)); return 1; }
  }
  unsigned long long records = 0;
  std::string why;
  int rc = gxvRebuildFromMembers(data, index, lk, config, records, why);
  lk.Close();
  if(!tmp_like.empty()) remove(tmp_like.c_str());
  if(rc != 0) { fprintf(stderr, "vix_rebuild: %s\n", why.c_str()); return 1; }
  gxVectorIndex done;
  unsigned long long keys = 0;
  if(done.Open(index.c_str(), gxDBASE_READONLY) != gxv_NO_ERROR) { fprintf(stderr, "vix_rebuild: cannot open the rebuilt %s\n", index.c_str()); return 1; }
  keys = done.Count();
  done.Close();
  printf("vix_rebuild: %s: %llu records, %llu nodes\n", index.c_str(), records, keys);
  return 0;
}
