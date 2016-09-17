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

Stat functions for Datareel load balancer.

*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <time.h>

#include "drlb_server.h"
#include "m_stats.h"

StatsThread::~StatsThread() 
{ 
  if(node_prev_connection_count) {
    delete[] node_prev_connection_count;
    node_prev_connection_count = 0;
  }
}

void *StatsThread::ThreadEntryRoutine(gxThread_t *thread)
{
  if(!servercfg->enable_stats) {
    LogMessage("DRLB stat reporting is disabled");
    return (void *)0;
  }

  LogMessage("DRLB stat reporting is enabled");
  gxString message;
  gxString stats_message, node_message;
  SysTime systime;
  time_t prev_etime = time(0);
  unsigned i;
  gxListNode<LBnode *> *ptr;
  char lbuf[255]; 
  int wait_secs = (servercfg->stats_min * 60) + servercfg->stats_secs;

  if(wait_secs <= 0) {
    LogMessage("Invalied stats report time in server config, defaulting to 5 minute reporting");
    wait_secs = 300;
  }

  unsigned num_nodes = servercfg->NUM_NODES();
  if(! node_prev_connection_count) node_prev_connection_count = new unsigned long[num_nodes];
  for(i = 0; i < num_nodes; i++) node_prev_connection_count[i] = 0; 
  unsigned long prev_total_connections = servercfg->CONNECTION_TOTAL();

  ptr = servercfg->nodes.GetHead();
  i = 0;
  while(ptr) {
    node_prev_connection_count[i] = ptr->data->CONNECTION_TOTAL();
    i++;
    ptr = ptr->next;
  }

  if(servercfg->stats_lock.MutexLock() != 0) {
    LogMessage("Stats thread unable to obtain a mutex lock, stats thread exiting");
    return (void *)0;
  }
  servercfg->stats_cond.ConditionTimedWait(&servercfg->stats_lock, wait_secs, 0);
  unsigned long report_number = 0;

  while(servercfg->accept_clients) { 
    report_number++;
    time_t etime = time(0);

    if(servercfg->debug && servercfg->debug_level >= 5) LogMessage("Generating DRLB stat report number");
    unsigned long total_runtime_connections = servercfg->CONNECTION_TOTAL();
    unsigned long elapsed_secs = etime - prev_etime;
    unsigned long server_connection_avg = 0;
    unsigned long node_connection_avg = 0.0;
    unsigned long node_current_connections = 0;

    node_message.Clear();
    ptr = servercfg->nodes.GetHead();
    i = 0;
    while(ptr) {
      unsigned long node_connections = ptr->data->NUM_CONNECTIONS();
      unsigned long node_total_runtime_connections = ptr->data->CONNECTION_TOTAL();
      unsigned long elapsed_node_connections = node_total_runtime_connections - node_prev_connection_count[i];
      node_current_connections +=  node_connections;
      memset(lbuf, 0, 255);
      sprintf(lbuf, "%lu", node_total_runtime_connections);
      node_message << "Total Runtime Connections to " << ptr->data->node_name << ": " << lbuf << "\n";
      node_connection_avg = elapsed_node_connections / elapsed_secs;

      memset(lbuf, 0, 255);
      sprintf(lbuf, "%lu", node_connection_avg);
      node_message << "Connections per second to " << ptr->data->node_name << ": " << lbuf << "\n";

      memset(lbuf, 0, 255);
      sprintf(lbuf, "%lu", node_connections);
      node_message << "Current Connections to " << ptr->data->node_name << ": " << node_connections;
      if(ptr->next) node_message << "\n";
      
      // Update the connection count for the next cycle
      node_prev_connection_count[i] = node_total_runtime_connections;
      i++;
      ptr = ptr->next;
    }

    unsigned long elapsed_connections = total_runtime_connections - prev_total_connections;
    server_connection_avg = elapsed_connections / elapsed_secs;

    time_t uptime = etime - servercfg->etime_server_start;
    time_t report_interval = etime - prev_etime;
    int days, hours, minutes, seconds;
    days = hours = minutes = seconds = 0;
    int rthours, rtminutes, rtseconds;
    rthours = rtminutes = rtseconds = 0;
    days = uptime / 86400;
    hours = (uptime % 86400)/3600;
    minutes = (uptime % 86400 % 3600) / 60;
    seconds = uptime % 86400 % 3600 % 60;
    rthours = report_interval / 3600;
    rtminutes = (report_interval % 3600)/60;
    rtseconds = report_interval % 3600 % 60;
    stats_message.Clear();
    stats_message << "\n";
    stats_message << "BEGIN " << servercfg->service_name << " stats" << "\n"; 
    stats_message << "Server start time: " << systime.MakeGMTDateTime(servercfg->etime_server_start) << "\n";
    stats_message << "Stats report time: " << systime.MakeGMTDateTime(etime) << "\n";
    stats_message << "Uptime: " << days << " days. " << hours << " hrs. " << minutes << " mins. " << seconds << " secs." << "\n";
    memset(lbuf, 0, 255);
    sprintf(lbuf, "%lu", total_runtime_connections);
    stats_message << "Total runtime connections: "  << lbuf << "\n";
    stats_message << "Report interval: "  << rthours << " hrs. " << rtminutes << " mins. " << rtseconds << " secs." << "\n";
    memset(lbuf, 0, 255);
    sprintf(lbuf, "%lu", server_connection_avg);
    stats_message << "Connections per second: " << lbuf << "\n";
    servercfg->CONNECTIONS_PER_SECOND(server_connection_avg);
    memset(lbuf, 0, 255);
    sprintf(lbuf, "%lu", node_current_connections);
    stats_message << "Current connections: " << lbuf << "\n";
    stats_message << node_message << "\n";
    stats_message << "END " << servercfg->service_name << " stats" << "\n"; 

    if(!servercfg->stats_file_name.is_null()) {
      if(!servercfg->stats_file) servercfg->stats_file.Open(servercfg->stats_file_name.c_str());
      if(servercfg->stats_file) {
	servercfg->stats_file << stats_message.c_str();
	servercfg->stats_file.df_Flush();
	rotate_stats_file();  
      }
      else { // Could not write to stats file
	message << clear << "ERROR - Cannot write to stats file " << servercfg->stats_file_name;
	LogMessage(message.c_str());
      }
    }
    prev_etime = etime;
    prev_total_connections = total_runtime_connections;
    servercfg->stats_cond.ConditionTimedWait(&servercfg->stats_lock, wait_secs, 0);  
  }

  servercfg->stats_lock.MutexUnlock();
  return (void *)0;
}

int StatsThread::rotate_stats_file()
{
  if(!servercfg->enable_stats) return 0;
  gxString message;
  gxString new_name = servercfg->stats_file_name.c_str();
  FAU_t fsize = servercfg->stats_file.df_FileSize(servercfg->stats_file_name.c_str());
  SysTime logtime;
  gxString prefix;

  prefix << clear << logtime.GetSyslogTime() << " " << servercfg->service_name << ": ";
  
  if(!servercfg->stats_file) servercfg->stats_file.Open(servercfg->stats_file_name.c_str());
			    
  if(!servercfg->stats_file) {
    message << clear << "Error writing to stats file " << servercfg->stats_file_name.c_str() << "\n";
    LogMessage(message.c_str());
    return 0;
  }
  if(fsize >= (FAU_t)servercfg->max_stats_size) {
    gxString output_message;
    output_message << clear << prefix << "Stats file exceeded " << servercfg->max_stats_size << " bytes" << "\n";
    servercfg->stats_file << output_message.c_str();
    servercfg->stats_file.df_Flush();
    int next_stats = 1;
    if(servercfg->last_stats == 0) {
      servercfg->last_stats = 1;
    }
    else {
      servercfg->last_stats++;
    }
    if(servercfg->last_stats > servercfg->num_stats_to_keep) {
      servercfg->last_stats = 0;  
      next_stats = 1;
    }
    else {
      next_stats = servercfg->last_stats;
    }
    new_name << "." << next_stats;
    output_message << clear << prefix << "Rotating stats, saving prev stats file " << new_name.c_str() << "\n";
    servercfg->stats_file << output_message.c_str();
    servercfg->stats_file.df_Flush();
    servercfg->stats_file.Close();
    if(servercfg->stats_file.df_rename(servercfg->stats_file_name.c_str(), new_name.c_str()) != DiskFileB::df_NO_ERROR) {
      servercfg->stats_file.Open(servercfg->stats_file_name.c_str());
      if(servercfg->stats_file) {
	output_message << clear << prefix << "ERROR - could not rotate stats, cannot create new stats file " << new_name.c_str() << "\n";
	servercfg->stats_file << output_message.c_str();
	servercfg->stats_file.df_Flush();
      }
      return 0;
    }

    if(servercfg->stats_file.Open(servercfg->stats_file_name.c_str()) == 0) {
      output_message << clear << prefix  << "New stats file started " << "\n";
      servercfg->stats_file << output_message.c_str();
      servercfg->stats_file.df_Flush();
    }
    else {
      message << clear << prefix << "Error writing to stats file " << servercfg->stats_file_name.c_str() << "\n";
      LogMessage(message.c_str());
      return 0; // Did not rotate stats
    }
    return 1; // Stats was rotated
  }

  return 0; // Did not rotate stats
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
