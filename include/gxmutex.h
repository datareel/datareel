// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxmutex.h
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

Mutual exclusion class used to provide simple lock primitives
that can be used to control access to a shared resource. A mutex
is used to cause other threads to wait while the thread holding
the mutex executes code in a critical section.
*/
// ----------------------------------------------------------- //   
#ifndef __GX_MUTEX_HPP__
#define __GX_MUTEX_HPP__

#include "gxdlcode.h"

#include "thrtypes.h"
#include "thelpers.h"

class GXDLCODE_API gxMutex
{
public:
  gxMutex(gxProcessType type = gxPROCESS_PRIVATE); 
  ~gxMutex();

private: // Disallow copying and assignment
  gxMutex(const gxMutex &ob) { } 
  void operator=(const gxMutex &ob) { }

public:
  int MutexInit(gxProcessType type = gxPROCESS_PRIVATE);
  int MutexDestroy();
  int MutexLock();
  int MutexUnlock();
  int MutexTryLock();
  int IsLocked() { return mutex_lock > 0; }
  int NumLocks() { return mutex_lock; }
  gxMutex_t *GetMutex() { return &mutex; }
  gxMutexError GetMutexError() { return mutex.mutex_error; }
  gxProcessType GetMutexProcessType() { return mutex.process_type; }

  // 08/28/2016: Added to work with Linux robust mutexs
  // Do not attempt to fix a dead lock, leave it to application
  // If reset, set back to auto fix
  // Returns the autofix setting after set or reset
  int DisableAutoFix(int reset = 0); 

  // Attempt to put robust mutex in consistent state
  // Return 0 if pass, or errno value if fail
  int MakeConsistent(); 
                        
public: // Mutex status/debug message functions
  const char *MutexExceptionMessage();

private:
  int mutex_lock;
  gxMutex_t mutex;
  int auto_fix_deadlock; // If robust mutex, try to fix deadlock
};

#endif // __GX_MUTEX_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
  

