// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File
// C++ Compiler Used: GNU, Intel
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

Application classes and functions for Datareel cluster manager.

*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"
#include "drcm_server.h"
#include "m_app.h"

void PrintGlobalConfig()
{
  gxString sbuf;

  NT_print("DR Cluster Manager Global Config"); 
  sbuf << clear << servercfg->udpport;
  NT_print("UDP port =", sbuf.c_str());
  sbuf << clear << servercfg->tcpport;
  NT_print("TCP port =", sbuf.c_str());
  sbuf << clear << servercfg->dead_node_count;
  NT_print("Dead node count =", sbuf.c_str());
  NT_print("run dir =", servercfg->run_dir.c_str());
  NT_print("log dir =", servercfg->log_dir.c_str());
  NT_print("spool dir = ", servercfg->spool_dir.c_str());
  NT_print("CM user =", servercfg->cluster_user.c_str());
  NT_print("CM group =", servercfg->cluster_group.c_str());
  NT_print("SUDO command =", servercfg->sudo_command.c_str());
  sbuf << clear << servercfg->log_queue_size;
  NT_print("Max number of log or console messages to queue", sbuf.c_str()); 
  sbuf << clear << servercfg->log_queue_debug_size;
  NT_print("Max number of debug messages to queue", sbuf.c_str()); 
  sbuf << clear << servercfg->log_queue_proc_size;
  NT_print("Max number of process messages to queue", sbuf.c_str()); 
  if(servercfg->enable_logging)  {
    NT_print("Logging is enabled");
    if(servercfg->clear_log) {
      NT_print("Log file will cleared when LB server is restarted");
    } 
    NT_print("Log file", servercfg->logfile_name.c_str()); 
    sbuf << clear << servercfg->num_logs_to_keep;
    NT_print("Number of log files to keep", sbuf.c_str()); 
    char lbuf[255];
    memset(lbuf, 0, 255);
    sprintf(lbuf, "%lu" , servercfg->max_log_size);
    sbuf << clear << wn << servercfg->max_log_size;
    NT_print("Max log file size set to", sbuf.c_str()); 
    sbuf << clear << servercfg->log_level;
    NT_print("Logging level set to", sbuf.c_str()); 
  }
  else {
    NT_print("Logging is disabled");
  }
  if(servercfg->debug)  {
    NT_print("Debug is enabled");
    sbuf << clear << servercfg->debug_level;
    NT_print("Debug level set to", sbuf.c_str()); 
    sbuf << clear << servercfg->verbose_level;
    NT_print("Verbose mode level set to", sbuf.c_str()); 
  }
  else {
    NT_print("Debug is disabled");
  }
  if(servercfg->verbose)  {
    NT_print("Verbose mode is enabled");
  }
  else {
    NT_print("Verbose mode is disabled");
  }

  sbuf.Clear();
  gxListNode <CMcrontabs> *cptr = servercfg->node_crontabs.GetHead();
  while(cptr) {
    sbuf << cptr->data.nickname;
    if(cptr->next) sbuf << ",";
    cptr = cptr->next;
  }
  NT_print("CM_CRONTABS:", sbuf.c_str());

  sbuf.Clear();
  gxListNode <CMipaddrs> *iptr = servercfg->node_ipaddrs.GetHead();
  while(iptr) {
    sbuf << iptr->data.nickname;
    if(iptr->next) sbuf << ",";
    iptr = iptr->next;
  }
  NT_print("CM_IPADDRS:", sbuf.c_str());

  sbuf.Clear();
  gxListNode <CMservices> *sptr = servercfg->node_services.GetHead();
  while(sptr) {
    sbuf << sptr->data.nickname;
    if(sptr->next) sbuf << ",";
    sptr = sptr->next;
  }
  NT_print("CM_SERVICES:", sbuf.c_str());

  sbuf.Clear();
  gxListNode <CMapplications> *aptr = servercfg->node_applications.GetHead();
  while(aptr) {
    sbuf << aptr->data.nickname;
    if(aptr->next) sbuf << ",";
    aptr = aptr->next;
  }
  NT_print("CM_APPLICATIONS:", sbuf.c_str());

  sbuf.Clear();
  gxListNode <CMfilesystems> *fptr = servercfg->node_filesystems.GetHead();
  while(fptr) {
    sbuf << fptr->data.nickname;
    if(fptr->next) sbuf << ",";
    fptr = fptr->next;
  }
  NT_print("CM_FILESYSTEMS:", sbuf.c_str());
}

