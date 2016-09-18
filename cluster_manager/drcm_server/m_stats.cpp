// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 07/17/2016
// Date Last Modified: 09/17/2016
// Copyright (c) 2016 DataReel Software Development
// ----------------------------------------------------------- // 
// ------------- Program Description and Details ------------- // 
// ----------------------------------------------------------- // 
/*
This file is part of the DataReel software distribution.

Datareel is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation, either version 3 of the License, or (at your
option) any later version. 

Datareel software is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with the DataReel software distribution.  If not, see
<http://www.gnu.org/licenses/>.

Stat functions for Datareel cluster manager.

*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "drcm_server.h"
#include "m_stats.h"

int CMstats::Add(gxString &s, int check_existing) 
{
  stats_lock.MutexLock();

  int num_try = 0;
  while(stats_is_locked != 0) {
    if(++num_try < stats_retries) {
      stats_cond.ConditionWait(&stats_lock);
    }
    else {
      return 0;
    }
  }
  
  stats_is_locked = 1; 
  
  // ********** Enter Critical Section ******************* //
  int has_stat = 0;
  int was_added = 0;
  gxString sbuf;
  SysTime logtime;

  if(check_existing) {
    gxListNode<gxString> *ptr = stats.GetHead();
    while(ptr) {
      sbuf = ptr->data;
      sbuf.DeleteAfterIncluding(",");
      if(sbuf == s) {
	has_stat = 1;
	break;
      }
      ptr = ptr->next;
    }
  }
  if(!has_stat) {
    was_added = 1;
    sbuf << clear << s << "," << logtime.GetSyslogTime();
    stats.Add(sbuf);
  }
  // ********** Leave Critical Section ******************* //
  
  stats_is_locked = 0; 
  stats_cond.ConditionSignal();
  stats_lock.MutexUnlock();

  return was_added;
}

void CMstats::Clear() {
  stats_lock.MutexLock();
    
  int num_try = 0;
  while(stats_is_locked != 0) {
    if(++num_try < stats_retries) {
      stats_cond.ConditionWait(&stats_lock);
    }
    else {
      return;
    }
  }
  
  stats_is_locked = 1; 
  
  // ********** Enter Critical Section ******************* //
  stats.ClearList();
  // ********** Leave Critical Section ******************* //
  
  stats_is_locked = 0; 
  stats_cond.ConditionSignal();
  stats_lock.MutexUnlock();
}

int CMstats::Remove(const gxString &s) 
{
  stats_lock.MutexLock();
    
  int num_try = 0;
  while(stats_is_locked != 0) {
    if(++num_try < stats_retries) {
      stats_cond.ConditionWait(&stats_lock);
    }
    else {
      return 0;
    }
  }
  
  stats_is_locked = 1; 
  
  // ********** Enter Critical Section ******************* //
  gxListNode<gxString> *ptr = stats.GetHead();
  int has_stat = 0;
  gxString sbuf;
  while(ptr) {
    sbuf = ptr->data;
    sbuf.DeleteAfterIncluding(",");
    if(sbuf == s) {
      stats.Remove(ptr);
      has_stat = 1;
      break;
    }
    ptr = ptr->next;
  }
  // ********** Leave Critical Section ******************* //
  
  stats_is_locked = 0; 
  stats_cond.ConditionSignal();
  stats_lock.MutexUnlock();

  return has_stat;
}

void CMstats::Get(gxString &sbuf) 
{

  sbuf.Clear();
  gxList<gxString> s;
  Get(s);
  gxListNode<gxString> *ptr = s.GetHead();
  while(ptr) {
    sbuf << ptr->data << "\n";
    ptr = ptr->next;
  }
  if(sbuf.is_null()) sbuf << "No CM resources started" << "\n";

}

void CMstats::Get(gxList<gxString> &s) 
{
  stats_lock.MutexLock();
    
  int num_try = 0;
  while(stats_is_locked != 0) {
    if(++num_try < stats_retries) {
      stats_cond.ConditionWait(&stats_lock);
    }
    else {
      return;
    }
  }
  
  stats_is_locked = 1; 
  
  // ********** Enter Critical Section ******************* //
  s.ClearList();
  gxListNode<gxString> *ptr = stats.GetHead();
  while(ptr) {
    s.Add(ptr->data);
    ptr = ptr->next;
  }
  // ********** Leave Critical Section ******************* //
  
  stats_is_locked = 0; 
  stats_cond.ConditionSignal();
  stats_lock.MutexUnlock();
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
