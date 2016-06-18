// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: thrapiw.cpp
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

The gxThreadAPIWrapper class is used to call native thread API 
functions for each specified platform. NOTE: This class is not 
intended to be used directly by an application or a derived 
class. It is used by the gxThread base class to call the correct 
native thread API function for each of the supported platforms.

Changes:
==============================================================
11/14/2002: Modified the gxThreadAPIWrapper::api_mSleep() to 
use the timed condition instead of the usleep() function 
under UNIX/POSIX.

05/17/2003: Modified the gxThreadAPIWrapper::apiThreadStartRoutine() 
function to test the thread and entry function pointers before 
calling the entry function and after the entry function returns. 
This change was implemented to prevent a possible program crash 
if the entry function goes out of scope before the thread exit 
routine is called. 

09/05/2003: Modified the gxThreadAPIWrapper::apiDestroyThread() 
function to test for null pointers and set the thread pointer 
to 0 after deleting it.

09/05/2003: Modified the gxThreadAPIWrapper::apiDestroyThread() 
not to close the thread under POSIX due to segmentation faults 
under GCC version 3.2.2 when the close thread call detaches the thread.

01/21/2003: Modified the gxThreadAPIWrapper::apiCreateThread() 
function to use the PTHREAD_STACK_MIN value by default instead of 
the 2MB stack size per thread, which is the default POSIX stack size. 
This change affects UNIX systems and was implemented to all multiple 
threads to execute on low memory systems. 

01/22/2004: Modified the gxThreadAPIWrapper::apiCreateThread() to set 
the pthread contention scope to SYSTEM under HPUX 11 only.

01/22/2004: Modified the gxThreadAPIWrapper::apiCreateThread() not to 
set a stack size exception under POSIX.

01/22/2004: Under Linux the iostream include file is used to ensure 
proper linkage in C++ programs when using GCC and the Intel C++ compilers.

07/14/2009: Changed to fix compiler: cast from void* to gxThreadExitCode loses precision 
thread->thread_exit_code = (gxThreadExitCode)thread->entry->ThreadEntryRoutine(thread);
Using a long will fix the 64-bit alignment problem when type casting a void * pointer 
on 64-bit compilers.

06/17/2016: Some POSIX implementations are setting the default
stack size ot 64KB casuing a segmentation fault when a thead
requires more than 64KB of stack space. Added fix to set the
minimum size to 2MB.

06/17/2016: Added stack size set fix for POSIX threads that do not have the 
PTHREAD_STACK_MIN value defined.
==============================================================
*/
// ----------------------------------------------------------- //   
#include "gxdlcode.h"

#include "thrapiw.h"
#include "gxthread.h"
#include "gthreadt.h"
#include "gxmutex.h"
#include "gxcond.h"

#if defined (__POSIX__) && defined (__REENTRANT__)
#include <time.h>
#endif

#if defined (__LINUX__)
#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__
#endif // __LINUX__

int gxThreadAPIWrapper::apiCloseThread(gxThread_t *thread)
// Close the thread's handle to indicate that the that storage
// for the thread can be reclaimed. Returns a non-zero value if
// the thread cannot be closed. NOTE: This function will have no
// effect on POSIX 1003.1c threads that were created in a detached
// state.
{
#if defined (__WIN32__)
  if(thread->thread_id) {
    if(!CloseHandle(thread->thread_id)) { 
      thread->thread_error = gxTHREAD_CLOSE_ERROR;
      return 1;
    }
    thread->thread_id = 0;
  }

#elif defined (__HPUX10__) && defined (__POSIX__)
  // HPUX 10.20 Common Multithread Architecture (CMA) services

  // The HPUX 10.20 CMA services will not allow thread to be created
  // in a detached state so detach the thread at this point whether
  // it is joinable or detached
  int rv = pthread_detach(&thread->thread_id);
  if(rv != 0) {
    thread->thread_error = gxTHREAD_CLOSE_ERROR;
    return rv;
  }
  
#elif defined (__POSIX__)
  // POSIX 1003.1c 1995 thread standard
  
  if(thread->thread_type == gxTHREAD_TYPE_DETACHED) {
    // The thread was created in a detached state meaning the
    // gxThread_t::thread_id has been released and can no longer
    // be used.
    return 0;
  }
  
  // Detach the thread
  pthread_detach(thread->thread_id);

#else
#error You must define an API: __WIN32__ or __POSIX__
#endif

  return 0; // No errors were reported
}

int gxThreadAPIWrapper::apiSuspendThread(gxThread_t *thread)
// Suspend a thread that is currently running. Returns a
// non-zero value if the thread cannot be suspended.
{
  // Get the previous state incase the thread cannot be suspended
  gxThreadState prev_state = thread->thread_state;

  // Set the thread's state here or it will not be set correctly
  // when the thread is suspended.
  thread->thread_state = gxTHREAD_STATE_SUSPENDED;
  
#if defined (__WIN32__)
  DWORD rv = ::SuspendThread(thread->thread_id);
  if(rv == (DWORD)-1) {
    thread->thread_error = gxTHREAD_SUSPEND_ERROR;
    thread->thread_state = prev_state;
    return 1;
  }
#elif defined (__POSIX__)
  thread->suspend_lock_t->MutexLock();
  if(thread->suspend_t->ConditionWait(thread->suspend_lock_t) != 0) {
    thread->suspend_lock_t->MutexUnlock();
    thread->thread_error = gxTHREAD_SUSPEND_ERROR;
    thread->thread_state = prev_state;
    return 1;
  }
#else
#error You must define an API: __WIN32__ or __POSIX__
#endif

  return 0; // No errors were reported
}

