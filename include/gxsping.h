// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxsping.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
// Date Last Modified: 06/17/2016
// Copyright (c) 2001-2016 DataReel Software Development
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
*/
// ----------------------------------------------------------- //   
#ifndef __GX_PING__HPP__
#define __GX_PING__HPP__

#include "gxdlcode.h"

#include "gxsutils.h"

class GXDLCODE_API gxsPing
{
public:
  gxsPing(gxSocket *s) {
    raw_socket = s; time_sent = elapsed_time = time_to_live = 0;
  }
  ~gxsPing() { }

public: // User interface
  gxSocketError Ping(int time_out_sec = 1, int time_out_usec = 0);

public: // Internal processing functions
  gxSocketError SendEchoRequest();
  gxSocketError RecvEchoReply();  
  int WaitForEchoReply(int time_out_sec = 1, int time_out_usec = 0);
  
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
