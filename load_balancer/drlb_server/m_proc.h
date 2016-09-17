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

class ProcessThread : public gxThread
{
public:
  ProcessThread() { }
  ~ProcessThread() { }

private:
  void *ThreadEntryRoutine(gxThread_t *thread);
};

// Signal I/O process functions
void signal_handler_IO(int status);  // Signal handler
void termination_handler(int signum);

// Main process functions
int StopProc();
void ExitProc(int return_code);
int StopThreads();

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
