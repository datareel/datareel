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
 
Test program demonstrating how to set thread priorities using the
within a thread pool.
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
  cout << "Executing worker thread: " << (long)thread->GetThreadParm() << ", "
       << thread->ThreadPriorityMessage() << "\n" << flush;
  return 0;
}

int main()
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

  const unsigned NUM_WORKERS = 10;

  SimpleThread *t = new SimpleThread;
  thrPool *pool = new thrPool;
  gxThread_t *thread[NUM_WORKERS];
  
  unsigned i;
  // Construct some new threads without executing them
  for(i = 0; i < NUM_WORKERS; i++) thread[i] = t->ConstructThread((void *)i);

  // Change some of the thread priorities before placing them in the pool
  thread[NUM_WORKERS-1]->SetThreadPriority(gxTHREAD_PRIORITY_HIGH);
  thread[NUM_WORKERS-2]->SetThreadPriority(gxTHREAD_PRIORITY_HIGH);
  thread[NUM_WORKERS-3]->SetThreadPriority(gxTHREAD_PRIORITY_HIGH);

  thread[0]->SetThreadPriority(gxTHREAD_PRIORITY_LOW);
  thread[1]->SetThreadPriority(gxTHREAD_PRIORITY_LOW);
  thread[2]->SetThreadPriority(gxTHREAD_PRIORITY_LOW);

  // Put the threads in the thread pool
  for(i = 0; i < NUM_WORKERS; i++) pool->AddThread(thread[i]);

  // Execute all the threads in the pool
  thrPoolNode *ptr = pool->GetHead();
  while(ptr) {
    t->CreateThread(ptr->GetThreadPtr());
    t->sSleep(1);
    ptr = ptr->GetNext();
  }

  // Wait for the treads to finish before destroying the pool
  t->JoinThread(pool);

  cout << "\n" << flush;
  cout << "Destroying the thread pool..." << "\n" << flush;
  t->DestroyThreadPool(pool);

  cout << "Exiting..." << "\n" << flush;

  delete t;
  return 0; // Exit the process, terminating all threads
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //


  

