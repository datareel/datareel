// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 06/17/2016
// Date Last Modified: 07/11/2016 
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
  clear_log = 0;
  config_file = "drlb_server.cfg";
  logfile_name.Clear();
  log_level = 0;
  enable_logging = 0; 
  service_name = "drlb_server"; // Default service name
  ProgramName = "drlb_server";  // Default program name
  user_config_file = 0;
  port = 8085;
  accept_clients = 1;
  echo_loop = 1;
  client_request_pool = new thrPool;
  fatal_error = 0;
  num_nodes = 0;
  num_nodes_is_locked = 0;
  num_nodes_thread_retries = 3;
  num_client_threads_is_locked = 0;
  num_client_threads_retries = 3;
  num_logs_to_keep = 3;
  last_log = 0;
  max_log_size = 5000000;
  enable_buffer_overflow_detection = 0;
  buffer_overflow_size = 6400000;
  server_thread = 0;
  console_thread = 0;
  log_thread  = 0;
  retries = 3;
  retry_wait = 1;
  timeout_secs = 300;
  timeout_usecs = 0;
  use_timeout = 0;
  server_accept_socket = -1;
  somaxconn = 128;
  server_retry = 1;
  rules_config_is_locked = 0;
  rules_config_retries = 3;
  assigned_default = LB_RR;
  queue_node_count = -1;
  queue_debug_count = -1;
  queue_proc_count = -1;
  log_queue_size = 5000;
  log_queue_debug_size = 5000;
  log_queue_proc_size = 5000;
  loq_queue_nodes = new LogQueueNode[log_queue_size];
  loq_queue_debug_nodes = new LogQueueNode[log_queue_debug_size];
  loq_queue_proc_nodes = new LogQueueNode[log_queue_proc_size];
  queue_node_count_is_locked = 0;
  queue_node_count_retries = log_queue_size;
  queue_debug_count_is_locked = 0;
  queue_debug_count_retries = log_queue_debug_size;
  queue_proc_count_is_locked = 0;
  queue_proc_count_retries = log_queue_proc_size;
  refactor_scale = 1;
  refactored_connections = 1;
  prev_refactored_connections = 1;
  weight_scale = 1;
  weight_scale_is_locked = 0;
  weight_scale_retries = 3;
  refactored_connections_is_locked = 0;
  refactored_connections_retries = 3;
  prev_refactored_connections_is_locked = 0;
  prev_refactored_connections_retries = 3;
}

