// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 06/17/2016
// Date Last Modified: 10/09/2016
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

Program config functions for Datareel load balancer.

*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <regex.h>

#include "drlb_server.h"
#include "m_config.h"
#include "gxbstree.h"

LBconfig ServerConfigSruct;
LBconfig *servercfg = &ServerConfigSruct;

LBconfig::LBconfig() 
{
  scheme = LB_RR;
  use_buffer_cache = 1;
  buffer_size = 1555;
  debug = 0;
  debug_level = 1;
  verbose = 0;
  verbose_level = 1;
  max_threads = -1;
  max_connections = -1;
  num_client_threads = 0;
  num_server_connections = 0;
  clear_log = 0;
  config_file = "drlb_server.cfg";
  logfile_name.Clear();
  log_level = 0;
  enable_logging = 0; 
  service_name = "drlb_server"; // Default service name
  ProgramName = "drlb_server";  // Default program name
  user_config_file = 0;
  port = 8080;
  listen_ip_addr.Clear();
  resolve_assigned_hostnames = 1;
  accept_clients = 1;
  client_request_pool = new thrPool;
  fatal_error = 0;
  num_nodes = 0;
  enable_stats = 0;
  stats_file_name = "drlb_stats.log";
  max_stats_size  = 5000000;
  num_stats_to_keep = 3;
  last_stats = 0;
  stats_min = 5;
  stats_secs = 0;

  // Stat reporting members
  connection_total = 0;
  etime_server_start = time(0);

  num_logs_to_keep = 3;
  last_log = 0;
  max_log_size = 5000000;
  enable_buffer_overflow_detection = 0;
  buffer_overflow_size = 6400000;
  server_thread = 0;
  log_thread  = 0;
  stats_thread = 0;
  retries = 3;
  retry_wait = 1;
  timeout_secs = 300;
  timeout_usecs = 0;
  use_timeout = 0;
  server_accept_socket = -1;
  somaxconn = 128;
  connection_total_is_locked = 0;
  assigned_default = LB_RR;
  queue_node_count = -1;
  queue_debug_count = -1;
  queue_proc_count = -1;
  log_queue_size = 2048;
  log_queue_debug_size = 4096;
  log_queue_proc_size = 255;
  loq_queue_nodes = 0;
  loq_queue_debug_nodes = 0;
  loq_queue_proc_nodes = 0;
  queue_node_count_is_locked = 0;
  queue_debug_count_is_locked = 0;
  queue_proc_count_is_locked = 0;
  refactor_scale = 1;
  refactored_connections = 1;
  prev_refactored_connections = 1;
  weight_scale = 1;
  weight_scale_is_locked = 0;
  refactored_connections_is_locked = 0;
  prev_refactored_connections_is_locked = 0;
  has_debug_override = 0;
  has_debug_level_override = 0;
  has_verbose_override = 0;
  has_verbose_level_override = 0;
  has_config_file_override = 0;
  has_log_file_name_override = 0;
  has_log_level_override = 0;
  has_log_file_clear_override = 0;
  has_enable_logging_override = 0;
  has_disable_logging_override = 0;
  has_stats_file_name_override = 0;
  has_enable_stats_override = 0;
  has_disable_stats_override = 0;
  is_client = 0;
  is_client_interactive = 0;
  check_config = 0;
  log_file_err = 0;
  max_idle_count = 60000;
  idle_wait_secs = 0;
  idle_wait_usecs = 500;
  use_nonblock_sockets = 0;
  process_thread = 0;
  process_loop = 1;
  process_is_locked = 0;
  kill_server = 0;
  restart_server = 0;
  enable_throttling = 0;
  throttle_apply_by_load = 0;
  throttle_every_connections = 10;
  throttle_connections_per_sec = 10;
  throttle_wait_secs = 1;
  throttle_wait_usecs = 0;
  throttle_count_is_locked = 0;
  throttle_count = 0;
  connections_per_second = 0;
  connections_per_second_is_locked = 0;
  enable_http_forwarding = 0;
  http_request_strings = "DELETE,GET,HEAD,PATCH,POST,PUT";
  http_requests = 0;
  http_hdr_str = "HTTP/";
  http_eol = "\r\n";
  http_eoh = "\r\n\r\n";
  http_forward_for_str = "X-Forwarded-For";
}

LBconfig::~LBconfig()
{
  if(http_requests) {
    delete [] http_requests;
    http_requests = 0;
  }

  if(client_request_pool) {
    delete client_request_pool;
    client_request_pool = 0;
  }
  if(server_thread) {
    delete server_thread;
    server_thread = 0;
  }
  if(log_thread) {
    delete log_thread;
    log_thread = 0;
  }
  if(stats_thread) {
    delete stats_thread;
    stats_thread = 0;
  }
  if(process_thread) {
    delete process_thread;
    process_thread = 0;
  }
  gxListNode<LB_rule *> *lptr = rules_config_list.GetHead();
  while(lptr) {
    delete lptr->data;
    lptr->data = 0;
    lptr = lptr->next;
  }
  rules_config_list.ClearList();

  gxListNode<LBnode *> *ptr = nodes.GetHead();
  while(ptr) {
    delete ptr->data;
    ptr->data = 0;
    ptr = ptr->next;
  }
  nodes.ClearList();

  if(loq_queue_nodes) {
    delete[] loq_queue_nodes;
    loq_queue_nodes = 0;
  }
  if(loq_queue_debug_nodes) {
    delete[] loq_queue_debug_nodes;
    loq_queue_debug_nodes = 0;
  }
  if(loq_queue_proc_nodes) {
    delete[] loq_queue_proc_nodes;
    loq_queue_proc_nodes = 0;
  }

  if(logfile) logfile.Close();
  if(stats_file) stats_file.Close();
}

unsigned LBconfig::NUM_NODES()
{
  // Num nodes already set
  if(num_nodes > 0) return num_nodes;

  gxListNode<LBnode *> *ptr = nodes.GetHead();
  while(ptr) {
    num_nodes++;
    ptr = ptr->next;
  }
  return num_nodes;
}

unsigned long LBconfig::CONNECTION_TOTAL(int val)
{
  int has_mutex = 1;
  if(connection_total_lock.MutexLock() != 0) has_mutex = 0;
  if(connection_total_is_locked && !has_mutex) return connection_total;
  connection_total_is_locked = 1;

  // ********** Enter Critical Section ******************* //
  if(val > -1) connection_total += val;
  unsigned long buf = connection_total;
  // ********** Leave Critical Section ******************* //

  connection_total_is_locked = 0;
  if(has_mutex) connection_total_lock.MutexUnlock();
  return buf;
}

int LBconfig::QUEUE_NODE_COUNT()
{
  int has_mutex = 1;
  if(queue_node_count_lock.MutexLock() != 0) has_mutex = 0;
  if(queue_node_count_is_locked && !has_mutex) return -1;
  queue_node_count_is_locked = 1; 

  // ********** Enter Critical Section ******************* //
  if(queue_node_count == log_queue_size) queue_node_count = 0; 
  queue_node_count++;
  int buf = queue_node_count;
  // ********** Leave Critical Section ******************* //

  queue_node_count_is_locked = 0; 
  if(has_mutex) queue_node_count_lock.MutexUnlock();
  return buf;
}

int LBconfig::QUEUE_DEBUG_COUNT()
{
  int has_mutex = 1;
  if(queue_debug_count_lock.MutexLock() != 0) has_mutex = 0;
  if(queue_debug_count_is_locked && !has_mutex) return -1;
  queue_debug_count_is_locked = 1;

  // ********** Enter Critical Section ******************* //
  if(queue_debug_count == log_queue_size) queue_debug_count = 0; 
  queue_debug_count++;
  int buf = queue_debug_count;
  // ********** Leave Critical Section ******************* //

  queue_debug_count_is_locked = 0; 
  if(has_mutex) queue_debug_count_lock.MutexUnlock();
  return buf;
}

int LBconfig::QUEUE_PROC_COUNT()
{
  int has_mutex = 1;
  if(queue_proc_count_lock.MutexLock() != 0) has_mutex = 0;
  if(queue_proc_count_is_locked && !has_mutex) return -1;
  queue_proc_count_is_locked = 1; 

  // ********** Enter Critical Section ******************* //
  if(queue_proc_count == log_queue_size) queue_proc_count = 0; 
  queue_proc_count++;
  int buf = queue_proc_count;
  // ********** Leave Critical Section ******************* //

  queue_proc_count_is_locked = 0; 
  if(has_mutex) queue_proc_count_lock.MutexUnlock();
  return buf;
}

