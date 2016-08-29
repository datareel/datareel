// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: thelpers.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
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

The GX thread helpers are a collection of classes and standalone 
functions used to emulate uniform thread synchronization functions
on multiple target platforms. NOTE: None of the data structures and 
functions defined are intended to be used directly. They are used 
by the gxThread synchronization classes to call the correct native 
thread API functions for each supported platform.    

Changes:
==============================================================
11/14/2002: Corrected bug in the gxThreadConditionTimedWait()
function for POSIX threads preventing the pthread_cond_timedwait() 
function from behaving in a predictable manner.

07/18/2003: Modified the WIN32 versions of gxThreadConditionWait() 
and gxThreadConditionTimedWait() to ensure that only threads of the  
same process can make LeaveCriticalSection() calls to ensure that the 
next call to EnterCriticalSection() does not cause the process to 
wait indefinitely. 

07/19/2003: Modified the WIN32 version of gxThreadSemaphoreInit() to 
ensure that only threads of the same process can make LeaveCriticalSection() 
calls to ensure that the next call to  EnterCriticalSection() does not 
cause the process to wait indefinitely. 

09/22/2005: Modified the WIN32 versions of the gxThreadConditionWait() 
and gxThreadConditionTimedWait() functions to test the waiter and
release counts before decrementing.
==============================================================
*/
// ----------------------------------------------------------- //   
#include "gxdlcode.h"

#if defined(__LINUX__)
#include <errno.h>
#endif

#include <string.h>
#include "thelpers.h"

// --------------------------------------------------------------
// Constants used for synchronization status and debug functions
// --------------------------------------------------------------
// NOTE: This array must contain the same number of exceptions as the
// gxMutexError enumeration. 
const int gxMaxMutexExceptionMessages = 15;
const char *gxMutexExceptionMessages[gxMaxMutexExceptionMessages] = {
  "Mutex exception: No exception reported",          // NO_ERROR
  "Mutex exception: Invalid exception code",         // INVALID_CODE
  "Mutex exception: Error destroying attributes",    // ATTR_DESTROY_ERROR
  "Mutex exception: Error initializing attributes",  // ATTR_INIT_ERROR
  "Mutex exception: Error destroying mutex",         // DESTROY_ERROR
  "Mutex exception: Error initializing the mutex",   // INIT_ERROR
  "Mutex exception: Error locking the mutex",        // LOCK_ERROR
  "Mutex exception: Error setting shared attribute", // SET_SHARE_ERROR
  "Mutex exception: Error trying to lock the mutex", // TRY_LOCK_ERROR
  "Mutex exception: Error unlocking the mutex",      // UNLOCK_ERROR
  "Mutex exception: Error mutex lock not recoverable",
  "Mutex exception: Error mutex lock owner is dead",
  "Mutex exception: Try lock mutex busy",
  "Mutex exception: Thread does not own this mutex",
  "Mutex exception: Value specified by mutex is invalid"
};

// NOTE: This array must contain the same number of exceptions as the
// gxCondtionError enumeration. 
const int gxMaxConditionExceptionMessages = 13;
const char *gxConditionExceptionMessages[gxMaxConditionExceptionMessages] = {
  "Condition exception: No exception reported",          // NO_ERROR
  "Condition exception: Invalid exception code",         // INVALID_CODE
  "Condition exception: Error destroying attributes",    // ATTR_DESTROY_ERROR
  "Condition exception: Error initializing attributes",  // ATTR_INIT_ERROR
  "Condition exception: Error broadcasting",             // BROADCAST_ERROR 
  "Condition exception: Error destroying condition",     // DESTROY_ERROR
  "Condition exception: External mutex error",           // EXTERNAL_ERROR
  "Condition exception: Error initializing condition",   // INIT_ERROR
  "Condition exception: Internal mutex error",           // INTERNAL_ERROR
  "Condition exception: Error setting shared attribute", // SET_SHARE_ERROR
  "Condition exception: Error signaling",                // SIGNAL_ERROR
  "Condition exception: Error during a timed waiting",   // TIMED_WAIT_ERROR
  "Condition exception: Error waiting"                   // WAIT_ERROR
};

// NOTE: This array must contain the same number of exceptions as the
// gxSemaphoreError enumeration. 
const int gxMaxSemaphoreExceptionMessages = 7;
const char *gxSemaphoreExceptionMessages[gxMaxSemaphoreExceptionMessages] = {
  "Semaphore exception: No exception reported",          // NO_ERROR
  "Semaphore exception: Invalid exception code",         // INVALID_CODE
  "Semaphore exception: Error destroying semaphore",     // DESTROY_ERROR
  "Semaphore exception: Error initializing semaphore",   // INIT_ERROR
  "Semaphore exception: Internal mutex error",           // INTERNAL_ERROR
  "Semaphore exception: Error posting",                  // POST_ERROR
  "Semaphore exception: Error waiting"                   // WAIT_ERROR;
};
// --------------------------------------------------------------

const char *gxThreadMutexExceptionMessage(gxMutex_t *m)
// Returns a null termainated string that can be used to log
// or print a mutex execption.
{
  int error = (int)m->mutex_error;
  if(error > (gxMaxMutexExceptionMessages-1)) {
    error = gxMUTEX_INVALID_CODE;
  }
  // Find the corresponding message in the exception array
  return gxMutexExceptionMessages[error];
}

