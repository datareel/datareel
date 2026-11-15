// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: vector_search_service.cpp
// Compiler Used: GCC
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
version 3 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library. If not, see
<https://www.gnu.org/licenses/>.

Vector searches through the database object server (gxdbobject.h ops 7
VECTOR_SEARCH and 8 VECTOR_INFO, DataReel 7.4), over gxStream with mutual
TLS and the multiplex protocol (gxdbmux.h). POSIX only, like the object
server.

  vector_search_service make DIR
        Three day files (2026-11-12 to 2026-11-14) of 300 vectors each
        (dimension 8, tag 2 = the day) and the manifest DIR/notes.vixset,
        and an empty database file DIR/notes.gxd.
  vector_search_service serve PORT CERT KEY CA CLIENT_CN DIR
        The server: database "notes" (DIR/notes.gxd, read only) with
        "vectors = DIR/notes.vixset". It answers vector requests at once,
        without the database's own lock; at most 2 searches run at a time
        (SetVectorLimits), one more waits up to 5 s.
  vector_search_service search HOST PORT SERVER_NAME CERT KEY CA [DAY_LO DAY_HI]
        The client: VECTOR_INFO (dimension, element type, metric, files,
        nodes, spec), then VECTOR_SEARCH for the 5 nearest to a fixed
        query, all days or only DAY_LO..DAY_HI (tag 2): the files outside
        the range are skipped by the server from the manifest.

The request and reply layouts (little-endian):
  VECTOR_INFO:   request empty; reply u32 version, dim, element, metric,
                 files, u64 nodes, deleted, then u32 length + the spec
  VECTOR_SEARCH: request u32 version, k, ef (0 = 64), dim, flags (bit 0:
                 day range, bit 1: strict), tag mask, u32 lo[4], hi[4],
                 day_lo, day_hi, then dim float32 values; reply u32 version,
                 n, skipped, missing, n x (u64 id, f32 distance, u32 tags[4],
                 u32 file), u32 count, count x (u32 file, u32 length, path)

vector_search_walkthrough.sh runs make, serve and search on 127.0.0.1
with throwaway certificates.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <atomic>
#include <filesystem>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include "gxdbase.h"
#include "gxdbserver.h"
#include "gxdbmux.h"
#include "gxdbobject.h"
#include "gxstream.h"
#include "gxvindex.h"
#include "gxvset.h"

using namespace datareel;

static const unsigned DIM = 8;

// ---------------- make ----------------
static int make(const std::string &dir)
{
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  gxVectorSet set;
  if(set.Create((dir + "/notes.vixset").c_str(), "notes", DIM, gxvFLOAT32, gxvCOSINE, "example/notes") != gxv_NO_ERROR) return 1;
  std::mt19937 g(9);
  std::normal_distribution<float> n(0.0f, 1.0f);
  float v[DIM];
  long long id = 1;
  for(unsigned day = 20261112; day <= 20261114; day++) {
    std::string rel = "day_" + std::to_string(day) + ".vix";
    gxVectorIndex vix;
    vix.Create((dir + "/" + rel).c_str(), DIM, gxvFLOAT32, gxvCOSINE, 16, "example/notes", 100);
    for(int i = 0; i < 300; i++) {
      for(auto &x : v) x = n(g);
      unsigned tags[gxvTagCount] = { 0, 0, day, 0 };
      vix.Insert(id++, v, tags);
    }
    vix.Close();
    if(set.AddFile(rel.c_str()) != gxv_NO_ERROR) { printf("%s\n", set.Error().c_str()); return 1; }
  }
  set.Close();
  gxDatabase db;
  db.Create((dir + "/notes.gxd").c_str());
  db.Close();
  printf("made %s/notes.vixset (3 day files, %lld vectors) and notes.gxd\n", dir.c_str(), id - 1);
  return 0;
}

// ---------------- serve ----------------
static gxDatabaseServer *g_server = 0;
static std::atomic<bool> g_stop{false};
static void on_signal(int) { g_stop = true; if(g_server) g_server->Stop(); }

