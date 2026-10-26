// DataReel 7 sample: multi-threaded TLS line server. Replies to each
// line with the line in upper case. "quit" ends a session and "stop"
// stops the server.
// Usage: tls_server port cert.pem key.pem
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include "gxstls.h"
#include "thrpool.h"

using namespace datareel;

static std::atomic<bool> stopping(false);

static void session(std::shared_ptr<gxsTLSConnection> conn, int port)
{
  std::string line;
  while(conn->ReadLine(line)) { // false on close, timeout or error
    if(line == "quit") break;
    if(line == "stop") {
      stopping = true;
      gxSocket wake; // Wake the blocked Accept() so main() can exit
      wake.Connect("localhost", port, 5);
      break;
    }
    for(char &c : line) c = (char)std::toupper((unsigned char)c);
    if(conn->SendLine(line) < 0) break;
  }
  conn->Close();
}

int main(int argc, char **argv)
{
  if(argc < 4) {
    std::cerr << "Usage: tls_server port cert.pem key.pem\n";
    return 1;
  }
  gxsTLSServer server;
  // Listens on IPv4 and IPv6. Returns 0 on success.
  if(server.Listen(std::atoi(argv[1]), argv[2], argv[3]) != 0) {
    std::cerr << "Listen failed: " << server.Error() << "\n";
    return 1;
  }
  std::cout << "Listening on port " << server.Port() << std::endl;
  ThreadPool pool(8);
  while(!stopping) {
    auto conn = std::make_shared<gxsTLSConnection>();
    conn->SetTimeout(120); // Idle sessions are dropped after two minutes
    // Accept() completes the TLS handshake; a failed handshake only
    // affects that client.
    if(server.Accept(*conn) != 0) continue;
    if(stopping) break;
    pool.Submit(session, conn, server.Port());
  }
  pool.Wait();
  return 0;
}
