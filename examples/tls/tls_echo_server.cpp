// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: tls_echo_server.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 10/26/2026
// Date Last Modified: 10/26/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- // 
// ------------- Program Description and Details ------------- // 
// ----------------------------------------------------------- // 
/*
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.
 
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  
USA

TLS echo server. Each client runs on a datareel::ThreadPool worker
and every line it sends is sent back. A client that sends "quit"
is disconnected, and "shutdown" stops the server.

Usage: tls_echo_server port cert.pem key.pem [threads]
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include "gxstls.h"
#include "thrpool.h"

using namespace datareel;

static std::mutex console_mutex;
static std::atomic<bool> stopping(false);

static void say(const std::string &s)
{
  std::lock_guard<std::mutex> lock(console_mutex);
  std::cout << s << std::endl;
}

static void serve_client(std::shared_ptr<gxsTLSConnection> conn, int port)
{
  std::string peer = conn->Socket().PeerAddress().ToString();
  say("Client connected: " + peer);
  std::string line;
  while(conn->ReadLine(line)) {
    if(line == "quit") break;
    if(line == "shutdown") {
      stopping = true;
      conn->SendLine("Server shutting down");
      // Wake the accept() call with a connection of our own
      gxSocket wake;
      wake.Connect("localhost", port, 5);
      break;
    }
    if(conn->SendLine(line) < 0) break;
  }
  conn->Close();
  say("Client disconnected: " + peer);
}

int main(int argc, char **argv)
{
  if(argc < 4) {
    std::cerr << "Usage: " << argv[0] << " port cert.pem key.pem [threads]\n";
    return 1;
  }
  int port = std::atoi(argv[1]);
  std::size_t threads = argc > 4 ? (std::size_t)std::atoi(argv[4]) : 4;

  gxsTLSServer server;
  if(server.Listen(port, argv[2], argv[3]) != 0) {
    std::cerr << server.Error() << "\n";
    return 1;
  }
  say("Listening on port " + std::to_string(server.Port()));

  ThreadPool pool(threads);
  while(!stopping) {
    auto conn = std::make_shared<gxsTLSConnection>();
    conn->SetTimeout(300);
    if(server.Accept(*conn) != 0) {
      if(!stopping) say("Accept failed: " + server.Error());
      continue;
    }
    if(stopping) break;
    pool.Submit(serve_client, conn, port);
  }
  pool.Wait();
  server.Close();
  say("Server stopped");
  return 0;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