LBnode::LBnode() 
{
  node_name  = "LBnode";
  // Set default port and host
  port_number = 8080;
  hostname = "localhost";
  weight = -1; // Not set
  scheme = LB_RR; // Use round robin as default
  max_connections = -1; // Not set
  buffer_size = 1555; // Default read/write buffer size
  num_connections = 0;
  num_connections_is_locked = 0;
  lb_flag_is_locked = 0;
  connection_total_is_locked = 0;
  active_ptr = this;

  // Stat reporting members
  connection_total = 0;
}

void LBnode::Copy(const LBnode &n) 
{
  port_number = n.port_number;
  hostname = n.hostname;
  weight = n.weight;
  scheme = n.scheme;
  max_connections = n.max_connections;
  buffer_size = n.buffer_size;
  num_connections = n.num_connections;
  node_name = n.node_name;
  num_connections_is_locked = 0;
  connection_total_is_locked = 0;
  lb_flag_is_locked = 0;
  active_ptr = n.active_ptr;
  connection_total = n.connection_total;
}

int operator==(const LBnode &a, const LBnode &b) 
{
  return a.weight == b.weight;
}

int operator!=(const LBnode &a, const LBnode &b)
{
  return a.weight != b.weight;
}

int operator<(const LBnode &a, const LBnode &b)
{
  return a.weight < b.weight;
}

int operator>(const LBnode &a, const LBnode &b)
{
  return a.weight > b.weight;
}

int LBnode::LB_FLAG(int flag) 
{
  int has_mutex = 1;
  if(lb_flag_lock.MutexLock() != 0) has_mutex = 0;
  if(lb_flag_is_locked && !has_mutex) return 0;
  lb_flag_is_locked = 1; 

  // ********** Enter Critical Section ******************* //
  if(flag != -1) lb_flag = flag;
  int buf = lb_flag;
  // ********** Leave Critical Section ******************* //

  lb_flag_is_locked = 0; 
  if(has_mutex) lb_flag_lock.MutexUnlock();
  return buf;
}

unsigned long LBnode::CONNECTION_TOTAL(int val)
{
  int has_mutex = 1;
  if(connection_total_lock.MutexLock() != 0) has_mutex = 0;
  if(connection_total_is_locked && !has_mutex) return connection_total;
  connection_total_is_locked = 1;

  // ********** Enter Critical Section ******************* //
  if(val > -1) connection_total += val;
  unsigned long buf = connection_total;
  // ********** Leave Critical Section ******************* //

  connection_total_is_locked = 0;
  if(has_mutex) connection_total_lock.MutexUnlock();
  return buf;
}

unsigned LBnode::NUM_CONNECTIONS(int inc, int dec, unsigned set_to)
{
  int has_mutex = 1;
  if(num_connections_lock.MutexLock() != 0) has_mutex = 0;
  if(num_connections_is_locked && !has_mutex) return num_connections;
  num_connections_is_locked = 1; 

  // ********** Enter Critical Section ******************* //
  if(num_connections < 0) num_connections = 0;
  if(inc > 0) num_connections ++;
  if(dec > 0) {
    num_connections--;
    if(num_connections < 0) num_connections = 0;
  }
  if(set_to > 0) num_connections = set_to;
  unsigned buf = num_connections; 
  // ********** Leave Critical Section ******************* //

  num_connections_is_locked = 0; 
  if(has_mutex) num_connections_lock.MutexUnlock();
  return buf;
} 

int ProcessDashDashArg(gxString &arg)
{
  gxString sbuf, equal_arg;
  int has_arg_errors = 0;
  int has_arg = 0;

  if(arg.Find("=") != -1) {
    // Look for equal arguments
    // --config-file="/etc/program.cfg"
    equal_arg = arg;
    equal_arg.DeleteBeforeIncluding("=");
    arg.DeleteAfterIncluding("=");
    equal_arg.TrimLeading(' '); equal_arg.TrimTrailing(' ');
    equal_arg.TrimLeading('\"'); equal_arg.TrimTrailing('\"');
    equal_arg.TrimLeading('\''); equal_arg.TrimTrailing('\'');
  }
  
  arg.ToLower();
  
  // Process all -- arguments here
  if(arg == "help") {
    HelpMessage(servercfg->ProgramName.c_str());
    ExitProc(1); // Signal program to exit
  }
  if(arg == "?") {
    HelpMessage(servercfg->ProgramName.c_str());
    ExitProc(1); // Signal program to exit
  }
  if((arg == "version") || (arg == "ver")) {
    VersionMessage();
    ExitProc(1); // Signal program to exit
  }

  if(arg == "verbose") {
    servercfg->has_verbose_override = 1;
    servercfg->verbose = 1;
    has_arg = 1;
  }

  if(arg == "verbose-level") {
    if(equal_arg.is_null()) { 
      servercfg->verbose = 1;
      NT_print("Error no verbose level supplied with the --verbose-level swtich");
      has_arg_errors++;
    }
    else {
      servercfg->has_verbose_level_override = 1;
      servercfg->verbose_level = equal_arg.Atoi(); 
      if(servercfg->verbose_level <= 0) servercfg->verbose_level = 1;
      has_arg = 1;
    }
  }

  if(arg == "debug") {
    servercfg->has_debug_override = 1;
    servercfg->debug = 1;
    has_arg = 1;
  }

  if(arg == "debug-level") {
    if(equal_arg.is_null()) { 
      servercfg->verbose = 1;
      NT_print("Error no debug level supplied with the --debug-level swtich");
      has_arg_errors++;
    }
    else {
      servercfg->has_debug_level_override = 1;
      servercfg->debug = 1;
      servercfg->debug_level = equal_arg.Atoi(); 
      if(servercfg->debug_level <= 0) servercfg->debug_level = 1;
      has_arg = 1;
    }
  }
  if(arg == "check-config") {
    servercfg->check_config = 1;
    servercfg->is_client = 1;
    servercfg->is_client_interactive = 1;
    has_arg = 1;
  }
  if(arg == "config-file") {
    if(equal_arg.is_null()) { 
      servercfg->verbose = 1;
      NT_print("Error no config file name supplied with the --config-file swtich"); 
      has_arg_errors++;
    }
    else {
      servercfg->has_config_file_override = 1;
      servercfg->config_file = equal_arg;
      servercfg->user_config_file = 1;
      has_arg = 1;
    }
  }

  if(arg == "log-file") {
    if(equal_arg.is_null()) { 
      servercfg->verbose = 1;
      NT_print("Error no LOG file name supplied with the --log-file swtich");
      has_arg_errors++;
    }
    else {
      servercfg->has_log_file_name_override = 1;
      servercfg->logfile_name = equal_arg;
      servercfg->enable_logging = 1;
      has_arg = 1;
    }
  }

  if(arg == "log-file-clear") {
    servercfg->has_log_file_clear_override = 1;
    servercfg->clear_log = 1;
    has_arg = 1;
  }

  if(arg == "log-disable") {
    servercfg->has_disable_logging_override = 1;
    servercfg->enable_logging = 0;
    has_arg = 1;
  }

  if(arg == "log-level") {
    if(equal_arg.is_null()) { 
      servercfg->verbose = 1;
      NT_print("Error no debug level supplied with the --log-level swtich");
      has_arg_errors++;
    }
    else {
      servercfg->has_log_level_override = 1;
      servercfg->log_level = equal_arg.Atoi(); 
      if(servercfg->log_level < 0) servercfg->log_level = 0;
      has_arg = 1;
    }
  }

  if(arg == "stats-file") {
    if(equal_arg.is_null()) { 
      servercfg->verbose = 1;
      NT_print("Error no STATS file name supplied with the --stats-file swtich");
      has_arg_errors++;
    }
    else {
      servercfg->has_stats_file_name_override = 1;
      servercfg->stats_file_name = equal_arg;
      servercfg->enable_stats = 1;
      has_arg = 1;
    }
  }
  if(arg == "stats-disable") {
    servercfg->has_disable_stats_override = 1;
    servercfg->enable_stats = 0;
    has_arg = 1;
  }

  arg.Clear(); 
  if(has_arg == 0) has_arg_errors++;
  
  return has_arg_errors;
}

