// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: fuzz_repl.cpp
// Compiler Used: Clang (libFuzzer)
// Produced By: DataReel Software Development Team
// File Creation Date: 11/14/2026
// Date Last Modified: 11/14/2026
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

libFuzzer target for the DataReel 7.4 replication and vector code
(POSIX), the 7.4 security scan. The first input byte picks the code
under test and the rest is its input:

  0  ship log records from a peer: gxdParseShipRecords(), then for each
     record gxdParseShipCommitRecord(), gxdParseShipObjectsRecord()
     (type 5, the object notes), gxdShipObjectsFor() and
     gxdShipObjectsOrphan()
  1  the gxrepl config: the input is loaded as a config file with
     gxReplConfig::Load(), every set looked up again
  2  a vector set manifest (.vixset): the input is the manifest,
     opened with gxVectorSet::Open() (relative paths, "..", fields),
     every file's full path asked for
  3  a vector index file (.vix): the input is the file, opened
     read-only, counted, digested and searched with a fixed query
  4  the object server's vector requests (ops 7 VECTOR_SEARCH and 8
     VECTOR_INFO, byte 1 picks): the rest is the request body, sent to
     gxObjectService::Request() for a small vector index made at start
  5  stamps and derived state: the input is split at its first 0xff
     into a vector index stamp (x.vix.stamp) and the folder's derived
     state, written into a throwaway set folder, then
     gxReplDerivedStatus() and gxReplTornCopies() read them

Temporary files go in the system temp folder, named by process id.

Build (clang, 64-bit engine, as tests/fuzz/build_fuzz.sh builds the
library):
  clang++ -std=c++17 -fsanitize=fuzzer,address,undefined $DEFS \
    -Iinclude tests/fuzz/fuzz_repl.cpp libdatareel.a -lssl -lcrypto -lz \
    -lpthread -ldl -o fuzz_repl
Every run must end with an error code or good data: a crash, hang,
sanitizer report or leak is a finding.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>
#include "gxdrepl.h"
#include "gxrepl.h"
#include "gxvindex.h"
#include "gxvset.h"
#include "gxdbobject.h"

using namespace datareel;
namespace fs = std::filesystem;

static std::string tmp_dir()
{
  static std::string d;
  if(d.empty()) {
    d = (fs::temp_directory_path() / ("fuzz_repl_" + std::to_string((long)getpid()))).string();
    fs::create_directories(d);
  }
  return d;
}

static void put(const std::string &path, const uint8_t *p, size_t n)
{
  std::ofstream o(path, std::ios::binary | std::ios::trunc);
  o.write((const char *)p, (std::streamsize)n);
}

static const unsigned DIM = 8;
static gxObjectService *service = 0;

// A small vector index for the object server requests (made once)
static void make_service()
{
  std::string v = tmp_dir() + "/svc.vix";
  {
    gxVectorIndex vix;
    if(vix.Create(v.c_str(), DIM, gxvFLOAT32, gxvL2, 8, "fuzz/v1", 50) != gxv_NO_ERROR) return;
    float q[DIM];
    for(long long id = 1; id <= 64; id++) {
      for(unsigned i = 0; i < DIM; i++) q[i] = (float)((id * 31 + i * 7) % 17) / 17.0f;
      unsigned t[gxvTagCount] = { 0, 0, (unsigned)(20261100 + id % 15), 0 };
      vix.Insert(id, q, t);
    }
    vix.Close();
  }
  std::vector<gxObjectDbConfig> dbs(1);
  dbs[0].name = "f";
  dbs[0].path = tmp_dir() + "/svc.gxd";
  dbs[0].vectors = v;
  service = new gxObjectService(dbs);
  service->SetVectorLimits(2, 2);
}

static void ship_records(const uint8_t *p, size_t n)
{
  std::vector<gxdShipRecordView> recs;
  gxdParseShipRecords(p, n, recs);              // the records before a damaged one come back too
  for(size_t i = 0; i < recs.size(); i++) {
    std::vector<unsigned char> storage;
    std::vector<gxdShipCommitFile> files;
    if(gxdParseShipCommitRecord(recs[i], storage, files) == 0)
      for(auto &f : files) for(auto &pg : f.pages) { volatile unsigned char c = pg.second ? pg.second[0] : 0; (void)c; }
    gxdShipObjectsRecord o;
    gxdParseShipObjectsRecord(recs[i], o);
    gxdShipObjectsRecord o2;
    gxdShipObjectsFor(recs, i, o2);
    gxdShipObjectsOrphan(recs, i);
  }
}

static void config(const uint8_t *p, size_t n)
{
  std::string f = tmp_dir() + "/repl.conf";
  put(f, p, n);
  gxReplConfig c;
  std::string err;
  if(c.Load(f, err) == 0)
    for(auto &s : c.sets) c.Set(s.name);
}

static void manifest(const uint8_t *p, size_t n)
{
  std::string f = tmp_dir() + "/set.vixset";
  put(f, p, n);
  gxVectorSet s;
  if(s.Open(f.c_str()) == gxv_NO_ERROR)
    for(unsigned i = 0; i < s.Files().size(); i++) s.FullPath(i);
}

static void vix_file(const uint8_t *p, size_t n)
{
  std::string f = tmp_dir() + "/in.vix";
  put(f, p, n);
  gxVectorIndex vix;
  if(vix.Open(f.c_str(), gxDBASE_READONLY) != gxv_NO_ERROR) return;
  vix.Count();
  unsigned long long nodes = 0;
  vix.LogicalDigest(&nodes);
  unsigned d = vix.Dim();
  if(d > 0 && d <= 4096) {
    std::vector<float> q(d, 0.25f);
    std::vector<gxvResult> r;
    vix.Search(q.data(), 5, r, 32);
  }
  vix.Close();
}

static void vector_request(const uint8_t *p, size_t n)
{
  if(!service || n < 1) return;
  uint8_t op = (p[0] & 1) ? gxOBJ_VECTOR_INFO : gxOBJ_VECTOR_SEARCH;
  std::vector<unsigned char> body(p + 1, p + n), reply;
  std::string err;
  service->Request("f", false, op, body, reply, err);
}

static void stamps(const uint8_t *p, size_t n)
{
  std::string root = tmp_dir() + "/set";
  fs::create_directories(root + "/.gxrepl/derived");
  size_t cut = 0;
  while(cut < n && p[cut] != 0xff) cut++;
  put(root + "/x.vix.stamp", p, cut);
  put(root + "/.gxrepl/derived/_root", p + (cut < n ? cut + 1 : n), cut < n ? n - cut - 1 : 0);
  gxReplSetConfig sc;
  sc.name = "f";
  sc.root = root;
  sc.derived.push_back("x.vix");
  sc.derived_apply = "incremental";
  sc.derived_extractor = "gxvmember";
  sc.derived_source = "data.gxd";
  std::vector<std::string> lines, torn;
  gxReplDerivedStatus(sc, lines);
  gxReplTornCopies(sc, torn);
}

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
  make_service();
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  if(size < 1) return 0;
  const uint8_t *p = data + 1;
  size_t n = size - 1;
  switch(data[0] % 6) {
  case 0: ship_records(p, n); break;
  case 1: config(p, n); break;
  case 2: manifest(p, n); break;
  case 3: vix_file(p, n); break;
  case 4: vector_request(p, n); break;
  case 5: stamps(p, n); break;
  }
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
