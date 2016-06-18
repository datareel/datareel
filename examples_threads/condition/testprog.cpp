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
gxThread class using condition variables.
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
#include "gxcond.h"
#include "gxmutex.h"

#ifdef __MSVC_DEBUG__
#include "leaktest.h"
#endif

// For safe condition variable usage, must use a Boolean predicate and 
// a mutex with the condition.
int is_locked = 0;
gxCondition cond;
gxMutex mutex;

class SimpleThread : public gxThread
{
private: // Base class interface
  void *ThreadEntryRoutine(gxThread_t *thread);
};

void *SimpleThread::ThreadEntryRoutine(gxThread_t *thread)
// Thread's entry function
{
  int num = *((int *)thread->GetThreadParm());

  // Serialize access to the shared resource.
  mutex.MutexLock();
  
  int num_try = 0;
  while(is_locked != 0) {
    if(++num_try < 255) {
      cout << "Thread " << num << " execution blocked..." << "\n" << flush;
      cond.ConditionWait(&mutex);
    }
    else {
      cout << "Thread " << num << " could not wait for condition" 
	   << "\n" << flush;
      mutex.MutexUnlock();
      return 0;
    }
  }

  // Tell other threads to wait until shared resource is available
  is_locked = 1;

  // ********** Enter Critical Section ******************* //
  cout << "Thread " << num << " accesing shared resource" << "\n" << flush;
  // ...
  // ...
  // ...
  cout << "Thread " << num << " releasing shared resource" << "\n" << flush;
  // ********** Leave Critical Section ******************* //

  // Tell other threads shared resource is available
  is_locked = 0;

  // Wake up the next thread waiting on this condition
  cond.ConditionSignal();
  mutex.MutexUnlock();
  
  return 0;
}

int main(int argc, char **argv)
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

  const int NUM_THREADS = 5;
  SimpleThread t;
  gxThread_t *thread[NUM_THREADS];

  cout << "Create " << NUM_THREADS << " threads" << "\n" << flush;
  int i;
  for(i = 0; i < NUM_THREADS; i++) thread[i] = t.CreateThread((void *)&i); 

  // Wait for the all the threads to finish before exiting
  cout << "Waiting for all threads before exiting" << "\n" << flush;
  for(i = 0; i < NUM_THREADS; ++i) t.JoinThread(thread[i]);

  // Release the memory allocated for each thread
  for(i = 0; i < NUM_THREADS; ++i) delete thread[i];

  cout << "Exiting program" << "\n" << flush;
  return 0;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //


  

