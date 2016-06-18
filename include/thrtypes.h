// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: thrtypes.h
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

This include file contains cross-platform type definitions and
enumerations used throughout the GX thread library.

The following is a list of conditional directives used for cross-
compiler/cross platform compilation. NOTE: These directives must be
defined in project makefile with the "-D" compiler option.

__WIN32__ - WIN32 directive for Windows 95/98/NT applications.
__HAS__BEGINTHREADEX__ - Create thread using the _beginthreadex() C RTL call

__UNIX__    - Generic UNIX directive. Must be defined for all UNIX systems.
__POSIX__   - Define for applications using POSIX threads.
__HPUX10__  - Define for all HPUX 10.20 applications.
__SOLARIS__ - Solaris directive. Must be defined for Solaris or POSIX threads.

The following is a list of conditional directives reserved for future use
in the GX thread library:

__MSVC__    - Microsoft Visual C++ 4.2 and higher compiler specifics.
__BCC32__   - Borland BCC 4.0 and higher compiler specifics.
__MFC__     - MFC directive for Windows 95/98/NT MFC applications.
__HPUX__    - HPUX directive defined for all versions of HPUX.
__HPUX11__  - Define for all HPUX 11.0 and higher applications.
__LINUX__   - Linux directive. Defined for Linux or POSIX threads.
__LINUX_THREADS__   - Define for applications using Linux threads.
__SOLARIS_THREADS__ - Define for applications using Solaris threads.
*/
// ----------------------------------------------------------- //   
#ifndef __GX_THREAD_TYPES_HPP__
#define __GX_THREAD_TYPES_HPP__

#include "gxdlcode.h"

// --------------------------------------------------------------
// Platform specific include files
// --------------------------------------------------------------
// Generic UNIX include files
#if defined (__UNIX__)
#include <unistd.h>
#endif

// Thread specific native API include files
#if defined (__WIN32__)
#include <windows.h> 
#include <process.h>

#elif defined (__SOLARIS__) && defined (__POSIX__)
#ifndef __EXTENSIONS__
#define __EXTENSIONS__
#endif
#include <pthread.h>
#include <sched.h>

#elif defined (__HPUX__) && defined (__POSIX__)
#ifndef _INCLUDE_POSIX1C_SOURCE
#define _INCLUDE_POSIX1C_SOURCE
#endif
#include <pthread.h>
#include <sched.h>

#elif defined (__POSIX__) // Generic UNIX OS with POSIX compliant extensions
#include <pthread.h>
#include <sched.h>

#else
#error You must define an API: __WIN32__ or __POSIX__ and a UNIX variant
#endif
// --------------------------------------------------------------

// --------------------------------------------------------------
// Native thread API specific type definitions
// --------------------------------------------------------------
#if defined (__WIN32__)
typedef HANDLE gxThreadID;        // WIN32 thread handle
typedef DWORD gxStackSizeType;    // Type used to specify a stack size
typedef DWORD gxThreadExitCode;   // Type used to hold a thread's exit code
typedef DWORD gxThreadKey;        // Key type for thread-specific values
typedef LPSECURITY_ATTRIBUTES gxThreadAttribute; // Security attributes

#elif defined (__HPUX10__) && defined (__POSIX__)
// HPUX 10.20 Common Multithread Architecture (CMA) services
// POSIX 1003.4 interface
typedef cma_t_attr gxThreadAttribute; // CMA thread attribute type
typedef cma_t_thread gxThreadID; // Type used to identify a thread
typedef long gxStackSizeType;    // Type used to specify a stack size
typedef int gxThreadExitCode;    // Type used to hold a thread's exit code
typedef cma_t_key gxThreadKey;   // Key type for thread-specific values

#elif defined (__POSIX__)
// POSIX 1003.1c 1995 thread standard
typedef pthread_attr_t gxThreadAttribute; // POSIX thread attribute type
typedef pthread_t gxThreadID;      // Type used to identify a thread
typedef unsigned gxStackSizeType;  // Type used to specify a stack size
typedef int gxThreadExitCode;      // Type used to hold a thread's exit code
typedef pthread_key_t gxThreadKey; // Key type for thread-specific values
 
#else
#error You must define an API: __WIN32__ or __POSIX__
#endif
// --------------------------------------------------------------

