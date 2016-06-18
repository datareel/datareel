// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxthread.cpp
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

The gxThread class is an abstract base class used to create
platform independent multi-threaded applications.

Changes:
==============================================================
09/29/2005: Modified the gxThread::DestroyThreadPool() function 
to set the thrPool *thread_pool variable to zero after deleting 
the object.
==============================================================
*/
// ----------------------------------------------------------- //   
#include "gxdlcode.h"

#include "gxthread.h"
#include "thrapiw.h"

#ifdef __BCC32__
#pragma warn -8057
#endif

gxThread::gxThread()
{
  // Initialize the native API thread wrapper 
  thread_api_wrapper = new gxThreadAPIWrapper;
}

gxThread::~gxThread()
{
  // Prevent program crash if not initialized
  // PC-lint 09/08/2005: Function may throw exception in destructor
  if(thread_api_wrapper) delete thread_api_wrapper;
}

gxThread_t *gxThread::ConstructThread(void *parm, gxThreadType t,
				      gxStackSizeType ssize)
// Construct a new thread without executing it. Returns the newly
// constructed thread container or null if thread cannot be constructed.
{
  // Create a new thread container
  gxThread_t *thread = new gxThread_t;
  if(!thread) return 0;
  
  thread->thread_parm = parm;
  thread->thread_type = t;
  thread->stack_size = ssize;

  // Initialize the thread's entry pointer
  thread->entry = this; 
  
  return thread;
}

gxThread_t *gxThread::ConstructThread(gxThreadType t, gxStackSizeType ssize)
// Construct a new thread without executing it. Returns the newly
// constructed thread container or null if thread cannot be constructed.
{
  return ConstructThread((void *)0, t, ssize);
}

gxThread_t *gxThread::CreateThread(void *parm, gxThreadType t, 
				   gxStackSizeType ssize)
// Create and execute a new thread. NOTE: Any errors that occur will be
// recorded in gxThread_t::thread_error member and can be obtained by the
// application with a call to the gxThread_t::GetThreadError() function. 
// Any errors reported by the thread's entry function can be obtained with 
// a call to the gxThread_t::GetThreadExitCode() function.
{
  // Construct a new thread container
  gxThread_t *thread = ConstructThread(parm, t, ssize);
  if(!thread) return 0;

  // NOTE: The thread priority and scheduling policy can be 
  // changed after the thread is created with a call to the
  // gxThread::SetThreadPriority() function.

  // Execute the thread
  thread_api_wrapper->apiCreateThread(thread);

  // Return a pointer to the thread
  return thread; 
}

gxThread_t *gxThread::CreateThread(gxThreadType t, gxStackSizeType ssize)
// Create and execute a new thread. NOTE: Any errors that occur will be
// recorded in gxThread_t::thread_error member and can be obtained by the
// application with a call to the gxThread_t::GetThreadError() function. 
// Any errors reported by the thread's entry function can be obtained with 
// a call to the gxThread_t::GetThreadExitCode() function. 
{
  return CreateThread((void *)0, t, ssize);
}

int gxThread::CreateThread(gxThread_t *thread)
// Execute the specified thread, which was constructed by
// another entity. Returns a non-zero value if any errors
// occur.
{
  if(!thread) return 1; // Ensure that the thread pointer is not null

  // Return an error coniditon if this is not a new thread
  if(thread->thread_state != gxTHREAD_STATE_NEW) {
    thread->thread_error = gxTHREAD_CREATE_ERROR;
    return 1;
  }

  thread->entry = this; // Initialize the entry pointer

  // Call the native API function used to execute the thread
  return thread_api_wrapper->apiCreateThread(thread);
}

