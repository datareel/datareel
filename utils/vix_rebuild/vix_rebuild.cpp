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
rebuild of a replicated copy (gxrepl "derived_rebuild"), and prints
digests to compare indexes.

  vix_rebuild --data DATA.gxd --index OUT.vix --member infohog:N|offset:N
              [--tags A,B,C,D] [--like OLD.vix]
              [--dim D --element f32|f16|i8 --metric l2|cosine|ip
               --m M --efc N --spec TEXT]
              [--stamp-dir DIR --lsn L [--commit-block NAME@OFFSET]]
  vix_rebuild --digest FILE.vix [--digest-kind logical|bytes]

The header (dimension, element type, metric, M, ef_construction, spec)
comes from --like, or OUT.vix itself when it exists, else from the
options. --stamp-dir writes DIR/derived.stamp ("gen G lsn L records N
keys K") as gxrepl checks it: G from the commit block (default
commit.bin@8), L from --lsn ({lsn} of derived_rebuild). In a gxrepl
config:

  derived = vectors.vix
  derived_rebuild = vix_rebuild --data {dir}/data.gxd --index {dir}/vectors.vix
      --member infohog:1 --tags -,-,infohog:2,- --dim 16 --spec news
      --stamp-dir {dir} --lsn {lsn}

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
#include "gxdrepl.h"

using namespace datareel;

static int usage()
{
  fprintf(stderr, "usage: vix_rebuild --data DATA.gxd --index OUT.vix --member infohog:N|offset:N [--tags A,B,C,D]\n"
	  "         [--like OLD.vix | --dim D --element f32|f16|i8 --metric l2|cosine|ip --m M --efc N --spec TEXT]\n"
	  "         [--stamp-dir DIR --lsn L [--commit-block NAME@OFFSET]]\n"
	  "       vix_rebuild --digest FILE.vix [--digest-kind logical|bytes]\n");
  return 2;
}

int main(int argc, char **argv)
{
  std::string data, index, like, member, tags, spec, stamp_dir, cb = "commit.bin@8", digest, kind = "logical";
  unsigned dim = 0, m = 16, efc = 200;
  unsigned long long lsn = 0;
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
    else if(a == "--stamp-dir" && val(v)) stamp_dir = v;
    else if(a == "--lsn" && val(v)) lsn = strtoull(v.c_str(), 0, 10);
    else if(a == "--commit-block" && val(v)) cb = v;
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
      while((n = fread(b.data(), 1, b.size(), f)) > 0) { h = gxdReplHash(b.data(), n, h); total += n; }
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
  if(!stamp_dir.empty()) {
    size_t at = cb.find('@');
    std::string cbn = cb.substr(0, at);
    long off = at == std::string::npos ? 8 : atol(cb.c_str() + at + 1);
    unsigned long long g = 0;
    FILE *cf = fopen((stamp_dir + "/" + cbn).c_str(), "rb");
    if(!cf || fseek(cf, off, SEEK_SET) != 0 || fread(&g, 8, 1, cf) != 1) {
      if(cf) fclose(cf);
      fprintf(stderr, "vix_rebuild: cannot read the commit block %s/%s\n", stamp_dir.c_str(), cbn.c_str()); return 1;
    }
    fclose(cf);
    std::string tmp = stamp_dir + "/.derived.stamp.tmp";
    FILE *sf = fopen(tmp.c_str(), "w");
    if(!sf) { fprintf(stderr, "vix_rebuild: cannot stamp\n"); return 1; }
    fprintf(sf, "gen %llu lsn %llu records %llu keys %llu\n", g, lsn, records, keys);
    fflush(sf);
    fclose(sf);
    if(rename(tmp.c_str(), (stamp_dir + "/derived.stamp").c_str()) != 0) { fprintf(stderr, "vix_rebuild: cannot stamp\n"); return 1; }
  }
  printf("vix_rebuild: %s: %llu records, %llu nodes\n", index.c_str(), records, keys);
  return 0;
}