int ProcessArgs(int argc, char *argv[])
{
  // process the program's argument list
  int i;
  gxString sbuf;
  int has_arg_errors = 0;

  for(i = 1; i < argc; i++ ) {
    if(*argv[i] == '-') {
      char sw = *(argv[i] +1);
      switch(sw) {

	case '-':
	  sbuf = &argv[i][2]; 
	  // Add all -- prepend filters here
	  sbuf.TrimLeading('-');
	  if(ProcessDashDashArg(sbuf) != 0) has_arg_errors++;
	  break;

	case '?':
	  HelpMessage(servercfg->ProgramName.c_str());
	  ExitProc(1); // Signal program to exit
	  break;
	  
	case 'v': case 'V': 
	  servercfg->verbose = 1;
	  servercfg->has_verbose_override = 1;
	  break;

	case 'd': case 'D':
	  servercfg->has_debug_override = 1;
	  servercfg->debug = 1;
	  break;

	default:
	  servercfg->verbose = 1; 
	  sbuf << clear << "Unknown command line arg " << argv[i];
	  NT_print(sbuf.c_str());
	  NT_print("Exiting with errors");
	  ExitProc(1); // Signal program to exit
	  break;
      }
    }
  }

  return has_arg_errors;
}

// Get the parm name before the = sign
void CfgGetParmName(const gxString &cfgline, gxString &parm_name)
{
  parm_name = cfgline;
  parm_name.DeleteAfterIncluding("=");
  parm_name.DeleteAfterIncluding("\n");
  parm_name.TrimLeadingSpaces();
  parm_name.TrimTrailingSpaces();
}

// Get the parm after = sign
void CfgGetEqualToParm(const gxString &cfgline, gxString &parm)
{
  parm = cfgline;
  parm.DeleteBeforeIncluding("=");
  parm.DeleteAfterIncluding("\n");
  parm.TrimLeadingSpaces();
  parm.TrimTrailingSpaces();
}

int CfgGetTrueFalseValue(const gxString &cfgline) 
{
  gxString parm;
  CfgGetEqualToParm(cfgline, parm);
  parm.ToUpper();
  if((parm == "TRUE") || (parm == "T") || (parm == "YES") || (parm == "Y")) return 1;
  if((parm == "FALSE") || (parm == "F") || (parm == "NO") || (parm == "N")) return 0;
  if(parm.Atoi() >= 1) return 1;
  if(parm.Atoi() <= 0) return 0;
  return 0;
}

int CfgGetTrueFalseValue(const gxString &cfgline, gxString &parm) 
{
  int rv = CfgGetTrueFalseValue(cfgline);
  if(rv == 1) {
    parm = "TRUE";
    return 1;
  }
  parm = "FALSE";
  return 0;
}

int CfgGetEqualToParm(const gxString &cfgline)
{
  gxString parm;
  CfgGetEqualToParm(cfgline, parm);
  return parm.Atoi();
}

char *CfgGetEqualToParm(const gxString &cfgline, char sbuf[255])
{
  memset(sbuf, 0, 255);
  gxString parm;
  CfgGetEqualToParm(cfgline, parm);
  strcpy(sbuf, parm.c_str());
  return sbuf;
}

