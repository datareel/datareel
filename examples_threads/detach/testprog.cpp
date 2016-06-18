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

Test program demonstrating how to change the detachstate attribute
of a newly created thread. You cannot use the thread ID (the value
of type gxThread_t::thread_is) for a detached thread. This means
that you cannot cancel the thread or use the gxThread::JoinThread()
function to to wait for the thread to complete or to retrieve the
thread's exit status. The system will reclaim a detached thread's
resources when the detach thread exits. 
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
  cout << "Executing thread: " << (long)thread->GetThreadParm() << ", "
       << thread->ThreadTypeMessage() << '\n' << flush;
    
  return 0;
}

int main()
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

  const unsigned NUM_THREADS = 10;

  gxThreadType type = gxTHREAD_TYPE_DETACHED;
  
  SimpleThread *t = new SimpleThread;
  gxThread_t *thread[NUM_THREADS];
  
  unsigned i;
  cout << "Creating " << NUM_THREADS << " threads" << "\n" << flush;
  for(i = 0; i < NUM_THREADS; i++) {
    thread[i] = t->CreateThread((void *)i, type);

    // Check for any errors
    if(thread[i]->GetThreadError() != gxTHREAD_NO_ERROR)
      cout << thread[i]->ThreadExceptionMessage() << "\n" << flush;
    
    // Allow the threads to print there messages in order
    t->sSleep(1);
  }

  cout << "Waiting for the all threads to finish..." << "\n" << flush;
  t->sSleep(3);

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


  