// Multiplex sessions for the object service (as utils/dbserver/object_server.cpp)
class MuxObjects : public gxDatabaseService {
public:
  explicit MuxObjects(gxObjectService &o) : obj(o) { }
  const char *ServiceName() const override { return "object multiplex"; }
  bool Claims(const gxBlockHeader &gx, const std::vector<unsigned char> &body) override {
    gxdMuxFrame f;
    return gxStream::BlockStatus(gx) == gxSendBlock && gxdMuxDecodeBody(body.data(), body.size(), f) == 0 &&
      f.kind == gxdMUX_HELLO;
  }
  void Serve(gxDatabaseSession &s) override {
    gxdMuxFrame first;
    gxdMuxDecodeBody(s.first_body.data(), s.first_body.size(), first);
    gxdMuxSession mux(gxdMuxStreamTransport(s.Stream()));
    mux.SetOpenHandler([&](uint32_t, const std::string &service, const std::string &name,
			   const std::string &mode, std::vector<unsigned char> &, std::string &why) {
      if(service != "object") { why = "objects only here"; return false; }
      return obj.OpenAllowed(name, mode, why);
    });
    mux.Start(&first);
    std::vector<std::thread> t;
    while(!g_stop && mux.Alive()) {
      uint32_t ch; std::string sv, nm, md;
      if(mux.AcceptChannel(ch, sv, nm, md, 500) == 0)
	t.emplace_back([&, ch, nm, md] { obj.ServeChannel(mux, ch, nm, md, g_stop); });
    }
    mux.Shutdown();
    for(auto &x : t) x.join();
  }
private:
  gxObjectService &obj;
};

static int serve(char **a)
{
  gxDatabaseServerConfig c;
  c.listen_port = atoi(a[0]);
  c.cert = a[1]; c.key = a[2]; c.ca = a[3];
  c.allow_cn.push_back(a[4]);
  std::string dir = a[5];
  gxObjectDbConfig db;
  db.name = "notes";
  db.path = dir + "/notes.gxd";
  db.access = "read";
  db.vectors = dir + "/notes.vixset";      // a .vixset manifest (or one .vix file)
  gxObjectService objects(std::vector<gxObjectDbConfig>(1, db));
  objects.SetVectorLimits(2, 2);            // 2 threads a search, 2 searches at a time
  MuxObjects mux(objects);
  gxDatabaseServer server(c);
  server.AddService(&mux);
  g_server = &server;
  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);
  printf("serving vector searches of \"notes\" on port %s\n", a[0]);
  fflush(stdout);
  std::string err;
  if(server.Run(err) != 0) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
  return 0;
}

// ---------------- search ----------------
static void p32(std::vector<unsigned char> &v, uint32_t x) { unsigned char b[4]; memcpy(b, &x, 4); v.insert(v.end(), b, b + 4); }
static uint32_t g32(const std::vector<unsigned char> &v, size_t &o) { uint32_t x = 0; if(o + 4 <= v.size()) memcpy(&x, v.data() + o, 4); o += 4; return x; }
static uint64_t g64(const std::vector<unsigned char> &v, size_t &o) { uint64_t x = 0; if(o + 8 <= v.size()) memcpy(&x, v.data() + o, 8); o += 8; return x; }

static int call(gxdMuxSession &mux, uint8_t op, const std::vector<unsigned char> &body,
		std::vector<unsigned char> &result, std::string &err)
{
  std::vector<unsigned char> m(1, op);
  m.insert(m.end(), body.begin(), body.end());
  if(mux.Send(1, m, gxdMUX_LAST, err) != 0) return -1;
  std::vector<unsigned char> back;
  uint64_t seq;
  if(mux.ReceiveMessage(1, back, seq, 30000, err) != 0) return -1;
  if(back.empty() || back[0] != 0) {      // 1, u32 length, the server's reason
    size_t o = 1;
    uint32_t l = g32(back, o);
    err = o + l <= back.size() ? std::string((const char *)back.data() + o, l) : "refused";
    return -1;
  }
  result.assign(back.begin() + 1, back.end());
  return 0;
}

static int search(int argc, char **a)
{
  gxStream st;
  gxsTLSConnection &c = st.Connection();
  c.SetCAFile(a[5]);
  c.SetClientCert(a[3], a[4]);
  c.SetVerifyPeer(true);
  gxSocket sock;
  if(sock.Connect(a[0], atoi(a[1]), 10) != 0 || c.AttachClient(std::move(sock), a[2]) != 0) {
    fprintf(stderr, "cannot connect: %s\n", c.Error().c_str());
    return 1;
  }
  gxdMuxSession mux(gxdMuxStreamTransport(st));
  mux.Start();
  std::string err;
  if(mux.Hello(err) != 0 || mux.Open(1, "object", "notes", "read", 0, err) != 0) { fprintf(stderr, "%s\n", err.c_str()); return 1; }

  std::vector<unsigned char> body, res;
  if(call(mux, gxOBJ_VECTOR_INFO, body, res, err) != 0) { fprintf(stderr, "info: %s\n", err.c_str()); return 1; }
  size_t o = 0;
  uint32_t ver = g32(res, o), dim = g32(res, o), el = g32(res, o), met = g32(res, o), files = g32(res, o);
  uint64_t nodes = g64(res, o), deleted = g64(res, o);
  uint32_t sl = g32(res, o);
  std::string spec = o + sl <= res.size() ? std::string((const char *)res.data() + o, sl) : std::string();
  printf("VECTOR_INFO: version %u, dimension %u, element %u, metric %u, %u file(s), %llu nodes (%llu deleted), spec \"%s\"\n",
	 ver, dim, el, met, files, (unsigned long long)nodes, (unsigned long long)deleted, spec.c_str());

  float q[DIM] = { 0.5f, -0.2f, 0.1f, 0.9f, -0.4f, 0.3f, 0.0f, -0.7f };
  bool range = argc >= 8;
  uint32_t lo = range ? (uint32_t)strtoul(a[6], 0, 10) : 0, hi = range ? (uint32_t)strtoul(a[7], 0, 10) : 0;
  body.clear();
  p32(body, gxObjVectorVersion);
  p32(body, 5);                 // k
  p32(body, 0);                 // ef: the server's default (64)
  p32(body, dim);
  p32(body, range ? 1 : 0);     // flags: bit 0 = day range
  p32(body, 0);                 // tag mask: no tag filter
  for(int i = 0; i < 8; i++) p32(body, 0);   // lo[4], hi[4]
  p32(body, lo);
  p32(body, hi);
  body.insert(body.end(), (unsigned char *)q, (unsigned char *)q + 4 * DIM);
  if(call(mux, gxOBJ_VECTOR_SEARCH, body, res, err) != 0) { fprintf(stderr, "search: %s\n", err.c_str()); return 1; }
  o = 0;
  g32(res, o);
  uint32_t n = g32(res, o), skipped = g32(res, o), missing = g32(res, o);
  struct Row { long long id; float d; uint32_t tags[4]; uint32_t file; };
  std::vector<Row> rows(n);
  for(uint32_t i = 0; i < n; i++) {
    Row &r = rows[i];
    r.id = (long long)g64(res, o);
    memcpy(&r.d, res.data() + o, 4); o += 4;
    for(int t = 0; t < 4; t++) r.tags[t] = g32(res, o);
    r.file = g32(res, o);
  }
  std::vector<std::string> names(files);
  uint32_t nn = g32(res, o);
  for(uint32_t i = 0; i < nn; i++) {
    uint32_t f = g32(res, o), l = g32(res, o);
    if(f < names.size() && o + l <= res.size()) names[f].assign((const char *)res.data() + o, l);
    o += l;
  }
  printf("VECTOR_SEARCH%s: %u result(s), %u file(s) skipped by their day range, %u missing\n",
	 range ? (" days " + std::to_string(lo) + ".." + std::to_string(hi)).c_str() : "", n, skipped, missing);
  for(auto &r : rows)
    printf("  id %5lld  distance %.4f  day %u  %s\n", r.id, r.d, r.tags[2], r.file < names.size() ? names[r.file].c_str() : "?");
  mux.Shutdown();
  return 0;
}

int main(int argc, char **argv)
{
  std::string m = argc > 1 ? argv[1] : "";
  if(m == "make" && argc == 3) return make(argv[2]);
  if(m == "serve" && argc == 8) return serve(argv + 2);
  if(m == "search" && (argc == 8 || argc == 10)) return search(argc - 2, argv + 2);
  fprintf(stderr, "usage: vector_search_service make DIR\n"
	  "       vector_search_service serve PORT CERT KEY CA CLIENT_CN DIR\n"
	  "       vector_search_service search HOST PORT SERVER_NAME CERT KEY CA [DAY_LO DAY_HI]\n");
  return 64;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
