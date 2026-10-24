// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: thelpers.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 03/25/2000
// Date Last Modified: 10/24/2026
// Copyright (c) 2001-2026 DataReel Software Development
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

#if defined (__POSIX__) && !defined (__WIN32__)
#include <sys/time.h> // 10/14/2026: gettimeofday() for timed waits
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
  m->not_recoverable = 0; // 10/24/2026
  
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
    // 10/14/2026: Standard name, was pthread_mutexattr_setrobust_np
    // 10/14/2026: The return value was ignored. Fail if it cannot be set.
    rv = pthread_mutexattr_setrobust(&m->mutex_attribute, PTHREAD_MUTEX_ROBUST);
    if(rv != 0) {
      m->is_initialized = 0;
      gxThreadMutexAttributeDestroy(&m->mutex_attribute);
      m->mutex_error = gxMUTEX_ATTR_INIT_ERROR;
      return rv;
    }

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
    // 10/14/2026: Standard name, was pthread_mutexattr_setrobust_np
    // 10/14/2026: The return value was ignored. Fail if it cannot be set.
    rv = pthread_mutexattr_setrobust(&m->mutex_attribute, PTHREAD_MUTEX_ROBUST);
    if(rv != 0) {
      m->is_initialized = 0;
      gxThreadMutexAttributeDestroy(&m->mutex_attribute);
      m->mutex_error = gxMUTEX_ATTR_INIT_ERROR;
      return rv;
    }
    
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

  // 10/14/2026: The mutex was never marked as initialized, so the
  // destroy function never released it and a second init call was
  // not skipped.
  m->is_initialized = 1;

  if(m->mutex_error != gxMUTEX_NO_ERROR) return 1;
  return 0;
}

GXDLCODE_API int gxThreadMutexDestroy(gxMutex_t *m)
// Destroy the specified mutex object. Returns a non-zero value
// if the mutex cannot be destroyed.
{
  // The mutex was never initialized so lets avoid any undefined behavior
  if(!m->is_initialized) return 0;

  // 10/14/2026: Clear any error left by an earlier call. A stale error
  // made this function report a failure after a successful destroy.
  m->mutex_error = gxMUTEX_NO_ERROR;
  
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
#if defined(__LINUX__)
  // 10/14/2026: Linux builds create an attribute for private mutexes
  // too, but it was only destroyed for shared mutexes.
  if(gxThreadMutexAttributeDestroy(&m->mutex_attribute) != 0) {
    m->mutex_error = gxMUTEX_ATTR_DESTROY_ERROR;
  }
#else
  if(m->process_type == gxPROCESS_SHARED) {
    if(gxThreadMutexAttributeDestroy(&m->mutex_attribute) != 0) {
      m->mutex_error = gxMUTEX_ATTR_DESTROY_ERROR;
    }
  }
#endif

  // 10/24/2026: Do not unlock a mutex that is not recoverable. The glibc
  // unlock call can crash on it. See gxMutex_t::not_recoverable.
  if(!m->not_recoverable) pthread_mutex_unlock(&m->mutex);
  m->not_recoverable = 0;
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
#if defined(__POSIX__) && defined(ENOTRECOVERABLE)
  // 10/24/2026: Do not call the native API on a mutex that is not
  // recoverable. See gxMutex_t::not_recoverable.
  if(m->not_recoverable) {
    m->mutex_error = gxMUTEX_NOTRECOVERABLE_ERROR;
    return ENOTRECOVERABLE;
  }
#endif
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
#if defined(ENOTRECOVERABLE)
      // 10/14/2026: Was compared with the gxMUTEX_NOTRECOVERABLE_ERROR
      // enum value instead of the ENOTRECOVERABLE error code.
      case ENOTRECOVERABLE:
	m->mutex_error = gxMUTEX_NOTRECOVERABLE_ERROR;
	m->not_recoverable = 1; // 10/24/2026
	break;
#endif
      default:
	m->mutex_error = gxMUTEX_LOCK_ERROR;
	break;
    }
    return rv;
  }
#else
#error You must define an API: __WIN32__ or __POSIX__
#endif
  // 10/14/2026: Clear any error left by an earlier call. The lock
  // is held here so no other thread is writing this field.
  if(m->mutex_error != gxMUTEX_NO_ERROR) m->mutex_error = gxMUTEX_NO_ERROR;
  return 0;
}

