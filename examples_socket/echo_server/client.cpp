// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: client.cpp
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

Simple TCP echo client using the gxSocket class.
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
  if((argc < 2) || (argc > 3)) {
    cerr << "Usage: " << argv[0] << " server [port]" << "\n";
    return 1;
  }

  char *server_ip = argv[1];
  unsigned short port = 7;
  if(argc == 3) port = (unsigned short)atoi(argv[2]);

  cout << "Constructing a stream client" << "\n";
  gxSocket client(SOCK_STREAM, port, server_ip);
  if(!client) {
    cout << client.SocketExceptionMessage() << "\n";
    return 1;
  }

  // Connect to the server
  if(client.Connect() < 0) {
    cout << client.SocketExceptionMessage() << "\n";
    return 1;
  }

  char *echo_string = "The quick brown fox jumps over the lazy dog \
0123456789\n";
  int echo_string_len = strlen(echo_string);

  // Send the string to the server
  cout << "Sending a string to the echo server..." << "\n";
  int rv = client.Send((char *)echo_string, echo_string_len);
  if(rv < 0) {
    cout << client.SocketExceptionMessage() << "\n";
    return 1;
  }
  cout << "Sent " << rv << " bytes" << "\n";

  char echo_buffer[255];
  int bytes_read = 0; // Total bytes read
  
  // Read the bytes sent back from the server
  while(bytes_read < echo_string_len) {
    // Block until the server sends some data
    rv = client.RawRead((char *)echo_buffer, sizeof(echo_buffer));
    if(rv < 0) {
      cout << client.SocketExceptionMessage() << "\n";
      break;
    }
    bytes_read += rv;
    echo_buffer[rv] = 0; // Null terminate the string
    cout << echo_buffer;
  }
  cout << "Received " << bytes_read << " bytes" << "\n";

  cout << "Exiting..." << "\n";
  client.Close(); // Close the socket connection
  client.ReleaseSocketLibrary();
  
  return 0;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
