// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxsema.h
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

Semaphore synchronization class implemented using condition variables
and a mutually exclusive integer variable. Typically, an application
uses a semaphore to limit the number of threads using a resource by
causing a thread to stop and wait until another thread signals that it
has arrived at a certain place in the entry function.
*/
// ----------------------------------------------------------- //   
#ifndef __GX_THREAD_SEMAPHORE_HPP__
#define __GX_THREAD_SEMAPHORE_HPP__

#include "gxdlcode.h"

#include "thrtypes.h"
#include "thelpers.h"

class GXDLCODE_API gxSemaphore
{
public:
  gxSemaphore(gxProcessType type = gxPROCESS_PRIVATE);
  ~gxSemaphore();

private: // Disallow copying and assignment
  gxSemaphore(const gxSemaphore &ob) { }
  void operator=(const gxSemaphore &ob) { }

public:
  int SemaphoreInit(gxProcessType type = gxPROCESS_PRIVATE);
  int SemaphoreDestroy();
  int SemaphoreValue();
  int SemaphoreWait();
  int SemaphorePost();
  int SemaphoreDecrement();
  int SemaphoreIncrement();
  gxSemaphore_t *GetSemaphore() { return &semaphore; }
  gxSemaphoreError GetSemaphoreError() { return semaphore.semaphore_error; }
  gxProcessType GetSemaphoreProcessType() { return semaphore.process_type; }
  
public: // Semaphore status/debug message functions
  const char *SemaphoreExceptionMessage();
  
public: // Overloaded operators
  void operator++(int) { SemaphoreIncrement(); }
  void operator--(int) { SemaphoreDecrement(); }

public: // Conversion functions
  operator int() { return SemaphoreValue(); }

private:
  gxSemaphore_t semaphore;
};

#endif // __GX_THREAD_SEMAPHORE_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
