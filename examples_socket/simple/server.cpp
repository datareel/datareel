// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: server.cpp
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

Simple stream server demo using the gxSocket class.
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

#if defined (__LINUX__)
#include <sys/signal.h>
#endif

int main(int argc, char **argv)
{
#if defined (__LINUX__)
  // NOTE: Linux will kill a process that attempts to write to a pipe 
  // with no readers. If this application may send data to a client socket 
  // that has disconnected tell the kernel to ignore the broken pipe.
  signal(SIGPIPE, SIG_IGN);
#endif

  // Check arguments. Should be only one: the port number to bind to.
  if(argc != 2) {
    cerr << "Usage: " << argv[0] << " port" << "\n" << flush;
    return 1;
  }

  unsigned short port = (unsigned short) atoi(argv[1]);

  cout << "Constructing a stream server" << "\n" << flush;
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

  cout << "Opening stream server on host " << hostname << "\n" << flush;
  cout << "Listening on port " << port << "\n" << flush;

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

  // Accept a connection and block execution until data is
  // received from a client connection to this port.
  rv = server.Accept();
  if(rv < 0) {
    cout << server.SocketExceptionMessage() << "\n" << flush;
    return 1;
  }

  // Get the client info (statistical info only)
  char client_name[gxsMAX_NAME_LEN]; int r_port = -1;
  server.GetClientInfo(client_name, r_port);
  cout << "Receiving data from " << client_name 
       << " remote port " << r_port << "\n" << flush;

  const int buf_len = 255; // Receive buffer size
  char sbuf[buf_len];
  // Block until the client sends some data
  rv = server.RawRemoteRead(sbuf, buf_len); 
  if(rv < 0) {
    cout << server.SocketExceptionMessage() << "\n" << flush;
    return 1;
  }
  
  cout << "Received " << rv << " bytes" << "\n" << flush;

  // Print the buffer contents to the stdout for visual conformation
  sbuf[rv] = 0; // Null terminate the buffer
  cout << sbuf;

  cout << "Exiting..." << "\n" << flush;
  server.Close(); // Close the socket connection
  server.ReleaseSocketLibrary();
  return 0;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