int gxThread::ResumeThread(gxThread_t *thread)
// Resume a thread that is currently paused. 
// Returns a non-zero value if any errors occur.
{
  if(!thread) return 1; // Ensure that the thread pointer is initialized
  
  // The thread is already running so return without signaling an error
  if(thread->thread_state == gxTHREAD_STATE_RUNNING) return 0;
  
  // The thread must have been previously suspended to be resumed
  if(thread->thread_state != gxTHREAD_STATE_SUSPENDED) {
    thread->thread_error = gxTHREAD_RESUME_ERROR;
    return 1;
  }

  // Call the native API function
  return thread_api_wrapper->apiResumeThread(thread);
}

int gxThread::SuspendThread(gxThread_t *thread)
// Suspend a thread that is currently running. Returns a non-zero
// value if any errors occur. NOTE: This function should only be
// called inside the thread entry routine.
{
  // The thread is already suspended so return without signaling an error  
  if(thread->thread_state == gxTHREAD_STATE_SUSPENDED) return 0;

  // The thread must be currently running in order to be suspened
  if(thread->thread_state != gxTHREAD_STATE_RUNNING) {
    thread->thread_error = gxTHREAD_SUSPEND_ERROR;
    return 1;
  }

  // Call the native API function
  return thread_api_wrapper->apiSuspendThread(thread);
}

int gxThread::DestroyThread(gxThread_t *thread, int check_state)
// Destroy a thread previously created with a call to the 
// gxThread::CreateThread() function. If the "check_state" 
// variable is true the thread's current state will be 
// evaluated before the thread is destroyed. Returns a 
// non-zero value if the thread cannot be destroyed.
{
  if(!thread) return 1; // The thread pointer is not initialized
  return thread_api_wrapper->apiDestroyThread(thread, check_state);
}

int gxThread::JoinThread(gxThread_t *thread)
// This function will force a process to wait until the specified 
// thread has finished execution before allowing the process to 
// continue. Returns a non-zero value if any errors occur or the 
// thread is not joinable.
{
  if(!thread) return 1; // The thread pointer is not initialized

  // Call the native API join function
  return thread_api_wrapper->apiJoinThread(thread);
}

void gxThread::ThreadExitRoutine(gxThread_t *thread)
// Function used to perform any routines needed before
// exiting a thread under normal conditions.
{
  // Override this function in the derived class if needed
  // Ignore BCC32 warning 8057 thread is never used
}

void gxThread::ThreadCleanupHandler(gxThread_t *thread)
// Function used to perform any cleanup routines needed when a
// a thread is canceled. 
{
  // Override this function in the derived class if needed
  // Ignore BCC32 warning 8057 thread is never used
}

void gxThread::nSleep(unsigned long nanoseconds)
// Sleep for a specified number of nanoseconds. 
{
  thread_api_wrapper->api_nSleep(nanoseconds);
}

void gxThread::uSleep(unsigned long microseconds)
// Sleep for a specified number of microseconds. 
{
  thread_api_wrapper->api_uSleep(microseconds);
}

void gxThread::mSleep(unsigned long milliseconds)
// Sleep for a specified number of milliseconds. 
{
  thread_api_wrapper->api_mSleep(milliseconds);
}

void gxThread::sSleep(unsigned long seconds)
// Sleep for a specified number of seconds. 
{
  thread_api_wrapper->api_sSleep(seconds);
}

void *gxThread::ExitThread(gxThread_t *thread, int exit_code)
// Exit function used by threads wishing to terminate explicitly
// in the middle of a task without terminating the whole process.
// Returns a pointer to the thread's exit code to allow this 
// function to be used as a return value. NOTE: This function
// should only be called inside the thread entry routine.
{
  if(!thread) return 0; // The thread pointer is not initialized
  return thread_api_wrapper->apiExitThread(thread, exit_code);
}

int gxThread::CancelThread(gxThread_t *thread)
// Function used to cancel a thread during an operation when the
// thread is not allocating resources or accessing locked variables.
// Returns a non-zero value if the thread cannot be canceled.
{
  if(!thread) return 1; // The thread pointer is not initialized
  return thread_api_wrapper->apiCancelThread(thread);
}