void PrintNodeConfig()
{
  gxString sbuf;
  gxListNode<gxString> *node_listptr;

  NT_print("CM node config"); 
  gxListNode<CMnode *> *ptr = servercfg->nodes.GetHead();
  int node_cfg_error_level = 0;
  while(ptr) {
    NT_print("nodename =", ptr->data->nodename.c_str());
    NT_print("hostname =", ptr->data->hostname.c_str());

    if(ptr->data->node_is_active) {
      NT_print("This node is active");
    }
    else {
      NT_print("This not is not active");
    }
    if(ptr->data->keep_services) {
      NT_print("Will keep node_services enabled if CM is offline");
    }
    else {
      NT_print("Will stop node_services if CM is offline");
    }
    if(ptr->data->keep_filesystems) {
      NT_print("Node will keep node_filesystems mounted if CM is offline");
    }
    else {
      NT_print("Node will unmount node_filesystems if CM is offline");
    }
    if(ptr->data->keep_applications) {
      NT_print("Will keep node_applications enabled if CM is offline");
    }
    else {
      NT_print("Will stop node_applications if CM is offline");
    }
    NT_print("keep_alive_ip =", ptr->data->keep_alive_ip.c_str());
    sbuf << clear << "crontabs_check = " << ptr->data->crontabs_check << " seconds";
    NT_print(sbuf.c_str()); 
    sbuf << clear << "services_check = " << ptr->data->services_check << " seconds";
    NT_print(sbuf.c_str()); 
    sbuf << clear << "applications_check = " << ptr->data->applications_check << " seconds";
    NT_print(sbuf.c_str()); 
    sbuf << clear << "ipaddrs_check = " << ptr->data->ipaddrs_check << " seconds";
    NT_print(sbuf.c_str()); 
    sbuf << clear << "filesystems_check = " << ptr->data->filesystems_check << " seconds";
    NT_print(sbuf.c_str()); 

    sbuf.Clear();
    node_listptr = ptr->data->crontabs.GetHead();
    while(node_listptr) {
      sbuf << node_listptr->data;
      if(node_listptr->next) sbuf << ", ";
      node_listptr = node_listptr->next;
    }
    NT_print("node_crontabs =", sbuf.c_str());

    sbuf.Clear();
    node_listptr = ptr->data->backup_crontabs.GetHead();
    while(node_listptr) {
      sbuf << node_listptr->data;
      if(node_listptr->next) sbuf << ", ";
      node_listptr = node_listptr->next;
    }
    NT_print("node_backup_crontabs =", sbuf.c_str());

    sbuf.Clear();
    node_listptr = ptr->data->floating_ip_addrs.GetHead();
    while(node_listptr) {
      sbuf << node_listptr->data;
      if(node_listptr->next) sbuf << ", ";
      node_listptr = node_listptr->next;
    }
    NT_print("node_floating_ip_addrs =", sbuf.c_str());

    sbuf.Clear();
    node_listptr = ptr->data->backup_floating_ip_addrs.GetHead();
    while(node_listptr) {
      sbuf << node_listptr->data;
      if(node_listptr->next) sbuf << ", ";
      node_listptr = node_listptr->next;
    }
    NT_print("node_backup_floating_ip_addrs =", sbuf.c_str());

    sbuf.Clear();
    node_listptr = ptr->data->services.GetHead();
    while(node_listptr) {
      sbuf << node_listptr->data;
      if(node_listptr->next) sbuf << ", ";
      node_listptr = node_listptr->next;
    }
    NT_print("node_services =", sbuf.c_str());

    sbuf.Clear();
    node_listptr = ptr->data->backup_services.GetHead();
    while(node_listptr) {
      sbuf << node_listptr->data;
      if(node_listptr->next) sbuf << ", ";
      node_listptr = node_listptr->next;
    }
    NT_print("node_backup_services =", sbuf.c_str());

    sbuf.Clear();
    node_listptr = ptr->data->applications.GetHead();
    while(node_listptr) {
      sbuf << node_listptr->data;
      if(node_listptr->next) sbuf << ", ";
      node_listptr = node_listptr->next;
    }
    NT_print("node_applications =", sbuf.c_str());

    sbuf.Clear();
    node_listptr = ptr->data->backup_applications.GetHead();
    while(node_listptr) {
      sbuf << node_listptr->data;
      if(node_listptr->next) sbuf << ", ";
      node_listptr = node_listptr->next;
    }
    NT_print("node_backup_applications =", sbuf.c_str());

    sbuf.Clear();
    node_listptr = ptr->data->filesystems.GetHead();
    while(node_listptr) {
      sbuf << node_listptr->data;
      if(node_listptr->next) sbuf << ", ";
      node_listptr = node_listptr->next;
    }
    NT_print("node_filesystems =", sbuf.c_str());

    sbuf.Clear();
    node_listptr = ptr->data->backup_filesystems.GetHead();
    while(node_listptr) {
      sbuf << node_listptr->data;
      if(node_listptr->next) sbuf << ", ";
      node_listptr = node_listptr->next;
    }
    NT_print("node_backup_filesystems =", sbuf.c_str());

    ptr = ptr->next;
  }
}

