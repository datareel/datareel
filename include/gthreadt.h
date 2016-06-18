// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gthreadt.h
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 03/25/2000
// Date Last Modified: 06/17/2016
// Copyright (c) 2001-2016 DataReel Software Development
// ----------------------------------------------------------- // 
// ---------- Include File Description and Details  ---------- // 
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
#ifndef __GX_THREAD_THREAD_CONTAINER__
#define __GX_THREAD_THREAD_CONTAINER__

#include "gxdlcode.h"

#include "thrtypes.h"

// Foward class calling conventions
class gxThread;           // Base class used to create threaded objects
class gxThreadAPIWrapper; // Class used to handle native API calls
class gxMutex;            // Mutual exclusion class 
class gxCondition;        // Class used to handle condition variables

class GXDLCODE_API gxThread_t
{
public:
  gxThread_t(gxThreadType t = gxTHREAD_TYPE_JOINABLE);
  ~gxThread_t();

private: // Disallow coping and assignment
  gxThread_t(const gxThread_t &ob) { } 
  void operator=(const gxThread_t &ob) { }
  
public: // Thread status functions
  gxThreadState GetThreadState() { return thread_state; }
  gxThreadState GetThreadState() const { return thread_state; }
  gxThreadID GetThreadID() { return thread_id; }
  gxThreadID GetThreadID() const { return thread_id; }
  gxThreadType GetThreadType() { return thread_type; }
  gxThreadType GetThreadType() const { return thread_type; }
  gxStackSizeType GetStackSize() { return stack_size; }
  gxStackSizeType GetStackSize() const { return stack_size; }
  gxThreadAttribute *GetThreadAttribute() { return &thread_attribute; }
  
public: // Thread status/debug message functions
  const char *ThreadExceptionMessage();
  const char *ThreadPriorityMessage();
  const char *ThreadPriorityClassMessage();
  const char *ThreadStateMessage();
  const char *ThreadTypeMessage();
  
public: // Thread initialization functions
        // NOTE: These values must be set before the thread is created
  void SetThreadStackSize(unsigned ssize) { stack_size = ssize; }
  void SetThreadType(gxThreadType t) { thread_type = t; }

public: // Thread priority functions
  void SetThreadPriority(gxThreadPriority p) { thread_priority = p; }
  gxThreadPriority GetThreadPriority() { return thread_priority; }
  gxThreadPriority GetThreadPriority() const { return thread_priority; }
  gxThreadPriorityClass GetThreadPriorityClass() {
    return thread_priority_class;
  }
  gxThreadPriorityClass GetThreadPriorityClass() const {
    return thread_priority_class;
  }
  void SetThreadPriorityClass(gxThreadPriorityClass prio_class) {
    thread_priority_class = prio_class;
  }

public: // Thread error functions
  gxThreadError GetThreadError() { return thread_error; }
  gxThreadError GetThreadError() const { return thread_error; }
  void ResetThreadError() { thread_error = gxTHREAD_NO_ERROR; }
  void ResetThreadExitCode() { thread_exit_code = (gxThreadExitCode)0; }
  gxThreadExitCode GetThreadExitCode() { return thread_exit_code; }
  gxThreadExitCode GetThreadExitCode() const { return thread_exit_code; }

public: // Thread parameter functions
  void *GetThreadParm() { return thread_parm; }
  void SetThreadParm(void *v) { thread_parm = v; }

public: // Class ID/Object ID functions
  gxThreadClassID GetClassID() { return thread_cid; }
  gxThreadClassID GetClassID() const { return thread_cid; }
  gxThreadObjectID GetObjectID() { return thread_oid; }
  gxThreadObjectID GetObjectID() const { return thread_oid; }
  void SetClassID(gxThreadClassID cid) { thread_cid = cid; }
  void SetObjectID(gxThreadObjectID oid) { thread_oid = oid; }

public: // Overloaded operators
  GXDLCODE_API friend int operator==(const gxThread_t &a, const gxThread_t &b);
  GXDLCODE_API friend int operator!=(const gxThread_t &a, const gxThread_t &b); 
  
private: // gxThread base class interface
  friend class gxThread;
  gxThread *entry; // Pointer to this thread's entry function

private: // Interface to API wrapper class
  friend class gxThreadAPIWrapper;

private: // Thread variables
  gxStackSizeType stack_size;                  // Thread stack size
  gxThreadPriority thread_priority;            // Thread priority
  gxThreadPriorityClass thread_priority_class; // Thread scheduling policy 
  gxThreadState thread_state;                  // Thread state
  gxThreadID thread_id;                        // Thread ID or handle 
  gxThreadAttribute thread_attribute;          // Thread attribute

  gxThreadType thread_type;           // Type of thread (Detached or Joinable)
  gxThreadError thread_error;         // Last reported thread error
  gxThreadExitCode thread_exit_code;  // Thread exit code

  // Thread parameter pointer
  void *thread_parm; // Optional thread parameter pointer
  
  // Object and class identification members
  gxThreadObjectID thread_oid; // Object ID for this thread     
  gxThreadClassID thread_cid;  // Class ID for this thread
  
  // Members used to suspend and resume threads 
  gxMutex *suspend_lock_t; // Lock used to serialize access to suspend_t
  gxCondition *suspend_t;  // Thread suspend condition
};

#endif // __GX_THREAD_THREAD_CONTAINER__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
