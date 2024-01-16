// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: client.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 10/20/2001
// Date Last Modified: 06/17/2016
// Copyright (c) 2001-2024 DataReel Software Development
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

Simple multicast client demo using the gxSocket class. This 
test program uses a UDP socket to join a IP multicast group 
and send/receive datagrams from a multicast group. Multicast 
addresses range from 224.0.0.0 to 239.255.255.255 with addresses 
from 224.0.0.0 to 224.0.0.255 reserved for multicast routing 
information. A multicast address identifies a set of receivers 
for multicast messages. A sender can send datagrams to any group 
without being a member of that group. Clients must be member of
the multicast group in order to receive the multicast datagram
message.

NOTE: The Multicast examples programs will not compile using
BCC32 5.5.1. The compiler complains about the IP_MULTICAST_TTL
definition, the ip_mreq multicast address join structure, and the
IP_ADD_MEMBERSHIP definition. These items are part of the Winsock
library yet the BCC compiler will not recognize them.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include <string.h>
#include <stdlib.h>
#include "gxsocket.h"

#ifdef __BCC32__
int main() { return 0; }
#else
int main(int argc, char **argv)
{
  if(argc != 3) {
    cerr << "Usage: " << argv[0] << " <Multicast IP> <Multicast Port>" 
	 << "\n" << flush;
    cerr << "Example: " << argv[0] << " 225.0.0.39 12345" << "\n" << flush;
    return 1;
  }

  char *multicast_ip = argv[1];
  unsigned short port = (unsigned short) atoi(argv[2]);

  cout << "Construcing multicast client..." << "\n" << flush;
  // NOTE: Do not set the IP address here. This will allow the
  // socket to accept any incoming interface
  gxSocket client(SOCK_DGRAM, SOCK_DGRAM, IPPROTO_UDP, port);
  if(!client) {
    cout << client.SocketExceptionMessage() << "\n" << flush;
    return 1;
  }

  // Bind to the multicast port
  int rv = client.Bind();
  if(rv < 0) {
    cout << client.SocketExceptionMessage() << "\n" << flush;
    return 1;
  }

  ip_mreq mreq; // Multicast address join structure
  mreq.imr_multiaddr.s_addr=inet_addr(multicast_ip);

  // Accept multicast from any interface
  mreq.imr_interface.s_addr=htonl(INADDR_ANY);

  // Join the multicast group using the SetSockOpt() function to
  // request that the kernel join a multicast group.
  rv = client.SetSockOpt(IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *)&mreq,
			 sizeof(mreq));
  if(rv < 0) {
    cout << client.SocketExceptionMessage() << "\n" << flush;
    return 1;
  }
  cout << "Joining multicast group " << multicast_ip << "\n" << flush;
  cout << "Receiving datagrams on port " << port << "\n" << flush;
  cout << "Press Ctrl-C to exit" << "\n" << flush;

  const int buf_len = 4096; // Receive buffer size
  char sbuf[buf_len]; sbuf[0] = 0;
  
  while(1) {
    // Block until the server sends some data
    rv = client.RawReadFrom(sbuf, buf_len);
    if(rv < 0) {
      cout << client.SocketExceptionMessage() << "\n" << flush;
      return 1;
    }
    sbuf[rv] = 0; // Null terminate the string before printing
    cout << sbuf << flush;
  }

  cout << "Exiting..." << "\n" << flush;
  client.Close(); // Close the socket connection
  client.ReleaseSocketLibrary();
  
  return 0;
}
#endif // __BCC32__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
