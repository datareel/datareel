// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File
// Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 07/17/2016
// Date Last Modified: 07/27/2016
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

Datareel cluster manager server.

*/
// ----------------------------------------------------------- //   
#include "gxdlcode.h"

#include "drcm_server.h"

int main(int argc, char **argv)
{
  // Ignore broken pipe so kernel does not kill this progress
  // for sockets with no readers
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
    
  // Allow sockets to close before any core dumps    
  signal(SIGBUS, termination_handler);  // Misaligned memory access
  signal(SIGSEGV, termination_handler); // Illegal memory access

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

  if(servercfg->check_config) {
    // Run the check config program and exit
    return check_config();
  }

  if(servercfg->is_client) {
    // Run the client program and return 
    return run_client();
  }

  int retries = 3;

  // Start the CM server
  while(!servercfg->kill_server) {
    if(run_server() != 0) break;
    if(servercfg->restart_server) {
      LogMessage("Restarting CM server");
      while(StopThreads() != 0 && retries--) sleep(1);
      sSleep(5); // Allow for I/O recovery time
      continue;
    }
  }

  NT_print("Main process thread has exited program will exit");  
  sSleep(5);

  retries = 3;
  while(!StopProc() && --retries) sSleep(1);
  NT_print("Exiting program...");
  return 0;
}