const char *gxThreadConditionExceptionMessage(gxCondition_t *c)
// Returns a null termainated string that can be used to log or
// print a condition execption.
{
  int error = (int)c->condition_error;
  if(error > (gxMaxConditionExceptionMessages-1)) {
    error = gxCONDITION_INVALID_CODE;
  }
  // Find the corresponding message in the exception array
  return gxConditionExceptionMessages[error];
}

const char *gxThreadSemaphoreExceptionMessage(gxSemaphore_t *s)
// Returns a null termainated string that can be used to log or
// print a semaphore execption.
{
  int error = (int)s->semaphore_error;
  if(error > (gxMaxSemaphoreExceptionMessages-1)) {
    error = gxSEMAPHORE_INVALID_CODE;
  }
  // Find the corresponding message in the exception array
  return gxSemaphoreExceptionMessages[error];
}

GXDLCODE_API int gxThreadMutexInit(gxMutex_t *m, gxProcessType type)
// Initialize the specified mutex object. By default the process type
// is set to gxPROCESS_PRIVATE. Returns a non-zero value if the mutex
// cannot be initialized.
{
  // The mutex was already initialized so lets avoid any undefined behavior
  if(m->is_initialized) return 0;

  if(type == gxPROCESS_SHARED) m->process_type = gxPROCESS_SHARED;
  else m->process_type = gxPROCESS_PRIVATE;

  m->mutex_error = gxMUTEX_NO_ERROR;
  
#if defined (__WIN32__)
  // Enable multiple processes to get handles of the same mutex
  if(m->process_type == gxPROCESS_SHARED) {
    if(gxThreadMutexAttributeInit(&m->mutex_attribute) != 0) {
      m->mutex_error = gxMUTEX_ATTR_INIT_ERROR;
      m->mutex_attribute = (gxMutexAttribute)0;
    }
    
    // The mutex is not owned when the initial ownership flag is set to false.
    // If the initial ownership flag is TRUE the calling thread requests 
    // immediate ownership of the mutex object.
    m->shared_mutex = CreateMutex(m->mutex_attribute, FALSE, 0);
    
    if(!m->shared_mutex) {
      gxThreadMutexAttributeDestroy(&m->mutex_attribute);
      m->is_initialized = 0;
      m->mutex_error = gxMUTEX_INIT_ERROR;
      return 1;
    }
  }
  else { // Only threads from the same process will be using this mutex
    InitializeCriticalSection(&m->mutex);
  }

#elif defined (__HPUX10__) && defined (__POSIX__)
  int rv = gxThreadMutexAttributeInit(&m->mutex_attribute);
  if(rv != 0) {
    m->is_initialized = 0;
    m->mutex_error = gxMUTEX_ATTR_INIT_ERROR;
    return rv;
  }

  if(m->process_type == gxPROCESS_SHARED) {
    // This UNIX variant does not support shared mutexs
    m->process_type = gxPROCESS_PRIVATE;
    m->mutex_error = gxMUTEX_SET_SHARE_ERROR;
  }

  rv = pthread_mutex_init(&m->mutex, m->mutex_attribute);
  if(rv != 0) { // Could not initialize the mutex
    m->is_initialized = 0;
    m->mutex_error = gxMUTEX_INIT_ERROR;
    return rv;
  }

#elif defined (__POSIX__)
  int rv; // Return value variable
  if(m->process_type == gxPROCESS_SHARED) {
#if defined(PTHREAD_PROCESS_SHARED) && !defined(__LINUX__)
    rv = gxThreadMutexAttributeInit(&m->mutex_attribute);
    if(rv != 0) {
      m->is_initialized = 0;
      m->mutex_error = gxMUTEX_ATTR_INIT_ERROR;
      return rv;
    }

    rv = pthread_mutexattr_setpshared(&m->mutex_attribute, PTHREAD_PROCESS_SHARED);
    if(rv != 0) {
      m->is_initialized = 0;
      gxThreadMutexAttributeDestroy(&m->mutex_attribute);
      m->mutex_error = gxMUTEX_SET_SHARE_ERROR;
      return rv;
    }

    rv = pthread_mutex_init(&m->mutex, &m->mutex_attribute);
    if(rv != 0) {
      m->is_initialized = 0;
      gxThreadMutexAttributeDestroy(&m->mutex_attribute);
      m->mutex_error = gxMUTEX_INIT_ERROR;
      return rv;
    }
#elif defined(__LINUX__)
    rv = gxThreadMutexAttributeInit(&m->mutex_attribute);
    if(rv != 0) {
      m->is_initialized = 0;
      m->mutex_error = gxMUTEX_ATTR_INIT_ERROR;
      return rv;
    }

    rv = pthread_mutexattr_setpshared(&m->mutex_attribute, PTHREAD_PROCESS_SHARED);
    if(rv != 0) {
      m->is_initialized = 0;
      gxThreadMutexAttributeDestroy(&m->mutex_attribute);
      m->mutex_error = gxMUTEX_SET_SHARE_ERROR;
      return rv;
    }

    // 08/26/2016: Using robust mutex for Linux builds
    pthread_mutexattr_setrobust_np(&m->mutex_attribute, PTHREAD_MUTEX_ROBUST_NP);

    rv = pthread_mutex_init(&m->mutex, &m->mutex_attribute);
    if(rv != 0) {
      m->is_initialized = 0;
      gxThreadMutexAttributeDestroy(&m->mutex_attribute);
      m->mutex_error = gxMUTEX_INIT_ERROR;
      return rv;
    }

#else
    // This UNIX variant does not support shared mutexs
    m->process_type = gxPROCESS_PRIVATE;
    m->mutex_error = gxMUTEX_SET_SHARE_ERROR;

    rv = pthread_mutex_init(&m->mutex, (pthread_mutexattr_t*)0);
    if(rv != 0) { // Could not initialize the mutex
      m->is_initialized = 0;
      m->mutex_error = gxMUTEX_INIT_ERROR;
      return rv;
    }
#endif // PTHREAD_PROCESS_SHARED
  }
  else {
#if defined(__LINUX__)
    rv = gxThreadMutexAttributeInit(&m->mutex_attribute);
    if(rv != 0) {
      m->is_initialized = 0;
      m->mutex_error = gxMUTEX_ATTR_INIT_ERROR;
      return rv;
    }
    
    rv = pthread_mutexattr_setpshared(&m->mutex_attribute, PTHREAD_PROCESS_PRIVATE);
    if(rv != 0) {
      m->is_initialized = 0;
      gxThreadMutexAttributeDestroy(&m->mutex_attribute);
      m->mutex_error = gxMUTEX_SET_SHARE_ERROR;
      return rv;
    }
    
    // 08/26/2016: Using robust mutex for Linux builds
    pthread_mutexattr_setrobust_np(&m->mutex_attribute, PTHREAD_MUTEX_ROBUST_NP);
    
    rv = pthread_mutex_init(&m->mutex, &m->mutex_attribute);
    if(rv != 0) {
      m->is_initialized = 0;
      gxThreadMutexAttributeDestroy(&m->mutex_attribute);
      m->mutex_error = gxMUTEX_INIT_ERROR;
      return rv;
    }
#else
    rv = pthread_mutex_init(&m->mutex, (pthread_mutexattr_t*)0);
    if(rv != 0) { // Could not initialize the mutex
      m->is_initialized = 0;
      m->mutex_error = gxMUTEX_INIT_ERROR;
      return rv;
    }
#endif
  }
#else
#error You must define an API: __WIN32__ or __POSIX__
#endif

  if(m->mutex_error != gxMUTEX_NO_ERROR) return 1;
  return 0;
}