int gxThread::CloseThread(gxThread_t *thread)
// Close the thread's handle prior to destruction to indicate that
// the that storage for the thread can be reclaimed. Returns a
// non-zero value if the thread cannot be closed.
{
  if(!thread) return 1; // The thread pointer is not initialized
  return thread_api_wrapper->apiCloseThread(thread);
}


int gxThread::ThreadKeyCreate(gxThreadKey &key)
// Creates a thread-specific data key. Returns a non-zero
// value if any errors occur.
{
  return thread_api_wrapper->apiThreadKeyCreate(key);
}

int gxThread::ThreadKeyDelete(gxThreadKey &key)
// Releases a thread local storage key. NOTE: It is the responsibility 
// of the application to free any allocated dynamic storage associated
// with this key prior to calling this function. Returns a non-zero value
// if any errors occur.
{
  return thread_api_wrapper->apiThreadKeyDelete(key);
}

int gxThread::ThreadSetSpecific(gxThreadKey &key, const void *value)
// Stores a value in the calling thread's thread local storage slot for
// the specified thread. Returns a non-zero value if any errors occur.
{
  return thread_api_wrapper->apiThreadSetSpecific(key, value);
}

void *gxThread::ThreadGetSpecific(gxThreadKey &key)
// Returns the value currently bound to the specified key for the
// calling thread. Returns a non-zero value if an error occurs.
{
  return thread_api_wrapper->apiThreadGetSpecific(key);
}

int gxThread::SetThreadPriority(gxThread_t *thread)
// Set or change the priority and scheduling policy of the a thread
// in accordance with gxThread_t::thread_priority and thread_class
// members. Returns a non-zero value if the priority or policy can
// not be changed. NOTE: Do to cross-platform issues the thread
// priority is no longer set when the thread is created. This
// function must be used to change a thread priority after it has
// been created. 
{
  if(!thread) return 1; // The thread pointer is not initialized

  // NOTE: If this thread is conatined in a thread pool the 
  // thread position in the pool will not be changed by this
  // function.
  return thread_api_wrapper->apiSetThreadPriority(thread);
}

int gxThread::SetThreadPriority(gxThread_t *thread, gxThreadPriority prio)
// Set or change the priority of the specified thread. Returns a non-zero 
// value if the priority can not be changed.
{
  if(!thread) return 1; // The thread pointer is not initialized
  thread->thread_priority = prio;
  return thread_api_wrapper->apiSetThreadPriority(thread);
}

int gxThread::SetThreadPriority(gxThread_t *thread, gxThreadPriority prio,
				gxThreadPriorityClass prio_class)
// Set or change the priority and scheduling policy of the specified
// thread. Returns a non-zero value if the priority or policy can
// not be changed. NOTE: The WIN32 scheduling policy is determined
// by the priority class of its process and will not be set here.
{
  if(!thread) return 1; // The thread pointer is not initialized
  thread->thread_priority = prio;
  thread->thread_priority_class = prio_class;
  return thread_api_wrapper->apiSetThreadPriority(thread);
}

// --------------------------------------------------------------
// Thread Pool Functions
// --------------------------------------------------------------
gxThread_t *gxThread::ConstructThread(thrPool *thread_pool, void *parm,
				      gxThreadPriority prio,
				      gxThreadType t,
				      gxStackSizeType ssize)
// Construct a new thread and throw it in the specified thread pool
// without executing it. Returns the newly constructed thread or null
// if thread cannot be constructed. NOTE: The thread priority will 
// determine its priority in the thread queue. 
{
  if(!thread_pool) return 0; // The thread_pool pointer is not initialized

  // Construct a new thread container
  gxThread_t *thread = ConstructThread();
  if(!thread) return 0;

  thread->thread_priority = prio; 
  thread->thread_parm = parm; 
  thread->thread_type = t;
  thread->stack_size = ssize;

  // Throw the thread into the pool without executing it
  thread_pool->AddThread(thread);
  
  return thread; 
}

