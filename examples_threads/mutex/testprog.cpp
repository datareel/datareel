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
 
gxMutex example program demonstrating how to protect access to
shared data.
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
#include "gxmutex.h"

#ifdef __MSVC_DEBUG__
#include "leaktest.h"
#endif

// Constants
const int NUMTHREADS = 3;

// Mutex that will be shared by threads within this process
gxMutex mutex;

// Shared data
int sharedData1 = 0;
int sharedData2 = 0;

class SimpleThread : public gxThread
{
public:
  SimpleThread() { }
  ~SimpleThread();
  
public:
  void Begin();
  void End();
  
private: // Base class interface
  void *ThreadEntryRoutine(gxThread_t *thread);

public:
  gxThread_t *t[NUMTHREADS];
};

void *SimpleThread::ThreadEntryRoutine(gxThread_t *thread)
{
  cout << "Entering thread" << "\n" << flush;
  mutex.MutexLock();
  
  //********** Critical Section *******************//
  cout << "Start critical section, holding lock" << "\n" << flush;

  // Access to shared data goes here 
  ++sharedData1; --sharedData2;
  cout << "sharedData1 = " << sharedData1 << ", sharedData2 = " << sharedData2
       << "\n" << flush;
  
  cout << "End critical section, release lock" << "\n" << flush;
  //********** Critical Section *******************//

  mutex.MutexUnlock();
  return 0;
}

void SimpleThread::Begin()
{
  for(int i = 0; i < NUMTHREADS; ++i) t[i] = CreateThread();
}

void SimpleThread::End()
{
  for(int i = 0; i < NUMTHREADS; ++i) JoinThread(t[i]);
}

SimpleThread::~SimpleThread()
{
  for(int i = 0; i < NUMTHREADS; ++i) {
    if(t[i]) delete t[i];
  }
}

int main()
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

  SimpleThread t;
  
  cout << "Hold Mutex to prevent access to shared data" << "\n" << flush;
  cout << "sharedData1 = " << sharedData1 << ", sharedData2 = " << sharedData2
       << "\n" << flush;
  
  mutex.MutexLock();
 
  cout << "Create/start threads" << "\n" << flush;
  t.Begin();
  
  cout << "Wait a until we are done with the shared data" << "\n" << flush;
  t.sSleep(3);

  cout << "Unlock shared data" << "\n" << flush;
  mutex.MutexUnlock();

  cout << "Wait for the threads to complete, and release their resources"
       << "\n" << flush;
  t.End();
  
  return 0; // Exit the process
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //

 
