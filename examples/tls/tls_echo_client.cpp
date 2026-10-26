// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: tls_echo_client.cpp
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

TLS echo client. Connects to tls_echo_server, sends each line read
from standard input and prints the reply. The server certificate is
checked against the system certificate store, or the CA file given
with --ca. --insecure skips the check (test servers only).

Usage: tls_echo_client host port [--ca ca.pem] [--insecure]
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include "gxstls.h"

int main(int argc, char **argv)
{
  if(argc < 3) {
    std::cerr << "Usage: " << argv[0] 
	      << " host port [--ca ca.pem] [--insecure]\n";
    return 1;
  }
  gxsTLSConnection conn;
  for(int i = 3; i < argc; i++) {
    if(strcmp(argv[i], "--ca") == 0 && i + 1 < argc) conn.SetCAFile(argv[++i]);
    else if(strcmp(argv[i], "--insecure") == 0) conn.SetVerifyPeer(false);
  }
  conn.SetTimeout(30);
  if(conn.Connect(argv[1], std::atoi(argv[2])) != 0) {
    std::cerr << conn.Error() << "\n";
    return 1;
  }
  std::cerr << "Connected with TLS to " << argv[1] << "\n";

  std::string line, reply;
  while(std::getline(std::cin, line)) {
    if(conn.SendLine(line) < 0) {
      std::cerr << conn.Error() << "\n";
      return 1;
    }
    if(line == "quit") break;
    if(!conn.ReadLine(reply)) {
      std::cerr << "Connection closed" << "\n";
      break;
    }
    std::cout << reply << "\n";
    if(line == "shutdown") break;
  }
  conn.Close();
  return 0;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
