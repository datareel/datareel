// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File
// Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 06/17/2016
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

Datareel load balancer server.

*/
// ----------------------------------------------------------- //   
#include "gxdlcode.h"

#include "drlb_server.h"

int main(int argc, char **argv)
{
  // NOTE: Linux will kill a process that attempts to write to a pipe 
  // with no readers. This application may send data to client socket 
  // that has disconnected. If we do not tell the kernel to explicitly 
  // ignore the broken pipe the kernel will kill this process.
  signal(SIGPIPE, SIG_IGN);

  // Handle UNIX termination signals
  signal(SIGINT, termination_handler);  // Ctrl-C or "kill INT pid"
  signal(SIGQUIT, termination_handler); // Ctrl-backslash
  signal(SIGTERM, termination_handler); // Kill command "kill pid"
  signal(SIGHUP, termination_handler);  // Hangup if pstty closes with process running
  signal(SIGKILL, termination_handler); // Used by kill -9
  signal(SIGTSTP, termination_handler); // Ctrl-Z, resume with fg command
  signal(SIGABRT, termination_handler); // Ctrl-backslash
  signal(SIGUSR1, termination_handler); // kill -USR1
  signal(SIGUSR2, termination_handler); // kill -USR2

  // Allow sockets to close before any core dumps, if not DEBUG compile 
#ifndef __DEBUG__
  signal(SIGBUS, termination_handler);  // Misaligned memory access
  signal(SIGSEGV, termination_handler); // Illegal memory access
#endif

 // Process command ling arguments and files 
  int narg;
  char *arg = argv[narg = 1];
  while(narg < argc) {
    if(arg[0] != '\0') {
      if(arg[0] == '-') { // Look for command line arguments
	// Exit if argument is not valid or argument signals program to exit
	if(ProcessArgs(argc, argv) != 0) {
	  servercfg->verbose = 1;
	  NT_print("ERROR - Bad -- argument, use --help to print valid -- arguments");
	  return 1; 
	}
      }
    }
    arg = argv[++narg];
  }

  if(!servercfg->user_config_file) {
    servercfg->verbose = 1;
    NT_print("ERROR - No config file specified. Use --config-file option to specify a config file");
    NT_print("INFO - If you do not have a config file the program will build a default config and exit");
    NT_print("INFO - Use the --help option to see help menu");
    return 1;
  }
  
  if(servercfg->check_config) {
    // Run the check config program and exit
    if(check_config() != 0) return 1;
    NT_print("DRLB config file checks good");
    return 0;
  }

  NT_print("Staring DR Load Balancer");
  int error_level = check_config();
  if(error_level != 0) return error_level;

  // Start logging operations here
  if(servercfg->enable_logging) {
    NT_log_rotate();
    if(servercfg->clear_log) {
      if(servercfg->debug && servercfg->debug_level >= 3) {
	NT_print("INFO - Clearing log file", servercfg->logfile_name.c_str());
      }
      servercfg->logfile.Create(servercfg->logfile_name.c_str());
    }
  }

  int retries = 3;
  error_level = 0;
  
  // Start the CM server
  while(!servercfg->kill_server) {
    if(servercfg->debug) LogMessage("Run LB server() called");
    error_level = run_server();
    if(error_level != 0) break;
    if(servercfg->restart_server) {
      LogMessage("Restarting LB server");
      while(StopThreads() != 0 && retries--) sleep(1);
      sSleep(1); // Allow for I/O recovery time
      continue;
    }
  }

  NT_print("Main process thread has exited program will exit");  
  sSleep(5);
  
  retries = 3;
  while(!StopProc() && --retries) sSleep(1);
  NT_print("Exiting DRLB program...");

  return error_level;
}

int run_server()
{
  int error_level = 0;
  LBServerThread server;
  LogThread log;
  StatsThread stats;
  ProcessThread process;
  int rv;
  int retries = 3;

  if(servercfg->log_thread) { 
    if(servercfg->log_thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
      NT_print("LB log thread already running");
    }
    else {
      delete servercfg->log_thread;
      servercfg->log_thread = 0;
    }
  }
  else {
    NT_print("Starting LB log thread");
    servercfg->log_thread = log.CreateThread();
    if(!servercfg->log_thread) {
      LogMessage("ERROR - Encountered fatal error creating LB log thread");
      return 1;
    }
    if(servercfg->log_thread->GetThreadError() != gxTHREAD_NO_ERROR) {
      NT_print("ERROR - Fatal error starting log thread");
      NT_print(servercfg->server_thread->ThreadExceptionMessage());
      return 1;
    }
  }

  if(servercfg->process_thread) { 
    if(servercfg->process_thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
      NT_print("LB process thread already running");
    }
    else {
      delete servercfg->process_thread;
      servercfg->process_thread = 0;
    }
  }
  else {
    NT_print("Starting LB process thread");
    servercfg->process_thread = process.CreateThread();
    if(!servercfg->process_thread) {
      LogMessage("ERROR - Encountered fatal error creating LB process thread");
      return 1;
    }
    if(servercfg->process_thread->GetThreadError() != gxTHREAD_NO_ERROR) {
      LogMessage("ERROR - Encountered fatal error starting LB process thread");
      NT_print(servercfg->process_thread->ThreadExceptionMessage());
      return 1;
    }
  }
  
  if(servercfg->enable_stats) {
    if(servercfg->stats_thread) { 
      if(servercfg->stats_thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
	NT_print("LB stats thread already running");
      }
      else {
	delete servercfg->stats_thread;
	servercfg->stats_thread = 0;
      }
    }
    else {
      servercfg->stats_thread = stats.CreateThread();
      if(!servercfg->stats_thread) {
	LogMessage("ERROR - Encountered fatal error creating LB stats thread");
	return 1;
      }
      if(servercfg->stats_thread->GetThreadError() != gxTHREAD_NO_ERROR) {
	NT_print("ERROR - Fatal error starting stats thread");
	NT_print(servercfg->server_thread->ThreadExceptionMessage());
	return 1;
      }
    }
  }


  if(servercfg->server_thread) { 
    if(servercfg->server_thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
      NT_print("LB server thread already running");
    }
    else {
      delete servercfg->server_thread;
      servercfg->server_thread = 0;
    }
  }
  else {
    while(--retries) {
      NT_print("Initializing DRLB server...");
      rv = server.InitServer(servercfg->somaxconn);
      if(rv == 0) break;
      if(rv != 0) {
	NT_print("ERROR - Cannot initializing DRLB server, retrying");
	NT_print(server.SocketExceptionMessage());
	sSleep(60);
	continue;
      }
    }
    if(rv != 0) {
      NT_print("ERROR - Cannot initializing DRLBserver");
      NT_print(server.SocketExceptionMessage());
      return 1;
    }
    servercfg->server_thread = server.CreateThread();
    if(!servercfg->server_thread) {
      LogMessage("ERROR - Encountered fatal error creating LB server thread");
      return 1;
    }
    if(servercfg->server_thread->GetThreadError() != gxTHREAD_NO_ERROR) {
      NT_print("ERROR - Fatal error starting main server thread");
      NT_print(servercfg->server_thread->ThreadExceptionMessage());
      return 1;
    }
    else {
      NT_print("DRLB thread started, waiting for client connections...");
    }
  }

  // run_server will not return from here until the process thread is 
  // signaled to stop process or check process to see if we need to 
  // exit the program due to a fatal error.
  if(servercfg->debug) LogMessage("Joining LB process thread");
  if(process.JoinThread(servercfg->process_thread) != 0) {
    LogMessage("ERROR - Could not join LB process thread");
    return 1;
  }

  return error_level;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //


  