GXDLCODE_API int gxThreadMutexDestroy(gxMutex_t *m)
// Destroy the specified mutex object. Returns a non-zero value
// if the mutex cannot be destroyed.
{
  // The mutex was never initialized so lets avoid any undefined behavior
  if(!m->is_initialized) return 0;
  
#if defined (__WIN32__)
  if(m->process_type == gxPROCESS_SHARED) {
    if(gxThreadMutexAttributeDestroy(&m->mutex_attribute) != 0) {
      m->mutex_error = gxMUTEX_ATTR_DESTROY_ERROR;
    }
    if(!CloseHandle(m->shared_mutex)) {
      m->mutex_error = gxMUTEX_DESTROY_ERROR;
      return 1;
    }
  }
  else {
    DeleteCriticalSection(&m->mutex); 
  }
#elif defined (__POSIX__)
  if(m->process_type == gxPROCESS_SHARED) {
    if(gxThreadMutexAttributeDestroy(&m->mutex_attribute) != 0) {
      m->mutex_error = gxMUTEX_ATTR_DESTROY_ERROR;
    }
  }

  pthread_mutex_unlock(&m->mutex);
  int rv = pthread_mutex_destroy(&m->mutex);
  if(rv != 0) {
    m->mutex_error = gxMUTEX_DESTROY_ERROR;
    return rv;
  }
#else
#error You must define an API: __WIN32__ or __POSIX__
#endif

  m->process_type = gxPROCESS_PRIVATE;
  m->is_initialized = 0;

  if(m->mutex_error != gxMUTEX_NO_ERROR) return 1;
  return 0;
}

GXDLCODE_API int gxThreadMutexAttributeInit(gxMutexAttribute *attr)
// Initialize the specified mutex attribute. Returns a
// non-zero value if the attribute cannot be initialized.
{
#if defined (__WIN32__)
  // NOTE: The security descriptor must be NULL for Windows 95/98 applications.
  // The security descriptor is a pointer to a SECURITY_ATTRIBUTES structure
  // that determines whether the returned handle can be inherited by child
  // processes. The handle cannot be inherited if the security descriptor is
  // a NULL value. 
  *(attr) = (gxMutexAttribute)0;

#elif defined (__HPUX10__) && defined (__POSIX__)
  // HPUX 10.20 Common Multithread Architecture (CMA) services
  int rv = pthread_mutexattr_create(attr);
  if(rv != 0) return rv;

#elif defined (__POSIX__)
  // Initialize the POSIX mutex attribute with the default settings.
  int rv = pthread_mutexattr_init(attr);
  if(rv != 0) return rv;

#else
#error You must define an API: __WIN32__ or __POSIX__
#endif
  return 0;
}

GXDLCODE_API int gxThreadMutexAttributeDestroy(gxMutexAttribute *attr)
// Destroy the specified mutex attribute. Returns a non-zero
// value if the attribute cannot be destroyed.
{
#if defined (__WIN32__)
  // Nothing to do for a null security descriptor
  if(*(attr) == (gxMutexAttribute)0) return 0; 

#elif defined (__HPUX10__) && defined (__POSIX__)
  // HPUX 10.20 Common Multithread Architecture (CMA) services
  int rv = pthread_mutexattr_delete(attr);
  if(rv != 0) return rv;

#elif defined (__POSIX__)
  int rv = pthread_mutexattr_destroy(attr);
  if(rv != 0) return rv;

#else
#error You must define an API: __WIN32__ or __POSIX__
#endif
  return 0;
}

