// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxsping.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
// Date Last Modified: 06/17/2016
// Copyright (c) 2001-2016 DataReel Software Development
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

Embedded ping program using ICMP (Internet Control Message Protocol)
and raw sockets.

Changes:
==============================================================
01/24/2002: Added time.h include file to make time functions
visible during Linux library builds
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <string.h>
#include <time.h>
#include "gxsping.h"

#ifdef __BCC32__
#pragma warn -8071
#endif

gxSocketError gxsPing::Ping(int time_out_sec, int time_out_usec)
{
  // Reset the time values
  // PC-lint 09/14/2005: clock function is considered dangerous 
  time_sent = (int)clock();
  elapsed_time = time_to_live = 0;
  
  if(SendEchoRequest() != gxSOCKET_NO_ERROR) {
    // PC-lint 09/14/2005: clock function is considered dangerous 
    elapsed_time = int((clock() - time_sent) / CLOCKS_PER_SEC);
    return raw_socket->GetSocketError();
  }
  
  if(!WaitForEchoReply(time_out_sec, time_out_usec)) {
    // PC-lint 09/14/2005: clock function is considered dangerous 
    elapsed_time = int((clock() - time_sent) / CLOCKS_PER_SEC);
    return raw_socket->SetSocketError(gxSOCKET_REQUEST_TIMEOUT);
  }

  if(RecvEchoReply() != gxSOCKET_NO_ERROR) {
    // PC-lint 09/14/2005: clock function is considered dangerous 
    elapsed_time = int((clock() - time_sent) / CLOCKS_PER_SEC);
    return raw_socket->GetSocketError();
  }
  
  // Calculate the elapsed time in milliseconds. 
  // time_sent = echo_reply.echo_request.time_sent;
  // PC-lint 09/14/2005: clock function is considered dangerous 
  elapsed_time = int((clock() - time_sent) / CLOCKS_PER_SEC);
  time_to_live = (int)echo_reply.ip_header.time_to_live;

  return gxSOCKET_NO_ERROR;
}

gxSocketError gxsPing::SendEchoRequest()
// Send the echo request header. 
{
  // Initalize the echo request variables
  echo_request.icmp_header.type = gxsICMP_ECHO_REQUEST;
  echo_request.icmp_header.code = 0;
  echo_request.icmp_header.checksum = 0;

  // The echoer will return these same values in the echo reply
  echo_request.icmp_header.identifier = 2; 
  echo_request.icmp_header.sequence = 2;

  // Data to send
  // Ignore BCC32 conversion warning
  for(int i = 0; i < gxsECHO_REQ_DATASIZE; i++) echo_request.data[i] = ' '+i;
  
  // Save tick count when sent
  // PC-lint 09/14/2005: clock function is considered dangerous 
  echo_request.time_sent = (int)clock();
  
  // Calculate the header's checksum
  echo_request.icmp_header.checksum = InChecksum16((u_short *)&echo_request,
						   sizeof(gxsEchoRequest));
  
  raw_socket->SendTo(&echo_request, sizeof(gxsEchoRequest));
  return raw_socket->GetSocketError();
}

gxSocketError gxsPing::RecvEchoReply()
// Receive the echo reply.
{
  raw_socket->RawReadFrom(raw_socket->GetSocket(), &raw_socket->sin,
			  &echo_reply, sizeof(gxsEchoReply));
  return raw_socket->GetSocketError();
}

int gxsPing::WaitForEchoReply(int time_out_sec, int time_out_usec)
// Wait for the echo reply using the select function to signal when data
// is waiting to be read. Returns false if the request times out when the
// specified timeout value is reached.
{  
  return raw_socket->ReadSelect(raw_socket->GetSocket(), time_out_sec,
				time_out_usec);
}

#ifdef __BCC32__
#pragma warn .8071
#endif
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