int LoadOrBuildConfigFile()
{
  NT_print("Loading or building config file", servercfg->config_file.c_str());
  
  if(!futils_exists(servercfg->config_file.c_str())) {
    NT_print("DRLB configuration file not found. Building default file and exiting");
    DiskFileB dfile(servercfg->config_file.c_str(), DiskFileB::df_READWRITE, DiskFileB::df_CREATE); 
    if(!dfile) {
      NT_print("ERROR - Cannot write to config file", servercfg->config_file.c_str());
      return 3;
    }
    dfile << "# " << GetProgramDescription() << " " << GetVersionString() << " configuration file" << "\n";
    dfile << "#" << "\n";
    dfile << "\n";
    dfile << "# Default LB config file" << "\n";
    dfile << "\n";
    dfile << "# Global config" << "\n";
    dfile << "[LBSERVER]" << "\n";
    dfile << "port = 8080 # All incoming traffic connections here" << "\n";
    dfile << "#" << "\n";
    dfile << "# Set to accept clients on single interface" << "\n";
    dfile << "##listen_ip_addr = 0.0.0.0" << "\n";
    dfile << "#" << "\n";
    dfile << "# LB_ASSIGNED scheme requires a rules config file" << "\n";
    dfile << "# A default rules config file will be build if specified file does not exist" << "\n";
    dfile << "##scheme = LB_ASSIGNED" << "\n";
    dfile << "##rules_config_file = drlb_server_rules.cfg" << "\n";
    dfile << "##resolve_assigned_hostnames = 1" << "\n";
    dfile << "##assigned_default = LB_RR # Use round robin for node fail over" << "\n";
    dfile << "##assigned_default = LB_DISTRIB  # Use distib for node fail over" << "\n";
    dfile << "##assigned_default = LB_WEIGHTED # Use weighted for node fail over" << "\n";
    dfile << "# Set assigned_default to LB_NONE to disable node fail over, meaning:" << "\n";
    dfile << "# If the assigned node is not available the client connection will fail" << "\n";
    dfile << "##assigned_default = LB_NONE" << "\n";
    dfile << "#" << "\n";
    dfile << "##scheme = LB_DISTRIB" << "\n";
    dfile << "##scheme = LB_WEIGHTED" << "\n";
    dfile << "# Use Round Robin as default LB scheme" << "\n";
    dfile << "scheme = LB_RR" << "\n";
    dfile << "use_buffer_cache = 1" << "\n";
    dfile << "buffer_size = 1500" << "\n";
    dfile << "max_connections = -1 # Not set, if set will limit number of connections this server" << "\n";
    dfile << "max_threads = -1 # Not set, if set will limit number of threads for this process" << "\n";
    dfile << "#" << "\n";
    dfile << "# Log settings" << "\n";
    dfile << "# Keep log file plus last 3. Disk space will be: (max_log_size * (num_logs_to_keep +1))" << "\n";
    dfile << "num_logs_to_keep = 3" << "\n";
    dfile << "# Set max size per log file, max is 2000000000" << "\n";
    dfile << "max_log_size = 5000000 # Default is 5M, max is 2G" << "\n";
    dfile << "log_queue_size = 2048 # Max number of log or console messages to queue" << "\n";
    dfile << "log_queue_debug_size = 4096 # Max number of debug messages to queue" << "\n";
    dfile << "log_queue_proc_size = 255 # Max number of process messages to queue" << "\n";
    dfile << "# Values below can be set here or args when program is launched" << "\n";
    dfile << "##clear_log = 0" << "\n";
    dfile << "##enable_logging = 0" << "\n";
    dfile << "# Log levels 0-5, 0 lowest log level, 5 highest log level " << "\n";
    dfile << "##log_level = 0" << "\n";
    dfile << "##logfile_name = /var/log/drlb/drlb_server.log" << "\n";
    dfile << "#" << "\n";
    dfile << "# Stats settings" << "\n";
    dfile << "# Keep stats file plus last 3. Disk space will be: (max_stats_size * (num_stats_to_keep +1))" << "\n";
    dfile << "num_stats_to_keep = 3" << "\n";
    dfile << "# Set max size per stats file, max is 2000000000" << "\n";
    dfile << "max_stats_size = 5000000 # Default is 5M, max is 2G" << "\n";
    dfile << "stats_min = 5 # Generate a stats report every 5 minutes" << "\n";
    dfile << "stats_secs = 0 # If min=0 gen a stats report less than every min" << "\n";
    dfile << "##enable_stats = 0" << "\n";
    dfile << "##stats_file_name = /var/log/drlb/drlb_stats.log" << "\n";
    dfile << "#" << "\n";
    dfile << "# Set max number of back logged connections" << "\n";
    dfile << "# Should match: cat /proc/sys/net/core/somaxconn" << "\n";
    dfile << "somaxconn = 128" << "\n";
    dfile << "#" << "\n";
    dfile << "# Buffer overflow detection with using cached reads, disabled by default" << "\n";
    dfile << "enable_buffer_overflow_detection = 0" << "\n";
    dfile << "buffer_overflow_size = 6400000" << "\n";
    dfile << "#" << "\n";
    dfile << "# Retries for backend node connection errors" << "\n";
    dfile << "retries = 3" << "\n";
    dfile << "retry_wait = 1" << "\n";
    dfile << "#" << "\n";
    dfile << "# Use timeout on blocking reads" << "\n";
    dfile << "use_timeout = 0" << "\n";
    dfile << "timeout_secs = 300" << "\n";
    dfile << "timeout_usecs = 0" << "\n";
    dfile << "#" << "\n";
    dfile << "# Service name that appears in log file" << "\n";
    dfile << "##service_name = drlb_server" << "\n";
    dfile << "#" << "\n";
    dfile << "# Idle time for blocking sockets" << "\n";
    dfile << "max_idle_count = 60000" << "\n";
    dfile << "idle_wait_secs = 0"  << "\n";
    dfile << "idle_wait_usecs = 500" << "\n";
    dfile << "# Use non-blocking sockets" << "\n";
    dfile << "use_nonblock_sockets = 0" << "\n";
    dfile << "#" << "\n";
    dfile << "# " << "\n";
    dfile << "# Settings for throttling connections" << "\n";
    dfile << "# Throttling is used to prevent backend server saturation during peak load events" << "\n";
    dfile << "#" << "\n";
    dfile << "# Enable throttling" << "\n";
    dfile << "##enable_throttling = 0" << "\n";
    dfile << "# Apply throttle control only when load reached a certain number of connection per second" << "\n";
    dfile << "# If apply by load is set to 0, we will throttle based on the connection count" << "\n";
    dfile << "##throttle_apply_by_load = 0" << "\n";
    dfile << "# Set the number of connection per second to start throttling connections" << "\n";
    dfile << "##throttle_connections_per_sec = 10" << "\n";
    dfile << "# Set the number of connections to start throttling" << "\n";
    dfile << "# A setting of 1 will throttle every connection" << "\n";
    dfile << "##throttle_every_connections = 10" << "\n";
    dfile << "# Set the time in seconds or microseconds to hold a connection in the throttle queue" << "\n";
    dfile << "##throttle_wait_secs = 1" << "\n";
    dfile << "##throttle_wait_usecs = 0" << "\n";
    dfile << "#" << "\n";
    dfile << "# HTTP IP address forwarding" << "\n";
    dfile << "##enable_http_forwarding = 0" << "\n";
    dfile << "##http_request_strings = DELETE,GET,HEAD,PATCH,POST,PUT" << "\n";
    dfile << "##http_hdr_str = HTTP/" << "\n";
    dfile << "##http_forward_for_str = X-Forwarded-For" << "\n";
    dfile << "#" << "\n";
    dfile << "# Values below can be set here or args when program is launched" << "\n";
    dfile << "# Debug and verbose modes used mainly for development and testing" << "\n";
    dfile << "# NOTE: Debug level 5 will greatly increase log file size" << "\n";
    dfile << "# NOTE: Verbose mode will echo messages to the console, used mainly to test config files" << "\n";
    dfile << "##debug = 0" << "\n";
    dfile << "##debug_level = 1" << "\n";
    dfile << "##verbose = 0" << "\n";
    dfile << "##verbose_level = 1" << "\n";
    dfile << "\n";
    dfile << "# Node Configs, define a [LBNODE] section for each node" << "\n";
    dfile << "# The default config uses 4 nodes as an example" << "\n";
    dfile << "# Replace 192.168.122 address with the IP address or hostname of the backend node" << "\n";
    dfile << "# running the service you want to load balance." << "\n";
    dfile << "[LBNODE]" << "\n";
    dfile << "node_name = lbnode1" << "\n";
    dfile << "port_number = 80   # Port backend host is listening on " << "\n";
    dfile << "hostname = 192.168.122.111 # Backend host" << "\n";
    dfile << "weight = -1 # Must be set if using LB_WEIGHTED or LB_DISTRIB scheme" << "\n";
    dfile << "max_connections = -1 # Not set, if set will limit number of connection to this node" << "\n";
    dfile << "buffer_size = -1 # Use global size" << "\n";
    dfile << "\n";
    dfile << "[LBNODE]" << "\n";
    dfile << "node_name = lbnode2" << "\n";
    dfile << "port_number = 80" << "\n";
    dfile << "hostname = 192.168.122.112" << "\n";
    dfile << "weight = -1 # Must be set if using LB_WEIGHTED or LB_DISTRIB scheme" << "\n";
    dfile << "max_connections = -1 # Not set, if set will limit number of connection to this node" << "\n";
    dfile << "buffer_size = -1 # Use global size" << "\n";
    dfile << "\n";
    dfile << "[LBNODE]" << "\n";
    dfile << "node_name = lbnode3" << "\n";
    dfile << "port_number = 80" << "\n";
    dfile << "hostname = 192.168.122.113" << "\n";
    dfile << "weight = -1 # Must be set if using LB_WEIGHTED or LB_DISTRIB scheme" << "\n";
    dfile << "max_connections = -1 # Not set, if set will limit number of connection to this node" << "\n";
    dfile << "buffer_size = -1 # Use global size" << "\n";
    dfile << "\n";
    dfile << "[LBNODE]" << "\n";
    dfile << "node_name = lbnode4" << "\n";
    dfile << "port_number = 80" << "\n";
    dfile << "hostname = 192.168.122.114" << "\n";
    dfile << "weight = -1 # Must be set if using LB_WEIGHTED or LB_DISTRIB scheme" << "\n";
    dfile << "max_connections = -1 # Not set, if set will limit number of connection to this node" << "\n";
    dfile << "buffer_size = -1 # Use global size" << "\n";
    dfile << "\n";
    dfile << "# End of configuration file" << "\n";
    dfile << "\n";
    dfile.df_Close();
    return 2;
  }
  
  char sbuf[1024];
  DiskFileB ifile;
  
  ifile.df_Open(servercfg->config_file.c_str());
  if(!ifile) {
    NT_print("ERROR - Cannot open config file for reading", servercfg->config_file.c_str());
    NT_print(ifile.df_ExceptionMessage());
    return 1;
  }
  
  UString info_line;
  int error_level = 0;
  
  gxList<gxString> list;
  
  while(!ifile.df_EOF()) {
    ifile.df_GetLine(sbuf, sizeof(sbuf), '\n');
    if(ifile.df_GetError() != DiskFileB::df_NO_ERROR) {
      NT_print("ERROR - A fatal I/O error reading", servercfg->config_file.c_str());
      NT_print(ifile.df_ExceptionMessage());
      error_level = 1;
      break;
    }
    
    info_line = sbuf;
    info_line.FilterChar('\n');    
    info_line.FilterChar('\r');    
    info_line.TrimLeadingSpaces();
    info_line.TrimTrailingSpaces();
    
    // Skip remark lines
    if(info_line[0] == '#') continue; 
    if(info_line[0] == ';') continue;
    
    // Filter inline remarks
    info_line.DeleteAfterIncluding("#");
    info_line.TrimTrailingSpaces();
    
    info_line << "\n";
    
    list.Add(info_line);
  }
    
  gxListNode<gxString> *ptr;
  int has_lbserver = 0;
  int has_lbnode = 0;
  ptr = list.GetHead();
  char pbuf[255];
  gxString parm;
  int has_scheme = 0;
  gxString parm_name;
  
  while(ptr) {
    gxString strdup = ptr->data;
    strdup.FilterChar(' ');
    if(strdup.IFind("[LBSERVER]") != -1) {
      has_lbserver = 1;
      ptr = ptr->next;
      while(ptr) {
	if(ptr->data[0] == '[') {
	  ptr = ptr->prev;
	  break;
	}
	if(ptr->data.IFind("verbose") != -1) { // Check verbose mode first
	  CfgGetParmName(ptr->data, parm_name); parm_name.ToLower();
	  if(parm_name == "verbose") {
	    if(!servercfg->has_verbose_override) servercfg->verbose = CfgGetTrueFalseValue(ptr->data);
	  }
	}
	if(ptr->data.IFind("verbose_level") != -1) {
	  CfgGetParmName(ptr->data, parm_name); parm_name.ToLower();
	  if(parm_name == "verbose_level") {
	    if(!servercfg->has_verbose_level_override) servercfg->verbose_level = CfgGetEqualToParm(ptr->data);
	  }
	}
	if(ptr->data.IFind("debug") != -1) { // Set the debug level second
	  CfgGetParmName(ptr->data, parm_name); parm_name.ToLower();
	  if(parm_name == "debug") {
	    if(!servercfg->has_debug_override) servercfg->debug = CfgGetTrueFalseValue(ptr->data);
	  }
	}
	if(ptr->data.IFind("debug_level") != -1) {
	  CfgGetParmName(ptr->data, parm_name); parm_name.ToLower();
	  if(parm_name == "debug_level") {
	    if(!servercfg->has_debug_level_override) servercfg->debug_level = CfgGetEqualToParm(ptr->data);
	  }
	}
	if(ptr->data.IFind("log_level") != -1) { // Set the log level and check log settings third
	  CfgGetParmName(ptr->data, parm_name); parm_name.ToLower();
	  if(parm_name == "log_level") {
	    if(!servercfg->has_log_level_override) servercfg->log_level = CfgGetEqualToParm(ptr->data);
	  }
	}
	if(!servercfg->has_disable_logging_override) { 
	  if(ptr->data.IFind("clear_log") != -1) {
	    CfgGetParmName(ptr->data, parm_name); parm_name.ToLower();
	    if(parm_name == "clear_log") {
	      if(!servercfg->has_log_file_clear_override) servercfg->clear_log = CfgGetTrueFalseValue(ptr->data);
	    }
	  }
	  if(ptr->data.IFind("logfile_name") != -1) {
	    CfgGetParmName(ptr->data, parm_name); parm_name.ToLower();
	    if(parm_name == "logfile_name") {
	      if(!servercfg->has_log_file_name_override) servercfg->logfile_name = CfgGetEqualToParm(ptr->data, pbuf);
	    }
	  }
	  if(ptr->data.IFind("enable_logging") != -1) {
	    CfgGetParmName(ptr->data, parm_name); parm_name.ToLower();
	    if(parm_name == "enable_logging") {
	      if(!servercfg->has_enable_logging_override) servercfg->enable_logging = CfgGetTrueFalseValue(ptr->data);
	    }
	  }
	}
	//
	// Add all other DRLB conifg reads here
	//
	if(ptr->data.IFind("scheme") != -1) {
	  CfgGetParmName(ptr->data, parm_name); parm_name.ToLower();
	  if(parm_name == "scheme") {
	    has_scheme++;
	    CfgGetEqualToParm(ptr->data, parm);
	    if(parm.IFind("LB_ASSIGNED") != -1) {
	      servercfg->scheme = LB_ASSIGNED;
	    }
	    else if(parm.IFind("LB_DISTRIB") != -1) {
	      servercfg->scheme = LB_DISTRIB;
	    }
	    else if(parm.IFind("LB_RR") != -1) {
	      servercfg->scheme = LB_RR;
	    }
	    else if(parm.IFind("LB_WEIGHTED") != -1) {
	      servercfg->scheme = LB_WEIGHTED;
	    }
	    else {
	      NT_print("Config file has bad scheme value", parm.c_str());
	      error_level = 1;
	    }
	    if(has_scheme > 1) {
	      NT_print("Config file has more than one scheme defined");
	      error_level = 1;
	    }
	  }
	}
	if(ptr->data.IFind("use_buffer_cache") != -1) {
	  servercfg->use_buffer_cache = CfgGetTrueFalseValue(ptr->data);
	}
	if(ptr->data.IFind("buffer_size") != -1) {
	  servercfg->buffer_size = CfgGetEqualToParm(ptr->data);
	  if(servercfg->buffer_size <= 0) {
	    NT_print("Config file has bad buffer_size value");
	    error_level = 1;
	  }
	}
	if(ptr->data.IFind("max_threads") != -1) {
	  servercfg->max_threads = CfgGetEqualToParm(ptr->data);
	}
	if(ptr->data.IFind("max_connections") != -1) {
	  servercfg->max_connections = CfgGetEqualToParm(ptr->data);
	}
	if(!servercfg->has_disable_stats_override) {
	  if(ptr->data.IFind("stats_file_name") != -1) {
	    if(!servercfg->has_stats_file_name_override) servercfg->stats_file_name = CfgGetEqualToParm(ptr->data, pbuf);
	  }
	  if(ptr->data.IFind("enable_stats") != -1) {
	    if(!servercfg->has_enable_stats_override) servercfg->enable_stats = CfgGetTrueFalseValue(ptr->data);
	  }
	}
	if(ptr->data.IFind("num_stats_to_keep") != -1) {
	  servercfg->num_stats_to_keep = CfgGetEqualToParm(ptr->data);
	  if(servercfg->num_stats_to_keep <= 0) {
	    NT_print("Config file has bad num_stats_to_keep value");
	    error_level = 1;
	  }
	}
	if(ptr->data.IFind("max_stats_size") != -1) {
	  gxString lsbuf = CfgGetEqualToParm(ptr->data, pbuf);
	  servercfg->max_stats_size = lsbuf.Atol();
	  if(servercfg->max_stats_size > 2000000000) {
	    NT_print("Config max_stats_size is over 2GB");
	    error_level =1;
	  }
	  if(servercfg->max_stats_size <= 0) {
	    NT_print("Config file has bad max_ststs_size");
	    error_level = 1;
	  }
	}
	if(ptr->data.IFind("stats_min") != -1) {
	  servercfg->stats_min = CfgGetEqualToParm(ptr->data);
	  if(servercfg->stats_min < 0) {
	    NT_print("Config file has bad stats_min value");
	    error_level = 1;
	  }
	}
	if(ptr->data.IFind("stats_secs") != -1) {
	  servercfg->stats_secs = CfgGetEqualToParm(ptr->data);
	  if(servercfg->stats_secs < 0) {
	    NT_print("Config file has bad stats_secs value");
	    error_level = 1;
	  }
	}
	if(ptr->data.IFind("service_name") != -1) {
	  servercfg->service_name = CfgGetEqualToParm(ptr->data, pbuf);
	}
	if(ptr->data.IFind("num_logs_to_keep") != -1) {
	  servercfg->num_logs_to_keep = CfgGetEqualToParm(ptr->data);
	  if(servercfg->num_logs_to_keep <= 0) {
	    NT_print("Config file has bad num_logs_to_keep value");
	    error_level = 1;
	  }
	}
	if(ptr->data.IFind("max_log_size") != -1) {
	  gxString lbuf = CfgGetEqualToParm(ptr->data, pbuf);
	  servercfg->max_log_size = lbuf.Atol();
	  if(servercfg->max_log_size > 2000000000) {
	    NT_print("Config max_log_size is over 2GB");
	    error_level =1;
	  }
	  if(servercfg->max_log_size <= 0) {
	    NT_print("Config file has bad max_log_size");
	    error_level = 1;
	  }
	}
	if(ptr->data.IFind("enable_buffer_overflow_detection") != -1) {
	  servercfg->enable_buffer_overflow_detection = CfgGetTrueFalseValue(ptr->data);
	}
	if(ptr->data.IFind("buffer_overflow_size") != -1) {
	  servercfg->buffer_overflow_size = CfgGetEqualToParm(ptr->data);
	  if(servercfg->buffer_overflow_size <= 0) {
	    NT_print("Config file has bad buffer_overflow_size value");
	    error_level = 1;
	  }
	}
	if(ptr->data.IFind("log_queue_size") != -1) {
	  servercfg->log_queue_size = CfgGetEqualToParm(ptr->data);
	  if(servercfg->log_queue_size <= 0) {
	    NT_print("Config file has bad log_queue_size value");
	    error_level = 1;
	  }
	}
	if(ptr->data.IFind("log_queue_debug_size") != -1) {
	  servercfg->log_queue_debug_size = CfgGetEqualToParm(ptr->data);
	  if(servercfg->log_queue_debug_size <= 0) {
	    NT_print("Config file has bad log_queue_debug_size value");
	    error_level = 1;
	  }
	}
	if(ptr->data.IFind("log_queue_proc_size") != -1) {
	  servercfg->log_queue_proc_size = CfgGetEqualToParm(ptr->data);
	  if(servercfg->log_queue_proc_size <= 0) {
	    NT_print("Config file has bad log_queue_proc_size value");
	    error_level = 1;
	  }
	}
	if(ptr->data.IFind("retries") != -1) {
	  CfgGetParmName(ptr->data, parm_name); parm_name.ToLower();
	  if(parm_name == "retries") {
	    servercfg->retries = CfgGetEqualToParm(ptr->data);
	    if(servercfg->retries < 0) {
	      NT_print("Config file has bad retries value");
	      error_level = 1;
	    }
	  }
	}
	if(ptr->data.IFind("retry_wait") != -1) {
	  servercfg->retry_wait = CfgGetEqualToParm(ptr->data);
	  if(servercfg->retry_wait < 0) {
	    NT_print("Config file has bad retry_wait value");
	    error_level = 1;
	  }
	}
	if(ptr->data.IFind("timeout_secs") != -1) {
	  servercfg->timeout_secs = CfgGetEqualToParm(ptr->data);
	  if(servercfg->timeout_secs < 0) {
	    NT_print("Config file has bad timeout_secs value");
	    error_level = 1;
	  }
	}
	if(ptr->data.IFind("timeout_usecs") != -1) {
	  servercfg->timeout_usecs = CfgGetEqualToParm(ptr->data);
	  if(servercfg->timeout_usecs < 0) {
	    NT_print("Config file has bad timeout_usecs value");
	    error_level = 1;
	  }
	}
	if(ptr->data.IFind("use_nonblock_sockets") != -1) {
	  servercfg->use_nonblock_sockets = CfgGetTrueFalseValue(ptr->data);
	}
	if(ptr->data.IFind("max_idle_count") != -1) {
	  servercfg->max_idle_count = CfgGetEqualToParm(ptr->data);
	  if(servercfg->max_idle_count < 0) {
	    NT_print("Config file has bad max_idle_count value");
	    error_level = 1;
	  }
	}
	if(ptr->data.IFind("idle_wait_secs") != -1) {
	  servercfg->idle_wait_secs = CfgGetEqualToParm(ptr->data);
	  if(servercfg->idle_wait_secs < 0) {
	    NT_print("Config file has bad idle_wait_secs value");
	    error_level = 1;
	  }
	}
	if(ptr->data.IFind("idle_wait_usecs") != -1) {
	  servercfg->idle_wait_usecs = CfgGetEqualToParm(ptr->data);
	  if(servercfg->idle_wait_usecs < 0) {
	    NT_print("Config file has bad idle_wait_usecs value");
	    error_level = 1;
	  }
	}
	if(ptr->data.IFind("use_timeout") != -1) {
	  servercfg->use_timeout = CfgGetTrueFalseValue(ptr->data);
	}
	if(ptr->data.IFind("port") != -1) {
	  CfgGetParmName(ptr->data, parm_name); parm_name.ToLower();
	  if(parm_name == "port") {
	    servercfg->port = CfgGetEqualToParm(ptr->data);
	    if(servercfg->port <= 0) {
	      NT_print("Config file has bad server port value");
	      error_level = 1;
	    }
	  }
	}
	if(ptr->data.IFind("listen_ip_addr") != -1) {
	    CfgGetParmName(ptr->data, parm_name); parm_name.ToLower();
	    if(parm_name == "listen_ip_addr") {
	      servercfg->listen_ip_addr = CfgGetEqualToParm(ptr->data, pbuf);
	    }
	}
	if(ptr->data.IFind("somaxconn") != -1) {
	  CfgGetParmName(ptr->data, parm_name); parm_name.ToLower();
	  if(parm_name == "somaxconn") {
	    servercfg->somaxconn = CfgGetEqualToParm(ptr->data);
	    if(servercfg->somaxconn <= 0) {
	      NT_print("Config file has bad somaxconn value");
	      error_level = 1;
	    }
	  }
	}
	if(ptr->data.IFind("resolve_assigned_hostnames") != -1) {
	  CfgGetParmName(ptr->data, parm_name); parm_name.ToLower();
	  if(parm_name == "resolve_assigned_hostnames") {
	    servercfg->resolve_assigned_hostnames = CfgGetTrueFalseValue(ptr->data);
	  }
	}
	if(ptr->data.IFind("assigned_default") != -1) {
	  CfgGetEqualToParm(ptr->data, parm);
	  if(parm.IFind("LB_ASSIGNED") != -1) {
	    NT_print("Config file error, assigned_default cannot equal LB_ASSIGNED", parm.c_str());
	    error_level = 1;
	  }
	  else if(parm.IFind("LB_DISTRIB") != -1) {
	    servercfg->assigned_default = LB_DISTRIB;
	  }
	  else if(parm.IFind("LB_RR") != -1) {
	    servercfg->assigned_default = LB_RR;
	  }
	  else if(parm.IFind("LB_WEIGHTED") != -1) {
	    servercfg->assigned_default = LB_WEIGHTED;
	  }
	  else if(parm.IFind("LB_NONE") != -1) {
	    servercfg->assigned_default = LB_NONE;
	  }
	  else {
	    NT_print("Config file has bad assigned_default scheme default value", parm.c_str());
	    error_level = 1;
	  }
	}
	if(ptr->data.IFind("rules_config_file") != -1) {
	  servercfg->rules_config_file = CfgGetEqualToParm(ptr->data, pbuf);
	}
	
	if(ptr->data.IFind("enable_throttling") != -1) {
	  CfgGetParmName(ptr->data, parm_name); parm_name.ToLower();
	  if(parm_name == "enable_throttling") {
	    servercfg->enable_throttling = CfgGetTrueFalseValue(ptr->data);
	  }
	}
	if(ptr->data.IFind("throttle_apply_by_load") != -1) {
	  CfgGetParmName(ptr->data, parm_name); parm_name.ToLower();
	  if(parm_name == "throttle_apply_by_load") {
	    servercfg->throttle_apply_by_load = CfgGetTrueFalseValue(ptr->data);
	  }
	}
	if(ptr->data.IFind("throttle_connections_per_sec") != -1) {
	  CfgGetParmName(ptr->data, parm_name); parm_name.ToLower();
	  if(parm_name == "throttle_connections_per_sec") {
	    servercfg->throttle_connections_per_sec = CfgGetEqualToParm(ptr->data);
	    if(servercfg->throttle_connections_per_sec <= 0) {
	      NT_print("Config file has bad throttle_connections_per_sec value");
	      error_level = 1;
	    }
	  }
	}
	if(ptr->data.IFind("throttle_every_connections") != -1) {
	  CfgGetParmName(ptr->data, parm_name); parm_name.ToLower();
	  if(parm_name == "throttle_every_connections") {
	    servercfg->throttle_every_connections = CfgGetEqualToParm(ptr->data);
	    if(servercfg->throttle_every_connections <= 0) {
	      NT_print("Config file has bad throttle_every_connections value");
	      error_level = 1;
	    }
	  }
	}
	
	if(ptr->data.IFind("throttle_wait_secs") != -1) {
	  CfgGetParmName(ptr->data, parm_name); parm_name.ToLower();
	  if(parm_name == "throttle_wait_secs") {
	    servercfg->throttle_wait_secs = CfgGetEqualToParm(ptr->data);
	    if(servercfg->throttle_wait_secs < 0) {
	      NT_print("Config file has bad throttle_wait_secs value");
	      error_level = 1;
	    }
	  }
	}
	if(ptr->data.IFind("throttle_wait_usecs") != -1) {
	  CfgGetParmName(ptr->data, parm_name); parm_name.ToLower();
	  if(parm_name == "throttle_wait_usecs") {
	    servercfg->throttle_wait_usecs = CfgGetEqualToParm(ptr->data);
	    if(servercfg->throttle_wait_usecs < 0) {
	      NT_print("Config file has bad throttle_wait_usecs value");
	      error_level = 1;
	    }
	  }
	}
	if(ptr->data.IFind("enable_http_forwarding") != -1) {
	  CfgGetParmName(ptr->data, parm_name); parm_name.ToLower();
	  if(parm_name == "enable_http_forwarding") {
	    servercfg->enable_http_forwarding = CfgGetTrueFalseValue(ptr->data);
	  }
	}
	if(ptr->data.IFind("http_request_strings") != -1) {
	  CfgGetParmName(ptr->data, parm_name); parm_name.ToLower();
	  if(parm_name == "http_request_strings") {
	    servercfg->http_request_strings = CfgGetEqualToParm(ptr->data, pbuf);
	  }
	}
	if(ptr->data.IFind("http_hdr_str") != -1) {
	  CfgGetParmName(ptr->data, parm_name); parm_name.ToLower();
	  if(parm_name == "http_hdr_str") {
	    servercfg->http_hdr_str = CfgGetEqualToParm(ptr->data, pbuf);
	  }
	}
	if(ptr->data.IFind("http_forward_for_str") != -1) {
	  CfgGetParmName(ptr->data, parm_name); parm_name.ToLower();
	  if(parm_name == "http_forward_for_str") {
	    servercfg->http_forward_for_str = CfgGetEqualToParm(ptr->data, pbuf);
	  }
	}
	ptr = ptr->next;
      }
    }
    
    if(strdup.IFind("[LBNODE]") != -1) {
      has_lbnode = 1;
      // Start new node
      LBnode *n = new LBnode;
      n->buffer_size = servercfg->buffer_size; // Set global buffer size
      ptr = ptr->next;
      
      while(ptr) {
	if(ptr->data[0] == '[') {
	  // End of new node
	  servercfg->nodes.Add(n);
	  ptr = ptr->prev;
	  break;
	}
	// Add all node config reads here
	if(ptr->data.IFind("node_name") != -1) {
	  n->node_name = CfgGetEqualToParm(ptr->data, pbuf);
	}
	if(ptr->data.IFind("port_number") != -1) {
	  n->port_number = CfgGetEqualToParm(ptr->data);
	  if(n->port_number <= 0) {
	    NT_print("Config file has bad node port value", parm.c_str());
	    error_level = 1;
	  }
	}
	if(ptr->data.IFind("hostname") != -1) {
	  CfgGetParmName(ptr->data, parm_name); parm_name.ToLower();
	  if(parm_name == "hostname") {
	    n->hostname = CfgGetEqualToParm(ptr->data, pbuf);
	  }
	}
	if(ptr->data.IFind("weight") != -1) {
	  CfgGetParmName(ptr->data, parm_name); parm_name.ToLower();
	  if(parm_name == "weight") {
	    gxString wbuf = ptr->data;
	    wbuf.FilterChar('%');
	    n->weight = CfgGetEqualToParm(wbuf);
	  }
	}
	if(ptr->data.IFind("max_connections") != -1) {
	  n->max_connections = CfgGetEqualToParm(ptr->data);
	}
	if(ptr->data.IFind("buffer_size") != -1) {	
	  n->buffer_size = CfgGetEqualToParm(ptr->data);  
	  if(n->buffer_size <= 0) {
	    n->buffer_size = servercfg->buffer_size;
	  }
	}
	ptr = ptr->next;
	if(!ptr) {
	  // We reached the end of file on last node
	  servercfg->nodes.Add(n);
	  break;
	}
      }
    }
    if(!ptr) break;
    ptr = ptr->next;
  }
  
  if(has_lbserver == 0) {
    NT_print("Config file missing [LBSERVER] section", parm.c_str());
    error_level = 1;
  }
  if(has_lbnode == 0) {
    NT_print("Config file missing [LBNODE] sections", parm.c_str());
    error_level = 1;
  }
  
  ifile.df_Close();
  return error_level;
}

