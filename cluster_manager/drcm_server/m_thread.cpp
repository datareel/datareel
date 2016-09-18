// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 07/17/2016
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

Multi-threaded framework Datareel cluster manager.

*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <regex.h>
#include <string.h>
#include <errno.h>

#include "gxstring.h"
#include "gxlist.h"
#include "membuf.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include "m_thread.h"
#include "m_log.h"

int get_fd_thread_error(int &error_number, gxString &sbuf)
{
  error_number = errno;
  return fd_error_to_string(error_number, sbuf);
}

int CM_UDP_ServerThread::InitServer()
// Initialize the server. Returns a non-zero value if any errors ocurr.
{
  if(InitSocketLibrary() == 0) {
    if(InitSocket(SOCK_DGRAM, servercfg->udpport) < 0) return 1;
  }
  else {
    return 1;
  }

  int reuse = 1;
  if(setsockopt(gxsocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0) {
    if(servercfg->debug) {
      LogMessage("ERROR - Socket error setting SO_REUSEADDR option");
    }
  }

#ifdef SO_REUSEPORT
  if (setsockopt(gxsocket, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse, sizeof(reuse)) < 0) {
    if(servercfg->debug) {
      LogMessage("ERROR - Socket error setting SO_REUSEPORT option");
    }
  }
#endif

  if(servercfg->bind_to_keep_alive_ip) {
    gxListNode<CMnode *> *ptr = servercfg->nodes.GetHead();
    while(ptr) {
      if(ptr->data->hostname == servercfg->my_hostname) {
	break;
      }
      ptr = ptr->next;
    }
    if(ptr) inet_aton(ptr->data->keep_alive_ip.c_str(), &sin.sin_addr);
  }

  // Bind the name to the socket
  if(Bind() < 0) {
    LogMessage("ERROR - Fatal bind() error initializing server");
    CheckSocketError((gxSocket *)this);
    Close();
    return 1;
  }

  return 0;
}

void *CM_UDP_ServerThread::ThreadEntryRoutine(gxThread_t *thread)
{
  gxString message, sbuf;
  CM_MessageHeader cmhdr, cmhdr_dup;
  int err_count = 0;
  int recv_err_count = 0;
  int error_number = 0;

  while(servercfg->accept_clients) { 
    if(servercfg->restart_server) break;
    if(servercfg->kill_server) break;

    if(servercfg->debug && servercfg->debug_level >= 5 && servercfg->verbose_level >= 5) {
      LogDebugMessage("CM UDP server accepting clients");
    }

    cmhdr.clear(); 
    memset(&remote_sin, 0, sizeof(remote_sin));

    // Block the until cluster nodes sends a CM message
    int rv = RemoteRecvFrom(&cmhdr, sizeof(cmhdr));
    get_fd_thread_error(error_number, sbuf);

    char client_name[gxsMAX_NAME_LEN]; int r_port = -1;
    GetClientInfo(client_name, r_port);
    int seq_num = r_port;
    if(servercfg->debug && servercfg->debug_level >= 5 && servercfg->verbose_level >= 5) {
      message << clear << "[" << seq_num << "]: UDP client connect: " << client_name;
      LogDebugMessage(message.c_str());
      message << clear << "[" << seq_num << "]: Validating UDP client connection";
      LogDebugMessage(message.c_str());
    }

    if(rv != sizeof(cmhdr)) {
      if(servercfg->debug) {
	message << clear << "[" << seq_num << "]: ERROR - Received invalid UDP message header";
	LogDebugMessage(message.c_str());
	message << clear << "[" << seq_num << "]: revcfrom() Return: " << rv << " ENUM: " << error_number << " Message: " << sbuf;
	LogDebugMessage(message.c_str());
      }
      recv_err_count++;
      if(recv_err_count > 100) {
	message << clear << "[" << seq_num << "]: ERROR - Received invalid UDP message header, error count over 100";
	LogMessage(message.c_str());
      }
      continue;
    }
    recv_err_count = 0;

    // Make sure the client header is in sync
    if(cmhdr.get_word(cmhdr.checkword) != NET_CHECKWORD) {
      message << clear << "[" << seq_num << "]: ERROR - Bad or wrong UDP checkword received from " << client_name;
      LogMessage(message.c_str());
      err_count++;
      continue;
    }

    // Check the SHA1 hash
    if(memcmp(cmhdr.sha1sum, servercfg->sha1sum, sizeof(cmhdr.sha1sum)) != 0) {
      message << clear << "[" << seq_num << "]: ERROR - Bad or wrong auth key hash received from " << client_name;
      LogMessage(message.c_str());
      err_count++;
      continue;
    }

    int cluster_command = cmhdr.get_word(cmhdr.cluster_command);
    gxString stats_buf;

    switch(cluster_command) {
      case CM_CMD_NAK:
	if(servercfg->debug) {
	  message << clear << "[" << seq_num << "]: INFO - Received null command from " << client_name;
	  LogDebugMessage(message.c_str());
	}
	break;

      case CM_CMD_PING:
	if(servercfg->debug && servercfg->debug_level >= 5 && servercfg->verbose_level >= 5) {
	  message << clear << "[" << seq_num << "]: Received ping command from " << client_name;
	  LogDebugMessage(message.c_str());
	}
	cmhdr_dup.copy(cmhdr);
	cmhdr_dup.set_word(cmhdr_dup.cluster_command, CM_CMD_ACK);
	bytes_moved = RemoteRawWriteTo(&cmhdr_dup, sizeof(cmhdr_dup));
	get_fd_thread_error(error_number, sbuf);
	if(bytes_moved !=  sizeof(cmhdr_dup)) {
	  if(servercfg->debug) {
	    message << clear << "[" << seq_num << "]: ERROR - Error sending ping ack to " << client_name;
	    LogDebugMessage(message.c_str());
	    message << clear << "[" << seq_num << "]: send() Return: " << bytes_moved << " ENUM: " << error_number << " Message: " << sbuf;
	    LogDebugMessage(message.c_str());
	  }
	}
	break;
      case CM_CMD_KEEPALIVE:
	if(servercfg->debug && servercfg->debug_level >= 5 && servercfg->verbose_level >= 5) {
	  message << clear << "[" << seq_num << "]: Received keep alive command from " << client_name;
	  LogDebugMessage(message.c_str());
	}
	cmhdr_dup.copy(cmhdr);
	cmhdr_dup.set_word(cmhdr_dup.cluster_command, CM_CMD_ACK);
	bytes_moved = RemoteRawWriteTo(&cmhdr_dup, sizeof(cmhdr_dup));
	get_fd_thread_error(error_number, sbuf);
	if(bytes_moved !=  sizeof(cmhdr_dup)) {
	  if(servercfg->debug) {
	    message << clear << "[" << seq_num << "]: ERROR - Error sending keep alive ack to " << client_name;
	    LogDebugMessage(message.c_str());
	    message << clear << "[" << seq_num << "]: send() Return: " << bytes_moved << " ENUM: " << error_number << " Message: " << sbuf;
	    LogDebugMessage(message.c_str());
	  }
	}
	break;
      case CM_CMD_GETSTATS:
	servercfg->stats.Get(stats_buf);
	if(servercfg->debug && servercfg->debug_level >= 5 && servercfg->verbose_level >= 5) {
	  message << clear << "[" << seq_num << "]: Received get stats command from " << client_name;
	  LogDebugMessage(message.c_str());
	}

	cmhdr_dup.copy(cmhdr);
	cmhdr_dup.set_word(cmhdr_dup.cluster_command, CM_CMD_ACK);
	cmhdr_dup.set_word(cmhdr_dup.datagram_size, stats_buf.length());
	bytes_moved = RemoteRawWriteTo(&cmhdr_dup, sizeof(cmhdr_dup));
	get_fd_thread_error(error_number, sbuf);
	if(bytes_moved !=  sizeof(cmhdr_dup)) {
	  if(servercfg->debug) {
	    message << clear << "[" << seq_num << "]: ERROR - Error sending stats ack to " << client_name;
	    LogDebugMessage(message.c_str());
	    message << clear << "[" << seq_num << "]: send() Return: " << bytes_moved << " ENUM: " << error_number << " Message: " << sbuf;
	    LogDebugMessage(message.c_str());
	  }
	}
	bytes_moved = RemoteRawWriteTo(stats_buf.c_str(), stats_buf.length());
	get_fd_thread_error(error_number, sbuf);
	if((unsigned)bytes_moved !=  stats_buf.length()) {
	  if(servercfg->debug) {
	    message << clear << "[" << seq_num << "]: ERROR - Error sending stats to " << client_name;
	    LogDebugMessage(message.c_str());
	    message << clear << "[" << seq_num << "]: send() Return: " << bytes_moved << " ENUM: " << error_number << " Message: " << sbuf;
	    LogDebugMessage(message.c_str());
	  }
	}
	break;

      default:
	if(servercfg->debug) {
	  message << clear << "[" << seq_num << "]: WARNING - Received bad or unknown command from " << client_name;
	  LogDebugMessage(message.c_str());
	}
	break;
    }
    continue;
  }

  if(servercfg->debug && servercfg->debug_level >= 5) {
    LogDebugMessage("CM UDP server exiting thread entry routine");
  }

  Close();
  return (void *)0;
}

void CM_UDP_ServerThread::ThreadExitRoutine(gxThread_t *thread)
// Thread exit function used to close the server thread.
{
  if(servercfg->debug && servercfg->debug_level >= 5) {
    LogDebugMessage("CM UDP server thread exit routine called");
  }

}

void CM_UDP_ServerThread::ThreadCleanupHandler(gxThread_t *thread)
// Thread cleanup handler used in the event that the thread is
// canceled.
{
  if(servercfg->debug && servercfg->debug_level >= 5) {
    LogDebugMessage("CM UDP server thread cleanup handler called");
  }
  Close();
}

void *CM_KeepAliveThread::ThreadEntryRoutine(gxThread_t *thread)
{
  gxString message;
  const int max_message_count = 10;
  const int max_failed_message_count = 10;
  const int num_inactive_node_count = 100;
  int message_count = 0;
  int keep_alive_error_level = 0;

  while(servercfg->accept_clients) { 
    if(servercfg->restart_server) break;
    if(servercfg->kill_server) break;

    if(servercfg->debug && servercfg->debug_level >= 5) {
      if(message_count == 0) {
	LogDebugMessage("CM keep alive thread checking active nodes");
      }
    }

    int error_count = 0;
    int num_nodes = 0;
    gxList<CMnode *> failed_nodes;
    gxList<CMnode *> prev_failed_nodes;
    int in_active_node_count = 0;
    int num_active_nodes = 0;

    gxListNode<CMnode *> *ptr = servercfg->nodes.GetHead();
    while(ptr) {
      if(servercfg->restart_server) break;
      if(servercfg->kill_server) break;
      num_nodes++;

      // Skip the keep alive check on this node
      if(servercfg->my_hostname == ptr->data->hostname) {
	if(ptr->data->node_is_active) num_active_nodes++;
	ptr = ptr->next;
	continue;
      }

      if(!ptr->data->node_is_active) {
	if(ptr->data->CLUSTER_TAKE_OVER_FLAG() == 0) {
	  if(in_active_node_count == 0) {
	    message << clear << ptr->data->hostname << " is marked inactive in CM config";
	    LogMessage(message.c_str());
	  }
	  if(ptr->data->CLUSTER_TAKE_OVER_FLAG() == 0) {	
	    ptr->data->CLUSTER_TAKE_OVER_FLAG(1, 0);
	    if(in_active_node_count == 0) {
	      message << clear << "Node " << ptr->data->nodename << " is inactive, signaling cluster resource failover to active nodes";
	      LogMessage(message.c_str());
	    }
	  }
	  else {
	    if(in_active_node_count == 0) {
	      message << clear << "Node " << ptr->data->nodename << " is inactive, resources failed over to active nodes";
	      LogMessage(message.c_str());
	    }
	    ptr->data->CLUSTER_TAKE_OVER_FLAG(1, 0);
	  }
	}
	in_active_node_count++;
	if(in_active_node_count >= num_inactive_node_count) in_active_node_count = 0;
	if(!ptr->next) break;
      }
      else { 
	num_active_nodes++;
      }
      if(servercfg->debug && servercfg->debug_level >= 5) {
	if(message_count == 0) {
	  message << clear << "Checking active node " << ptr->data->nodename;
	  LogDebugMessage(message.c_str());
	}
      }
      if(send_keep_alive(ptr->data, keep_alive_error_level, 1, 0) != 0) {
	if(servercfg->debug && servercfg->debug_level >= 5) {
	  message << clear << "Node " << ptr->data->nodename << " failed pre connection test";
	  LogDebugMessage(message.c_str());
	}
	error_count++;
	failed_nodes.Add(ptr->data);
      }
      else {
	if(message_count == 0) {
	  message << clear << "Node " << ptr->data->nodename << " is alive";
	  LogMessage(message.c_str());
	}
	if(ptr->data->CLUSTER_TAKE_OVER_FLAG() == 1) {
	  prev_failed_nodes.Add(ptr->data);
	}
      }
      ptr = ptr->next;
    }

    ptr = prev_failed_nodes.GetHead();
    while(ptr) {
      message << clear << "Node " << ptr->data->nodename << " is alive but failed over";
      LogMessage(message.c_str());
      int count = 10;
      if(servercfg->debug && servercfg->debug_level >= 5) {
	message << clear << "Checking previous failed node " << ptr->data->nodename;
	LogDebugMessage(message.c_str());
      }
      while(count--) {
	send_keep_alive(ptr->data, keep_alive_error_level, 1, 0);
	sSleep(1);
      }
      if(keep_alive_error_level == 0) {
	ptr->data->CLUSTER_TAKE_OVER_FLAG(0, 1);
      }
      else {
	message << clear << "Node " << ptr->data->nodename << " is back in failed state, will not fail back";
	LogMessage(message.c_str());
	failed_nodes.Add(ptr->data);
      }
      ptr = ptr->next;
    }

    if(error_count == num_nodes) {
      if(message_count == 0) {
	LogMessage("ERROR - All active CM nodes failed pre connection test");
      }
    }

    if(num_active_nodes == 0) {
      if(message_count == 0) {
	LogMessage("ERROR - All nodes are marked inactive in CM config");
      }
    }

    int max_errors = servercfg->dead_node_count;
    error_count = 0;
    ptr = failed_nodes.GetHead();
    int failed_message_count = 0;
    while(ptr) {
      if(servercfg->debug && servercfg->debug_level >= 5) {
	message << clear << "Checking failed node " << ptr->data->nodename;
	LogDebugMessage(message.c_str());
      }
      while(send_keep_alive(ptr->data, keep_alive_error_level, 1, 0) != 0) {
	error_count++;
	if(failed_message_count == 0) {
	  message << clear << "Node " << ptr->data->nodename << " failed keep alive check";
	  LogMessage(message.c_str());
	}
	failed_message_count++;
	if(failed_message_count >= max_failed_message_count) failed_message_count = 0;
	if(error_count >= max_errors) {
	  if(ptr->data->CLUSTER_TAKE_OVER_FLAG() == 0) {	
	    ptr->data->CLUSTER_TAKE_OVER_FLAG(1, 0);
	    message << clear << "Node " << ptr->data->nodename << " is dead, signaling cluster resource failover to active nodes";
	    LogMessage(message.c_str());
	  }
	  else {
	    message << clear << "Node " << ptr->data->nodename << " is dead, resources failed over to active nodes";
	    LogMessage(message.c_str());
	    ptr->data->CLUSTER_TAKE_OVER_FLAG(1, 0);
	  }
	  break;
	}
	sSleep(1);
      }
      if((error_count <  max_errors) && keep_alive_error_level == 0) {
	message << clear << "Node " << ptr->data->nodename << " is back on line";
	LogMessage(message.c_str());
	if(ptr->data->CLUSTER_TAKE_OVER_FLAG() == 1) {
	  ptr->data->CLUSTER_TAKE_OVER_FLAG(0, 1);
	  message << clear << "Node " << ptr->data->nodename << " signaling cluster failback to take back resources";
	  LogMessage(message.c_str());
	}
      }
      ptr = ptr->next;
    }

    message_count++;
    if(message_count >= max_message_count) message_count = 0;
    sSleep(5);
  }

  return (void *)0;
}

void CM_KeepAliveThread::ThreadExitRoutine(gxThread_t *thread)
// Thread exit function used to close the server thread.
{
  if(servercfg->debug && servercfg->debug_level >= 5) {
    LogDebugMessage("CM keep alive thread exit routine called");
  }

}

void CM_KeepAliveThread::ThreadCleanupHandler(gxThread_t *thread)
// Thread cleanup handler used in the event that the thread is
// canceled.
{
  if(servercfg->debug && servercfg->debug_level >= 5) {
    LogDebugMessage("CM keep alive thread cleanup handler called");
  }

}

void CM_CrontabsThread::ThreadExitRoutine(gxThread_t *thread)
// Thread exit function used to close the server thread.
{
  if(servercfg->debug && servercfg->debug_level >= 5) {
    LogDebugMessage("CM crontabs thread exit routine called");
  }

  remove_all_crontabs();
}

void CM_CrontabsThread::ThreadCleanupHandler(gxThread_t *thread)
// Thread cleanup handler used in the event that the thread is
// canceled.
{
  if(servercfg->debug && servercfg->debug_level >= 5) {
    LogDebugMessage("CM crontabs thread cleanup handler called");
  }

  remove_all_crontabs();

  if(servercfg->debug && servercfg->debug_level >= 5) {
    LogDebugMessage("CM crontabs thread cleanup handler complete");
  }
}

void *CM_CrontabsThread::ThreadEntryRoutine(gxThread_t *thread)
{
  gxString command;

  sSleep(node.crontabs_check); // Time for process start up
  command << clear << "date > " << servercfg->log_dir << "/crontab_resource.log";
  RunSystemCommand(command);

  while(servercfg->accept_clients) { 
    if(servercfg->restart_server) break;
    if(servercfg->kill_server) break;
  
    if(install_crontabs() != 0) {
      LogMessage("ERROR - Crontab install reported errors");
    }

    sSleep(node.crontabs_check);
  }  

  return (void *)0;
}

int CM_CrontabsThread::set_this_node()
{
  if(node_is_set == 1) return 0;
  int error_level = 1;

  gxListNode<CMnode *> *ptr = servercfg->nodes.GetHead();
  while(ptr) {
    if(ptr->data->hostname == servercfg->my_hostname) {
      node.Copy(*ptr->data);
      node_is_set = 1;
      error_level = 0;
      break;
    }
    ptr = ptr->next;
  }

  return error_level;
}

int CM_CrontabsThread::install_crontabs()
{
  if(set_this_node() != 0) return 1;

  gxListNode<CMnode *> *ptr;
  gxListNode <CMcrontabs> *cptr;
  gxListNode<gxString> *listptr;
  gxListNode<gxString> *installed_ptr;
  gxString command;
  gxString message;
  int has_errors = 0;
  int is_installed = 0;
  int rv = 0;
  gxString stat_message;
  gxString sbuf;

  // Install crontabs for this host
  listptr = node.crontabs.GetHead();
  while(listptr) {
    cptr = servercfg->node_crontabs.GetHead();
    while(cptr) {
      if(listptr->data == cptr->data.nickname) {
	installed_ptr = installed_crontabs.GetHead();
	is_installed = 0;
	while(installed_ptr) {
	  if(installed_ptr->data == cptr->data.nickname) {
	    is_installed = 1;
	    break;
	  }
	  installed_ptr = installed_ptr->next;
	}
	if(is_installed == 0) {
	  message << clear << "Installing crontab " << cptr->data.template_file << " to " << cptr->data.install_location;
	  LogMessage(message.c_str());
	  command << clear << cptr->data.resource_script << " "  
		  << cptr->data.template_file << " " << cptr->data.install_location << " start >> " 
		  <<  servercfg->log_dir << "/crontab_resource.log 2>&1";
	  rv = RunSystemCommand(command);
	  if(rv != 0) {
	    message << clear << "ERROR - Resource script could not install " << cptr->data.template_file 
		    << " to " << cptr->data.install_location;
	    LogMessage(message.c_str());
	    has_errors++;
	  }
	  else {
	    installed_crontabs.Add(cptr->data.nickname);
	    stat_message << clear << "cron:" << cptr->data.nickname;
	    servercfg->stats.Add(stat_message);
	  }
	}
      }
      cptr = cptr->next;
    }
    listptr = listptr->next;
  }

  // Install or remove backup crontabs if we need to failover or fail back
  ptr = servercfg->nodes.GetHead();
  while(ptr) {
    if(ptr->data->hostname == servercfg->my_hostname) {
      ptr = ptr->next; 
      continue;
    }
    if(ptr->data->CLUSTER_TAKE_OVER_FLAG() == 1) {
      // Check to see if we need to fail over
      listptr = node.backup_crontabs.GetHead();
      while(listptr) {
	gxString failed_nodename = listptr->data;
	failed_nodename.FilterChar(' ');
	failed_nodename.DeleteAfterIncluding(":");
	if(ptr->data->nodename == failed_nodename) {
	  gxString cron_nickname = listptr->data;
	  cron_nickname.FilterChar(' ');
	  cron_nickname.DeleteBeforeLastIncluding(":");
	  cptr = servercfg->node_crontabs.GetHead();
	  while(cptr) {
	    is_installed = 0;

	    // Check the other cluster nodes to see if they picked the failover resources
	    gxListNode<CMnode *> *takeoverptr = servercfg->nodes.GetHead();
	    gxString rstats_buffer, rstats_error_message;
	    while(takeoverptr) {
	      if(takeoverptr->data->nodename != ptr->data->nodename) { // Don't check the failed node
		if(takeoverptr->data->hostname == servercfg->my_hostname) { // Dont't check this node
		  takeoverptr = takeoverptr->next;
		  continue;
		}
		if(get_node_stat_buffer(takeoverptr->data, rstats_buffer, rstats_error_message) == 0) {
		  stat_message << clear << "backup_cron:" << cptr->data.nickname;
		  if(rstats_buffer.Find(stat_message) != -1) {
		    is_installed = 1;
		    message << clear << takeoverptr->data->nodename  << " has " 
			    << cptr->data.nickname << " failover crontab installed";
		    LogMessage(message.c_str());
		  }
		}
	      }
	      takeoverptr = takeoverptr->next;
	    }

	    installed_ptr = installed_backup_crontabs.GetHead();
	    while(installed_ptr) {
	      sbuf << clear << ptr->data->nodename << ":" << cptr->data.nickname;
	      if(installed_ptr->data == sbuf) {
		is_installed = 1;
		break;
	      }
	      installed_ptr = installed_ptr->next;
	    }
	    
	    if(cron_nickname == cptr->data.nickname && is_installed == 0) {
	      message << clear << ptr->data->nodename << " failover crontab install " << cptr->data.template_file
		      << " " << cptr->data.install_location;
	      LogMessage(message.c_str());
	      command << clear << cptr->data.resource_script << " "  
		      << cptr->data.template_file << " " << cptr->data.install_location << " start >> " 
		      <<  servercfg->log_dir << "/crontab_resource.log 2>&1";
	      rv = RunSystemCommand(command);
	      if(rv != 0) {
		message << clear << "ERROR - Resource script could not install failover crontab" 
			<< cptr->data.template_file;
		LogMessage(message.c_str());
		has_errors++;
	      }
	      else {
		sbuf << clear << ptr->data->nodename << ":" << cptr->data.nickname;
		installed_backup_crontabs.Add(sbuf);
		stat_message << clear << "backup_cron:" << cptr->data.nickname;
		servercfg->stats.Add(stat_message);
	      }
	    }
	    cptr = cptr->next;
	  }
	}
	listptr = listptr->next;
      }
    }
    else { // Check to see if we need to failback
      installed_ptr = installed_backup_crontabs.GetHead();
      while(installed_ptr) {
	cptr = servercfg->node_crontabs.GetHead();
	while(cptr) {
	  sbuf << clear << ptr->data->nodename << ":" << cptr->data.nickname;
	  if(installed_ptr->data == sbuf) {
	    message << clear << ptr->data->nodename << " failback crontab remove " << cptr->data.template_file
		    << " " << cptr->data.install_location;
	    LogMessage(message.c_str());
	    command << clear << cptr->data.resource_script << " "  
		    << cptr->data.template_file << " " << cptr->data.install_location << " stop >> " 
		    <<  servercfg->log_dir << "/crontab_resource.log 2>&1";
	    rv = RunSystemCommand(command);
	    if(rv != 0) {
	      message << clear << "ERROR - Resource script could not remove failover crontab" 
		      << cptr->data.template_file;
	      LogMessage(message.c_str());
	      has_errors++;
	    }
	    else {
	      installed_backup_crontabs.Remove(installed_ptr);
	      installed_ptr = 0; // Signal to start at top of installed backup list
	      stat_message << clear << "backup_cron:" << cptr->data.nickname;
	      servercfg->stats.Remove(stat_message);
	      break;
	    }
	  }
	  cptr = cptr->next;
	}
	if(!installed_ptr) { // Check to see if the list pointer was removed
	  installed_ptr = installed_backup_crontabs.GetHead();
	  continue;
	}
	installed_ptr = installed_ptr->next;
      }
    }	    
    ptr = ptr->next;
  }

  return has_errors;
}

int CM_CrontabsThread::remove_all_crontabs()
{
  int has_errors = 0;
  gxListNode <CMcrontabs> *cptr = servercfg->node_crontabs.GetHead();
  gxString command;
  gxString message;

  if(servercfg->debug && servercfg->debug_level >= 5) {
    LogDebugMessage("Removing all CM crontabs");
  }

  while(cptr) {
    command << clear << cptr->data.resource_script << " "  
	    << cptr->data.template_file << " " << cptr->data.install_location << " stop >> " 
	    <<  servercfg->log_dir << "/crontab_resource.log 2>&1";
    if(RunSystemCommand(command) != 0) {
      message << clear << "ERROR - Resource script could not remove crontab" 
	      << cptr->data.template_file << " " << cptr->data.install_location;
      LogMessage(message.c_str());
      has_errors++;
    }
    cptr = cptr->next;
  }

  if(has_errors == 0) {
    installed_crontabs.ClearList();
    installed_backup_crontabs.ClearList();
  }
  return has_errors;
}

void CM_IPaddrsThread::ThreadExitRoutine(gxThread_t *thread)
// Thread exit function used to close the server thread.
{
  if(servercfg->debug && servercfg->debug_level >= 5) {
    LogDebugMessage("CM floating IP address thread exit routine called");
  }
  remove_all_ipaddrs();
}

void CM_IPaddrsThread::ThreadCleanupHandler(gxThread_t *thread)
{
  if(servercfg->debug && servercfg->debug_level >= 5) {
    LogDebugMessage("CM floating IP address thread cleanup handler called");
  }
  remove_all_ipaddrs();
}

void *CM_IPaddrsThread::ThreadEntryRoutine(gxThread_t *thread)
{
  gxString command;

  sSleep(node.ipaddrs_check); // Time for process start up
  command << clear << "date > " << servercfg->log_dir << "/ipv4addr_resource.log";
  RunSystemCommand(command);

  while(servercfg->accept_clients) { 
    if(servercfg->restart_server) break;
    if(servercfg->kill_server) break;
  
    if(install_floating_ipaddrs() != 0) {
      LogMessage("ERROR - Floating IP address install reported errors");
    }

    sSleep(node.ipaddrs_check);
  }  

  return (void *)0;
}

int CM_IPaddrsThread::set_this_node()
{
  if(node_is_set == 1) return 0;
  int error_level = 1;

  gxListNode<CMnode *> *ptr = servercfg->nodes.GetHead();
  while(ptr) {
    if(ptr->data->hostname == servercfg->my_hostname) {
      node.Copy(*ptr->data);
      node_is_set = 1;
      error_level = 0;
      break;
    }
    ptr = ptr->next;
  }

  return error_level;
}

int CM_IPaddrsThread::remove_all_ipaddrs()
{
  int has_errors = 0;
  gxListNode <CMipaddrs> *iptr = servercfg->node_ipaddrs.GetHead();
  gxString command;
  gxString message;

  if(servercfg->debug && servercfg->debug_level >= 5) {
    LogDebugMessage("Removing all floating IP addresses");
  }

  while(iptr) {
    command << clear << iptr->data.resource_script << " "  
	    << iptr->data.ip_address << " " << iptr->data.netmask 
	    << " " << iptr->data.interface << " stop >> " 
	    <<  servercfg->log_dir << "/ipv4addr_resource.log 2>&1";
    if(RunSystemCommand(command) != 0) {
      message << clear << "ERROR - Resource script could not remove floating IP address " 
	      << iptr->data.ip_address << " " << iptr->data.interface;
      LogMessage(message.c_str());
      has_errors++;
    }
    iptr = iptr->next;
  }

  if(has_errors == 0) {
    installed_floating_ipaddrs.ClearList();
    installed_backup_floating_ipaddrs.ClearList();
  }
  return has_errors;
}

int CM_IPaddrsThread::install_floating_ipaddrs()
{
  if(set_this_node() != 0) return 1;

  gxListNode<CMnode *> *ptr;
  gxListNode <CMipaddrs> *iptr;
  gxListNode<gxString> *listptr;
  gxListNode<gxString> *installed_ptr;
  gxString command;
  gxString message;
  int has_errors = 0;
  int is_installed = 0;
  int rv = 0;
  gxString stat_message;
  gxString sbuf;

  // Install floating IP addresses for this host
  listptr = node.floating_ip_addrs.GetHead();
  while(listptr) {
    iptr = servercfg->node_ipaddrs.GetHead();
    while(iptr) {
      if(listptr->data == iptr->data.nickname) {
	installed_ptr = installed_floating_ipaddrs.GetHead();
	is_installed = 0;
	while(installed_ptr) {
	  if(installed_ptr->data == iptr->data.nickname) {
	    is_installed = 1;
	    // TODO: Need to add a watch to the IP resource script 
	    // TODO: Interface may not get set if back node has not released floating IP
	    break;
	  }
	  installed_ptr = installed_ptr->next;
	}
	if(is_installed == 0) {
	  message << clear << "Installing floating IP " << iptr->data.ip_address << " to " << iptr->data.interface;
	  LogMessage(message.c_str());
	  command << clear << iptr->data.resource_script << " "  
		  << iptr->data.ip_address << " " << iptr->data.netmask
		  << " " << iptr->data.interface << " start >> " 
		  <<  servercfg->log_dir << "/ipv4addr_resource.log 2>&1";
	  rv = RunSystemCommand(command);
	  if(rv != 0) {
	    message << clear << "ERROR - Resource script could not install " << iptr->data.ip_address << " to " << iptr->data.interface;
	    LogMessage(message.c_str());
	    has_errors++;
	  }
	  else {
	    installed_floating_ipaddrs.Add(iptr->data.nickname);
	    stat_message << clear << "ip:" << iptr->data.nickname;
	    servercfg->stats.Add(stat_message);
	  }
	}
      }
      iptr = iptr->next;
    }
    listptr = listptr->next;
  }

  // Install or remove backup ipaddrs if we need to failover or fail back
  ptr = servercfg->nodes.GetHead();
  while(ptr) {
    if(ptr->data->hostname == servercfg->my_hostname) {
      ptr = ptr->next; 
      continue;
    }
    if(ptr->data->CLUSTER_TAKE_OVER_FLAG() == 1) {
      // Check to see if we need to fail over
      listptr = node.backup_floating_ip_addrs.GetHead();
      while(listptr) {
	gxString failed_nodename = listptr->data;
	failed_nodename.FilterChar(' ');
	failed_nodename.DeleteAfterIncluding(":");
	if(ptr->data->nodename == failed_nodename) {
	  gxString ipaddr_nickname = listptr->data;
	  ipaddr_nickname.FilterChar(' ');
	  ipaddr_nickname.DeleteBeforeLastIncluding(":");
	  iptr = servercfg->node_ipaddrs.GetHead();
	  while(iptr) {
	    is_installed = 0;

	    // Check the other cluster nodes to see if they picked the failover resources
	    gxListNode<CMnode *> *takeoverptr = servercfg->nodes.GetHead();
	    gxString rstats_buffer, rstats_error_message;
	    while(takeoverptr) {
	      if(takeoverptr->data->nodename != ptr->data->nodename) { // Don't check the failed node
		if(takeoverptr->data->hostname == servercfg->my_hostname) { // Dont't check this node
		  takeoverptr = takeoverptr->next;
		  continue;
		}
		if(get_node_stat_buffer(takeoverptr->data, rstats_buffer, rstats_error_message) == 0) {
		  stat_message << clear << "backup_ip:" << iptr->data.nickname;
		  if(rstats_buffer.Find(stat_message) != -1) {
		    is_installed = 1;
		    message << clear << takeoverptr->data->nodename  << " has " 
			    << iptr->data.nickname << " failover ipaddr installed";
		    LogMessage(message.c_str());
		  }
		}
	      }
	      takeoverptr = takeoverptr->next;
	    }

	    installed_ptr = installed_backup_floating_ipaddrs.GetHead();
	    while(installed_ptr) {
	      sbuf << clear << ptr->data->nodename << ":" << iptr->data.nickname;
	      if(installed_ptr->data == sbuf) {
		is_installed = 1;
		break;
	      }
	      installed_ptr = installed_ptr->next;
	    }
	    
	    if(ipaddr_nickname == iptr->data.nickname && is_installed == 0) {
	      message << clear << ptr->data->nodename << " failover floating IP install " << iptr->data.ip_address
		      << " " << iptr->data.interface;
	      LogMessage(message.c_str());
	      command << clear << iptr->data.resource_script << " "  
		      << iptr->data.ip_address << " " << iptr->data.netmask 
		      << " " << iptr->data.interface << " start >> " 
		      <<  servercfg->log_dir << "/ipv4addr_resource.log 2>&1";
	      rv = RunSystemCommand(command);
	      if(rv != 0) {
		message << clear << "ERROR - Resource script could not install failover floaing IP address " 
			<< iptr->data.ip_address;
		LogMessage(message.c_str());
		has_errors++;
	      }
	      else {
		sbuf << clear << ptr->data->nodename << ":" << iptr->data.nickname;
		installed_backup_floating_ipaddrs.Add(sbuf);
		stat_message << clear << "backup_ip:" << iptr->data.nickname;
		servercfg->stats.Add(stat_message);
	      }
	    }
	    iptr = iptr->next;
	  }
	}
	listptr = listptr->next;
      }
    }
    else { // Check to see if we need to failback
      installed_ptr = installed_backup_floating_ipaddrs.GetHead();
      while(installed_ptr) {
	iptr = servercfg->node_ipaddrs.GetHead();
	while(iptr) {
	  sbuf << clear << ptr->data->nodename << ":" << iptr->data.nickname;
	  if(installed_ptr->data == sbuf) {
	    message << clear << ptr->data->nodename << " failback floaing IP address remove " << iptr->data.ip_address
		    << " " << iptr->data.interface;
	    LogMessage(message.c_str());
	    command << clear << iptr->data.resource_script << " "  
		    << iptr->data.ip_address << " " << iptr->data.netmask 
		    << " " << iptr->data.interface << " stop >> " 
		    <<  servercfg->log_dir << "/ipv4addr_resource.log 2>&1";
	    rv = RunSystemCommand(command);
	    if(rv != 0) {
	      message << clear << "ERROR - Resource script could not remove failover floating IP address " 
		      << iptr->data.ip_address;
	      LogMessage(message.c_str());
	      has_errors++;
	    }
	    else {
	      installed_backup_floating_ipaddrs.Remove(installed_ptr);
	      installed_ptr = 0; // Signal to start at top of installed backup list
	      stat_message << clear << "backup_ip:" << iptr->data.nickname;
	      servercfg->stats.Remove(stat_message);
	      break;
	    }
	  }
	  iptr = iptr->next;
	}
	if(!installed_ptr) { // Check to see if the list pointer was removed
	  installed_ptr = installed_backup_floating_ipaddrs.GetHead();
	  continue;
	}
	installed_ptr = installed_ptr->next;
      }
    }	    
    ptr = ptr->next;
  }

  return has_errors;
}

void CM_servicesThread::ThreadExitRoutine(gxThread_t *thread)
// Thread exit function used to close the server thread.
{
  if(servercfg->debug && servercfg->debug_level >= 5) {
    LogDebugMessage("CM services thread exit routine called");
  }
  set_this_node();
  if(node.keep_services) {
    stop_backup_services();
  }
  else {
    stop_all_services();
  }
}

void CM_servicesThread::ThreadCleanupHandler(gxThread_t *thread)
{
  if(servercfg->debug && servercfg->debug_level >= 5) {
    LogDebugMessage("CM services thread cleanup handler called");
  }
  set_this_node();
  if(node.keep_services) {
    stop_backup_services();
  }
  else {
    stop_all_services();
  }
}

void *CM_servicesThread::ThreadEntryRoutine(gxThread_t *thread)
{
  gxString command;

  sSleep(node.services_check); // Time for process start up

  command << clear << "date > " << servercfg->log_dir << "/service_resource.log";
  RunSystemCommand(command);

  while(servercfg->accept_clients) { 
    if(servercfg->restart_server) break;
    if(servercfg->kill_server) break;
  
    if(install_services() != 0) {
      LogMessage("ERROR - Service start/watch reported errors");
    }

    sSleep(node.services_check);
  }  

  return (void *)0;
}

int CM_servicesThread::set_this_node()
{
  if(node_is_set == 1) return 0;
  int error_level = 1;

  gxListNode<CMnode *> *ptr = servercfg->nodes.GetHead();
  while(ptr) {
    if(ptr->data->hostname == servercfg->my_hostname) {
      node.Copy(*ptr->data);
      node_is_set = 1;
      error_level = 0;
      break;
    }
    ptr = ptr->next;
  }

  return error_level;
}

int CM_servicesThread::stop_backup_services()
{
  int has_errors = 0;
  if(set_this_node() != 0) return 1;

  gxListNode <CMservices> *sptr;
  gxString command;
  gxString message;
  gxListNode<gxString> *installed_ptr;
  int rv;

  installed_ptr = installed_backup_services.GetHead();
  while(installed_ptr) {
    sptr = servercfg->node_services.GetHead();
    while(sptr) {
      if(installed_ptr->data == sptr->data.nickname) {
	message << clear << "Stopping failover service " << sptr->data.service_name;
	LogMessage(message.c_str());
	command << clear << sptr->data.resource_script << " " << sptr->data.service_name << " stop >> " 
		<<  servercfg->log_dir << "/service_resource.log 2>&1";
	rv = RunSystemCommand(command);
	if(rv != 0) {
	  message << clear << "ERROR - Resource script could not stop failover service" 
		  << sptr->data.service_name;
	  LogMessage(message.c_str());
	  has_errors++;
	}
	else {
	  installed_backup_services.Remove(installed_ptr);
	  installed_ptr = 0; // Signal to start at top of installed backup list
	  break;
	}
      }
      sptr = sptr->next;
    }
    if(!installed_ptr) { // Check to see if the list pointer was removed
      installed_ptr = installed_backup_services.GetHead();
      continue;
    }
    installed_ptr = installed_ptr->next;
  }

  return has_errors;
}

int CM_servicesThread::stop_all_services()
{
  int has_errors = 0;
  gxListNode <CMservices> *sptr = servercfg->node_services.GetHead();
  gxString command;
  gxString message;

  if(servercfg->debug && servercfg->debug_level >= 5) {
    LogDebugMessage("Stopping all CM services");
  }

  while(sptr) {
    command << clear << sptr->data.resource_script << " " << sptr->data.service_name << " stop >> " 
	    <<  servercfg->log_dir << "/service_resource.log 2>&1";
    if(RunSystemCommand(command) != 0) {
      message << clear << "ERROR - Resource script could not stop service" 
	      << sptr->data.service_name;
      LogMessage(message.c_str());
      has_errors++;
    }
    sptr = sptr->next;
  }

  if(has_errors == 0) {
    installed_services.ClearList();
    installed_backup_services.ClearList();
  }
  return has_errors;
}

int CM_servicesThread::install_services()
{
  if(set_this_node() != 0) return 1;

  gxListNode<CMnode *> *ptr;
  gxListNode <CMservices> *sptr;
  gxListNode<gxString> *listptr;
  gxListNode<gxString> *installed_ptr;
  gxString command;
  gxString message;
  int has_errors = 0;
  int is_started = 0;
  int rv = 0;
  gxString stat_message;
  gxString sbuf;

  // Install services for this host
  listptr = node.services.GetHead();
  while(listptr) {
    sptr = servercfg->node_services.GetHead();
    while(sptr) {
      if(listptr->data == sptr->data.nickname) {
	installed_ptr = installed_services.GetHead();
	is_started = 0;
	while(installed_ptr) {
	  if(installed_ptr->data == sptr->data.nickname) {
	    is_started = 1;
	    if(servercfg->debug && servercfg->debug_level >= 5) {
	      message << clear << "Checking service " << sptr->data.service_name;
	      LogDebugMessage(message.c_str());
	    }
	    command << clear << sptr->data.resource_script << " " << sptr->data.service_name 
		    << " watch >> " <<  servercfg->log_dir << "/service_resource.log 2>&1";
	    rv = RunSystemCommand(command);
	    if(rv != 0) {
	      if(servercfg->debug && servercfg->debug_level >= 5) {
		message << clear << "ERROR - Resource script reported service error " 
			<< sptr->data.service_name;
		LogDebugMessage(message.c_str());
	      }
	    }
	    break;
	  }
	  installed_ptr = installed_ptr->next;
	}
	if(is_started == 0) {
	  message << clear << "Starting service " << sptr->data.service_name;
	  LogMessage(message.c_str());
	  command << clear << sptr->data.resource_script << " " << sptr->data.service_name << " start >> " 
		  <<  servercfg->log_dir << "/service_resource.log 2>&1";
	  rv = RunSystemCommand(command);
	  if(rv != 0) {
	    message << clear << "ERROR - Resource script could not service " << sptr->data.service_name;
	    LogMessage(message.c_str());
	    has_errors++;
	  }
	  else {
	    installed_services.Add(sptr->data.nickname);
	    stat_message << clear << "service:" << sptr->data.nickname;
	    servercfg->stats.Add(stat_message);
	  }
	}
      }
      sptr = sptr->next;
    }
    listptr = listptr->next;
  }

  // Install or remove backup services if we need to failover or fail back
  ptr = servercfg->nodes.GetHead();
  while(ptr) {
    if(ptr->data->hostname == servercfg->my_hostname) {
      ptr = ptr->next; 
      continue;
    }
    if(ptr->data->CLUSTER_TAKE_OVER_FLAG() == 1) {
      // Check to see if we need to fail over
      listptr = node.backup_services.GetHead();
      while(listptr) {
	gxString failed_nodename = listptr->data;
	failed_nodename.FilterChar(' ');
	failed_nodename.DeleteAfterIncluding(":");
	if(ptr->data->nodename == failed_nodename) {
	  gxString service_nickname = listptr->data;
	  service_nickname.FilterChar(' ');
	  service_nickname.DeleteBeforeLastIncluding(":");
	  sptr = servercfg->node_services.GetHead();
	  while(sptr) {
	    is_started = 0;

	    // Check the other cluster nodes to see if they picked the failover resources
	    gxListNode<CMnode *> *takeoverptr = servercfg->nodes.GetHead();
	    gxString rstats_buffer, rstats_error_message;
	    while(takeoverptr) {
	      if(takeoverptr->data->nodename != ptr->data->nodename) { // Don't check the failed node 
		if(takeoverptr->data->hostname == servercfg->my_hostname) { // Dont't check this node
		  takeoverptr = takeoverptr->next;
		  continue;
		}
		if(get_node_stat_buffer(takeoverptr->data, rstats_buffer, rstats_error_message) == 0) {
		  stat_message << clear << "backup_service:" << sptr->data.nickname;
		  if(rstats_buffer.Find(stat_message) != -1) {
		    is_started = 1;
		    message << clear << takeoverptr->data->nodename  << " has " 
			    << sptr->data.nickname << " failover service installed";
		    LogMessage(message.c_str());
		  }
		}
	      }
	      takeoverptr = takeoverptr->next;
	    }

	    installed_ptr = installed_backup_services.GetHead();
	    while(installed_ptr) {
	      sbuf << clear << ptr->data->nodename << ":" << sptr->data.nickname;
	      if(installed_ptr->data == sbuf) {
		is_started = 1;
		if(servercfg->debug && servercfg->debug_level >= 5) {
		  message << clear << "Checking failed over service " << sptr->data.service_name;
		  LogDebugMessage(message.c_str());
		}
		command << clear << sptr->data.resource_script << " " << sptr->data.service_name 
			<< " watch >> " <<  servercfg->log_dir << "/service_resource.log 2>&1";
		rv = RunSystemCommand(command);
		if(rv != 0) {
		  if(servercfg->debug && servercfg->debug_level >= 5) {
		    message << clear << "ERROR - Resource script reported failed over service error " 
			    << sptr->data.service_name;
		    LogDebugMessage(message.c_str());
		  }
		}
		break;
	      }
	      installed_ptr = installed_ptr->next;
	    }
	    if(service_nickname == sptr->data.nickname && is_started == 0) {
	      message << clear << ptr->data->nodename << " failover service start " << sptr->data.service_name;
	      LogMessage(message.c_str());
	      command << clear << sptr->data.resource_script << " " << sptr->data.service_name << " start >> " 
		      <<  servercfg->log_dir << "/service_resource.log 2>&1";
	      rv = RunSystemCommand(command);
	      if(rv != 0) {
		message << clear << "ERROR - Resource script could not start failover service" 
			<< sptr->data.service_name;
		LogMessage(message.c_str());
		has_errors++;
	      }
	      else {
		sbuf << clear << ptr->data->nodename << ":" << sptr->data.nickname;
		installed_backup_services.Add(sbuf);
		stat_message << clear << "backup_service:" << sptr->data.nickname;
		servercfg->stats.Add(stat_message);
	      }
	    }
	    sptr = sptr->next;
	  }
	}
	listptr = listptr->next;
      }
    }
    else { // Check to see if we need to failback
      installed_ptr = installed_backup_services.GetHead();
      while(installed_ptr) {
	sptr = servercfg->node_services.GetHead();
	while(sptr) {
	  sbuf << clear << ptr->data->nodename << ":" << sptr->data.nickname;
	  if(installed_ptr->data == sbuf) {
	    message << clear << ptr->data->nodename << " failback service stop " << sptr->data.service_name;
	    LogMessage(message.c_str());
	    command << clear << sptr->data.resource_script << " " << sptr->data.service_name << " stop >> " 
		    <<  servercfg->log_dir << "/service_resource.log 2>&1";
	    rv = RunSystemCommand(command);
	    if(rv != 0) {
	      message << clear << "ERROR - Resource script could not stop failover service" 
		      << sptr->data.service_name;
	      LogMessage(message.c_str());
	      has_errors++;
	    }
	    else {
	      installed_backup_services.Remove(installed_ptr);
	      installed_ptr = 0; // Signal to start at top of installed backup list
	      stat_message << clear << "backup_service:" << sptr->data.nickname;
	      servercfg->stats.Remove(stat_message);
	      break;
	    }
	  }
	  sptr = sptr->next;
	}
	if(!installed_ptr) { // Check to see if the list pointer was removed
	  installed_ptr = installed_backup_services.GetHead();
	  continue;
	}
	installed_ptr = installed_ptr->next;
      }
    }	    
    ptr = ptr->next;
  }

  return has_errors;
}

void CM_applicationsThread::ThreadExitRoutine(gxThread_t *thread)
// Thread exit function used to close the server thread.
{
  if(servercfg->debug && servercfg->debug_level >= 5) {
    LogDebugMessage("CM applications thread exit routine called");
  }
  set_this_node();
  if(node.keep_applications) {
    stop_backup_applications();
  }
  else {
    stop_all_applications();
  }
}

void CM_applicationsThread::ThreadCleanupHandler(gxThread_t *thread)
{
  if(servercfg->debug && servercfg->debug_level >= 5) {
    LogDebugMessage("CM applications thread cleanup handler called");
  }
  set_this_node();
  if(node.keep_applications) {
    stop_backup_applications();
  }
  else {
    stop_all_applications();
  }
}

void *CM_applicationsThread::ThreadEntryRoutine(gxThread_t *thread)
{
  gxString command;

  sSleep(node.applications_check); // Time for process start up

  command << clear << "date > " << servercfg->log_dir << "/application_resource.log";
  RunSystemCommand(command);

  while(servercfg->accept_clients) { 
    if(servercfg->restart_server) break;
    if(servercfg->kill_server) break;
  
    if(install_applications() != 0) {
      LogMessage("ERROR - Application start/watch reported errors");
    }

    sSleep(node.applications_check);
  }  

  return (void *)0;
}

int CM_applicationsThread::set_this_node()
{
  if(node_is_set == 1) return 0;
  int error_level = 1;

  gxListNode<CMnode *> *ptr = servercfg->nodes.GetHead();
  while(ptr) {
    if(ptr->data->hostname == servercfg->my_hostname) {
      node.Copy(*ptr->data);
      node_is_set = 1;
      error_level = 0;
      break;
    }
    ptr = ptr->next;
  }

  return error_level;
}

int CM_applicationsThread::stop_all_applications()
{
  int has_errors = 0;
  gxListNode <CMapplications> *aptr = servercfg->node_applications.GetHead();
  gxString command;
  gxString message;

  if(servercfg->debug && servercfg->debug_level >= 5) {
    LogDebugMessage("Stopping all CM applications");
  }

  while(aptr) {
    if(servercfg->debug && servercfg->debug_level >= 10) {
      command << clear << aptr->data.stop_program;
    }
    else {
      command << clear << aptr->data.stop_program << " > /dev/null 2>&1";
    }
    if(RunSystemCommand(command, aptr->data.user, aptr->data.group) != 0) {
      message << clear << "ERROR - Resource script could not stop application" 
	      << aptr->data.nickname;
      LogMessage(message.c_str());
      has_errors++;
    }
    aptr = aptr->next;
  }

  if(has_errors == 0) {
    installed_applications.ClearList();
    installed_backup_applications.ClearList();
  }
  return has_errors;
}

int CM_applicationsThread::stop_backup_applications()
{
  int has_errors = 0;
  if(set_this_node() != 0) return 1;

  gxListNode <CMapplications> *aptr;
  gxString command;
  gxString message;
  gxListNode<gxString> *installed_ptr;
  int rv;

  installed_ptr = installed_backup_applications.GetHead();
  while(installed_ptr) {
    aptr = servercfg->node_applications.GetHead();
    while(aptr) {
      if(installed_ptr->data == aptr->data.nickname) {
	message << clear << "Stopping failover application " << aptr->data.nickname;
	LogMessage(message.c_str());
	if(servercfg->debug && servercfg->debug_level >= 10) {
	  command << clear << aptr->data.stop_program;
	}
	else {
	  command << clear << aptr->data.stop_program << " > /dev/null 2>&1";
	}
	rv = RunSystemCommand(command, aptr->data.user, aptr->data.group);
	if(rv != 0) {
	  message << clear << "ERROR - Resource script could not stop failover application" 
		  << aptr->data.nickname;
	  LogMessage(message.c_str());
	  has_errors++;
	}
	else {
	  installed_backup_applications.Remove(installed_ptr);
	  installed_ptr = 0; // Signal to start at top of installed backup list
	  break;
	}
      }
      aptr = aptr->next;
    }
    if(!installed_ptr) { // Check to see if the list pointer was removed
      installed_ptr = installed_backup_applications.GetHead();
      continue;
    }
    installed_ptr = installed_ptr->next;
  }
  
  return has_errors;
}

int CM_applicationsThread::install_applications()
{
  if(set_this_node() != 0) return 1;

  gxListNode<CMnode *> *ptr;
  gxListNode <CMapplications> *aptr;
  gxListNode<gxString> *listptr;
  gxListNode<gxString> *installed_ptr;
  gxString command;
  gxString message;
  int has_errors = 0;
  int is_started = 0;
  int rv = 0;
  gxString stat_message;
  gxString sbuf;

  // Install applications for this host
  listptr = node.applications.GetHead();
  while(listptr) {
    aptr = servercfg->node_applications.GetHead();
    while(aptr) {
      if(listptr->data == aptr->data.nickname) {
	installed_ptr = installed_applications.GetHead();
	is_started = 0;
	while(installed_ptr) {
	  if(installed_ptr->data == aptr->data.nickname) {
	    is_started = 1;
	    if(!aptr->data.ensure_script.is_null()) {
	      if(servercfg->debug && servercfg->debug_level >= 5) {
		message << clear << "Checking application " << aptr->data.nickname;
		LogDebugMessage(message.c_str());
	      }
	      if(servercfg->debug && servercfg->debug_level >= 10) {
		command << clear << aptr->data.ensure_script;
	      }
	      else {
		command << clear << aptr->data.ensure_script << " > /dev/null 2>&1";
	      }
	      rv = RunSystemCommand(command, aptr->data.user, aptr->data.group);
	      if(rv != 0) {
		if(servercfg->debug && servercfg->debug_level >= 5) {
		  message << clear << "ERROR - Resource script reported application ensure error " 
			  << aptr->data.nickname;
		  LogDebugMessage(message.c_str());
		}
	      }
	    }
	    break;
	  }
	  installed_ptr = installed_ptr->next;
	}
	if(is_started == 0) {
	  message << clear << "Starting application " << aptr->data.nickname;
	  LogMessage(message.c_str());
	  if(servercfg->debug && servercfg->debug_level >= 10) {
	    command << clear << aptr->data.start_program;
	  }
	  else {
	    command << clear << aptr->data.start_program << " > /dev/null 2>&1";
	  }
	  rv = RunSystemCommand(command, aptr->data.user, aptr->data.group);
	  if(rv != 0) {
	    message << clear << "ERROR - Resource script could not start application " << aptr->data.nickname;
	    LogMessage(message.c_str());
	    has_errors++;
	  }
	  else {
	    installed_applications.Add(aptr->data.nickname);
	    stat_message << clear << "application:" << aptr->data.nickname;
	    servercfg->stats.Add(stat_message);
	  }
	}
      }
      aptr = aptr->next;
    }
    listptr = listptr->next;
  }

  // Install or remove backup applications if we need to failover or fail back
  ptr = servercfg->nodes.GetHead();
  while(ptr) {
    if(ptr->data->hostname == servercfg->my_hostname) {
      ptr = ptr->next; 
      continue;
    }
    if(ptr->data->CLUSTER_TAKE_OVER_FLAG() == 1) {
      // Check to see if we need to fail over
      listptr = node.backup_applications.GetHead();
      while(listptr) {
	gxString failed_nodename = listptr->data;
	failed_nodename.FilterChar(' ');
	failed_nodename.DeleteAfterIncluding(":");
	if(ptr->data->nodename == failed_nodename) {
	  gxString application_nickname = listptr->data;
	  application_nickname.FilterChar(' ');
	  application_nickname.DeleteBeforeLastIncluding(":");
	  aptr = servercfg->node_applications.GetHead();
	  while(aptr) {
	    is_started = 0;

	    // Check the other cluster nodes to see if they picked the failover resources
	    gxListNode<CMnode *> *takeoverptr = servercfg->nodes.GetHead();
	    gxString rstats_buffer, rstats_error_message;
	    while(takeoverptr) {
	      if(takeoverptr->data->nodename != ptr->data->nodename) { // Don't check the failed node 
		if(takeoverptr->data->hostname == servercfg->my_hostname) { // Dont't check this node
		  takeoverptr = takeoverptr->next;
		  continue;
		}
		if(get_node_stat_buffer(takeoverptr->data, rstats_buffer, rstats_error_message) == 0) {
		  stat_message << clear << "backup_application:" << aptr->data.nickname;
		  if(rstats_buffer.Find(stat_message) != -1) {
		    is_started = 1;
		    message << clear << takeoverptr->data->nodename  << " has " 
			    << aptr->data.nickname << " failover application installed";
		    LogMessage(message.c_str());
		  }
		}
	      }
	      takeoverptr = takeoverptr->next;
	    }

	    installed_ptr = installed_backup_applications.GetHead();
	    while(installed_ptr) {
	      sbuf << clear << ptr->data->nodename << ":" << aptr->data.nickname;
	      if(installed_ptr->data == sbuf) {
		is_started = 1;
		if(!aptr->data.ensure_script.is_null()) {
		  if(servercfg->debug && servercfg->debug_level >= 5) {
		    message << clear << "Checking failed over application " << aptr->data.nickname;
		    LogDebugMessage(message.c_str());
		  }
		  if(servercfg->debug && servercfg->debug_level >= 10) {
		    command << clear << aptr->data.ensure_script;
		  }
		  else {
		    command << clear << aptr->data.ensure_script << " > /dev/null 2>&1";
		  }
		  rv = RunSystemCommand(command, aptr->data.user, aptr->data.group);
		  if(rv != 0) {
		    if(servercfg->debug && servercfg->debug_level >= 5) {
		      message << clear << "ERROR - Resource script reported failed over application ensure error " 
			      << aptr->data.nickname;
		      LogDebugMessage(message.c_str());
		    }
		  }
		}
		break;
	      }
	      installed_ptr = installed_ptr->next;
	    }
	    if(application_nickname == aptr->data.nickname && is_started == 0) {
	      message << clear << ptr->data->nodename << " failover application start " << aptr->data.nickname;
	      LogMessage(message.c_str());
	      if(servercfg->debug && servercfg->debug_level >= 10) {
		command << clear << aptr->data.start_program;
	      }
	      else {
		command << clear << aptr->data.start_program << " > /dev/null 2>&1";
	      }
	      rv = RunSystemCommand(command, aptr->data.user, aptr->data.group);
	      if(rv != 0) {
		message << clear << "ERROR - Resource script could not start failover application" 
			<< aptr->data.nickname;
		LogMessage(message.c_str());
		has_errors++;
	      }
	      else {
		sbuf << clear << ptr->data->nodename << ":" << aptr->data.nickname;
		installed_backup_applications.Add(sbuf);
		stat_message << clear << "backup_application:" << aptr->data.nickname;
		servercfg->stats.Add(stat_message);
	      }
	    }
	    aptr = aptr->next;
	  }
	}
	listptr = listptr->next;
      }
    }
    else { // Check to see if we need to failback
      installed_ptr = installed_backup_applications.GetHead();
      while(installed_ptr) {
	aptr = servercfg->node_applications.GetHead();
	while(aptr) {
	  sbuf << clear << ptr->data->nodename << ":" << aptr->data.nickname;
	  if(installed_ptr->data == sbuf) {
	    message << clear << ptr->data->nodename << " failback application stop " << aptr->data.nickname;
	    LogMessage(message.c_str());
	    if(servercfg->debug && servercfg->debug_level >= 10) {
	      command << clear << aptr->data.stop_program;
	    }
	    else {
	      command << clear << aptr->data.stop_program << " > /dev/null 2>&1";
	    }
	    rv = RunSystemCommand(command, aptr->data.user, aptr->data.group);
	    if(rv != 0) {
	      message << clear << "ERROR - Resource script could not stop failover application" 
		      << aptr->data.nickname;
	      LogMessage(message.c_str());
	      has_errors++;
	    }
	    else {
	      installed_backup_applications.Remove(installed_ptr);
	      installed_ptr = 0; // Signal to start at top of installed backup list
	      stat_message << clear << "backup_application:" << aptr->data.nickname;
	      servercfg->stats.Remove(stat_message);
	      break;
	    }
	  }
	  aptr = aptr->next;
	}
	if(!installed_ptr) { // Check to see if the list pointer was removed
	  installed_ptr = installed_backup_applications.GetHead();
	  continue;
	}
	installed_ptr = installed_ptr->next;
      }
    }	    
    ptr = ptr->next;
  }

  return has_errors;
}

void CM_filesystemsThread::ThreadExitRoutine(gxThread_t *thread)
// Thread exit function used to close the server thread.
{
  if(servercfg->debug && servercfg->debug_level >= 5) {
    LogDebugMessage("CM file systems thread exit routine called");
  }

  set_this_node();
  if(node.keep_filesystems) {
    unmount_backup_filesystems();
  }
  else {
    unmount_all_filesystems();
  }
}

void CM_filesystemsThread::ThreadCleanupHandler(gxThread_t *thread)
{
  if(servercfg->debug && servercfg->debug_level >= 5) {
    LogDebugMessage("CM file systems thread cleanup handler called");
  }

  set_this_node();
  if(node.keep_filesystems) {
    unmount_backup_filesystems();
  }
  else {
    unmount_all_filesystems();
  }
}

void *CM_filesystemsThread::ThreadEntryRoutine(gxThread_t *thread)
{
  gxString command;

  sSleep(node.applications_check); // Time for process start up
  command << clear << "date > " << servercfg->log_dir << "/filesystem_resource.log";
  RunSystemCommand(command);

  while(servercfg->accept_clients) { 
    if(servercfg->restart_server) break;
    if(servercfg->kill_server) break;
  
    if(mount_filesystems() != 0) {
      LogMessage("ERROR - File systems mount/unmount reported errors");
    }

    sSleep(node.filesystems_check);
  }  

  return (void *)0;
}

int CM_filesystemsThread::set_this_node()
{
  if(node_is_set == 1) return 0;
  int error_level = 1;

  gxListNode<CMnode *> *ptr = servercfg->nodes.GetHead();
  while(ptr) {
    if(ptr->data->hostname == servercfg->my_hostname) {
      node.Copy(*ptr->data);
      node_is_set = 1;
      error_level = 0;
      break;
    }
    ptr = ptr->next;
  }

  return error_level;
}

int CM_filesystemsThread::mount_filesystems()
{
  if(set_this_node() != 0) return 1;

  gxListNode<CMnode *> *ptr;
  gxListNode <CMfilesystems> *fptr;
  gxListNode<gxString> *listptr;
  gxListNode<gxString> *installed_ptr;
  gxString command;
  gxString message;
  int has_errors = 0;
  int is_mounted = 0;
  int rv = 0;
  gxString stat_message;
  gxString sbuf;

  // Mount file systems for this host
  listptr = node.filesystems.GetHead();
  while(listptr) {
    fptr = servercfg->node_filesystems.GetHead();
    while(fptr) {
      if(listptr->data == fptr->data.nickname) {
	installed_ptr = installed_filesystems.GetHead();
	is_mounted = 0;
	while(installed_ptr) {
	  if(installed_ptr->data == fptr->data.nickname) {
	    is_mounted = 1;
	    if(servercfg->debug && servercfg->debug_level >= 5) {
	      message << clear << "Checking file system mount " << fptr->data.nickname;
	      LogDebugMessage(message.c_str());
	    }
	    command << clear << fptr->data.resource_script << " " << fptr->data.source << " " << fptr->data.target 
		    << " watch > /dev/null 2>&1";
	    rv = RunSystemCommand(command);
	    if(rv != 0) {
	      if(servercfg->debug && servercfg->debug_level >= 5) {
		message << clear << "ERROR - Resource script reported file system watch error " 
			<< fptr->data.nickname;
		LogMessage(message.c_str());
	      }
	    }
	    break;
	  }
	  installed_ptr = installed_ptr->next;
	}
	if(is_mounted == 0) {
	  message << clear << "Mounting file system " << fptr->data.nickname;
	  LogMessage(message.c_str());
	  command << clear << fptr->data.resource_script << " " << fptr->data.source << " " << fptr->data.target 
		  << " mount >> " << servercfg->log_dir << "/filesystem_resource.log 2>&1";
	  rv = RunSystemCommand(command);
	  if(rv != 0) {
	    message << clear << "ERROR - Resource script could not mount file system " << fptr->data.nickname;
	    LogMessage(message.c_str());
	    has_errors++;
	  }
	  else {
	    installed_filesystems.Add(fptr->data.nickname);
	    stat_message << clear << "filesystem:" << fptr->data.nickname;
	    servercfg->stats.Add(stat_message);
	  }
	}
      }
      fptr = fptr->next;
    }
    listptr = listptr->next;
  }

  // Install or remove backup filesystems if we need to failover or fail back
  ptr = servercfg->nodes.GetHead();
  while(ptr) {
    if(ptr->data->hostname == servercfg->my_hostname) {
      ptr = ptr->next; 
      continue;
    }
    if(ptr->data->CLUSTER_TAKE_OVER_FLAG() == 1) {
      // Check to see if we need to fail over
      listptr = node.backup_filesystems.GetHead();
      while(listptr) {
	gxString failed_nodename = listptr->data;
	failed_nodename.FilterChar(' ');
	failed_nodename.DeleteAfterIncluding(":");
	if(ptr->data->nodename == failed_nodename) {
	  gxString filesystem_nickname = listptr->data;
	  filesystem_nickname.FilterChar(' ');
	  filesystem_nickname.DeleteBeforeLastIncluding(":");
	  fptr = servercfg->node_filesystems.GetHead();
	  while(fptr) {
	    is_mounted = 0;

	    // Check the other cluster nodes to see if they picked the failover resources
	    gxListNode<CMnode *> *takeoverptr = servercfg->nodes.GetHead();
	    gxString rstats_buffer, rstats_error_message;
	    while(takeoverptr) {
	      if(takeoverptr->data->nodename != ptr->data->nodename) { // Don't check the failed node 
		if(takeoverptr->data->hostname == servercfg->my_hostname) { // Dont't check this node
		  takeoverptr = takeoverptr->next;
		  continue;
		}
		if(get_node_stat_buffer(takeoverptr->data, rstats_buffer, rstats_error_message) == 0) {
		  stat_message << clear << "backup_filesystem:" << fptr->data.nickname;
		  if(rstats_buffer.Find(stat_message) != -1) {
		    is_mounted = 1;
		    message << clear << takeoverptr->data->nodename  << " has " 
			    << fptr->data.nickname << " failover file system mounted";
		    LogMessage(message.c_str());
		  }
		}
	      }
	      takeoverptr = takeoverptr->next;
	    }

	    installed_ptr = installed_backup_filesystems.GetHead();
	    while(installed_ptr) {
	      sbuf << clear << ptr->data->nodename << ":" << fptr->data.nickname;
	      if(installed_ptr->data == sbuf) {
		is_mounted = 1;
		if(servercfg->debug && servercfg->debug_level >= 5) {
		  message << clear << "Checking failed over file system mount " << fptr->data.nickname;
		  LogDebugMessage(message.c_str());
		}
		command << clear << fptr->data.resource_script << " " << fptr->data.source << " " << fptr->data.target 
			<< " watch > /dev/null 2>&1";
		rv = RunSystemCommand(command);
		if(rv != 0) {
		  if(servercfg->debug && servercfg->debug_level >= 5) {
		    message << clear << "ERROR - Resource script reported failed over file system watch error " 
			    << fptr->data.nickname;
		    LogDebugMessage(message.c_str());
		  }
		}
		break;
	      }
	      installed_ptr = installed_ptr->next;
	    }
	    if(filesystem_nickname == fptr->data.nickname && is_mounted == 0) {
	      message << clear << ptr->data->nodename << " failover file system mount " << fptr->data.nickname;
	      LogMessage(message.c_str());
	      command << clear << fptr->data.resource_script << " " << fptr->data.source << " " << fptr->data.target 
		      << " mount >> " << servercfg->log_dir << "/filesystem_resource.log 2>&1";
	      rv = RunSystemCommand(command);
	      if(rv != 0) {
		message << clear << "ERROR - Resource script could not mount failover filesystem " 
			<< fptr->data.nickname;
		LogMessage(message.c_str());
		has_errors++;
	      }
	      else {
		sbuf << clear << ptr->data->nodename << ":" << fptr->data.nickname;
		installed_backup_filesystems.Add(sbuf);
		stat_message << clear << "backup_filesystem:" << fptr->data.nickname;
		servercfg->stats.Add(stat_message);
	      }
	    }
	    fptr = fptr->next;
	  }
	}
	listptr = listptr->next;
      }
    }
    else { // Check to see if we need to failback
      installed_ptr = installed_backup_filesystems.GetHead();
      while(installed_ptr) {
	fptr = servercfg->node_filesystems.GetHead();
	while(fptr) {
	  sbuf << clear << ptr->data->nodename << ":" << fptr->data.nickname;
	  if(installed_ptr->data == sbuf) {
	    message << clear << ptr->data->nodename << " failback file system unmount " << fptr->data.nickname;
	    LogMessage(message.c_str());
	    command << clear << fptr->data.resource_script << " " << fptr->data.source << " " << fptr->data.target 
		    << " unmount >> " << servercfg->log_dir << "/filesystem_resource.log 2>&1";
	    rv = RunSystemCommand(command);
	    if(rv != 0) {
	      message << clear << "ERROR - Resource script could not stop failover filesystem" 
		      << fptr->data.nickname;
	      LogMessage(message.c_str());
	      has_errors++;
	    }
	    else {
	      installed_backup_filesystems.Remove(installed_ptr);
	      installed_ptr = 0; // Signal to start at top of installed backup list
	      stat_message << clear << "backup_filesystem:" << fptr->data.nickname;
	      servercfg->stats.Remove(stat_message);
	      break;
	    }
	  }
	  fptr = fptr->next;
	}
	if(!installed_ptr) { // Check to see if the list pointer was removed
	  installed_ptr = installed_backup_filesystems.GetHead();
	  continue;
	}
	installed_ptr = installed_ptr->next;
      }
    }	    
    ptr = ptr->next;
  }

  return has_errors;
}

int CM_filesystemsThread::unmount_all_filesystems()
{
  int has_errors = 0;
  gxListNode <CMfilesystems> *fptr = servercfg->node_filesystems.GetHead();
  gxString command;
  gxString message;

  if(servercfg->debug && servercfg->debug_level >= 5) {
    LogDebugMessage("Unmounting all CM filesystems");
  }

  while(fptr) {
    command << clear << fptr->data.resource_script << " " << fptr->data.source << " " << fptr->data.target 
	    << " unmount >> " << servercfg->log_dir << "/filesystem_resource.log 2>&1";
    if(RunSystemCommand(command) != 0) {
      message << clear << "ERROR - Resource script could not unmount filesystem" 
	      << fptr->data.nickname;
      LogMessage(message.c_str());
      has_errors++;
    }
    fptr = fptr->next;
  }

  if(has_errors == 0) {
    installed_filesystems.ClearList();
    installed_backup_filesystems.ClearList();
  }
  return has_errors;
}


int CM_filesystemsThread::unmount_backup_filesystems()
{
  int has_errors = 0;
  if(set_this_node() != 0) return 1;

  gxListNode <CMfilesystems> *fptr;
  gxString command;
  gxString message;
  gxListNode<gxString> *installed_ptr;
  int rv;

  installed_ptr = installed_backup_filesystems.GetHead();
  while(installed_ptr) {
    fptr = servercfg->node_filesystems.GetHead();
    while(fptr) {
      if(installed_ptr->data == fptr->data.nickname) {
	message << clear << "Unmounting failover file system " << fptr->data.nickname;
	LogMessage(message.c_str());
	command << clear << fptr->data.resource_script << " " << fptr->data.source << " " << fptr->data.target 
		<< " unmount >> " << servercfg->log_dir << "/filesystem_resource.log 2>&1";
	rv = RunSystemCommand(command);
	if(rv != 0) {
	  message << clear << "ERROR - Resource script could not unmount failover filesystem" 
		  << fptr->data.nickname;
	  LogMessage(message.c_str());
	  has_errors++;
	}
	else {
	  installed_backup_filesystems.Remove(installed_ptr);
	  installed_ptr = 0; // Signal to start at top of installed backup list
	  break;
	}
      }
      fptr = fptr->next;
    }
    if(!installed_ptr) { // Check to see if the list pointer was removed
      installed_ptr = installed_backup_filesystems.GetHead();
      continue;
    }
    installed_ptr = installed_ptr->next;
  }
  
  return has_errors;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //


