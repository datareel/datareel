// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: udp_broadcast.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/16/2026
// Date Last Modified: 11/16/2026
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

Send and receive UDP broadcast datagrams with gxSocket (OpenDatagram,
SendTo, RecvFrom). Based on the 2001 broadcast sender and receiver
programs.

PLAIN TEXT: datagrams are not encrypted or authenticated and anyone
on the network can read or forge them. Use broadcasts only on a
trusted local network, for data that is not secret.

Usage: udp_broadcast send address port [--count N] [--interval ms]
                     [text]
       udp_broadcast recv port [--count N] [--timeout seconds]

  send  sends N datagrams ("text 1", "text 2", ...) to the address,
        such as 255.255.255.255, a subnet's broadcast address
        (192.168.1.255) or 127.255.255.255 on this host
  recv  prints each datagram and its sender until N arrived (exit 0)
        or no datagram came for the timeout (exit 1)
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include "gxsocket.h"

using namespace datareel;

static int usage(const char *prog)
{
  std::cerr << "Usage: " << prog << " send address port [--count N] [--interval ms] [text]\n"
	    << "       " << prog << " recv port [--count N] [--timeout seconds]\n";
  return 1;
}

int main(int argc, char **argv)
{
  if(argc < 3) return usage(argv[0]);
  std::string mode = argv[1];
  int count = 5, interval_ms = 200, timeout = 10;
  std::string text = "broadcast";
  std::vector<std::string> pos;
  for(int i = 2; i < argc; i++) {
    std::string a = argv[i];
    if(a == "--count" && i + 1 < argc) count = std::atoi(argv[++i]);
    else if(a == "--interval" && i + 1 < argc) interval_ms = std::atoi(argv[++i]);
    else if(a == "--timeout" && i + 1 < argc) timeout = std::atoi(argv[++i]);
    else pos.push_back(a);
  }
  if(count < 1) count = 1;

  gxSocket s;
  if(mode == "send") {
    if(pos.size() < 2) return usage(argv[0]);
    if(pos.size() > 2) text = pos[2];
    std::vector<gxsAddress> to;
    if(gxsAddress::Resolve(pos[0], std::atoi(pos[1].c_str()), to, SOCK_DGRAM, AF_INET) != 0) {
      std::cerr << "Cannot resolve " << pos[0] << "\n";
      return 1;
    }
    if(s.OpenDatagram(AF_INET) != 0) {
      std::cerr << s.SocketExceptionMessage() << "\n";
      return 1;
    }
    int on = 1; // A broadcast address needs SO_BROADCAST
    if(s.SetSockOpt(SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) != 0) {
      std::cerr << "SO_BROADCAST: " << s.SocketExceptionMessage() << "\n";
      return 1;
    }
    for(int n = 1; n <= count; n++) {
      std::string msg = text + " " + std::to_string(n);
      if(s.SendTo(to[0], msg.data(), (int)msg.size()) < 0) {
	std::cerr << "Send to " << to[0].ToString() << ": " << s.SocketExceptionMessage() << "\n";
	return 1;
      }
      std::cout << "sent \"" << msg << "\" to " << to[0].ToString() << "\n";
      if(n < count) std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
    return 0;
  }
  if(mode == "recv") {
    if(pos.empty()) return usage(argv[0]);
    if(s.OpenDatagram(AF_INET, std::atoi(pos[0].c_str())) != 0) {
      std::cerr << "Bind port " << pos[0] << ": " << s.SocketExceptionMessage() << "\n";
      return 1;
    }
    std::cout << "receiving broadcasts on port " << pos[0] << "\n" << std::flush;
    char buf[65536];
    for(int n = 0; n < count; n++) {
      gxsAddress from;
      int bytes = s.RecvFrom(from, buf, (int)sizeof(buf), timeout);
      if(bytes < 0) {
	std::cerr << "no datagram for " << timeout << " seconds (" << n << " of "
		  << count << " received)\n";
	return 1;
      }
      std::cout << "from " << from.ToString() << ": " << std::string(buf, bytes) << "\n" << std::flush;
    }
    std::cout << count << " datagrams received\n";
    return 0;
  }
  return usage(argv[0]);
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
