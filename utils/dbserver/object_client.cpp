// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: object_client.cpp
// Produced By: DataReel Software Development Team
// File Creation Date: 11/08/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- //
/*
Example: store an object in a database on a DataReel database object
server and read it back (object_server.cpp, gxdbobject.h).

  object_client HOST PORT SERVER_NAME CERT KEY CA DB_NAME "text"

Connects over mutual TLS, opens a multiplex channel to database DB_NAME
for writing, adds "text" as an object, requests it by its address and
prints both.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"
#include "gxstream.h"
#include "gxdbmux.h"
#include "gxdbobject.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

using namespace datareel;

// One request on the channel: op and body out, the result back
static int call(gxdMuxSession &mux, uint32_t ch, uint8_t op, const std::vector<unsigned char> &body,
		std::vector<unsigned char> &result, std::string &err)
{
  std::vector<unsigned char> m(1, op);
  m.insert(m.end(), body.begin(), body.end());
  if(mux.Send(ch, m, gxdMUX_LAST, err) != 0) return -1;
  std::vector<unsigned char> back;
  uint64_t seq;
  if(mux.ReceiveMessage(ch, back, seq, 30000, err) != 0) return -1;
  if(back.empty() || back[0] != 0) { err = "the server refused the request"; return -1; }
  result.assign(back.begin() + 1, back.end());
  return 0;
}

int main(int argc, char **argv)
{
  // 11/14/2026 (7.4): "--vinfo", or "--vsearch K" with the query on stdin (whitespace-separated floats)
  bool vinfo = argc == 9 && !strcmp(argv[8], "--vinfo");
  bool vsearch = argc == 10 && !strcmp(argv[8], "--vsearch");
  if(argc != 9 && !vsearch) {
    fprintf(stderr, "usage: object_client HOST PORT SERVER_NAME CERT KEY CA DB_NAME \"text\"|--vinfo|--vsearch K\n");
    return 64;
  }
  gxStream st;
  gxsTLSConnection &c = st.Connection();
  c.SetCAFile(argv[6]);
  c.SetClientCert(argv[4], argv[5]);
  c.SetVerifyPeer(true);
  gxSocket sock;
  if(sock.Connect(argv[1], atoi(argv[2]), 10) != 0 || c.AttachClient(std::move(sock), argv[3]) != 0) {
    fprintf(stderr, "cannot connect: %s\n", c.Error().c_str());
    return 1;
  }
  gxdMuxSession mux(gxdMuxStreamTransport(st));
  mux.Start();
  std::string err;
  if(mux.Hello(err) != 0 || mux.Open(1, "object", argv[7], (vinfo || vsearch) ? "read" : "write", 0, err) != 0) {
    fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  if(vinfo || vsearch) {
    auto p32 = [](std::vector<unsigned char> &v, uint32_t x) { unsigned char b[4]; memcpy(b, &x, 4); v.insert(v.end(), b, b + 4); };
    auto g32 = [](const std::vector<unsigned char> &v, size_t &o) { uint32_t x = 0; if(o + 4 <= v.size()) memcpy(&x, v.data() + o, 4); o += 4; return x; };
    std::vector<unsigned char> res, body;
    if(call(mux, 1, gxOBJ_VECTOR_INFO, body, res, err) != 0) { fprintf(stderr, "vinfo: %s\n", err.c_str()); return 1; }
    size_t o = 0;
    uint32_t ver = g32(res, o), dim = g32(res, o), el = g32(res, o), met = g32(res, o), nf = g32(res, o);
    uint64_t nodes = 0, del = 0;
    if(o + 16 <= res.size()) { memcpy(&nodes, res.data() + o, 8); memcpy(&del, res.data() + o + 8, 8); }
    o += 16;
    uint32_t sl = g32(res, o);
    std::string spec = o + sl <= res.size() ? std::string((const char *)res.data() + o, sl) : std::string();
    if(vinfo) {
      printf("vectors: version %u, dimension %u, element %u, metric %u, %u file(s), %llu nodes (%llu deleted), spec \"%s\"\n",
	     ver, dim, el, met, nf, (unsigned long long)nodes, (unsigned long long)del, spec.c_str());
      mux.Shutdown();
      return 0;
    }
    std::vector<float> q;
    float x;
    while(scanf("%f", &x) == 1) q.push_back(x);
    if(q.size() != dim) { fprintf(stderr, "the query has %zu values, the index %u\n", q.size(), dim); return 1; }
    p32(body, 1); p32(body, (uint32_t)atoi(argv[9])); p32(body, 0); p32(body, dim); p32(body, 0); p32(body, 0);
    for(int i = 0; i < 10; i++) p32(body, 0);
    body.insert(body.end(), (unsigned char *)q.data(), (unsigned char *)q.data() + 4 * q.size());
    if(call(mux, 1, gxOBJ_VECTOR_SEARCH, body, res, err) != 0) { fprintf(stderr, "vsearch: %s\n", err.c_str()); return 1; }
    o = 0;
    g32(res, o);
    uint32_t n = g32(res, o), skipped = g32(res, o), missing = g32(res, o);
    std::vector<std::pair<long long, std::pair<float, uint32_t> > > rows;
    for(uint32_t i = 0; i < n && o + 32 <= res.size(); i++) {
      long long id; float d; uint32_t f;
      memcpy(&id, res.data() + o, 8); memcpy(&d, res.data() + o + 8, 4); memcpy(&f, res.data() + o + 28, 4);
      o += 32;
      rows.push_back(std::make_pair(id, std::make_pair(d, f)));
    }
    std::vector<std::string> names(nf);
    uint32_t nn = g32(res, o);
    for(uint32_t i = 0; i < nn; i++) {
      uint32_t f = g32(res, o), l = g32(res, o);
      if(f < names.size() && o + l <= res.size()) names[f].assign((const char *)res.data() + o, l);
      o += l;
    }
    printf("%u result(s), %u file(s) skipped, %u missing\n", n, skipped, missing);
    for(auto &rw : rows) printf("id %lld distance %g in %s\n", rw.first, rw.second.first,
				 rw.second.second < names.size() ? names[rw.second.second].c_str() : "?");
    mux.Shutdown();
    return 0;
  }
  // ADD: u64 length, the bytes -> u64 address
  std::string text = argv[8];
  std::vector<unsigned char> body, res;
  uint64_t n = text.size();
  body.insert(body.end(), (unsigned char *)&n, (unsigned char *)&n + 8);
  body.insert(body.end(), text.begin(), text.end());
  if(call(mux, 1, gxOBJ_ADD, body, res, err) != 0 || res.size() < 8) { fprintf(stderr, "add: %s\n", err.c_str()); return 1; }
  uint64_t addr;
  memcpy(&addr, res.data(), 8);
  printf("stored %zu bytes at address %llu\n", text.size(), (unsigned long long)addr);
  // REQUEST: u64 address -> u64 length, the bytes
  body.assign((unsigned char *)&addr, (unsigned char *)&addr + 8);
  if(call(mux, 1, gxOBJ_REQUEST, body, res, err) != 0 || res.size() < 8) { fprintf(stderr, "request: %s\n", err.c_str()); return 1; }
  printf("read back: %s\n", std::string(res.begin() + 8, res.end()).c_str());
  mux.Shutdown();
  return 0;
}
