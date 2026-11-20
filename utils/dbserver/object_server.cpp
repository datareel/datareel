// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: object_server.cpp
// Produced By: DataReel Software Development Team
// File Creation Date: 11/08/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- //
/*
Example: a DataReel database object server with only the object service.

  object_server PORT CERT KEY CA CLIENT_CN DB_NAME DB_FILE

Serves DB_FILE (created when missing) for writing as database DB_NAME on
PORT, to clients whose certificate is signed by CA and names CLIENT_CN.
Stop it with Ctrl-C. See object_client.cpp and gxdbobject.h.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"
#include "gxdbserver.h"
#include "gxdbmux.h"
#include "gxdbobject.h"

#include <signal.h>
#if defined (_WIN32)
#include <winsock2.h>
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <atomic>
#include <thread>
#include <vector>

using namespace datareel;

static gxDatabaseServer *g_server = 0;
static std::atomic<bool> g_stop{false};
static void on_signal(int) { g_stop = true; if(g_server) g_server->Stop(); }
#if defined (_WIN32)
// 11/17/2026 (7.4): Ctrl+C, Ctrl+Break, the console closed, log off or shut down
static BOOL WINAPI on_console(DWORD) { on_signal(0); return TRUE; }
#endif

// The object service takes multiplex sessions through this small service:
// it answers the multiplex HELLO and serves each "object" channel
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
  // 11/17/2026 (7.4): --stop-file F: stop when F appears (a service wrapper, a script, or Windows, where a console
  // program gets no SIGTERM); Ctrl+C, Ctrl+Break and closing the console stop it too
  // 11/20/2026 (7.5): --vectors V: the database also answers vector searches from V (a .vix file or a .vixset)
  std::string stop_file, vectors;
  for(int i = 8; i + 1 < argc; ) {
    if(strcmp(argv[i], "--stop-file") == 0) stop_file = argv[i + 1];
    else if(strcmp(argv[i], "--vectors") == 0) vectors = argv[i + 1];
    else break;
    for(int k = i; k + 2 < argc; k++) argv[k] = argv[k + 2];
    argc -= 2;
  }
  if(argc != 8) {
    fprintf(stderr, "usage: object_server PORT CERT KEY CA CLIENT_CN DB_NAME DB_FILE [--stop-file FILE] [--vectors VIX]\n");
    return 64;
  }
  gxDatabaseServerConfig c;
  c.listen_port = atoi(argv[1]);
  c.cert = argv[2]; c.key = argv[3]; c.ca = argv[4];
  c.allow_cn.push_back(argv[5]);
  gxObjectDbConfig db;
  db.name = argv[6]; db.path = argv[7]; db.access = "write"; db.vectors = vectors;
  gxObjectService objects(std::vector<gxObjectDbConfig>(1, db));
  MuxObjects mux(objects);
  gxDatabaseServer server(c);
  server.AddService(&mux);
  g_server = &server;
  if(!stop_file.empty()) {
    remove(stop_file.c_str());
    server.SetTick([&] {
      FILE *f = fopen(stop_file.c_str(), "rb");
      if(f && !g_stop) { printf("stop file %s seen\n", stop_file.c_str()); fflush(stdout); on_signal(0); }
      if(f) fclose(f);
    }, 1);
  }
  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);
#if defined (_WIN32)
  SetConsoleCtrlHandler(on_console, TRUE);
#endif
  std::string err;
  printf("serving database %s on port %s\n", argv[6], argv[1]);
  fflush(stdout);
  if(server.Run(err) != 0) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
  printf("stopped\n");
  return 0;
}
