// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxthread.h
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

The gxThread class is an abstract base class used to create
platform independent multi-threaded applications. The derived
class interface consists of three functions:

virtual void *gxThread::ThreadEntryRoutine(gxThread_t *thread) = 0;
virtual void gxThread::ThreadExitRoutine(gxThread_t *thread);
virtual void gxThread::ThreadCleanupHandler(gxThread_t *thread);

A derived class must override the gxThread::ThreadEntryRoutine()
function, which is the thread's entry point of execution.

The gxThread::ThreadExitRoutine() function can be used by a
derived class to perform any additional operations required
before the thread exits.

The gxThread::ThreadCleanupHandler should be used by a derived
class if any cleanup operations need to be performed prior to
thread cancellation.

Changes:
==============================================================
11/14/2002: Changed argument type from int to unsigned long in 
the gxThread::api_mSleep() and gxThread::api_sSleep() functions.

11/14/2002: Added the gxThread::api_uSleep() and 
gxThread::api_nSleep() functions used to sleep for a specified 
number of microseconds and nanoseconds.

03/20/2003: Depreciated the gxThread::DestoryThreadPool() 
function name with transposed "ro" in function name. Replaced 
with gxThread::DestroyThreadPool() function.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_THREAD_HPP__
#define __GX_THREAD_HPP__

#include "gxdlcode.h"

#include "thrtypes.h"
#include "gthreadt.h"
#include "thrpool.h"

// Forward class calling conventions
class gxThreadAPIWrapper; // Class used to handle native API calls

class GXDLCODE_API gxThread
{
public:
  gxThread();
  virtual ~gxThread();

private: // Disallow coping and assignment
  gxThread(const gxThread &ob) { }
  void operator=(const gxThread &ob) { } 

public: // Thread functions
  gxThread_t *ConstructThread(gxThreadType t = gxTHREAD_TYPE_JOINABLE, 
			      gxStackSizeType ssize = 0);

  gxThread_t *ConstructThread(void *parm,
			      gxThreadType t = gxTHREAD_TYPE_JOINABLE, 
			      gxStackSizeType ssize = 0);

  gxThread_t *CreateThread(gxThreadType t = gxTHREAD_TYPE_JOINABLE, 
			   gxStackSizeType ssize = 0);

  // Overload that allows a parameter to be passed to the thread
  gxThread_t *CreateThread(void *parm,
			   gxThreadType t = gxTHREAD_TYPE_JOINABLE, 
			   gxStackSizeType ssize = 0);

  int CreateThread(gxThread_t *thread);
  int DestroyThread(gxThread_t *thread, int check_state = 1);
  int ResumeThread(gxThread_t *thread);
  int SuspendThread(gxThread_t *thread);
  int JoinThread(gxThread_t *thread);
  int CloseThread(gxThread_t *thread);
  int CancelThread(gxThread_t *thread);
  void *ExitThread(gxThread_t *thread, int exit_code);
  int ThreadKeyCreate(gxThreadKey &key);
  int ThreadKeyDelete(gxThreadKey &key);
  int ThreadSetSpecific(gxThreadKey &key, const void *value);
  void *ThreadGetSpecific(gxThreadKey &key);
  int SetThreadPriority(gxThread_t *thread);
  int SetThreadPriority(gxThread_t *thread, gxThreadPriority prio);
  int SetThreadPriority(gxThread_t *thread, gxThreadPriority prio,
			gxThreadPriorityClass prio_class);

public: // Thread pool functions
  gxThread_t *ConstructThread(thrPool *thread_pool,
			   gxThreadPriority prio = gxTHREAD_PRIORITY_NORMAL, 
			   gxThreadType t = gxTHREAD_TYPE_JOINABLE, 
			   gxStackSizeType ssize = 0);

  gxThread_t *CreateThread(thrPool *thread_pool,
  			   gxThreadPriority prio = gxTHREAD_PRIORITY_NORMAL, 
  			   gxThreadType t = gxTHREAD_TYPE_JOINABLE, 
  			   gxStackSizeType ssize = 0);

  gxThread_t *ConstructThread(thrPool *thread_pool, void *parm,
			      gxThreadPriority prio = gxTHREAD_PRIORITY_NORMAL, 
			      gxThreadType t = gxTHREAD_TYPE_JOINABLE, 
			      gxStackSizeType ssize = 0);

  gxThread_t *CreateThread(thrPool *thread_pool, void *parm,
  			   gxThreadPriority prio = gxTHREAD_PRIORITY_NORMAL, 
  			   gxThreadType t = gxTHREAD_TYPE_JOINABLE, 
  			   gxStackSizeType ssize = 0);

  thrPool *ConstructThreadPool(unsigned pool_size);

  thrPool *CreateThreadPool(unsigned pool_size, void *parm = 0,
			    gxThreadPriority prio = gxTHREAD_PRIORITY_NORMAL, 
			    gxThreadType t = gxTHREAD_TYPE_JOINABLE, 
			    gxStackSizeType ssize = 0);
  
  int JoinThread(thrPool *thread_pool);
  void RebuildThreadPool(thrPool *thread_pool);
  int DestroyThreadPool(thrPool *thread_pool, int check_state = 1);

public: // Timer functions
  void sSleep(unsigned long seconds);
  void mSleep(unsigned long milliseconds);
  void uSleep(unsigned long microseconds);
  void nSleep(unsigned long nanoseconds);

protected: // Derived class interface
  virtual void *ThreadEntryRoutine(gxThread_t *thread) = 0;
  virtual void ThreadExitRoutine(gxThread_t *thread);
  virtual void ThreadCleanupHandler(gxThread_t *thread);
  
protected: // gxThread_t class interface
  friend class gxThread_t;

protected: // Interface to API wrapper class
  friend class gxThreadAPIWrapper;
  gxThreadAPIWrapper *thread_api_wrapper;
};

#endif // __GX_THREAD_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
