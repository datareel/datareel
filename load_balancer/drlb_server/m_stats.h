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

Stat functions for Datareel load balancer.

*/
// ----------------------------------------------------------- //   
#ifndef __DRLB_STATS_HPP__
#define __DRLB_STATS_HPP__

#include "gxdlcode.h"

class StatsThread : public gxThread
{
public:
  StatsThread() { 
    node_prev_connection_count = 0;
  }
  ~StatsThread();

public:
  int rotate_stats_file();

private:
  void *ThreadEntryRoutine(gxThread_t *thread);

private:
  unsigned long *node_prev_connection_count;
};


#endif // __DRLB_STATS_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
