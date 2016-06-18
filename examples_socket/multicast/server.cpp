// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: server.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 10/30/2001
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

Simple multicast server demo using the gxSocket class. This 
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
  // Check arguments. Should be only one: the port number to bind to.
  if(argc != 3) {
    cerr << "Usage: " << argv[0] << " <Multicast IP> <Multicast Port>" 
	 << "\n" << flush;
    cerr << "Example: " << argv[0] << " 225.0.0.39 12345" << "\n" << flush;
    return 1;
  }

  char *multicast_ip = argv[1];
  unsigned short port = (unsigned short) atoi(argv[2]);
  int rv;

  cout << "Construcing datagram socket..." << "\n" << flush;
  gxSocket server(SOCK_DGRAM, SOCK_DGRAM, IPPROTO_UDP, port, multicast_ip);
  if(!server) {
    cout << server.SocketExceptionMessage() << "\n" << flush;
    return 1;
  }
  
  // Set the TTL of multicast packet to limit the number of hops a 
  // multicast packet can traverse from the sender. As the TTL values 
  // increase a router will expand the number of hops they will forward a
  // multicast packet:
  // 0:   Restricted to the same host 
  // 1:   Restricted to the same subnet 
  // 32:  Restricted to the same site 
  // 64:  Restricted to the same region 
  // 128: Restricted to the same continent 
  // 255: Unrestricted 
  unsigned char multicast_TTL = 1; // Restricted to the same subnet
  rv = server.SetSockOpt(IPPROTO_IP, IP_MULTICAST_TTL,
			 (void *)&multicast_TTL, sizeof(multicast_TTL));
  if(rv < 0) {
    cout << "Could not change the default IP packet TTL" << "\n" << flush;
    cout <<  server.SocketExceptionMessage() << "\n" << flush;
    cout << "\n" << flush;
  }

  cout << "Sending multicast messages to group " << multicast_ip << "\n" 
       << flush;
  cout << "Sending datagrams on port " << port << "\n" << flush;
  cout << "Press Ctrl-C to exit" << "\n" << flush;

  const char *message = "The quick brown fox jumps over the lazy dog \
0123456789\n";

  while(1) {
    rv = server.SendTo((char *)message, strlen(message));
    if(rv < 0) {
      cout << server.SocketExceptionMessage() << "\n" << flush;
      break;
    }
#ifdef __WIN32__
    Sleep(1000);
#else
    sleep(1);
#endif
  }

  cout << "Exiting..." << "\n" << flush;
  server.Close(); // Close the socket connection
  server.ReleaseSocketLibrary();
  return 0;
}
#endif // __BCC32__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