int gxThreadAPIWrapper::apiResumeThread(gxThread_t *thread)
// Resume a suspended thread. Returns a non-zero value if
// the thread cannot be resumed.
{
  // Get the previous state incase the thread cannot be resumed
  gxThreadState prev_state = thread->thread_state;
  
  // Set the thread's state here or it will not be set correctly
  // when the thread is resumed
  thread->thread_state = gxTHREAD_STATE_RUNNING;

#if defined (__WIN32__)
  DWORD rv = ::ResumeThread(thread->thread_id);
  if(rv == (DWORD)-1) {
    thread->thread_error = gxTHREAD_RESUME_ERROR;
    thread->thread_state = prev_state;
    return 1;
  }
#elif defined (__POSIX__)
  thread->suspend_lock_t->MutexUnlock();
  int rv = thread->suspend_t->ConditionSignal();

  if(rv) {
    thread->thread_error = gxTHREAD_RESUME_ERROR;
    thread->thread_state = prev_state;
    return 1;
  } 
#else
#error You must define an API: __WIN32__ or __POSIX__
#endif

  return 0; // No errors were reported
}

#if defined (__WIN32__)
DWORD gxThreadAPIWrapper::apiThreadStartRoutine(void *arg)
#elif defined (__POSIX__)
void *gxThreadAPIWrapper::apiThreadStartRoutine(void *arg)
#else
#error You must define an API: __WIN32__ or __POSIX__
#endif
// Static API wrapper member function used by the WIN32 and POSIX
// interfaces "create thread" functions to invoke the thread entry 
// routine. The WIN32/POSIX "end thread" routine is automatically 
// called when this function returns a value. A return value of
// ZERO indicates that no errors where encountered. A non-zero
// return value indicates that an error has occurred. NOTE: Both
// the WIN32 _endthreadex() and ExitThread() calls do not close the
// thread handle. The WIN32 thread handle must be closed after this
// function returns.
{
  // Type cast the void pointer to the correct type
  gxThread_t *thread = (gxThread_t *)(arg);

  if(!thread) { // Thread pointer is not valid 
#if defined (__WIN32__) 
    return 0;         // No errors were reported 
#elif defined (__POSIX__)
    return (void *)0; // No errors were reported
#else
#error You must define an API: __WIN32__ or __POSIX__
#endif
  }

  // Original pointer address holders
  gxThread *entry_ptr = 0;
  gxThread_t *thread_ptr = 0;
   
  if(thread->entry) { // Ensure the gxThread pointer has been initialized 

    entry_ptr = thread->entry; // Record the original function address
    thread_ptr = thread; // Record the original thread address	

    // Enter any variables that need to be initialized before the thread
    // is executed or callback functions here
    // ------------------------------------------------------------
    thread->thread_state = gxTHREAD_STATE_RUNNING;

#if defined (__WIN32__)
    thread->thread_exit_code = \
      (gxThreadExitCode)thread->entry->ThreadEntryRoutine(thread);
    
#elif defined (__POSIX__)
    // Install the thread's cleanup handler for threads that
    // are cancelled by the application.
    pthread_cleanup_push(apiThreadCleanupHandler, (void *)(thread));
    
    // 07/14/2009: Changed to fix compiler: cast from void* to gxThreadExitCode loses precision
    // thread->thread_exit_code = (gxThreadExitCode)thread->entry->ThreadEntryRoutine(thread);
    // Using a long will fix the 64-bit alignment problem when type casting a void * pointer 
    // on 64-bit compilers.
    long rv = (long)thread->entry->ThreadEntryRoutine(thread);
    thread->thread_exit_code = (gxThreadExitCode)rv;
    
    // Remove the cleanup handler routine at the top of the cancellation
    // cleanup stack of the calling thread
    pthread_cleanup_pop(0);
  
#else
#error You must define an API: __WIN32__ or __POSIX__
#endif
  }
  else { // Could not execute the entry function
    thread->thread_error = gxTHREAD_EXECUTE_ERROR;
#if defined (__WIN32__)
    return (DWORD)1;
#elif defined (__POSIX__)
    return (void *)1; 
#else
#error You must define an API: __WIN32__ or __POSIX__
#endif
  }
  
  // Test to ensure that the thread pointer is still valid and
  // pointing to same thread that invoked the call.
  if(thread_ptr == thread) {
  
    // Test to ensure that the entry function is still valid and 
    // pointing to the same entry function used with this thread.
    if(thread->entry == entry_ptr) {
      // The thread entry function has exited
      thread->thread_state = gxTHREAD_STATE_EXITED;
      
      // Clean up routine defined in the derived class.
      // NOTE: The gxThread_t pointer will no longer be
      // used after the derived class exit routine is
      // called. This allows the derived class to delete
      // the pointer if it is no longer needed.
      thread->entry->ThreadExitRoutine(thread);
    }
    else {
      // The state of this thread is not known
      thread->thread_state = gxTHREAD_STATE_INVALID;
    }
  }

  // The entry and exit routines have finished executing and
  // exited normally at this point. Now this function will
  // return to the gxThreadAPIWrapper::apiCreateThread()
  // function with invoked this call. 
#if defined (__WIN32__) 
  return 0;         // No errors were reported 
#elif defined (__POSIX__)
  return (void *)0; // No errors were reported
#else
#error You must define an API: __WIN32__ or __POSIX__
#endif
}