GXDLCODE_API int gxThreadMutexLock(gxMutex_t *m)
// Lock the specified mutex. Returns a non-zero value if the
// mutex cannot be locked.
{
#if defined (__WIN32__)
  if(m->process_type == gxPROCESS_SHARED) {
    // The function's time-out interval never elapses 
    DWORD rv = WaitForSingleObject(m->shared_mutex, INFINITE);
    if(rv == WAIT_FAILED) {
      m->mutex_error = gxMUTEX_LOCK_ERROR;
      return 1;
    }
  }
  else {
    EnterCriticalSection(&m->mutex);
  }
#elif defined (__POSIX__)
  int rv = pthread_mutex_lock(&m->mutex);
  if(rv != 0) {
    switch(rv) {
      case EOWNERDEAD:
	m->mutex_error = gxMUTEX_OWNERDEAD_ERROR;
	break;
      case gxMUTEX_NOTRECOVERABLE_ERROR:
	m->mutex_error = gxMUTEX_NOTRECOVERABLE_ERROR;
	break;
      default:
	m->mutex_error = gxMUTEX_LOCK_ERROR;
	break;
    }
    return rv;
  }
#else
#error You must define an API: __WIN32__ or __POSIX__
#endif
  return 0;
}

GXDLCODE_API int gxThreadMutexUnlock(gxMutex_t *m)
// Unlock the specified mutex. Returns a non-zero value if the
// mutex cannot be unlocked.
{
#if defined (__WIN32__)  
  if(m->process_type == gxPROCESS_SHARED) {
    // The ReleaseMutex function fails if the calling thread 
    // does not own the mutex object. A thread gets ownership 
    // of a mutex by specifying a handle of the mutex in one 
    // of the wait functions.  
    int rv = ReleaseMutex(m->shared_mutex);
    if(!rv) {
      m->mutex_error = gxMUTEX_UNLOCK_ERROR;
      return 1;
    }
  }
  else {
    LeaveCriticalSection(&m->mutex);
  }
#elif defined (__POSIX__)
  // If a thread attempts to unlock a mutex that it has not 
  // locked or a mutex which is unlocked, undefined behavior 
  // results. 
  int rv = pthread_mutex_unlock(&m->mutex);
  if(rv != 0) {
    switch(rv) {
      case EPERM:
	m->mutex_error = gxMUTEX_PERM_ERROR;
	break;
      default:
	m->mutex_error = gxMUTEX_UNLOCK_ERROR;
	break;
    }
    return rv;
  }
#else
#error You must define an API: __WIN32__ or __POSIX__
#endif
  return 0;
}

GXDLCODE_API int gxThreadMutexMakeConsistent(gxMutex_t *m)
{
#if defined(__POSIX__) && defined(__LINUX__)
  int rv = pthread_mutex_consistent_np(&m->mutex);
  if(rv == EINVAL) {
    m->mutex_error = gxMUTEX_INVAL_ERROR;
  }
  if(rv == 0) m->mutex_error = gxMUTEX_NO_ERROR; 
  return rv;
#else
  return 0;
#endif
}

GXDLCODE_API int gxThreadMutexTryLock(gxMutex_t *m)
// Test the mutex before locking it. Returns a non-zero
// value if the mutex cannot be locked.
{
#if defined (__WIN32__)
  if(m->process_type == gxPROCESS_SHARED) {
    // Test the object's state and return immediately
    DWORD rv = WaitForSingleObject(m->shared_mutex, 0);
    if(rv == WAIT_TIMEOUT || rv == WAIT_ABANDONED) {
      m->mutex_error = gxMUTEX_TRY_LOCK_ERROR;
      return 1;
    }
  }
  else {
    LeaveCriticalSection(&m->mutex);
  }
#elif defined (__POSIX__)
  int rv = pthread_mutex_trylock(&m->mutex);
  if(rv != 0) {
    switch(rv) {
      case EOWNERDEAD:
        m->mutex_error = gxMUTEX_OWNERDEAD_ERROR;
        break;
      case gxMUTEX_NOTRECOVERABLE_ERROR:
        m->mutex_error = gxMUTEX_NOTRECOVERABLE_ERROR;
	break;
      case EBUSY:
        m->mutex_error = gxMUTEX_BUSY_ERROR;
        break;
      default:
	m->mutex_error = gxMUTEX_LOCK_ERROR;
	break;
    }
    return rv;
  }
#else
#error You must define an API: __WIN32__ or __POSIX__
#endif
  return 0;
}