int check_config()
{
  servercfg->loq_queue_nodes = new LogQueueNode[servercfg->log_queue_size];
  servercfg->loq_queue_debug_nodes = new LogQueueNode[servercfg->log_queue_debug_size];
  servercfg->loq_queue_proc_nodes = new LogQueueNode[servercfg->log_queue_proc_size];

  int error_level = 0;
  int warning_level = 0;
  int app_info_level = 0;
  servercfg->verbose = 1;
  servercfg->debug = 1;
  servercfg->debug_level = 5;
  servercfg->verbose_level = 5;

  // Keep logging disabled so config check does not get logged
  servercfg->enable_logging = 0;
  servercfg->has_enable_logging_override = 1;

  // Do not reset when CFG file is read
  if(!servercfg->has_debug_override) servercfg->has_debug_override = 1;
  if(!servercfg->has_debug_level_override) servercfg->has_debug_level_override = 1;
  if(!servercfg->has_verbose_override) servercfg->has_verbose_override = 1;
  if(!servercfg->has_verbose_level_override) servercfg->has_verbose_level_override = 1;

  gxListNode<gxString> *node_listptr;
  gxListNode<CMnode *> *ptr;

  error_level = LoadOrBuildConfigFile();
  if(error_level == 2) {
     NT_print("CM configuration file not found. Building default file and exiting");
    return error_level;
  }
  if(error_level != 0) {
    error_level = 1;
  }

  if(load_hash_key() != 0) error_level = 1;

  PrintGlobalConfig();
  PrintNodeConfig();

  gxString message, sbuf;
  gxString user = "root";
  if(servercfg->cluster_user == user.c_str()) {
    if(geteuid() != 0) {
      servercfg->verbose = 1;
      NT_print("WARNING - CM config file set to run CM server as root");
      NT_print("WARNING - You will need to start the CM server as root");
      warning_level =1;
    }
  }
  else {
    user.Clear();
    char *e = getenv("USER");
    if(!e) {
      servercfg->verbose = 1;
      NT_print("ERROR - ${USER} var not set in your ENV");
      error_level = 1;
    } 
    else {
      user = getenv("USER");
      if(servercfg->cluster_user != user.c_str()) {
	servercfg->verbose = 1;
	message << clear << "WARNING - Username checking CM config is " << user;
	NT_print(message.c_str());
	message << clear << "WARNING - CM config file username is set to " << servercfg->cluster_user; 
	NT_print(message.c_str());
	warning_level = 1;
      }
    }
  }
  
  if(!futils_exists(servercfg->etc_dir.c_str())) {
    servercfg->verbose = 1;
    NT_print("ERROR - Missing or cannot read DIR ", servercfg->etc_dir.c_str());
    error_level = 1;
  }
  if(!futils_exists(servercfg->log_dir.c_str())) {
    servercfg->verbose = 1;
    NT_print("ERROR - Missing or cannot read DIR ", servercfg->log_dir.c_str());
    error_level = 1;
  }
  if(!futils_exists(servercfg->spool_dir.c_str())) {
    servercfg->verbose = 1;
    NT_print("ERROR - Missing or cannot read DIR ", servercfg->spool_dir.c_str());
    error_level = 1;
  }
  if(!futils_exists(servercfg->run_dir.c_str())) {
    servercfg->verbose = 1;
    NT_print("ERROR - Missing or cannot read DIR ", servercfg->run_dir.c_str());
    error_level = 1;
  }
  
  char hbuf[gxsMAX_NAME_LEN];
  memset(hbuf, 0 , gxsMAX_NAME_LEN);
  if(gethostname(hbuf, gxsMAX_NAME_LEN) < 0) {
    servercfg->verbose = 1;
    NT_print("ERROR - Could not get hostname for this server");
    error_level = 1;
  }
  else {
    NT_print("Hostname for this CM node =", hbuf);
    servercfg->my_hostname = hbuf;
    int has_hostname = 0;

    ptr = servercfg->nodes.GetHead();
    int node_cfg_error_level = 0;
    while(ptr) {
      if(ptr->data->hostname == servercfg->my_hostname) has_hostname = 1;
      ptr = ptr->next;
    }
    
    if(!has_hostname) {
      servercfg->verbose = 1;
      NT_print("ERROR - The hostname of this server does not match any node hostname in config file");
      error_level = 1;
    }
  }

  gxListNode <CMcrontabs> *cptr = servercfg->node_crontabs.GetHead();
  while(cptr) {
    if(!futils_exists(cptr->data.template_file.c_str())) {
      NT_print("ERROR - Missing or cannot read crontab template:", cptr->data.template_file.c_str());
      error_level = 1;
    }
    if(!futils_exists(cptr->data.install_location.c_str())) {
      NT_print("ERROR - Missing or cannot read crontab DIR:", cptr->data.install_location.c_str());
      error_level = 1;
    }
    if(!futils_exists(cptr->data.resource_script.c_str())) {
      NT_print("ERROR - Missing or cannot read crontab resouce:", cptr->data.resource_script.c_str());
      error_level = 1;
    }
    cptr = cptr->next;
  }

  gxListNode <CMipaddrs> *iptr = servercfg->node_ipaddrs.GetHead();
  while(iptr) {
    if(!futils_exists(iptr->data.resource_script.c_str())) {
      NT_print("ERROR - Missing or cannot read IP address resouce:", iptr->data.resource_script.c_str());
      error_level = 1;
    }
    iptr = iptr->next;
  }

  gxListNode <CMservices> *sptr = servercfg->node_services.GetHead();
  while(sptr) {
    if(!futils_exists(sptr->data.resource_script.c_str())) {
      NT_print("ERROR - Missing or cannot read service resouce:", sptr->data.resource_script.c_str());
      error_level = 1;
    }
    sptr = sptr->next;
  }

  gxListNode <CMapplications> *aptr = servercfg->node_applications.GetHead();
  int num_apps = 0;
  while(aptr) {
    num_apps++;
    message << clear << "INFO - " << aptr->data.nickname << " applciation user: " << aptr->data.user << " group:" << aptr->data.group;
    NT_print(message.c_str());
    message << clear << "INFO - " << aptr->data.nickname << " applciation has start program: " << aptr->data.start_program;
    NT_print(message.c_str());
    message << clear << "INFO - " << aptr->data.nickname << " applciation has stop program: " << aptr->data.stop_program;
    NT_print(message.c_str());
    message << clear << "INFO - " << aptr->data.nickname << " applciation has ensure script: " << aptr->data.ensure_script;
    NT_print(message.c_str());
    aptr = aptr->next;
  }
  if(num_apps > 0) {
    message << clear << "INFO - " << num_apps << " application(s) installed"; 
    NT_print(message.c_str());
    app_info_level = 1;
  }
  else {
    NT_print("INFO - No applications installed");
  }

  gxListNode <CMfilesystems> *fptr = servercfg->node_filesystems.GetHead();
  while(fptr) {
    if(!futils_exists(fptr->data.resource_script.c_str())) {
      NT_print("ERROR - Missing or cannot read filesystem resouce:", fptr->data.resource_script.c_str());
      error_level = 1;
    }
    fptr = fptr->next;
  }

  ptr = servercfg->nodes.GetHead();
  int has_value = 0;
  int num_node_errors = 0;
  while(ptr) {
    num_node_errors = 0;
    node_listptr = ptr->data->crontabs.GetHead();
    while(node_listptr) {
      has_value = 0;
      cptr = servercfg->node_crontabs.GetHead();
      while(cptr) {
	if(node_listptr->data == cptr->data.nickname) {
	  has_value = 1;
	  break;
	}
	cptr = cptr->next;
      }
      if(!has_value) num_node_errors++;
      node_listptr = node_listptr->next;
    }
    if(num_node_errors != 0) {
      message << clear << "ERROR - " << ptr->data->nodename << " node has crontab not listed in global resource";
      NT_print(message.c_str());
      error_level = 1;
    }
    ptr = ptr->next;
  }

  ptr = servercfg->nodes.GetHead();
  has_value = 0;
  num_node_errors = 0;
  int backup_missing_nodename = 0;
  while(ptr) {
    num_node_errors = 0;
    node_listptr = ptr->data->backup_crontabs.GetHead();
    while(node_listptr) {
      has_value = 0;
      cptr = servercfg->node_crontabs.GetHead();
      sbuf << clear << node_listptr->data;
      if(sbuf.Find(":") == -1) backup_missing_nodename++;
      sbuf.DeleteBeforeLastIncluding(":");
      while(cptr) {
	if(sbuf == cptr->data.nickname) {
	  has_value = 1;
	  break;
	}
	cptr = cptr->next;
      }
      if(!has_value) num_node_errors++;
      node_listptr = node_listptr->next;
    }
    if(num_node_errors != 0) {
      message << clear << "ERROR - " << ptr->data->nodename << " node has backup crontab not listed in global resource";
      NT_print(message.c_str());
      error_level = 1;
    }
    if(backup_missing_nodename != 0) {
      message << clear << "ERROR - " << ptr->data->nodename << " backup crontab with no backup nodename, bad nodename:nickname format";
      NT_print(message.c_str());
      error_level = 1;
    }
    ptr = ptr->next;
  }

  ptr = servercfg->nodes.GetHead();
  has_value = 0;
  num_node_errors = 0;
  while(ptr) {
    num_node_errors = 0;
    node_listptr = ptr->data->floating_ip_addrs.GetHead();
    while(node_listptr) {
      has_value = 0;
      iptr = servercfg->node_ipaddrs.GetHead();
      while(iptr) {
	if(node_listptr->data == iptr->data.nickname) {
	  has_value = 1;
	  break;
	}
	iptr = iptr->next;
      }
      if(!has_value) num_node_errors++;
      node_listptr = node_listptr->next;
    }
    if(num_node_errors != 0) {
      message << clear << "ERROR - " << ptr->data->nodename << " node has floating IP address not listed in global resource";
      NT_print(message.c_str());
      error_level = 1;
    }
    ptr = ptr->next;
  }

  ptr = servercfg->nodes.GetHead();
  has_value = 0;
  num_node_errors = 0;
  backup_missing_nodename = 0;
  while(ptr) {
    num_node_errors = 0;
    node_listptr = ptr->data->backup_floating_ip_addrs.GetHead();
    while(node_listptr) {
      has_value = 0;
      iptr = servercfg->node_ipaddrs.GetHead();
      sbuf << clear << node_listptr->data;  
      if(sbuf.Find(":") == -1) backup_missing_nodename++;
      sbuf.DeleteBeforeLastIncluding(":");
      while(iptr) {
	if(sbuf == iptr->data.nickname) {
	  has_value = 1;
	  break;
	}
	iptr = iptr->next;
      }
      if(!has_value) num_node_errors++;
      node_listptr = node_listptr->next;
    }
    if(num_node_errors != 0) {
      message << clear << "ERROR - " << ptr->data->nodename << " node has backup floating IP address not listed in global resource";
      NT_print(message.c_str());
      error_level = 1;
    }
    if(backup_missing_nodename != 0) {
      message << clear << "ERROR - " << ptr->data->nodename << " backup floating IP with no backup nodename, bad nodename:nickname format";
      NT_print(message.c_str());
      error_level = 1;
    }
    ptr = ptr->next;
  }

  ptr = servercfg->nodes.GetHead();
  has_value = 0;
  num_node_errors = 0;
  while(ptr) {
    num_node_errors = 0;
    node_listptr = ptr->data->services.GetHead();
    while(node_listptr) {
      has_value = 0;
      sptr = servercfg->node_services.GetHead();
      while(sptr) {
	if(node_listptr->data == sptr->data.nickname) {
	  has_value = 1;
	  break;
	}
	sptr = sptr->next;
      }
      if(!has_value) num_node_errors++;
      node_listptr = node_listptr->next;
    }
    if(num_node_errors != 0) {
      message << clear << "ERROR - " << ptr->data->nodename << " node has service not listed in global resource";
      NT_print(message.c_str());
      error_level = 1;
    }
    ptr = ptr->next;
  }


  ptr = servercfg->nodes.GetHead();
  has_value = 0;
  num_node_errors = 0;
  backup_missing_nodename = 0;
  while(ptr) {
    num_node_errors = 0;
    node_listptr = ptr->data->backup_services.GetHead();
    while(node_listptr) {
      has_value = 0;
      sptr = servercfg->node_services.GetHead();
      sbuf << clear << node_listptr->data;  
      if(sbuf.Find(":") == -1) backup_missing_nodename++;
      sbuf.DeleteBeforeLastIncluding(":");
      while(sptr) {
	if(sbuf == sptr->data.nickname) {
	  has_value = 1;
	  break;
	}
	sptr = sptr->next;
      }
      if(!has_value) num_node_errors++;
      node_listptr = node_listptr->next;
    }
    if(num_node_errors != 0) {
      message << clear << "ERROR - " << ptr->data->nodename << " node has backup service not listed in global resource";
      NT_print(message.c_str());
      error_level = 1;
    }
    if(backup_missing_nodename != 0) {
      message << clear << "ERROR - " << ptr->data->nodename << " backup service with no backup nodename, bad nodename:nickname format";
      NT_print(message.c_str());
      error_level = 1;
    }
    ptr = ptr->next;
  }

  ptr = servercfg->nodes.GetHead();
  has_value = 0;
  num_node_errors = 0;
  while(ptr) {
    num_node_errors = 0;
    node_listptr = ptr->data->applications.GetHead();
    while(node_listptr) {
      has_value = 0;
      aptr = servercfg->node_applications.GetHead();
      while(aptr) {
	if(node_listptr->data == aptr->data.nickname) {
	  has_value = 1;
	  break;
	}
	aptr = aptr->next;
      }
      if(!has_value) num_node_errors++;
      node_listptr = node_listptr->next;
    }
    if(num_node_errors != 0) {
      message << clear << "ERROR - " << ptr->data->nodename << " node has application not listed in global resource";
      NT_print(message.c_str());
      error_level = 1;
    }
    ptr = ptr->next;
  }

  ptr = servercfg->nodes.GetHead();
  has_value = 0;
  num_node_errors = 0;
  backup_missing_nodename = 0;
  while(ptr) {
    num_node_errors = 0;
    node_listptr = ptr->data->backup_applications.GetHead();
    while(node_listptr) {
      has_value = 0;
      aptr = servercfg->node_applications.GetHead();
      sbuf << clear << node_listptr->data;  
      if(sbuf.Find(":") == -1) backup_missing_nodename++;
      sbuf.DeleteBeforeLastIncluding(":");
      while(aptr) {
	if(sbuf == aptr->data.nickname) {
	  has_value = 1;
	  break;
	}
	aptr = aptr->next;
      }
      if(!has_value) num_node_errors++;
      node_listptr = node_listptr->next;
    }
    if(num_node_errors != 0) {
      message << clear << "ERROR - " << ptr->data->nodename << " node has backup application not listed in global resource";
      NT_print(message.c_str());
      error_level = 1;
    }
    if(backup_missing_nodename != 0) {
      message << clear << "ERROR - " << ptr->data->nodename << " backup application with no backup nodename, bad nodename:nickname format";
      NT_print(message.c_str());
      error_level = 1;
    }
    ptr = ptr->next;
  }

  ptr = servercfg->nodes.GetHead();
  has_value = 0;
  num_node_errors = 0;
  while(ptr) {
    num_node_errors = 0;
    node_listptr = ptr->data->filesystems.GetHead();
    while(node_listptr) {
      has_value = 0;
      fptr = servercfg->node_filesystems.GetHead();
      while(fptr) {
	if(node_listptr->data == fptr->data.nickname) {
	  has_value = 1;
	  break;
	}
	fptr = fptr->next;
      }
      if(!has_value) num_node_errors++;
      node_listptr = node_listptr->next;
    }
    if(num_node_errors != 0) {
      message << clear << "ERROR - " << ptr->data->nodename << " node has file system not listed in global resource";
      NT_print(message.c_str());
      error_level = 1;
    }
    ptr = ptr->next;
  }

  ptr = servercfg->nodes.GetHead();
  has_value = 0;
  num_node_errors = 0;
  backup_missing_nodename = 0;
  while(ptr) {
    num_node_errors = 0;
    node_listptr = ptr->data->backup_filesystems.GetHead();
    while(node_listptr) {
      has_value = 0;
      fptr = servercfg->node_filesystems.GetHead();
      sbuf << clear << node_listptr->data;  
      if(sbuf.Find(":") == -1) backup_missing_nodename++;
      sbuf.DeleteBeforeLastIncluding(":");
      while(fptr) {
	if(sbuf == fptr->data.nickname) {
	  has_value = 1;
	  break;
	}
	fptr = fptr->next;
      }
      if(!has_value) num_node_errors++;
      node_listptr = node_listptr->next;
    }
    if(num_node_errors != 0) {
      message << clear << "ERROR - " << ptr->data->nodename << " node has backup file system not listed in global resource";
      NT_print(message.c_str());
      error_level = 1;
    }
    if(backup_missing_nodename != 0) {
      message << clear << "ERROR - " << ptr->data->nodename << " backup file system with no backup nodename, bad nodename:nickname format";
      NT_print(message.c_str());
      error_level = 1;
    }
    ptr = ptr->next;
  }

  if(warning_level != 0) {
    NT_print("CM config file has WARNINGS");
  }
  if(error_level != 0) {
    NT_print("CM config file has ERRORS");
  }
  if(app_info_level != 0) {
    NT_print("INFO - Check your application programs before starting CM server");
  }

  if(error_level == 0 && warning_level == 0)  {
    NT_print("INFO - CM config file checks good");
  }

  if(error_level != 0) return 1;
  if(warning_level != 0) return 2;

  return 0;
}

