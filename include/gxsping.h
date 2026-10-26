// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxsping.h
// C++ Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
// Date Last Modified: 10/26/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- // 
// ---------- Include File Description and Details  ---------- // 
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

Embedded ping program using ICMP (Internet Control Message Protocol)
and raw sockets.

Changes:
==============================================================
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
#ifndef __GX_PING__HPP__
#define __GX_PING__HPP__

#include "gxdlcode.h"

#include <string>

#include "gxsocket.h"

namespace datareel {


struct GXDLCODE_API gxsPingResult
{
  bool ok = false;      // True if an echo reply arrived in time
  double rtt_ms = 0;    // Round trip time in milliseconds
  int ttl = -1;         // Reply time to live, -1 if not known
  std::string address;  // Numeric address that was pinged
  std::string error;    // Reason for a failure
};

class GXDLCODE_API gxsPing
{
public:
  gxsPing() { }

public:
  void SetTimeout(int milliseconds) { timeout_ms = milliseconds; }

  // Send one ICMP or ICMPv6 echo request to host and wait for the
  // reply. family is AF_UNSPEC, AF_INET or AF_INET6. Returns 0 if a
  // reply arrived.
  int Ping(const std::string &host, gxsPingResult &result, 
	   int family = AF_UNSPEC);

private:
  int timeout_ms = 1000;
  unsigned short sequence = 0;
};

} // namespace datareel

#endif // __GX_PING__HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
