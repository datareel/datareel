// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 07/17/2016
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

Datareel cluster manager.

*/
// ----------------------------------------------------------- //   
#ifndef __DRCM_SERVER_HPP__
#define __DRCM_SERVER_HPP__

#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__
#include <string.h>
#include <stdlib.h>
#include <sys/signal.h>
#ifndef __USE_SIGNAL_IO__
#define __USE_SIGNAL_IO__
#endif

// Datareel include files
#include "gxstring.h"
#include "gxlist.h"
#include "gxmutex.h"
#include "gxcond.h"
#include "dfileb.h"
#include "systime.h"
#include "logfile.h" 
#include "gxconfig.h"
#include "gxthread.h"
#include "gxtelnet.h"
#include "thrpool.h"
#include "gxscomm.h"

// Project include files
#include "m_log.h"
#include "m_stats.h"
#include "m_config.h"
#include "m_socket.h"
#include "m_app.h"
#include "m_dbase.h"
#include "m_thread.h"
#include "m_proc.h"
#include "m_help.h"


int run_server();
int run_client();

#endif // __DRCM_SERVER_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
