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

Test program demonstrating the basic operation of the gxThread
pool class.
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

// Turn console message on or off before the thread are created
int display_message = 1;

void PausePrg()
// Pause the program until enter is pressed
{
  cout << "\n" << flush;
  cout << "Press Enter to continue..." << "\n" << flush;
  cin.get();
}

class SimpleThread : public gxThread
{
private: // Base class interface
  void *ThreadEntryRoutine(gxThread_t *thread);
};

void *SimpleThread::ThreadEntryRoutine(gxThread_t *thread)
// Thread's entry function
{
  if(display_message) {
    cout << "Executing worker thread: " << (long)thread->GetThreadParm() 
	 << "\n" << flush;
  }

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

  unsigned i;
  for(i = 0; i < NUM_WORKERS; i++) {
    t->CreateThread(pool, (void *)i);
    t->sSleep(1);
  }

  t->JoinThread(pool);

  cout << "\n" << flush;
  cout << "Destroying the thread pool..." << "\n" << flush;
  t->DestroyThreadPool(pool);

  cout << "Creating anther pool of " << NUM_WORKERS << " threads" 
       << "\n" << flush;
  PausePrg();

  display_message = 0; // Do not display any console messages
  pool = t->CreateThreadPool(NUM_WORKERS);

  t->sSleep(1);
  cout << "\n" << flush;
  cout << "Rebuilding the pool" << "\n" << flush;
  t->RebuildThreadPool(pool);

  // Wait for the treads to finish before destroying the pool
  t->JoinThread(pool);

  cout << "\n" << flush;
  cout << "Destroying the thread pool..." << "\n" << flush;
  if(!pool->IsEmpty()) {
    t->DestroyThreadPool(pool);
  }
  else {
    cout << "The pool is empty" << "\n" << flush;
    // If DestroyThreadPool() is not called the pool will not be deleted
    delete pool; 
  }
  
  cout << "Exiting..." << "\n" << flush;
  delete t;

  return 0; // Exit the process, terminating all threads
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //


  

