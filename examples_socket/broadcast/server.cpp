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

Simple broadcast sender demo using the gxSocket class. 
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

int main(int argc, char **argv)
{
  // Check arguments. Should be only one: the port number to bind to.
  if(argc != 3) {
    cerr << "Usage: " << argv[0] << " <Broadcast IP> <Broadcast Port>" 
	 << "\n" << flush;
    cerr << "Example: " << argv[0] << " 192.168.0.255 12345" << "\n" 
	 << flush;
    return 1;
  }

  char *broadcast_ip = argv[1];
  unsigned short port = (unsigned short) atoi(argv[2]);
  int rv;

  cout << "Construcing datagram socket..." << "\n" << flush;
  gxSocket server(SOCK_DGRAM, SOCK_DGRAM, IPPROTO_UDP, port, broadcast_ip);
  if(!server) {
    cout << server.SocketExceptionMessage() << "\n" << flush;
    return 1;
  }
  
  // Set the socket option to allow broadcasting. By default
  // sockets cannot broadcast. NOTE: Local broadcast messages
  // are never forwarded by routers.
  int broadcast_permission = 1;
  rv = server.SetSockOpt(SOL_SOCKET, SO_BROADCAST, 
			 (void *)&broadcast_permission, 
			 sizeof(broadcast_permission));
  if(rv < 0) {
    cout << "Could not set socket with broadcast permission" << "\n" << flush;
    cout <<  server.SocketExceptionMessage() << "\n" << flush;
    return 0;
  }

  cout << "Sending broadcast messages to network " << broadcast_ip << "\n" 
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

    // Set a long delay between messages to avoid flooding the network
#ifdef __WIN32__
    Sleep(3000); // 3 second delay
#else
    sleep(3);
#endif
  }

  cout << "Exiting..." << "\n" << flush;
  server.Close(); // Close the socket connection
  server.ReleaseSocketLibrary();
  return 0;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
