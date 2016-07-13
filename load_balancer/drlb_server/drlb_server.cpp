// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File
// Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 06/17/2016
// Date Last Modified: 07/07/2016
// Copyright (c) 2016 DataReel Software Development
// ----------------------------------------------------------- // 
// ------------- Program Description and Details ------------- // 
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
	if(!ProcessArgs(argc, argv)) return 1; 
      }
    }
    arg = argv[++narg];
  }

  if(!servercfg->user_config_file) {
    servercfg->verbose = 1;
    NT_print("\n");
    NT_print("ERROR - No config file specified. Use --config-file option to specify a config file");
    NT_print("INFO - If you do not have a config file the program will build a default config and exit");
    NT_print("INFO - Use --help option to see help menu");
    NT_print("\n");
    return 1;
  }

  // Start logging operations here
  if(servercfg->clear_log) {
    if(servercfg->debug) {
      if(servercfg->debug_level >= 3) NT_print("INFO - Clearing log file", servercfg->logfile_name.c_str());
    }
    servercfg->logfile.Create(servercfg->logfile_name.c_str());
    servercfg->logfile.Close();
  }

  NT_log_rotate();

  // Starting program here
  NT_print("Staring DR Load Balancer Service");


  // NOTE: This program will only load a config file is the user has specified one."
  int error_level = LoadOrBuildConfigFile();
  if(error_level == 2) {
    servercfg->verbose = 1;
    NT_print("ERROR - Server configuration file not found. Building default file and exiting");
    return 1;
  }
  if(error_level != 0) {
    if(!servercfg->verbose) {
      servercfg->verbose = 1;
      NT_print("ERROR - Server config file error. Run with --verbose to print error messages");
    }
    else {
      NT_print("ERROR - Server config file error.");
    }
    return 1;
  }

  gxString sbuf;
  sbuf << clear << servercfg->port;
  NT_print("Listening on port", sbuf.c_str()); 
  if(servercfg->scheme == LB_DISTRIB) NT_print("Load balancing scheme = LB_DISTRIB");
  if(servercfg->scheme == LB_RR) NT_print("Load balancing scheme = LB_RR");
  if(servercfg->scheme == LB_WEIGHTED) NT_print("Load balancing scheme = LB_WEIGHTED");

  if(servercfg->scheme == LB_ASSIGNED) {
    NT_print("Load balancing scheme = LB_ASSIGNED");
    NT_print("Checking rules configuration file");
    if(servercfg->rules_config_file.is_null()) {
      NT_print("ERROR - No rules_config_file specified in LB server config");
      return 1;
    }
    switch(servercfg->ReadRulesConfig()) {
      case 0:
	NT_print("INFO - Rules config file checks good at start up");
	if(servercfg->dynamic_rules_read) {
	  NT_print("INFO - Rules config dynamic read is enabled.");
	  NT_print("INFO - Rules config file will be re-read. Changes do not require a server restart.");
	}
	else {
	  NT_print("INFO - Rules config dynamic read is disenabled.");
	  NT_print("INFO - Rules config will only be read once. Changes will require a server restart.");
	}
	break;
      case 1:
	  NT_print("ERROR - Fatal error reading rules config file");
	return 1;
      case 2:
	NT_print("ERROR - Bad rule line in rules config file");
	return 1;
      case 3:
	NT_print("ERROR - Bad regular expression in rules config file");
	return 1;
      default:
	NT_print("ERROR - Unknown error reading rules config file");
	return 1;
    }

    switch(servercfg->assigned_default) {
      case LB_RR:
	NT_print("INFO - Assigned connections will use LB round robin for node fail over");
	break;
      case LB_DISTRIB:
	NT_print("INFO - Assigned connections will use LB distributed for node fail over");
	break;
      case LB_WEIGHTED:
	NT_print("INFO - Assigned connections will use LB weighted for node fail over");
	break;
      case LB_NONE:
	NT_print("INFO - Assigned connections will not fail over to other nodes");
	break;
      default:
	NT_print("ERROR - Bad assigned_default value in server config");
	return 1;
    }
  }

  if(servercfg->use_buffer_cache == 1) {
    NT_print("Socket buffer cache enabled");
    if(servercfg->enable_buffer_overflow_detection) {
      NT_print("INFO: Buffer overflow detection is enabled");
      sbuf << clear << shn << servercfg->buffer_overflow_size;
      NT_print("INFO: Buffer overflow size", sbuf.c_str()); 
    }
    else {
      NT_print("Buffer overflow detection is disabled");
    }
  }
  else {
    NT_print("Socket buffer cache disabled");
  }
  if(servercfg->max_threads >= 1) {
    sbuf << clear << servercfg->max_threads;
    NT_print("Max client threads set to", sbuf.c_str()); 
  }
  else {
    NT_print("Max client threads not set");
  }

  if(servercfg->max_connections >= 1) {
    sbuf << clear << servercfg->max_connections;
    NT_print("Max frontend server clients set to", sbuf.c_str()); 
  }
  else {
    NT_print("Max frontend server clients not set");
  }
  sbuf << clear << servercfg->retries;
  NT_print("Max retries for dead nodes set to", sbuf.c_str()); 
  sbuf << clear << servercfg->retry_wait;
  NT_print("Retry wait secs for dead nodes set to", sbuf.c_str()); 
  if(servercfg->use_timeout) {
    NT_print("Socket read timeouts enabled");
    sbuf << clear << servercfg->timeout_secs;
    NT_print("Timout secs", sbuf.c_str()); 
    sbuf << clear << servercfg->timeout_usecs;
    NT_print("Timout usecs", sbuf.c_str()); 
  }
  else {
    NT_print("Socket read timeouts disabled");
  }
    sbuf << clear << servercfg->log_queue_size;
  NT_print("Max number of log or console messages to queue", sbuf.c_str()); 
  sbuf << clear << servercfg->log_queue_debug_size;
  NT_print("Max number of debug messages to queue", sbuf.c_str()); 
  sbuf << clear << servercfg->log_queue_proc_size;
  NT_print("Max number of process messages to queue", sbuf.c_str()); 
  sbuf << clear << servercfg->num_logs_to_keep;
  if(servercfg->enable_logging)  {
    NT_print("Logging is enabled");
    if(servercfg->clear_log) {
      NT_print("Log file will cleared when LB server is restarted");
    } 

    NT_print("Number of log files to keep", sbuf.c_str()); 
    sbuf << clear << shn << servercfg->max_log_size;
    NT_print("Max log file size set to", sbuf.c_str()); 
    sbuf << clear << servercfg->log_level;
    NT_print("Logging level set to", sbuf.c_str()); 
    NT_print("Log file", servercfg->logfile_name.c_str()); 
  }
  else {
    NT_print("Logging is disabled");
  }
  if(servercfg->debug)  {
    NT_print("Debug is enabled");
    sbuf << clear << servercfg->debug_level;
    NT_print("Debug level set to", sbuf.c_str()); 
  }
  else {
    NT_print("Debug is disabled");
  }
  if(servercfg->verbose)  {
    NT_print("Verbose mode is enabled");
    sbuf << clear << servercfg->verbose_level;
    NT_print("Verbose mode level set to", sbuf.c_str()); 
  }
  else {
    NT_print("Verbose mode is disabled");
  }

  NT_print("LB node config"); 
  gxListNode<LBnode *> *ptr = servercfg->nodes.GetHead();
  int node_cfg_error_level = 0;
  while(ptr) {
    NT_print("Node name", ptr->data->node_name.c_str());
    NT_print("LB node host", ptr->data->hostname.c_str());
    sbuf << clear << ptr->data->port_number;
    NT_print("LB node port", sbuf.c_str()); 
    if(ptr->data->weight >= 1) {
      sbuf << clear << ptr->data->weight;
      if(servercfg->scheme == LB_WEIGHTED) {
	NT_print("LB node set for LB_WEIGHTED mode");
	NT_print("Weight for this node is", sbuf.c_str()); 
      }
      else if(servercfg->scheme == LB_DISTRIB) {
	NT_print("LB node set for LB_DISTRIB mode");
	NT_print("Percent of load for this node is", sbuf.c_str(), "percent"); 
      }
      else {
	NT_print("LB node weight", sbuf.c_str());
	NT_print("Node weight not used in LB_RR mode");
      }
    }
    else {
      if(servercfg->scheme == LB_WEIGHTED) { 
	NT_print("ERROR - Node configuration error. Using LB_WEIGHTED mode with no node weight");
	node_cfg_error_level = 1;  
      }
      if(servercfg->scheme == LB_DISTRIB) { 
	NT_print("ERROR - Node configuration error. Using LB_DISTRIB mode with no node load percent value");
	node_cfg_error_level = 1;
      }
      if(servercfg->scheme == LB_ASSIGNED) {
	if(servercfg->assigned_default == LB_DISTRIB) { 
	  NT_print("ERROR - Node configuration error. Using LB_ASSIGNED with distrib fail over and no node load percent value");
	  node_cfg_error_level = 1;
	}

	if(servercfg->assigned_default == LB_WEIGHTED) { 
	  NT_print("ERROR - Node configuration error. Using LB_ASSIGNED with weighted fail over and no node weight value");
	  node_cfg_error_level = 1;
	}
      }
      NT_print("LB node weight not set");
    }
    if(ptr->data->max_connections >= 1) {
      sbuf << clear << ptr->data->max_connections;
      NT_print("LB node max_connections", sbuf.c_str()); 
    }
    else {
      NT_print("LB node max_connections not set");
    }

    sbuf << clear << ptr->data->buffer_size;
    NT_print("LB node buffer size", sbuf.c_str()); 
    ptr = ptr->next;
  }

  if(node_cfg_error_level != 0) {
    servercfg->verbose = 1;
    NT_print("ERROR - Node configuration error, check your server config file");
    NT_print("Exiting with config errors");
    return 1;
  }

  if((servercfg->scheme == LB_DISTRIB) || (servercfg->scheme == LB_ASSIGNED && servercfg->assigned_default == LB_DISTRIB)) { 
    NT_print("Building distributed node assignments");
    if(!servercfg->build_distributed_rr_node_list()) {
      servercfg->verbose = 1;
      NT_print("ERROR - Error building distributed node list");
      NT_print("Exiting with config errors");
      return 1;
    }
    gxListNode<LBnode *> *ptr = servercfg->distributed_rr_node_list.GetHead();
    int total_percent = 0;
    while(ptr) {
      sbuf << clear << ptr->data->node_name.c_str() <<  " will assume " << ptr->data->weight << "% of the backend load";
      NT_print(sbuf.c_str());
      total_percent += ptr->data->weight;
      ptr = ptr->next;
    }
    if(total_percent == 100) NT_print("Total load percent is 100");
    if(total_percent > 100) {
      sbuf << clear << "WARNING - Total load percent of " << total_percent << " is greater than 100";
      NT_print(sbuf.c_str());
    }
    if(total_percent < 100) {
      sbuf << clear << "WARNING - Total load percent of " << total_percent << " is less than 100";
      NT_print(sbuf.c_str());
    }

    NT_print("Building distributed refactored assignments");
    gxList<limit_node> limit_node_list;
    servercfg->REFACTORED_CONNECTIONS(0, 0, servercfg->refactor_distribution_limits(limit_node_list, servercfg->refactor_scale));
    if(!servercfg->REFACTORED_CONNECTIONS()) {
      servercfg->verbose = 1;
      NT_print("ERROR - Error building distributed refactored assignments");
      NT_print("Exiting with config errors");
      return 1;
    }
    gxListNode<limit_node> *limit_ptr = limit_node_list.GetHead();
    while(limit_ptr) {
      sbuf << clear << limit_ptr->data.active_ptr->node_name.c_str() <<  " refactored to start with " 
	   << limit_ptr->data.limit << " backend connections";
      NT_print(sbuf.c_str());
      limit_ptr = limit_ptr->next;
    }
  }

  if((servercfg->scheme == LB_WEIGHTED) || (servercfg->scheme == LB_ASSIGNED && servercfg->assigned_default == LB_WEIGHTED)) { 
    NT_print("Building weighted node assignments");
    if(!servercfg->build_weighted_rr_node_list()) {
      servercfg->verbose = 1;
      NT_print("ERROR - Error building weighted node list");
      NT_print("Exiting with config errors");
      return 1;
    }
    gxListNode<LBnode *> *ptr = servercfg->weighted_rr_node_list.GetHead();
    while(ptr) {
      sbuf << clear << ptr->data->node_name.c_str() <<  " assigned a weight factor of " << ptr->data->weight << " for backend load";
      NT_print(sbuf.c_str());
      ptr = ptr->next;
    }
    NT_print("Building weighted refactored assignments");
    gxList<limit_node> limit_node_list;
    servercfg->REFACTORED_CONNECTIONS(0, 0, servercfg->refactor_weighted_limits(limit_node_list, servercfg->WEIGHT_SCALE()));
    if(!servercfg->REFACTORED_CONNECTIONS()) {
      servercfg->verbose = 1;
      NT_print("ERROR - Error building weighted refactored assignments");
      NT_print("Exiting with config errors");
      return 1;
    }
    gxListNode<limit_node> *limit_ptr = limit_node_list.GetHead();
    while(limit_ptr) {
      sbuf << clear << limit_ptr->data.active_ptr->node_name.c_str() <<  " refactored to start with " 
	   << limit_ptr->data.limit << " backend connections";
      NT_print(sbuf.c_str());
      limit_ptr = limit_ptr->next;
    }
    sbuf << clear << "Starting with " << servercfg->REFACTORED_CONNECTIONS() << " refactored connections";
    NT_print(sbuf.c_str());
  }

  LBServerThread server;
  LogThread log;

  int rv;
  int count = 0;
  int retries = 5;
  
  servercfg->log_thread = log.CreateThread();
  if(servercfg->log_thread->GetThreadError() != gxTHREAD_NO_ERROR) {
    NT_print("ERROR - Fatal error starting log thread");
    NT_print(servercfg->server_thread->ThreadExceptionMessage());
    return 1;
  }

  while(--retries) {
    NT_print("Initializing server...");
    rv = server.InitServer(servercfg->somaxconn);
    if(rv == 0) break;
    if(rv != 0) {
      NT_print("ERROR - Cannot initializing server, retrying");
      NT_print(server.SocketExceptionMessage());
      sSleep(60);
      continue;
    }
  }

  if(rv != 0) {
    NT_print("ERROR - Cannot initializing server, exiting program");
    NT_print(server.SocketExceptionMessage());
    if(servercfg->log_thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
      NT_print("Stopping log thread");
      log.CancelThread(servercfg->log_thread);
      sSleep(1);
      log.JoinThread(servercfg->log_thread);
      if(servercfg->log_thread->GetThreadState() == gxTHREAD_STATE_CANCELED) {
	NT_print("Log thread was stopped");
      }
      else {
	NT_print("LB server thread did not shutdown properly");
	NT_print("Exiting program...");
	return 1;
      }

    }
    NT_print("Exiting program...");
    return 1;
  }

  servercfg->server_thread = server.CreateThread();
  if(servercfg->server_thread->GetThreadError() != gxTHREAD_NO_ERROR) {
    NT_print("ERROR - Fatal error starting main server thread");
    NT_print(servercfg->server_thread->ThreadExceptionMessage());
  }
  else {
    NT_print("Main server thread started, waiting for client connections...");
    if(server.JoinThread(servercfg->server_thread) != 0) {
      NT_print("ERROR - Fatal error joining main server thread");
      NT_print(servercfg->server_thread->ThreadExceptionMessage());
    }
  }

  NT_print("Main server thread has exited program will exit");  

  retries = 3;
  while(!StopProc() && --retries) sSleep(1);
  NT_print("Exiting program...");
  return 1;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //


  

