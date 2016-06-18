// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: testprog.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 03/25/2000
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
 
Simple test program demonstrating the basic operation of the
gxThread class using timing delays to perform synchronization.
*/
// ----------------------------------------------------------- //   
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include <stdlib.h>
#include "gxthread.h"

#ifdef __MSVC_DEBUG__
#include "leaktest.h"
#endif

class SimpleThread : public gxThread
{
public:
  SimpleThread() { }
  ~SimpleThread() { }
  
public:
  char *message;
  
private: // Base class interface
  void *ThreadEntryRoutine(gxThread_t *thread);
};

void *SimpleThread::ThreadEntryRoutine(gxThread_t *thread)
{
  cout << "\n" << flush;
  cout << message << "\n" << flush;
  return 0;
}

int main()
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

  SimpleThread t;
  char *message1 = "Test message 1";
  char *message2 = "Test message 2";

  t.message = message1;
  gxThread_t *thread = t.CreateThread();

  t.sSleep(1); // Wait for the first thread to exit
  delete thread; // Prevent memory leak
  
  t.message = message2;
  thread = t.CreateThread();
  t.sSleep(1); // Wait for the second thread to exit
  delete thread;

  cout << "\n" << flush;
  cout << "Testing all the reentrant gxThread sleep functions..."
       << "\n" << flush;

  cout << "Sleeping for 1 second" << "\n" << flush;
  t.sSleep(1);

  cout << "Sleeping for 1000 milliseconds" << "\n" << flush;
  t.mSleep(1000);

  cout << "Sleeping for 1000000 microseconds" << "\n" << flush;
  t.uSleep(1000000);

  cout << "Sleeping for 100 microseconds" << "\n" << flush;
  t.uSleep(100);

  cout << "Sleeping for 1000000000 nanoseconds" << "\n" << flush;
  t.nSleep(1000000000);

  cout << "Sleeping for 100 nanoseconds" << "\n" << flush;
  t.nSleep(100);

  return 0; // Exit the process
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //


  