int load_hash_key()
{
  // Load the auth key and hash
  DiskFileB akfile, shafile;
  gxString sbuf;
  akfile.df_Open(servercfg->auth_key_file.c_str());
  if(!akfile) {
    NT_print("ERROR - Cannot open or read auth key file", servercfg->auth_key_file.c_str());
    return 1;
  }
  if(akfile.df_Read(servercfg->auth_key, 2048) != DiskFileB::df_NO_ERROR) {
    NT_print("ERROR - Cannot read auth key ", servercfg->auth_key_file.c_str());
    return 1;
  }
  shafile.df_Open(servercfg->sha1_file.c_str());
  if(!shafile) {
    NT_print("ERROR - Cannot open or read auth hash", servercfg->sha1_file.c_str());
    NT_print(shafile.df_ExceptionMessage());
    return 1;
  }
  if(shafile.df_Read(servercfg->sha1sum, 40) != DiskFileB::df_NO_ERROR) {
    NT_print("ERROR - Cannot read auth hash", servercfg->sha1_file.c_str());
    NT_print(shafile.df_ExceptionMessage());
    return 1;
  }
  akfile.df_Close();
  shafile.df_Close();

  return 0;
}

void ConsoleThread::ThreadExitRoutine(gxThread_t *thread)
{
  cout << "\n" << "Exiting interactive console" << "\n" << flush;
  servercfg->process_loop = 0;
  if(servercfg->process_lock.IsLocked()) {
    servercfg->process_cond.ConditionSignal(); 
    servercfg->process_lock.MutexUnlock();
  }
}

