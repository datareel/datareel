// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxsema.cpp
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

Semaphore synchronization class implemented using condition variables
and a mutually exclusive integer variable. Typically, an application
uses a semaphore to limit the number of threads using a resource by
causing a thread to stop and wait until another thread signals that it
has arrived at a certain place in the entry function.

Changes:
==============================================================
02/11/2002: Modified the gxSemaphore::SemaphoreDecrement() to 
correctly decrement the semaphore.
==============================================================
*/
// ----------------------------------------------------------- //   
#include "gxdlcode.h"

#include "gxsema.h"

gxSemaphore::gxSemaphore(gxProcessType type)
{
  SemaphoreInit(type);
}

gxSemaphore::~gxSemaphore()
{
  // PC-lint 09/08/2005: Function may throw exception in destructor
  SemaphoreDestroy();
}

int gxSemaphore::SemaphoreInit(gxProcessType type)
// Initialize the semaphore. Returns a non-zero value if
// any errors occur.
{
  if(gxThreadSemaphoreInit(&semaphore, type) != 0) return 1;
  return 0;
}

int gxSemaphore::SemaphoreDestroy()
// Destroy the semaphore. Returns a non-zero value if
// any errors occur.
{
  return gxThreadSemaphoreDestroy(&semaphore);
}

int gxSemaphore::SemaphoreWait()
// Decrement the semaphore and block if the semaphore
// value is zero until another thread signals a change.
// Returns a non-zero value if any errors occur.
{
  return gxThreadSemaphoreWait(&semaphore);
}

int gxSemaphore::SemaphorePost()
// Increments the semaphore and signals any threads that are blocked.
// Returns a non-zero value if any errors occur.
{
  return gxThreadSemaphorePost(&semaphore);
}

int gxSemaphore::SemaphoreDecrement()
// gxSemaphore::SemaphoreDecrement() is a non-blocking function
// that decrements the value of the semaphore. It allows threads
// to decrement the semaphore to some negative value as part of
// an initialization process. Decrements allow multiple threads
// to move up on a semaphore before another thread can go down.
// Returns a non-zero value if any errors occur.
{
  return gxThreadSemaphoreDecrement(&semaphore);
}

int gxSemaphore::SemaphoreIncrement()
// gxSemaphore::SemaphoreIncrement() is a non-blocking function
// that increments the value of the semaphore. Returns a non-zero
// value if any errors occur.
{
  return gxThreadSemaphoreIncrement(&semaphore);
}

int gxSemaphore::SemaphoreValue()
// Returns the value of the semaphore at the time the
// critical section is accessed. NOTE: The value may
// change after the function unlocks the critical
// section.
{
  return gxThreadSemaphoreValue(&semaphore);
}

const char *gxSemaphore::SemaphoreExceptionMessage()
// Returns a null terminated string that can
// be use to log or print a semaphore exception.
{
  return gxThreadSemaphoreExceptionMessage(&semaphore);
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