LBconfig::~LBconfig()
{
  if(client_request_pool) {
    delete client_request_pool;
    client_request_pool = 0;
  }

  if(server_thread) {
    delete server_thread;
    server_thread = 0;
  }
  if(console_thread) {
    delete console_thread;
    console_thread = 0;
  }
  if(log_thread) {
    delete log_thread;
    log_thread = 0;
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
}

unsigned LBconfig::NUM_NODES()
{
  if(num_nodes > 0) return num_nodes;
  num_nodes_lock.MutexLock();
  int num_try = 0;
  while(num_nodes_is_locked != 0) {
    if(++num_try < num_nodes_thread_retries) {
      num_nodes_cond.ConditionWait(&num_nodes_lock);
    }
    else {
      return num_nodes; // Could not update
    }
  }

  num_nodes_is_locked = 1; 

  // ********** Enter Critical Section ******************* //
  gxListNode<LBnode *> *ptr = nodes.GetHead();
  while(ptr) {
    num_nodes++;
    ptr = ptr->next;
  }
  // ********** Leave Critical Section ******************* //

  num_nodes_is_locked = 0; 
  num_nodes_cond.ConditionSignal();
  num_nodes_lock.MutexUnlock();

  return num_nodes;
}

unsigned LBconfig::NUM_CLIENT_THREADS(int inc, int dec, unsigned set_to)
{
  num_client_threads_lock.MutexLock();
  int num_try = 0;
  while(num_client_threads_is_locked != 0) {
    if(++num_try < num_client_threads_retries) {
      num_client_threads_cond.ConditionWait(&num_client_threads_lock);
    }
    else {
      return num_client_threads; // Could not update
    }
  }

  num_client_threads_is_locked = 1; 

  // ********** Enter Critical Section ******************* //
  if(inc > 0) num_client_threads++;
  if(dec > 0) {
    num_client_threads--;
    if(num_client_threads < 0) num_client_threads = 0;
  }
  if(set_to > 0) num_client_threads = set_to;
  // ********** Leave Critical Section ******************* //

  num_client_threads_is_locked = 0; 
  num_client_threads_cond.ConditionSignal();
  num_client_threads_lock.MutexUnlock();

  return num_client_threads;
}

int LBconfig::QUEUE_NODE_COUNT()
{
  if(queue_node_count_lock.MutexTryLock() != 0) {
    return -1; 
  }
  int num_try = 0;
  while(queue_node_count_is_locked != 0) {
    if(++num_try < queue_node_count_retries) {
      queue_node_count_cond.ConditionTimedWait(&queue_node_count_lock, 1, 0);
    }
    else {
      return -1;
    }
  }
  queue_node_count_is_locked = 1; 

  // ********** Enter Critical Section ******************* //
  if(queue_node_count == log_queue_size) queue_node_count = 0; 
  queue_node_count++;
  // ********** Leave Critical Section ******************* //

  queue_node_count_is_locked = 0; 
  queue_node_count_cond.ConditionSignal();

  if(queue_node_count_lock.IsLocked()) {
    queue_node_count_lock.MutexUnlock();
  }

  return queue_node_count;
}

int LBconfig::QUEUE_DEBUG_COUNT()
{
  if(queue_debug_count_lock.MutexTryLock() != 0) {
    return -1; 
  }
  int num_try = 0;
  while(queue_debug_count_is_locked != 0) {
    if(++num_try < queue_debug_count_retries) {
      queue_debug_count_cond.ConditionTimedWait(&queue_debug_count_lock, 1, 0);
    }
    else {
      return -1;
    }
  }
  queue_debug_count_is_locked = 1; 

  // ********** Enter Critical Section ******************* //
  if(queue_debug_count == log_queue_size) queue_debug_count = 0; 
  queue_debug_count++;
  // ********** Leave Critical Section ******************* //

  queue_debug_count_is_locked = 0; 
  queue_debug_count_cond.ConditionSignal();

  if(queue_debug_count_lock.IsLocked()) {
    queue_debug_count_lock.MutexUnlock();
  }

  return queue_debug_count;
}

int LBconfig::QUEUE_PROC_COUNT()
{
  if(queue_proc_count_lock.MutexTryLock() != 0) {
    return -1; 
  }
  int num_try = 0;
  while(queue_proc_count_is_locked != 0) {
    if(++num_try < queue_proc_count_retries) {
      queue_proc_count_cond.ConditionTimedWait(&queue_proc_count_lock, 1, 0);
    }
    else {
      return -1;
    }
  }
  queue_proc_count_is_locked = 1; 

  // ********** Enter Critical Section ******************* //
  if(queue_proc_count == log_queue_size) queue_proc_count = 0; 
  queue_proc_count++;
  // ********** Leave Critical Section ******************* //

  queue_proc_count_is_locked = 0; 
  queue_proc_count_cond.ConditionSignal();

  if(queue_proc_count_lock.IsLocked()) {
    queue_proc_count_lock.MutexUnlock();
  }

  return queue_proc_count;
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
  num_connections_retries = 3;
  lb_flag_is_locked = 0;
  lb_flag_retries = 3;
  active_ptr = this;
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
  num_connections_retries = 3;
  lb_flag_is_locked = 0;
  lb_flag_retries = 3;
  active_ptr = n.active_ptr;
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
  lb_flag_lock.MutexLock();

  int num_try = 0;
  while(lb_flag_is_locked != 0) {
    if(++num_try < lb_flag_retries) {
      lb_flag_cond.ConditionWait(&lb_flag_lock);
    }
    else {
      return lb_flag; // Could not update
    }
  }

  lb_flag_is_locked = 1; 

  // ********** Enter Critical Section ******************* //
  if(flag != -1) lb_flag = flag;
  // ********** Leave Critical Section ******************* //

  lb_flag_is_locked = 0; 
  lb_flag_cond.ConditionSignal();
  lb_flag_lock.MutexUnlock();

  return lb_flag;
}

unsigned LBnode::NUM_CONNECTIONS(int inc, int dec, unsigned set_to)
{
  num_connections_lock.MutexLock();

  int num_try = 0;
  while(num_connections_is_locked != 0) {
    if(++num_try < num_connections_retries) {
      num_connections_cond.ConditionWait(&num_connections_lock);
    }
    else {
      return num_connections; // Could not update
    }
  }

  num_connections_is_locked = 1; 

  // ********** Enter Critical Section ******************* //
  if(num_connections < 0) num_connections = 0;
  if(inc > 0) num_connections ++;
  if(dec > 0) {
    num_connections--;
    if(num_connections < 0) num_connections = 0;
  }
  if(set_to > 0) num_connections = set_to;
  // ********** Leave Critical Section ******************* //

  num_connections_is_locked = 0; 
  num_connections_cond.ConditionSignal();
  num_connections_lock.MutexUnlock();

  return num_connections;
} 

int ProcessDashDashArg(gxString &arg)
{
  gxString sbuf, equal_arg;
  int has_valid_args = 0;
  
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
    has_valid_args = 1;
    HelpMessage(servercfg->ProgramName.c_str());
    return 0; // Signal program to exit
  }
  if(arg == "?") {
    has_valid_args = 1;
    HelpMessage(servercfg->ProgramName.c_str());
    return 0; // Signal program to exit
  }
  if((arg == "version") || (arg == "ver")) {
    has_valid_args = 1;
    VersionMessage();
    return 0; // Signal program to exit
  }

  if(arg == "verbose") {
    has_valid_args = 1;
    servercfg->verbose = 1;
  }

  if(arg == "verbose-level") {
    if(equal_arg.is_null()) { 
      servercfg->verbose = 1;
      NT_print("Error no verbose level supplied with the --verbose-level swtich");
      return 0;
    }
    has_valid_args = 1;
    servercfg->verbose = 1;
    if(!equal_arg.is_null()) { 
      servercfg->verbose_level = equal_arg.Atoi(); 
      if(servercfg->verbose_level <= 0) servercfg->verbose_level = 1;
    }
  }

  if(arg == "debug") {
    has_valid_args = 1;
    servercfg->debug = 1;
    if(!equal_arg.is_null()) { 
      servercfg->debug_level = equal_arg.Atoi(); 
      if(servercfg->debug_level <= 0) servercfg->debug_level = 1;
    }
  }

  if(arg == "debug-level") {
    if(equal_arg.is_null()) { 
      servercfg->verbose = 1;
      NT_print("Error no debug level supplied with the --debug-level swtich");
      return 0;
    }
    has_valid_args = 1;
    servercfg->debug = 1;
    if(!equal_arg.is_null()) { 
      servercfg->debug_level = equal_arg.Atoi(); 
      if(servercfg->debug_level <= 0) servercfg->debug_level = 1;
    }
  }

  if(arg == "config-file") {
    if(equal_arg.is_null()) { 
      servercfg->verbose = 1;
      NT_print("Error no config file name supplied with the --config-file swtich"); 
      return 0;
    }
    has_valid_args = 1;
    servercfg->config_file = equal_arg;
    servercfg->user_config_file = 1;
  }

  if(arg == "log-file") {
    if(equal_arg.is_null()) { 
      servercfg->verbose = 1;
      NT_print("Error no LOG file name supplied with the --log-file swtich");
      return 0;
    }
    has_valid_args = 1;
    servercfg->logfile_name = equal_arg;
    servercfg->enable_logging = 1;
  }

  if(arg == "log-file-clear") {
    has_valid_args = 1;
    servercfg->clear_log = 1;
  }

  if(arg == "log-level") {
    if(equal_arg.is_null()) { 
      servercfg->verbose = 1;
      NT_print("Error no debug level supplied with the --log-level swtich");
      return 0;
    }
    has_valid_args = 1;
    if(!equal_arg.is_null()) { 
      servercfg->log_level = equal_arg.Atoi(); 
      if(servercfg->log_level < 0) servercfg->log_level = 0;
    }
  }

  arg.Clear();
  return has_valid_args;
}