gxThread_t *gxThread::CreateThread(thrPool *thread_pool, void *parm,
				   gxThreadPriority prio,
				   gxThreadType t,
				   gxStackSizeType ssize)
// Create a new thread and throw it in the specified thread pool
// after executing it. Returns the newly constructed thread or null
// if thread cannot be constructed. NOTE: The thread priority will 
// determine its priority in the thread queue.
{
  if(!thread_pool) return 0; // The thread_pool pointer is not initialized

  // Construct a new thread container and throw it in the pool
  gxThread_t *thread = ConstructThread(thread_pool, parm, prio, t, ssize);
  if(!thread) return 0;

  // Execute the thread
  thread_api_wrapper->apiCreateThread(thread);

  return thread; 
}

gxThread_t *gxThread::CreateThread(thrPool *thread_pool,
				   gxThreadPriority prio,
				   gxThreadType t,
				   gxStackSizeType ssize)
// Create a new thread and throw it in the specified thread pool
// after executing it. Returns the newly constructed thread or null
// if thread cannot be constructed. NOTE: The thread priority will 
// determine its priority in the thread queue.
{
  if(!thread_pool) return 0; // The thread_pool pointer is not initialized
  return CreateThread(thread_pool, (void *)0, prio, t, ssize);
}

gxThread_t *gxThread::ConstructThread(thrPool *thread_pool,
				      gxThreadPriority prio,
				      gxThreadType t,
				      gxStackSizeType ssize)
// Construct a new thread and throw it in the specified thread pool
// without executing it. Returns the newly constructed thread or null
// if thread cannot be constructed. NOTE: The thread priority will 
// determine its priority in the thread queue.
{
  if(!thread_pool) return 0; // The thread_pool pointer is not initialized
  return ConstructThread(thread_pool, (void *)0, prio, t, ssize);
}

thrPool *gxThread::ConstructThreadPool(unsigned pool_size)
// Construct a new thread pool with the specified number of nodes.
// NOTE: None of threads will be executed.
{
  thrPool *thread_pool = new thrPool;
  if(!thread_pool) return 0;

  for(unsigned i = 0; i < pool_size; i++)
    ConstructThread(thread_pool);
  
  return thread_pool;
}

thrPool *gxThread::CreateThreadPool(unsigned pool_size, void *parm,
				    gxThreadPriority prio,  gxThreadType t,
				    gxStackSizeType ssize)
// Create a new thread pool and execute each thread in the
// pool as it is created.
{
  thrPool *thread_pool = new thrPool;
  if(!thread_pool) return 0;
  
  for(unsigned i = 0; i < pool_size; i++)
    CreateThread(thread_pool, parm, prio, t, ssize);
  
  return thread_pool;
}

int gxThread::JoinThread(thrPool *thread_pool)
// Make the process wait until all the threads in the 
// specified pool have finished executing. Returns a
// non-zero value if the threads cannot be joined.
{
  if(!thread_pool) return 1;
  
  thrPoolNode *ptr = thread_pool->GetHead();

  while(ptr) {
    int rv = JoinThread(ptr->GetThreadPtr());
    if(rv) return rv;
    ptr = ptr->GetNext();
  }

  return 0;
}