int gxThreadAPIWrapper::apiCreateThread(gxThread_t *thread)
// Native API used to create and execute a thread. Returns a
// non-zero value if the thread cannot be created. NOTE: Do to
// cross-platform issues the thread priority is no longer set
// here. The gxThreadAPIWrapper::apiSetThreadPriority() function
// must be used to change a thread priority after it has been
// created.
{
  // Initialize the thread attributes
  int rv = apiInitThreadAttribute(&thread->thread_attribute);
  if(rv != 0) {
    thread->thread_error = gxTHREAD_CREATE_ERROR;
    return rv; // Could not initialize the thread attribute
  }

#if defined (__WIN32__)
#ifdef __HAS__BEGINTHREADEX__
  // C RTL call used with compilers that have the _beginthreadex() function.
  // NOTE: Any thread that uses the _beginthreadex() function should not
  // be used in conjunction with threads created with the CreateThread() call.
  // If the C RTL and WIN32 calls are mixed memory leaks will occur when the
  // ExitThread() function is called.
  DWORD thread_addr; // Temporary variable used to hold the threads address
  void *security = (void *)(thread->thread_attribute);
  thread->thread_id = (HANDLE)_beginthreadex(security, // Security descriptor
					     thread->stack_size,
           (gxThreadStartRoutine)gxThreadAPIWrapper::apiThreadStartRoutine,
					     (void *)(thread),
					     0,
					     (unsigned int *)&thread_addr);
#else 
  // WIN32 API call for compilers that do not have the _beginthreadex() 
  // function.
  DWORD win32_thread_id; // Temporary variable used to hold the WIN32 thread ID
  thread->thread_id = CreateThread(thread->thread_attribute, 
				   thread->stack_size,
  (LPTHREAD_START_ROUTINE)gxThreadAPIWrapper::apiThreadStartRoutine,
				   (LPVOID)thread,
				   DWORD(0),
				   &win32_thread_id);
#endif // C RTL/WIN32 API call
  
  // Could not create the thread 
  if(!thread->thread_id) {
    thread->thread_error = gxTHREAD_CREATE_ERROR;
    return 1;
  }

#elif defined (__HPUX10__) && defined (__POSIX__)
// HPUX 10.20 Common Multithread Architecture (CMA) services

  // Set the stack size if stack size greater then zero is specified 
  if(thread->stack_size > ( gxStackSizeType)0) {
    if(pthread_attr_setstacksize(&thread->thread_attribute,
				 thread->stack_size)!= 0) {
      thread->thread_error = gxTHREAD_STACK_ERROR;
      thread->stack_size = 0;
    }
  }
  
  // Create the thread and execute the thread's entry routine
  rv = pthread_create(&thread->thread_id, thread->thread_attribute,
   (gxThreadStartRoutine)gxThreadAPIWrapper::apiThreadStartRoutine,
		      (void *)(thread));
  if(rv != 0) {
    thread->thread_error = gxTHREAD_CREATE_ERROR;
    return rv; // Could not create the thread
  }

#elif defined (__POSIX__)
// POSIX 1003.1c 1995 thread standard
  
  // Set the thread's contention scope.
#if defined(PTHREAD_SCOPE_SYSTEM) && defined(PTHREAD_SCOPE_PROCESS)
  // The contention scope attribute specifies the set of threads with which
  // a thread must compete for processing resources. The contention scope
  // attribute specifies whether the new thread competes for processing
  // resources only with other threads in its own process, called process
  // contention scope, or with all threads on the system, called system
  // contention scope (PTHREAD_SCOPE_PROCESS or PTHREAD_SCOPE_SYSTEM).
  // NOTE: The contention scope is set to SCOPE_SYSTEM to avoid contention
  // scope errors under HPUX 11.
#if defined (__HPUX11__)
  if(pthread_attr_setscope(&thread->thread_attribute,
			   PTHREAD_SCOPE_SYSTEM) != 0) {

    // NOTE: The AS/400 implementation of pthreads does not support
    // this function so the SCOPE_ERROR should be ignored on AS/400
    // platforms.
    thread->thread_error = gxTHREAD_SCOPE_ERROR;
  }
#endif // __HPUX11__
#endif // Contention scope 

  // Set the threads state
#if defined(PTHREAD_CREATE_DETACHED)
  if(thread->thread_type == gxTHREAD_TYPE_DETACHED) {
    // Mark a thread so that the system reclaims the thread resources when
    // the thread  terminates. After a threads resources are freed, the exit
    // status is no longer available, and the thread cannot be detached or
    // joined to.
    if(pthread_attr_setdetachstate(&thread->thread_attribute, 
				   PTHREAD_CREATE_DETACHED) != 0) {
      
      // Tell the application that this thread is now joinable
      // because it could not be created in a detached state
      thread->thread_type = gxTHREAD_TYPE_JOINABLE;
    }
  }
#endif // Thread state

  // Set the stack size if a value greater then PTHREAD_STACK_MIN is defined
#if defined (PTHREAD_STACK_MIN) && defined (__POSIX__)
  // NOTE: The default POSIX stack size is 2MB. In order to allow many 
  // threads per process to run use the smallest stack size possible. 
  // The caller can always increase the stack size with the thread is created.
  if(thread->stack_size != PTHREAD_STACK_MIN) {
    if(thread->stack_size < PTHREAD_STACK_MIN) {
      thread->stack_size = PTHREAD_STACK_MIN; 
    }
    // 06/17/2016: Some POSIX implementations are setting the default
    // stack size ot 64KB casuing a segmentation fault when a thead
    // requires more than 64KB of stack space. Added fix below to set the
    // minimum size to 2MB.
    if(thread->stack_size < (2000*1000)) {
      thread->stack_size = 2000*1000;
    }

    if(pthread_attr_setstacksize(&thread->thread_attribute,
				 thread->stack_size)!= 0) {
      // NOTE: The AS/400 implementation of pthreads does not support
      // this function so the STACK_ERROR should be ignored on AS/400
      // platforms.
      // 01/22/2004: Do not set an exception here. A stack size of 0 under 
      // POSIX indicates the stack size has not been set.
      // thread->thread_error = gxTHREAD_STACK_ERROR;
      thread->stack_size = 0;
    }
  }
#elif defined (__POSIX__) && !defined (PTHREAD_STACK_MIN)
  // 06/17/2016: We are using POSIX threads but do not have PTHREAD_STACK_MIN
  // value defined.
  if(thread->stack_size > (gxStackSizeType)0) {
    size_t default_stack_size;
    pthread_attr_t default_attr;
    pthread_attr_init(&default_attr);
    pthread_attr_getstacksize(&default_attr, &default_stack_size);
    
    if(thread->stack_size != default_stack_size) {
      if(thread->stack_size < default_stack_size) {
	thread->stack_size = default_stack_size; 
      }
      if(thread->stack_size < (2000*1000)) {
	thread->stack_size = 2000*1000;
      }
      
      if(pthread_attr_setstacksize(&thread->thread_attribute,
				   thread->stack_size) != 0) {
	// A stack size of 0 indicates the stack size has not been set.
	// thread->thread_error = gxTHREAD_STACK_ERROR;
	thread->stack_size = 0;
      }
    }
  } 
#else
  // Tell the application that the stack size could not be set
  if(thread->stack_size > (gxStackSizeType)0) {
    thread->thread_error = gxTHREAD_STACK_ERROR;
    thread->stack_size = 0;
  }
#endif // Stack size

  // Create the thread and execute the thread's entry routine
  rv = pthread_create(&thread->thread_id, &thread->thread_attribute,
    (gxThreadStartRoutine)gxThreadAPIWrapper::apiThreadStartRoutine,
		      (void *)(thread));
  if(rv != 0) {
    thread->thread_error = gxTHREAD_CREATE_ERROR;
    return rv; // Could not create the thread
  }

#else
#error You must define an API: __WIN32__ or __POSIX__
#endif // End of CreateThread conditional directives

  // Continue here after the gxThreadAPIWrapper::apiThreadStartRoutine()
  // function has returned.

  // Clean up any thread variables that were initialized. This section
  // of the code will not be reached until the apiThreadStartRoutine()
  // function has returned, meaning that the thread has exited normally
  // at this point.
  apiDestroyThreadAttribute(&thread->thread_attribute);

  return 0; // No errors were reported 
}