int LBconfig::build_distributed_rr_node_list()
{
  distributed_rr_node_list.ClearList();

  gxBStree<LBnode> tree;
  gxList<LBnode> distributed_list;
  gxListNode<LBnode *> *ptr = nodes.GetHead();

  while(ptr) {
    LBnode n = *ptr->data;
    if(!tree.Insert(n)) distributed_list.Add(n);
    ptr = ptr->next;
  }

  LBnode key;
  while(!tree.IsEmpty()) {
    tree.ExtractFirst(key);
    distributed_list.Add(key);
  }
  
  gxListNode<LBnode> *wptr = distributed_list.GetTail();

  int num_nodes = 0;
  while(wptr) {
    distributed_rr_node_list.Add(wptr->data.active_ptr);
    num_nodes++;
    wptr = wptr->prev;
  }
  
  return num_nodes;
}

int LBconfig::get_num_node_connections()
{
  int num_connections = 0;

  gxListNode<LBnode *> *ptr = nodes.GetHead();
  while(ptr) {
    num_connections += ptr->data->NUM_CONNECTIONS();
    ptr = ptr->next;
  }

  return num_connections;
}

int LBconfig::NUM_SERVER_CONNECTIONS(int num)
{
  if(num > 0) num_server_connections = num;
  return num_server_connections;
}

