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

Datareel load balancer.

*/
// ----------------------------------------------------------- //   
#ifndef __DRLB_SERVER_HPP__
#define __DRLB_SERVER_HPP__

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
#include "m_config.h"
#include "m_app.h"
#include "m_dbase.h"
#include "m_thread.h"
#include "m_socket.h"
#include "m_proc.h"
#include "m_help.h"
#include "m_log.h"
#include "m_stats.h"

#endif // __DRLB_SERVER_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
