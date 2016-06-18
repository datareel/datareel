// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: testprog.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/23/2001
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

Test program for the embedded telnet client class.
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
#include <time.h>
#include "gxtelnet.h"

#ifdef __MSVC_DEBUG__
#include "leaktest.h"
#endif

int CheckError(gxsTelnetClient *telnetc)
// Checks the specified stream socket for an error condition
{
  if(telnetc->GetSocketError() != gxSOCKET_NO_ERROR) {
    cout << "\n" << flush;
    cout << telnetc->SocketExceptionMessage() << flush << "\n";
    telnetc->Close();
    return 0;
  }
  return 1; // No errors reported
}

int main(int argc, char **argv)
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

  if(argc < 4) {
    cout << "You must specify host, username, and password" << "\n";
    cout << "Example: " << argv[0] << " host usr passwd [port]" << "\n";
    cout << "host = Host name of the telnet server" << "\n";
    cout << "usr  = Your user name" << "\n";
    cout << "passwd = Your password" << "\n";
    return 0;
  }
  char *hostname = argv[1];
  char *username = argv[2];
  char *password = argv[3];
  unsigned short port = (unsigned short)23;
  if(argc == 5) port = atoi(argv[4]);
    
  gxsTelnetClient telnetc;

  // Test terminal emulations
  // telnetc.SetTermType(gxsTEL_NVT_TERM); // Default
  // telnetc.SetTermType(gxsTEL_ENV_TERM); // Use ENV TERM setting
  // telnetc.SetTermType(gxsTEL_VT100F_TERM); // Filtered VT100 emulation

  // Try with WIN32 consoles using the ANSI.SYS driver 
  // telnetc.SetTermType("ANSI");

  cout << "Connecting to " << hostname << " on port " << port << "\n";
  cout << "Our terminal type is: " << telnetc.GetTermType() << "\n";

  telnetc.ConnectClient(hostname, port);
  if(!CheckError(&telnetc)) return 1;
  
  telnetc.RecvInitSeq();
  if(!CheckError(&telnetc)) return 1;

  const int sbuf_size = 4096;
  char sbuf[sbuf_size];
  telnetc.RecvString(sbuf, sbuf_size, "login");
  if(!CheckError(&telnetc)) { 
    cout << sbuf << "\n" << flush;
    return 1;
  }
  cout << sbuf << flush;

  telnetc.SendString((const char *)username, strlen(username));
  if(!CheckError(&telnetc)) return 1;

  telnetc.RecvString(sbuf, sbuf_size, "Password");
  if(!CheckError(&telnetc)) { 
    cout << sbuf << "\n" << flush;
    return 1;
  }
  cout << sbuf << "\n" << flush;

  telnetc.SendString((const char *)password, strlen(password));
  if(!CheckError(&telnetc)) return 1;

  telnetc.RecvString(sbuf, sbuf_size, "$");
  if(!CheckError(&telnetc)) { 
    cout << sbuf << "\n" << flush;
    return 1;
  }
  cout << sbuf << "\n" << flush;

  telnetc.SendString("ls -l /tmp", strlen("ls -l /tmp"));
  if(!CheckError(&telnetc)) return 1;

  telnetc.RecvString(sbuf, sbuf_size, "$");
  cout << sbuf << "\n" << flush;

  cout << "Closing the telnet connection..." << "\n";
  cout << "Exiting..." << "\n";
  telnetc.Close(); 
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
