// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: server.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/02/2001
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

Simple TCP echo server using the gxSocket class.
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

int HandleClientRequest(gxSocket *server)
// Function used to handle the client's request.
// Returns a non-zero value if an error occurs.
{
  // Record the client socket assigned by the kernel
  gxsSocket_t client_sock = server->GetRemoteSocket();

  const int buf_len = 255; // Receive buffer size
  char echo_buffer[buf_len];

  // Block until the client sends some data
  int bytes_read = server->RawRead(client_sock, echo_buffer, buf_len);
  if(bytes_read < 0) {
    cout << server->SocketExceptionMessage() << "\n" << flush;
    return -1;
  }

  while(bytes_read > 0) {
    // Echo the message back to the client
    if(server->RawWrite(client_sock, echo_buffer, bytes_read) < 0) {
      cout << server->SocketExceptionMessage() << "\n" << flush;
      return -1;
    }
    
    // Keep receiving data from the client
    bytes_read = server->RawRead(client_sock, echo_buffer, buf_len);
    if(bytes_read < 0) {
      cout << server->SocketExceptionMessage() << "\n" << flush;
      return -1;
    }
  }
  
  server->Close(client_sock);
  return 0;
}

int main(int argc, char **argv)
{
  // Check arguments. Should be only one: the port number to bind to.
  if(argc != 2) {
    cerr << "Usage: " << argv[0] << " port" << "\n" << flush;
    return 1;
  }

  unsigned short port = (unsigned short) atoi(argv[1]);

  cout << "Constructing an echo server" << "\n" << flush;
  gxSocket server(SOCK_STREAM, port);
  if(!server) {
    cout << server.SocketExceptionMessage() << "\n" << flush;
    return 1;
  }
  
  // Get the host name assigned to this machine (statistical info only)
  char hostname[gxsMAX_NAME_LEN];
  int rv = server.GetHostName(hostname);
  if(rv < 0) {
    cout << server.SocketExceptionMessage() << "\n" << flush;
    return 1;
  }

  cout << "Opening echo server on host " << hostname << "\n" << flush;
  cout << "Listening on port " << port << "\n" << flush;
  cout << "Press Ctrl-C to exit" << "\n" << flush;

  // Bind the name to the socket
  rv = server.Bind();
  if(rv < 0) {
    cout << server.SocketExceptionMessage() << "\n" << flush;
    return 1;
  }
 
  // Listen for connections
  rv = server.Listen();
  if(rv < 0) {
    cout << server.SocketExceptionMessage() << "\n" << flush;
    return 1;
  }

  int server_loop = 1; 
  while(server_loop) { // Loop forever accepting connections

    // Accept a connection and block execution until data is
    // received from a client connection to this port.
    rv = server.Accept();
    if(rv < 0) {
      cout << server.SocketExceptionMessage() << "\n" << flush;
      break;
    }
    
    // Get the client info (statistical info only)
    char client_name[gxsMAX_NAME_LEN]; int r_port = -1;
    server.GetClientInfo(client_name, r_port);
    cout << "Receiving data from " << client_name 
	 << " remote port " << r_port << "\n" << flush;
    
    // Handle the clients request
    HandleClientRequest(&server);    
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