int ProcessArgs(int argc, char *argv[])
{
  // process the program's argument list
  int i;
  gxString sbuf;

  for(i = 1; i < argc; i++ ) {
    if(*argv[i] == '-') {
      char sw = *(argv[i] +1);
      switch(sw) {

	case '-':
	  sbuf = &argv[i][2]; 
	  // Add all -- prepend filters here
	  sbuf.TrimLeading('-');
	  if(!ProcessDashDashArg(sbuf)) return 0;
	  break;

	case '?':
	  HelpMessage(servercfg->ProgramName.c_str());
	  return 0; // Signal program to exit
	  
	case 'v': case 'V': 
	  servercfg->verbose = 1;
	  break;

	case 'd': case 'D':
	  servercfg->debug = 1;
	  break;

	default:
	  servercfg->verbose = 1; 
	  NT_print("\n");
	  sbuf << clear << "Unknown command line arg " << argv[i];
	  NT_print(sbuf.c_str());
	  NT_print("Exiting with errors");
	  NT_print("\n");
	  return 0;
      }
    }
  }

  return 1; // All command line arguments were valid
}

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
  memset(sbuf, 0, sizeof(sbuf));
  gxString parm;
  CfgGetEqualToParm(cfgline, parm);
  strcpy(sbuf, parm.c_str());
  return sbuf;
}

