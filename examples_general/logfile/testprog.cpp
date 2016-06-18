// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: testprog.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/17/1995
// Date Last Modified: 06/17/2016
// Copyright (c) 2001-2016 DataReel Software Development
// ----------------------------------------------------------- // 
// ------------- Program description and details ------------- // 
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

This is a test program for the LogFile class.
*/
// ----------------------------------------------------------- //   
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include "logfile.h"

#ifdef __MSVC_DEBUG__
#include "leaktest.h"
#endif

static LogFile *SysLog;

int main()
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

  SysLog = new LogFile;
  char *fname = "logfile.log";
  
  char stime[255];
  cout << "Writing to logfile.log @ " << SysLog->GetSystemTime(stime) << "\n";
  if(SysLog->OverWrite(fname) != 0) {
    cout << "Could not write to log file" << "\n";
    return 0;
  }
  *(SysLog) << "Writing to logfile @";
  SysLog->WriteSysTime();
  *(SysLog) << "\n";

  SysLog->Flush();
  
  cout << "Appending 10 entries to logfile.log" << "\n";
  for(int i = 0; i < 10; i++) {
    *(SysLog) << text << "Appending to logfile [" << i << "]" << "\n";
  }
  
  cout << "Appending two strings to logfile" << "\n";
  *(SysLog) << "Appending string " << 1 << " and " << 2 << "\n";
  
  cout << "Appending three strings to logfile" << "\n";
  *(SysLog) << "Appending string " << 1.5 << ' ' << 2.5 << " and " << 3.5
	    << "\n";
  
  cout << "\n";
  cout << "Printing the logfile to the console" << "\n";
  cout << "\n";

  char sbuf[255];
  SysLog->df_Rewind();
  while(!SysLog->df_EOF()) {
    if(SysLog->df_GetLine(sbuf, sizeof(sbuf), '\n', 1) !=
       DiskFileB::df_NO_ERROR) {
      break;
    }
    cout << sbuf << "\n";
  }

  delete SysLog;
  return 0;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
