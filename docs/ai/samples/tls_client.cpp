// DataReel 7 sample: TLS client that sends lines and prints the replies.
// Works with examples/tls/tls_echo_server.
// Usage: tls_client host port [ca.pem]
#include <cstdlib>
#include <iostream>
#include <string>
#include "gxstls.h"

using namespace datareel;

int main(int argc, char **argv)
{
  if(argc < 3) {
    std::cerr << "Usage: tls_client host port [ca.pem]\n";
    return 1;
  }
  gxsTLSConnection conn;
  conn.SetTimeout(30);                 // Seconds, for connect and each read
  if(argc > 3) conn.SetCAFile(argv[3]); // Trust a private CA as well
  // Certificates are verified against the system store and the host
  // name by default. Connect() returns 0 on success.
  if(conn.Connect(argv[1], std::atoi(argv[2])) != 0) {
    std::cerr << "Connect failed: " << conn.Error() << "\n";
    return 1;
  }
  for(const char *msg : { "hello", "DataReel 7" }) {
    std::string reply;
    if(conn.SendLine(msg) < 0 || !conn.ReadLine(reply)) {
      std::cerr << "I/O failed: " << conn.Error() << "\n";
      return 1;
    }
    std::cout << "Sent \"" << msg << "\", got \"" << reply << "\"\n";
  }
  conn.SendLine("quit");
  conn.Close();
  return 0;
}
