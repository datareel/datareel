// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: client.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
// Date Last Modified: 05/14/2019
// Copyright (c) 2001-2019 DataReel Software Development
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
#include "dfileb.h"
#include "futils.h"
#include "gxstring.h"

int main(int argc, char **argv)
{
  if(argc != 4) {
    cerr << "Usage: " << argv[0] << " hostname port filename" << "\n" << flush;
    return 1;
  }

  DiskFileB ifile;
  unsigned short port = (unsigned short) atoi(argv[2]);

  ifile.df_Open(argv[3]);
  if(!ifile) {
    cout << "ERROR - Cannot open file " 
         << argv[3] << "\n";
    cout << ifile.df_ExceptionMessage() << "\n";
    return 1;
  }


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

  int error_flag = 0;
  const unsigned read_size = 1024;
  const unsigned buf_size = read_size * 2;
  char rbuf[buf_size];
  unsigned bytes_read = 0;
  int bytes_sent = 0;
  unsigned total_bytes_sent = 0;
  unsigned total_bytes_read = 0;

  while(!ifile.df_EOF()) {
    memset(rbuf, 0, buf_size);
    ifile.df_Read(rbuf, read_size);
    if(ifile.df_GetError() != DiskFileB::df_NO_ERROR) {
      if(ifile.df_GetError() == DiskFileB::df_EOF_ERROR) {
	bytes_read = ifile.df_BytesRead();
	rbuf[bytes_read] = 0;
         bytes_sent = client.Send(rbuf, bytes_read);
         if(bytes_sent < 0) {
	   cout << "ERROR - Socket send error" << "\n" << flush;
           cout << client.SocketExceptionMessage() << "\n" << flush;
           error_flag = 1;
	   break; 
        }
	total_bytes_sent += bytes_sent;
	total_bytes_read += bytes_read;
	error_flag = 0;
	break;
      } 
      cout << "ERROR - A fatal I/O error reading input file" << "\n";
      cout << ifile.df_ExceptionMessage() << "\n";
      error_flag = 1;
      break;
    }
    bytes_read = ifile.df_BytesRead();
    bytes_sent = client.Send(rbuf, bytes_read);
    if(bytes_sent < 0) {
      cout << "ERROR - Socket send error" << "\n" << flush;
      cout << client.SocketExceptionMessage() << "\n" << flush;
      error_flag = 1;
      break; 
    }
    total_bytes_sent += bytes_sent;
    total_bytes_read += bytes_read;
  }
  
  cout << "Read " << total_bytes_read  << " bytes" << "\n" << flush;
  cout << "Sent " << total_bytes_sent  << " bytes" << "\n" << flush;

  if (error_flag != 0) {
    cout << "Exiting with errors" << "\n" << flush;
  }
  else {
    cout << "Exiting" << "\n" << flush;
  }
  client.Close(); // Close the socket connection
  client.ReleaseSocketLibrary();
  
  return error_flag;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
