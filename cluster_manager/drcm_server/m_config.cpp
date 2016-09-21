// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 07/17/2016
// Date Last Modified: 09/21/2016
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

Program config functions for Datareel cluster manager.

*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <regex.h>

#include "drcm_server.h"
#include "m_config.h"
#include "gxbstree.h"

CMconfig ServerConfigSruct;
CMconfig *servercfg = &ServerConfigSruct;

CMnode::CMnode() 
{
  crontabs_check = 60;
  services_check = 60;
  applications_check = 60;
  ipaddrs_check = 60;
  filesystems_check = 60;
  cluster_take_over_flag = 0; 
  node_is_active = 1;
  cluster_take_over_flag = 0; 
  active_ptr = this;
  cluster_take_over_flag_is_locked = 0;
  keep_services = 0;
  keep_filesystems = 1;
  keep_applications = 0;
}

int CMnode::CLUSTER_TAKE_OVER_FLAG(int set, int unset) 
{
  int has_mutex = 1;
  if(cluster_take_over_flag_lock.MutexLock() != 0) has_mutex = 0;
  if(cluster_take_over_flag_is_locked && !has_mutex) return cluster_take_over_flag;
  cluster_take_over_flag_is_locked = 1; 
  
  // ********** Enter Critical Section ******************* //
  if(set) cluster_take_over_flag = 1;
  if(unset) cluster_take_over_flag = 0;
  int buf = cluster_take_over_flag;
  // ********** Leave Critical Section ******************* //
  
  cluster_take_over_flag_is_locked = 0; 
  if(has_mutex) cluster_take_over_flag_lock.MutexUnlock();
  
  return buf;
}

void CMnode::Copy(const CMnode &n) {
  if(&n == this) return;
  nodename = n.nodename;
  hostname = n.hostname;
  keep_alive_ip = n.keep_alive_ip;
  crontabs_check = n.crontabs_check;
  services_check = n.services_check;
  applications_check = n.applications_check;
  ipaddrs_check = n.ipaddrs_check;
  filesystems_check = n.filesystems_check;
  node_is_active = n.node_is_active;
  cluster_take_over_flag = n.cluster_take_over_flag; 
  copy_list(n.crontabs, crontabs); 
  copy_list(n.backup_crontabs, backup_crontabs); 
  copy_list(n.services, services); 
  copy_list(n.backup_services, backup_services); 
  copy_list(n.applications, applications); 
  copy_list(n.backup_applications, backup_applications); 
  copy_list(n.floating_ip_addrs, floating_ip_addrs); 
  copy_list(n.backup_floating_ip_addrs, backup_floating_ip_addrs);  
  copy_list(n.filesystems, filesystems); 
  copy_list(n.backup_filesystems, backup_filesystems); 
  active_ptr = n.active_ptr;
  cluster_take_over_flag_is_locked = 0;
  keep_services = n.keep_services;
  keep_filesystems = n.keep_filesystems;
  keep_applications = n.keep_applications;
}

void CMnode::copy_list(const gxList<gxString> &source, gxList<gxString> &dest) 
{
  dest.ClearList();
  gxListNode<gxString> *ptr = source.GetHead();
  while(ptr) {
    dest.Add(ptr->data);
    ptr = ptr->next;
  }
}

void CMstatsnode::Copy(const CMstatsnode &n) 
{
  if(&n == this) return;
  node.Copy(n.node);
  is_alive = n.is_alive;
  stats.ClearList();
  gxListNode<gxString> *ptr = n.stats.GetHead();
  while(ptr) {
    stats.Add(ptr->data);
    ptr = ptr->next;
  }
}

CMconfig::CMconfig() 
{ 
  is_client = 0;
  client_ping_count = 1;
  is_client_interactive = 1;
  debug = 0;
  debug_level = 1;
  verbose = 0;
  verbose_level = 1;

  cluster_user = "root";
  cluster_group = "root";
  sudo_command = "sudo su root -c";
  run_dir = "/var/run/drcm";
  log_dir = "/var/log/drcm";
  spool_dir = "/var/spool/drcm";
  etc_dir = "/etc/drcm";
  dead_node_count = 60;
  bind_to_keep_alive_ip = 1;

  check_config = 0;
  config_file << clear << etc_dir << "/cm.cfg";
  logfile_name << clear << log_dir << "/drcm.log";
  log_file_err = 0;

  log_level = 1;
  enable_logging = 1; 
  service_name = "drcm_server"; // Default service name
  client_name = "drcm_client";  // Default client serivce name
  ProgramName = "drcm_server";  // Default program name
  user_config_file = 0;
  udpport = tcpport = DEFAULT_PORT;
  accept_clients = 1;
  echo_loop = 1;
  kill_server = 0;
  restart_server = 0;
  process_loop = 1;
  fatal_error = 0;
  num_logs_to_keep = 3;
  last_log = 0;
  max_log_size = 5000000;
  clear_log = 0;
  server_retry = 1;
  client_request_pool = new thrPool;
  process_thread = 0;
  console_thread = 0;
  log_thread = 0;
  udp_server_thread = 0;
  keep_alive_thread = 0;
  tcp_server_thread = 0;
  crontab_thread = 0;
  services_thread = 0;
  applications_thread = 0;
  ipaddr_thread = 0;
  filesystems_thread = 0;
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

  auth_key_file = "/etc/drcm/.auth/authkey";
  sha1_file = "/etc/drcm/.auth/authkey.sha1";

  memset(sha1sum, 0, 40);
  memset(auth_key, 0, 2048);

  has_debug_override = 0;
  has_debug_level_override = 0;
  has_verbose_override = 0;
  has_verbose_level_override = 0;
  has_config_file_override = 0;
  has_log_file_name_override = 0;
  has_log_level_override = 0;
  has_log_file_clear_override = 0;
  has_enable_logging_override = 0;
  num_resource_crons = 0;
  num_resource_ipaddrs = 0;
  num_resource_services = 0;
  num_resource_applications = 0;
  num_resource_filesystems = 0;
}

CMconfig::~CMconfig() 
{ 

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
  if(process_thread) {
    delete process_thread;
    process_thread = 0;
  }
  if(log_thread) {
    delete log_thread;
    log_thread = 0;
  }
  if(udp_server_thread) {
    delete udp_server_thread;
    udp_server_thread = 0;
  }
  if(keep_alive_thread) {
    delete keep_alive_thread;
    keep_alive_thread = 0;
  }
  if(tcp_server_thread) {
    delete tcp_server_thread;
    tcp_server_thread = 0;
  }
  if(crontab_thread) {
    delete crontab_thread;
    crontab_thread = 0;
  }
  if(services_thread) {
    delete services_thread;
    services_thread = 0;
  }
  if(applications_thread) {
    delete applications_thread;
    applications_thread = 0;
  }
  if(ipaddr_thread) {
    delete ipaddr_thread;
    ipaddr_thread = 0;
  }
  if(filesystems_thread) {
    delete filesystems_thread;
    filesystems_thread = 0;
  }
  if(client_request_pool) {
    delete client_request_pool;
    client_request_pool = 0;
  }
  if(console_thread) {
    delete console_thread;
    console_thread = 0;
  }

  gxListNode<CMnode *> *ptr = nodes.GetHead();
  while(ptr) {
    delete ptr->data;
    ptr->data = 0;
    ptr = ptr->next;
  }
  nodes.ClearList();
}

