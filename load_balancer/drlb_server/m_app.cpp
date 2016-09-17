// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File
// C++ Compiler Used: GNU, Intel
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

Application classes and functions for Datareel load balancer.

*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "drlb_server.h"
#include "m_app.h"

int check_config()
{
  if(servercfg->check_config) {
    NT_print("Running DRLB configuration check and exiting");
  }

  int error_level = LoadOrBuildConfigFile();
  if(error_level == 2) {
    if(servercfg->check_config) return error_level;
    servercfg->verbose = 1;
    NT_print("ERROR - DRLB config file was not found. A default config file was built.");
    return  error_level;
  }
  if(error_level == 3) {
    servercfg->verbose = 1;
    NT_print("ERROR - Could not build a default DRLB configuration file template");
    return  error_level;
  }
  if(error_level != 0) {
    if(!servercfg->verbose) {
      servercfg->verbose = 1;
      NT_print("ERROR - DRLB config file error. Run with --verbose to print error messages");
    }
    else {
      NT_print("ERROR - DRLB config file error.");
    }
    return  error_level;
  }

  gxString sbuf;
  sbuf << clear << servercfg->port;
  NT_print("LB listening port", sbuf.c_str()); 
  if(servercfg->scheme == LB_DISTRIB) NT_print("Load balancing scheme = LB_DISTRIB");
  if(servercfg->scheme == LB_RR) NT_print("Load balancing scheme = LB_RR");
  if(servercfg->scheme == LB_WEIGHTED) NT_print("Load balancing scheme = LB_WEIGHTED");

  if(servercfg->scheme == LB_ASSIGNED) {
    NT_print("Load balancing scheme = LB_ASSIGNED");
    switch(servercfg->assigned_default) {
      case LB_RR:
	NT_print("INFO - Assigned connections will use LB round robin for nodes with no rules");
	break;
      case LB_DISTRIB:
	NT_print("INFO - Assigned connections will use LB distributed for node with no rules");
	break;
      case LB_WEIGHTED:
	NT_print("INFO - Assigned connections will use LB weighted for node with no rules");
	break;
      case LB_NONE:
	NT_print("INFO - Connections to nodes with assign rules will only connect to assigned node");
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
      sbuf << clear << wn << servercfg->buffer_overflow_size;
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
  sbuf << clear << "Max queued connection requests " << servercfg->somaxconn;
  NT_print(sbuf.c_str());
  sbuf << clear << servercfg->max_idle_count;
  NT_print("Idle count for blocking sockets", sbuf.c_str()); 
  sbuf << clear << servercfg->idle_wait_secs;
  NT_print("Idle secs for blocking sockets", sbuf.c_str()); 
  sbuf << clear << servercfg->idle_wait_usecs;
  NT_print("Idle usecs for blocking sockets", sbuf.c_str()); 
  if(servercfg->use_nonblock_sockets) {
    NT_print("Using non-blocking sockets, idle settings ignored");
  }
  else {
    NT_print("Using blocking sockets with idle settings");
  }
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
    sbuf << clear << servercfg->num_logs_to_keep;
    NT_print("Number of log files to keep", sbuf.c_str()); 
    char lbuf[255];
    memset(lbuf, 0, 255);
    sprintf(lbuf, "%lu" , servercfg->max_log_size);
    sbuf << clear << wn << servercfg->max_log_size;
    NT_print("Max log file size set to", sbuf.c_str()); 
    sbuf << clear << servercfg->log_level;
    NT_print("Logging level set to", sbuf.c_str()); 
    NT_print("Log file", servercfg->logfile_name.c_str()); 
    if(!servercfg->logfile && !servercfg->check_config) {
      servercfg->logfile.Open(servercfg->logfile_name.c_str());
      if(!servercfg->logfile) {
	servercfg->enable_logging = 0;
	servercfg->verbose = 1;
	NT_print("ERROR - Cannot open log file", servercfg->logfile_name.c_str());
	NT_print("Exiting with errors");
	return 1;
      }
    }
  }
  else {
    NT_print("Logging is disabled");
  }

  if(servercfg->enable_stats)  {
    NT_print("Stats collection is enabled");
    sbuf << clear << servercfg->num_stats_to_keep;
    NT_print("Number of stats files to keep", sbuf.c_str()); 
    char lbuf[255];
    memset(lbuf, 0, 255);
    sprintf(lbuf, "%lu" , servercfg->max_stats_size);
    sbuf << clear << wn << servercfg->max_stats_size;
    NT_print("Max stats file size set to", sbuf.c_str()); 
    NT_print("Stats file", servercfg->stats_file_name.c_str()); 
    if(!servercfg->stats_file && !servercfg->check_config) {
      servercfg->stats_file.Open(servercfg->stats_file_name.c_str());
      if(!servercfg->stats_file) {
	servercfg->enable_stats = 0;
	servercfg->verbose = 1;
	NT_print("ERROR - Cannot open stats file", servercfg->stats_file_name.c_str());
	NT_print("Exiting with errors");
	return 1;
      }
    }
    sbuf << clear << "Stats report time is " << servercfg->stats_min << " minute(s) " 
	 << servercfg->stats_secs << " second(s)";
    NT_print(sbuf.c_str());
  }
  else {
    NT_print("Stats collection is disabled");
  }

  if(servercfg->enable_throttling) {
    NT_print("Connection throttling is enabled");
    sbuf << clear <<  "Number of connections to start throttling " << servercfg->throttle_every_connections;
    NT_print(sbuf.c_str());
    sbuf << clear <<  "Seconds to hold connection in the throttle queue " << servercfg->throttle_wait_secs;
    NT_print(sbuf.c_str());
    sbuf << clear <<  "Microseconds to hold connection in the throttle queue " << servercfg->throttle_wait_usecs;
    NT_print(sbuf.c_str());
    if(servercfg->throttle_apply_by_load) {
      NT_print("Peak load throttling is enabled");
      sbuf << clear << "Number of connection per second to start throttling " 
	   << servercfg->throttle_connections_per_sec;
      NT_print(sbuf.c_str());
      if(!servercfg->enable_stats) {
	NT_print("NOTICE - DRLB server config has stats collection is disabled");
 	NT_print("NOTICE - Stats will be enabled without logging for load throttling stat collection");
	servercfg->stats_file_name.Clear();
	servercfg->enable_stats = 1;
	sbuf << clear << "Stats collection time is " << servercfg->stats_min << " minute(s) " 
	     << servercfg->stats_secs << " second(s)";
	NT_print(sbuf.c_str());
	NT_print("To set faster stat collection times set the stats_min and/or stats_secs parms");
      }
    }
    else {
      NT_print("Peak load throttling is disabled");
    }
  }
  else {
    NT_print("Connection throttling is disabled");
  }

  if(servercfg->enable_http_forwarding) {
    NT_print("HTTP IP address forwarding is enabled");
    gxString delimiter = ",";
    servercfg->http_request_strings.FilterChar(' ');
    servercfg->http_request_strings.TrimLeading(',');
    servercfg->http_request_strings.TrimTrailing(',');
    while(servercfg->http_request_strings.Find(",,") != -1) {
      servercfg->http_request_strings.ReplaceString(",,", ",");      
    }
    if(servercfg->http_request_strings.is_null()) {
      NT_print("ERROR - Bad HTTP request strings", servercfg->http_request_strings.c_str());
      return 1;
    }
    if(servercfg->http_request_strings.Find(",") == -1) {
      servercfg->http_request_strings << ",";
    }
    servercfg->http_requests = ParseStrings(servercfg->http_request_strings, 
					    delimiter, servercfg->num_http_requests);
    if(servercfg->num_http_requests == 0) {
      NT_print("ERROR - Bad HTTP request strings", servercfg->http_request_strings.c_str());
      return 1;
    }
    NT_print("HTTP request string =", servercfg->http_request_strings.c_str());
    NT_print("HTTP header string =", servercfg->http_hdr_str.c_str());
    NT_print("HTTP forward string =", servercfg->http_forward_for_str.c_str());
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
	  NT_print("ERROR - Node configuration error. Using LB_ASSIGNED with distrib failover and no node load percent value");
	  node_cfg_error_level = 1;
	}

	if(servercfg->assigned_default == LB_WEIGHTED) { 
	  NT_print("ERROR - Node configuration error. Using LB_ASSIGNED with weighted failover and no node weight value");
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

  servercfg->loq_queue_nodes = new LogQueueNode[servercfg->log_queue_size];
  servercfg->loq_queue_debug_nodes = new LogQueueNode[servercfg->log_queue_debug_size];
  servercfg->loq_queue_proc_nodes = new LogQueueNode[servercfg->log_queue_proc_size];

  if(servercfg->scheme == LB_ASSIGNED) {
    NT_print("Checking rules configuration file");
    if(servercfg->rules_config_file.is_null()) {
      NT_print("ERROR - No rules_config_file specified in LB server config");
      return 1;
    }
    switch(servercfg->NT_ReadRulesConfig()) {
      case 0:
	NT_print("INFO - Assignment rules config file checks");
	NT_print("INFO - Rules config will only be read once. Changes will require a server restart.");
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

  return error_level;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