void *ConsoleThread::ThreadEntryRoutine(gxThread_t *thread)
{
  const int cmd_len = 255;
  char sbuf[cmd_len];
  gxString command;
  gxString prompt = "DRCM> ";
  cout << "\n" << "Entering interactive console, enter help for commands or exit to quit" << "\n" << flush;
  int error_level = 0;

  while(servercfg->echo_loop) {
    cout << prompt.c_str() << flush;
    memset(sbuf, 0, 255);
    cin >> sbuf;
    command << clear << sbuf;
    if(command == "quit" || command == "exit") break;
    error_level = run_console_command(command);
    if(error_level < 0) {
      cout << "Invalid command" << "\n" << flush;
      cout << "Enter exit to quit or help for valid command" << "\n" << flush;
    }
  }

  return (void *)0;
}

int console_command_is_valid(const gxString &command)
{
  if(command == "help") return 1;
  if(command == "ping") return 1;
  if(command == "printcfg") return 1;
  if(command == "rstats") return 1;

  return 0;
}

int print_console_help()
{
  cout << "DRCM console commands" << "\n";
  cout << "\n";
  cout << "ping     - Test connectivity to all active cluster nodes" << "\n";
  cout << "printcfg - Pring cluster configuration" << "\n";
  cout << "rstats    - Print raw stats buffer for each cluster node" << "\n";
  cout << "\n";
  cout.flush();
  return 0;
}

