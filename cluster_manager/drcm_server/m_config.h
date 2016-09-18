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

Program config funcions for Datareel cluster manager.

*/
// ----------------------------------------------------------- //   
#ifndef __DRCM_CONFIG_HPP__
#define __DRCM_CONFIG_HPP__

#include "gxdlcode.h"
#include "drcm_server.h"

struct CMcrontabs {
  CMcrontabs() { }
  ~CMcrontabs() { }
  CMcrontabs(const CMcrontabs &n) { Copy(n); }
  CMcrontabs operator=(const CMcrontabs &n) {
    Copy(n);
    return *this;
  }

  void Copy(const CMcrontabs &n) {
    if(&n == this) return;
    nickname = n.nickname;
    template_file = n.template_file;
    install_location = n.install_location;
    resource_script = n.resource_script;
  }

  gxString nickname;
  gxString template_file;
  gxString install_location;
  gxString resource_script;
};

struct CMipaddrs {
  CMipaddrs() { }
  ~CMipaddrs() { }
  CMipaddrs(const CMipaddrs &n) { Copy(n); }
  CMipaddrs operator=(const CMipaddrs &n) {
    Copy(n);
    return *this;
  }

  void Copy(const CMipaddrs &n) {
    if(&n == this) return;
    nickname = n.nickname;
    ip_address = n.ip_address;
    netmask = n.netmask;
    interface = n.interface;
    resource_script = n.resource_script;
  }

  gxString nickname;
  gxString ip_address;
  gxString netmask;
  gxString interface;
  gxString resource_script;
};

struct CMservices {
  CMservices() { }
  ~CMservices() { }
  CMservices(const CMservices &n) { Copy(n); }
  CMservices operator=(const CMservices &n) {
    Copy(n);
    return *this;
  }

  void Copy(const CMservices &n) {
    if(&n == this) return;
    nickname = n.nickname;
    service_name = n.service_name;
    resource_script = n.resource_script;
  }

  gxString nickname;
  gxString service_name;
  gxString resource_script;
};

struct CMapplications {
  CMapplications() { }
  ~CMapplications() { }
  CMapplications(const CMapplications &n) { Copy(n); }
  CMapplications operator=(const CMapplications &n) {
    Copy(n);
    return *this;
  }

  void Copy(const CMapplications &n) {
    if(&n == this) return;
    nickname = n.nickname;
    user = n.user;
    group = n.group;
    start_program = n.start_program;
    stop_program = n.stop_program;
    ensure_script = n.ensure_script;
  }

  gxString nickname;
  gxString user;
  gxString group;
  gxString start_program;
  gxString stop_program;
  gxString ensure_script;
};

struct CMfilesystems {
  CMfilesystems() { }
  ~CMfilesystems() { }
  CMfilesystems(const CMfilesystems &n) { Copy(n); }
  CMfilesystems operator=(const CMfilesystems &n) {
    Copy(n);
    return *this;
  }

  void Copy(const CMfilesystems &n) {
    if(&n == this) return;
    nickname = n.nickname;
    source = n.source;
    target = n.target;
    resource_script = n.resource_script;
  }

  gxString nickname;
  gxString source;
  gxString target;
  gxString resource_script;
};

struct CMnode {
  CMnode();
  ~CMnode() { }
  
  CMnode(const CMnode &n) { Copy(n); }
  CMnode operator=(const CMnode &n) {
    Copy(n);
    return *this;
  }

  int CLUSTER_TAKE_OVER_FLAG(int set = 0, int unset = 0);
  void Copy(const CMnode &n);
  void copy_list(const gxList<gxString> &source, gxList<gxString> &dest);

  gxString nodename;
  gxString hostname;
  gxString keep_alive_ip;
  int crontabs_check;
  int services_check;
  int applications_check;
  int ipaddrs_check;
  int filesystems_check;
  int keep_services;
  int keep_filesystems;
  int keep_applications;
  gxList<gxString> crontabs; 
  gxList<gxString> backup_crontabs; 
  gxList<gxString> floating_ip_addrs; 
  gxList<gxString> backup_floating_ip_addrs;  
  gxList<gxString> services; 
  gxList<gxString> backup_services; 
  gxList<gxString> applications; 
  gxList<gxString> backup_applications; 
  gxList<gxString> filesystems;
  gxList<gxString> backup_filesystems;
  CMnode *active_ptr;
  int node_is_active;

private:
  int cluster_take_over_flag; 
  gxMutex cluster_take_over_flag_lock;
  int cluster_take_over_flag_is_locked;
};