int CMconfig::QUEUE_NODE_COUNT()
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

int CMconfig::QUEUE_DEBUG_COUNT()
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

int CMconfig::QUEUE_PROC_COUNT()
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

int ProcessDashDashArg(gxString &arg)
{
  gxString sbuf, equal_arg;
  int error_level = 0;
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

  if(arg.is_null()) return 1;
  
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
  if(arg == "client") {
    servercfg->is_client = 1;
    has_arg = 1;
  }

  if(arg == "command") {
    if(equal_arg.is_null()) { 
      servercfg->verbose = 1;
      NT_print("Error no config file name supplied with the client --command swtich"); 
      error_level = 1;
    }
    else {
      servercfg->client_command = equal_arg;
      servercfg->is_client_interactive = 0;
      has_arg = 1;
    }
  }

  if(arg == "verbose-level") {
    if(equal_arg.is_null()) { 
      servercfg->verbose = 1;
      NT_print("Error no verbose level supplied with the --verbose-level swtich");
      error_level = 1;
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
      error_level = 1;
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
    has_arg = 1;
  }

  if(arg == "config-file") {
    if(equal_arg.is_null()) { 
      servercfg->verbose = 1;
      NT_print("Error no config file name supplied with the --config-file swtich"); 
      error_level = 1;
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
      NT_print("Error no log file name supplied with the --log-file swtich");
      error_level = 1;
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
    servercfg->has_enable_logging_override = 1;
    servercfg->enable_logging = 0;
    has_arg = 1;
  }

  if(arg == "log-level") {
    if(equal_arg.is_null()) { 
      NT_print("Error no log level supplied with the --log-level swtich");
      error_level = 1;
    }
    else {
      servercfg->has_log_level_override = 1;
      servercfg->log_level = equal_arg.Atoi(); 
      if(servercfg->log_level < 0) servercfg->log_level = 0;
      has_arg = 1;
    }
  }

  arg.Clear();
  if(has_arg == 0) error_level = 1;

  return error_level;
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
	  if(sbuf.is_null()) break; // -- escape
	  if(ProcessDashDashArg(sbuf) != 0) has_arg_errors++;
	  break;

	case 'c':
	  sbuf = &argv[i][2]; 
	  if(sbuf.is_null()) {
	    servercfg->verbose = 1; 
	    sbuf << clear << "No count value supplied with arg " << argv[i];
	    NT_print(sbuf.c_str());
	    NT_print("Exiting with errors");
	    ExitProc(1); // Signal program to exit
	  }
	  servercfg->client_ping_count = sbuf.Atoi();
	  if(servercfg->client_ping_count <= 0) servercfg->client_ping_count = 1;
	  break;

	case 'n':
	  sbuf = &argv[i][2]; 
	  if(sbuf.is_null()) {
	    servercfg->verbose = 1; 
	    sbuf << clear << "No name or IP value supplied with arg " << argv[i];
	    NT_print(sbuf.c_str());
	    NT_print("Exiting with errors");
	    ExitProc(1); // Signal program to exit
	  }
	  servercfg->client_ping_nodename = sbuf;
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
    dfile << "# " << GetProgramDescription() << " " << GetVersionString() << " configuration file" << "\n";
    dfile << "#" << "\n";
    dfile << "\n";
    dfile << "# Default CM config file" << "\n";
    dfile << "\n";
    dfile << "# Comment lines and in line comments are designated by a # symbol" << "\n";
    dfile << "# NOTE: If a # is part of a cluster command use \\# to escape the comment parser." << "\n";
    dfile << "" << "\n";
    dfile << "# Global cluser configuration" << "\n";
    dfile << "[CM_SERVER]" << "\n";
    dfile << "# Cluster setup" << "\n";
    dfile << "udpport = 53897" << "\n";
    dfile << "tcpport = 53897" << "\n";
    dfile << "dead_node_count = 60" << "\n";
    dfile << "bind_to_keep_alive_ip = 1" << "\n";
    dfile << "" << "\n";
    dfile << "# Process DIR locations" << "\n";
    dfile << "run_dir = /var/run/drcm" << "\n";
    dfile << "log_dir = /var/log/drcm" << "\n";
    dfile << "spool_dir = /var/spool/drcm" << "\n";
    dfile << "" << "\n";
    dfile << "# CM username setup" << "\n";
    dfile << "cluster_user = root" << "\n";
    dfile << "cluster_group = root" << "\n";
    dfile << "# sudo command for non-root CM user" << "\n";
    dfile << "sudo_command = sudo su root -c" << "\n";
    dfile << "" << "\n";
    dfile << "# Log setup" << "\n";
    dfile << "# Service name that appears in log file" << "\n";
    dfile << "##service_name = drcm_server" << "\n";
    dfile << "# Log file should not contain a path. Logs will be stored in log_dir location" << "\n";
    dfile << "# Path entries in the logfile config line will be ignored" << "\n";
    dfile << "logfile = drcm.log" << "\n";
    dfile << "# Keep log file plus last 3. Disk space will be: (max_log_size * (num_logs_to_keep +1))" << "\n";
    dfile << "num_logs_to_keep = 3" << "\n";
    dfile << "# Set max size per log file, max is 2000000000" << "\n";
    dfile << "max_log_size = 5000000 # Default is 5M, max is 2G" << "\n";
    dfile << "log_queue_size = 2048 # Max number of log or console messages to queue" << "\n";
    dfile << "log_queue_debug_size = 4096 # Max number of debug messages to queue" << "\n";
    dfile << "log_queue_proc_size = 255 # Max number of process messages to queue" << "\n";
    dfile << "# Values below can be set here or args when program is launched" << "\n";
    dfile << "##clear_log = 0" << "\n";
    dfile << "enable_logging = 1" << "\n";
    dfile << "# Log levels 0-5, 0 lowest log level, 5 highest log level " << "\n";
    dfile << "log_level = 1" << "\n";
    dfile << "" << "\n";
    dfile << "# Values below can be set here or args when program is launched" << "\n";
    dfile << "# Debug and verbose modes used mainly for development and testing" << "\n";
    dfile << "# NOTE: Debug and verbose level 5 will greatly increase log file size" << "\n";
    dfile << "debug = 0" << "\n";
    dfile << "debug_level = 1" << "\n";
    dfile << "verbose = 0 # echo log messages to the console" << "\n";
    dfile << "verbose_level = 1 # verbose level for debugging" << "\n";
    dfile << "" << "\n";
    dfile << "[CM_CRONTABS]" << "\n";
    dfile << "# Cluter crontabs" << "\n";
    dfile << "# CSV format: nickname, template_file, install_location, resource_script " << "\n";
    dfile << "system, /etc/drcm/crontabs/system_crons, /etc/cron.d, /etc/drcm/resources/crontab.sh" << "\n";
    dfile << "user, /etc/drcm/crontabs/user_crons, /etc/cron.d, /etc/drcm/resources/crontab.sh" << "\n";
    dfile << "apps1, /etc/drcm/crontabs/apps_package1, /etc/cron.d, /etc/drcm/resources/crontab.sh" << "\n";
    dfile << "apps2, /etc/drcm/crontabs/apps_package2, /etc/cron.d, /etc/drcm/resources/crontab.sh" << "\n";
    dfile << "" << "\n";
    dfile << "[CM_IPADDRS]" << "\n";
    dfile << "# Cluster floating IP addresses" << "\n";
    dfile << "# CSV format: nickname, Floating IP, netmask, Ethernet interface, ip takeover script" << "\n";
    dfile << "web, 192.168.122.25, 255.255.255.0, eth0:1, /etc/drcm/resources/ipv4addr.sh" << "\n";
    dfile << "ldm, 192.168.122.35, 255.255.255.0, eth0:2, /etc/drcm/resources/ipv4addr.sh" << "\n";
    dfile << "" << "\n";
    dfile << "[CM_SERVICES]" << "\n";
    dfile << "# Services managed by the cluster manager" << "\n";
    dfile << "# CSV format: nickname, service_name, resource_script" << "\n";
    dfile << "web, httpd, /etc/drcm/resources/service.sh" << "\n";
    dfile << "ntp, ntpd, /etc/drcm/resources/service.sh" << "\n";
    dfile << "" << "\n";
    dfile << "[CM_APPLICATIONS]" << "\n";
    dfile << "# Applications managed the by cluster manager" << "\n";
    dfile << "# CSV format: nickname, user:group, start_program, stop_program" << "\n";
    dfile << "# Or with optional application ensure script:" << "\n";
    dfile << "# CSV format: nickname, user:group, start_program, stop_program, ensure_script" << "\n";
    dfile << "##ldm, ldm:fxalpha, ldmadmin clean; ldmadmin mkqueue; ldmadmin start, ldmadmin stop; ldmadmin delqueue" << "\n";
    dfile << "" << "\n";
    dfile << "[CM_FILESYSTEMS]" << "\n";
    dfile << "# File system mounts for cluster nodes" << "\n";
    dfile << "# CSV format: nickname, source, target, resource_script" << "\n";
    dfile << "data, 192.168.122.1:/data, /data, /etc/drcm/resources/nfs.sh" << "\n";
    dfile << "archive, 192.168.122.1:/archive, /archive, /etc/drcm/resources/nfs.sh" << "\n";
    dfile << "### webshare, //192.168.122.225/users/web, /mnt/dfs, /etc/drcm/resources/cifs.sh" << "\n";
    dfile << "" << "\n";
    dfile << "# CM node configs" << "\n";
    dfile << "[CM_NODE]" << "\n";
    dfile << "nodename = cmnode1 # Your name for this cluster node " << "\n";
    dfile << "# Fully qualified hostname of this node" << "\n";
    dfile << "# Must match uname -n" << "\n";
    dfile << "hostname = vm1.example.com" << "\n";
    dfile << "#" << "\n";
    dfile << "node_is_active = 1 # Set to 0 to deacitvate this node but keep in config" << "\n";
    dfile << "#" << "\n";
    dfile << "keep_services = 0 # Set to 1 to keep services online if cluster manager exits " << "\n";
    dfile << "keep_filesystems = 1 # Set to 0 to unmount file systems when cluster manager exits" << "\n";
    dfile << "keep_applications = 0 # Set to 1 to keep applications online when cluster manager exits" << "\n";
    dfile << "#" << "\n";
    dfile << "# The IP of this node to send and receive cluster messages" << "\n";
    dfile << "keep_alive_ip = 192.168.122.111" << "\n";
    dfile << "#" << "\n";
    dfile << "# Number of seconds to init and watch cluster services" << "\n";
    dfile << "ipapddrs_check = 35" << "\n";
    dfile << "filesystems_check = 45" << "\n";
    dfile << "services_check = 50" << "\n";
    dfile << "crontabs_check = 60" << "\n";
    dfile << "applications_check = 65" << "\n";
    dfile << "#" << "\n";
    dfile << "# CSV list of crontabs to run on this node, format = nickname" << "\n";
    dfile << "# nicknames set in [CM_CRONTABS] section" << "\n";
    dfile << "node_crontabs = user,system,apps1" << "\n";
    dfile << "# CSV list of crontabs to backup, format = nodename:nickname" << "\n";
    dfile << "node_backup_crontabs = cmnode2:apps2" << "\n";
    dfile << "#" << "\n";
    dfile << "# CSV list of floating IP addresses on this node, format = nickname" << "\n";
    dfile << "# nicknames set in [CM_IPADDRS] section" << "\n";
    dfile << "node_floating_ip_addrs = web, ldm" << "\n";
    dfile << "# CSV list of floating IP addresses to backup, format = nodename:nickname" << "\n";
    dfile << "##node_backup_floating_ip_addrs =" << "\n";
    dfile << "#" << "\n";
    dfile << "# CSV list of services to run on this node, format = nickname" << "\n";
    dfile << "# nicknames set in [CM_SERVICES] section" << "\n";
    dfile << "node_services = web,ntp" << "\n";
    dfile << "# CSV list of services to backup, format = nodename:nickname" << "\n";
    dfile << "node_backup_services = " << "\n";
    dfile << "#" << "\n";
    dfile << "# CSV list of applications to run on this node, format = nickname" << "\n";
    dfile << "# nicknames set in [CM_APPLICATIONS] section" << "\n";
    dfile << "###node_applications = ldm" << "\n";
    dfile << "# CSV list of applications to backup, format = nodename:nickname" << "\n";
    dfile << "##node_backup_applications = " << "\n";
    dfile << "#" << "\n";
    dfile << "# CSV list of file systems to mount on this node, format = nickname" << "\n";
    dfile << "# nicknames set in [CM_FILESYSTEMS] section" << "\n";
    dfile << "node_filesystems = data" << "\n";
    dfile << "# CSV list of file systems to backup, format = nodename:nickname" << "\n";
    dfile << "node_backup_filesystems = cmnode2:archive" << "\n";
    dfile << "" << "\n";
    dfile << "[CM_NODE]" << "\n";
    dfile << "nodename = cmnode2 # Your name for this cluster node " << "\n";
    dfile << "# Fully qualified hostname of this node" << "\n";
    dfile << "# Must match uname -n" << "\n";
    dfile << "hostname = vm2.example.com" << "\n";
    dfile << "node_is_active = 1" << "\n";
    dfile << "keep_services = 0" << "\n";
    dfile << "keep_filesystems = 1" << "\n";
    dfile << "keep_applications = 0" << "\n";
    dfile << "keep_alive_ip = 192.168.122.112" << "\n";
    dfile << "ipapddrs_check = 40" << "\n";
    dfile << "filesystems_check = 50" << "\n";
    dfile << "services_check = 55" << "\n";
    dfile << "crontabs_check = 65" << "\n";
    dfile << "applications_check = 65" << "\n";
    dfile << "node_crontabs = apps2" << "\n";
    dfile << "node_backup_crontabs = cmnode1:user,cmnode1:system,cmnode1:apps1" << "\n";
    dfile << "##node_floating_ip_addrs =" << "\n";
    dfile << "node_backup_floating_ip_addrs = cmnode1:web, cmnode1:ldm" << "\n";
    dfile << "node_services = ntp" << "\n";
    dfile << "node_backup_services = cmnode1:web " << "\n";
    dfile << "##node_applications =" << "\n";
    dfile << "###node_backup_applications = cmnode1:ldm " << "\n";
    dfile << "node_filesystems = archive" << "\n";
    dfile << "node_backup_filesystems = cmnode1:data" << "\n";
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
    info_line.ReplaceString("\\#", "HashMarkPlaceHolder");
    if(info_line[0] == '#') continue; // Skip remark lines
    info_line.DeleteAfterIncluding("#"); // Filter inline remarks
    info_line.TrimTrailingSpaces();
    info_line.ReplaceString("HashMarkPlaceHolder", "#");
    list.Add(info_line);
  }

  gxListNode<gxString> *ptr;
  int has_cmserver = 0;
  int has_cmnode = 0;
  ptr = list.GetHead();
  char pbuf[255];
  gxString parm;

  gxString line_dup;
  gxListNode<gxString> *listptr;
  gxList<gxString> cronlist;
  gxList<gxString> iplist;
  gxList<gxString> serviceslist;
  gxList<gxString> applicationslist;
  gxList<gxString> filesystemslist;
  unsigned num_arr;
  gxString *vals = 0; 
  gxString delimiter = ",";
  unsigned i = 0;

  while(ptr) {
    gxString strdup = ptr->data;
    strdup.FilterChar(' ');
    if(strdup.Find("[CM_SERVER]") != -1) {
      has_cmserver = 1;
      ptr = ptr->next;
      while(ptr) {
	if(ptr->data[0] == '[') {
	  ptr = ptr->prev;
	  break;
	}
	// Add all server conifg reads here
	if(ptr->data.Find("debug") != -1) {
	  if(!servercfg->has_debug_override) servercfg->debug = CfgGetTrueFalseValue(ptr->data);
	}
	if(ptr->data.Find("debug_level") != -1) {
	  if(!servercfg->has_debug_level_override) servercfg->debug_level = CfgGetEqualToParm(ptr->data);
	}
	if(ptr->data.Find("verbose") != -1) {
	  if(!servercfg->has_verbose_override) servercfg->verbose = CfgGetTrueFalseValue(ptr->data);
	}
	if(ptr->data.Find("verbose_level") != -1) {
	  if(!servercfg->has_verbose_level_override) servercfg->verbose_level = CfgGetEqualToParm(ptr->data);
	}
	if(ptr->data.Find("clear_log") != -1) {
	  if(!servercfg->has_log_file_clear_override) servercfg->clear_log = CfgGetTrueFalseValue(ptr->data);
	}
	if(ptr->data.Find("logfile_name") != -1) {
	  if(!servercfg->has_log_file_name_override) servercfg->logfile_name = CfgGetEqualToParm(ptr->data, pbuf);
	}
	if(ptr->data.Find("enable_logging") != -1) {
	  if(!servercfg->has_enable_logging_override) servercfg->enable_logging = CfgGetTrueFalseValue(ptr->data);
	}
	if(ptr->data.Find("log_level") != -1) {
	  if(!servercfg->has_log_level_override) servercfg->log_level = CfgGetEqualToParm(ptr->data);
	}
	if(ptr->data.Find("service_name") != -1) {
	  if(!servercfg->is_client) servercfg->service_name = CfgGetEqualToParm(ptr->data, pbuf);
	}
	if(ptr->data.Find("num_logs_to_keep") != -1) {
	  servercfg->num_logs_to_keep = CfgGetEqualToParm(ptr->data);
	  if(servercfg->num_logs_to_keep <= 0) {
	    NT_print("Config file has bad num_logs_to_keep value");
	    error_level = 1;
	  }
	}
	if(ptr->data.Find("max_log_size") != -1) {
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
	if(ptr->data.Find("log_queue_size") != -1) {
	  servercfg->log_queue_size = CfgGetEqualToParm(ptr->data);
	  if(servercfg->log_queue_size <= 0) {
	    NT_print("Config file has bad log_queue_size value");
	    error_level = 1;
	  }
	}
	if(ptr->data.Find("log_queue_debug_size") != -1) {
	  servercfg->log_queue_debug_size = CfgGetEqualToParm(ptr->data);
	  if(servercfg->log_queue_debug_size <= 0) {
	    NT_print("Config file has bad log_queue_debug_size value");
	    error_level = 1;
	  }
	}
	if(ptr->data.Find("log_queue_proc_size") != -1) {
	  servercfg->log_queue_proc_size = CfgGetEqualToParm(ptr->data);
	  if(servercfg->log_queue_proc_size <= 0) {
	    NT_print("Config file has bad log_queue_proc_size value");
	    error_level = 1;
	  }
	}
	if(ptr->data.Find("udpport") != -1) {
	  servercfg->udpport = CfgGetEqualToParm(ptr->data);
	  if(servercfg->udpport <= 0) {
	    NT_print("Config file has bad server udpport value");
	    error_level = 1;
	  }
	}
	if(ptr->data.Find("tcpport") != -1) {
	  servercfg->tcpport = CfgGetEqualToParm(ptr->data);
	  if(servercfg->tcpport <= 0) {
	    NT_print("Config file has bad server tcpport value");
	    error_level = 1;
	  }
	}
	if(ptr->data.Find("dead_node_count") != -1) {
	  servercfg->dead_node_count = CfgGetEqualToParm(ptr->data);
	  if(servercfg->dead_node_count <= 0) {
	    NT_print("Config file has bad somaxconn value");
	    error_level = 1;
	  }
	}
	if(ptr->data.Find("run_dir") != -1) {
	  servercfg->run_dir = CfgGetEqualToParm(ptr->data, pbuf);
	}
	if(ptr->data.Find("log_dir") != -1) {
	  servercfg->log_dir = CfgGetEqualToParm(ptr->data, pbuf);
	}
	if(ptr->data.Find("spool_dir") != -1) {
	  servercfg->spool_dir = CfgGetEqualToParm(ptr->data, pbuf);
	}
	if(ptr->data.Find("cluster_user") != -1) {
	  servercfg->cluster_user = CfgGetEqualToParm(ptr->data, pbuf);
	}
	if(ptr->data.Find("cluster_group") != -1) {
	  servercfg->cluster_group = CfgGetEqualToParm(ptr->data, pbuf);
	}
	if(ptr->data.Find("cluser_mask") != -1) {
	  servercfg->cluster_umask = CfgGetEqualToParm(ptr->data, pbuf);
	}
	if(ptr->data.Find("sudo_command") != -1) {
	  servercfg->sudo_command = CfgGetEqualToParm(ptr->data, pbuf);
	}
	if(ptr->data.Find("bind_to_keep_alive_ip") != -1) {
	  servercfg->bind_to_keep_alive_ip = CfgGetTrueFalseValue(ptr->data);
	}
	ptr = ptr->next;
      }
    }

    if(strdup.Find("[CM_CRONTABS]") != -1) {
      // Start of section
      ptr = ptr->next;
      
      while(ptr) {
	if(ptr->data[0] == '[') {
	  // End of section
	  ptr = ptr->prev;
	  break;
	}
	line_dup = ptr->data;
	cronlist.Add(line_dup);
	ptr = ptr->next;
	if(!ptr) {
	  break;
	}
      }
    }

    if(strdup.Find("[CM_IPADDRS]") != -1) {
      // Start of section
      ptr = ptr->next;
      
      while(ptr) {
	if(ptr->data[0] == '[') {
	  // End of section
	  ptr = ptr->prev;
	  break;
	}
	line_dup = ptr->data;
	iplist.Add(line_dup);
	ptr = ptr->next;
	if(!ptr) {
	  break;
	}
      }
    }

    if(strdup.Find("[CM_SERVICES]") != -1) {
      // Start of section
      ptr = ptr->next;
      
      while(ptr) {
	if(ptr->data[0] == '[') {
	  // End of section
	  ptr = ptr->prev;
	  break;
	}
	line_dup = ptr->data;
	serviceslist.Add(line_dup);
	ptr = ptr->next;
	if(!ptr) {
	  break;
	}
      }
    }

    if(strdup.Find("[CM_APPLICATIONS]") != -1) {
      // Start of section
      ptr = ptr->next;
      
      while(ptr) {
	if(ptr->data[0] == '[') {
	  // End of section
	  ptr = ptr->prev;
	  break;
	}
	line_dup = ptr->data;
	if(!line_dup.is_null()) applicationslist.Add(line_dup);
	ptr = ptr->next;
	if(!ptr) {
	  break;
	}
      }
    }

    if(strdup.Find("[CM_FILESYSTEMS]") != -1) {
      // Start of section
      ptr = ptr->next;
      
      while(ptr) {
	if(ptr->data[0] == '[') {
	  // End of section
	  ptr = ptr->prev;
	  break;
	}
	line_dup = ptr->data;
	filesystemslist.Add(line_dup);
	ptr = ptr->next;
	if(!ptr) {
	  break;
	}
      }
    }
        
    if(strdup.Find("[CM_NODE]") != -1) {
      has_cmnode = 1;
      // Start new node
      CMnode *n = new CMnode;
      ptr = ptr->next;
      
      while(ptr) {
	if(ptr->data[0] == '[') {
	  // End of new node
	  servercfg->nodes.Add(n);
	  ptr = ptr->prev;
	  break;
	}
	// Add all node config reads here
	if(ptr->data.Find("nodename") != -1) {
	  n->nodename = CfgGetEqualToParm(ptr->data, pbuf);
	}
	if(ptr->data.Find("hostname") != -1) {
	  n->hostname = CfgGetEqualToParm(ptr->data, pbuf);
	}
	if(ptr->data.Find("node_is_active") != -1) {
	  n->node_is_active = CfgGetTrueFalseValue(ptr->data);
	}
	if(ptr->data.Find("keep_services") != -1) {
	  n->keep_services = CfgGetTrueFalseValue(ptr->data);
	}
	if(ptr->data.Find("keep_filesystems") != -1) {
	  n->keep_filesystems = CfgGetTrueFalseValue(ptr->data);
	}
	if(ptr->data.Find("keep_applications") != -1) {
	  n->keep_applications = CfgGetTrueFalseValue(ptr->data);
	}
	if(ptr->data.Find("keep_alive_ip") != -1) {
	  n->keep_alive_ip = CfgGetEqualToParm(ptr->data, pbuf);
	}
	if(ptr->data.Find("crontabs_check") != -1) {
	  n->crontabs_check = CfgGetEqualToParm(ptr->data);
	  if(n->crontabs_check <= 0) {
	    NT_print("Config file has bad crontabs_check value");
	    error_level = 1;
	  }
	}
	if(ptr->data.Find("services_check") != -1) {
	  n->services_check = CfgGetEqualToParm(ptr->data);
	  if(n->services_check <= 0) {
	    NT_print("Config file has bad services_check value");
	    error_level = 1;
	  }
	}
	if(ptr->data.Find("applications_check") != -1) {
	  n->applications_check = CfgGetEqualToParm(ptr->data);
	  if(n->applications_check <= 0) {
	    NT_print("Config file has bad applications_check value");
	    error_level = 1;
	  }
	}
	if(ptr->data.Find("ipaddr_check") != -1) {
	  n->ipaddrs_check = CfgGetEqualToParm(ptr->data);
	  if(n->ipaddrs_check <= 0) {
	    NT_print("Config file has bad ipaddrs_check value");
	    error_level = 1;
	  }
	}
	if(ptr->data.Find("filesystems_check") != -1) {
	  n->filesystems_check = CfgGetEqualToParm(ptr->data);
	  if(n->filesystems_check <= 0) {
	    NT_print("Config file has bad filesystems_check value");
	    error_level = 1;
	  }
	}

	if(ptr->data.Find("node_crontabs") != -1) {
	  line_dup = ptr->data;
	  line_dup.DeleteBeforeIncluding("=");
	  line_dup.FilterChar(' '); line_dup.FilterChar('\n');
	  if(servercfg->check_config) {
	    NT_print("Adding node crons:", line_dup.c_str());
	  }
	  while(line_dup.Find(",,") != -1) line_dup.ReplaceString(",,", ",");      
	  line_dup.TrimLeading(','); line_dup.TrimTrailing(',');
	  if(line_dup.Find(",") == -1) {
	    if(!line_dup.is_null()) n->crontabs.Add(line_dup);
	  }
	  else {
	    num_arr = 0;
	    vals = ParseStrings(line_dup, delimiter, num_arr);
	    for(i = 0; i < num_arr; i++) {
	      n->crontabs.Add(vals[i]);
	    }
	    if(vals) {
	      delete[] vals;
	      vals = 0; 
	    }
	  }
	}
	if(ptr->data.Find("node_backup_crontabs") != -1) {
	  line_dup = ptr->data;
	  line_dup.DeleteBeforeIncluding("=");
	  line_dup.FilterChar(' '); line_dup.FilterChar('\n');
	  line_dup.FilterChar('\n');
	  if(servercfg->check_config) {
	    NT_print("Adding node backup crons:", line_dup.c_str());
	  }
	  while(line_dup.Find(",,") != -1) line_dup.ReplaceString(",,", ",");      
	  line_dup.TrimLeading(','); line_dup.TrimTrailing(',');
	  if(line_dup.Find(",") == -1) {
	    if(!line_dup.is_null()) n->backup_crontabs.Add(line_dup);
	  }
	  else {
	    num_arr = 0;
	    vals = ParseStrings(line_dup, delimiter, num_arr);
	    for(i = 0; i < num_arr; i++) {
	      n->backup_crontabs.Add(vals[i]);
	    }
	    if(vals) {
	      delete[] vals;
	      vals = 0; 
	    }
	  }
	}

	if(ptr->data.Find("node_floating_ip_addrs") != -1) {
	  line_dup = ptr->data;
	  line_dup.DeleteBeforeIncluding("=");
	  line_dup.FilterChar(' '); line_dup.FilterChar('\n');
	  line_dup.FilterChar('\n');
	  if(servercfg->check_config) {
	    NT_print("Adding node floating IP addr:", line_dup.c_str());
	  }
	  while(line_dup.Find(",,") != -1) line_dup.ReplaceString(",,", ",");      
	  line_dup.TrimLeading(','); line_dup.TrimTrailing(',');
	  if(line_dup.Find(",") == -1) {
	    if(!line_dup.is_null()) n->floating_ip_addrs.Add(line_dup);
	  }
	  else {
	    num_arr = 0;
	    vals = ParseStrings(line_dup, delimiter, num_arr);
	    for(i = 0; i < num_arr; i++) {
	      n->floating_ip_addrs.Add(vals[i]);
	    }
	    if(vals) {
	      delete[] vals;
	      vals = 0; 
	    }
	  }
	}
	if(ptr->data.Find("node_backup_floating_ip_addrs") != -1) {
	  line_dup = ptr->data;
	  line_dup.DeleteBeforeIncluding("=");
	  line_dup.FilterChar(' '); line_dup.FilterChar('\n');
	  if(servercfg->check_config) {
	    NT_print("Adding node backup floating IP addr:", line_dup.c_str());
	  }
	  while(line_dup.Find(",,") != -1) line_dup.ReplaceString(",,", ",");      
	  line_dup.TrimLeading(','); line_dup.TrimTrailing(',');
	  if(line_dup.Find(",") == -1) {
	    if(!line_dup.is_null()) n->backup_floating_ip_addrs.Add(line_dup);
	  }
	  else {
	    num_arr = 0;
	    vals = ParseStrings(line_dup, delimiter, num_arr);
	    for(i = 0; i < num_arr; i++) {
	      n->backup_floating_ip_addrs.Add(vals[i]);
	    }
	    if(vals) {
	      delete[] vals;
	      vals = 0; 
	    }
	  }
	}

	if(ptr->data.Find("node_services") != -1) {
	  line_dup = ptr->data;
	  line_dup.DeleteBeforeIncluding("=");
	  line_dup.FilterChar(' '); line_dup.FilterChar('\n');
	  line_dup.FilterChar('\n');
	  if(servercfg->check_config) {
	    NT_print("Adding node service:", line_dup.c_str());
	  }
	  while(line_dup.Find(",,") != -1) line_dup.ReplaceString(",,", ",");      
	  line_dup.TrimLeading(','); line_dup.TrimTrailing(',');
	  if(line_dup.Find(",") == -1) {
	    if(!line_dup.is_null()) n->services.Add(line_dup);
	  }
	  else {
	    num_arr = 0;
	    vals = ParseStrings(line_dup, delimiter, num_arr);
	    for(i = 0; i < num_arr; i++) {
	      n->services.Add(vals[i]);
	    }
	    if(vals) {
	      delete[] vals;
	      vals = 0; 
	    }
	  }
	}
	if(ptr->data.Find("node_backup_services") != -1) {
	  line_dup = ptr->data;
	  line_dup.DeleteBeforeIncluding("=");
	  line_dup.FilterChar(' '); line_dup.FilterChar('\n');
	  if(servercfg->check_config) {
	    NT_print("Adding node backup service:", line_dup.c_str());
	  }
	  while(line_dup.Find(",,") != -1) line_dup.ReplaceString(",,", ",");      
	  line_dup.TrimLeading(','); line_dup.TrimTrailing(',');
	  if(line_dup.Find(",") == -1) {
	    if(!line_dup.is_null()) n->backup_services.Add(line_dup);
	  }
	  else {
	    num_arr = 0;
	    vals = ParseStrings(line_dup, delimiter, num_arr);
	    for(i = 0; i < num_arr; i++) {
	      n->backup_services.Add(vals[i]);
	    }
	    if(vals) {
	      delete[] vals;
	      vals = 0; 
	    }
	  }
	}

	if(ptr->data.Find("node_applications") != -1) {
	  line_dup = ptr->data;
	  line_dup.DeleteBeforeIncluding("=");
	  line_dup.FilterChar(' '); line_dup.FilterChar('\n');
	  if(servercfg->check_config) {
	    NT_print("Adding node application:", line_dup.c_str());
	  }
	  while(line_dup.Find(",,") != -1) line_dup.ReplaceString(",,", ",");      
	  line_dup.TrimLeading(','); line_dup.TrimTrailing(',');
	  if(line_dup.Find(",") == -1) {
	    if(!line_dup.is_null()) n->applications.Add(line_dup);
	  }
	  else {
	    num_arr = 0;
	    vals = ParseStrings(line_dup, delimiter, num_arr);
	    for(i = 0; i < num_arr; i++) {
	      n->applications.Add(vals[i]);
	    }
	    if(vals) {
	      delete[] vals;
	      vals = 0; 
	    }
	  }
	}
	if(ptr->data.Find("node_backup_applications") != -1) {
	  line_dup = ptr->data;
	  line_dup.DeleteBeforeIncluding("=");
	  line_dup.FilterChar(' '); line_dup.FilterChar('\n');
	  if(servercfg->check_config) {
	    NT_print("Adding node backup application:", line_dup.c_str());
	  }
	  while(line_dup.Find(",,") != -1) line_dup.ReplaceString(",,", ",");      
	  line_dup.TrimLeading(','); line_dup.TrimTrailing(',');
	  if(line_dup.Find(",") == -1) {
	    if(!line_dup.is_null()) n->backup_applications.Add(line_dup);
	  }
	  else {
	    num_arr = 0;
	    vals = ParseStrings(line_dup, delimiter, num_arr);
	    for(i = 0; i < num_arr; i++) {
	      n->backup_applications.Add(vals[i]);
	    }
	    if(vals) {
	      delete[] vals;
	      vals = 0; 
	    }
	  }
	}

	if(ptr->data.Find("node_filesystems") != -1) {
	  line_dup = ptr->data;
	  line_dup.DeleteBeforeIncluding("=");
	  line_dup.FilterChar(' '); line_dup.FilterChar('\n');
	  if(servercfg->check_config) {
	    NT_print("Adding node file system:", line_dup.c_str());
	  }
	  while(line_dup.Find(",,") != -1) line_dup.ReplaceString(",,", ",");      
	  line_dup.TrimLeading(','); line_dup.TrimTrailing(',');
	  if(line_dup.Find(",") == -1) {
	    if(!line_dup.is_null()) n->filesystems.Add(line_dup);
	  }
	  else {
	    num_arr = 0;
	    vals = ParseStrings(line_dup, delimiter, num_arr);
	    for(i = 0; i < num_arr; i++) {
	      n->filesystems.Add(vals[i]);
	    }
	    if(vals) {
	      delete[] vals;
	      vals = 0; 
	    }
	  }
	}
	if(ptr->data.Find("node_backup_filesystems") != -1) {
	  line_dup = ptr->data;
	  line_dup.DeleteBeforeIncluding("=");
	  line_dup.FilterChar(' '); line_dup.FilterChar('\n');
	  if(servercfg->check_config) {
	    NT_print("Adding node backup file system:", line_dup.c_str());
	  }
	  while(line_dup.Find(",,") != -1) line_dup.ReplaceString(",,", ",");      
	  line_dup.TrimLeading(','); line_dup.TrimTrailing(',');
	  if(line_dup.Find(",") == -1) {
	    if(!line_dup.is_null()) n->backup_filesystems.Add(line_dup);
	  }
	  else {
	    num_arr = 0;
	    vals = ParseStrings(line_dup, delimiter, num_arr);
	    for(i = 0; i < num_arr; i++) {
	      n->backup_filesystems.Add(vals[i]);
	    }
	    if(vals) {
	      delete[] vals;
	      vals = 0; 
	    }
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
  
  if(has_cmserver == 0) {
    NT_print("Config file missing [CM_SERVER] section", parm.c_str());
    error_level = 1;
  }
  if(has_cmnode == 0) {
    NT_print("Config file missing [CMNODE] section(s)", parm.c_str());
    error_level = 1;
  }
  
  // Set CM logfile name
  if(servercfg->is_client) {
    if(!servercfg->has_log_file_name_override) servercfg->logfile_name << clear << "drcm_client.log";
  }

  if(servercfg->logfile_name.Find("/") != -1) {
    servercfg->logfile_name.DeleteBeforeLastIncluding("/");
  }
  gxString logbuf = servercfg->logfile_name;
  servercfg->logfile_name << clear << servercfg->log_dir << "/" << logbuf;
  
  // Add the CM services for all nodes
  listptr = cronlist.GetHead();
  if(!listptr) {
    if(servercfg->check_config) NT_print("INFO - CM config does not have any crontabs");
  }
  else {
    if(servercfg->check_config) NT_print("Installing CM crontabs");
    while(listptr) {
      line_dup = listptr->data.c_str();
      line_dup.TrimLeadingSpaces(); line_dup.TrimTrailingSpaces();
      line_dup.FilterChar('\n');
      if(line_dup.is_null()) {
	listptr = listptr->next;
	continue;
      }
      if(servercfg->check_config) NT_print("Adding crontab:", line_dup.c_str());
      while(line_dup.Find(",,") != -1) line_dup.ReplaceString(",,", ",");      
      line_dup.TrimLeading(','); line_dup.TrimTrailing(',');
      num_arr = 0;
      if(line_dup.is_null()) {
	listptr = listptr->next;
	continue;
      }
      vals = ParseStrings(line_dup, delimiter, num_arr);
      if(num_arr != 4) {
	NT_print("ERROR - missing or extra values on line:", listptr->data.c_str());
	error_level++;
      }
      else {
	CMcrontabs crontab;
	crontab.nickname = vals[0];  
	crontab.nickname.TrimLeadingSpaces(); crontab.nickname.TrimTrailingSpaces();  
	crontab.template_file = vals[1];
	crontab.template_file.FilterChar(' ');
	crontab.install_location = vals[2];
	crontab.install_location.FilterChar(' ');
	crontab.resource_script = vals[3];
	crontab.resource_script.TrimLeadingSpaces(); crontab.resource_script.TrimTrailingSpaces();  
	servercfg->num_resource_crons++;
	servercfg->node_crontabs.Add(crontab);
      }
      if(vals) { delete[] vals; vals = 0; }
      listptr = listptr->next;
    }
  }

  listptr = iplist.GetHead();
  if(!listptr) {
    if(servercfg->check_config) NT_print("INFO - CM config does not have any floating IP addresses");
  }
  else {
   if(servercfg->check_config)  NT_print("Installing CM floating IP addresses");
    while(listptr) {
      line_dup = listptr->data.c_str();
      line_dup.TrimLeadingSpaces(); line_dup.TrimTrailingSpaces();
      line_dup.FilterChar('\n');
      if(line_dup.is_null()) {
	listptr = listptr->next;
	continue;
      }
      if(servercfg->check_config) NT_print("Adding floating IP:", line_dup.c_str());
      while(line_dup.Find(",,") != -1) line_dup.ReplaceString(",,", ",");      
      line_dup.TrimLeading(','); line_dup.TrimTrailing(',');
      num_arr = 0;
      if(line_dup.is_null()) {
	listptr = listptr->next;
	continue;
      }
      vals = ParseStrings(line_dup, delimiter, num_arr);
      if(num_arr != 5) {
	NT_print("ERROR - missing or extra values on line:", listptr->data.c_str());
	error_level++;
      }
      else {
	CMipaddrs ipaddr;
	ipaddr.nickname = vals[0];  
	ipaddr.nickname.TrimLeadingSpaces(); ipaddr.nickname.TrimTrailingSpaces();  
	ipaddr.ip_address = vals[1];
	ipaddr.ip_address.FilterChar(' ');
	ipaddr.netmask = vals[2];
	ipaddr.netmask.FilterChar(' ');
	ipaddr.interface = vals[3];
	ipaddr.interface.FilterChar(' ');
	ipaddr.resource_script = vals[4];
	ipaddr.resource_script.TrimLeadingSpaces(); ipaddr.resource_script.TrimTrailingSpaces();  
	servercfg->num_resource_ipaddrs++;
	servercfg->node_ipaddrs.Add(ipaddr);
      }
      if(vals) { delete[] vals; vals = 0; }
      listptr = listptr->next;
    }
  }

  listptr = serviceslist.GetHead();
  if(!listptr) {
    if(servercfg->check_config) NT_print("INFO - CM config does not have any services");
  }
  else {
    if(servercfg->check_config) NT_print("Installing CM services");
    while(listptr) {
      line_dup = listptr->data.c_str();
      line_dup.TrimLeadingSpaces(); line_dup.TrimTrailingSpaces();
      line_dup.FilterChar('\n');
      if(line_dup.is_null()) {
	listptr = listptr->next;
	continue;
      }
      if(servercfg->check_config) NT_print("Adding service:", line_dup.c_str());
      while(line_dup.Find(",,") != -1) line_dup.ReplaceString(",,", ",");      
      line_dup.TrimLeading(','); line_dup.TrimTrailing(',');
      num_arr = 0;
      if(line_dup.is_null()) {
	listptr = listptr->next;
	continue;
      }
      vals = ParseStrings(line_dup, delimiter, num_arr);
      if(num_arr != 3) {
	NT_print("ERROR - missing or extra values on line:", listptr->data.c_str());
	error_level++;
      }
      else {
	CMservices service;
	service.nickname = vals[0];  
	service.nickname.TrimLeadingSpaces(); service.nickname.TrimTrailingSpaces();  
	service.service_name = vals[1];
	service.service_name.FilterChar(' ');
	service.resource_script = vals[2];
	service.resource_script.TrimLeadingSpaces(); service.resource_script.TrimTrailingSpaces();  
	servercfg->num_resource_services++;
	servercfg->node_services.Add(service);
      }
      if(vals) { delete[] vals; vals = 0; }
      listptr = listptr->next;
    }
  }

  listptr = applicationslist.GetHead();
  if(!listptr) {
    if(servercfg->check_config) NT_print("INFO - CM config does not have any applications");
  }
  else {
    if(servercfg->check_config) NT_print("Installing CM applications");
    while(listptr) {
      line_dup = listptr->data.c_str();
      line_dup.TrimLeadingSpaces(); line_dup.TrimTrailingSpaces();
      line_dup.FilterChar('\n');
      if(line_dup.is_null()) {
	listptr = listptr->next;
	continue;
      }
      if(servercfg->check_config) NT_print("Adding application:", line_dup.c_str());
      while(line_dup.Find(",,") != -1) line_dup.ReplaceString(",,", ",");      
      line_dup.TrimLeading(','); line_dup.TrimTrailing(',');
      num_arr = 0;
      if(line_dup.is_null()) {
	listptr = listptr->next;
	continue;
      }
      vals = ParseStrings(line_dup, delimiter, num_arr);
      if(num_arr < 4 || num_arr > 5) {
	NT_print("ERROR - missing or extra values on line:", listptr->data.c_str());
	error_level++;
      }
      else {
	CMapplications application;
	if(vals[1].Find(":") == -1) {
	  NT_print("ERROR - Bad user:group on line:", listptr->data.c_str());
	  error_level++;
	}
	else {
	  application.nickname = vals[0];  
	  application.nickname.TrimLeadingSpaces(); application.nickname.TrimTrailingSpaces();  
	  gxString appuser = vals[1];
	  appuser.DeleteAfterIncluding(":");
	  appuser.FilterChar(' ');
	  application.user = appuser;
	  gxString appgroup = vals[1];
	  appgroup.DeleteBeforeLastIncluding(":");
	  appgroup.FilterChar(' ');
	  application.group = appgroup;
	  application.start_program = vals[2];
	  application.start_program.TrimLeadingSpaces(); application.start_program.TrimTrailingSpaces();  
	  application.stop_program = vals[3];
	  application.stop_program.TrimLeadingSpaces(); application.stop_program.TrimTrailingSpaces();  
	  if(num_arr == 5) {
	    application.ensure_script = vals[4];
	    application.ensure_script.TrimLeadingSpaces(); application.ensure_script.TrimTrailingSpaces();  
	  }
	  servercfg->num_resource_applications++;
	  servercfg->node_applications.Add(application);
	}
      }
      if(vals) { delete[] vals; vals = 0; }
      listptr = listptr->next;
    }
  }
  
  listptr = filesystemslist.GetHead();
  if(!listptr) {
    if(servercfg->check_config) NT_print("INFO - CM config does not have any file systems");
  }
  else {
    if(servercfg->check_config) NT_print("Installing CM file systems");
    while(listptr) {
      line_dup = listptr->data.c_str();
      line_dup.TrimLeadingSpaces(); line_dup.TrimTrailingSpaces();
      line_dup.FilterChar('\n');
      if(line_dup.is_null()) {
	listptr = listptr->next;
	continue;
      }
      if(servercfg->check_config) NT_print("Adding file system:", line_dup.c_str());
      while(line_dup.Find(",,") != -1) line_dup.ReplaceString(",,", ",");      
      line_dup.TrimLeading(','); line_dup.TrimTrailing(',');
      num_arr = 0;
      if(line_dup.is_null()) {
	listptr = listptr->next;
	continue;
      }
      vals = ParseStrings(line_dup, delimiter, num_arr);
      if(num_arr != 4) {
	NT_print("ERROR - missing or extra values on line:", listptr->data.c_str());
	error_level++;
      }
      else {
	CMfilesystems filesystem;
	filesystem.nickname = vals[0];  
	filesystem.nickname.TrimLeadingSpaces(); filesystem.nickname.TrimTrailingSpaces();  
	filesystem.source = vals[1];
	filesystem.source.FilterChar(' ');
	filesystem.target = vals[2];
	filesystem.target.FilterChar(' ');
	filesystem.resource_script = vals[3];
	filesystem.resource_script.TrimLeadingSpaces(); filesystem.resource_script.TrimTrailingSpaces();  
	servercfg->num_resource_filesystems++;
	servercfg->node_filesystems.Add(filesystem);
      }
      if(vals) { delete[] vals; vals = 0; }
      listptr = listptr->next;
    }
  }
  
  ifile.df_Close();
  return error_level;
}

int NT_check_dir(const gxString &dir)
{
  if(futils_exists(dir.c_str())) return 1;
  gxString command;
  gxString superuser = "root";
  if(servercfg->cluster_user == superuser) futils_mkdir(dir.c_str());
  if(servercfg->cluster_user != superuser) {
    command << clear << servercfg->sudo_command << " \'/bin/mkdir -p " << dir.c_str() << " &> /dev/null\'";
    if(servercfg->log_level >= 4 || servercfg->debug) {
      NT_print("SUDO command:", command.c_str()); 
    }
    system(command.c_str());
    command << clear << servercfg->sudo_command << " \'chown " 
	    << servercfg->cluster_user << ":" << servercfg->cluster_group << " " << dir.c_str() << " &> /dev/null\'";
    if(servercfg->log_level >= 4 || servercfg->debug) {
      NT_print("SUDO command:", command.c_str()); 
    }
    system(command.c_str());
  }
  if(!futils_exists(dir.c_str())) return 0;

  return 1;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