GXDLCODE_API int gxThreadMutexUnlock(gxMutex_t *m)
// Unlock the specified mutex. Returns a non-zero value if the
// mutex cannot be unlocked.
{
  // 10/14/2026: Clear any error left by an earlier call while the
  // calling thread still holds the lock. An error from this unlock
  // call is stored below.
  gxMutexError prev_error = m->mutex_error;
  if(prev_error != gxMUTEX_NO_ERROR) m->mutex_error = gxMUTEX_NO_ERROR;

#if defined(__POSIX__) && defined(ENOTRECOVERABLE)
  // 10/24/2026: A mutex that is not recoverable cannot be held, and the
  // glibc unlock call can crash on it. See gxMutex_t::not_recoverable.
  if(m->not_recoverable) {
    m->mutex_error = gxMUTEX_NOTRECOVERABLE_ERROR;
    return ENOTRECOVERABLE;
  }
#endif

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
  int rv = pthread_mutex_consistent(&m->mutex); // 10/14/2026: Was pthread_mutex_consistent_np
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
#if defined(__POSIX__) && defined(ENOTRECOVERABLE)
  // 10/24/2026: Do not call the native API on a mutex that is not
  // recoverable. See gxMutex_t::not_recoverable.
  if(m->not_recoverable) {
    m->mutex_error = gxMUTEX_NOTRECOVERABLE_ERROR;
    return ENOTRECOVERABLE;
  }
#endif
#if defined (__WIN32__)
  if(m->process_type == gxPROCESS_SHARED) {
    // Test the object's state and return immediately
    DWORD rv = WaitForSingleObject(m->shared_mutex, 0);
    // 10/14/2026: WAIT_ABANDONED gives the lock to the caller, so it
    // is a success as in gxThreadMutexLock(). WAIT_FAILED was treated
    // as a success.
    if(rv == WAIT_TIMEOUT || rv == WAIT_FAILED) {
      m->mutex_error = gxMUTEX_TRY_LOCK_ERROR;
      return 1;
    }
  }
  else {
    // 10/14/2026: Was LeaveCriticalSection(), which released the lock
    // instead of trying to take it.
    if(!TryEnterCriticalSection(&m->mutex)) {
      m->mutex_error = gxMUTEX_TRY_LOCK_ERROR;
      return 1;
    }
  }
#elif defined (__POSIX__)
  int rv = pthread_mutex_trylock(&m->mutex);
  if(rv != 0) {
    switch(rv) {
      case EOWNERDEAD:
        m->mutex_error = gxMUTEX_OWNERDEAD_ERROR;
        break;
#if defined(ENOTRECOVERABLE)
      // 10/14/2026: Was compared with the gxMUTEX_NOTRECOVERABLE_ERROR
      // enum value instead of the ENOTRECOVERABLE error code.
      case ENOTRECOVERABLE:
        m->mutex_error = gxMUTEX_NOTRECOVERABLE_ERROR;
        m->not_recoverable = 1; // 10/24/2026
	break;
#endif
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
  // 10/14/2026: Clear any error left by an earlier call. The lock
  // is held here so no other thread is writing this field.
  if(m->mutex_error != gxMUTEX_NO_ERROR) m->mutex_error = gxMUTEX_NO_ERROR;
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
    // 10/14/2026: Linux was excluded here, so shared condition variables
    // fell back to private ones even though Linux supports them.
#if defined(PTHREAD_PROCESS_SHARED) || defined(__LINUX__)
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

  // 10/14/2026: The condition was never marked as initialized, so the
  // destroy function never released it and a second init call was
  // not skipped.
  c->is_initialized = 1;

  if(c->condition_error != gxCONDITION_NO_ERROR) return 1;
  return 0;
}

GXDLCODE_API int gxThreadConditionDestroy(gxCondition_t *c)
// Standalone function used to destroy a gxCondition_t object. 
// Returns a non-zero value if any errors occur.
{
  // The condition was never initialized so avoid any undefined behavior
  if(!c->is_initialized) return 0;

  // 10/14/2026: Clear any error left by an earlier call. A stale error
  // made this function report a failure after a successful destroy.
  c->condition_error = gxCONDITION_NO_ERROR;
  
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
  // 10/14/2026: Errors from this call are kept in a local variable.
  // The function used to return 1 for any error stored by an earlier
  // call, and that error was never cleared.
  gxConditionError error = gxCONDITION_NO_ERROR;

#if defined (__WIN32__)
  // Avoid race conditions.
  if(gxThreadMutexLock(&c->waiters_count_lock) != 0)
    error = gxCONDITION_INTERNAL_ERROR;

  // Increment count of waiters.
  c->waiters_count++;

  // Store current generation in our activation record.
  int my_generation = c->wait_generation_count;

  if(gxThreadMutexUnlock(&c->waiters_count_lock) != 0)
    error = gxCONDITION_INTERNAL_ERROR;

  // 10/14/2026: The external mutex was only released for shared
  // mutexes. A private mutex stayed locked during the wait, so a
  // thread that locked it before signaling could never get it.
  if(gxThreadMutexUnlock(external_mutex) != 0) {
    error = gxCONDITION_EXTERNAL_ERROR;
  }

  int wait_failed = 0;
  for(;;) {
    // Wait until the event is signaled.
    DWORD rv = WaitForSingleObject(c->event, INFINITE);
    if(rv == WAIT_FAILED) {
      // 10/14/2026: Used to return here without locking the external
      // mutex again and without lowering the waiter count.
      wait_failed = 1;
      break;
    }
    
    if(gxThreadMutexLock(&c->waiters_count_lock) != 0)
      error = gxCONDITION_INTERNAL_ERROR;
    // Exit the loop when the event is signaled and
    // there are still waiting threads from this wait_generation
    // that haven't been released from this wait yet.
    int wait_done = c->release_count > 0
                    && c->wait_generation_count != my_generation;
    if(gxThreadMutexUnlock(&c->waiters_count_lock) != 0)
      error = gxCONDITION_INTERNAL_ERROR;

    if(wait_done) break;
  }

  if(gxThreadMutexLock(external_mutex) != 0) {
    error = gxCONDITION_EXTERNAL_ERROR;
  }

  if(gxThreadMutexLock(&c->waiters_count_lock) != 0)
    error = gxCONDITION_INTERNAL_ERROR;

  // 09/22/2005: Test before decrement
  if(c->waiters_count > 0) c->waiters_count--;
  int last_waiter = 0;
  if(!wait_failed) {
    if(c->release_count > 0) c->release_count--;
    // 10/14/2026: Test the release count while the lock is held
    last_waiter = (c->release_count == 0);
  }

  if(gxThreadMutexUnlock(&c->waiters_count_lock) != 0)
    error = gxCONDITION_INTERNAL_ERROR;

  if(last_waiter) {
    // Last waiter to be notified, so reset the manual event.
    if(!ResetEvent(c->event)) {
      error = gxCONDITION_WAIT_ERROR;
    }
  }
  if(wait_failed) error = gxCONDITION_WAIT_ERROR;
#elif defined (__POSIX__)
  int rv = pthread_cond_wait(&c->condition, &external_mutex->mutex);
  if(rv != 0) {
    c->condition_error = gxCONDITION_WAIT_ERROR;
    return rv;
  }
#else
#error You must define an API: __WIN32__ or __POSIX__
#endif

  if(error != gxCONDITION_NO_ERROR) {
    c->condition_error = error;
    return 1;
  }
  // 10/14/2026: A successful call clears the stored error
  if(c->condition_error != gxCONDITION_NO_ERROR) 
    c->condition_error = gxCONDITION_NO_ERROR;
  return 0;
}

GXDLCODE_API int gxThreadConditionSignal(gxCondition_t *c)
// Standalone function used to wake up a thread waiting on the
// specified condition. Returns a non-zero value if any errors
// occur.
{
  // 10/14/2026: Errors from this call are kept in a local variable.
  // The function used to return 1 for any error stored by an earlier
  // call, and that error was never cleared.
  gxConditionError error = gxCONDITION_NO_ERROR;

#if defined (__WIN32__)
  if(gxThreadMutexLock(&c->waiters_count_lock) != 0)
    error = gxCONDITION_INTERNAL_ERROR;
  if(c->waiters_count > c->release_count) {
    // Signal the manual-reset event
    if(!SetEvent(c->event)) {
      // 10/14/2026: Used to return here with waiters_count_lock locked
      error = gxCONDITION_SIGNAL_ERROR;
    } 
    else {
      c->release_count++;
      c->wait_generation_count++;
    }
  }
  if(gxThreadMutexUnlock(&c->waiters_count_lock) != 0)
    error = gxCONDITION_INTERNAL_ERROR;
#elif defined (__POSIX__)
  int rv = pthread_cond_signal(&c->condition);
  if(rv != 0) {
    c->condition_error = gxCONDITION_SIGNAL_ERROR;
    return rv;
  }
#else
#error You must define an API: __WIN32__ or __POSIX__
#endif

  if(error != gxCONDITION_NO_ERROR) {
    c->condition_error = error;
    return 1;
  }
  // 10/14/2026: A successful call clears the stored error
  if(c->condition_error != gxCONDITION_NO_ERROR) 
    c->condition_error = gxCONDITION_NO_ERROR;
  return 0;
}

GXDLCODE_API int gxThreadConditionBroadcast(gxCondition_t *c)
// Standalone function used to wake up all threads waiting on the
// specified condition. Returns a non-zero value if any errors
// occur.
{
  // 10/14/2026: Errors from this call are kept in a local variable.
  // The function used to return 1 for any error stored by an earlier
  // call, and that error was never cleared.
  gxConditionError error = gxCONDITION_NO_ERROR;

#if defined (__WIN32__)
  if(gxThreadMutexLock(&c->waiters_count_lock) != 0)
    error = gxCONDITION_INTERNAL_ERROR;
  if(c->waiters_count > 0) {  
    if(!SetEvent(c->event)) {
      // 10/14/2026: Used to return here with waiters_count_lock locked
      error = gxCONDITION_BROADCAST_ERROR;
    } 
    else {
      // Release all the threads in this generation.
      c->release_count = c->waiters_count;

      // Start a new generation.
      c->wait_generation_count++;
    }
  }
  if(gxThreadMutexUnlock(&c->waiters_count_lock) != 0)
    error = gxCONDITION_INTERNAL_ERROR;
#elif defined (__POSIX__)
  int rv = pthread_cond_broadcast(&c->condition);
  if(rv != 0) {
    c->condition_error = gxCONDITION_BROADCAST_ERROR;
    return rv;
  }   
#else
#error You must define an API: __WIN32__ or __POSIX__
#endif

  if(error != gxCONDITION_NO_ERROR) {
    c->condition_error = error;
    return 1;
  }
  // 10/14/2026: A successful call clears the stored error
  if(c->condition_error != gxCONDITION_NO_ERROR) 
    c->condition_error = gxCONDITION_NO_ERROR;
  return 0;
}

GXDLCODE_API int gxThreadConditionTimedWait(gxCondition_t *c, 
					    gxMutex_t *external_mutex,
					    unsigned long sec, 
					    unsigned long nsec)
// Standalone function used to block a thread from its own execution until a 
// signal is raised or the timeout value elapses. Returns a non-zero value if 
// any errors occur or the timeout value elapses.
{
  // 10/14/2026: Errors from this call are kept in a local variable.
  // The function used to return 1 for any error stored by an earlier
  // call, and that error was never cleared.
  gxConditionError error = gxCONDITION_NO_ERROR;

#if defined (__WIN32__)
  // Avoid race conditions.
  if(gxThreadMutexLock(&c->waiters_count_lock) != 0)
    error = gxCONDITION_INTERNAL_ERROR;

  DWORD delay = (DWORD)(sec*1000 + nsec/1000000);

  // 10/14/2026: Measure the time already waited. The full delay used
  // to restart each time the thread woke up without being released.
  DWORD start_time = GetTickCount();

  // Increment count of waiters.
  c->waiters_count++;

  // Store current generation in our activation record.
  int my_generation = c->wait_generation_count;

  if(gxThreadMutexUnlock(&c->waiters_count_lock) != 0)
    error = gxCONDITION_INTERNAL_ERROR;

  // 10/14/2026: The external mutex was only released for shared
  // mutexes. A private mutex stayed locked during the wait.
  if(gxThreadMutexUnlock(external_mutex) != 0)
    error = gxCONDITION_EXTERNAL_ERROR;

  int wait_failed = 0;
  int timed_out = 0;
  for(;;) {
    DWORD elapsed = GetTickCount() - start_time;
    DWORD remaining = (elapsed >= delay) ? 0 : (delay - elapsed);

    // Wait until the event is signaled.
    DWORD rv = WaitForSingleObject(c->event, remaining);
    if(rv == WAIT_FAILED) {
      // 10/14/2026: Used to return here without locking the external
      // mutex again and without lowering the waiter count.
      wait_failed = 1;
      break;
    }
    
    if(gxThreadMutexLock(&c->waiters_count_lock) != 0)
      error = gxCONDITION_INTERNAL_ERROR;
    // Exit the loop when the event is signaled and
    // there are still waiting threads from this wait_generation
    // that haven't been released from this wait yet.
    int wait_done = c->release_count > 0
                    && c->wait_generation_count != my_generation;
    if(gxThreadMutexUnlock(&c->waiters_count_lock) != 0)
      error = gxCONDITION_INTERNAL_ERROR;

    if(wait_done) break;

    // WAIT_TIMEOUT indicates that the time-out interval elapsed, 
    // and the object's state is nonsignaled.
    if(rv == WAIT_TIMEOUT || remaining == 0) {
      // 10/14/2026: A thread that timed out used to take one of the
      // releases meant for a signaled thread.
      timed_out = 1;
      break;
    }
  }

  if(gxThreadMutexLock(external_mutex) != 0)
    error = gxCONDITION_EXTERNAL_ERROR;

  if(gxThreadMutexLock(&c->waiters_count_lock) != 0)
    error = gxCONDITION_INTERNAL_ERROR;

  // 09/22/2005: Test before decrement
  if(c->waiters_count > 0) c->waiters_count--;
  int last_waiter = 0;
  if(!wait_failed && !timed_out) {
    if(c->release_count > 0) c->release_count--;
    // 10/14/2026: Test the release count while the lock is held
    last_waiter = (c->release_count == 0);
  }

  if(gxThreadMutexUnlock(&c->waiters_count_lock) != 0)
    error = gxCONDITION_INTERNAL_ERROR;

  if(last_waiter) {
    // Last waiter to be notified, so reset the manual event.
    if(!ResetEvent(c->event)) {
      error = gxCONDITION_TIMED_WAIT_ERROR;
    }
  }

  // 10/14/2026: A timeout used to return 0, so the caller could not
  // tell it from a signal. It is now reported as under POSIX.
  if(wait_failed || timed_out) error = gxCONDITION_TIMED_WAIT_ERROR;

#elif defined (__POSIX__)
  // 10/14/2026: The deadline was time(0)+sec seconds with "nsec" used
  // as the absolute nanosecond field. The wait could end up to one
  // second early, and an nsec value of one second or more failed with
  // EINVAL without waiting. The deadline is now the current time plus
  // sec seconds plus nsec nanoseconds.
  struct timeval now;
  gettimeofday(&now, 0);
  unsigned long total_nsec = ((unsigned long)now.tv_usec * 1000UL) + 
    (nsec % 1000000000UL);
  timespec delay;
  memset(&delay, 0, sizeof(delay));
  delay.tv_sec = now.tv_sec + (time_t)sec + (time_t)(nsec / 1000000000UL) +
    (time_t)(total_nsec / 1000000000UL);
  delay.tv_nsec = (long)(total_nsec % 1000000000UL);
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

  if(error != gxCONDITION_NO_ERROR) {
    c->condition_error = error;
    return 1;
  }
  // 10/14/2026: A successful call clears the stored error
  if(c->condition_error != gxCONDITION_NO_ERROR) 
    c->condition_error = gxCONDITION_NO_ERROR;
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

  // 10/14/2026: Clear any error left by an earlier call and record the
  // process type, which was never set.
  s->semaphore_error = gxSEMAPHORE_NO_ERROR;
  if(type == gxPROCESS_SHARED) s->process_type = gxPROCESS_SHARED;
  else s->process_type = gxPROCESS_PRIVATE;

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

  // 10/14/2026: Was not set, so gxThreadSemaphoreValue() returned an
  // uninitialized value until the first operation.
  s->value_after_operation = s->value;
  return 0;
}

GXDLCODE_API int gxThreadSemaphoreDestroy(gxSemaphore_t *s)
// Destroy the semaphore. Returns a non-zero value if
// any errors occur.
{
  // The semaphore was never initialized so
  // lets avoid any undefined behavior.
  if(!s->is_initialized) return 0;

  // 10/14/2026: Clear any error left by an earlier call. A stale error
  // made this function report a failure after a successful destroy.
  s->semaphore_error = gxSEMAPHORE_NO_ERROR;

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
  s->value_after_operation = s->value;
  s->process_type = gxPROCESS_PRIVATE;

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

// 10/14/2026: In the semaphore functions below, errors from the current
// call are kept in a local variable and stored while the semaphore lock
// is held. A successful call now clears the stored error. These functions
// used to return 1 for any error stored by an earlier call.

GXDLCODE_API int gxThreadSemaphoreWait(gxSemaphore_t *s)
// Decrements the semaphore and blocks if the semaphore
// value is zero until another thread signals a change.
// Returns a non-zero value if any errors occur.
{
  gxSemaphoreError error = gxSEMAPHORE_NO_ERROR;
  if(gxThreadMutexLock(&s->mutex) != 0)
    error = gxSEMAPHORE_INTERNAL_ERROR;

  while(s->value <= 0 ) {
    int rv = gxThreadConditionWait(&s->semaphore, &s->mutex);
    if(rv != 0) {
      s->semaphore_error = gxSEMAPHORE_WAIT_ERROR;
      // 10/14/2026: Used to return with the semaphore mutex locked
      gxThreadMutexUnlock(&s->mutex);
      return rv;
    }
  }
  s->value--;
  s->value_after_operation = s->value;
  s->semaphore_error = error;

  if(gxThreadMutexUnlock(&s->mutex) != 0) {
    error = gxSEMAPHORE_INTERNAL_ERROR;
    s->semaphore_error = error;
  }

  if(error != gxSEMAPHORE_NO_ERROR) return 1;
  return 0;
}

GXDLCODE_API int gxThreadSemaphorePost(gxSemaphore_t *s)
// Increments the semaphore and signals any threads that are blocked.
// Returns a non-zero value if any errors occur.
{
  gxSemaphoreError error = gxSEMAPHORE_NO_ERROR;
  if(gxThreadMutexLock(&s->mutex) != 0)
    error = gxSEMAPHORE_INTERNAL_ERROR;
  s->value++;
  s->value_after_operation = s->value;
  s->semaphore_error = error;
  if(gxThreadMutexUnlock(&s->mutex) != 0) {
    error = gxSEMAPHORE_INTERNAL_ERROR;
    s->semaphore_error = error;
  }

  int rv = gxThreadConditionSignal(&s->semaphore);
  if(rv != 0) {
    s->semaphore_error = gxSEMAPHORE_POST_ERROR;
    return rv;
  }

  if(error != gxSEMAPHORE_NO_ERROR) return 1;
  return 0;
}

GXDLCODE_API int gxThreadSemaphoreDecrement(gxSemaphore_t *s)
// This is a non-blocking function that decrements the value of the
// semaphore. It allows threads to decrement the semaphore to some
// negative value as part of an initialization process. Decrements
// allow multiple threads to move up on a semaphore before another
// thread can go down. Returns a non-zero value if an error occurs.
{
  gxSemaphoreError error = gxSEMAPHORE_NO_ERROR;
  if(gxThreadMutexLock(&s->mutex) != 0)
    error = gxSEMAPHORE_INTERNAL_ERROR;
  s->value--;
  s->value_after_operation = s->value;
  s->semaphore_error = error;
  if(gxThreadMutexUnlock(&s->mutex) != 0) {
    error = gxSEMAPHORE_INTERNAL_ERROR;
    s->semaphore_error = error;
  }

  if(error != gxSEMAPHORE_NO_ERROR) return 1;
  return 0;
}

GXDLCODE_API int gxThreadSemaphoreIncrement(gxSemaphore_t *s)
// This is a non-blocking function that increments the value of
// the semaphore and wakes a thread waiting on the semaphore.
// Returns a non-zero value if an errors occur.
{
  gxSemaphoreError error = gxSEMAPHORE_NO_ERROR;
  if(gxThreadMutexLock(&s->mutex) != 0)
    error = gxSEMAPHORE_INTERNAL_ERROR;
  s->value++;
  s->value_after_operation = s->value;
  s->semaphore_error = error;
  if(gxThreadMutexUnlock(&s->mutex) != 0) {
    error = gxSEMAPHORE_INTERNAL_ERROR;
    s->semaphore_error = error;
  }

  // 10/14/2026: Waiting threads were not signaled, so a thread blocked
  // in gxThreadSemaphoreWait() stayed blocked after the value went up.
  int rv = gxThreadConditionSignal(&s->semaphore);
  if(rv != 0) {
    s->semaphore_error = gxSEMAPHORE_POST_ERROR;
    return rv;
  }

  if(error != gxSEMAPHORE_NO_ERROR) return 1;
  return 0;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