// Node info for detailed status reports
struct CMstatsnode {
  CMstatsnode() {
    is_alive = 0;
  }
  ~CMstatsnode() { }
  CMstatsnode(const CMstatsnode &n) { Copy(n); }
  CMstatsnode operator=(const CMstatsnode &n) {
    Copy(n);
    return *this;
  }
  void Copy(const CMstatsnode &n);

  CMnode node;
  int is_alive;
  gxList<gxString> stats;
};

struct CMconfig {
  CMconfig(); 
  ~CMconfig();

  int QUEUE_NODE_COUNT();
  int QUEUE_DEBUG_COUNT();
  int QUEUE_PROC_COUNT();

  CMstats stats;
  gxList<CMnode *> nodes;
  gxList<CMcrontabs> node_crontabs;
  gxList<CMipaddrs> node_ipaddrs;
  gxList<CMservices> node_services;
  gxList<CMapplications> node_applications;
  gxList<CMfilesystems> node_filesystems;
  int num_resource_crons;
  int num_resource_ipaddrs;
  int num_resource_services;
  int num_resource_applications;
  int num_resource_filesystems;
  int is_client;
  int is_client_interactive;
  int client_ping_count; // Client ping count
  gxString client_ping_nodename; // Ping and rstats node name
  gxString client_command;
  int debug;
  int debug_level;
  int verbose;
  int verbose_level;
  gxString config_file;
  int user_config_file;
  int check_config;
  gxsPort_t udpport;
  gxsPort_t tcpport;
  int dead_node_count;
  gxString my_hostname;
  gxString run_dir, log_dir, spool_dir;
  gxString etc_dir;
  int accept_clients;
  int server_retry;
  int fatal_error;
  gxString service_name;
  gxString client_name;
  gxString ProgramName;
  int echo_loop;
  gxString cluster_user;
  gxString cluster_group;
  gxString cluster_umask;
  gxString sudo_command;

  // Server stop and restart variables
  int kill_server;
  int restart_server;
  int process_loop;
  gxMutex process_lock;
  gxCondition process_cond;
  gxThread_t *process_thread;
  gxThread_t *log_thread;
  gxThread_t *udp_server_thread;
  gxThread_t *keep_alive_thread;
  gxThread_t *tcp_server_thread;
  gxThread_t *crontab_thread;
  gxThread_t *services_thread;
  gxThread_t *applications_thread;
  gxThread_t *ipaddr_thread;
  gxThread_t *filesystems_thread;
  thrPool *client_request_pool;
  gxThread_t *console_thread;

  // -- arg overrides when CFG file loaded
  int has_debug_override;
  int has_debug_level_override;
  int has_verbose_override;
  int has_verbose_level_override;
  int has_config_file_override;
  int has_log_file_name_override;
  int has_log_level_override;
  int has_log_file_clear_override;
  int has_enable_logging_override;

  // Log settings
  LogFile logfile;
  int clear_log;
  int enable_logging;
  int num_logs_to_keep;
  long max_log_size;
  int last_log;
  gxString logfile_name;
  int log_level;
  int log_file_err;
  int bind_to_keep_alive_ip;

  // Log queue
  int log_queue_size;
  int log_queue_debug_size;
  int log_queue_proc_size;
  LogQueueNode *loq_queue_nodes;
  LogQueueNode *loq_queue_debug_nodes;
  LogQueueNode *loq_queue_proc_nodes;

  // Server hash and auth key
  char auth_key[2048];
  char sha1sum[40];
  gxString auth_key_file;
  gxString sha1_file;

private:  // Cannot be accessed directly
  int queue_node_count;
  int queue_debug_count;
  int queue_proc_count;

private: // Internal thread vars
  gxMutex queue_node_count_lock;
  int queue_node_count_is_locked;
  gxMutex queue_debug_count_lock;
  int queue_debug_count_is_locked;
  gxMutex queue_proc_count_lock;
  int queue_proc_count_is_locked;
};

// Config functions
int LoadOrBuildConfigFile();

int ProcessArgs(int argc, char *argv[]);
int ProcessDashDashArg(gxString &arg);

// Non-thread safe check DIR function uses when program starts
// Return 1 if DIR exists or 0 if DIR doesn't exist or can be created
int NT_check_dir(const gxString &dir);

extern CMconfig ServerConfigSruct;
extern CMconfig *servercfg;

#endif // __DRCM_CONFIG_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
