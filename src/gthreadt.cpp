// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gthreadt.cpp
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

The gxThread_t class is a data structure used to initialize and
store thread variables prior to and after thread creation.
*/
// ----------------------------------------------------------- //   
#include "gxdlcode.h"

#include "gthreadt.h"
#include "gxthread.h"
#include "gxmutex.h"
#include "gxcond.h"

// --------------------------------------------------------------
// Constants used for thread status and debug functions
// --------------------------------------------------------------
// NOTE: This array must contain the same number of exceptions as the
// gxThreadState enumeration. 
const int gxMaxThreadStateMessages = 7;
const char *gxThreadStateMessages[gxMaxThreadStateMessages] = {
  "Thread state: INVALID",  // gxTHREAD_STATE_INVALID
  "Thread state: CANCELED", // gxTHREAD_STATE_CANCELED
  "Thread state: EXITED",   // gxTHREAD_STATE_EXITED
  "Thread state: NEW",      // gxTHREAD_STATE_NEW
  "Thread state: RUNNING",  // gxTHREAD_STATE_RUNNING
  "Thread state: SUSPENED", // gxTHREAD_STATE_SUSPENDED 
  "Thread state: WAITING"   // gxTHREAD_STATE_WAITING
};

// NOTE: This array must contain the same number of exceptions as the
// gxThreadPriority enumeration. 
const int gxMaxThreadPriorityMessages = 4;
const char *gxThreadPriorityMessages[gxMaxThreadPriorityMessages] = {
  "Thread priority: INVALID",
  "Thread priority: LOW",
  "Thread priority: NORMAL",
  "Thread priority: HIGH"
};

// NOTE: This array must contain the same number of exceptions as the
// gxThreadPriorityClass enumeration. 
const int gxMaxThreadPriorityClassMessages = 4;
const char *gxThreadPriorityClassMessages[gxMaxThreadPriorityClassMessages] = {
  "Thread priority class: INVALID",
  "Thread priority class: OTHER",    
  "Thread priority class: FIFO",
  "Thread priority class: ROUND ROBIN"
};

// NOTE: This array must contain the same number of exceptions as the
// gxThreadError enumeration.
const int gxMaxThreadExceptionMessages = 15;
const char *gxThreadExceptionMessages[gxMaxThreadExceptionMessages] = {
  "Thread exception: No exception reported",             // NO_ERROR
  "Thread exception: Invalid exception code",            // INVALID_CODE 
  "Thread exception: Error canceling thread",            // CANCEL_ERROR
  "Thread exception: Error closing thread",              // CLOSE_ERROR 
  "Thread exception: Error creating thread",             // CREATE_ERROR
  "Thread exception: Thread is already running",         // RUNNING_ERROR
  "Thread exception: Thread cannot be executed",         // EXECUTE_ERROR 
  "Thread exception: Error setting scheduling policy",   // POLICY_ERROR 
  "Thread exception: Error setting priority",            // PRIORITY_ERROR
  "Thread exception: Error resuming thread",             // RESUME_ERROR
  "Thread exception: Scheduling error",                  // SCHED_ERROR
  "Thread exception: Contention scope error",            // SCOPE_ERROR
  "Thread exception: Error setting the stack size",      // STACK_ERROR
  "Thread exception: Could not set the thread state",    // STATE_ERROR
  "Thread exception: Error suspending thread"            // SUSPEND_ERROR
};

// NOTE: This array must contain the same number of exceptions as the
// gxThreadType enumeration.
const int gxMaxThreadTypeMessages = 2;
const char *gxThreadTypeMessages[gxMaxThreadTypeMessages] = {
  "Thread type: DETACHED", // gxTHREAD_TYPE_DETACHED,
  "Thread type: JOINABLE"  // gxTHREAD_TYPE_JOINABLE
};
// --------------------------------------------------------------