GXDLCODE_API int gxThreadConditionInit(gxCondition_t *c, gxProcessType type)
// Standalone function used to initialize a gxCondition_t object.
// Returns a non-zero value if any errors occurs during
// initialization.
{
  // The condition was already initialized so avoid any undefined behavior
  if(c->is_initialized) return 0;

  if(type == gxPROCESS_SHARED) c->process_type = gxPROCESS_SHARED;
  else c->process_type = gxPROCESS_PRIVATE;

  c->condition_error = gxCONDITION_NO_ERROR;

#if defined (__WIN32__)
  c->waiters_count = 0;
  c->wait_generation_count = 0;
  c->release_count = 0;

  if(gxThreadConditionAttributeInit(&c->condition_attribute) != 0) {
    c->condition_error = gxCONDITION_ATTR_INIT_ERROR;
    c->condition_attribute = (gxConditionAttribute)0;
  }

  // Create a manual-reset event.
  c->event = CreateEvent(c->condition_attribute, TRUE, FALSE, 0);
  if(!c->event) {
    c->is_initialized = 0;
    c->condition_error = gxCONDITION_INIT_ERROR;
    return 1; // Could not the create event
  }

  if(gxThreadMutexInit(&c->waiters_count_lock, type) != 0) {
    // A mutex error occurred while initializing a shared condition 
    c->condition_error = gxCONDITION_INTERNAL_ERROR;
  }

#elif defined (__HPUX10__) && defined (__POSIX__)
  int rv = gxThreadConditionAttributeInit(&c->condition_attribute);
  if(rv != 0) {
    c->is_initialized = 0;
    c->condition_error = gxCONDITION_ATTR_INIT_ERROR;
    return rv;
  }

  if(c->process_type == gxPROCESS_SHARED) {
    // This UNIX variant does not support shared mutexs
    c->process_type = gxPROCESS_PRIVATE;
    c->condition_error = gxCONDITION_SET_SHARE_ERROR;
  }

  rv = pthread_cond_init(&c->condition, c->condition_attribute);
  if(rv != 0) { // Could not initialize the mutex
    c->is_initialized = 0;
    c->condition_error = gxCONDITION_INIT_ERROR;
    return rv;
  }

#elif defined (__POSIX__)
  int rv; // Return value variable
  if(c->process_type == gxPROCESS_SHARED) {
#if defined(PTHREAD_PROCESS_SHARED) && !defined(__LINUX__)
    rv = gxThreadConditionAttributeInit(&c->condition_attribute);
    if(rv != 0) {
      c->is_initialized = 0;
      c->condition_error = gxCONDITION_ATTR_INIT_ERROR;
      return rv;
    }

    rv = pthread_condattr_setpshared(&c->condition_attribute,
				      PTHREAD_PROCESS_SHARED);
    if(rv != 0) {
      c->is_initialized = 0;
      gxThreadConditionAttributeDestroy(&c->condition_attribute);
      c->condition_error = gxCONDITION_SET_SHARE_ERROR;
      return rv;
    }
    
    rv = pthread_cond_init(&c->condition, &c->condition_attribute);
    if(rv != 0) {
      c->is_initialized = 0;
      gxThreadConditionAttributeDestroy(&c->condition_attribute);
      c->condition_error = gxCONDITION_INIT_ERROR;
      return rv;
    }
#else
    // This UNIX variant does not support shared condition variables
    c->process_type = gxPROCESS_PRIVATE;
    c->condition_error = gxCONDITION_SET_SHARE_ERROR;
    rv = pthread_cond_init(&c->condition, (pthread_condattr_t *)0);
    if(rv != 0) { // Could not initialize the mutex
      c->is_initialized = 0;
      c->condition_error = gxCONDITION_INIT_ERROR;
      return rv;
    }
#endif // PTHREAD_PROCESS_SHARED
  }
  else {
    rv = pthread_cond_init(&c->condition, (pthread_condattr_t *)0);
    if(rv != 0) { // Could not initialize the condition variable
      c->is_initialized = 0;
      c->condition_error = gxCONDITION_INIT_ERROR;
      return rv;
    }
  }
#else
#error You must define an API: __WIN32__ or __POSIX__
#endif
  if(c->condition_error != gxCONDITION_NO_ERROR) return 1;
  return 0;
}

GXDLCODE_API int gxThreadConditionDestroy(gxCondition_t *c)
// Standalone function used to destroy a gxCondition_t object. 
// Returns a non-zero value if any errors occur.
{
  // The condition was never initialized so avoid any undefined behavior
  if(!c->is_initialized) return 0;
  
#if defined (__WIN32__)
  if(gxThreadConditionAttributeDestroy(&c->condition_attribute) != 0) {
    c->condition_error = gxCONDITION_ATTR_DESTROY_ERROR;
  }

  if(!CloseHandle(c->event)) {
    c->condition_error = gxCONDITION_DESTROY_ERROR;
    return 1;
  }
  
  c->waiters_count = 0;
  c->wait_generation_count = 0;
  c->release_count = 0;
  
  if(gxThreadMutexDestroy(&c->waiters_count_lock) != 0) {
    // A mutex error occurred while destroying a shared condition 
    c->condition_error = gxCONDITION_INTERNAL_ERROR;
  }
  
#elif defined (__POSIX__)
  if(c->process_type == gxPROCESS_SHARED) {
    if(gxThreadConditionAttributeDestroy(&c->condition_attribute) != 0) {
      c->condition_error = gxCONDITION_ATTR_DESTROY_ERROR;
    }
  }
  
  int rv = pthread_cond_destroy(&c->condition);
  if(rv != 0) {
    c->condition_error = gxCONDITION_DESTROY_ERROR;
    return rv;
  }
#else
#error You must define an API: __WIN32__ or __POSIX__
#endif

  c->process_type = gxPROCESS_PRIVATE;
  c->is_initialized = 0;
  if(c->condition_error != gxCONDITION_NO_ERROR) return 1;
  return 0;
}

