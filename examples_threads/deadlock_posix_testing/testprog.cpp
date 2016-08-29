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
 
Posix thread example for Linux testing dead lock detection using a 
robust mutex.
*/
// ----------------------------------------------------------- //   
#include "gxdlcode.h"

#include <errno.h>

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include "gxthread.h"

pthread_mutex_t mutex;

class SimpleThread : public gxThread
{
private: // Base class interface
  void *ThreadEntryRoutine(gxThread_t *thread);
};

int value = 0;

void *SimpleThread::ThreadEntryRoutine(gxThread_t *thread)
// Thread's entry function
{
  int tid = *((int *)thread->GetThreadParm());

  int is_locked = 0;
  int rv = pthread_mutex_lock(&mutex);
  if(rv == 0) is_locked = 1;
  if(rv != 0) {
    if(rv == EOWNERDEAD) {
      cout << "ERROR - In thread " << tid << " mutex owner is dead, taking ownership" << "\n" << flush;
      rv = pthread_mutex_consistent_np(&mutex); // We now own this lock
      if(rv != 0) { // Cannot recover lock
	pthread_mutex_unlock(&mutex); // Signal all locks to fail
	return 0;
      }
      is_locked = 1;
    }
  }

  if(!is_locked) {
    cout << "ERROR - not locked" << "\n" << flush;
    return 0;
  }

  if(tid == 2) return 0; // Simulate a deadlock by exiting thread without unlocking mutex

  // ********** Enter Critical Section ******************* //
  cout << "Thread " << tid << " accesing shared resource" << "\n" << flush;
  // ...
  value++;
  // ...
  cout << "Thread " << tid << " releasing shared resource" << "\n" << flush;
  // ********** Leave Critical Section ******************* //

  if(pthread_mutex_unlock(&mutex) != 0) {
    cout << "ERROR - Error unlocking mutex" << "\n" << flush;
  }

  return 0;
}

void runtest()
{
  const int NUM_THREADS = 5;
  SimpleThread t;
  gxThread_t *thread[NUM_THREADS];

  cout << "Create " << NUM_THREADS << " threads" << "\n" << flush;
  int i;
  for(i = 0; i < NUM_THREADS; i++) {
    int *tcount = new int;
    *tcount = i;
    thread[i] = t.CreateThread((void *)tcount); 
  }

  // Wait for the all the threads to finish before exiting
  //cout << "Waiting for all threads before exiting" << "\n" << flush;
  for(i = 0; i < NUM_THREADS; ++i) t.JoinThread(thread[i]);

  // Release the memory allocated for each thread
  for(i = 0; i < NUM_THREADS; ++i) {
    int *tcount = (int *)thread[i]->GetThreadParm();
    delete tcount; tcount = 0;
    delete thread[i]; thread[i] = 0;
  }

  cout << "Final value = " << value << "\n\n" << flush;
  value = 0;
}

int main(int argc, char **argv)
{
  pthread_mutexattr_t ma;
  pthread_mutexattr_init(&ma);
  // pthread_mutexattr_setpshared(&ma, PTHREAD_PROCESS_SHARED);
  pthread_mutexattr_setpshared(&ma, PTHREAD_PROCESS_PRIVATE);
  pthread_mutexattr_setrobust_np(&ma, PTHREAD_MUTEX_ROBUST_NP);
  pthread_mutex_init(&mutex, &ma);

  int count = 255;
  int i = 1;
  while(count--) {
    cout << "Test number " << i++ << "\n" << flush;
    runtest();
  }
  cout << "Exiting program" << "\n" << flush;
  return 0;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
  