gxThread_t::gxThread_t(gxThreadType t)
{
  // Use the API's default stack size. NOTE: The WIN32 Interface
  // will used the will use the same the stack size specified for
  // the main thread when the stack_size value is set to zero.
  stack_size = 0; 

  thread_type = t;
  thread_error = gxTHREAD_NO_ERROR;
  thread_state = gxTHREAD_STATE_NEW;
  thread_priority = gxTHREAD_PRIORITY_NORMAL;
  thread_priority_class = gxTHREAD_PRIORITY_CLASS_OTHER;
  
  // Set the thread exit value to zero initially. This value will
  // be set when the thread's entry function returns.
  thread_exit_code = (gxThreadExitCode)0;

  // NOTE: The thread ID will be set when the thread is created.
  // Not all UNIX variants treat the threads IDs as an integer type.
  // PC-lint 05/28/2002: Lint warns that variable is not initialized
  // thread_id = 0; 

  // PC-lint 09/08/2005: Lint warns that thread_attribute variable 
  // is not initialized. The thread_attribute is initialized when the
  // thread is created by the attribute API function in thrapiw.cpp  

  // Initialize the gxThread base class pointer. NOTE: This
  // pointer will be set by the gxThread class when a gxThread_t
  // object is passed to any of the gxThread::CreateThread()
  // functions.
  entry = 0;

  // Initialize the additional thread parameter pointer. NOTE: This
  // value is an optional and should only be accessed if was thread
  // parameter is being used with this thread.
  thread_parm = 0;
  
  // Set the class ID and object ID to -1 initially. These values are
  // optional and must be set by the class that create the thread. A
  // value of -1 indicates that this value has not been set.
  thread_oid = gxThreadObjectID(-1); 
  thread_cid = gxThreadClassID(-1); 

  suspend_lock_t = new gxMutex; // Lock used to serialize access to suspend_t
  suspend_t = new gxCondition;  // Thread suspend condition
}

gxThread_t::~gxThread_t()
{
  // PC-lint 09/08/2005: Function may throw exception in destructor
  if(suspend_lock_t) delete suspend_lock_t;
  if(suspend_t) delete suspend_t;
}

const char *gxThread_t::ThreadExceptionMessage()
// Returns a null terminated string that can
// be used to log or print a thread exception.
{
  int error = (int)thread_error;
  if(error > gxMaxThreadExceptionMessages) error = gxTHREAD_INVALID_CODE;
  // Find the corresponding message in the exception array
  return gxThreadExceptionMessages[error];
}

const char *gxThread_t::ThreadTypeMessage()
// Returns a null terminated string that can
// be used to log or print the thread type.
{
  int type = (int)thread_type;
  if(type > gxMaxThreadTypeMessages) type = gxTHREAD_TYPE_JOINABLE;
  return gxThreadTypeMessages[type];
}

const char *gxThread_t::ThreadPriorityMessage()
// Returns a null terminated string that can
// be used to log or print the thread's priority.
{
  // The use of a switch case statement allows values to be assigned
  // to the priority members of the gxThreadPriority enumeration.
  switch(thread_priority) {
    case gxTHREAD_PRIORITY_LOW:
      return gxThreadPriorityMessages[1];
    case gxTHREAD_PRIORITY_NORMAL:
      return gxThreadPriorityMessages[2];
    case gxTHREAD_PRIORITY_HIGH:
      return gxThreadPriorityMessages[3];
    default:
      break; // Unknown priority
  }
  return gxThreadPriorityMessages[0];
}

const char *gxThread_t::ThreadPriorityClassMessage()
// Returns a null terminated string that can
// be used to log or print the thread's priority class.
{
  int type = (int)thread_priority_class;
  if(type > gxMaxThreadPriorityClassMessages)
    type = gxTHREAD_PRIORITY_CLASS_INVALID;
  return gxThreadPriorityClassMessages[type];
}

const char *gxThread_t::ThreadStateMessage()
// Returns a null terminated string that can
// be used to log or print the thread's state.
{
  int state = (int)thread_state;
  if(state > gxMaxThreadStateMessages) state = gxTHREAD_STATE_INVALID;
  return gxThreadStateMessages[state];
}

GXDLCODE_API int operator==(const gxThread_t &a, const gxThread_t &b)
// Compare the thread IDs gxThread_t "a" and "b". Returns
// true if the IDs are equal. This function was added
// for UNIX variants that do not use an integer type for
// thread IDs.
{
#if defined (__WIN32__)
  return a.thread_id == b.thread_id;
#elif defined (__POSIX__)
  // NOTE: Undefined behavior will result If both gxThread_t
  // objects do not contain valid thread IDs.
  if(pthread_equal(a.thread_id, b.thread_id)) return 1;
  return 0; // The thread IDs are not equal
#else 
#error You must define a native API: __WIN32__ or __UNIX__
#endif
}

GXDLCODE_API int operator!=(const gxThread_t &a, const gxThread_t &b)
// Compare the thread IDs gxThread_t "a" and "b". Returns
// true if the IDs are not equal. This function was 
// added for UNIX variants that do not use an integer type
// for thread IDs.
{
#if defined (__WIN32__)
  return a.thread_id != b.thread_id;
#elif defined (__POSIX__)
  // NOTE: Undefined behavior will result If both gxThread_t
  // objects do not contain valid thread IDs.
  if(pthread_equal(a.thread_id, b.thread_id) != 0) return 1;
  return 0; // The thread IDs not equal
#else 
#error You must define a native API: __WIN32__ or __UNIX__
#endif
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