int run_console_command(const gxString &command)
{
  int error_level = 0;
  if(!console_command_is_valid(command)) return -1;
  if(command == "help") return print_console_help(); 
  if(command == "printcfg") return print_config(); 
  if(command == "ping") return ping_cluster(); 
  if(command == "rstats") return get_cluster_rstats(); 
  return error_level;
}

int print_config()
{
  PrintGlobalConfig();
  PrintNodeConfig();
  return 0;
}

int ping_cluster()
{
  int error_level = 0;
  cout << "ping DRCM cluster" << "\n" << flush;
  gxListNode<CMnode *> *ptr = servercfg->nodes.GetHead();
  while(ptr) {
    error_level = ping_node(ptr->data); 
    ptr = ptr->next;
  }
  if(error_level == 0) {
    cout << "ping DRCM cluster successful" << "\n" << flush;
  }
  else {
    cout << "ping DRCM cluster failed" << "\n" << flush;
  }
  return error_level;
}

int ping_node(CMnode *node)
{
  gxString suffix;
  suffix << clear << node->nodename << "@" << node->keep_alive_ip;
  gxSocket client(SOCK_DGRAM, servercfg->udpport, node->keep_alive_ip.c_str());
  if(!client) {
    cout << "ping fatal error could not start CM client socket" << "\n" << flush;
    return 1;
  }

  CM_MessageHeader cmhdr;
  int hdr_size = sizeof(CM_MessageHeader);
  
  cmhdr.set_word(cmhdr.checkword, NET_CHECKWORD);
  memmove(cmhdr.sha1sum, servercfg->sha1sum, sizeof(cmhdr.sha1sum));
  cmhdr.set_word(cmhdr.cluster_command, CM_CMD_PING);

  int optVal = 1;
  client.SetSockOpt(SOL_SOCKET, SO_REUSEADDR, (char *)&optVal, sizeof(optVal));

  int bytes_moved = client.SendTo(&cmhdr, sizeof(cmhdr));
  if(bytes_moved != sizeof(cmhdr)) {
    cout << "ping " << suffix.c_str() << " failed with send error" << "\n" << flush;      
    return 1;
  }
  cmhdr.clear();
  int bytes_read = client.RemoteRecvFrom(&cmhdr, sizeof(cmhdr), 5, 0);
  if(bytes_read != sizeof(cmhdr)) {
    cout << "ping " << suffix.c_str() << " failed with recv error" << "\n" << flush;      
    return 1;
  }
  cout << "ping " << suffix.c_str() << " successful" << "\n" << flush;      
  return 0;
}

int get_cluster_rstats()
{
  int error_level = 0;
  gxListNode<CMnode *> *ptr = servercfg->nodes.GetHead();
  while(ptr) {
    error_level = get_rstats(ptr->data); 
    ptr = ptr->next;
  }
  return error_level;
}

int get_rstats(CMnode *node)
{
  gxString suffix;
  suffix << clear << node->nodename << "@" << node->keep_alive_ip;
  gxSocket client(SOCK_DGRAM, servercfg->udpport, node->keep_alive_ip.c_str());
  if(!client) {
    cout << "rstats fatal error could not start CM client socket" << "\n" << flush;
    return 1;
  }

  CM_MessageHeader cmhdr;
  int hdr_size = sizeof(CM_MessageHeader);
  
  cmhdr.set_word(cmhdr.checkword, NET_CHECKWORD);
  memmove(cmhdr.sha1sum, servercfg->sha1sum, sizeof(cmhdr.sha1sum));
  cmhdr.set_word(cmhdr.cluster_command, CM_CMD_GETSTATS);

  int optVal = 1;
  client.SetSockOpt(SOL_SOCKET, SO_REUSEADDR, (char *)&optVal, sizeof(optVal));

  int bytes_moved = client.SendTo(&cmhdr, sizeof(cmhdr));
  if(bytes_moved != sizeof(cmhdr)) {
    cout << "rstats " << suffix.c_str() << " failed with send error" << "\n" << flush;      
    return 1;
  }

  cmhdr.clear();
  int bytes_read = client.RemoteRecvFrom(&cmhdr, sizeof(cmhdr), 5, 0);
  if(bytes_read != sizeof(cmhdr)) {
    cout << "rstats ack " << suffix.c_str() << " failed with recv error" << "\n" << flush;      
    return 1;
  }

  int sizebuf = cmhdr.get_word(cmhdr.datagram_size);
  if(sizebuf <= 0) {
    cout << "rstats ack " << suffix.c_str() << " failed with bad datagram size" << "\n" << flush;      
    return 1;
  }
  char *datagram = new char[sizebuf+1];
  memset(datagram, 0, (sizebuf+1));

  bytes_read = client.RemoteRecvFrom(datagram, sizebuf, 15, 0);
  if(bytes_read != sizebuf) {
    cout << "rstats " << suffix.c_str() << " failed with recv error" << "\n" << flush;      
    return 1;
  }

  cout << "rstats " << suffix.c_str() << " successful" << "\n" << flush;      
  cout << datagram << flush;
  cout << "rstats " << suffix.c_str() << " end" << "\n" << flush;      

  return 0;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
