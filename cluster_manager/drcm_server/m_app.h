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

Application classes and functions for Datareel cluster manager.

*/
// ----------------------------------------------------------- //   
#ifndef __DRCM_APP_HPP__
#define __DRCM_APP_HPP__

#include "gxdlcode.h"
#include "drcm_server.h"

// Thread for CM client based applications
class ConsoleThread : public gxThread
{
public:
  ConsoleThread() { }
  ~ConsoleThread() { }

private:
  void ThreadExitRoutine(gxThread_t *thread);
  void *ThreadEntryRoutine(gxThread_t *thread);
};

void PrintGlobalConfig();
void PrintNodeConfig();
int check_config();

// Load the hash key for this client or server, return 0 if pass, non-zero for fail
int load_hash_key();

// Get raw stats buffer, return 0 if pass, non-zero for fail
// Pass back rstats in buffer string and any error message in error_message string
int get_node_stat_buffer(CMnode *node, gxString &buffer, gxString &error_message);

// DRCM client console functions
int console_command_is_valid(const gxString &command);
int print_console_help();
int run_console_command(const gxString &command);
int ping_cluster();
int ping_cluster_node(const gxString &n);
int ping_node(CMnode *node);
int print_config();
int get_cluster_rstats();
int get_rstats_node(const gxString &n);
int get_rstats(CMnode *node);
CMnode *get_client_node(const gxString &n) ;
int cm_stat();

#endif // __DRCM_APP_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