GXDLCODE_API int gxThreadConditionWait(gxCondition_t *c, gxMutex_t *external_mutex)
// Standalone function used to block a thread from its own execution.
// Returns a non-zero value if any errors occur.
{
#if defined (__WIN32__)
  // Avoid race conditions.
  if(gxThreadMutexLock(&c->waiters_count_lock) != 0)
    c->condition_error = gxCONDITION_INTERNAL_ERROR;

  // Increment count of waiters.
  c->waiters_count++;

  // Store current generation in our activation record.
  int my_generation = c->wait_generation_count;

  if(gxThreadMutexUnlock(&c->waiters_count_lock) != 0)
    c->condition_error = gxCONDITION_INTERNAL_ERROR;

  if(external_mutex->process_type == gxPROCESS_SHARED) {
    if(gxThreadMutexUnlock(external_mutex) != 0) {
      c->condition_error = gxCONDITION_EXTERNAL_ERROR;
    }
  }

  for(;;) {
    // Wait until the event is signaled.
    DWORD rv = WaitForSingleObject(c->event, INFINITE);
    if(rv == WAIT_FAILED) {
      c->condition_error = gxCONDITION_WAIT_ERROR;
      return 1;
    }
    
    if(gxThreadMutexLock(&c->waiters_count_lock) != 0)
      c->condition_error = gxCONDITION_INTERNAL_ERROR;
    // Exit the loop when the event is signaled and
    // there are still waiting threads from this wait_generation
    // that haven't been released from this wait yet.
    int wait_done = c->release_count > 0
                    && c->wait_generation_count != my_generation;
    if(gxThreadMutexUnlock(&c->waiters_count_lock) != 0)
      c->condition_error = gxCONDITION_INTERNAL_ERROR;

    if(wait_done) break;
  }

  if(external_mutex->process_type == gxPROCESS_SHARED) {
    if(gxThreadMutexLock(external_mutex) != 0) {
      c->condition_error = gxCONDITION_EXTERNAL_ERROR;
    }
  }

  if(gxThreadMutexLock(&c->waiters_count_lock) != 0)
    c->condition_error = gxCONDITION_INTERNAL_ERROR;

  // 09/22/2005: Test before decrement
  if(c->waiters_count > 0) c->waiters_count--;
  if(c->release_count > 0) c->release_count--;

  if(gxThreadMutexUnlock(&c->waiters_count_lock) != 0)
    c->condition_error = gxCONDITION_INTERNAL_ERROR;

  if(c->release_count == 0) {
    // Last waiter to be notified, so reset the manual event.
    if(!ResetEvent(c->event)) {
      c->condition_error = gxCONDITION_WAIT_ERROR;
      return 1;
    }
  }
#elif defined (__POSIX__)
  int rv = pthread_cond_wait(&c->condition, &external_mutex->mutex);
  if(rv != 0) {
    c->condition_error = gxCONDITION_WAIT_ERROR;
    return rv;
  }
#else
#error You must define an API: __WIN32__ or __POSIX__
#endif

  if(c->condition_error != gxCONDITION_NO_ERROR) return 1;
  return 0;
}

GXDLCODE_API int gxThreadConditionSignal(gxCondition_t *c)
// Standalone function used to wake up a thread waiting on the
// specified condition. Returns a non-zero value if any errors
// occur.
{
#if defined (__WIN32__)
  if(gxThreadMutexLock(&c->waiters_count_lock) != 0)
    c->condition_error = gxCONDITION_INTERNAL_ERROR;
  if(c->waiters_count > c->release_count) {
    // Signal the manual-reset event
    if(!SetEvent(c->event)) {
      c->condition_error = gxCONDITION_SIGNAL_ERROR;
      return 1;
    } 
    c->release_count++;
    c->wait_generation_count++;
  }
  if(gxThreadMutexUnlock(&c->waiters_count_lock) != 0)
    c->condition_error = gxCONDITION_INTERNAL_ERROR;
#elif defined (__POSIX__)
  int rv = pthread_cond_signal(&c->condition);
  if(rv != 0) {
    c->condition_error = gxCONDITION_SIGNAL_ERROR;
    return rv;
  }
#else
#error You must define an API: __WIN32__ or __POSIX__
#endif

  if(c->condition_error != gxCONDITION_NO_ERROR) return 1;
  return 0;
}

GXDLCODE_API int gxThreadConditionBroadcast(gxCondition_t *c)
// Standalone function used to wake up all threads waiting on the
// specified condition. Returns a non-zero value if any errors
// occur.
{
#if defined (__WIN32__)
  if(gxThreadMutexLock(&c->waiters_count_lock) != 0)
    c->condition_error = gxCONDITION_INTERNAL_ERROR;
  if(c->waiters_count > 0) {  
    if(!SetEvent(c->event)) {
      c->condition_error = gxCONDITION_BROADCAST_ERROR;
      return 1;
    } 

    // Release all the threads in this generation.
    c->release_count = c->waiters_count;

    // Start a new generation.
    c->wait_generation_count++;
  }
  if(gxThreadMutexUnlock(&c->waiters_count_lock) != 0)
    c->condition_error = gxCONDITION_INTERNAL_ERROR;
#elif defined (__POSIX__)
  int rv = pthread_cond_broadcast(&c->condition);
  if(rv != 0) {
    c->condition_error = gxCONDITION_BROADCAST_ERROR;
    return rv;
  }   
#else
#error You must define an API: __WIN32__ or __POSIX__
#endif
  if(c->condition_error != gxCONDITION_NO_ERROR) return 1;
  return 0;
}

GXDLCODE_API int gxThreadConditionTimedWait(gxCondition_t *c, 
					    gxMutex_t *external_mutex,
					    unsigned long sec, 
					    unsigned long nsec)
