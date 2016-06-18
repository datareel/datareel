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
 
Simple test program used to demonstrate how to exit from a 
thread without terminating the whole process.
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
const int THREADFAIL = 1;
const int THREADPASS = 0;
 
// Global variables
int err_condition = 0;

class SimpleThread : public gxThread
{
private: // Base class interface
  void *ThreadEntryRoutine(gxThread_t *thread);
};

void *SimpleThread::ThreadEntryRoutine(gxThread_t *thread)
{
  cout << "Executing thread" << "\n" << flush;  
  if(err_condition) {
    cout << "Reached error condition!" << "\n" << flush;
    ExitThread(thread, THREADFAIL);
  }

  cout << "Exiting the thread with no errors reported" << "\n" << flush;
  return ExitThread(thread, THREADPASS); 
}

void CheckExitCode(gxThread_t *thread)
{
  cout << "Checking the thread's status" << "\n" << flush;
  if(int(thread->GetThreadExitCode()) == THREADPASS) 
    cout << "The thread passed" << "\n" << flush;
  else if(int(thread->GetThreadExitCode()) == THREADFAIL)  
    cout << "The thread failed" << "\n" << flush;
  else 
    cout << "Invalid thread exit code reported" << "\n" << flush;

  cout << "\n" << flush;
}

int main()
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

  SimpleThread t;
 
  // Create and start a thread
  gxThread_t *thread = t.CreateThread();
 
  // Wait for the thread to complete, and release its resources
  t.JoinThread(thread);
  CheckExitCode(thread); 
  delete thread;

  // Simulate and error condition
  err_condition = 1;
  thread = t.CreateThread();
  t.JoinThread(thread);
  CheckExitCode(thread); 

  delete thread;
  return 0; // Exit the process
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //

 

