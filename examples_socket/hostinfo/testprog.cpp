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

Test program used to get host name information.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include "gxsocket.h"

#ifdef __MSVC_DEBUG__
#include "leaktest.h"
#endif

int main(int argc, char **argv)
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

  // Check the command line arguments
  if(argc != 2) {
    cout << "\n";
    cout << "Usage: " << argv[0] << " hostname" << "\n";
    return 1;
  }

  char *hostname = argv[1];
  
  gxSocket sock_lib;
  if(sock_lib.InitSocketLibrary() != 0) {
    cout << sock_lib.SocketExceptionMessage() << "\n";
    return 1;
  }
  
  gxsInternetAddress *ialist;
  gxsHostNameInfo *hostnm = sock_lib.GetHostInformation(hostname);

  if(!hostnm) {
    cout << "Socket exception: Could not resolve hostname" << "\n";
    return 1;
  }

  cout << "\n";
  cout << "Hostname: " << hostnm->h_name << "\n";

  int i;
  for(i = 0; ; i++) {
    char *alias = hostnm->h_aliases[i];
    if(alias == 0) break;
    cout << "Host Alias: " << alias << "\n"; 
  }

  for(i = 0; ; i++) {
    ialist = (gxsInternetAddress *)hostnm->h_addr_list[i];
    if(ialist == 0) break;
    cout << "Host IP Address: " << inet_ntoa(*ialist) << "\n";
  }
  
  // Free the memory allocated for the gxsHostNameInfo data structure
  delete hostnm;
  
  return 0;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
