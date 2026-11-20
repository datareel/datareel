// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: test_gxobjrepl_reader.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/19/2026
// Date Last Modified: 11/19/2026
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

7.4.1: the gxObjReplReader test program, run by test_objrepl_reader.sh.

  test_gxobjrepl_reader write ROOT PKI N [COPY_PORT]
      a primary: N commits, each rewrites the object in "big" (12000
      bytes, every byte from its commit number) and adds one object to
      "vec" (an 8-byte key and a 4-float vector, both from the commit
      number); with COPY_PORT it pumps to a copy there after each commit
  test_gxobjrepl_reader read ROOT SECONDS
      reads ROOT from another process while it changes: every read of
      "big" must be one commit's bytes (never half of two), Seq() never
      goes back, every key up to the commit just read is found once, its
      object holds that key, and the nearest vector to it is that object
  test_gxobjrepl_reader final ROOT [N]
      the files as they are: "big" names as many commits as "vec" holds
      objects (N when given), and the reader's Seq() is at least that
*/
// ----------------------------------------------------------- //
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include "gxobjrepl.h"

using namespace datareel;

static const char *INDEX = "k0:vec:B:0:8,v0:vec:V:8:4";
static const size_t BIG = 12000;

static int write_mode(const std::string &root, const std::string &pki, long n, const std::string &port)
{
  gxObjReplConfig c;
  bool ok = c.Set("ca", pki + "/ca.pem") && c.Set("cert", pki + "/primary.pem") && c.Set("key", pki + "/primary.key") &&
	    c.Set("copy", "127.0.0.1:" + (port.empty() ? std::string("1") : port)) && c.Set("index", INDEX) && c.Set("name", "P");
  if(!ok) { fprintf(stderr, "write: config\n"); return 64; }
  c.log = [](const std::string &) {};
  gxObjRepl r;
  if(r.Open(root, c, gxObjRepl::PRIMARY) != 0) { fprintf(stderr, "write: %s\n", r.Error().c_str()); return 1; }
  if(r.OpenDatabase("big") != 0 || r.OpenDatabase("vec") != 0) { fprintf(stderr, "write: %s\n", r.Error().c_str()); return 1; }
  uint64_t big = 0;
  { std::vector<uint64_t> ids = r.Ids("big"); if(!ids.empty()) big = ids[0]; }
  long start = (long)r.Ids("vec").size();
  for(long k = start + 1; k <= n; k++) {
    if(r.Begin() != 0) { fprintf(stderr, "write: begin %s\n", r.Error().c_str()); return 1; }
    if(!big) big = r.NewId();
    std::vector<unsigned char> b(BIG);
    uint64_t kk = (uint64_t)k; memcpy(b.data(), &kk, 8);
    for(size_t i = 8; i < BIG; i++) b[i] = (unsigned char)(kk * 7 + i);
    if(r.Put("big", big, b.data(), (uint32_t)b.size()) != 0) { fprintf(stderr, "write: put %s\n", r.Error().c_str()); return 1; }
    unsigned char v[24]; float f[4] = {(float)k, 0, 0, 0};
    memcpy(v, &kk, 8); memcpy(v + 8, f, 16);
    if(r.Put("vec", r.NewId(), v, sizeof(v)) != 0) { fprintf(stderr, "write: put %s\n", r.Error().c_str()); return 1; }
    if(r.Commit() != 0) { fprintf(stderr, "write: commit %s\n", r.Error().c_str()); return 1; }
    if(!port.empty()) r.Pump();
    if(k % 100 == 0) { printf("write: %ld\n", k); fflush(stdout); }
  }
  if(!port.empty()) r.Drain(60);
  r.Close();
  printf("write: done %ld\n", n);
  return 0;
}