int LoadOrBuildConfigFile()
{
  NT_print("Loading config file", servercfg->config_file.c_str());
  
  if(!futils_exists(servercfg->config_file.c_str())) {
    if(servercfg->user_config_file) {
      NT_print("User specified config file not found");
    }
    else {
      NT_print("Config file not found");
    }
    NT_print("Building default configuration file");

    DiskFileB dfile(servercfg->config_file.c_str(), DiskFileB::df_READWRITE, DiskFileB::df_CREATE); 
    if(!dfile) {
      NT_print("Cannot write to config file", servercfg->config_file.c_str());
      return 0;
    }
    dfile << "#  " << GetProgramDescription() << " " << GetVersionString() << " configuration file" << "\n";
    dfile << "#" << "\n";
    dfile << "\n";
    dfile << "# Default LB config file" << "\n";
    dfile << "\n";
    dfile << "# Global config" << "\n";
    dfile << "[LBSERVER]" << "\n";
    dfile << "port = 8080 # All incoming traffic connections here" << "\n";
    dfile << "#" << "\n";
    dfile << "# LB_ASSIGNED scheme requires a rules config file" << "\n";
    dfile << "# A default rules config file will be build if specified file does not exist" << "\n";
    dfile << "##scheme = LB_ASSIGNED" << "\n";
    dfile << "##rules_config_file = drlb_server_rules.cfg" << "\n";
    dfile << "##dynamic_rules_read = 1 # Read the rules config while LB server is running" << "\n";
    dfile << "##assigned_default = LB_RR; # Use round robin, distib or weighted, for node fail over" << "\n";
    dfile << "# Set assigned_default to LB_NONE to disable node fail over, meaning:" << "\n";
    dfile << "# If the assigned node is not available the client connection will fail" << "\n";
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
    dfile << "num_logs_to_keep = 3" << "\n";
    dfile << "last_log = 1" << "\n";
    dfile << "max_log_size = 5000000" << "\n";
    dfile << "log_queue_size = 5000 # Max number of log or console messages to queue" << "\n";
    dfile << "log_queue_debug_size = 5000 # Max number of debug messages to queue" << "\n";
    dfile << "log_queue_proc_size = 5000 # Max number of process messages to queue" << "\n";
    dfile << "# Values below can be set here or args when program is launched" << "\n";
    dfile << "##clear_log = 0" << "\n";
    dfile << "##enable_logging = 0" << "\n";
    dfile << "# Log levels 0-5, 0 lowest log level, 5 highest log level " << "\n";
    dfile << "##log_level = 1" << "\n";
    dfile << "##logfile_name = drlb_server.log" << "\n";
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
    dfile << "# Values below can be set here or args when program is launched" << "\n";
    dfile << "##debug = 0" << "\n";
    dfile << "##debug_level = 1" << "\n";
    dfile << "##verbose = 0" << "\n";
    dfile << "##verbose_level = 1" << "\n";
    dfile << "\n";
    dfile << "# Node Configs, define a [LBNODE] section for each node" << "\n";
    dfile << "# The default config uses 4 nodes as an example" << "\n";
    dfile << "# Replace 192.168.122 address with the IP address or hostname of the backend node" << "\n";
    dfile << "# running the service you wan to load balance." << "\n";
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
    NT_print("Cannot open config file for reading", servercfg->config_file.c_str());
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
	// Add all server conifg reads here
	if(ptr->data.IFind("scheme") != -1) {
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
	if(ptr->data.IFind("debug") != -1) {
	  servercfg->debug = CfgGetTrueFalseValue(ptr->data);
	}
	if(ptr->data.IFind("debug_level") != -1) {
	  servercfg->debug_level = CfgGetEqualToParm(ptr->data);
	}
	if(ptr->data.IFind("verbose") != -1) {
	  servercfg->verbose = CfgGetTrueFalseValue(ptr->data);
	}
	if(ptr->data.IFind("verbose_level") != -1) {
	  servercfg->verbose_level = CfgGetEqualToParm(ptr->data);
	}
	if(ptr->data.IFind("max_threads") != -1) {
	  servercfg->max_threads = CfgGetEqualToParm(ptr->data);
	}
	if(ptr->data.IFind("max_connections") != -1) {
	  servercfg->max_connections = CfgGetEqualToParm(ptr->data);
	}
	if(ptr->data.IFind("clear_log") != -1) {
	  servercfg->clear_log = CfgGetTrueFalseValue(ptr->data);
	}
	if(ptr->data.IFind("logfile_name") != -1) {
	  servercfg->logfile_name = CfgGetEqualToParm(ptr->data, pbuf);
	}
	if(ptr->data.IFind("enable_logging") != -1) {
	  servercfg->enable_logging = CfgGetTrueFalseValue(ptr->data);
	}
	if(ptr->data.IFind("log_level") != -1) {
	  servercfg->log_level = CfgGetEqualToParm(ptr->data);
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
	  servercfg->max_log_size = CfgGetEqualToParm(ptr->data);
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
	  servercfg->retries = CfgGetEqualToParm(ptr->data);
	  if(servercfg->retries < 0) {
	    NT_print("Config file has bad retries value");
	    error_level = 1;
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
	if(ptr->data.IFind("use_timeout") != -1) {
	  servercfg->use_timeout = CfgGetTrueFalseValue(ptr->data);
	}
	if(ptr->data.IFind("port") != -1) {
	  servercfg->port = CfgGetEqualToParm(ptr->data);
	  if(servercfg->port <= 0) {
	    NT_print("Config file has bad server port value");
	    error_level = 1;
	  }
	}
	if(ptr->data.IFind("somaxconn") != -1) {
	  servercfg->somaxconn = CfgGetEqualToParm(ptr->data);
	  if(servercfg->somaxconn <= 0) {
	    NT_print("Config file has bad somaxconn value");
	    error_level = 1;
	  }
	}
	if(ptr->data.IFind("dynamic_rules_read") != -1) {
	  servercfg->dynamic_rules_read = CfgGetTrueFalseValue(ptr->data);
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
	  n->hostname = CfgGetEqualToParm(ptr->data, pbuf);
	}
	if(ptr->data.IFind("weight") != -1) {
	  gxString wbuf = ptr->data;
	  wbuf.FilterChar('%');
	  n->weight = CfgGetEqualToParm(wbuf);
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

// Read the rules config
// Return 0 if no errors
// Return 1 if fatal error
// Return 2 if bad rule line
// Return 3 if bad regex
int LBconfig::ReadRulesConfig()
{
  if(rules_config_file.is_null()) {
    if(log_level >= 4 || debug) {
	LogDebugMessage("WARNING - No rules_config_file specified in LB server config");
    }
    return 1;
  }

  rules_config_lock.MutexLock();
  int num_try = 0;
  while(rules_config_is_locked != 0) {
    if(++num_try < rules_config_retries) {
      rules_config_cond.ConditionWait(&rules_config_lock);
    }
    else {
      return 1; // Could not update
    }
  }
  rules_config_is_locked = 1; 

  // ********** Enter Critical Section ******************* //
  int error_level = 0;
  gxString message;

  if(!futils_exists(rules_config_file.c_str())) {
    if(log_level >= 4 || debug) {
      message << clear << "No rules cfg " << rules_config_file;
      LogDebugMessage(message.c_str());
      LogDebugMessage("Building default rules configuration file");
    }

    DiskFileB dfile(rules_config_file.c_str(), DiskFileB::df_READWRITE, DiskFileB::df_CREATE); 
    if(!dfile) {
      if(log_level >= 4 || debug) {
	message << clear << "write error " << rules_config_file;
	LogDebugMessage(message.c_str());
      }
      // ********** Leave Critical Section ******************* //
      rules_config_is_locked = 0; 
      rules_config_cond.ConditionSignal();
      rules_config_lock.MutexUnlock();
      return 1;
    }

    dfile << "#  " << GetProgramDescription() << " " << GetVersionString() << " rule set configuration file" << "\n";
    dfile << "#" << "\n";
    dfile << "\n";
    dfile << "# Default LB rules config file" << "\n";
    dfile << "\n";
    dfile << "# ROUTE rule set for LB_ASSIGNED scheme" << "\n";
    dfile << "# Route rule for assigned LB format: route IP node" << "\n"; 
    dfile << "# IP = Frontend IP address in dotted notation or regular expression form" << "\n";
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
      if(log_level >= 4 || debug) {
	message << clear << "Cannot open" << rules_config_file;
	LogDebugMessage(message.c_str());
      }
      // ********** Leave Critical Section ******************* //
      rules_config_is_locked = 0; 
      rules_config_cond.ConditionSignal();
      rules_config_lock.MutexUnlock();
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
	if(log_level >= 4 || debug) {
	  message << clear << "ERROR - read failed " << rules_config_file.c_str();
	  LogDebugMessage(message.c_str());
	}
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
	  if(log_level >= 4 || debug) {
	    message << clear << "ERROR - Bad ROUTE rule " << info_line;
	    LogDebugMessage(message.c_str());
	  }
	  error_level = 2;
	}
	else {
	  reti = regcomp(&regex, vals[1].c_str(), REG_EXTENDED|REG_NOSUB);
	  if(reti) {
	    if(log_level >= 4 || debug) {
	      message << clear << "ERROR - Bad regex " << vals[1];
	      LogDebugMessage(message.c_str());
	    }
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
  // ********** Leave Critical Section ******************* //

  rules_config_is_locked = 0; 
  rules_config_cond.ConditionSignal();
  rules_config_lock.MutexUnlock();

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

  int num_nodes;
  while(wptr) {
    distributed_rr_node_list.Add(wptr->data.active_ptr);
    num_nodes++;
    wptr = wptr->prev;
  }
  
  return num_nodes;
}

int LBconfig::get_num_server_connections()
{
  int num_connections = 0;

  gxListNode<LBnode *> *ptr = nodes.GetHead();
  while(ptr) {
    num_connections += ptr->data->NUM_CONNECTIONS();
    ptr = ptr->next;
  }

  return num_connections;
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

  int num_nodes;
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
  refactored_connections_lock.MutexLock();
  int num_try = 0;
  while(refactored_connections_is_locked != 0) {
    if(++num_try < refactored_connections_retries) {
      refactored_connections_cond.ConditionWait(&refactored_connections_lock);
    }
    else {
      return refactored_connections; // Could not update
    }
  }

  refactored_connections_is_locked = 1; 

  // ********** Enter Critical Section ******************* //
  if(inc > 0) refactored_connections++;
  if(dec > 0) {
    refactored_connections--;
    if(refactored_connections < 0) refactored_connections = 0;
  }
  if(set_to > 0) refactored_connections = set_to;
  // ********** Leave Critical Section ******************* //

  refactored_connections_is_locked = 0; 
  refactored_connections_cond.ConditionSignal();
  refactored_connections_lock.MutexUnlock();

  return refactored_connections;
}

int LBconfig::PREV_REFACTORED_CONNECTIONS(int inc, int dec, unsigned set_to)
{
  prev_refactored_connections_lock.MutexLock();
  int num_try = 0;
  while(prev_refactored_connections_is_locked != 0) {
    if(++num_try < prev_refactored_connections_retries) {
      prev_refactored_connections_cond.ConditionWait(&prev_refactored_connections_lock);
    }
    else {
      return prev_refactored_connections; // Could not update
    }
  }

  prev_refactored_connections_is_locked = 1; 

  // ********** Enter Critical Section ******************* //
  if(inc > 0) prev_refactored_connections++;
  if(dec > 0) {
    prev_refactored_connections--;
    if(prev_refactored_connections < 0) prev_refactored_connections = 0;
  }
  if(set_to > 0) prev_refactored_connections = set_to;
  // ********** Leave Critical Section ******************* //

  prev_refactored_connections_is_locked = 0; 
  prev_refactored_connections_cond.ConditionSignal();
  prev_refactored_connections_lock.MutexUnlock();

  return prev_refactored_connections;
}

int LBconfig::WEIGHT_SCALE(int inc, int dec, unsigned set_to)
{
  weight_scale_lock.MutexLock();
  int num_try = 0;
  while(weight_scale_is_locked != 0) {
    if(++num_try < weight_scale_retries) {
      weight_scale_cond.ConditionWait(&weight_scale_lock);
    }
    else {
      return weight_scale; // Could not update
    }
  }

  weight_scale_is_locked = 1; 

  // ********** Enter Critical Section ******************* //
  if(inc > 0) weight_scale++;
  if(dec > 0) {
    weight_scale--;
    if(weight_scale <= 0) weight_scale = 1;
  }
  if(set_to > 0) weight_scale = set_to;
  // ********** Leave Critical Section ******************* //

  weight_scale_is_locked = 0; 
  weight_scale_cond.ConditionSignal();
  weight_scale_lock.MutexUnlock();

  return weight_scale;
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
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
