// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxsping.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
// Date Last Modified: 10/31/2026
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

Embedded ping program using ICMP (Internet Control Message Protocol)
and raw sockets.
*/
// ----------------------------------------------------------- //   
#ifndef __GX_PING__HPP__
#define __GX_PING__HPP__

#include "gxdlcode.h"

#include <string.h> // 10/31/2026: memset()
#include "gxsutils.h"

class GXDLCODE_API gxsPing
{
public:
  gxsPing(gxSocket *s) {
    raw_socket = s; time_sent = elapsed_time = time_to_live = 0;
    // 10/31/2026: The sequence number in echo_request counts up
    memset(&echo_request, 0, sizeof(echo_request));
    memset(&echo_reply, 0, sizeof(echo_reply));
  }
  ~gxsPing() { }

public: // User interface
  gxSocketError Ping(int time_out_sec = 1, int time_out_usec = 0);

public: // Internal processing functions
  gxSocketError SendEchoRequest();
  gxSocketError RecvEchoReply();  
  int WaitForEchoReply(int time_out_sec = 1, int time_out_usec = 0);

public: // 10/31/2026: Reply checks (DataReel 7.3.1)
  // Validate one packet read from an IPv4 raw ICMP socket: "buf" holds
  // "n" bytes, starting with the IP header. Returns 1 if it is an echo
  // reply with this identifier and sequence number (as stored in the
  // request) and sets "ttl", or 0 if it is short, malformed or another
  // packet. Pure function, also used by the tests.
  static int ParseEchoReply(const unsigned char *buf, int n,
			    u_short identifier, u_short sequence, int &ttl);
  // True if "from" holds the same IPv4 address as "target"
  static int SameAddress(const gxsSocketAddress *from, 
			 const gxsSocketAddress *target);
  
public:
  gxSocket *raw_socket;        // Ping socket
  gxsEchoRequest echo_request; // Echo request header
  gxsEchoReply echo_reply;     // Echo reply header

  int time_sent;    // Time the packet was sent
  int elapsed_time; // Time in millisecond for the packet to return 
  int time_to_live; // Time to live
};

#endif // __GX_PING__HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
