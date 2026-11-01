// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxsping.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
// Date Last Modified: 10/31/2026
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

#if defined (__UNIX__)
#include <sys/time.h>
#include <unistd.h>
#endif

static long gxsPingNowMS()
// 10/31/2026: Wall clock milliseconds, used for the reply deadline
{
#if defined (__WIN32__)
  return (long)GetTickCount();
#elif defined (__UNIX__)
  struct timeval tv;
  gettimeofday(&tv, 0);
  return (long)(tv.tv_sec % 1000000L) * 1000L + (long)(tv.tv_usec / 1000);
#else
  return (long)(time(0) % 1000000L) * 1000L;
#endif
}

int gxsPing::ParseEchoReply(const unsigned char *buf, int n,
			    u_short identifier, u_short sequence, int &ttl)
// 10/31/2026: The reply was not checked at all: any packet the raw
// socket saw (our own echo request when pinging the local host, another
// program's reply, any ICMP error, a short packet) was taken as the
// reply, and the TTL was read from whatever the structure held.
{
  ttl = -1;
  if(!buf || (n < 20)) return 0;      // Shorter than a minimal IP header
  if((buf[0] >> 4) != 4) return 0;    // Not IPv4
  int ihl = (buf[0] & 0x0F) * 4;
  if((ihl < 20) || (n < ihl + 8)) return 0;
  const unsigned char *icmp = buf + ihl;
  if((icmp[0] != (unsigned char)gxsICMP_ECHO_REPLY) || (icmp[1] != 0)) return 0;
  // Compare the identifier and sequence as stored in the request (the
  // echoer returns the same bytes)
  u_short rid, rseq;
  memcpy(&rid, icmp + 4, sizeof(rid));
  memcpy(&rseq, icmp + 6, sizeof(rseq));
  if((rid != identifier) || (rseq != sequence)) return 0;
  ttl = buf[8];
  return 1;
}

int gxsPing::SameAddress(const gxsSocketAddress *from, 
			 const gxsSocketAddress *target)
// 10/31/2026: True if the reply came from the host pinged
{
  if(!from || !target) return 0;
  if(from->sin_family != AF_INET) return 0;
  return memcmp(&from->sin_addr, &target->sin_addr, sizeof(from->sin_addr)) == 0;
}

static int gxsPingRecvOne(gxsPing *p)
// 10/31/2026: Read one packet. Returns 1 if it is the reply to the
// request sent (it is copied into p->echo_reply), 0 if it is another
// packet, or -1 if the read failed. The sender's address is read into
// a local address; it was read into the socket's own address (sin), so
// any packet changed the host the next request went to.
{
  unsigned char buf[1500];
  gxsSocketAddress from;
  memset(&from, 0, sizeof(from));
  int n = p->raw_socket->RawReadFrom(p->raw_socket->GetSocket(), &from,
				     buf, (int)sizeof(buf));
  if(n <= 0) return -1;
  int ttl;
  if(!gxsPing::SameAddress(&from, &p->raw_socket->sin)) return 0;
  if(!gxsPing::ParseEchoReply(buf, n, p->echo_request.icmp_header.identifier,
			      p->echo_request.icmp_header.sequence, ttl)) {
    return 0;
  }
  memset(&p->echo_reply, 0, sizeof(p->echo_reply));
  memcpy(&p->echo_reply, buf, 
	 (n < (int)sizeof(p->echo_reply)) ? (size_t)n : sizeof(p->echo_reply));
  return 1;
}

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
  
  // 10/31/2026: Wait for the reply to this request until the timeout.
  // Packets that are not the reply (see ParseEchoReply()) are skipped;
  // the first packet of any kind used to end the wait.
  long time_out_ms = (long)time_out_sec * 1000L + (long)(time_out_usec / 1000);
  long start_ms = gxsPingNowMS();
  for(;;) {
    long elapsed_ms = gxsPingNowMS() - start_ms;
    if(elapsed_ms < 0) elapsed_ms = time_out_ms; // The clock wrapped
    long left_ms = time_out_ms - elapsed_ms;
    if((left_ms <= 0) || 
       !WaitForEchoReply((int)(left_ms / 1000L), (int)((left_ms % 1000L) * 1000L))) {
      // PC-lint 09/14/2005: clock function is considered dangerous 
      elapsed_time = int((clock() - time_sent) / CLOCKS_PER_SEC);
      return raw_socket->SetSocketError(gxSOCKET_REQUEST_TIMEOUT);
    }
    int rv = gxsPingRecvOne(this);
    if(rv == 1) break;
    if(rv < 0) {
      // PC-lint 09/14/2005: clock function is considered dangerous 
      elapsed_time = int((clock() - time_sent) / CLOCKS_PER_SEC);
      return raw_socket->SetSocketError(gxSOCKET_RECEIVE_ERROR);
    }
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
  // 10/31/2026: The identifier and sequence were always 2, so a reply to
  // another ping program, or to an earlier request, matched. The
  // identifier is now the process ID and the sequence counts up.
#if defined (__WIN32__)
  echo_request.icmp_header.identifier = (u_short)GetCurrentProcessId();
#elif defined (__UNIX__)
  echo_request.icmp_header.identifier = (u_short)getpid();
#else
  echo_request.icmp_header.identifier = 2; 
#endif
  echo_request.icmp_header.sequence++;

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
// 10/31/2026: Returns gxSOCKET_RECEIVE_ERROR if the packet read is not
// the reply to the request sent (see ParseEchoReply()); the sender's
// address no longer replaces the socket's address.
{
  int rv = gxsPingRecvOne(this);
  if(rv == 1) return raw_socket->SetSocketError(gxSOCKET_NO_ERROR);
  return raw_socket->SetSocketError(gxSOCKET_RECEIVE_ERROR);
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
