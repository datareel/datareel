// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxcond.cpp
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

Condition variable class used used by a thread to block its own
execution until an expression involving shared data attains a
particular state. 
*/
// ----------------------------------------------------------- //   
#include "gxdlcode.h"

#include "gxcond.h"

gxCondition::gxCondition(gxProcessType type)
{
  ConditionInit(type);
}

gxCondition::~gxCondition()
{
  // PC-lint 09/08/2005: Function may throw exception in destructor
  ConditionDestroy();
}

int gxCondition::ConditionInit(gxProcessType type)
// Initialize the condition variable. Returns a non-zero
// value if any errors occur.
{
  int rv = gxThreadConditionInit(&condition, type);
  if(rv != 0) return rv;
  num_waiters = 0;
  return 0;
}

int gxCondition::ConditionDestroy()
// Destroy the condition variable. Returns a non-zero
// value if any errors occur.
{
  int rv = gxThreadConditionDestroy(&condition);
  if(rv != 0) return rv;
  num_waiters = 0;
  return 0;
}

int gxCondition::ConditionWait(gxMutex *m)
// Function used to block a thread from its own execution
// until this condition is signaled. Returns a non-zero value
// if any errors occur.
{
  num_waiters++;
  int rv = gxThreadConditionWait(&condition, m->GetMutex());
  if(rv != 0) {
    num_waiters--;
    return rv;
  }
  return 0;
}

int gxCondition::ConditionSignal()
// Function used to wake up a thread waiting on the this
// condition. Returns a non-zero value if any errors occur.
{
  int rv = gxThreadConditionSignal(&condition);
  if(rv != 0) return rv;
  if(num_waiters > 0) num_waiters--;
  return 0;
}

int gxCondition::ConditionBroadcast()
// Function used to wake up all threads waiting on this
// condition Returns a non-zero value if any errors occur.
{
  int rv = gxThreadConditionBroadcast(&condition);
  if(rv != 0) return rv;
  num_waiters = 0;
  return 0;
}

int gxCondition::ConditionTimedWait(gxMutex *m, unsigned long sec,
				    unsigned long nsec)
// Function used to block a thread from its own execution until this 
// condition is signaled or the timeout value elapses. Returns a
// non-zero value if any errors occur.
{
  num_waiters++;
  int rv = gxThreadConditionTimedWait(&condition, m->GetMutex(), sec, nsec);
  if(rv != 0) {
    num_waiters--;
    return rv;
  }
  return 0;
}

const char *gxCondition::ConditionExceptionMessage()
// Returns a null terminated string that can be
// use to log or print a condition exception.
{
  return gxThreadConditionExceptionMessage(&condition);
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