// Standalone function used to block a thread from its own execution until a 
// signal is raised or the timeout value elapses. Returns a non-zero value if 
// any errors occur.
{
#if defined (__WIN32__)
  // Avoid race conditions.
  if(gxThreadMutexLock(&c->waiters_count_lock) != 0)
    c->condition_error = gxCONDITION_INTERNAL_ERROR;

  DWORD delay = (DWORD)(sec*1000 + nsec/1000000);

  // Increment count of waiters.
  c->waiters_count++;

  // Store current generation in our activation record.
  int my_generation = c->wait_generation_count;

  if(gxThreadMutexUnlock(&c->waiters_count_lock) != 0)
    c->condition_error = gxCONDITION_INTERNAL_ERROR;

  if(external_mutex->process_type == gxPROCESS_SHARED) {
    if(gxThreadMutexUnlock(external_mutex) != 0)
      c->condition_error = gxCONDITION_EXTERNAL_ERROR;
  }

  for(;;) {
    // Wait until the event is signaled.
    DWORD rv = WaitForSingleObject(c->event, delay);
    if(rv == WAIT_FAILED) {
      c->condition_error = gxCONDITION_TIMED_WAIT_ERROR;
      return 1;
    }
    else if(rv == WAIT_TIMEOUT || rv == WAIT_ABANDONED) {
      // WAIT_TIMEOUT indicates that the  time-out interval elapsed, 
      // and the object's state is nonsignaled.

      // WAIT_ABANDONED	indicated that the specified object is a mutex
      // object that was not released by the thread that owned the mutex
      // object before the owning thread terminated. Ownership of the mutex
      // object is granted to the calling thread, and the mutex is set to
      // nonsignaled.

      // WAIT_OBJECT_0 indicated that the state of the specified object is
      // signaled.

      break;
    }
    
    if(gxThreadMutexLock(&c->waiters_count_lock) != 0)
      c->condition_error = gxCONDITION_INTERNAL_ERROR;
    // Exit the loop when the event is signaled and
    // there are still waiting threads from this wait_generation
    // that haven't been released from this wait yet.
    int wait_done = c->release_count > 0
                    && c->wait_generation_count != my_generation;
    if(gxThreadMutexUnlock(&c->waiters_count_lock) != 0)
      c->condition_error = gxCONDITION_INTERNAL_ERROR;

    if(wait_done) break;
  }

  if(external_mutex->process_type == gxPROCESS_SHARED) {
    if(gxThreadMutexLock(external_mutex) != 0)
      c->condition_error = gxCONDITION_EXTERNAL_ERROR;
  }

  if(gxThreadMutexLock(&c->waiters_count_lock) != 0)
    c->condition_error = gxCONDITION_INTERNAL_ERROR;

  // 09/22/2005: Test before decrement
  if(c->waiters_count > 0) c->waiters_count--;
  if(c->release_count > 0) c->release_count--;

  if(gxThreadMutexUnlock(&c->waiters_count_lock) != 0)
    c->condition_error = gxCONDITION_INTERNAL_ERROR;

  if(c->release_count == 0) {
    // Last waiter to be notified, so reset the manual event.
    if(!ResetEvent(c->event)) {
      c->condition_error = gxCONDITION_TIMED_WAIT_ERROR;
      return 1;
    }
  }

#elif defined (__POSIX__)
  timespec delay;
  memset(&delay, 0, sizeof(delay));
  delay.tv_sec = time(0)+sec;
  delay.tv_nsec = nsec;
  int rv = pthread_cond_timedwait(&c->condition,
				  &external_mutex->mutex,
				  &delay);
  if(rv != 0) {
    c->condition_error = gxCONDITION_TIMED_WAIT_ERROR;
    return rv;
  }
#else
#error You must define an API: __WIN32__ or __POSIX__
#endif
  if(c->condition_error != gxCONDITION_NO_ERROR) return 1;
  return 0;
}

GXDLCODE_API int gxThreadConditionAttributeInit(gxConditionAttribute *attr)
// Initialize a condition attribute. Returns a non-zero value
// if the attribute cannot be initialized.
{
#if defined (__WIN32__)
  // NOTE: The security descriptor must be NULL for Windows 95/98 applications.
  // The security descriptor is a pointer to a SECURITY_ATTRIBUTES structure
  // that determines whether the returned handle can be inherited by child
  // processes. The handle cannot be inherited if the security descriptor is
  // a NULL value. 
  *(attr) = (gxConditionAttribute)0;

#elif defined (__HPUX10__) && defined (__POSIX__)
  // HPUX 10.20 Common Multithread Architecture (CMA) services
  int rv = pthread_condattr_create(attr);
  if(rv != 0) return rv;

#elif defined (__POSIX__)
  // Initialize the POSIX condition attribute with the default settings.
  int rv = pthread_condattr_init(attr);
  if(rv != 0) return rv;

#else
#error You must define an API: __WIN32__ or __POSIX__
#endif
  return 0;
}

GXDLCODE_API int gxThreadConditionAttributeDestroy(gxConditionAttribute *attr)
// Destroy a condition attribute. Returns a non-zero value if
// the attribute cannot be destroyed.
{
#if defined (__WIN32__)
  // Nothing to do for a null security descriptor
  if(*(attr) == (gxConditionAttribute)0) return 0; 

#elif defined (__HPUX10__) && defined (__POSIX__)
  // HPUX 10.20 Common Multithread Architecture (CMA) services
  int rv = pthread_condattr_delete(attr);
  if(rv != 0) return rv;

#elif defined (__POSIX__)
  int rv = pthread_condattr_destroy(attr);
  if(rv != 0) return rv;

#else
#error You must define an API: __WIN32__ or __POSIX__
#endif
  return 0;
}