static int read_mode(const std::string &root, double seconds)
{
  gxObjReplReader r;
  auto t0 = std::chrono::steady_clock::now();
  while(r.Open(root, INDEX) != 0) {                            // the writer may not have made the node yet
    if(std::chrono::steady_clock::now() - t0 > std::chrono::seconds(30)) { printf("FAIL read: open: %s\n", r.Error().c_str()); return 1; }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  long reads = 0, keys = 0, bad = 0, errs = 0; uint64_t last_seq = 0, last_k = 0;
  std::string first_bad, first_err;
  auto refused = [&](const char *call) { if(!errs++) first_err = std::string(call) + ": " + r.Error(); };
  auto badf = [&](const std::string &w) { if(!bad++) first_bad = w; };
  unsigned rnd = 12345;
  while(std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() < seconds) {
    std::vector<uint64_t> ids;
    if(r.Ids("big", ids) != 0) { refused("Ids"); continue; }
    if(r.Seq() < last_seq) badf("Seq() went back from " + std::to_string(last_seq) + " to " + std::to_string(r.Seq()));
    last_seq = r.Seq();
    if(ids.size() != 1) { if(!ids.empty()) badf("big holds " + std::to_string(ids.size()) + " objects"); continue; }
    std::vector<unsigned char> b;
    if(r.Get("big", ids[0], b) != 0) {
      refused("Get");
      if(getenv("READER_DEBUG")) {
	uint64_t s1 = r.Seq(); std::vector<uint64_t> again; r.Ids("big", again);
	fprintf(stderr, "debug: Get(big %llu) failed [%s] at seq %llu (Ids at %llu); Ids now %zu at seq %llu, IsLive %d\n",
		(unsigned long long)ids[0], r.Error().c_str(), (unsigned long long)s1, (unsigned long long)last_seq, again.size(),
		(unsigned long long)r.Seq(), r.IsLive("big", ids[0]));
      }
      continue;
    }
    reads++;
    if(b.size() != BIG) { badf("big is " + std::to_string(b.size()) + " bytes"); continue; }
    uint64_t k; memcpy(&k, b.data(), 8);
    for(size_t i = 8; i < BIG; i++) if(b[i] != (unsigned char)(k * 7 + i)) { badf("big read half written (commit " + std::to_string(k) + ", byte " + std::to_string(i) + ")"); break; }
    if(k < last_k) badf("big went back from commit " + std::to_string(last_k) + " to " + std::to_string(k));
    last_k = k;
    if(r.Seq() < k) badf("Seq() " + std::to_string(r.Seq()) + " is behind commit " + std::to_string(k) + " read");
    // a key up to k: found once, its object holds it, and it is its own nearest vector
    rnd = rnd * 1103515245 + 12345;
    uint64_t j = 1 + (rnd >> 8) % k;
    std::vector<uint64_t> hit;
    if(r.FindKey("k0", &j, 8, hit) != 0) { refused("FindKey"); continue; }
    if(hit.size() != 1) { badf("key " + std::to_string(j) + " found " + std::to_string(hit.size()) + " times at commit " + std::to_string(k)); continue; }
    std::vector<unsigned char> v;
    if(r.Get("vec", hit[0], v) != 0 || v.size() != 24) { badf("key " + std::to_string(j) + ": its object cannot be read"); continue; }
    uint64_t vk; memcpy(&vk, v.data(), 8);
    if(vk != j) { badf("key " + std::to_string(j) + " found object with key " + std::to_string(vk)); continue; }
    float q[4] = {(float)j, 0, 0, 0};
    std::vector<gxObjReplHit> nh;
    if(r.Nearest("v0", q, 1, nh) != 0) { refused("Nearest"); continue; }
    if(nh.size() != 1 || nh[0].id != hit[0] || nh[0].distance != 0.0f) { badf("nearest to " + std::to_string(j) + " is not its object"); continue; }
    keys++;
  }
  printf("%s read: %ld reads of big up to commit %llu, %ld key/vector checks, %ld refused calls, seq %llu%s%s\n",
	 bad || reads == 0 || keys == 0 ? "FAIL" : "PASS", reads, (unsigned long long)last_k, keys, errs,
	 (unsigned long long)last_seq, bad ? ": " : "", bad ? (std::to_string(bad) + " bad, first: " + first_bad).c_str() : "");
  if(errs) printf("read: the first refusal: %s\n", first_err.c_str());
  return bad || reads == 0 || keys == 0 ? 1 : 0;
}

static int final_mode(const std::string &root, long n)
{
  gxObjReplReader r;
  if(r.Open(root, INDEX) != 0) { printf("FAIL final: open: %s\n", r.Error().c_str()); return 1; }
  std::vector<uint64_t> big, vec;
  if(r.Ids("big", big) != 0 || r.Ids("vec", vec) != 0) { printf("FAIL final: %s\n", r.Error().c_str()); return 1; }
  uint64_t k = 0;
  if(big.size() == 1) {
    std::vector<unsigned char> b;
    if(r.Get("big", big[0], b) != 0 || b.size() != BIG) { printf("FAIL final: big: %s\n", r.Error().c_str()); return 1; }
    memcpy(&k, b.data(), 8);
  }
  bool ok = k == vec.size() && (n < 0 || (long)k == n) && r.Seq() >= k;
  printf("%s final: big at commit %llu, vec holds %zu, seq %llu%s\n", ok ? "PASS" : "FAIL", (unsigned long long)k, vec.size(),
	 (unsigned long long)r.Seq(), n >= 0 ? (", expected " + std::to_string(n)).c_str() : "");
  return ok ? 0 : 1;
}

int main(int argc, char **argv)
{
  std::string m = argc > 1 ? argv[1] : "";
  if(m == "write" && argc >= 5) return write_mode(argv[2], argv[3], atol(argv[4]), argc > 5 ? argv[5] : "");
  if(m == "read" && argc >= 4) return read_mode(argv[2], atof(argv[3]));
  if(m == "final" && argc >= 3) return final_mode(argv[2], argc > 3 ? atol(argv[3]) : -1);
  fprintf(stderr, "usage: test_gxobjrepl_reader write ROOT PKI N [COPY_PORT] | read ROOT SECONDS | final ROOT [N]\n");
  return 64;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
