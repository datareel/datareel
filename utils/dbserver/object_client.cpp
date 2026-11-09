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
  if(argc != 9) {
    fprintf(stderr, "usage: object_client HOST PORT SERVER_NAME CERT KEY CA DB_NAME \"text\"\n");
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
  if(mux.Hello(err) != 0 || mux.Open(1, "object", argv[7], "write", 0, err) != 0) {
    fprintf(stderr, "%s\n", err.c_str());
    return 1;
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
