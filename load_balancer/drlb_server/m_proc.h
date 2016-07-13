// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 06/17/2016
// Date Last Modified: 07/01/2016
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

Process functions for Datareel load balancer.

*/
// ----------------------------------------------------------- //   
#ifndef __DRLB_PROC_HPP__
#define __DRLB_PROC_HPP__

#include "gxdlcode.h"

#include <string.h>
#include <stdlib.h>
#include "drlb_server.h"
#include "m_thread.h"
#include "m_help.h"
#include "m_config.h"
#include "m_log.h"
#include "m_dbase.h"

class ConsoleThread : public gxThread
{
public:
  ConsoleThread() { }
  ~ConsoleThread() { }

private:
  void *ThreadEntryRoutine(gxThread_t *thread);
};


// Signal I/O process functions
void signal_handler_IO(int status);  // Signal handler
void termination_handler(int signum);

// Main process functions
int StopProc();
void ExitProc();
void SpwanChildProcess(const char *command, const char *switches, const char *arg);

// Timer functions
void sSleep(unsigned long seconds);
void nSleep(unsigned long nanoseconds);
void uSleep(unsigned long microseconds);
void mSleep(unsigned long  milliseconds);

#endif // __DRLB_PROC_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