void gxThread::RebuildThreadPool(thrPool *thread_pool)
// Remove all the threads in the pool that have exited
// or have been canceled.
{
  if(!thread_pool) return; // The pool pointer has not been initialized
  
  // Start rebuilding at the top of the pool to allow the lower
  // prioity threads to finish executing.
  thrPoolNode *next = thread_pool->GetHead();

  thrPoolNode *curr;
  gxThread_t *thread;
  gxThreadState state ;

  while(next) {
    state = next->GetThreadPtr()->GetThreadState();
    
    if((state == gxTHREAD_STATE_CANCELED) || (state == gxTHREAD_STATE_EXITED))
      {
	curr = next;
	if(next->GetNext() == 0) { // This is the last node in the pool
	  thread = thread_pool->RemoveNode(curr);
	  if(thread) DestroyThread(thread, 0);
	  thread_pool->MakeEmpty(); // The pool is now empty
	  return;
	}
	else { // We are somewhere in the middle of the list
	  next = next->GetNext();
	  thread = thread_pool->RemoveNode(curr);
	  if(thread) DestroyThread(thread, 0);
	  continue;
	}
      }
    
    next = next->GetNext();    
  }

  return;
}

int gxThread::DestroyThreadPool(thrPool *thread_pool, int check_state)
// Destroy a previously created thread pool. If the "check_state" variable
// is true the current state of each thread will be checked before it is
// destroyed. If a thread cannot be canceled and the "check_state" variable
// is true, this function will return a non-zero value.
{
  if(!thread_pool) return 1;

  thrPoolNode *ptr;
  gxThread_t *thread = 0;
  gxThreadType type;
  gxThreadState state;
  
  while(!thread_pool->IsEmpty()) {
    // Start checking at the tail since the higher priority threads
    // at the head of the pool would most likely have exited first.
    ptr = thread_pool->GetTail();
    state = ptr->GetThreadPtr()->GetThreadState();
    type = ptr->GetThreadPtr()->GetThreadType();
    switch(state) {
      case gxTHREAD_STATE_INVALID :
	// We don't know what state this thread is in so lets delete it
	thread = thread_pool->RemoveNode(ptr);
	if(thread) DestroyThread(thread, 0);
	break;
      case gxTHREAD_STATE_CANCELED :
	thread = thread_pool->RemoveNode(ptr);
	if(thread) DestroyThread(thread, 0);
	break;
      case gxTHREAD_STATE_EXITED :
	thread = thread_pool->RemoveNode(ptr);
	if(thread) DestroyThread(thread, 0);
	break;
      case gxTHREAD_STATE_NEW :
	// Newly created thread that was never executed
	// Lets assume that is not intended to be executed
	// at this point.
	thread = thread_pool->RemoveNode(ptr);
	if(thread) DestroyThread(thread, 0);
	break;
      case gxTHREAD_STATE_RUNNING :
	if((check_state) && (type != gxTHREAD_TYPE_DETACHED)) {
	  // PC-lint 09/14/2005: thread may not be initalized 
	  thread = ptr->GetThreadPtr();
	  if(thread) {
	    if(CancelThread(thread) != 0) return 1;
	  }
	}
	thread = thread_pool->RemoveNode(ptr);
	if(thread) DestroyThread(thread, 0);
	break;
      case gxTHREAD_STATE_SUSPENDED :
	if((check_state) && (type != gxTHREAD_TYPE_DETACHED)) {
	  // PC-lint 09/14/2005: thread may not be initalized 
	  thread = ptr->GetThreadPtr();
	  if(thread) {
	    if(CancelThread(thread) != 0) return 1;
	  }
	}
	thread = thread_pool->RemoveNode(ptr);
	if(thread) DestroyThread(thread, 0);
	break;
      case gxTHREAD_STATE_WAITING :
	if((check_state) && (type != gxTHREAD_TYPE_DETACHED)) {
	  // PC-lint 09/14/2005: thread may not be initalized 
	  thread = ptr->GetThreadPtr();
	  if(thread) {
	    if(CancelThread(thread) != 0) return 1;
	  }
	}
	thread = thread_pool->RemoveNode(ptr);
	if(thread) DestroyThread(thread, 0);
	break;
      default:
	thread = thread_pool->RemoveNode(ptr);
	if(thread) DestroyThread(thread, 0);
	break;
    }
  }

  delete thread_pool;
  thread_pool = 0;
  return 0;
}

#ifdef __BCC32__
#pragma warn .8057
#endif
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
