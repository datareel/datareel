// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: gxdbserver.cpp
// C++ Compiler Used: GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/08/2026
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

The DataReel database object server (gxdbserver.h): the shared
listener, the handshake threads, the session limits, and the choice of
service from a session's first block.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

// 11/17/2026 (7.4): Windows too

#include "gxdbserver.h"

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <chrono>
#include <thread>

namespace datareel {

static std::mutex srv_log_lock;

void gxDatabaseServerLog(const char *fmt, ...)
{
  char tb[64];
  time_t t = time(0);
  struct tm tmv;
#if defined (__WIN32__)
  localtime_s(&tmv, &t);
#else
  localtime_r(&t, &tmv);
#endif
  strftime(tb, sizeof(tb), "%Y-%m-%d %H:%M:%S", &tmv);
  char msg[2048];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof(msg), fmt, ap);
  va_end(ap);
  std::lock_guard<std::mutex> g(srv_log_lock);
  fprintf(stderr, "%s dbserver: %s\n", tb, msg);
  fflush(stderr);
}

static long long now_s() { return (long long)time(0); }

void gxDatabaseSession::SetDescription(const std::string &d)
{
  if(server) server->describe(slot, d);
}

gxDatabaseServer::gxDatabaseServer(const gxDatabaseServerConfig &c) : cfg(c) { }

gxDatabaseServer::~gxDatabaseServer() { server.Close(); }

int gxDatabaseServer::Port() { return server.Port(); }

std::vector<std::string> gxDatabaseServer::Connections()
{
  std::lock_guard<std::mutex> g(conn_lock);
  std::vector<std::string> v;
  for(auto &e : conn_info) v.push_back(e.second);
  return v;
}

void gxDatabaseServer::describe(int slot, const std::string &d)
{
  std::lock_guard<std::mutex> g(conn_lock);
  conn_info[slot] = d + " since " + std::to_string(now_s());
}

int gxDatabaseServer::Run(std::string &err)
{
#if !defined (__WIN32__)
  signal(SIGPIPE, SIG_IGN);   // Windows sockets do not raise signals
#endif
  if(services.empty()) { err = "no service to serve"; return -1; }
  server.SetClientCAFile(cfg.ca);
  for(auto &cn : cfg.allow_cn) server.AllowClientCN(cn);
  for(auto &fp : cfg.allow_fingerprint) {
    if(server.AllowClientFingerprint(fp) != 0) { err = "bad fingerprint " + fp; return -1; }
  }
  if(server.Listen(cfg.listen_port, cfg.cert, cfg.key, cfg.key_passphrase,
		   cfg.listen_address) != 0) {
    err = "listen on port " + std::to_string(cfg.listen_port) + ": " + server.Error();
    return -1;
  }
  if(tick) tick();
  int slot = 0;
  long long last_tick = now_s();
  while(!stop) {
    if(tick && now_s() - last_tick >= tick_every) { tick(); last_tick = now_s(); }
    if(server.Listener().WaitReadable(500) <= 0) continue;
    // Only the TCP accept runs here. The TLS handshake runs in its own
    // thread with a short timeout, so a connection that never starts or
    // finishes TLS (a scanner on an open port) cannot hold up a client.
    gxSocket c = server.Listener().Accept();
    if(!c.IsOpen()) continue;
    if(handshaking >= cfg.max_handshakes) {
      gxDatabaseServerLog("dropped %s: %d handshakes already running", c.PeerIPAddress().c_str(),
			  (int)handshaking);
      c.Close();
      continue;
    }
    handshaking++;
    int id = ++slot;
    std::thread(&gxDatabaseServer::handshake, this, id,
		std::make_shared<gxSocket>(std::move(c))).detach();
  }
  server.Close();
  for(int i = 0; i < 100 && (clients > 0 || handshaking > 0); i++) std::this_thread::sleep_for(std::chrono::milliseconds(100));
  if(tick) tick();
  return 0;
}

void gxDatabaseServer::handshake(int slot, std::shared_ptr<gxSocket> c)
{
  std::string ip = c->PeerIPAddress();
  auto s = std::make_shared<gxStream>();
  s->SetTimeOut(cfg.handshake_timeout);
  s->SetMaxBlockSize((gxUINT32)cfg.max_block);
  int rc = server.Handshake(s->Connection(), std::move(*c));
  if(rc != 0) {
    gxDatabaseServerLog("refused %s: %s", ip.c_str(), s->Connection().Error().c_str());
    s->Close();
    handshaking--;
    return;
  }
  // Only clients that passed the certificate checks count against max_clients
  if(clients.fetch_add(1) >= cfg.max_clients) {
    clients--;
    gxDatabaseServerLog("refused %s: %d clients already", s->Connection().PeerCN().c_str(),
			cfg.max_clients);
    s->Close();
    handshaking--;
    return;
  }
  handshaking--;
  s->SetTimeOut(cfg.timeout);
  session(slot, s);
}

void gxDatabaseServer::session(int slot, std::shared_ptr<gxStream> s)
{
  gxDatabaseSession ses;
  ses.stream = s;
  ses.slot = slot;
  ses.server = this;
  ses.peer_cn = s->Connection().PeerCN();
  ses.peer_host = s->Connection().Host();
  describe(slot, ses.peer_cn + "@" + ses.peer_host);
  // The first block chooses the service
  gxDatabaseService *svc = 0;
  if(s->ReadHeader(ses.first_header) == gxSOCKET_NO_ERROR &&
     s->ReadBlock(ses.first_body, ses.first_header) == gxSOCKET_NO_ERROR) {
    for(auto *x : services) {
      if(x->Claims(ses.first_header, ses.first_body)) { svc = x; break; }
    }
    if(!svc) gxDatabaseServerLog("client %s@%s: no service for its first block",
				 ses.peer_cn.c_str(), ses.peer_host.c_str());
  }
  if(svc) svc->Serve(ses);
  s->Close();
  {
    std::lock_guard<std::mutex> g(conn_lock);
    conn_info.erase(slot);
  }
  clients--;
}

} // namespace datareel

// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
