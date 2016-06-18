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
 
Simple test program demonstrating how to join a gxThread.
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

// Constants
const int THREAD_EXIT_CODE = 15; // Arbitrary exit code value

// Global variables
int thread_count = 0;

class SimpleThread : public gxThread
{
public:
  SimpleThread() { }
  ~SimpleThread() { }
  
private: // Base class interface
  void *ThreadEntryRoutine(gxThread_t *thread);
};

void *SimpleThread::ThreadEntryRoutine(gxThread_t *thread)
{
  cout << "Excuting thread: " << thread_count++ << "\n" << flush;
  return (void *)THREAD_EXIT_CODE;
}

// Main thread of execution
int main()
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

  SimpleThread t;

  const int num_threads = 10;
  gxThread_t *thread[num_threads];

  int i;
  for(i = 0; i < num_threads; i++) {
    thread[i] = t.CreateThread();
    t.sSleep(1);
  }

  // Wait for all the threads to finish before exiting
  for(i = 0; i < num_threads; ++i) {
    if(t.JoinThread(thread[i]) != 0)
      cout << "Could not join the thread" << "\n";
    if(thread[i]->GetThreadExitCode() != THREAD_EXIT_CODE)
      cout << "This thread exited with an error" << "\n";
  }

  // Prevent memory leaks
  for(i = 0; i < num_threads; ++i) delete thread[i];
  
  return 0; // Exit the process
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //


  

