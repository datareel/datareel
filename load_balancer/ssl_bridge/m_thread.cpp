// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 06/17/2016
// Date Last Modified: 10/19/2016
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

Multi-threaded framework Datareel load balancer.

*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <regex.h>
#include <string.h>
#include <errno.h>

#include "gxstring.h"
#include "gxlist.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include "m_thread.h"
#include "m_log.h"
#include "membuf.h"

#include "ssl_utils.h"
#include "ssl_config.h"

ClientSocket_t::~ClientSocket_t()
{
  FreeSSL();
} 

void ClientSocket_t::FreeSSL() 
{
  if(openssl) {
    openssl->ReleaseCTX(); // Do not free Context by ~gxSSL() call
    delete openssl; 
    openssl = 0;
  }
  if(openssl_client) {
    delete openssl_client;
    openssl_client = 0;
  }
}

int LBServerThread::InitServer(int max_connections)
// Initialize the server. Returns a non-zero value if any errors ocurr.
{

  if(LoadSSLConfig() != 0) {
    StopProc();
    ExitProc(1);
  }

  // Load the SSL key and cert
  if(init_ssl_cert() != 0) {
    StopProc();
    ExitProc(1);
  }

  if(InitSocketLibrary() == 0) {
    if(InitSocket(SOCK_STREAM, servercfg->port) < 0) return 1;
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

  if(!servercfg->listen_ip_addr.is_null()) {
    if(servercfg->listen_ip_addr != "0.0.0.0") {
      if(servercfg->listen_ip_addr != "::0") {
	inet_aton(servercfg->listen_ip_addr.c_str(), &sin.sin_addr);
      }
    }
  }

  // Bind the name to the socket
  if(Bind() < 0) {
    LogMessage("ERROR - Fatal bind() error initializing server");
    CheckSocketError((gxSocket *)this);
    Close();
    return 1;
  }
    
  // Listen for connections with a specified backlog
  if(Listen(max_connections) < 0) {
    LogMessage("ERROR - Fatal listen() error initializing server");
    CheckSocketError((gxSocket *)this);
    Close();
    return 1;
  }

  return 0;
}

void *LBServerThread::ThreadEntryRoutine(gxThread_t *thread)
{
  gxString message, sbuf;
  int accept_retires = 5;
  int error_number = 0;
  int num_start_threads = 4; // main + server_thread + log_thread + process_thread
  if(servercfg->enable_stats) num_start_threads++;
  int thread_count = 0;
  int num_connections;

  while(servercfg->accept_clients) { 
    if(servercfg->debug && servercfg->debug_level >= 5) {
      LogDebugMessage("LB server thread accept() call");
    }

    memset(&remote_sin, 0, sizeof(gxsSocketAddress));
    gxsSocketLength_t addr_size = (gxsSocketLength_t)sizeof(remote_sin); 
    remote_socket = accept(gxsocket, (struct sockaddr *)&remote_sin, &addr_size);
    if(remote_socket < 0) {
      if(servercfg->debug) {
	get_fd_error(error_number, sbuf);
	message << clear << " ENUM: " << error_number << " Message: " << sbuf;
	LogDebugMessage(message.c_str());
	LogDebugMessage("ERROR - LB server thread accept() call failed");
      }
      sSleep(1); // Allow for recovery time
      if(--accept_retires <= 0) {
	get_fd_error(error_number, sbuf);
	message << clear << "ERROR - accept() call failed, ENUM: " << error_number << " Message: " << sbuf;
	LogMessage(message.c_str());
	break;
      }
      continue;
    }
    accept_retires = 5;

    servercfg->CONNECTION_TOTAL(1);

    char client_name[gxsMAX_NAME_LEN]; int r_port = -1;
    memset(client_name, 0, gxsMAX_NAME_LEN);
    GetClientInfo(client_name, r_port);
    // NOTE: gxSocket library was updated to use code below:
    // r_port = ntohs(remote_sin.sin_port);
    // inet_ntop(AF_INET, &remote_sin.sin_addr, client_name, gxsMAX_NAME_LEN);
    int seq_num = r_port;
    if(servercfg->debug || servercfg->log_level >= 1) {
      message << clear << "[" << seq_num << "]: Frontend client connect: " << client_name;
      LogMessage(message.c_str());
    }

    thread_count = 0;
    num_connections = 1; // Count this connection 
    thrPoolNode *ptr = servercfg->client_request_pool->GetHead();
    while(ptr) {
      gxThread_t *thread = ptr->GetThreadPtr();
      if(thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
	thread_count++; // count all the running client threads
	num_connections++; // count all the connected threads
      }
      ptr = ptr->GetNext();
    }

    servercfg->num_client_threads = thread_count;
    int num_threads = num_start_threads + thread_count;
    if(servercfg->debug && servercfg->debug_level >= 5) {
      message << clear << num_threads << " threads working";
      LogDebugMessage(message.c_str());
    }

    servercfg->NUM_SERVER_CONNECTIONS(num_connections);

    if(servercfg->debug && servercfg->debug_level >= 5) {
      message << clear <<  num_connections << " server connections";
      LogDebugMessage(message.c_str());
    }

    if(servercfg->max_threads > 1) {
      if(servercfg->debug && servercfg->debug_level >= 5) {
	message << clear << "Max thread limit is set to " << servercfg->max_threads; 
	LogDebugMessage(message.c_str());
      }
      if(num_threads >= servercfg->max_threads) {
	message << clear << "[" << seq_num << "]: Reached " << num_threads << " max thread limit. " << client_name << " connection denied";
	LogMessage(message.c_str());
	close(remote_socket); remote_socket = -1;
	continue;
      }
    }

    if(servercfg->max_connections > 1) {
      if(servercfg->debug && servercfg->debug_level >= 5) {
	message << clear << "Max frontend connection limit is set to " << servercfg->max_connections; 
	LogDebugMessage(message.c_str());
      }
      if(num_connections >= servercfg->max_connections) {
	message << clear << "[" << seq_num << "]: Reached " << num_connections
		<< " max frontend connection limit. " << client_name << " connection denied";
	LogMessage(message.c_str());
	close(remote_socket); remote_socket = -1;
	continue;
      }
    }

    ClientSocket_t *s = new ClientSocket_t;
    if(!s) {
      get_fd_error(error_number, sbuf);
      message << clear << "[" << seq_num << "]: ERROR - A fatal memory allocation error occurred in main server thread";
      LogMessage(message.c_str());
      message << clear << "[" << seq_num << "]: Fatal memory error ENUM: " << error_number << " Message: " << sbuf;
      LogProcMessage(message.c_str());
      servercfg->accept_clients = 0;
      break;
    }
    
    // Record the file descriptor assigned by the Operating System
    s->client_socket = remote_socket;
    s->seq_num = seq_num;
    s->client_name = client_name;
    s->r_port = r_port;

    SSL_CTX_set_cipher_list(sslcfg->openssl.GetCTX(), "ALL");
 
    s->openssl = new gxSSL(sslcfg->openssl.GetCTX());
    if(!s->openssl) {
      message << clear << "[" << seq_num << "]: ERROR - A fatal memory allocation error occurred in main server thread";
      LogMessage(message.c_str());
      servercfg->accept_clients = 0;
      delete s;
      break;
    }
    if(sslcfg->ssl_encrypt_backend) {
      s->openssl_client = new gxSSL();
      if(!s->openssl_client) {
	message << clear << "[" << seq_num << "]: ERROR - A fatal memory allocation error occurred in main server thread";
	LogMessage(message.c_str());
	servercfg->accept_clients = 0;
	delete s;
	break;
      }
      if((sslcfg->ssl_verify_backend_cert) && (!sslcfg->ssl_backend_hostname.is_null())) {
	s->openssl_client->SetCAList(sslcfg->ssl_backend_ca_list_file.c_str());
      }
    }

    if(servercfg->debug && servercfg->debug_level >= 5) LogDebugMessage("LB server rebuilding thread pool");
    RebuildThreadPool(servercfg->client_request_pool);

    if(servercfg->debug && servercfg->debug_level >= 5) LogDebugMessage("LB server starting client request thread");

    // 08/18/2016: Must set thread type to detached to free thread resources. To watch memory usage under high load:
    // $ watch -n 1 'ps -eo %mem,pid,user,args | grep drlb | grep -v grep'
    // $ watch -n 1 "cat /proc/$(ps -ef | grep drlb_server | grep -v grep | awk '{ print $2 }')/status | grep -i vmsize"
    //
    gxThreadType type = gxTHREAD_TYPE_DETACHED;
    gxThreadPriority prio = gxTHREAD_PRIORITY_NORMAL;
    gxStackSizeType ssize = 2048;
    gxThread_t *rthread = request_thread.CreateThread(servercfg->client_request_pool, (void *)s, prio, type, ssize);
    if(!rthread) {
      get_fd_error(error_number, sbuf);
      message << clear << "[" << seq_num << "]: ERROR - Memory allocation main server thread, cannot allocate client thread";
      LogMessage(message.c_str());
      message << clear << "[" << seq_num << "]: Fatal thread create error ENUM: " << error_number << " Message: " << sbuf;
      LogProcMessage(message.c_str());
      delete s; s = 0;
      break;
    }

    if(CheckThreadError(rthread, seq_num)) {
      get_fd_error(error_number, sbuf);
      message << clear << "[" << seq_num << "]: ERROR - Fatal error starting client thread";
      LogMessage(message.c_str());
      message << clear << "[" << seq_num << "]: " << rthread->ThreadExceptionMessage() << " ENUM: " << error_number << " Message: " << sbuf;
      LogProcMessage(message.c_str());
      delete s; s = 0;
      break;
    }
  }
  servercfg->accept_clients = 0;

  // Send notice that program is ending
  NT_print("NOTICE - Main server thread has exited");
  
  if(servercfg->debug && servercfg->debug_level >= 5) { 
    LogDebugMessage("Server thread exit routine closing connections");
  }
  CloseSocket(); // Close the server side socket

  // Fatal error, we need to tell the process thread to exit the process
  servercfg->kill_server = 1;
  servercfg->process_cond.ConditionSignal(); 
  return (void *)0;
}

void LBServerThread::ThreadCleanupHandler(gxThread_t *thread)
// Thread cleanup handler used in the event that the thread is
// canceled.
{
  servercfg->accept_clients = 0;

  if(servercfg->debug && servercfg->debug_level > 5) { // We should never log from exit/cancel call 
    LogDebugMessage("Server thread clean up handler closing connections");
  }

  CloseSocket(); // Close the server side socket
}

void *LBClientRequestThread::ThreadEntryRoutine(gxThread_t *thread)
{
  ClientSocket_t *s = (ClientSocket_t *)thread->GetThreadParm();
  gxString message;

  if(s) {
    if(servercfg->enable_throttling) {
      int throttle_this_connection = 0;
      int throttle_count = servercfg->THROTTLE_COUNT(1);
      if(throttle_count == 0) {
	if(servercfg->throttle_apply_by_load) {
	  if(servercfg->CONNECTIONS_PER_SECOND() >= servercfg->throttle_connections_per_sec) {
	    throttle_this_connection = 1;
	  }
	} 
	else {
	  throttle_this_connection = 1;
	}
	if(throttle_this_connection) {
	  if(servercfg->debug && servercfg->debug_level >= 5) { 
	    message << clear << "[" << s->seq_num  << "]: Throttling connection";
	    LogDebugMessage(message.c_str());
	  }
	  if(servercfg->throttle_wait_secs >= 1) sSleep(servercfg->throttle_wait_secs);
	  if(servercfg->throttle_wait_usecs >= 1) sSleep(servercfg->throttle_wait_usecs);
	}
      }
    }

    HandleClientRequest(s);

    if(servercfg->debug && servercfg->debug_level >= 5) { 
      message << clear << "[" << s->seq_num 
	      << "]: Client request thread exit routine closing connections";
      LogDebugMessage(message.c_str());
    }

    if(s->node) s->node->NUM_CONNECTIONS(0, 1);
    s->openssl->CloseSSLSocket();
    if(sslcfg->ssl_encrypt_backend && s->openssl_client) s->openssl_client->CloseSSLSocket();
    if(s->client_socket > 0) { close(s->client_socket); s->client_socket = -1; }
    if(s->client.GetSocket() > 0) { close(s->client.GetSocket()); s->client.SetSocket(-1); }
    delete s; s = 0;
  }

  return (void *)0;
}

void LBClientRequestThread::ThreadCleanupHandler(gxThread_t *thread)
// Thread cleanup handler used in the event that the thread is
// canceled.
{
  gxString message;
  ClientSocket_t *s = (ClientSocket_t *)thread->GetThreadParm();
  if(s) {
    if(servercfg->debug && servercfg->debug_level > 5) { // We should never log from exit/cancel call  
      message << clear << "[" << s->seq_num << "]: Client request thread exit routine closing connections";
      LogDebugMessage(message.c_str());
    }
    if(s->node) s->node->NUM_CONNECTIONS(0, 1);
    s->openssl->CloseSSLSocket();
    if(sslcfg->ssl_encrypt_backend && s->openssl_client) s->openssl_client->CloseSSLSocket();
    if(s->client_socket > 0) { close(s->client_socket); s->client_socket = -1; }
    if(s->client.GetSocket() > 0) { close(s->client.GetSocket()); s->client.SetSocket(-1); }
    delete s; s = 0;
  }
}

int get_fd_error(int &error_number, gxString &sbuf)
{
  error_number = errno;
  return fd_error_to_string(error_number, sbuf);
}

void LBClientRequestThread::HandleClientRequest(ClientSocket_t *s)
// Function used to handle a client request.
{
  gxString sbuf;
  gxString message;
  unsigned seq_num = s->seq_num;
  LBnode *n;
  
  if(s->openssl->OpenSSLSocket(s->client_socket) != gxSSL_NO_ERROR) {
    if(servercfg->log_level >= 3) {
      sbuf << clear << "[" << seq_num << "]: Error opening frontend SSL socket";
      LogMessage(sbuf.c_str());
      sbuf << clear << "[" << seq_num << "]: " << s->openssl->SSLErrorMessage();
      LogMessage(sbuf.c_str());
    }
    return;
  }
  
  if(s->openssl->AcceptSSLSocket() != gxSSL_NO_ERROR) {
    if(servercfg->log_level >= 3) {
      sbuf << clear << "[" << seq_num << "]: Error accepting frontend SSL socket";
      LogMessage(sbuf.c_str());
      sbuf << clear << "[" << seq_num << "]: " << s->openssl->SSLErrorMessage();
      LogMessage(sbuf.c_str());
      gxString err;
      gxSSL::GlobalSSLError(err);
      sbuf << clear << "[" << seq_num << "]: " << err;
      LogMessage(sbuf.c_str());
    }
    return;
  }

  switch (servercfg->scheme) {
    case LB_ASSIGNED:
      n = assigned(s);
      if(!n) {
	switch(servercfg->assigned_default) {
	  case LB_RR:
	    if(servercfg->debug && servercfg->debug_level >= 5) {
	      sbuf << clear << "[" << seq_num << "]: LB assigned will default to round robin";
	      LogDebugMessage(sbuf.c_str());
	    }
	    n = round_robin(s);
	    break;
	  case LB_DISTRIB:
	    if(servercfg->debug && servercfg->debug_level >= 5) {
	      sbuf << clear << "[" << seq_num << "]: LB assigned will fail over to distributed";
	      LogDebugMessage(sbuf.c_str());
	    }
	    n = distrib(s);
	    break;
	  case LB_WEIGHTED:
	    if(servercfg->debug && servercfg->debug_level >= 5) {
	      sbuf << clear << "[" << seq_num << "]: LB assigned will fail over to weighted";
	      LogDebugMessage(sbuf.c_str());
	    }
	    n = weighted(s);
	    break;
	  case LB_NONE:
	    if(servercfg->debug && servercfg->debug_level >= 5) {
	      sbuf << clear << "[" << seq_num << "]: LB assigned will not fail over to another node";
	      LogDebugMessage(sbuf.c_str());
	    }
	    return;
	  default:
	    if(servercfg->debug && servercfg->debug_level >= 5) {
	      sbuf << clear << "[" << seq_num << "]: LB assigned will default to round robin";
	      LogDebugMessage(sbuf.c_str());
	    }
	    n = round_robin(s);
	    break;
	}
      }
      break;
    case LB_DISTRIB:
      n = distrib(s);
      break;
    case LB_RR:
      n = round_robin(s);
      break;
    case LB_WEIGHTED:
      n = weighted(s);
      break;
    default:
      n = round_robin(s);
      break;
  }

  if(!n) return; // No backend nodes ready

  s->node = n;
  n->NUM_CONNECTIONS(1);
  n->CONNECTION_TOTAL(1);

  if(servercfg->debug && servercfg->debug_level >= 5) {
    int num_connections = n->NUM_CONNECTIONS();
    message << clear << "[" << seq_num << "]: " << n->node_name << " has " << num_connections << " client connections";
    LogDebugMessage(message.c_str());
  }

  if(servercfg->use_buffer_cache) {
    LB_CachedReadWrite(s, n->buffer_size);
  }
  else {
    LB_ReadWrite(s, n->buffer_size);
  }

  shutdown(s->client_socket, SHUT_RDWR);
  shutdown(s->client.GetSocket(), SHUT_RDWR);

  if(servercfg->debug && servercfg->debug_level >= 5) {
    message << clear << "[" << seq_num << "]: Client has disconnected, exiting client request thread";
    LogDebugMessage(message.c_str());
  }

  return;
}

int LBClientRequestThread::LB_ReadWrite(ClientSocket_t *s, int buffer_size)
{
  gxString sbuf, message;
  unsigned seq_num = s->seq_num;
  char *buffer = new char[buffer_size];
  int br, bm, flags1, ready, error_number;
  int error_level = 0;
  int idle_count = 0;

  while(servercfg->accept_clients) {
    if(servercfg->use_timeout && (!ReadSelect(s->client_socket, servercfg->timeout_secs, servercfg->timeout_usecs))) {
      if(servercfg->debug || servercfg->log_level >= 4) {
	message << clear << "[" << seq_num << "]: Frontend read socket timeout after " 
		<< servercfg->timeout_secs << " secs " << servercfg->timeout_usecs << " usecs";
	LogMessage(message.c_str());
      }
      ready = 0;
    }
    else {
      if(!servercfg->use_nonblock_sockets) {
	idle_count++;
	if(idle_count >= servercfg->max_idle_count) ReadSelect(s->client_socket, servercfg->idle_wait_secs, servercfg->idle_wait_usecs);
      }
      flags1 = fcntl(s->client_socket, F_GETFL, 0);
      fcntl(s->client_socket, F_SETFL, flags1 | O_NONBLOCK); // Set read not to block
      ready = recv(s->client_socket, buffer, buffer_size, MSG_PEEK); 
      if(errno == EAGAIN) { // EAGAIN and EWOULDBLOCK are the same value
	errno = 0;
      }
      fcntl(s->client_socket, F_SETFL, flags1); // Set socket back to blocking read
    }
    if(ready > 0) {
      idle_count = 0;
      br = s->openssl->RawReadSSLSocket(buffer, buffer_size);
      if(servercfg->debug && servercfg->debug_level >= 5) { 
	get_fd_error(error_number, sbuf);
	message << clear << "[" << seq_num << "]: " << "Frontend read() Return: " << br << " ENUM: " << error_number << " Message: " << sbuf;
	LogMessage(message.c_str());
      }
      if(br == 0) {
	if(servercfg->debug && servercfg->debug_level >= 5) { 
	  message << clear << "[" << seq_num << "]: " << "Frontend socket disconnected duing read()";
	  LogMessage(message.c_str());
	}
	error_level = 1;
	break;
      }
      if(br < 0) {
	if(servercfg->log_level >= 3) {
	  message << clear << "[" << seq_num << "]: Error reading from Frontend socket";
	  LogMessage(message.c_str());
	  message << clear << "[" << seq_num << "]: " << s->openssl->SSLErrorMessage();
	  LogMessage(message.c_str());
	}
	error_level = 1;
	break;
      }
      if(sslcfg->ssl_encrypt_backend && s->openssl_client) {
	bm = s->openssl_client->SendSSLSocket(buffer, br);
	if(s->openssl_client->HasError()) {
	  if(servercfg->log_level >= 3) {
	    message << clear << "[" << seq_num << "]: Error writing to Backend SSL socket";
	    LogMessage(message.c_str());
	    message << clear << "[" << seq_num << "]: " << s->openssl_client->SSLErrorMessage();
	    LogMessage(message.c_str());
	  }
	  error_level = 1;
	  break;
	}
      }
      else {
	bm = write(s->client.GetSocket(), buffer, br);
      }
      if(servercfg->debug && servercfg->debug_level >= 5) { 
	get_fd_error(error_number, sbuf);
	message << clear << "[" << seq_num << "]: " << "Backend write() Return: " << bm << " ENUM: " << error_number << " Message: " << sbuf;
	LogDebugMessage(message.c_str());
      }
      if(bm < 0) {
	if(servercfg->log_level >= 3) {
	  message << clear << "[" << seq_num << "]: Error writing to Backend socket";
	  LogMessage(message.c_str());
	}
	error_level = 1;
	break;
      }
      if(bm != br) {
	if(servercfg->log_level >= 3 || servercfg->verbose_level >= 3) {
	  message << clear << "[" << seq_num << "]: Failed to write " << br << " bytes to Backend socket, wrote " << bm;
	  LogMessage(message.c_str());
	}
      }
    }
    if(ready == 0) break;

    if(servercfg->use_timeout && (!ReadSelect(s->client.GetSocket(), servercfg->timeout_secs, servercfg->timeout_usecs))) {
      if(servercfg->debug || servercfg->log_level >= 4) {
	message << clear << "[" << seq_num << "]: Backend read socket timeout after " 
		<< servercfg->timeout_secs << " secs " << servercfg->timeout_usecs << " usecs";
	LogMessage(message.c_str());
      }
      ready = 0;
    }
    else {
      flags1 = fcntl(s->client.GetSocket(), F_GETFL, 0);
      fcntl(s->client.GetSocket(), F_SETFL, flags1 | O_NONBLOCK); // Set read not to block
      ready = recv(s->client.GetSocket(), buffer, buffer_size, MSG_PEEK); 
      if(errno == EAGAIN) { // EAGAIN and EWOULDBLOCK are the same value
	errno = 0;
      }
      fcntl(s->client.GetSocket(), F_SETFL, flags1); // Set socket back to blocking read
    }
    if(ready > 0) {
      idle_count = 0;
      if(sslcfg->ssl_encrypt_backend && s->openssl_client) {
	br = s->openssl_client->RawReadSSLSocket(buffer, buffer_size);
      }
      else {
	br = read(s->client.GetSocket(), buffer, buffer_size);
      }
      if(servercfg->debug && servercfg->debug_level >= 5) { 
	get_fd_error(error_number, sbuf);
	message << clear << "[" << seq_num << "]: " << "Backend read() Return: " << br << " ENUM: " << error_number << " Message: " << sbuf;
	LogMessage(message.c_str());
      }
      if(br == 0) {
	if(servercfg->debug && servercfg->debug_level >= 5) { 
	  message << clear << "[" << seq_num << "]: " << "Backend socket disconnected duing read()";
	  LogMessage(message.c_str());
	}
	error_level = 1;
	break;
      }
      if(br < 0) {
	if(servercfg->log_level >= 3) {
	  message << clear << "[" << seq_num << "]: Error reading from Backend socket";
	  LogMessage(message.c_str());
	}
	error_level = 1;
	break;
      }
      bm = s->openssl->SendSSLSocket(buffer, br);
      if(s->openssl->HasError()) {
	if(s->openssl->GetSSLError() == gxSSL_DISCONNECTED_ERROR) {
	  if(servercfg->log_level >= 3) {
	    message << clear << "[" << seq_num << "]: Error writing to Frontend SSL socket, connection is closed";
	    LogMessage(message.c_str());
	    message << clear << "[" << seq_num << "]: " << s->openssl->SSLErrorMessage();
	    LogMessage(message.c_str());
	  }
	  error_level = 1;
	  break;
	}
	else {
	  if(servercfg->log_level >= 3) {
	    message << clear << "[" << seq_num << "]: Error writing to Frontend SSL socket";
	    LogMessage(message.c_str());
	    message << clear << "[" << seq_num << "]: " << s->openssl->SSLErrorMessage();
	    LogMessage(message.c_str());
	  }
	  error_level = 1;
	  break;
	}
      }
      if(servercfg->debug && servercfg->debug_level >= 5) { 
	get_fd_error(error_number, sbuf);
	message << clear << "[" << seq_num << "]: " << "Frontend write() Return: " << bm << " ENUM: " << error_number << " Message: " << sbuf;
	LogDebugMessage(message.c_str());
      }
      if(bm < 0) {
	if(servercfg->log_level >= 3) {
	  message << clear << "[" << seq_num << "]: Error writing to Frontend socket";
	  LogMessage(message.c_str());
	}
	error_level = 1;
	break;
      }
      if(bm != br) {
	if(servercfg->log_level >= 3 || servercfg->verbose_level >= 3) {
	  message << clear << "[" << seq_num << "]: Failed to write " << br << " bytes to Frontend socket, wrote " << bm;
	  LogMessage(message.c_str());
	}
      }
    }
    if(ready == 0) break;
  }

  delete buffer;
  return error_level;
}

int LBClientRequestThread::LB_CachedReadWrite(ClientSocket_t *s, int buffer_size)
{
  gxString sbuf, message;
  unsigned seq_num = s->seq_num;
  char *buffer = new char[buffer_size];
  int br, bm, flags1, ready, error_number, bytes_read, bytes_moved;
  int error_level = 0;
  gxList<MemoryBuffer *> cache;
  gxListNode<MemoryBuffer *> *ptr = 0;
  int idle_count = 0;
  unsigned i;

  while(servercfg->accept_clients) {
    ready = 1;
    bytes_read = bytes_moved = 0;
    if(!servercfg->use_nonblock_sockets) {
      idle_count++;
      if(idle_count >= servercfg->max_idle_count) ReadSelect(s->client_socket, servercfg->idle_wait_secs, servercfg->idle_wait_usecs);
    }
    while(ready > 0) {
      flags1 = fcntl(s->client_socket, F_GETFL, 0);
      fcntl(s->client_socket, F_SETFL, flags1 | O_NONBLOCK); // Set read not to block
      ready = recv(s->client_socket, buffer, buffer_size, MSG_PEEK); 
      if(errno == EAGAIN) { // EAGAIN and EWOULDBLOCK are the same value
	errno = 0;
      }
      fcntl(s->client_socket, F_SETFL, flags1); // Set socket back to blocking read
      if(ready > 0) {
	idle_count = 0;
	br = s->openssl->RawReadSSLSocket(buffer, buffer_size);
	if(servercfg->debug && servercfg->debug_level >= 5) { 
	  get_fd_error(error_number, sbuf);
	  message << clear << "[" << seq_num << "]: Cached Frondend read() Return: " << br << " ENUM: " << error_number << " Message: " << sbuf;
	  LogDebugMessage(message.c_str());
	}
	if(br == 0) {
	  if(servercfg->debug && servercfg->debug_level >= 5) { 
	    message << clear << "[" << seq_num << "]: Frontend socket disconnected duing a cached read()";
	    LogMessage(message.c_str());
	  }
	  bytes_read = 0;
	  error_level = 1;
	  break;
	}
	if(br < 0) {
	  if(servercfg->log_level >= 3) {
	    message << clear << "[" << seq_num << "]: Error reading from Frontend client socket";
	    LogMessage(message.c_str());
	    message << clear << "[" << seq_num << "]: " << s->openssl->SSLErrorMessage();
	    LogMessage(message.c_str());
	  }
	  bytes_read = 0;
	  error_level = 1;
	  break;
	}

	MemoryBuffer *inbuf = new MemoryBuffer(buffer, br);
	cache.Add(inbuf);
	bytes_read += br;

	if(servercfg->enable_buffer_overflow_detection) {
	  if(bytes_read >= servercfg->buffer_overflow_size) {
	    if(servercfg->log_level >= 3) {
	      message << clear << "[" << seq_num << "]: Frontend read " << bytes_read << " bytes from client";
	      LogMessage(message.c_str());
	      message << clear << "[" << seq_num << "]: Closing Frontend connection due to buffer overflow";
	      LogMessage(message.c_str());
	    }
	    error_level = 1;
	    break;
	  }
	}
      }
    }

    if(error_level != 0) break;

    if(bytes_read > 0) {
      if(servercfg->debug && servercfg->debug_level >= 5) { 
	message << clear << "[" << seq_num << "]: Cached Frondend bytes read: " << bytes_read;
	LogDebugMessage(message.c_str());
      }
      ptr = cache.GetHead();
      while(ptr) {
	if(servercfg->enable_http_forwarding) {
	  for(i = 0; i < servercfg->num_http_requests; i++) {
	    if(ptr->data->Find((char *)servercfg->http_requests[i].c_str(), servercfg->http_requests[i].length(), 0) != __MEMBUF_NO_MATCH__) {
	      if((ptr->data->Find((char *)servercfg->http_hdr_str.c_str(), servercfg->http_hdr_str.length(), 0) != __MEMBUF_NO_MATCH__) &&
		 (ptr->data->Find((char *)servercfg->http_eol.c_str(), servercfg->http_eol.length(), 0) != __MEMBUF_NO_MATCH__)) {
		gxString ff;
		ff << clear << servercfg-> http_forward_for_str << ": " << s->client_name << "\r\n";
		unsigned offset = ptr->data->Find((char *)servercfg->http_eoh.c_str(), servercfg->http_eoh.length(), 0);
		if(offset == __MEMBUF_NO_MATCH__) {
		  ptr->data->Cat((void *)ff.c_str(), ff.length());
		}
		else {
		  ptr->data->InsertAt((offset+2), (void *)ff.c_str(), ff.length());
		}
	      }
	    }
	  }
	}
	if(sslcfg->ssl_encrypt_backend && s->openssl_client) {
	  bm = s->openssl_client->SendSSLSocket(ptr->data->m_buf(), ptr->data->length());
	  if(s->openssl_client->HasError()) {
	    if(servercfg->log_level >= 3) {
	      message << clear << "[" << seq_num << "]: Error writing to Backend SSL socket";
	      LogMessage(message.c_str());
	      message << clear << "[" << seq_num << "]: " << s->openssl_client->SSLErrorMessage();
	      LogMessage(message.c_str());
	    }
	    error_level = 1;
	    break;
	  }
	}
	else {
	  bm = write(s->client.GetSocket(), ptr->data->m_buf(), ptr->data->length());
	}
	get_fd_error(error_number, sbuf);
	if(servercfg->debug && servercfg->debug_level >= 5) { 
	  message << clear << "[" << seq_num << "]: Backend cached write() Return: " << bm << " ENUM: " << error_number << " Message: " << sbuf;
	  LogDebugMessage(message.c_str());
	}
	if(bm > 0) bytes_moved += bm;
	if(bm < 0 && error_number == EAGAIN) { // EAGAIN and EWOULDBLOCK are the same value
	  bm = bytes_moved;
	  break;
	}
	if(bm < 0) {
	  if(servercfg->log_level >= 3) {
	    message << clear << "[" << seq_num << "]: Error writing to Backend socket";
	    LogMessage(message.c_str());
	    if(servercfg->debug && servercfg->debug_level >= 5) { 
	      message << clear << "[" << seq_num << "]: Backend write " << bm << " ERROR: " << error_number << ": " << sbuf;
	      LogMessage(message.c_str());
	    }
	    error_level = 1;
	    break;
	  }
	}
	if(error_level != 0) break;
	ptr = ptr->next;
      }
      if(error_level != 0) break;
      if(servercfg->debug && servercfg->debug_level >= 5) { 
	message << clear << "[" << seq_num << "]: Cached Backend bytes moved: " << bytes_moved;
	LogDebugMessage(message.c_str());
      }
      ptr = cache.GetHead();
      while(ptr) {
	delete ptr->data;
	ptr->data = 0;
	ptr = ptr->next;
      }
      cache.ClearList();
    }
    if(error_level != 0) break;
    if(ready == 0) break;

    ready = 1;
    bytes_read = bytes_moved = 0;
    while(ready > 0) {
      flags1 = fcntl(s->client.GetSocket(), F_GETFL, 0);
      fcntl(s->client.GetSocket(), F_SETFL, flags1 | O_NONBLOCK); // Set read not to block
      ready = recv(s->client.GetSocket(), buffer, buffer_size, MSG_PEEK); 
      if(errno == EAGAIN) { // EAGAIN and EWOULDBLOCK are the same value
	errno = 0;
      }
      fcntl(s->client.GetSocket(), F_SETFL, flags1); // Set socket back to blocking read
      if(ready > 0) {
	idle_count = 0;
	if(sslcfg->ssl_encrypt_backend && s->openssl_client) {
	  br = s->openssl_client->RawReadSSLSocket(buffer, buffer_size);
	}
	else {
	  br = read(s->client.GetSocket(), buffer, buffer_size);
	}
	if(servercfg->debug && servercfg->debug_level >= 5) { 
	  get_fd_error(error_number, sbuf);
	  message << clear << "[" << seq_num << "]: Cached Backend read() Return: " << br << " ENUM: " << error_number << " Message: " << sbuf;
	  LogDebugMessage(message.c_str());
	}
	if(br == 0) {
	  if(servercfg->debug && servercfg->debug_level >= 5) { 
	    message << clear << "[" << seq_num << "]: Backend socket disconnected duing a cached read()";
	    LogMessage(message.c_str());
	  }
	  bytes_read = 0;
	  error_level = 1;
	  break;
	}
	if(br < 0) {
	  if(servercfg->log_level >= 3) {
	    message << clear << "[" << seq_num << "]: Error reading from Backend client socket";
	    LogMessage(message.c_str());
	  }
	  bytes_read = 0;
	  error_level = 1;
	  break;
	}
	MemoryBuffer *inbuf = new MemoryBuffer(buffer, br);
	cache.Add(inbuf);
	bytes_read += br;
      }
    }

    if(error_level != 0) break;

    if(bytes_read > 0) {
      if(servercfg->debug && servercfg->debug_level >= 5) { 
	message << clear << "[" << seq_num << "]: Cached Backend bytes read: " << bytes_read;
	LogDebugMessage(message.c_str());
      }
      ptr = cache.GetHead();
      while(ptr) {
	bm = s->openssl->SendSSLSocket(ptr->data->m_buf(), ptr->data->length());
	if(s->openssl->HasError()) {
	  if(s->openssl->GetSSLError() == gxSSL_DISCONNECTED_ERROR) {
	    if(servercfg->log_level >= 3) {
	      message << clear << "[" << seq_num << "]: Error writing to Frontend SSL socket, connection is closed";
	      LogMessage(message.c_str());
	      message << clear << "[" << seq_num << "]: " << s->openssl->SSLErrorMessage();
	      LogMessage(message.c_str());
	    }
	    error_level = 1;
	    break;
	  }
	  else {
	    if(servercfg->log_level >= 3) {
	      message << clear << "[" << seq_num << "]: Error writing to Frontend SSL socket";
	      LogMessage(message.c_str());
	      message << clear << "[" << seq_num << "]: " << s->openssl->SSLErrorMessage();
	      LogMessage(message.c_str());
	    }
	    error_level = 1;
	    break;
	  }
	}
	get_fd_error(error_number, sbuf);
	if(servercfg->debug && servercfg->debug_level >= 5) { 
	  message << clear << "[" << seq_num << "]: Frontend cached write() Return: " << bm << " ENUM: " << error_number << " Message: " << sbuf;
	  LogDebugMessage(message.c_str());
	}
	if(bm > 0) bytes_moved += bm;
	if(bm < 0 && error_number == EAGAIN) { // EAGAIN and EWOULDBLOCK are the same value
	  bm = bytes_moved;
	  break;
	}
	if(bm < 0) {
	  if(servercfg->log_level >= 3) {
	    message << clear << "[" << seq_num << "]: Error writing to Frontend socket";
	    LogMessage(message.c_str());
	    if(servercfg->debug && servercfg->debug_level >= 5) { 
	      message << clear << "[" << seq_num << "]: Frontend write " << bm << " ERROR: " << error_number << ": " << sbuf;
	      LogMessage(message.c_str());
	    }
	    error_level = 1;
	    break;
	  }
	}
	if(error_level != 0) break;
	ptr = ptr->next;
      }
      if(error_level != 0) break;
      if(servercfg->debug && servercfg->debug_level >= 5) { 
	message << clear << "[" << seq_num << "]: Cached Frontend bytes moved: " << bytes_moved;
	LogDebugMessage(message.c_str());
      }
      ptr = cache.GetHead();
      while(ptr) {
	delete ptr->data;
	ptr->data = 0;
	ptr = ptr->next;
      }
      cache.ClearList();
    }
    if(error_level != 0) break;
    if(ready == 0) break;
  }

  ptr = cache.GetHead();
  while(ptr) {
    delete ptr->data;
    ptr->data = 0;
    ptr = ptr->next;
  }
  cache.ClearList();
  delete buffer;
  return error_level;
}

LBnode *LBClientRequestThread::round_robin(ClientSocket_t *s)
{
  return round_robin(s, servercfg->nodes);
}

LBnode *LBClientRequestThread::round_robin(ClientSocket_t *s,  gxList<LBnode *> &node_list)
{
  gxString sbuf;
  gxString message;
  unsigned seq_num = s->seq_num;
  gxListNode<LBnode *> *ptr = node_list.GetHead();
  unsigned count = 0;
  unsigned retries = servercfg->retries;
  unsigned retry_wait = servercfg->retry_wait;
  unsigned error_flag = 0;
  unsigned num_connects = 0;
  unsigned num_nodes = servercfg->NUM_NODES();
  gxSocket *client = &s->client;
  int has_ssl_error = 0;

  while(ptr) {
    if(ptr->data->LB_FLAG() == 0 && servercfg->check_node_max_clients(ptr->data, seq_num) == 0) {
      if(servercfg->log_level >= 2) {
	sbuf << clear << "[" << seq_num << "]: " << ptr->data->node_name << " RR connect to " << ptr->data->hostname 
	     << " port " << ptr->data->port_number;
	LogMessage(sbuf.c_str());
      }
      client->Clear();
      int init_error = 0;
      if(client->InitSocket(SOCK_STREAM, ptr->data->port_number,  ptr->data->hostname.c_str()) < 0) {
	if(servercfg->log_level >= 1) {
	  sbuf << clear << "[" << seq_num << "]: ERROR - " << ptr->data->node_name << " cannot open connection to backend host " << ptr->data->hostname 
	       << " port " << ptr->data->port_number;
	  LogMessage(sbuf.c_str());
	  CheckSocketError(client, seq_num);
	}
	ptr->data->LB_FLAG(0);
	error_flag++;
	init_error = 1;
      }
      if(init_error == 0) {
	if(client->Connect() < 0) {
	  if(servercfg->log_level >= 1) {
	    sbuf << clear << "[" << seq_num << "]: ERROR - " << ptr->data->node_name << " cannot connect to backend host " 
		 << ptr->data->hostname << " port " << ptr->data->port_number;
	    LogMessage(sbuf.c_str());
	    CheckSocketError(client, seq_num);
	  }
	  if(client->GetSocket() > 0) { client->Close(); client->SetSocket(-1); }
	  ptr->data->LB_FLAG(0);
	  error_flag++;
	}
	else {
	  if(sslcfg->ssl_encrypt_backend && s->openssl_client) {
	    has_ssl_error = 0;
	    if(s->openssl_client->OpenSSLSocket(s->client.GetSocket()) != gxSSL_NO_ERROR) {
	      if(servercfg->log_level >= 3) {
		sbuf << clear << "[" << seq_num << "]: Error opening backend SSL socket";
		LogMessage(sbuf.c_str());
		sbuf << clear << "[" << seq_num << "]: " << s->openssl_client->SSLErrorMessage();
		LogMessage(sbuf.c_str());
	      }
	      has_ssl_error = 1;
	    }
	    if(!has_ssl_error) {
	      if(s->openssl_client->ConnectSSLSocket() != gxSSL_NO_ERROR) {
		if(servercfg->log_level >= 3) {
		  sbuf << clear << "[" << seq_num << "]: Error connecting to backend SSL socket";
		  LogMessage(sbuf.c_str());
		  sbuf << clear << "[" << seq_num << "]: " << s->openssl_client->SSLErrorMessage();
		  LogMessage(sbuf.c_str());
		}
		has_ssl_error = 1;
	      }
	    }
	    if(!has_ssl_error) {
	      if((sslcfg->ssl_verify_backend_cert) && (!sslcfg->ssl_backend_hostname.is_null())) {
		if(s->openssl_client->VerifyCert(sslcfg->ssl_backend_hostname.c_str()) != gxSSL_NO_ERROR) {
		  sbuf << clear << "[" << seq_num << "]: SSL cert verify error for " << sslcfg->ssl_backend_hostname;
		  LogMessage(sbuf.c_str());
		  sbuf << clear << "[" << seq_num << "]: " << s->openssl_client->SSLErrorMessage();
		  LogMessage(sbuf.c_str());
		  has_ssl_error = 1;
		}
	      }
	    }
	    if(has_ssl_error) {
	      s->openssl_client->CloseSSLSocket();
	      if(client->GetSocket() > 0) { client->Close(); client->SetSocket(-1); }
	      ptr->data->LB_FLAG(0);
	      error_flag++;
	    }
	    else {
	      num_connects++;
	      error_flag = 0;
	    }
	  }
	  else {
	    num_connects++;
	    error_flag = 0;
	  }
	}
      }
      if(error_flag == 0) {
	ptr->data->LB_FLAG(1);
	error_flag = 0;
	break;
      }
    }
    count++;
    if((count == num_nodes) && (num_connects == 0)) {
      if(retries == 0) {
	error_flag = 1;
	break;
      }
      count = 0;
      retries--;
      if(error_flag == num_nodes) {
	sbuf << clear << "[" << seq_num << "]: ERROR - All connection attempts failed, retry attempt number " << retries;
	LogMessage(sbuf.c_str());
	error_flag = 0;
	if(retry_wait > 0) {
	  sbuf << clear << "[" << seq_num << "]: ERROR - All node reporting errors. waiting " << retry_wait << " seconds";
	  LogMessage(sbuf.c_str());
	  sSleep(retry_wait);
	}
      }
      ptr = servercfg->nodes.GetHead();
      while(ptr) {
	ptr->data->LB_FLAG(0);
	ptr = ptr->next;
      }
      ptr = servercfg->nodes.GetHead();
      continue;
    }
    ptr = ptr->next;
  }

  if(error_flag != 0) {
    message << clear << "[" << seq_num << "]: ERROR - All connection attempts failed";
    LogMessage(message.c_str());
    return 0;
  }

  if(ptr) return ptr->data;

  return 0;
}

LBnode *LBClientRequestThread::assigned(ClientSocket_t *s)
{
  gxString sbuf;
  gxString message;
  unsigned seq_num = s->seq_num;
  unsigned error_level = 0;
  gxSocket *client = &s->client;
  gxString client_name = s->client_name;
  regex_t regex;
  int reti;
  char mbuf[255];
  gxListNode<LB_rule *> *lptr = servercfg->rules_config_list.GetHead();
  error_level = 0;

  while(lptr) {
    if(lptr->data->rule == LB_ROUTE) {
      reti = regcomp(&regex, lptr->data->front_end_client_ip.c_str(), REG_EXTENDED|REG_NOSUB);
      if(reti != 0) { // This should have been checked in config read
	if(servercfg->debug && servercfg->debug_level >= 5) {
	  sbuf << clear << "[" << seq_num << "]: ERROR - Bad regular expression in rules file: " << lptr->data->front_end_client_ip.c_str();
	  LogDebugMessage(sbuf.c_str());
	}
      }
      else {
	reti = regexec(&regex, s->client_name.c_str(), 0, NULL, 0);
	regfree(&regex);
	if(reti == 0) {
	  if(servercfg->debug && servercfg->debug_level >= 5) {
	    sbuf << clear << "[" << seq_num << "]: INFO - Frontend IP matches regex: " << s->client_name << " match " << lptr->data->front_end_client_ip;
	    LogDebugMessage(sbuf.c_str());
	  }
	  error_level = 0;
	  break;
	}
	else if(reti == REG_NOMATCH && servercfg->resolve_assigned_hostnames) {
	  gxString hostname_str, error_message;
	  if(ip_to_hostname(s->client_name, hostname_str, error_message) == 0) {
	    if((s->client_name != hostname_str) && (!hostname_str.is_null())) { 
	      regcomp(&regex, lptr->data->front_end_client_ip.c_str(), REG_EXTENDED|REG_NOSUB|REG_ICASE);
	      reti = regexec(&regex, hostname_str.c_str(), 0, NULL, 0);
	      regfree(&regex);
	      if(reti == 0) {
		if(servercfg->debug && servercfg->debug_level >= 5) {
		  sbuf << clear << "[" << seq_num << "]: INFO - Frontend hostname matches regex: " << hostname_str << " match " << lptr->data->front_end_client_ip;
		  LogDebugMessage(sbuf.c_str());
		}
		error_level = 0;
		break;
	      }
	      else {
		if(servercfg->debug && servercfg->debug_level >= 5) {
		  sbuf << clear << "[" << seq_num << "]: INFO - Frontend IP/hostname does not match regex: " 
		       << s->client_name << "/" << hostname_str << " no match " << lptr->data->front_end_client_ip;
		  LogDebugMessage(sbuf.c_str());
		}
		error_level = 0;
	      }
	    }
	    else {
	      if(servercfg->debug && servercfg->debug_level >= 5) {
		sbuf << clear << "[" << seq_num << "]: INFO - Frontend IP does not match regex: " << s->client_name << " no match " << lptr->data->front_end_client_ip;
		LogDebugMessage(sbuf.c_str());
	      }
	      error_level = 0;
	    }
	  }
	  else {
	    if(servercfg->debug && servercfg->debug_level > 3) {
	      sbuf << clear << "[" << seq_num << "]: ERROR - Get hostname error for IP " << s->client_name << " " << error_message;
	      LogDebugMessage(sbuf.c_str());
	    }
	    error_level = 0;
	  }
	}
	else if(reti == REG_NOMATCH) {
	  if(servercfg->debug && servercfg->debug_level >= 5) {
	    sbuf << clear << "[" << seq_num << "]: INFO - Frontend IP does not match regex: " << s->client_name << " no match " << lptr->data->front_end_client_ip;
	    LogDebugMessage(sbuf.c_str());
	  }
	  error_level = 0;
	}
	else {
	  regerror(reti, &regex, mbuf, sizeof(mbuf));
	  if(servercfg->debug && servercfg->debug_level >= 5) {
	    sbuf << clear << "[" << seq_num << "]: ERROR - Regex match failed: " << mbuf;
	    LogDebugMessage(sbuf.c_str());
	  }
	  error_level = 1;
	}
      }
    }
    lptr = lptr->next;
  }

  if(!lptr) {
    if(servercfg->debug && servercfg->debug_level >= 5) {
      if(error_level != 0) {
	if(servercfg->debug && servercfg->debug_level >= 5) {
	  sbuf << clear << "[" << seq_num << "]: ERROR - Parsing errors reading " << servercfg->rules_config_file;
	  LogDebugMessage(sbuf.c_str());
	}
      }
      else {
	if(servercfg->debug && servercfg->debug_level >= 5) {
	  sbuf << clear << "[" << seq_num << "]: INFO - No matching rules in config file " << servercfg->rules_config_file;
	  LogDebugMessage(sbuf.c_str());
	}
      }
    }
    return 0;
  }
  
  gxListNode<LBnode *> *ptr = servercfg->nodes.GetHead();
  while(ptr) {
    if(ptr->data->node_name == lptr->data->node_name) {
      if(servercfg->debug && servercfg->debug_level >= 5) {
	sbuf << clear << "[" << seq_num << "]: Assigned node " << ptr->data->node_name << " matches rule " << lptr->data->front_end_client_ip;
	LogDebugMessage(sbuf.c_str());
      }
      break;
    }
    ptr = ptr->next;
  }  

  if(!ptr) {
   if(servercfg->debug && servercfg->debug_level >= 2 ) {
     sbuf << clear << "[" << seq_num << "]: Rule config file error no LB nodes matchs rule set";
     LogMessage(sbuf.c_str());
   }
  }

  if(ptr) {
    if(servercfg->log_level >= 2) {
      sbuf << clear << "[" << seq_num << "]: " << ptr->data->node_name << " Assigned connect to " << ptr->data->hostname 
	   << " port " << ptr->data->port_number;
      LogMessage(sbuf.c_str());
    }
    if(servercfg->check_node_max_clients(ptr->data, seq_num) != 0) return 0;
    client->Clear();
    if(client->InitSocket(SOCK_STREAM, ptr->data->port_number,  ptr->data->hostname.c_str()) < 0) {
      if(servercfg->log_level >= 1) {
	sbuf << clear << "[" << seq_num << "]: ERROR - " << ptr->data->node_name << " cannot open TCP socket to " << ptr->data->hostname 
	     << " port " << ptr->data->port_number;
	LogMessage(sbuf.c_str());
	CheckSocketError(client, seq_num);
      }
      return 0;
    }
    if(client->Connect() < 0) {
      sbuf << clear << "[" << seq_num << "]: ERROR - " << ptr->data->node_name << " cannot connect to " 
	   << ptr->data->hostname << " port " << ptr->data->port_number;
      LogMessage(sbuf.c_str());
      CheckSocketError(client, seq_num);
      if(client->GetSocket() > 0) { client->Close(); client->SetSocket(-1); }
      return 0;
    }
    
    if(sslcfg->ssl_encrypt_backend && s->openssl_client) {
      if(s->openssl_client->OpenSSLSocket(s->client.GetSocket()) != gxSSL_NO_ERROR) {
	if(servercfg->log_level >= 3) {
	  sbuf << clear << "[" << seq_num << "]: Error opening backend SSL socket";
	  LogMessage(sbuf.c_str());
	  sbuf << clear << "[" << seq_num << "]: " << s->openssl_client->SSLErrorMessage();
	  LogMessage(sbuf.c_str());
	}
	s->openssl_client->CloseSSLSocket();
	if(client->GetSocket() > 0) { client->Close(); client->SetSocket(-1); }
	return 0;
      }
      if(s->openssl_client->ConnectSSLSocket() != gxSSL_NO_ERROR) {
	if(servercfg->log_level >= 3) {
	  sbuf << clear << "[" << seq_num << "]: Error connecting to backend SSL socket";
	  LogMessage(sbuf.c_str());
	  sbuf << clear << "[" << seq_num << "]: " << s->openssl_client->SSLErrorMessage();
	  LogMessage(sbuf.c_str());
	}
	s->openssl_client->CloseSSLSocket();
	if(client->GetSocket() > 0) { client->Close(); client->SetSocket(-1); }
	return 0;
      }
      if((sslcfg->ssl_verify_backend_cert) && (!sslcfg->ssl_backend_hostname.is_null())) {
	if(s->openssl_client->VerifyCert(sslcfg->ssl_backend_hostname.c_str()) != gxSSL_NO_ERROR) {
	  sbuf << clear << "[" << seq_num << "]: SSL cert verify error for " << sslcfg->ssl_backend_hostname;
	  LogMessage(sbuf.c_str());
	  sbuf << clear << "[" << seq_num << "]: " << s->openssl_client->SSLErrorMessage();
	  LogMessage(sbuf.c_str());
	  s->openssl_client->CloseSSLSocket();
	  if(client->GetSocket() > 0) { client->Close(); client->SetSocket(-1); }
	  return 0;
	}
      }
    }
  }

  if(ptr) return ptr->data; 
  return 0;
}

LBnode *LBClientRequestThread::distrib(ClientSocket_t *s)
{
  gxString sbuf;
  gxString message;
  unsigned seq_num = s->seq_num;
  gxSocket *client = &s->client;
  gxList<limit_node> limit_node_list;
  gxListNode<limit_node> *limit_ptr_debug;

  int num_connections = servercfg->get_num_node_connections();
  if(num_connections == 0) num_connections = 1;

  if(servercfg->debug) {
    sbuf << clear << "Number of active server connections " << num_connections;
    LogDebugMessage(sbuf.c_str());
    sbuf << clear << "Number of refactored connections " << servercfg->REFACTORED_CONNECTIONS();
    LogDebugMessage(sbuf.c_str());
    if(servercfg->debug_level >= 2) {
      sbuf << clear << "Number of prev refactored connections " << servercfg->PREV_REFACTORED_CONNECTIONS();
      LogDebugMessage(sbuf.c_str());
    }
  }

  if(num_connections >= servercfg->REFACTORED_CONNECTIONS()) {
    servercfg->PREV_REFACTORED_CONNECTIONS(servercfg->REFACTORED_CONNECTIONS());
    servercfg->REFACTORED_CONNECTIONS(0, 0, servercfg->refactor_distribution_limits(limit_node_list, num_connections));
    if(servercfg->debug) {
      limit_ptr_debug = limit_node_list.GetHead();
      while(limit_ptr_debug) {
	sbuf << clear << limit_ptr_debug->data.active_ptr->node_name.c_str() <<  " refactor level increased to " 
	     << limit_ptr_debug->data.limit << " backend connections";
	LogDebugMessage(sbuf.c_str());
	limit_ptr_debug = limit_ptr_debug->next;
      }
    }
  }
  if(num_connections < servercfg->PREV_REFACTORED_CONNECTIONS()) {
    servercfg->REFACTORED_CONNECTIONS(0, 0, servercfg->refactor_distribution_limits(limit_node_list, num_connections));
    servercfg->PREV_REFACTORED_CONNECTIONS(0, 0, servercfg->REFACTORED_CONNECTIONS());
    if(servercfg->debug) {
      limit_ptr_debug = limit_node_list.GetHead();
      while(limit_ptr_debug) {
	sbuf << clear << limit_ptr_debug->data.active_ptr->node_name.c_str() <<  " refactor level decresed to " 
	     << limit_ptr_debug->data.limit << " backend connections";
	LogDebugMessage(sbuf.c_str());
	limit_ptr_debug = limit_ptr_debug->next;
      }
    }
  }

  gxListNode<limit_node> *limit_ptr = limit_node_list.GetHead();
  if(!limit_ptr) { // Build our default list
    servercfg->refactor_distribution_limits(limit_node_list, servercfg->PREV_REFACTORED_CONNECTIONS());
    limit_ptr = limit_node_list.GetHead();
  }

  while(limit_ptr) {
    if(limit_ptr->data.active_ptr->NUM_CONNECTIONS() < limit_ptr->data.limit) {
      if(servercfg->check_node_max_clients(limit_ptr->data.active_ptr, seq_num) != 0) {
	limit_ptr = 0;
	break;
      }
      break;
    }
    limit_ptr = limit_ptr->next;
  }

 if(!limit_ptr) return round_robin(s, servercfg->distributed_rr_node_list);

  if(servercfg->log_level >= 2) {
    sbuf << clear << "[" << seq_num << "]: " << limit_ptr->data.active_ptr->node_name << " Distributed connect to " << limit_ptr->data.active_ptr->hostname 
	 << " port " << limit_ptr->data.active_ptr->port_number;
    LogMessage(sbuf.c_str());
  }
  client->Clear();
  if(client->InitSocket(SOCK_STREAM, limit_ptr->data.active_ptr->port_number,  limit_ptr->data.active_ptr->hostname.c_str()) < 0) {
    if(servercfg->log_level >= 1) {
      sbuf << clear << "[" << seq_num << "]: ERROR - " << limit_ptr->data.active_ptr->node_name << " cannot open TCP socket to " << limit_ptr->data.active_ptr->hostname 
	   << " port " << limit_ptr->data.active_ptr->port_number;
      LogMessage(sbuf.c_str());
      CheckSocketError(client, seq_num);
    }
    if(servercfg->debug && servercfg->debug_level >= 5) {
      sbuf << clear << "[" << seq_num << "]: Connection error will default to distributed round robin";
      LogMessage(sbuf.c_str());
    }
    return round_robin(s, servercfg->distributed_rr_node_list);
  }
  if(client->Connect() < 0) {
    sbuf << clear << "[" << seq_num << "]: ERROR - " << limit_ptr->data.active_ptr->node_name << " cannot connect to " 
	 << limit_ptr->data.active_ptr->hostname << " port " << limit_ptr->data.active_ptr->port_number;
    LogMessage(sbuf.c_str());
    CheckSocketError(client, seq_num);
    if(client->GetSocket() > 0) { client->Close(); client->SetSocket(-1); }
    if(servercfg->debug && servercfg->debug_level >= 5) {
      sbuf << clear << "[" << seq_num << "]: Connection error will default to distributed round robin";
      LogDebugMessage(sbuf.c_str());
    }
    return round_robin(s, servercfg->distributed_rr_node_list);
  }

  if(sslcfg->ssl_encrypt_backend && s->openssl_client) {
    if(s->openssl_client->OpenSSLSocket(s->client.GetSocket()) != gxSSL_NO_ERROR) {
      if(servercfg->log_level >= 3) {
	sbuf << clear << "[" << seq_num << "]: Error opening backend SSL socket";
	LogMessage(sbuf.c_str());
	sbuf << clear << "[" << seq_num << "]: " << s->openssl_client->SSLErrorMessage();
	LogMessage(sbuf.c_str());
      }
      s->openssl_client->CloseSSLSocket();
      if(client->GetSocket() > 0) { client->Close(); client->SetSocket(-1); }
      return round_robin(s, servercfg->distributed_rr_node_list);
    }
    if(s->openssl_client->ConnectSSLSocket() != gxSSL_NO_ERROR) {
      if(servercfg->log_level >= 3) {
	sbuf << clear << "[" << seq_num << "]: Error connecting to backend SSL socket";
	LogMessage(sbuf.c_str());
	sbuf << clear << "[" << seq_num << "]: " << s->openssl_client->SSLErrorMessage();
	LogMessage(sbuf.c_str());
      }
      s->openssl_client->CloseSSLSocket();
      if(client->GetSocket() > 0) { client->Close(); client->SetSocket(-1); }
      return round_robin(s, servercfg->distributed_rr_node_list);
    }
    if((sslcfg->ssl_verify_backend_cert) && (!sslcfg->ssl_backend_hostname.is_null())) {
      if(s->openssl_client->VerifyCert(sslcfg->ssl_backend_hostname.c_str()) != gxSSL_NO_ERROR) {
	sbuf << clear << "[" << seq_num << "]: SSL cert verify error for " << sslcfg->ssl_backend_hostname;
	LogMessage(sbuf.c_str());
	sbuf << clear << "[" << seq_num << "]: " << s->openssl_client->SSLErrorMessage();
	LogMessage(sbuf.c_str());
	s->openssl_client->CloseSSLSocket();
	if(client->GetSocket() > 0) { client->Close(); client->SetSocket(-1); }
	return round_robin(s, servercfg->distributed_rr_node_list);
      }
    }
  }
  
  return limit_ptr->data.active_ptr;
}

LBnode *LBClientRequestThread::weighted(ClientSocket_t *s)
{
  gxString sbuf;
  gxString message;
  unsigned seq_num = s->seq_num;
  gxSocket *client = &s->client;


  gxList<limit_node> limit_node_list;
  gxListNode<limit_node> *limit_ptr_debug;

  int num_connections = servercfg->get_num_node_connections();
  if(num_connections == 0) num_connections = 1;

  if(servercfg->debug) {
    sbuf << clear << "Number of active server connections " << num_connections;
    LogDebugMessage(sbuf.c_str());
    sbuf << clear << "Number of refactored connections " << servercfg->REFACTORED_CONNECTIONS();
    LogDebugMessage(sbuf.c_str());
    if(servercfg->debug_level >= 2) {
      sbuf << clear << "Number of prev refactored connections " << servercfg->PREV_REFACTORED_CONNECTIONS();
      LogDebugMessage(sbuf.c_str());
    }
  }

  if(num_connections >= servercfg->REFACTORED_CONNECTIONS()) {
    servercfg->PREV_REFACTORED_CONNECTIONS(0, 0, servercfg->REFACTORED_CONNECTIONS());
    servercfg->REFACTORED_CONNECTIONS(0, 0, servercfg->refactor_weighted_limits(limit_node_list, servercfg->WEIGHT_SCALE(1, 0, 0)));
    if(servercfg->debug) {
      limit_ptr_debug = limit_node_list.GetHead();
      while(limit_ptr_debug) {
	sbuf << clear << limit_ptr_debug->data.active_ptr->node_name.c_str() <<  " refactor level increased to " 
	     << limit_ptr_debug->data.limit << " backend connections";
	LogDebugMessage(sbuf.c_str());
	limit_ptr_debug = limit_ptr_debug->next;
      }
    }
  }
  if(num_connections < servercfg->PREV_REFACTORED_CONNECTIONS()) {
    servercfg->REFACTORED_CONNECTIONS(0, 0, servercfg->refactor_weighted_limits(limit_node_list, servercfg->WEIGHT_SCALE(0, 1, 0)));
    servercfg->PREV_REFACTORED_CONNECTIONS(0, 0, servercfg->REFACTORED_CONNECTIONS());
    if(servercfg->debug) {
      limit_ptr_debug = limit_node_list.GetHead();
      while(limit_ptr_debug) {
	sbuf << clear << limit_ptr_debug->data.active_ptr->node_name.c_str() <<  " refactor level decresed to " 
	     << limit_ptr_debug->data.limit << " backend connections";
	LogDebugMessage(sbuf.c_str());
	limit_ptr_debug = limit_ptr_debug->next;
      }
    }
  }

  gxListNode<limit_node> *limit_ptr = limit_node_list.GetHead();
  if(!limit_ptr) { // Build our default list
    servercfg->refactor_weighted_limits(limit_node_list, servercfg->WEIGHT_SCALE());
    limit_ptr = limit_node_list.GetHead();
  }

  while(limit_ptr) {
    if(limit_ptr->data.active_ptr->NUM_CONNECTIONS() < limit_ptr->data.limit) {
      if(servercfg->check_node_max_clients(limit_ptr->data.active_ptr, seq_num) != 0) {
	limit_ptr = 0;
	break;
      }
      break;
    }
    limit_ptr = limit_ptr->next;
  }

  if(!limit_ptr) return round_robin(s, servercfg->weighted_rr_node_list);

  if(servercfg->log_level >= 2) {
    sbuf << clear << "[" << seq_num << "]: " << limit_ptr->data.active_ptr->node_name << " Weighted connect to " << limit_ptr->data.active_ptr->hostname 
	 << " port " << limit_ptr->data.active_ptr->port_number;
    LogMessage(sbuf.c_str());
  }
  client->Clear();
  if(client->InitSocket(SOCK_STREAM, limit_ptr->data.active_ptr->port_number,  limit_ptr->data.active_ptr->hostname.c_str()) < 0) {
    if(servercfg->log_level >= 1) {
      sbuf << clear << "[" << seq_num << "]: ERROR - " << limit_ptr->data.active_ptr->node_name 
	   << " cannot open TCP socket to " << limit_ptr->data.active_ptr->hostname 
	   << " port " << limit_ptr->data.active_ptr->port_number;
      LogMessage(sbuf.c_str());
      CheckSocketError(client, seq_num);
    }
    if(servercfg->debug && servercfg->debug_level >= 5) {
      sbuf << clear << "[" << seq_num << "]: Connection error will default to weighted round robin";
      LogMessage(sbuf.c_str());
    }
    return round_robin(s, servercfg->weighted_rr_node_list);
  }
  if(client->Connect() < 0) {
    sbuf << clear << "[" << seq_num << "]: ERROR - " << limit_ptr->data.active_ptr->node_name << " cannot connect to " 
	 << limit_ptr->data.active_ptr->hostname << " port " << limit_ptr->data.active_ptr->port_number;
    LogMessage(sbuf.c_str());
    CheckSocketError(client, seq_num);
    if(client->GetSocket() > 0) { client->Close(); client->SetSocket(-1); }
    if(servercfg->debug && servercfg->debug_level >= 5) {
      sbuf << clear << "[" << seq_num << "]: Connection error will default to weighted round robin";
      LogDebugMessage(sbuf.c_str());
    }
    return round_robin(s, servercfg->weighted_rr_node_list);
  }

  if(sslcfg->ssl_encrypt_backend && s->openssl_client) {
    if(s->openssl_client->OpenSSLSocket(s->client.GetSocket()) != gxSSL_NO_ERROR) {
      if(servercfg->log_level >= 3) {
	sbuf << clear << "[" << seq_num << "]: Error opening backend SSL socket";
	LogMessage(sbuf.c_str());
	sbuf << clear << "[" << seq_num << "]: " << s->openssl_client->SSLErrorMessage();
	LogMessage(sbuf.c_str());
      }
      s->openssl_client->CloseSSLSocket();
      if(client->GetSocket() > 0) { client->Close(); client->SetSocket(-1); }
      return round_robin(s, servercfg->weighted_rr_node_list);
    }
    if(s->openssl_client->ConnectSSLSocket() != gxSSL_NO_ERROR) {
      if(servercfg->log_level >= 3) {
	sbuf << clear << "[" << seq_num << "]: Error connecting to backend SSL socket";
	LogMessage(sbuf.c_str());
	sbuf << clear << "[" << seq_num << "]: " << s->openssl_client->SSLErrorMessage();
	LogMessage(sbuf.c_str());
      }
      s->openssl_client->CloseSSLSocket();
      if(client->GetSocket() > 0) { client->Close(); client->SetSocket(-1); }
      return round_robin(s, servercfg->weighted_rr_node_list);
    }
    if((sslcfg->ssl_verify_backend_cert) && (!sslcfg->ssl_backend_hostname.is_null())) {
      if(s->openssl_client->VerifyCert(sslcfg->ssl_backend_hostname.c_str()) != gxSSL_NO_ERROR) {
	sbuf << clear << "[" << seq_num << "]: SSL cert verify error for " << sslcfg->ssl_backend_hostname;
	LogMessage(sbuf.c_str());
	sbuf << clear << "[" << seq_num << "]: " << s->openssl_client->SSLErrorMessage();
	LogMessage(sbuf.c_str());
	s->openssl_client->CloseSSLSocket();
	if(client->GetSocket() > 0) { client->Close(); client->SetSocket(-1); }
	return round_robin(s, servercfg->weighted_rr_node_list);
      }
    }
  }

  return limit_ptr->data.active_ptr;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
