// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: thelpers.h
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 03/25/2000
// Date Last Modified: 08/28/2016
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

The GX thread helpers are a collection of classes and standalone 
functions used to emulate uniform thread synchronization functions
on multiple target platforms. NOTE: None of the data structures and 
functions defined are intended to be used directly. They are used 
by the gxThread synchronization classes to call the correct native 
thread API functions for each supported platform.    
*/
// ----------------------------------------------------------- //   
#ifndef __GX_THREAD_HELPER_FUNCTIONS_HPP__
#define __GX_THREAD_HELPER_FUNCTIONS_HPP__

#include "gxdlcode.h"

#include "thrtypes.h"

// --------------------------------------------------------------
// Type definitions
// --------------------------------------------------------------
#if defined (__WIN32__)
typedef LPSECURITY_ATTRIBUTES gxMutexAttribute; 
typedef LPSECURITY_ATTRIBUTES gxConditionAttribute;

#elif defined (__HPUX10__) && defined (__POSIX__)
// HPUX 10.20 Common Multithread Architecture (CMA) services
// POSIX 1003.4 interface
typedef cma_t_attr gxMutexAttribute;
typedef cma_t_attr gxConditionAttribute;

#elif defined (__POSIX__)
// POSIX 1003.1c 1995 thread standard
typedef pthread_mutexattr_t gxMutexAttribute;
typedef pthread_condattr_t gxConditionAttribute;

#else
#error You must define an API: __WIN32__ or __POSIX___
#endif
// --------------------------------------------------------------

// --------------------------------------------------------------
// Enumerations
// --------------------------------------------------------------
enum gxMutexError { // Mutex error codes
  gxMUTEX_NO_ERROR = 0, // No errors reported
  gxMUTEX_INVALID_CODE, // Invalid error code

  gxMUTEX_ATTR_DESTROY_ERROR,  // Error destroying attribute
  gxMUTEX_ATTR_INIT_ERROR,     // Error initializing the attribute
  gxMUTEX_DESTROY_ERROR,       // Error destroying mutex
  gxMUTEX_INIT_ERROR,          // Error initializing the mutex
  gxMUTEX_LOCK_ERROR,          // Error locking the mutex
  gxMUTEX_SET_SHARE_ERROR,     // Error setting shared attribute
  gxMUTEX_TRY_LOCK_ERROR,      // Error trying to lock the mutex
  gxMUTEX_UNLOCK_ERROR,        // Error unlocking the mutex
  gxMUTEX_NOTRECOVERABLE_ERROR,
  gxMUTEX_OWNERDEAD_ERROR,
  gxMUTEX_BUSY_ERROR,
  gxMUTEX_PERM_ERROR,
  gxMUTEX_INVAL_ERROR
};

enum gxConditionError { // Condition variables error codes
  gxCONDITION_NO_ERROR = 0, // No errors reported
  gxCONDITION_INVALID_CODE, // Invalid error code
  
  gxCONDITION_ATTR_DESTROY_ERROR, // Error destroying attribute
  gxCONDITION_ATTR_INIT_ERROR,    // Error initializing the attribute
  gxCONDITION_BROADCAST_ERROR,    // Error broadcasting
  gxCONDITION_DESTROY_ERROR,      // Error destroying condition
  gxCONDITION_EXTERNAL_ERROR,     // External mutex error
  gxCONDITION_INIT_ERROR,         // Error initializing condition
  gxCONDITION_INTERNAL_ERROR,     // Internal condition error
  gxCONDITION_SET_SHARE_ERROR,    // Error setting shared attribute
  gxCONDITION_SIGNAL_ERROR,       // Error signaling
  gxCONDITION_TIMED_WAIT_ERROR,   // Error during a timed waiting
  gxCONDITION_WAIT_ERROR          // Error waiting
};

enum gxSemaphoreError { // Semaphore error codes
  gxSEMAPHORE_NO_ERROR = 0,   // No errors reported
  gxSEMAPHORE_INVALID_CODE,   // Invalid error code
  gxSEMAPHORE_DESTROY_ERROR,  // Error destroying semaphore
  gxSEMAPHORE_INIT_ERROR,     // Error initializing the semaphore
  gxSEMAPHORE_INTERNAL_ERROR, // Internal mutex error
  gxSEMAPHORE_POST_ERROR,     // Error posting
  gxSEMAPHORE_WAIT_ERROR      // Error waiting
};
// --------------------------------------------------------------

// --------------------------------------------------------------
// Thread synchronization helper classes
// --------------------------------------------------------------
// Data structure used to create shared and single process mutexs
struct GXDLCODE_API gxMutex_t
{
public:
  gxMutex_t() {
    is_initialized = 0; process_type = gxPROCESS_PRIVATE;
    mutex_error = gxMUTEX_NO_ERROR;
  }
  ~gxMutex_t() { }

private: // Disallow copying and assignment
  gxMutex_t(const gxMutex_t &ob) { }
  void operator=(const gxMutex_t &ob) { } 

public:
  gxProcessType process_type;       // Shared or private access
  gxMutexError mutex_error;         // Last error reported
  gxMutexAttribute mutex_attribute; // Mutex attribute
  int is_initialized;               // True if initialized
  
#if defined (__WIN32__)
  // Mutex types that can be shared across multiple processes
  HANDLE shared_mutex; 