// --------------------------------------------------------------
// Type definitions common to all platforms
// --------------------------------------------------------------
typedef long gxThreadObjectID; // Type used for object IDs
typedef long gxThreadClassID;  // Type used for class IDs
// --------------------------------------------------------------

// --------------------------------------------------------------
// Function pointers used for type casting thread start routines
// --------------------------------------------------------------
#if defined (__WIN32__)
typedef unsigned (__stdcall *gxThreadStartRoutine)(void *arg);

#elif defined (__HPUX10__) && defined (__POSIX__)
// HPUX 10.20 Common Multithread Architecture (CMA) services
typedef cma_t_start_routine gxThreadStartRoutine;

#elif defined (__POSIX__)
typedef void *(*gxThreadStartRoutine)(void *arg); 

#else
#error You must define an API: __WIN32__ or __POSIX__
#endif
// --------------------------------------------------------------

// --------------------------------------------------------------
// Enumerations
// --------------------------------------------------------------
enum gxThreadState { // Current thread state
  gxTHREAD_STATE_INVALID = 0,  // The current state of this thread is unknown
  gxTHREAD_STATE_CANCELED,     // Thread has been canceled
  gxTHREAD_STATE_EXITED,       // Thread has exited
  gxTHREAD_STATE_NEW,          // Newly created thread
  gxTHREAD_STATE_RUNNING,      // Thread is currently running
  gxTHREAD_STATE_SUSPENDED,    // Thread has been suspended
  gxTHREAD_STATE_WAITING       // Execution is blocked waiting for a signal 
};

enum gxThreadPriority { // Thread priority settings
  gxTHREAD_PRIORITY_INVALID = 0, // The priority of this thread is invalid
  gxTHREAD_PRIORITY_LOW,         // Move to the bottom of the priority chain
  gxTHREAD_PRIORITY_NORMAL,      // Add to the priority chain (default)
  gxTHREAD_PRIORITY_HIGH         // Move to the top of the priority chain
};

enum gxThreadPriorityClass { // Thread scheduling policies
  gxTHREAD_PRIORITY_CLASS_INVALID = 0, // The priority class is invalid

  // POSIX scheduling policies. NOTE: The WIN32 scheduling policy is
  // determined by the priority class of its process. None of these
  // policies have any effect under WIN32. 
  gxTHREAD_PRIORITY_CLASS_OTHER, // Another scheduling policy (default policy)
  gxTHREAD_PRIORITY_CLASS_FIFO,  // First in-first out (FIFO) scheduling policy
  gxTHREAD_PRIORITY_CLASS_RR     // Round robin scheduling policy
};

enum gxThreadError { // Thread error codes
  gxTHREAD_NO_ERROR = 0, // No errors reported
  gxTHREAD_INVALID_CODE, // Invalid thread error code
  
  gxTHREAD_CANCEL_ERROR,   // Error canceling thread
  gxTHREAD_CLOSE_ERROR,    // Error closing thread
  gxTHREAD_CREATE_ERROR,   // Error creating thread
  gxTHREAD_RUNNING_ERROR,  // Thread is already running
  gxTHREAD_EXECUTE_ERROR,  // Thread cannot be executed 
  gxTHREAD_POLICY_ERROR,   // Cannot retrieve thread scheduling policy
  gxTHREAD_PRIORITY_ERROR, // Error setting the thread priority
  gxTHREAD_RESUME_ERROR,   // Error resuming thread
  gxTHREAD_SCHED_ERROR,    // Scheduling error
  gxTHREAD_SCOPE_ERROR,    // Contention scope error
  gxTHREAD_STACK_ERROR,    // Error setting the stack size
  gxTHREAD_STATE_ERROR,    // Could not set the thread state
  gxTHREAD_SUSPEND_ERROR   // Error suspending thread 
};

enum gxThreadType { // Thread types
  gxTHREAD_TYPE_DETACHED, // System reclaims resources when thread terminates
  gxTHREAD_TYPE_JOINABLE
};

enum gxProcessType { // Process types
  gxPROCESS_PRIVATE = 0, // Resources are available only to a single process
  gxPROCESS_SHARED  = 1  // Resources are available to multiple processes
};
// --------------------------------------------------------------

#endif // __GX_THREAD_TYPES_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
