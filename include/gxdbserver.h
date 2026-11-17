// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxdbserver.h
// C++ Compiler Used: GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/08/2026
// Date Last Modified: 11/08/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- //
// ---------- Include File Description and Details  ---------- //
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

The DataReel database object server: one gxStream TLS listener on the
database port (gxsDATA_PORT, 2073) shared by several services, such as
remote access to database objects (gxdbobject.h) and vector searches.
See docs/DATABASE_OBJECT_SERVER_DESIGN.md.

The server owns what every service needs the same way:
  - one listener, one TLS context, client certificates required
    (mutual TLS: the CA, CN and fingerprint allow list);
  - the accept loop only takes the TCP connection; each TLS handshake
    runs in its own thread with handshake_timeout seconds, at most
    max_handshakes at once, so silent connections on an open port
    cannot keep real clients out;
  - at most max_clients authenticated sessions.
After the handshake the server reads the session's first gxStream
block and gives the session to the first service whose Claims() takes
that block. A block no service claims closes the session.

  gxDatabaseServerConfig c; ... c.ca = "ca.pem"; c.allow_cn.push_back("ws");
  gxDatabaseServer server(c);
  server.AddService(&my_service);    // not owned
  server.Run(err);                   // until Stop()

Changes:
==============================================================
11/08/2026: New in DataReel 7.3.1 (database object server, phase 2
step 1). gxReplServer was its first service.
11/16/2026: 7.4 has no gxrepl (replication is gxObjRepl); the object
service and its vector searches remain.
==============================================================
*/
// ----------------------------------------------------------- //
#ifndef __GX_DATABASE_SERVER_HPP__
#define __GX_DATABASE_SERVER_HPP__

#include "gxdlcode.h"

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "gxstream.h"

namespace datareel {

struct GXDLCODE_API gxDatabaseServerConfig {
  std::string listen_address;            // Empty: every address
  int listen_port = gxsDATA_PORT;        // 2073
  std::string cert, key, key_passphrase; // This server's certificate
  std::string ca;                        // CA of the client certificates
  std::vector<std::string> allow_cn, allow_fingerprint;
  int max_clients = 4;                   // Authenticated sessions at once
  int max_handshakes = 16;               // TLS handshakes running at once
  int handshake_timeout = 10;            // Seconds a client gets to finish TLS
  int timeout = 120;                     // Seconds for one block afterwards
  unsigned long max_block = 17 * 1024 * 1024; // Largest block read
};

// One authenticated client connection
class GXDLCODE_API gxDatabaseSession {
public:
  gxStream &Stream() { return *stream; }
  int Slot() const { return slot; }
  const std::string &PeerCN() { return peer_cn; }
  const std::string &PeerHost() { return peer_host; }
  // A service may change the name shown in the server's connection list
  void SetDescription(const std::string &d);

  // The first block, read by the server to choose the service. A
  // service reads it from here instead of the stream.
  gxBlockHeader first_header;
  std::vector<unsigned char> first_body;

private:
  friend class gxDatabaseServer;
  std::shared_ptr<gxStream> stream;
  int slot = 0;
  std::string peer_cn, peer_host;
  class gxDatabaseServer *server = 0;
};

class GXDLCODE_API gxDatabaseService {
public:
  virtual ~gxDatabaseService() { }
  virtual const char *ServiceName() const = 0;
  // True if this service takes a session whose first block is this one
  virtual bool Claims(const gxBlockHeader &gx,
		      const std::vector<unsigned char> &body) = 0;
  // Serve the session until it ends. Runs on the session's own thread;
  // the first block is in s.first_header / s.first_body.
  virtual void Serve(gxDatabaseSession &s) = 0;
};

class GXDLCODE_API gxDatabaseServer {
public:
  explicit gxDatabaseServer(const gxDatabaseServerConfig &cfg);
  ~gxDatabaseServer();

  void AddService(gxDatabaseService *svc) { services.push_back(svc); }
  // Called about every tick_seconds from the accept loop (status files)
  void SetTick(std::function<void()> f, int tick_seconds = 5) {
    tick = f; tick_every = tick_seconds;
  }
  // Listen and serve until Stop(). Returns 0, or -1 with "err" set.
  int Run(std::string &err);
  void Stop() { stop = true; }
  bool Stopping() const { return stop; }
  int Port();

  int Clients() const { return clients; }
  int Handshaking() const { return handshaking; }
  // "description since unix-time" for each open session
  std::vector<std::string> Connections();

private:
  friend class gxDatabaseSession;
  void handshake(int slot, std::shared_ptr<gxSocket> c);
  void session(int slot, std::shared_ptr<gxStream> s);
  void describe(int slot, const std::string &d);

private:
  gxDatabaseServerConfig cfg;
  gxStreamServer server;
  std::vector<gxDatabaseService *> services;
  std::function<void()> tick;
  int tick_every = 5;
  std::atomic<bool> stop{false};
  std::atomic<int> clients{0};
  std::atomic<int> handshaking{0};
  std::mutex conn_lock;
  std::map<int, std::string> conn_info;
};

// The log line writer the server uses (stderr with a time stamp by
// default). A program may replace it.
GXDLCODE_API void gxDatabaseServerLog(const char *fmt, ...);

} // namespace datareel

#endif // __GX_DATABASE_SERVER_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