int LBconfig::refactor_distribution_limits(gxList<limit_node> &limit_node_list, int num_connections)
{
  limit_node_list.ClearList();
  if(num_connections == 0) num_connections = servercfg->refactor_scale;  
  int total_refactored_connections = 0;

  gxListNode<LBnode *> *ptr = distributed_rr_node_list.GetHead();
  while(ptr) {
    float node_connection_limit = (float)ptr->data->weight / 100.00;
    float limit = node_connection_limit * num_connections;
    limit_node nl;
    nl.limit = (int)limit;
    if(nl.limit <= 0) nl.limit = 1;
    total_refactored_connections += nl.limit;
    nl.active_ptr = ptr->data;
    limit_node_list.Add(nl);
    ptr = ptr->next;
  }

  return total_refactored_connections;
}

int LBconfig::build_weighted_rr_node_list()
{
  weighted_rr_node_list.ClearList();

  gxBStree<LBnode> tree;
  gxList<LBnode> weighted_list;
  gxListNode<LBnode *> *ptr = nodes.GetHead();

  while(ptr) {
    LBnode n = *ptr->data;
    if(!tree.Insert(n)) weighted_list.Add(n);
    ptr = ptr->next;
  }

  LBnode key;
  while(!tree.IsEmpty()) {
    tree.ExtractFirst(key);
    weighted_list.Add(key);
  }
  
  gxListNode<LBnode> *wptr = weighted_list.GetTail();

  int num_nodes = 0;
  while(wptr) {
    weighted_rr_node_list.Add(wptr->data.active_ptr);
    num_nodes++;
    wptr = wptr->prev;
  }
  
  return num_nodes;
}