int gxThreadAPIWrapper::apiJoinThread(gxThread_t *thread)
// This function will force a process to wait until the specified 
// thread has finshed execution before allowing the process to 
// continue. Returns a non-zero value if any errors occur or the 
// thread is not joinable.
{
  
  if(thread->thread_type == gxTHREAD_TYPE_DETACHED) {
    // Cannot join detached threads. The system will reclaim the thread
    // resources when a detached thread terminates and no exit status will
    // be available.  
    return 1;
  }

#if defined (__WIN32__)
  // Evaluate the thread's state before waiting
  switch(thread->thread_state) {
    case gxTHREAD_STATE_INVALID :
      return 0;
    case gxTHREAD_STATE_CANCELED :
      return 0;
    case gxTHREAD_STATE_EXITED :
      return 0;
    case gxTHREAD_STATE_NEW :
      break;
    case gxTHREAD_STATE_RUNNING :
      break;
    case gxTHREAD_STATE_SUSPENDED :
      break;
    case gxTHREAD_STATE_WAITING :
      break;
    default:
      break;
  }

  DWORD rv = WaitForSingleObject(thread->thread_id, INFINITE);
  switch(rv) {
    case WAIT_FAILED :
      return (int)WAIT_FAILED;
    case WAIT_ABANDONED :
      return (int)WAIT_ABANDONED;
    case WAIT_OBJECT_0 :
      return (int)WAIT_OBJECT_0;
    case WAIT_TIMEOUT :
      return (int)WAIT_TIMEOUT;
    default:
      break;
  }
    
#elif defined (__POSIX__)
  // NOTE: The thread's exit status is set by the apiThreadStartRoutine()
  // function so it is no necessary to have the pthread_join() function
  // set the exit value passed to pthread_exit() function. 
  int rv = pthread_join(thread->thread_id, (void **)0);
  if(rv != 0) return rv; // Could not join the thread
#else
#error You must define an API: __WIN32__ or __POSIX__
#endif
  return 0; // No errors were reported 
}

int gxThreadAPIWrapper::apiInitThreadAttribute(gxThreadAttribute *attr)
// Initialize a thread attribute. Returns a non-zero value if the attribute
// cannot be initialized.
{
#if defined (__WIN32__)
  // NOTE: The security descriptor must be NULL for Windows 95/98 applications.
  // The security descriptor is a pointer to a SECURITY_ATTRIBUTES structure
  // that determines whether the returned handle can be inherited by child
  // processes. The handle cannot be inherited if the security descriptor is
  // a NULL value. 
  *(attr) = (gxThreadAttribute)0;

#elif defined (__HPUX10__) && defined (__POSIX__)
// HPUX 10.20 Common Multithread Architecture (CMA) services
  int rv = pthread_attr_create(attr);
  if(rv != 0) return rv;

#elif defined (__POSIX__)
  // Initialize the POSIX thread attribute with the default settings.
  // By default the detachstate is PTHREAD_JOINABLE and scheduling
  // policy is SCHED_OTHER
  int rv = pthread_attr_init(attr);
  if(rv != 0) return rv;
#else
#error You must define an API: __WIN32__ or __POSIX__
#endif
  return 0; // No errors were reported
}
  
int gxThreadAPIWrapper::apiDestroyThreadAttribute(gxThreadAttribute *attr)
// Destroy the thread attribute. Returns a non-zero value if the thread
// attribute cannot be destroyed.
{
#if defined (__WIN32__)
  // Nothing to do for a null security descriptor
  if(*(attr) == (gxThreadAttribute)0) return 0; 

#elif defined (__HPUX10__) && defined (__POSIX__)
// HPUX 10.20 Common Multithread Architecture (CMA) services
  int rv = pthread_attr_delete(attr);
  if(rv != 0) return rv;

#elif defined (__POSIX__)
  int rv = pthread_attr_destroy(attr);
  if(rv != 0) return rv;
#else
#error You must define an API: __WIN32__ or __POSIX__
#endif
  return 0; // No errors were reported
}

void gxThreadAPIWrapper::api_nSleep(unsigned long nanoseconds)
// Make the process sleep for a specified number of nanoseconds.
{
  gxMutex t_mutex;
  gxCondition t_condition;
  t_mutex.MutexLock();
  while(1) {
    t_condition.ConditionTimedWait(&t_mutex, 0, nanoseconds);
    break;
  }
}

void gxThreadAPIWrapper::api_uSleep(unsigned long microseconds)
// Make the process sleep for a specified number of nanoseconds.
{
  // Convert microseconds to nanoseconds
  unsigned long nseconds = microseconds * 1000; 

  gxMutex t_mutex;
  gxCondition t_condition;
  t_mutex.MutexLock();
  while(1) {
    t_condition.ConditionTimedWait(&t_mutex, 0, nseconds);
    break;
  }
}

void gxThreadAPIWrapper::api_mSleep(unsigned long milliseconds)
// Make the process sleep for a specified number of milliseconds.
{
#if defined (__WIN32__)
  Sleep((DWORD)milliseconds);

#elif defined (__POSIX__)
  unsigned long nseconds = milliseconds * (1000 * 1000); 
  gxMutex t_mutex;
  gxCondition t_condition;
  t_mutex.MutexLock();
  while(1) {
    t_condition.ConditionTimedWait(&t_mutex, 0, nseconds);
    break;
  }

#else // No native sleep functions are defined
#error You must define a native API: __WIN32__ or __UNIX__
#endif
}

void gxThreadAPIWrapper::api_sSleep(unsigned long seconds)
// Make the process sleep for a specified number of seconds. 
{
#if defined (__WIN32__)
  int i = seconds * 1000; // Convert milliseconds to seconds
  Sleep((DWORD)i);
#elif defined (__UNIX__)
  sleep(seconds);
#else // No native sleep functions are defined
#error You must define a native API: __WIN32__ or __UNIX__
#endif
}

void *gxThreadAPIWrapper::apiExitThread(gxThread_t *thread, int exit_code)
// Exit function used by threads wishing to terminate explicitly
// without terminating the whole process. Returns a pointer to the
// thread's exit code to allow this function to be used as a return 
// value.
{
  // Set the thread's exit code and change the thread's state to exited
  thread->thread_exit_code = (gxThreadExitCode)exit_code;
  thread->thread_state = gxTHREAD_STATE_EXITED;

  // The thread will exit at this point
#if defined (__WIN32__)
#ifdef __HAS__BEGINTHREADEX__
  // NOTE: _endthreadex() does not automatically close the thread handle 
  _endthreadex((unsigned)exit_code);
#else
  ExitThread((DWORD)exit_code); 
#endif // __WIN32__ directives
	     
#elif defined (__POSIX__)
  pthread_exit((void *)exit_code);
	     
#else
#error You must define a native API: __WIN32__ or __UNIX__
#endif

  // NOTE: The return value will never be reached but it is needed 
  // to prevent compiler errors.
  return (void *)&thread->thread_exit_code;
}

