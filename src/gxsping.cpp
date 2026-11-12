// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxsping.cpp
// C++ Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
// Date Last Modified: 11/01/2026
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

Embedded ping program using ICMP (Internet Control Message Protocol)
and raw sockets.

Changes:
==============================================================
01/24/2002: Added time.h include file to make time functions
visible during Linux library builds

10/26/2026: DataReel 7 rewrites gxsPing for IPv4 and IPv6. Linux 
uses unprivileged ICMP datagram sockets when the system allows 
them (net.ipv4.ping_group_range) and raw sockets otherwise. 
Windows uses the IcmpSendEcho functions, which need no 
administrator rights. Round trip times are measured with 
std::chrono in milliseconds. ICMP cannot be encrypted, so ping is 
a diagnostic tool, like telnet.
==============================================================
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <vector>
#include "gxsping.h"

#if defined (_WIN32)
#include <iphlpapi.h>
#include <icmpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#else
#include <unistd.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#endif

namespace datareel {


int gxsPing::ParseEchoReply(const unsigned char *buf, int n, bool v6,
			    bool ip_header, unsigned short id, 
			    unsigned short seq, bool check_id, int &ttl)
// 11/01/2026: Factored out of Ping(). The old code read the TTL at
// buf[8] and trusted the IHL nibble before checking that the reply was
// long enough to hold an IP header.
{
  ttl = -1;
  if(!buf || n <= 0) return 0;
  const unsigned char *icmp = buf;
  if(ip_header && !v6) { // IPv4 raw sockets return the IP header first
    if(n < 20) return 0;                 // Shorter than a minimal header
    if((buf[0] >> 4) != 4) return 0;     // Not IPv4
    int ihl = (buf[0] & 0x0F) * 4;
    if(ihl < 20 || n < ihl + 8) return 0;
    ttl = buf[8];
    icmp = buf + ihl;
    n -= ihl;
  }
  if(n < 8) return 0;
  // ICMP_ECHOREPLY is 0, ICMP6_ECHO_REPLY is 129
  unsigned char reply_type = v6 ? (unsigned char)129 : (unsigned char)0;
  if(icmp[0] != reply_type || icmp[1] != 0) return 0;
  unsigned short rseq = (unsigned short)((icmp[6] << 8) | icmp[7]);
  unsigned short rid = (unsigned short)((icmp[4] << 8) | icmp[5]);
  if(rseq != seq || (check_id && rid != id)) return 0;
  return 1;
}

bool gxsPing::SameAddress(const sockaddr *from, const sockaddr *target)
{
  if(!from || !target || from->sa_family != target->sa_family) return false;
  if(from->sa_family == AF_INET) {
    return memcmp(&((const sockaddr_in *)from)->sin_addr,
		  &((const sockaddr_in *)target)->sin_addr,
		  sizeof(in_addr)) == 0;
  }
  if(from->sa_family == AF_INET6) {
    return memcmp(&((const sockaddr_in6 *)from)->sin6_addr,
		  &((const sockaddr_in6 *)target)->sin6_addr,
		  sizeof(in6_addr)) == 0;
  }
  return false;
}

#if defined (_WIN32)

int gxsPing::Ping(const std::string &host, gxsPingResult &result, int family)
{
  result = gxsPingResult();
  std::vector<gxsAddress> addrs;
  if(gxsAddress::Resolve(host, 0, addrs, SOCK_RAW, family) != 0) {
    result.error = "Ping exception: Could not resolve " + host;
    return -1;
  }
  const gxsAddress &a = addrs[0];
  result.address = a.IPAddress();
  char data[32];
  for(int i = 0; i < (int)sizeof(data); i++) data[i] = (char)('a' + i % 26);
  std::vector<unsigned char> reply(sizeof(ICMPV6_ECHO_REPLY) + 
				   sizeof(ICMP_ECHO_REPLY) + sizeof(data) + 64);
  DWORD n = 0;
  if(a.Family() == AF_INET) {
    HANDLE h = IcmpCreateFile();
    if(h == INVALID_HANDLE_VALUE) {
      result.error = "Ping exception: IcmpCreateFile failed";
      return -1;
    }
    IPAddr dest = ((const sockaddr_in *)a.SockAddr())->sin_addr.s_addr;
    n = IcmpSendEcho(h, dest, data, sizeof(data), 0, reply.data(),
		     (DWORD)reply.size(), (DWORD)timeout_ms);
    IcmpCloseHandle(h);
    if(n > 0) {
      ICMP_ECHO_REPLY *r = (ICMP_ECHO_REPLY *)reply.data();
      if(r->Status == IP_SUCCESS) {
	result.ok = true;
	result.rtt_ms = (double)r->RoundTripTime;
	result.ttl = r->Options.Ttl;
	return 0;
      }
    }
  }
  else {
    HANDLE h = Icmp6CreateFile();
    if(h == INVALID_HANDLE_VALUE) {
      result.error = "Ping exception: Icmp6CreateFile failed";
      return -1;
    }
    sockaddr_in6 src;
    memset(&src, 0, sizeof(src));
    src.sin6_family = AF_INET6;
    n = Icmp6SendEcho2(h, 0, 0, 0, &src, (sockaddr_in6 *)a.SockAddr(),
		       data, sizeof(data), 0, reply.data(), 
		       (DWORD)reply.size(), (DWORD)timeout_ms);
    IcmpCloseHandle(h);
    if(n > 0) {
      ICMPV6_ECHO_REPLY *r = (ICMPV6_ECHO_REPLY *)reply.data();
      if(r->Status == IP_SUCCESS) {
	result.ok = true;
	result.rtt_ms = (double)r->RoundTripTime;
	return 0;
      }
    }
  }
  result.error = "Ping exception: Request timed out";
  return -1;
}

#else // POSIX

static unsigned short icmp_checksum(const unsigned char *p, int len)
{
  unsigned long sum = 0;
  for(; len > 1; len -= 2, p += 2) sum += (unsigned long)((p[0] << 8) | p[1]);
  if(len == 1) sum += (unsigned long)(p[0] << 8);
  while(sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
  return (unsigned short)~sum;
}

int gxsPing::Ping(const std::string &host, gxsPingResult &result, int family)
{
  result = gxsPingResult();
  std::vector<gxsAddress> addrs;
  if(gxsAddress::Resolve(host, 0, addrs, SOCK_DGRAM, family) != 0) {
    result.error = "Ping exception: Could not resolve " + host;
    return -1;
  }
  const gxsAddress &a = addrs[0];
  result.address = a.IPAddress();
  bool v6 = (a.Family() == AF_INET6);
  int proto = v6 ? (int)IPPROTO_ICMPV6 : (int)IPPROTO_ICMP;

  // Unprivileged ICMP sockets first, then raw sockets (root or
  // CAP_NET_RAW)
  bool raw = false;
  int s = socket(a.Family(), SOCK_DGRAM, proto);
  if(s < 0) {
    s = socket(a.Family(), SOCK_RAW, proto);
    raw = true;
  }
  if(s < 0) {
    result.error = std::string("Ping exception: Cannot open an ICMP socket: ") +
      strerror(errno);
    return -1;
  }

  unsigned short id = (unsigned short)(getpid() & 0xFFFF);
  unsigned short seq = ++sequence;
  unsigned char pkt[64];
  memset(pkt, 0, sizeof(pkt));
  pkt[0] = v6 ? (unsigned char)ICMP6_ECHO_REQUEST : (unsigned char)ICMP_ECHO;
  pkt[4] = (unsigned char)(id >> 8); pkt[5] = (unsigned char)id;
  pkt[6] = (unsigned char)(seq >> 8); pkt[7] = (unsigned char)seq;
  for(int i = 8; i < (int)sizeof(pkt); i++) pkt[i] = (unsigned char)i;
  if(!v6) { // The kernel computes the ICMPv6 checksum
    unsigned short ck = icmp_checksum(pkt, sizeof(pkt));
    pkt[2] = (unsigned char)(ck >> 8); pkt[3] = (unsigned char)ck;
  }

  auto start = std::chrono::steady_clock::now();
  if(sendto(s, pkt, sizeof(pkt), 0, a.SockAddr(), a.Length()) < 0) {
    result.error = std::string("Ping exception: ") + strerror(errno);
    close(s);
    return -1;
  }

  for(;;) {
    auto now = std::chrono::steady_clock::now();
    int left = timeout_ms - (int)std::chrono::duration_cast
      <std::chrono::milliseconds>(now - start).count();
    if(left <= 0 || gxSocket::WaitReadable(s, left) != 1) break;
    unsigned char buf[1500];
    // 11/01/2026: recvfrom() so a reply from another host (raw sockets
    // see every ICMP packet) cannot be taken for the target's reply
    sockaddr_storage from;
    memset(&from, 0, sizeof(from));
    socklen_t from_len = sizeof(from);
    ssize_t n = recvfrom(s, buf, sizeof(buf), 0, (sockaddr *)&from, &from_len);
    if(n <= 0) break;
    if(!SameAddress((const sockaddr *)&from, a.SockAddr())) continue;
    int ttl = -1;
    // Datagram sockets replace the identifier with the local port
    if(!ParseEchoReply(buf, (int)n, v6, raw && !v6, id, seq, raw, ttl)) {
      continue;
    }
    auto end = std::chrono::steady_clock::now();
    result.rtt_ms = std::chrono::duration<double, std::milli>(end - start).count();
    result.ttl = ttl;
    result.ok = true;
    close(s);
    return 0;
  }
  close(s);
  result.error = "Ping exception: Request timed out";
  return -1;
}

#endif
} // namespace datareel

// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