int LBconfig::refactor_weighted_limits(gxList<limit_node> &limit_node_list, int scale)
{
  limit_node_list.ClearList();
  if(scale <= 0) scale = 1;
  int total_refactored_connections = 0;

  gxListNode<LBnode *> *ptr = weighted_rr_node_list.GetHead();
  while(ptr) {
    limit_node nl;
    nl.limit = ptr->data->weight * scale;
    nl.active_ptr = ptr->data;
    limit_node_list.Add(nl);
    total_refactored_connections += nl.limit;
    ptr = ptr->next;
  }

  return total_refactored_connections;
}

int LBconfig::REFACTORED_CONNECTIONS(int inc, int dec, unsigned set_to)
{
  int has_mutex = 1;
  if(refactored_connections_lock.MutexLock() != 0) has_mutex = 0;
  if(refactored_connections_is_locked && !has_mutex) return refactored_connections;
  refactored_connections_is_locked = 1; 

  // ********** Enter Critical Section ******************* //
  if(inc > 0) refactored_connections++;
  if(dec > 0) {
    refactored_connections--;
    if(refactored_connections < 0) refactored_connections = 0;
  }
  if(set_to > 0) refactored_connections = set_to;
  int buf = refactored_connections;
  // ********** Leave Critical Section ******************* //

  refactored_connections_is_locked = 0; 
  if(has_mutex) refactored_connections_lock.MutexUnlock();
  return buf;
}

