// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: vector_server.cpp
// Compiler Used: GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/17/2026
// Date Last Modified: 11/17/2026
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

DataReel 6.4: the DataReel 7.x database object server (gxdbserver.h,
gxdbobject.h) serving one database read only with its vectors, for 6.x
programs that search them (gxv7ObjectClient, examples_socket/
vector_client). Built from the 7.x tree's headers only, like the 7.x
utils/dbserver/object_server (which serves one database without
vectors). DataReel 7.4 beta 5 removed gxrepl, whose "serve" command
served vectors before. POSIX only, as the 7.4 multiplex session.

  vector_server PORT CERT KEY CA CLIENT_CN DB_NAME DB_FILE VECTORS

VECTORS: a .vixset manifest or a .vix file. Listens on 127.0.0.1 until
TERM or INT.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"
#include "gxdbserver.h"
#include "gxdbmux.h"
#include "gxdbobject.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <atomic>
#include <thread>
#include <vector>

using namespace datareel;

static gxDatabaseServer *g_server = 0;
static std::atomic<bool> g_stop{false};
static void on_signal(int) { g_stop = true; if(g_server) g_server->Stop(); }

// The object service takes multiplex sessions through this service: it
// answers the multiplex HELLO and serves each "object" channel
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

int main(int argc, char **argv)
{
  if(argc != 9) {
    fprintf(stderr, "usage: vector_server PORT CERT KEY CA CLIENT_CN DB_NAME DB_FILE VECTORS\n");
    return 64;
  }
  gxDatabaseServerConfig c;
  c.listen_address = "127.0.0.1";
  c.listen_port = atoi(argv[1]);
  c.cert = argv[2]; c.key = argv[3]; c.ca = argv[4];
  c.allow_cn.push_back(argv[5]);
  gxObjectDbConfig db;
  db.name = argv[6]; db.path = argv[7]; db.access = "read"; db.vectors = argv[8];
  gxObjectService objects(std::vector<gxObjectDbConfig>(1, db));
  MuxObjects mux(objects);
  gxDatabaseServer server(c);
  server.AddService(&mux);
  g_server = &server;
  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);
  std::string err;
  printf("serving database %s with vectors %s on 127.0.0.1:%s\n", argv[6], argv[8], argv[1]);
  fflush(stdout);
  if(server.Run(err) != 0) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
