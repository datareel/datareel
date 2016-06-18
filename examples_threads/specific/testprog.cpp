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

Test program demonstrating how to create thread-specific data. 
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

// Constants
const int NUM_THREADS = 2;
 
// Thread specific data used in this process
struct threadSpecific_data_t
{
  threadSpecific_data_t(int i, int j) {
    threadSpecific1 = i; threadSpecific2 = j; 
  }
  
  int threadSpecific1;
  int threadSpecific2;
};

// Thread key
gxThreadKey threadSpecificKey;

class SimpleThread : public gxThread
{
public:
  SimpleThread () { oid = 1; }
  
private: // Base class interface
  void *ThreadEntryRoutine(gxThread_t *thread);
  void ThreadExitRoutine(gxThread_t *thread);
  
private: // Functions that use the threadSpecific data
  void foo(gxThread_t *thread);  
  void bar(gxThread_t *thread);

private:
  gxThreadObjectID oid;
};

void *SimpleThread::ThreadEntryRoutine(gxThread_t *thread)
// Thread's entry function
{
  thread->SetObjectID(oid++);
  cout << "Entering thread: " << thread->GetObjectID() << "\n" << flush;
  
  threadSpecific_data_t *gData = \
    (threadSpecific_data_t *)thread->GetThreadParm();

  if(ThreadSetSpecific(threadSpecificKey, gData) != 0) {
    cout << "Thread local storage error" << "\n" << flush;
    return ExitThread(thread, 1);
  }
  
  foo(thread);
  return 0;
}
 
void SimpleThread::foo(gxThread_t *thread)
{
  threadSpecific_data_t *gData = \
    (threadSpecific_data_t *)ThreadGetSpecific(threadSpecificKey);

  cout << "Thread: " << thread->GetObjectID() << " entering foo(), ";
  cout << "threadSpecific data = " << gData->threadSpecific1 << ' '
       << gData->threadSpecific2
       << "\n" << flush;

  bar(thread);
}
 
void SimpleThread::bar(gxThread_t *thread)
{
  threadSpecific_data_t *gData = \
    (threadSpecific_data_t *)ThreadGetSpecific(threadSpecificKey);
  
  cout << "Thread: " << thread->GetObjectID() << " entering bar(), ";
  cout << "threadSpecific data = " << gData->threadSpecific1 << ' '
       << gData->threadSpecific2
       << "\n" << flush;
}
 
void SimpleThread::ThreadExitRoutine(gxThread_t *thread)
{
  cout << "Thread: " << thread->GetObjectID() << " exiting" << "\n" << flush;

  void *parm = thread->GetThreadParm();

  // Free the local storage resources
  delete parm;
}
 
int main()
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

  SimpleThread t;
  gxThread_t *thread[NUM_THREADS];

  if(t.ThreadKeyCreate(threadSpecificKey) != 0) {
    cout << "Thread local storage error!" << "\n" << flush;
    cout << "Could not obtain and index." << "\n" << flush;
    return 1;
  }

  cout << "Creating " << NUM_THREADS << " threads" << "\n" << flush;
  int i;
  for(i = 0; i < NUM_THREADS; i++) {
    // Create per-thread threadSpecific data and pass it to the thread
    threadSpecific_data_t *gData = new threadSpecific_data_t(i, ((i+1)*2));
    thread[i] = t.CreateThread((void *)gData);
    t.sSleep(1); // Allow the thread to print their messages in order
  }
 
  cout << "Wait for the threads to complete, and release their resources"
       << "\n" << flush;
  for(i = 0; i < NUM_THREADS; i++) t.JoinThread(thread[i]);

  // Release the index
  t.ThreadKeyDelete(threadSpecificKey);

  // Prevent memory leaks
  for(i = 0; i < NUM_THREADS; i++) delete thread[i];
  return 0;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