void gxThreadAPIWrapper::apiThreadCleanupHandler(void *arg)
// When a thread is canceled, the currently stacked cleanup
// handler is executed and thread execution is terminated when
// this function returns.
{
  // Type cast the void pointer to the correct type
  gxThread_t *thread = (gxThread_t *)(arg);

  if(thread->entry) { // Ensure the gxThread pointer has been initialized 
    // Execute the exit routine at the point where the thread was canceled
    thread->entry->ThreadCleanupHandler(thread);
  }
}

int gxThreadAPIWrapper::apiCancelThread(gxThread_t *thread)
// This function is used to terminate the execution of any thread in 
// a process. Returns a non-zero if the thread cannot be canceled.
{
  if(thread->thread_type == gxTHREAD_TYPE_DETACHED) {
    // The thread was created in a detached state meaning the
    // gxThread_t::thread_id has been released and can no longer
    // be used.
    return 1;
  }
  
  // Get the previous state incase the thread cannot be canceled
  gxThreadState prev_state = thread->thread_state;

  // Set the thread's state here or it will not be set correctly
  // when the thread is canceled.
  thread->thread_state = gxTHREAD_STATE_CANCELED;
    
#if defined (__WIN32__)
  // Suspend the thread if it is currently running
  if(prev_state != gxTHREAD_STATE_SUSPENDED) {
    DWORD rv = ::SuspendThread(thread->thread_id);
    if(rv == (DWORD)-1) {
      thread->thread_error = gxTHREAD_CANCEL_ERROR;
      thread->thread_state = prev_state;
      return 1;
    }
  }
  
  // Execute the thread's cleanup handler
  apiThreadCleanupHandler((void *)(thread));
  
#elif defined (__POSIX__)
  int rv = pthread_cancel(thread->thread_id);
  if(rv != 0) {
    thread->thread_error = gxTHREAD_CANCEL_ERROR;
    thread->thread_state = prev_state;
    return rv;
  }
#else
#error You must define a native API: __WIN32__ or __UNIX__
#endif
  return 0; // No errors were reported
}

int gxThreadAPIWrapper::apiDestroyThread(gxThread_t *thread, int check_state)
// Destroy the specified thread. Returns a non-zero value if
// the thread cannot be destroyed.
{
  if(!thread) return 0;
  
  if(check_state) { // Evaluate the thread's state before destruction
    switch(thread->thread_state) {
      case gxTHREAD_STATE_INVALID :
	// Invalid state, so close the thread and delete the pointer
	break;
      case gxTHREAD_STATE_CANCELED :
	// This thread has exited due to a cancel call
	break;
      case gxTHREAD_STATE_EXITED :
	// This thread has exited
	break;
      case gxTHREAD_STATE_NEW :
	// Newly created thread that was never executed
	// Lets assume that is not intended to be executed
	// at this point.
	break;
      case gxTHREAD_STATE_RUNNING :
	// Cancel a joinable thread if it is currently running
	if(thread->thread_type == gxTHREAD_TYPE_DETACHED) break;
	if(apiCancelThread(thread) != 0) return 1;
	break; 
      case gxTHREAD_STATE_SUSPENDED :
	// Cancel a joinable thread if it is currently suspended
	if(thread->thread_type == gxTHREAD_TYPE_DETACHED) break;
	if(apiCancelThread(thread) != 0) return 1;
	break;
      case gxTHREAD_STATE_WAITING :
	// Cancel a joinable thread if it is currently waiting
	if(thread->thread_type == gxTHREAD_TYPE_DETACHED) break;
	if(apiCancelThread(thread) != 0) return 1;
	break;
      default:
	break;
    }
  }

  // NOTE: The apiCloseThread() function must be used
  // to close WIN32 threads created in a detached state.
#if defined (__WIN32__)
  if(apiCloseThread(thread) != 0) return 1;
#endif

  // Delete the thread pointer
  delete thread;
  thread = 0;

  return 0; // No errors were reported
}

int gxThreadAPIWrapper::apiThreadKeyCreate(gxThreadKey &key)
// Create a thread-specific data key to index a local storage space.
// A thread local storage key is an index, which can be used by multiple
// threads of the same process for global storage that is private to a
// thread. Returns a non-zero value if any errors occur.
{
#if defined (__WIN32__)
  key = TlsAlloc();
  if(key == 0xFFFFFFFF) return 1;

#elif defined (__HPUX10__) && defined (__POSIX__)
// HPUX 10.20 Common Multithread Architecture (CMA) services
  int rv = pthread_keycreate(&key, 0);
  if(rv != 0) return rv;

#elif defined (__POSIX__)
  int rv = pthread_key_create(&key, 0);
  if(rv != 0) return rv;

#else
#error You must define a native API: __WIN32__ or __UNIX__
#endif

  return 0; // No errors were reported
}

int gxThreadAPIWrapper::apiThreadKeyDelete(gxThreadKey &key)
// Releases a thread local storage key. NOTE: It is the responsibility 
// of the application to free any allocated dynamic storage associated
// with this key prior to calling this function. Returns a non-zero value
// if any errors occur.
{
#if defined (__WIN32__)
  if(!TlsFree(key)) return 1;
  return 0;

#elif defined (__HPUX10__) && defined (__POSIX__)
// HPUX 10.20 Common Multithread Architecture (CMA) services
  return 0; // No CMA delete key function is defined

#elif defined (__POSIX__)
  int rv = pthread_key_delete(key);
  if(rv != 0) return rv;
  return 0;

#else
#error You must define a native API: __WIN32__ or __UNIX__
#endif
}

int gxThreadAPIWrapper::apiThreadSetSpecific(gxThreadKey &key,
					     const void *value)
// Stores a value in the calling thread's thread local storage slot for
// the specified thread key. Returns a non-zero value if any errors occur.
{
#if defined (__WIN32__)
  if(!TlsSetValue(key, (LPVOID)(value))) return 1;


#elif defined (__HPUX10__) && defined (__POSIX__)
// HPUX 10.20 Common Multithread Architecture (CMA) services
  int rv = pthread_setspecific(key, (pthread_addr_t)value);
  if(rv != 0) return rv;
  
#elif defined (__POSIX__)
  int rv = pthread_setspecific(key, value);
  if(rv != 0) return rv;

#else
#error You must define a native API: __WIN32__ or __UNIX__
#endif

  return 0; // No errors were reported
}

void *gxThreadAPIWrapper::apiThreadGetSpecific(gxThreadKey &key)
// Returns the value currently bound to the specified key for the
// calling thread. Returns a non-zero value if an error occurrs.
{
#if defined (__WIN32__)
  return (void *)TlsGetValue(key);


#elif defined (__HPUX10__) && defined (__POSIX__)
// HPUX 10.20 Common Multithread Architecture (CMA) services
  void **value;
  pthread_getspecific(key, (pthread_addr_t *)value);
  return *(value);

#elif defined (__POSIX__)
  return pthread_getspecific(key);

#else
#error You must define a native API: __WIN32__ or __UNIX__
#endif
}

int gxThreadAPIWrapper:: apiSetThreadPriority(gxThread_t *thread)
// Set or change the priority and scheduling policy of the a thread
// in accordance with gxThread_t::thread_priority and thread_class
// members. Returns a non-zero value if the priority or policy can
// not be changed. NOTE: Do to cross-platform issues the thread
// priority is no longer set when the thread is created. This
// function must be used to change a thread priority after it has
// been created. 
{
  if(thread->thread_type == gxTHREAD_TYPE_DETACHED) {
    // The thread was created in a detached state meaning the
    // gxThread_t::thread_id has been released and can no longer
    // be used.
    return 1;
  }

#if defined (__WIN32__)
  // Set the thread priority. NOTE: The WIN32 scheduling policy is determined
  // by the priority class of its process and will not be set here. Every
  // thread has a base priority level determined by the thread's priority
  // value and the priority class of its process. The system uses the base
  // priority level of all executable threads to determine which thread gets
  // the next slice of CPU time. WIN32 threads are scheduled in a round-robin
  // fashion at each priority level, and only when there are no executable
  // threads at a higher level will scheduling of threads at a lower level
  // take place. 

  // Set the threads priority and ignore the thread's scheduling policy 
  switch(thread->thread_priority) {
    case gxTHREAD_PRIORITY_HIGH :
      if(!SetThreadPriority(thread->thread_id, THREAD_PRIORITY_HIGHEST)) {
	thread->thread_error = gxTHREAD_PRIORITY_ERROR;
	thread->thread_priority = gxTHREAD_PRIORITY_NORMAL;
	return 1;
      }
      break;
    case gxTHREAD_PRIORITY_LOW :
      if(!SetThreadPriority(thread->thread_id, THREAD_PRIORITY_LOWEST)) {
	thread->thread_error = gxTHREAD_PRIORITY_ERROR;
	thread->thread_priority = gxTHREAD_PRIORITY_NORMAL;
	return 1;
      }
      break;
    default:
      // Default to normal priority
      thread->thread_priority = gxTHREAD_PRIORITY_NORMAL;
      if(!SetThreadPriority(thread->thread_id, THREAD_PRIORITY_NORMAL)) {
	thread->thread_error = gxTHREAD_PRIORITY_ERROR;
	return 1;
      }
      break;
  }

#elif defined (__HPUX10__) && defined (__POSIX__)
  // HPUX 10.20 Common Multithread Architecture (CMA) services

  int policy, prio, min_prio, max_prio;
  switch(thread->thread_priority_class) {
    case gxTHREAD_PRIORITY_CLASS_OTHER:
      policy = SCHED_OTHER;
      max_prio = PRI_OTHER_MAX;
      min_prio = PRI_OTHER_MIN;
      break;
    case gxTHREAD_PRIORITY_CLASS_FIFO:
      policy = SCHED_FIFO;
      max_prio = PRI_FIFO_MAX;
      min_prio = PRI_FIFO_MIN;
      break;
    case gxTHREAD_PRIORITY_CLASS_RR:
      policy = SCHED_RR;
      max_prio = PRI_RR_MAX;
      min_prio = PRI_RR_MIN;
      break;
    default:
      policy = SCHED_OTHER;
      max_prio = PRI_OTHER_MAX;
      min_prio = PRI_OTHER_MIN;
      break;
  }

  // Calculate the priority for normal threads requesting to have their 
  // scheduling policy or priority changed.
  int normal_prio = min_prio + (max_prio - min_prio)/2;

  int rv;
  if(thread->thread_priority == gxTHREAD_PRIORITY_HIGH) {
    rv = pthread_setscheduler(thread->thread_id, policy, max_prio);
    prio = max_prio;
  }
  else if(thread->thread_priority == gxTHREAD_PRIORITY_LOW) {
    rv = pthread_setscheduler(thread->thread_id, policy, min_prio);
    prio = min_prio;
  }
  else { // Assume this is a normal priority
    rv = pthread_setscheduler(thread->thread_id, policy, normal_prio);
    prio = normal_prio;
  }

  if(rv == -1) { // Could not set the priority or the policy
    thread->thread_error = gxTHREAD_SCHED_ERROR;
    return 1;
  }

  // Check to ensure that the priority and policy were set correctly 
  int curr_policy = pthread_getscheduler(thread->thread_id);
  int curr_prio = pthread_getprio(thread->thread_id);

  if(curr_policy != policy) {
    thread->thread_error = gxTHREAD_POLICY_ERROR;
    thread->thread_priority = gxTHREAD_PRIORITY_INVALID;
    thread->thread_priority_class = gxTHREAD_PRIORITY_CLASS_INVALID;
    return 1;
  }
 
  if(curr_prio != prio) { 
    thread->thread_error = gxTHREAD_PRIORITY_ERROR;
    thread->thread_priority = gxTHREAD_PRIORITY_INVALID;
    thread->thread_priority_class = gxTHREAD_PRIORITY_CLASS_INVALID;
    return 1;
  }

#elif defined (__POSIX__) && !defined(__SOLARIS__)

  // Set the thread priority if POSIX scheduling are available
#ifdef _POSIX_THREAD_PRIORITY_SCHEDULING
  int policy;
  switch(thread->thread_priority_class) {
    case gxTHREAD_PRIORITY_CLASS_OTHER:
      policy = SCHED_OTHER; // Most UNIX systems only support SCHED_OTHER
      break;
    case gxTHREAD_PRIORITY_CLASS_FIFO:
      policy = SCHED_FIFO;  // First in-first out (FIFO) scheduling policy
      break;
    case gxTHREAD_PRIORITY_CLASS_RR:
      policy = SCHED_RR;    // Round robin scheduling policy
      break;
    default:
      policy = SCHED_OTHER;
      break;
  }

  // Get the priority limits for the specified policy
  int max_prio = sched_get_priority_max(policy);
  int min_prio = sched_get_priority_min(policy);

  if((max_prio == -1) || (min_prio == -1)) {
    thread->thread_error = gxTHREAD_PRIORITY_ERROR;
    return 1;
  }

  if(max_prio == min_prio) {
    // If the min/max limits the priority range cannot be determined
    thread->thread_error = gxTHREAD_PRIORITY_ERROR;
    return 1;
  }

  // Calculate the priority for normal threads requesting to have their 
  // scheduling policy or priority changed.
  int normal_prio = min_prio + (max_prio - min_prio)/2;

  sched_param param;

  if(thread->thread_priority == gxTHREAD_PRIORITY_HIGH) {
    param.sched_priority = max_prio;
  }
  else if(thread->thread_priority == gxTHREAD_PRIORITY_LOW) {
    param.sched_priority = min_prio;
  }
  else { 
    param.sched_priority = normal_prio;
  }

  // Set the priority and the policy at the same time
  if(pthread_setschedparam(thread->thread_id, policy, &param) != 0) {
    thread->thread_error = gxTHREAD_SCHED_ERROR;
    return 1;
  }

  // Check to ensure that the priority and policy were set correctly 
  int curr_policy;
  sched_param curr_param;
  if(pthread_getschedparam(thread->thread_id, &curr_policy, &curr_param) != 0)
    {
      thread->thread_error = gxTHREAD_SCHED_ERROR;
      thread->thread_priority = gxTHREAD_PRIORITY_INVALID;
      thread->thread_priority_class = gxTHREAD_PRIORITY_CLASS_INVALID;
      return 1;
    }
    
  if(curr_policy != policy) {
    thread->thread_error = gxTHREAD_POLICY_ERROR;
    thread->thread_priority = gxTHREAD_PRIORITY_INVALID;
    thread->thread_priority_class = gxTHREAD_PRIORITY_CLASS_INVALID;
    return 1;
  }
 
  if(curr_param.sched_priority != param.sched_priority) {
    thread->thread_error = gxTHREAD_PRIORITY_ERROR;
    thread->thread_priority = gxTHREAD_PRIORITY_INVALID;
    thread->thread_priority_class = gxTHREAD_PRIORITY_CLASS_INVALID;
    return 1;
  }

#endif // _POSIX_THREAD_PRIORITY_SCHEDULING
  
#else
  // Thread priority scheduling is not supported so reset the priority
  // to normal and flag the application with a thread PRIORITY_ERROR.
  if(thread->thread_priority != gxTHREAD_PRIORITY_NORMAL) {
    thread->thread_error = gxTHREAD_PRIORITY_ERROR;
    thread->thread_priority = gxTHREAD_PRIORITY_NORMAL;
    thread->thread_priority_class = gxTHREAD_PRIORITY_CLASS_OTHER;
  }
#endif

  return 0; // No errors were reported
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
