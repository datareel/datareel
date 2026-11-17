// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: fuzz_vector.cpp
// Compiler Used: Clang (libFuzzer)
// Produced By: DataReel Software Development Team
// File Creation Date: 11/14/2026
// Date Last Modified: 11/16/2026
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

libFuzzer target for the DataReel 7.4 vector code (POSIX), the 7.4
security scan. The first input byte picks the code under test and the
rest is its input:

  0  a vector set manifest (.vixset): the input is the manifest,
     opened with gxVectorSet::Open() (relative paths, "..", fields),
     every file's full path asked for
  1  a vector index file (.vix): the input is the file, opened
     read-only, counted, digested and searched with a fixed query
  2  the object server's vector requests (ops 7 VECTOR_SEARCH and 8
     VECTOR_INFO, byte 1 picks): the rest is the request body, sent to
     gxObjectService::Request() for a small vector index made at start

11/16/2026: was fuzz_repl.cpp. Its gxrepl parts (ship log records, the
gxrepl config, stamps and derived state) went with gxrepl.

Temporary files go in the system temp folder, named by process id.

Build (clang, 64-bit engine, as tests/fuzz/build_fuzz.sh builds the
library):
  clang++ -std=c++17 -fsanitize=fuzzer,address,undefined $DEFS \
    -Iinclude tests/fuzz/fuzz_vector.cpp libdatareel.a -lssl -lcrypto -lz \
    -lpthread -ldl -o fuzz_vector
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
#include "gxvindex.h"
#include "gxvset.h"
#include "gxdbobject.h"

using namespace datareel;
namespace fs = std::filesystem;

static std::string tmp_dir()
{
  static std::string d;
  if(d.empty()) {
    d = (fs::temp_directory_path() / ("fuzz_vector_" + std::to_string((long)getpid()))).string();
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
  switch(data[0] % 3) {
  case 0: manifest(p, n); break;
  case 1: vix_file(p, n); break;
  case 2: vector_request(p, n); break;
  }
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
