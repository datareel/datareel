// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: testprog.cpp
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

Test program for the embedded ping library.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include "gxsping.h"

#ifdef __MSVC_DEBUG__
#include "leaktest.h"
#endif

int main(int argc, char **argv)
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

#ifdef __UNIX__
  // Raw sockets require root access on all UNIX platforms. Programs
  // that use raw sockets can be excucuted by non-root users if the
  // effective user ID of the executing process is set to the same as
  // the file's owner. In the case root must own the executable and
  // the effective User-ID or Group-ID on execution bit must be set
  // by root using the chmod command: "chmod u+s" or "chmod 4755"
  // Now the effective user ID of the executing process will be set to 
  // the same user ID of the file's owner. 
  if(geteuid() != 0) {
    cout << "You must be root to run this program" << "\n";
    return 1;
  }
#endif

  // Check the command line arguments
  if(argc != 2) {
    cout << "\n";
    cout << "Usage: " << argv[0] << " hostname" << "\n";
    return 1;
  }

  char *hostname = argv[1];
  
  // Construct a raw socket
  gxSocket raw_socket(AF_INET, SOCK_RAW, IPPROTO_ICMP, 0, hostname);
  if(!raw_socket) {
    cout << raw_socket.SocketExceptionMessage() << "\n";
    return 1;
  }
  
  char ip_address[gxsMAX_NAME_LEN];
  raw_socket.GetBoundIPAddress(ip_address);
  cout << "Pinging " << ip_address << " with " << gxsECHO_REQ_DATASIZE
       << " bytes of data..." << "\n";

  gxsPing ping(&raw_socket);
  
  // Ping with a timeout value of one second
  if(ping.Ping(1) != gxSOCKET_NO_ERROR) {
    cout << ip_address << " is not responding" << "\n";
    cout << raw_socket.SocketExceptionMessage() << "\n";
    return 1;
  }

  cout << "Reply from " << ip_address << ": bytes=" << gxsECHO_REQ_DATASIZE
       << " time=" << ping.elapsed_time << "ms"
       << " TTL=" << ping.time_to_live << "\n";

  return 0;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