int LBconfig::PREV_REFACTORED_CONNECTIONS(int inc, int dec, unsigned set_to)
{
  int has_mutex = 1;
  if(prev_refactored_connections_lock.MutexLock() != 0) has_mutex = 0;
  if(prev_refactored_connections_is_locked && !has_mutex) return  prev_refactored_connections;
  prev_refactored_connections_is_locked = 1; 

  // ********** Enter Critical Section ******************* //
  if(inc > 0) prev_refactored_connections++;
  if(dec > 0) {
    prev_refactored_connections--;
    if(prev_refactored_connections < 0) prev_refactored_connections = 0;
  }
  if(set_to > 0) prev_refactored_connections = set_to;
  int buf = prev_refactored_connections;
  // ********** Leave Critical Section ******************* //

  prev_refactored_connections_is_locked = 0; 
  if(has_mutex) prev_refactored_connections_lock.MutexUnlock();
  return buf;
}

int LBconfig::WEIGHT_SCALE(int inc, int dec, unsigned set_to)
{
  int has_mutex = 1;
  if(weight_scale_lock.MutexLock() != 0) has_mutex = 0;
  if(weight_scale_is_locked && !has_mutex) return weight_scale;
  weight_scale_is_locked = 1; 
  
  // ********** Enter Critical Section ******************* //
  if(inc > 0) weight_scale++;
  if(dec > 0) {
    weight_scale--;
    if(weight_scale <= 0) weight_scale = 1;
  }
  if(set_to > 0) weight_scale = set_to;
  int buf = weight_scale;
  // ********** Leave Critical Section ******************* //
  
  weight_scale_is_locked = 0; 
  if(has_mutex) weight_scale_lock.MutexUnlock();
  return buf;
}

int LBconfig::check_node_max_clients(LBnode *n, int seq_num)
{
  if(!n) return 0;
  if(n->max_connections <= 1) return 0; // Max clients not set for this node
  
  gxString message;
  int num_connections = n->NUM_CONNECTIONS();
  if(servercfg->debug && servercfg->debug_level == 5) {
    message << clear << "[" << seq_num << "]: Max connection limit for " 
	    << n->node_name << " is set to " << n->max_connections; 
    LogDebugMessage(message.c_str());
  }
  if(num_connections >= n->max_connections) {
    message << clear << "[" << seq_num << "]: " << n->node_name << " reached " << num_connections 
	    << " connections. " << n->hostname << " connection will be denied";
    LogMessage(message.c_str());
    return 1;
  }

  return 0;
}

// Read the rules config
// Return 0 if no errors
// Return 1 if fatal error
// Return 2 if bad rule line
// Return 3 if bad regex
int LBconfig::NT_ReadRulesConfig()
{
  if(rules_config_file.is_null()) {
    NT_print("WARNING - No rules_config_file specified in LB server config");
    return 1;
  }

  int error_level = 0;
  gxString message;

  if(!futils_exists(rules_config_file.c_str())) {
    message << clear << "No rules cfg " << rules_config_file;
    NT_print(message.c_str());
    NT_print("Building default rules configuration file");

    DiskFileB dfile(rules_config_file.c_str(), DiskFileB::df_READWRITE, DiskFileB::df_CREATE); 
    if(!dfile) {
      message << clear << "write error " << rules_config_file;
      NT_print(message.c_str());
      return 1;
    }

    dfile << "#  " << GetProgramDescription() << " " << GetVersionString() << " rule set configuration file" << "\n";
    dfile << "#" << "\n";
    dfile << "\n";
    dfile << "# Default LB rules config file" << "\n";
    dfile << "\n";
    dfile << "# ROUTE rule set for LB_ASSIGNED scheme" << "\n";
    dfile << "# Route rule for assigned LB format: route IP node" << "\n"; 
    dfile << "# IP = IP address or hostname, can be extended regular expression" << "\n";
    dfile << "# node = node name to route traffic too. node name must match server config" << "\n";
    dfile << "# Route rule examples:" << "\n";
    dfile << "##route 192.168.122.1 lbnode1" << "\n";
    dfile << "##route ^192.* lbnode1" << "\n";
    dfile << "\n";
    dfile << "\n";
    dfile.df_Close();
  }
  else {
    char lbuf[1024];
    DiskFileB ifile;
    gxString rule;
    unsigned num_arr;
    gxString *vals = 0; 
    regex_t regex;
    int reti;
    gxString info_line;
    gxString delimiter = " ";

    ifile.df_Open(rules_config_file.c_str());
    if(!ifile) {
      message << clear << "Cannot open " << rules_config_file;
      NT_print(message.c_str());
      return 1;
    }

    // Clear the existing rule set
    gxListNode<LB_rule *> *lptr = rules_config_list.GetHead();
    while(lptr) {
      delete lptr->data;
      lptr->data = 0;
      lptr = lptr->next;
    }
    rules_config_list.ClearList();

    while(!ifile.df_EOF()) {
      ifile.df_GetLine(lbuf, sizeof(lbuf), '\n');
      if(ifile.df_GetError() != DiskFileB::df_NO_ERROR) {
	message << clear << "ERROR - read failed " << rules_config_file.c_str();
	NT_print(message.c_str());
	error_level = 1;
	break;
      }
      
      info_line = lbuf;
      info_line.FilterChar('\n');    
      info_line.FilterChar('\r');    
      info_line.TrimLeadingSpaces();
      info_line.TrimTrailingSpaces();
      
      // Skip remark lines
      if(info_line[0] == '#') continue; 
      if(info_line[0] == ';') continue;
      
      // Filter inline remarks
      info_line.DeleteAfterIncluding("#");
      info_line.TrimTrailingSpaces();
      
      // Replace tabs with spaces
      info_line.ReplaceString("\t", " ");
      
      // Replace multiple spaces with single space
      while(info_line.IFind("  ") != -1) info_line.ReplaceString("  ", " ");
      
      rule = info_line;
      rule.DeleteAfterIncluding(" ");
      if(rule.IFind("route") != -1) {
	num_arr = 0;
	vals = ParseStrings(info_line, delimiter, num_arr);
	if(num_arr != 3) {
	  message << clear << "ERROR - Bad ROUTE rule " << info_line;
	  NT_print(message.c_str());
	  error_level = 2;
	}
	else {
	  // Check for lead quotes and un-escaped trailing quotes
	  vals[1].TrimLeading('"');
	  if((vals[1][vals[1].length()-1] == '"') &&
	     (vals[1][vals[1].length()-2] != '\\')) {
	    vals[1].TrimTrailing('"');
	  }
	  vals[1].TrimLeading('\'');
	  if((vals[1][vals[1].length()-1] == '\'') &&
	     (vals[1][vals[1].length()-2] != '\\')) {
	    vals[1].TrimTrailing('\'');
	  }
	  reti = regcomp(&regex, vals[1].c_str(), REG_EXTENDED|REG_NOSUB);
	  if(reti) {
	    message << clear << "ERROR - Bad regex " << vals[1];
	    NT_print(message.c_str());
	    error_level = 3;
	  }
	  else {
	    LB_rule *p = new LB_rule;
	    p->rule =  LB_ROUTE;
	    p->front_end_client_ip = vals[1];
	    p->node_name = vals[2];
	    rules_config_list.Add(p);
	    regfree(&regex);
	  }
	}
	delete[] vals;
	vals = 0; 
      }
    }
    ifile.df_Close();
  }

  return error_level;
}

int LBconfig::THROTTLE_COUNT(int val)
{
  int has_mutex = 1;
  if(throttle_count_lock.MutexLock() != 0) has_mutex = 0;
  if(throttle_count_is_locked && !has_mutex) return throttle_count;
  throttle_count_is_locked = 1;

  // ********** Enter Critical Section ******************* //
  if(val > -1) {
    throttle_count += val;
    if(throttle_count >= throttle_every_connections) throttle_count = 0;
  }
  int buf = throttle_count;
  // ********** Leave Critical Section ******************* //

  throttle_count_is_locked = 0;
  if(has_mutex) throttle_count_lock.MutexUnlock();
  return buf;
}

int LBconfig::CONNECTIONS_PER_SECOND(int val)
{
  int has_mutex = 1;
  if(connections_per_second_lock.MutexLock() != 0) has_mutex = 0;
  if(connections_per_second_is_locked && !has_mutex) return connections_per_second;
  connections_per_second_is_locked = 1;

  // ********** Enter Critical Section ******************* //
  if(val > -1) connections_per_second = val;
  int buf = connections_per_second;
  // ********** Leave Critical Section ******************* //

  connections_per_second_is_locked = 0;
  if(has_mutex) connections_per_second_lock.MutexUnlock();
  return buf;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
