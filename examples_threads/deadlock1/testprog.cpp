// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: testprog.cpp
// Compiler Used: GNU
// Produced By: DataReel Software Development Team
// File Creation Date: 03/25/2000
// Date Last Modified: 08/28/2016
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
 
Test program demonstrating a dead lock where the gxMutex class
will attempt to automatically recover from the dead lock.

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

// Boolean predicate and a mutex
int is_locked = 0; 
gxMutex mutex;

int value = 0;

class SimpleThread : public gxThread
{
private: // Base class interface
  void *ThreadEntryRoutine(gxThread_t *thread);
};

void *SimpleThread::ThreadEntryRoutine(gxThread_t *thread)
// Thread's entry function
{
  int num = *((int *)thread->GetThreadParm());

  int has_mutex = 1;

  if(mutex.MutexLock() != 0) { 
    cout << gxThreadMutexExceptionMessage(mutex.GetMutex()) << "\n" << flush;
    has_mutex = 0;
  }

  if(!has_mutex) cout << "Lock failed for " << num << "\n";

  // NOTE: This is application dependant, for more complex operations we need to add
  // NOTE: a condition here, fail, or exit the program. In this example we can continue
  // NOTE: if no other thread is accessing the critical data and we cannot get a mutex
  if(is_locked && !has_mutex) return 0;

  if(num == 3) return 0; // Simulate a deadlock by exiting before unlocking

  // Tell other threads to wait until shared resource is available
  is_locked = 1;

  if(num == 4 || num == 6) return 0; // Simulate a deadlock by exiting before unlocking

  // ********** Enter Critical Section ******************* //
  cout << "Thread " << num << " accesing shared resource" << "\n" << flush;
  // ...
  value++;
  // ...
  cout << "Thread " << num << " releasing shared resource" << "\n" << flush;
  // ********** Leave Critical Section ******************* //

  // Tell other threads shared resource is available
  is_locked = 0;

  if(has_mutex) {
    if(mutex.MutexUnlock() != 0) {
      cout << gxThreadMutexExceptionMessage(mutex.GetMutex()) << "\n" << flush;
    }
  }

  return 0;
}

int main(int argc, char **argv)
{
  const int NUM_THREADS = 10;
  SimpleThread t;
  gxThread_t *thread[NUM_THREADS];

  cout << "\nCreate " << NUM_THREADS << " threads" << "\n" << flush;
  int i;
  for(i = 0; i < NUM_THREADS; i++) {
    int *tcount = new int;
    *tcount = i;
    thread[i] = t.CreateThread((void *)tcount); 
  }

  // Wait for the all the threads to finish before exiting
  for(i = 0; i < NUM_THREADS; ++i) t.JoinThread(thread[i]);

  // Release the memory allocated for each thread
  for(i = 0; i < NUM_THREADS; ++i) {
    int *tcount = (int *)thread[i]->GetThreadParm();
    if(tcount) { delete tcount; tcount = 0; }
    delete thread[i]; thread[i] = 0;
  }

  cout << "Final value = " << value << "\n\n" << flush;
  return 0;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //


  

