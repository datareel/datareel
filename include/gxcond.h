// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxcond.h
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

Condition variable class used by a thread to block its own
execution until an expression involving shared data attains a
particular state. 
*/
// ----------------------------------------------------------- //   
#ifndef __GX_CONDITION_HPP__
#define __GX_CONDITION_HPP__

#include "gxdlcode.h"

#include "thrtypes.h"
#include "thelpers.h"
#include "gxmutex.h"

class GXDLCODE_API gxCondition
{
public:
  gxCondition(gxProcessType type = gxPROCESS_PRIVATE);
  ~gxCondition();

private: // Disallow copying and assignment
  gxCondition(const gxCondition &ob) { } 
  void operator=(const gxCondition &ob) { }

public:
  int ConditionInit(gxProcessType type = gxPROCESS_PRIVATE);
  int ConditionDestroy();
  int ConditionWait(gxMutex *m);
  int ConditionTimedWait(gxMutex *m, unsigned long sec, unsigned long nsec=0);
  int ConditionSignal();
  int ConditionBroadcast();
  int ThreadsWaiting() { return num_waiters > 0; }
  int NumWaiting() { return num_waiters; }
  gxCondition_t *GetCondition() { return &condition; }
  gxConditionError GetConditionError() { return condition.condition_error; }
  gxProcessType GetConditionProcessType() { return condition.process_type; }

public: // Condition variable status/debug message functions
  const char *ConditionExceptionMessage();
  
private:
  int num_waiters;
  gxCondition_t condition;
};

#endif // __GX_CONDITION_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //


