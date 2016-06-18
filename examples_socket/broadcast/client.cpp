// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: client.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 10/20/2001
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

Simple broadcast receiver demo using the gxSocket class. 
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
  if(argc != 2) {
    cerr << "Usage: " << argv[0] << " <Broadcast Port>" << "\n" << flush;
    cerr << "Example: " << argv[0] << " 12345" << "\n" << flush;
    return 1;
  }

  unsigned short port = (unsigned short) atoi(argv[1]);

  cout << "Constructing broadcast client..." << "\n" << flush;
  // NOTE: Do not set the IP address here. This will allow the
  // socket to accept any incoming interface
  gxSocket client(SOCK_DGRAM, SOCK_DGRAM, IPPROTO_UDP, port);
  if(!client) {
    cout << client.SocketExceptionMessage() << "\n" << flush;
    return 1;
  }

  // Bind to the broadcast port
  int rv = client.Bind();
  if(rv < 0) {
    cout << client.SocketExceptionMessage() << "\n" << flush;
    return 1;
  }

  cout << "Receiving broadcast datagrams on port " << port << "\n" << flush;
  cout << "Press Ctrl-C to exit" << "\n" << flush;

  const int buf_len = 255; // Receive buffer size
  char sbuf[buf_len]; sbuf[0] = 0;
  
  while(1) {
    rv = client.RawReadFrom(sbuf, buf_len); // Non-blocking read
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
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