int run_server()
{
  // Starting server here
  NT_print("Staring DR Cluster Manager Server");
  gxString sbuf;
  int error_level = 0;

  if(!futils_exists(servercfg->config_file.c_str())) {
    servercfg->verbose = 1;
    NT_print("ERROR - Config file not found", servercfg->config_file.c_str());
    return 1; // We cannot continue
  }

  error_level = LoadOrBuildConfigFile();
  if((error_level != 0)) {
    servercfg->verbose = 1;
    NT_print("ERROR - CM config file load error.");
    return error_level; // We cannot continue;
  }

  if(load_hash_key() != 0) return 1;

  PrintGlobalConfig();
  PrintNodeConfig();

  // Set up the logging queues for server 
  servercfg->loq_queue_nodes = new LogQueueNode[servercfg->log_queue_size];
  servercfg->loq_queue_debug_nodes = new LogQueueNode[servercfg->log_queue_debug_size];
  servercfg->loq_queue_proc_nodes = new LogQueueNode[servercfg->log_queue_proc_size];

  gxString message, command;
  gxString user = "root";
  if(servercfg->cluster_user == user.c_str()) {
    if(geteuid() != 0) {
      servercfg->verbose = 1;
      NT_print("Config file set to run CM as root");
      NT_print("ERROR - You must be root to run this program");
      return 1;  // We cannot continue
    }
  }
  else {
    user.Clear();
    char *e = getenv("USER");
    if(!e) {
      servercfg->verbose = 1;
      NT_print("ERROR - ${USER} var not set in your ENV");
    }
    user = getenv("USER");
    if(servercfg->cluster_user != user.c_str()) {
      servercfg->verbose = 1;
      message << clear << "ERROR - Username starting CM server is " << user;
      NT_print(message.c_str());
      message << clear << "ERROR - CM config file username is set to " << servercfg->cluster_user; 
      NT_print(message.c_str());
      return 1;  // We cannot continue
    }
  }

  if(!futils_exists(servercfg->etc_dir.c_str())) {
    servercfg->verbose = 1;
    NT_print("ERROR - Missing DIR ", servercfg->etc_dir.c_str());
    error_level = 1;
  }
  if(!NT_check_dir(servercfg->log_dir)) {
    servercfg->verbose = 1;
    NT_print("ERROR - Missing DIR ", servercfg->log_dir.c_str());
    error_level = 1;
  }
  if(!NT_check_dir(servercfg->spool_dir)) {
    servercfg->verbose = 1;
    NT_print("ERROR - Missing DIR ", servercfg->spool_dir.c_str());
    return 1;
  }
  if(!NT_check_dir(servercfg->run_dir)) {
    servercfg->verbose = 1;
    NT_print("ERROR - Missing DIR ", servercfg->run_dir.c_str());
    error_level = 1;
  }
  
  char hbuf[gxsMAX_NAME_LEN];
  memset(hbuf, 0 , gxsMAX_NAME_LEN);
  if(gethostname(hbuf, gxsMAX_NAME_LEN) < 0) {
    servercfg->verbose = 1;
    NT_print("ERROR - Could not get hostname for this server");
    return 1; // We cannot continue
  }
  NT_print("Hostname for this CM node =", hbuf);
  servercfg->my_hostname = hbuf;
  int has_hostname = 0;

  // Run node checks here, only check VARs that would cause server to crash
  gxListNode<CMnode *> *ptr = servercfg->nodes.GetHead();
  int node_cfg_error_level = 0;
  while(ptr) {
    if(ptr->data->hostname == servercfg->my_hostname) has_hostname = 1;
    ptr = ptr->next;
  }

  if(!has_hostname) {
    servercfg->verbose = 1;
    NT_print("ERROR - The hostname of this server does not match and node hostname in config file");
    NT_print("Exiting with setup errors");
    error_level = 1;
  }

  // End of requried server startup checks
  if(error_level != 0) return error_level;

  sbuf << clear << servercfg->udpport;
  NT_print("CM keep alive listening on UDP port", sbuf.c_str()); 
  sbuf << clear << servercfg->tcpport;
  NT_print("CM server listening on TCP port", sbuf.c_str()); 

  NT_log_rotate();

  CM_UDP_ServerThread udp_server_t;
  CM_KeepAliveThread keep_alive_t;
  LogThread log;
  CM_CrontabsThread ctab_t;
  CM_IPaddrsThread ipaddrs_t;
  CM_servicesThread services_t;
  CM_applicationsThread applications_t;
  CM_filesystemsThread filesystems_t;

  int rv;
  int count = 0;
  int retries = 6;
  
  NT_print("Starting log server thread");
  servercfg->log_thread = log.CreateThread();
  if(servercfg->log_thread->GetThreadError() != gxTHREAD_NO_ERROR) {
    NT_print("ERROR - Fatal error starting log thread");
    NT_print(servercfg->udp_server_thread->ThreadExceptionMessage());
    return 1;
  }

  while(--retries) {
    NT_print("Initializing CM keep alive server...");
    rv = udp_server_t.InitServer();
    if(rv == 0) break;
    if(rv != 0) {
      NT_print("ERROR - Cannot initializing cluster keep alive server, retrying");
      NT_print(udp_server_t.SocketExceptionMessage());
      sSleep(3);
      continue;
    }
  }

  if(rv != 0) {
    NT_print("ERROR - Cannot initializing cluster manager keep alive, exiting program");
    NT_print(udp_server_t.SocketExceptionMessage());
    if(servercfg->log_thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
      NT_print("Stopping log thread");
      log.CancelThread(servercfg->log_thread);
      sSleep(1);
      log.JoinThread(servercfg->log_thread);
      if(servercfg->log_thread->GetThreadState() == gxTHREAD_STATE_CANCELED) {
	NT_print("Log thread was stopped");
      }
      else {
	NT_print("CM server thread did not shutdown properly");
	NT_print("Exiting program...");
	return 1;
      }

    }
    NT_print("Exiting program...");
    return 1;
  }

  NT_print("Starting CM process thread");
  ProcessThread process;
  gxThread_t *process_thread = process.CreateThread();
  if(!process_thread) {
    LogMessage("ERROR - Encountered fatal error creating CM process thread");
    return 1;
  }
  if(process_thread->GetThreadError() != gxTHREAD_NO_ERROR) {
    LogMessage("ERROR - Encountered fatal error starting CM process thread");
    return 1;
  }

  NT_print("Starting CM UDP server thread");
  servercfg->udp_server_thread = udp_server_t.CreateThread();
  if(servercfg->udp_server_thread->GetThreadError() != gxTHREAD_NO_ERROR) {
    NT_print("ERROR - Fatal error starting UDP server thread");
    NT_print(servercfg->udp_server_thread->ThreadExceptionMessage());
    return 1;
  }

  NT_print("Starting CM UDP keep alive thread");
  servercfg->keep_alive_thread = keep_alive_t.CreateThread();
  if(servercfg->keep_alive_thread->GetThreadError() != gxTHREAD_NO_ERROR) {
    NT_print("ERROR - Fatal error starting UDP keep alive thread");
    NT_print(servercfg->keep_alive_thread->ThreadExceptionMessage());
    return 1;
  }

  NT_print("Starting floating IP address thread");
  NT_print("Removing any old floating IP addresses");
  ipaddrs_t.remove_all_ipaddrs();
  servercfg->ipaddr_thread = ipaddrs_t.CreateThread();
  if(servercfg->ipaddr_thread->GetThreadError() != gxTHREAD_NO_ERROR) {
    NT_print("ERROR - Fatal error starting floating IP address thread");
    NT_print(servercfg->ipaddr_thread->ThreadExceptionMessage());
    return 1;
  }

  NT_print("Starting file systems thread");
  servercfg->filesystems_thread = filesystems_t.CreateThread();
  if(servercfg->filesystems_thread->GetThreadError() != gxTHREAD_NO_ERROR) {
    NT_print("ERROR - Fatal error starting filesystems thread");
    NT_print(servercfg->filesystems_thread->ThreadExceptionMessage());
    return 1;
  }

  NT_print("Starting services thread");
  servercfg->services_thread = services_t.CreateThread();
  if(servercfg->services_thread->GetThreadError() != gxTHREAD_NO_ERROR) {
    NT_print("ERROR - Fatal error starting services thread");
    NT_print(servercfg->services_thread->ThreadExceptionMessage());
    return 1;
  }

  NT_print("Starting crontab thread");
  NT_print("Removing any old crontabs");
  ctab_t.remove_all_crontabs();
  servercfg->crontab_thread = ctab_t.CreateThread();
  if(servercfg->crontab_thread->GetThreadError() != gxTHREAD_NO_ERROR) {
    NT_print("ERROR - Fatal error starting crontab thread");
    NT_print(servercfg->crontab_thread->ThreadExceptionMessage());
    return 1;
  }

  NT_print("Starting applications thread");
  servercfg->applications_thread = applications_t.CreateThread();
  if(servercfg->applications_thread->GetThreadError() != gxTHREAD_NO_ERROR) {
    NT_print("ERROR - Fatal error starting applications thread");
    NT_print(servercfg->applications_thread->ThreadExceptionMessage());
    return 1;
  }

  if(process.JoinThread(process_thread) != 0) {
    LogMessage("ERROR - Could not join CM process thread");
    return 1;
  }
  
  return 0;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //


  

