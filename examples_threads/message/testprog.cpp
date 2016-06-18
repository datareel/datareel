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
 
Simple test program demonstrating the gxThread status and error
message functions for gxThread_t types.
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

void PrintThreadStatus(gxThread_t *thread)
{
  cout << "Thread OID: " << (int)thread->GetObjectID() << "\n" << flush;
  cout << "Thread CID: " << (int)thread->GetClassID() << "\n" << flush;
  cout << thread->ThreadExceptionMessage() << "\n" << flush;
  cout << thread->ThreadPriorityMessage() << "\n" << flush;
  cout << thread->ThreadStateMessage() << "\n" << flush;
  cout << thread->ThreadTypeMessage() << "\n" << flush;
  if(thread->GetStackSize() > (gxStackSizeType)0)
    cout << "Stack size: " << (int)thread->GetStackSize() << "\n" << flush;
  if(thread->GetThreadExitCode() == (gxThreadExitCode)THREAD_EXIT_CODE)
    cout << "Exit code: " << (int)thread->GetThreadExitCode() << "\n" << flush;
}

class SimpleThread : public gxThread
{
public:
  SimpleThread() { cid = 1500; oid = 0; }
  ~SimpleThread() { }
  
private: // Base class interface
  void *ThreadEntryRoutine(gxThread_t *thread);
  void ThreadExitRoutine(gxThread_t *thread);

private:
  gxThreadObjectID oid;
  gxThreadClassID cid;
};

void *SimpleThread::ThreadEntryRoutine(gxThread_t *thread)
{
  thread->SetClassID(cid);
  thread->SetObjectID(oid++);
  PrintThreadStatus(thread);  
  sSleep(1);
  return (void *)THREAD_EXIT_CODE;
}

void SimpleThread::ThreadExitRoutine(gxThread_t *thread)
{
  cout << "\n" << flush;
  PrintThreadStatus(thread);  
  cout << "\n" << flush;
}

// Main thread of execution
int main()
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

  SimpleThread t;

  const int NUM_THREADS = 10;
  gxThread_t *thread[NUM_THREADS];

  int i;
  for(i = 0; i < NUM_THREADS; i++) {
    thread[i] = t.CreateThread();

    // Check for any errors created during thread creation
    if(thread[i]->GetThreadError() != 0) {
      cout << thread[i]->ThreadExceptionMessage() << "\n" << flush;
    }

    t.sSleep(3);
  }

  // Wait for all the threads to finish before exiting
  for(i = 0; i < NUM_THREADS; i++) t.JoinThread(thread[i]);
  

  // Cleanup all the gxThread_t pointers
  for(i = 0; i < NUM_THREADS; i++) {
    int rv = t.DestroyThread(thread[i]);
    if(rv != 0) {
      cout << "Error destroying thread!" << "\n" << flush;
    }
  }

  return 0; // Exit the process
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //


  