  // Mutex type that can only be shared by threads of the same process
  CRITICAL_SECTION mutex;

#elif defined (__POSIX__)
  pthread_mutex_t mutex;

#else
#error You must define an API: __WIN32__ or __POSIX___
#endif
};

// Data structure used to create shared and single process condition variables
struct GXDLCODE_API gxCondition_t
{
public:
  gxCondition_t() {
    is_initialized = 0; process_type = gxPROCESS_PRIVATE;
    condition_error = gxCONDITION_NO_ERROR;
  }
  ~gxCondition_t() { }

private: // Disallow copying and assignment
  gxCondition_t(const gxCondition_t &ob) { }
  void operator=(const gxCondition_t &ob) { }

public:
  gxProcessType process_type;               // Shared or private access
  gxConditionError condition_error;         // Last error reported
  gxConditionAttribute condition_attribute; // Condition attribute
  int is_initialized;                       // True if initialized
  
#if defined (__WIN32__)
  // Members needed to emulate condition variables under WIN32
  int waiters_count;            // Number of threads waiting
  int release_count;            // Number of threads to release
  int wait_generation_count;    // Current generation count
  gxMutex_t waiters_count_lock; // Serialize access to generation count
  HANDLE event; // Event used to block and release waiting threads

#elif defined (__POSIX__)
  pthread_cond_t condition;

#else
#error You must define an API: __WIN32__ or __POSIX___
#endif
};

// Data structure used to create shared and single process semaphores
struct GXDLCODE_API gxSemaphore_t
{
public:
  gxSemaphore_t() {
    is_initialized = 0; process_type = gxPROCESS_PRIVATE;
    semaphore_error = gxSEMAPHORE_NO_ERROR;
  }
  ~gxSemaphore_t() { }

private: // Disallow copying and assignment
  gxSemaphore_t(const gxSemaphore_t &ob) { }
  void operator=(const gxSemaphore_t &ob) { }

public:
  gxProcessType process_type;       // Shared or private access
  gxSemaphoreError semaphore_error; // Last error reported
  int is_initialized;               // True if initialized
  int value;                        // Value during operation
  int value_after_operation;        // Value after an operation
  gxMutex_t mutex;                  // Lock for semaphore variables
  gxCondition_t semaphore;          // Condition used for wait operations
};
// --------------------------------------------------------------

// --------------------------------------------------------------
// Cross-platform standalone synchronization functions
// --------------------------------------------------------------
GXDLCODE_API int gxThreadMutexInit(gxMutex_t *m, 
				   gxProcessType type = gxPROCESS_PRIVATE);
GXDLCODE_API int gxThreadMutexDestroy(gxMutex_t *m);
GXDLCODE_API int gxThreadMutexAttributeInit(gxMutexAttribute *attr);
GXDLCODE_API int gxThreadMutexAttributeDestroy(gxMutexAttribute *attr);
GXDLCODE_API int gxThreadMutexLock(gxMutex_t *m);
GXDLCODE_API int gxThreadMutexUnlock(gxMutex_t *m);
GXDLCODE_API int gxThreadMutexTryLock(gxMutex_t *m);
GXDLCODE_API int gxThreadMutexMakeConsistent(gxMutex_t *m);
GXDLCODE_API int gxThreadConditionInit(gxCondition_t *c,
				       gxProcessType type = gxPROCESS_PRIVATE);
GXDLCODE_API int gxThreadConditionDestroy(gxCondition_t *c);
GXDLCODE_API int gxThreadConditionAttributeInit(gxConditionAttribute *attr);
GXDLCODE_API int gxThreadConditionAttributeDestroy(gxConditionAttribute *attr);
GXDLCODE_API int gxThreadConditionSignal(gxCondition_t *c);
GXDLCODE_API int gxThreadConditionBroadcast(gxCondition_t *c);
GXDLCODE_API int gxThreadConditionWait(gxCondition_t *c, 
				       gxMutex_t *external_mutex);
GXDLCODE_API int gxThreadConditionTimedWait(gxCondition_t *c, 
					    gxMutex_t *external_mutex,
					    unsigned long sec, 
					    unsigned long nsec);
GXDLCODE_API int gxThreadSemaphoreInit(gxSemaphore_t *s,
				       gxProcessType type = gxPROCESS_PRIVATE);
GXDLCODE_API int gxThreadSemaphoreDestroy(gxSemaphore_t *s);
GXDLCODE_API int gxThreadSemaphoreValue(gxSemaphore_t *s);
GXDLCODE_API int gxThreadSemaphoreWait(gxSemaphore_t *s);
GXDLCODE_API int gxThreadSemaphorePost(gxSemaphore_t *s);
GXDLCODE_API int gxThreadSemaphoreDecrement(gxSemaphore_t *s);
GXDLCODE_API int gxThreadSemaphoreIncrement(gxSemaphore_t *s);
// --------------------------------------------------------------

// --------------------------------------------------------------
// Synchronization debug functions
// --------------------------------------------------------------
GXDLCODE_API const char *gxThreadMutexExceptionMessage(gxMutex_t *m);
GXDLCODE_API const char *gxThreadConditionExceptionMessage(gxCondition_t *c);
GXDLCODE_API const char *gxThreadSemaphoreExceptionMessage(gxSemaphore_t *s);
// --------------------------------------------------------------

#endif // __GX_THREAD_HELPER_FUNCTIONS_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //

