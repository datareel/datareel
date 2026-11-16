// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: udp_multicast.cpp
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

Send and receive IP multicast datagrams with gxSocket (OpenDatagram,
SendTo, RecvFrom and the multicast socket options). Based on the
2001 multicast sender and receiver programs. A sender does not have
to join a group; a receiver joins it to get its datagrams.

PLAIN TEXT: datagrams are not encrypted or authenticated and anyone
on the network can read or forge them. Use multicast only on a
trusted local network, for data that is not secret.

Usage: udp_multicast send group port [--count N] [--interval ms]
                     [--ttl N] [--interface address|index] [text]
       udp_multicast recv group port [--count N] [--timeout seconds]
                     [--interface address|index]

  group      an IPv4 group (239.0.0.0/8 is for local use, such as
             239.255.0.1) or an IPv6 group (ff02::/16 link-local)
  --ttl      hops the datagrams may cross (1 by default: this subnet)
  --interface  IPv4: the address of the interface to use (127.0.0.1
             keeps everything on this host); IPv6: the interface index
  send sends N datagrams ("text 1", ...), with loopback on so a
  receiver on the same host gets them; recv prints each datagram
  until N arrived (exit 0) or none came for the timeout (exit 1)
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include "gxsocket.h"

using namespace datareel;

static int usage(const char *prog)
{
  std::cerr << "Usage: " << prog << " send group port [--count N] [--interval ms] [--ttl N] "
	    << "[--interface address|index] [text]\n"
	    << "       " << prog << " recv group port [--count N] [--timeout seconds] "
	    << "[--interface address|index]\n";
  return 1;
}

int main(int argc, char **argv)
{
  if(argc < 4) return usage(argv[0]);
  std::string mode = argv[1];
  int count = 5, interval_ms = 200, timeout = 10, ttl = 1;
  std::string text = "multicast", iface;
  std::vector<std::string> pos;
  for(int i = 2; i < argc; i++) {
    std::string a = argv[i];
    if(a == "--count" && i + 1 < argc) count = std::atoi(argv[++i]);
    else if(a == "--interval" && i + 1 < argc) interval_ms = std::atoi(argv[++i]);
    else if(a == "--timeout" && i + 1 < argc) timeout = std::atoi(argv[++i]);
    else if(a == "--ttl" && i + 1 < argc) ttl = std::atoi(argv[++i]);
    else if(a == "--interface" && i + 1 < argc) iface = argv[++i];
    else pos.push_back(a);
  }
  if(pos.size() < 2) return usage(argv[0]);
  if(count < 1) count = 1;
  int port = std::atoi(pos[1].c_str());

  std::vector<gxsAddress> group;
  if(gxsAddress::Resolve(pos[0], port, group, SOCK_DGRAM, AF_UNSPEC) != 0) {
    std::cerr << "Cannot resolve " << pos[0] << "\n";
    return 1;
  }
  const gxsAddress &g = group[0];
  bool v6 = g.Family() == AF_INET6;
  in_addr if4;
  if4.s_addr = htonl(INADDR_ANY);
  unsigned if6 = 0;
  if(!iface.empty()) {
    if(v6) if6 = (unsigned)std::atoi(iface.c_str());
    else if(inet_pton(AF_INET, iface.c_str(), &if4) != 1) {
      std::cerr << "Not an IPv4 address: " << iface << "\n";
      return 1;
    }
  }

  gxSocket s;
  if(mode == "send") {
    if(pos.size() > 2) text = pos[2];
    if(s.OpenDatagram(g.Family()) != 0) {
      std::cerr << s.SocketExceptionMessage() << "\n";
      return 1;
    }
    int rv;
    int loop = 1; // int, not unsigned char: Windows takes a DWORD for these

    if(v6) {
      rv = s.SetSockOpt(IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &ttl, sizeof(ttl));
      if(rv == 0) rv = s.SetSockOpt(IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &loop, sizeof(loop));
      if(rv == 0 && if6) rv = s.SetSockOpt(IPPROTO_IPV6, IPV6_MULTICAST_IF, &if6, sizeof(if6));
    }
    else {
      rv = s.SetSockOpt(IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
      if(rv == 0) rv = s.SetSockOpt(IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));
      if(rv == 0 && !iface.empty()) rv = s.SetSockOpt(IPPROTO_IP, IP_MULTICAST_IF, &if4, sizeof(if4));
    }
    if(rv != 0) {
      std::cerr << "Multicast options: " << s.SocketExceptionMessage() << "\n";
      return 1;
    }
    for(int n = 1; n <= count; n++) {
      std::string msg = text + " " + std::to_string(n);
      if(s.SendTo(g, msg.data(), (int)msg.size()) < 0) {
	std::cerr << "Send to " << g.ToString() << ": " << s.SocketExceptionMessage() << "\n";
	return 1;
      }
      std::cout << "sent \"" << msg << "\" to " << g.ToString() << "\n";
      if(n < count) std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
    return 0;
  }
  if(mode == "recv") {
    // Bind the port on every address, then join the group
    if(s.OpenDatagram(g.Family(), port) != 0) {
      std::cerr << "Bind port " << port << ": " << s.SocketExceptionMessage() << "\n";
      return 1;
    }
    int rv;
    if(v6) {
      ipv6_mreq m;
      std::memset(&m, 0, sizeof(m));
      m.ipv6mr_multiaddr = ((const sockaddr_in6 *)g.SockAddr())->sin6_addr;
      m.ipv6mr_interface = if6;
      rv = s.SetSockOpt(IPPROTO_IPV6, IPV6_JOIN_GROUP, &m, sizeof(m));
    }
    else {
      ip_mreq m;
      std::memset(&m, 0, sizeof(m));
      m.imr_multiaddr = ((const sockaddr_in *)g.SockAddr())->sin_addr;
      m.imr_interface = if4;
      rv = s.SetSockOpt(IPPROTO_IP, IP_ADD_MEMBERSHIP, &m, sizeof(m));
    }
    if(rv != 0) {
      std::cerr << "Join " << pos[0] << ": " << s.SocketExceptionMessage() << "\n";
      return 1;
    }
    std::cout << "joined " << pos[0] << ", receiving on port " << port << "\n" << std::flush;
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
    return 0;   // Closing the socket leaves the group
  }
  return usage(argv[0]);
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
