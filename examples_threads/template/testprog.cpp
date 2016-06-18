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
gxThread class using templates.
*/
// ----------------------------------------------------------- //   
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include "gxthread.h"

#ifdef __MSVC_DEBUG__
#include "leaktest.h"
#endif

template <class TYPE>
class SimpleThread : public gxThread
{
private: // Base class interface
  void *ThreadEntryRoutine(gxThread_t *thread);
};

template <class TYPE>
void *SimpleThread<TYPE>::ThreadEntryRoutine(gxThread_t *thread)
// Thread's entry function
{
  cout << "\n" << flush;
  TYPE info = *((TYPE *)thread->GetThreadParm());
  cout << "Executing thread: " << info  << "\n" << flush;
  return 0;
}

// Program's main thread of execution
int main()
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif
  
  SimpleThread<int> t1;
  SimpleThread<char> t2;
  SimpleThread<char *> t3;

  int i = 5;
  // Create and execute a thread
  gxThread_t *thread = t1.CreateThread((void *)&i); 
  t1.sSleep(1);  // Wait for the thread to exit
  delete thread; // Prevent memory leak 
  
  char c = 'A';
  thread = t2.CreateThread((void *)&c); 
  t2.sSleep(1); 
  delete thread;
  
  char *s = "TEST";
  thread = t3.CreateThread((void *)&s); 
  t3.sSleep(1); 
  delete thread;
  
  return 0; // Exit the process, terminating all threads
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //


  

