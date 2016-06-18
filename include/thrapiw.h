// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: thrapiw.h
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

The gxThreadAPIWrapper class is used to call native thread API 
functions for each specified platform. NOTE: This class is not 
intended to be used directly by an application or a derived 
class. It is used by the gxThread base class to call the correct 
native thread API function for each of the supported platforms.

Changes:
==============================================================
11/14/2002: Changed argument type from int to unsigned long in 
the gxThreadAPIWrapper::api_mSleep() and 
gxThreadAPIWrapper::api_sSleep() functions.

11/14/2002: Added the gxThreadAPIWrapper::api_uSleep() and 
gxThreadAPIWrapper::api_nSleep() functions used to sleep for a 
specified number of microseconds and nanoseconds.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_THREAD_API_WRAPPER_HPP__
#define __GX_THREAD_API_WRAPPER_HPP__

#include "gxdlcode.h"

#include "thrtypes.h"

// Foward class calling conventions
class gxThread_t; // Thread class used to store thread variables
class gxThread;   // Base class used to create threaded objects 

class GXDLCODE_API gxThreadAPIWrapper
{
public:
  gxThreadAPIWrapper() { }   
  ~gxThreadAPIWrapper() { }

private: // Disallow copying and assignment
  gxThreadAPIWrapper(const gxThreadAPIWrapper &ob) { }
  void operator=(const gxThreadAPIWrapper &ob) { }

public: // Native thread API functions
  int apiInitThreadAttribute(gxThreadAttribute *attr);
  int apiDestroyThreadAttribute(gxThreadAttribute *attr);
  int apiCreateThread(gxThread_t *thread);
  int apiJoinThread(gxThread_t *thread);
  int apiSuspendThread(gxThread_t *thread);
  int apiResumeThread(gxThread_t *thread);
  int apiCancelThread(gxThread_t *thread);
  int apiCloseThread(gxThread_t *thread);
  int apiDestroyThread(gxThread_t *thread, int check_state = 1);
  void *apiExitThread(gxThread_t *thread, int exit_code);
  int apiSetThreadPriority(gxThread_t *thread);
  int apiThreadKeyCreate(gxThreadKey &key);
  int apiThreadKeyDelete(gxThreadKey &key);
  int apiThreadSetSpecific(gxThreadKey &key, const void *value);
  void *apiThreadGetSpecific(gxThreadKey &key);

public: // Timer functions
  void api_mSleep(unsigned long  milliseconds);
  void api_sSleep(unsigned long seconds);
  void api_uSleep(unsigned long microseconds);
  void api_nSleep(unsigned long nanoseconds);

public: // Cross-platform thread entry points
#if defined (__WIN32__)
  // NOTE: The *arg pointer will be cast to gxThread_t type
  static DWORD apiThreadStartRoutine(void *arg);
#elif defined (__POSIX__)
  // NOTE: The *arg pointer will be cast to gxThread_t type
  static void *apiThreadStartRoutine(void *arg);
#else
#error You must define an API: __WIN32__ or __POSIX__
#endif

public: // Thread cleanup handler
  // NOTE: The *arg pointer will be cast to gxThread_t type
  static void apiThreadCleanupHandler(void *arg);
};

#endif // __GX_THREAD_API_WRAPPER_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
