// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxmutex.cpp
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

Mutual exclusion class used to provide simple lock primitives
that can be used to control access to a shared resource. A mutex
is used to cause other threads to wait while the thread holding
the mutex executes code in a critical section.
*/
// ----------------------------------------------------------- //   
#include "gxdlcode.h"

#include "gxmutex.h"

gxMutex::gxMutex(gxProcessType type)
{
  MutexInit(type);
}

gxMutex::~gxMutex()
{
  // PC-lint 09/08/2005: Function may throw exception in destructor
  MutexDestroy();
}

int gxMutex::MutexInit(gxProcessType type)
// Initialize the mutex. Returns a non-zero value if the
// mutex cannot be initialized or if any errors occur.
{
  mutex_lock = 0;
  auto_fix_deadlock = 1;
  int rv = gxThreadMutexInit(&mutex, type);
  if(rv != 0) return rv;
  return 0;
}

int gxMutex::MutexDestroy()
// Destroy the mutex. Returns a non-zero value if the
// mutex cannot be destroyed or if any errors occur.
{
  int rv = gxThreadMutexDestroy(&mutex);
  if(rv != 0) return rv;
  mutex_lock = 0;
  return 0;
}

int gxMutex::MutexLock()
// Lock the mutex. If the mutex is already locked, the calling
// thread blocks until the mutex becomes available. Returns a
// non-zero value if the mutex cannot be locked or if any errors
// occur.
{
  mutex_lock++;
  int rv = gxThreadMutexLock(&mutex); // Blocking lock
  if(rv != 0) {
    if((mutex.mutex_error == gxMUTEX_OWNERDEAD_ERROR) && (auto_fix_deadlock == 1)) {
      rv = MakeConsistent(); // We now own this lock
      if(rv != 0) { // Unable to make the state consistent
	gxThreadMutexUnlock(&mutex);
	mutex_lock--;
	return rv;
      }
    }
  }  
  if(rv != 0) mutex_lock--;
  return rv;
}

int gxMutex::MutexUnlock()
// Unlock the mutex. Returns a non-zero value if the
// mutex cannot be unlocked or if any errors occur.
{
  int rv = gxThreadMutexUnlock(&mutex);
  if(rv != 0) return rv;
  if(mutex_lock > 0) mutex_lock--;
  return 0;
}

int gxMutex::MutexTryLock()
// Test the mutex state before locking it. Returns a non-zero if
// any errors occur.
{
  mutex_lock++;
  int rv = gxThreadMutexTryLock(&mutex); // Blocking lock
  if(rv != 0) {
    if((mutex.mutex_error == gxMUTEX_OWNERDEAD_ERROR) && (auto_fix_deadlock == 1)) {
      rv = MakeConsistent(); // We now own this lock
      if(rv != 0) { // Unable to make the state consistent
	gxThreadMutexUnlock(&mutex);  
	mutex_lock--;
	return rv;
      }
    }
  }  
  if(rv != 0) mutex_lock--;
  return rv;
}

const char *gxMutex::MutexExceptionMessage()
// Returns a null terminated string that can
// be use to log or print a mutex exception.
{
  return gxThreadMutexExceptionMessage(&mutex);
}

int gxMutex::DisableAutoFix(int reset) {
  if(reset) { auto_fix_deadlock = 1; return auto_fix_deadlock; }
  auto_fix_deadlock = 0;
  return auto_fix_deadlock;
}

int gxMutex::MakeConsistent() {
  return gxThreadMutexMakeConsistent(&mutex);
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
