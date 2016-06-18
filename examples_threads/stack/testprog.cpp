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
 
Test program demonstrating how change to a thread's stack size.
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

class SimpleThread : public gxThread
{
private: // Base class interface
  void *ThreadEntryRoutine(gxThread_t *thread);
};

void *SimpleThread::ThreadEntryRoutine(gxThread_t *thread)
// Thread's entry function
{
  cout << "Executing thread: " << (long)thread->GetThreadParm()
       << ", Stack Size: " << thread->GetStackSize() << "\n"
       << flush;
    
  return 0;
}

int main()
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

  const unsigned NUM_THREADS = 10;

  gxStackSizeType ssize = 4096;
  gxThreadType type = gxTHREAD_TYPE_JOINABLE;

#if defined (__POSIX__)
  ssize *= 5; // Min size under POSIX is 16K
#endif

  SimpleThread *t = new SimpleThread;
  gxThread_t *thread[NUM_THREADS];
  
  unsigned i;
  cout << "Creating " << NUM_THREADS << " threads" << "\n" << flush;
  cout << "New stack size per thread = " << ssize << "\n" << flush;

  for(i = 0; i < NUM_THREADS; i++) {
    thread[i] = t->CreateThread((void *)i, type, ssize);

    // Check for any errors
    if(thread[i]->GetThreadError() != gxTHREAD_NO_ERROR)
      cout << thread[i]->ThreadExceptionMessage() << "\n" << flush;
    
    // Allow the threads to print their messages in order
    t->sSleep(1);
  }

  // Wait for the all treads to finish
  for(i = 0; i < NUM_THREADS; i++) t->JoinThread(thread[i]);

  cout << "\n" << flush;
  cout << "Destroying all the threads..." << "\n" << flush;
  for(i = 0; i < NUM_THREADS; i++) t->DestroyThread(thread[i]);

  cout << "Exiting..." << "\n" << flush;

  delete t;
  return 0; // Exit the process, terminating all threads
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //


  