GXDLCODE_API int gxThreadSemaphoreInit(gxSemaphore_t *s, gxProcessType type)
// Initialize the semaphore. Returns a non-zero value if
// any errors occur.
{
  // The semaphore was already initialized so
  // lets avoid any undefined behavior.
  if(s->is_initialized) return 0;

  int rv = gxThreadConditionInit(&s->semaphore, type);
  if(rv != 0) {
    s->semaphore_error = gxSEMAPHORE_INIT_ERROR;
    s->is_initialized = 0;
    return rv;
  }

#if defined (__WIN32__)
  rv = gxThreadMutexInit(&s->mutex, gxPROCESS_SHARED);
#else 
  rv = gxThreadMutexInit(&s->mutex, type);
#endif

  if(rv != 0) {
    // Partially initialized
    s->semaphore_error = gxSEMAPHORE_INTERNAL_ERROR;
    return rv;
  }

  s->is_initialized = 1;
  s->value = 1; // All semaphores are initialized with a value of one
  return 0;
}

GXDLCODE_API int gxThreadSemaphoreDestroy(gxSemaphore_t *s)
// Destroy the semaphore. Returns a non-zero value if
// any errors occur.
{
  // The semaphore was never initialized so
  // lets avoid any undefined behavior.
  if(!s->is_initialized) return 0;

  int rv = gxThreadConditionDestroy(&s->semaphore);
  if(rv != 0) {
    s->semaphore_error = gxSEMAPHORE_DESTROY_ERROR;
    return 1;
  }
  rv = gxThreadMutexDestroy(&s->mutex);
  if(rv != 0) {
    // Partially destroyed
    s->semaphore_error = gxSEMAPHORE_INTERNAL_ERROR;
  }

  s->is_initialized = 0;
  s->value = 1;

  if(s->semaphore_error != gxSEMAPHORE_NO_ERROR) return 1;
  return 0;
}

GXDLCODE_API int gxThreadSemaphoreValue(gxSemaphore_t *s)
// Returns the value of the semaphore at the time the
// critical section is accessed. NOTE: The value may
// change after the function unlocks the critical
// section
{
  return s->value_after_operation;
}

GXDLCODE_API int gxThreadSemaphoreWait(gxSemaphore_t *s)
// Decrements the semaphore and blocks if the semaphore
// value is zero until another thread signals a change.
// Returns a non-zero value if any errors occur.
{
  if(gxThreadMutexLock(&s->mutex) != 0)
    s->semaphore_error = gxSEMAPHORE_INTERNAL_ERROR;

  while(s->value <= 0 ) {
    int rv = gxThreadConditionWait(&s->semaphore, &s->mutex);
    if(rv != 0) {
      s->semaphore_error = gxSEMAPHORE_WAIT_ERROR;
      return rv;
    }
  }
  s->value--;
  s->value_after_operation = s->value;

  if(gxThreadMutexUnlock(&s->mutex) != 0)
    s->semaphore_error = gxSEMAPHORE_INTERNAL_ERROR;

  if(s->semaphore_error != gxSEMAPHORE_NO_ERROR) return 1;
  return 0;
}

GXDLCODE_API int gxThreadSemaphorePost(gxSemaphore_t *s)
// Increments the semaphore and signals any threads that are blocked.
// Returns a non-zero value if any errors occur.
{
  if(gxThreadMutexLock(&s->mutex) != 0)
    s->semaphore_error = gxSEMAPHORE_INTERNAL_ERROR;
  s->value++;
  s->value_after_operation = s->value;
  if(gxThreadMutexUnlock(&s->mutex) != 0)
    s->semaphore_error = gxSEMAPHORE_INTERNAL_ERROR;

  int rv = gxThreadConditionSignal(&s->semaphore);
  if(rv != 0) {
    s->semaphore_error = gxSEMAPHORE_POST_ERROR;
    return rv;
  }

  if(s->semaphore_error != gxSEMAPHORE_NO_ERROR) return 1;
  return 0;
}

GXDLCODE_API int gxThreadSemaphoreDecrement(gxSemaphore_t *s)
// This is a non-blocking function that decrements the value of the
// semaphore. It allows threads to decrement the semaphore to some
// negative value as part of an initialization process. Decrements
// allow multiple threads to move up on a semaphore before another
// thread can go down. Returns a non-zero value if an error occurs.
{
  if(gxThreadMutexLock(&s->mutex) != 0)
    s->semaphore_error = gxSEMAPHORE_INTERNAL_ERROR;
  s->value--;
  s->value_after_operation = s->value;
  if(gxThreadMutexUnlock(&s->mutex) != 0)
    s->semaphore_error = gxSEMAPHORE_INTERNAL_ERROR;

  if(s->semaphore_error != gxSEMAPHORE_NO_ERROR) return 1;
  return 0;
}

GXDLCODE_API int gxThreadSemaphoreIncrement(gxSemaphore_t *s)
// This is a non-blocking function that increments
// the value of the semaphore. Returns a non-zero
// value if an errors occur.
{
  if(gxThreadMutexLock(&s->mutex) != 0)
    s->semaphore_error = gxSEMAPHORE_INTERNAL_ERROR;
  s->value++;
  s->value_after_operation = s->value;
  if(gxThreadMutexUnlock(&s->mutex) != 0)
    s->semaphore_error = gxSEMAPHORE_INTERNAL_ERROR;

  if(s->semaphore_error != gxSEMAPHORE_NO_ERROR) return 1;
  return 0;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
