// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 06/17/2016
// Date Last Modified: 09/17/2016
// Copyright (c) 2016 DataReel Software Development
// ----------------------------------------------------------- // 
// ---------- Include File Description and Details  ---------- // 
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

Log functions for Datareel load balancer.

*/
// ----------------------------------------------------------- //   
#ifndef __DRLB_LOG_HPP__
#define __DRLB_LOG_HPP__

#include "gxdlcode.h"
#include "drlb_server.h"

class LogThread : public gxThread
{
public:
  LogThread() { }
  ~LogThread() { }

public:
  void write_log_entry(char *sbuf);
  int flush_all_logs();

private:
  void *ThreadEntryRoutine(gxThread_t *thread);
  void ThreadCleanupHandler(gxThread_t *thread);
};

// Thread safe routines. Called when threads are running.
void LogMessage(const char *mesg); // Log to thread message queue
void LogDebugMessage(const char *mesg); // Log to debug queue
void LogProcMessage(const char *mesg);  // Log to process queue
int CheckThreadError(gxThread_t *thread, int seq_num = 0);
int CheckSocketError(gxSocket *s, int seq_num = 0);

// Non-thread safe console and logging functions.
// Only called when process starts and stops and no threads are running.
int NT_print(const char *mesg1, const char *mesg2 = 0, const char *mesg3 = 0);
int NT_printblock(const char *mesg1, const char *mesg2 = 0, 
		  const char *mesg3 = 0, int lines = 0, int blob = 0);
int NT_printlines(const char *mesg1, const char *mesg2 = 0, 
		  const char *mesg3 = 0);
int NT_log_rotate(); // Called when program starts and no threads are running

// Syslog functions
void SyslogErrorMessage(const char *mesg);
void SyslogDebugMessage(const char *mesg);
void SyslogInfoMessage(const char *mesg);
void SyslogWarningMessage(const char *mesg);

#endif // __DRLB_LOG_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
