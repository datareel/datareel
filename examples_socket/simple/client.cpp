// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: client.cpp
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

Simple stream client demo using the gxSocket class.
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
  if(argc != 3) {
    cerr << "Usage: " << argv[0] << " hostname port" << "\n" << flush;
    return 1;
  }

  unsigned short port = (unsigned short) atoi(argv[2]);

  cout << "Constructing a stream client" << "\n" << flush;
  gxSocket client(SOCK_STREAM, port, argv[1]);
  if(!client) {
    cout << client.SocketExceptionMessage() << "\n" << flush;
    return 1;
  }

  // Connect to the server
  if(client.Connect() < 0) {
    cout << client.SocketExceptionMessage() << "\n" << flush;
    return 1;
  }

  char *test_block = "The quick brown fox jumps over the lazy dog \
0123456789\n";
  
  // Send a block of data
  cout << "Sending a block " << strlen(test_block) << " bytes long..." 
       << "\n" << flush;
  int rv = client.Send((char *)test_block, strlen(test_block));
  if(rv < 0) {
    cout << client.SocketExceptionMessage() << "\n" << flush;
    return 1;
  }

  cout << "Sent " << rv << " bytes" << "\n" << flush;
  cout << "Exiting..." << "\n" << flush;
  client.Close(); // Close the socket connection
  client.ReleaseSocketLibrary();
  
  return 0;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
