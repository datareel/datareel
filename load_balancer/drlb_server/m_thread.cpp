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

ClientSocket_t::~ClientSocket_t()
{

}

SocketWR_t::~SocketWR_t() {
  if(buffer) delete buffer;
  gxListNode<MemoryBuffer *> *ptr = cache.GetHead();
  while(ptr) {
    delete ptr->data;
    ptr->data = 0;
    ptr = ptr->next;
  }
  cache.ClearList();
}

int LBServerThread::InitServer(int max_connections)
// Initialize the server. Returns a non-zero value if any errors ocurr.
{
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
  gxString message;
  int accept_retires = 5;
  while(servercfg->accept_clients) { 
    if(servercfg->debug && servercfg->debug_level == 5) {
      LogDebugMessage("LB server thread accept() call");
    }
    // Block the server until a client requests service
    if(Accept() < 0) {
      if(servercfg->debug) {
	gxString sbuf;
	int error_number;
	get_fd_error(error_number, sbuf);
	message << clear << " ENUM: " << error_number << " Message: " << sbuf;
	LogDebugMessage(message.c_str());
	LogDebugMessage("ERROR - LB server thread accept() call failed");
      }
      sSleep(1); // Allow for recovery time
      if(--accept_retires <= 0) break;
      continue;
    }
    accept_retires = 5;

    int num_threads = servercfg->NUM_CLIENT_THREADS();
    if(servercfg->debug && servercfg->debug_level == 5) {
      message << clear << num_threads << " client threads working";
      LogDebugMessage(message.c_str());
    }

    // NOTE: Getting client info for statistical purposes only
    char client_name[gxsMAX_NAME_LEN]; int r_port = -1;
    GetClientInfo(client_name, r_port);
    int seq_num = r_port;
    if(servercfg->log_level >= 1) {
      message << clear << "[" << seq_num << "]: Frontend client connect: " << client_name;
      LogMessage(message.c_str());
    }

    if(servercfg->max_threads > 1) {
      if(servercfg->debug && servercfg->debug_level == 5) {
	message << clear << "Max thread limit is set to " << servercfg->max_threads; 
	LogDebugMessage(message.c_str());
      }
      if(num_threads >= servercfg->max_threads) {
	message << clear << "[" << seq_num << "]: Reached " << num_threads << " max thread limit. " << client_name << " connection denied";
	LogMessage(message.c_str());
	close(GetRemoteSocket());
	continue;
      }
    }

    if(servercfg->max_connections > 1) {
      int num_connections = servercfg->get_num_server_connections();

      if(servercfg->debug && servercfg->debug_level == 5) {
	message << clear << "Max frontend connection limit is set to " << servercfg->max_connections; 
	LogDebugMessage(message.c_str());
      }
      if(num_connections >= servercfg->max_connections) {
	message << clear << "[" << seq_num << "]: Reached " << num_connections
		<< " max frontend connection limit. " << client_name << " connection denied";
	LogMessage(message.c_str());
	close(GetRemoteSocket());
	continue;
      }
    }

    ClientSocket_t *s = new ClientSocket_t;
    if(!s) {
      message << clear << "[" << seq_num << "]: ERROR - A fatal memory allocation error occurred in main server thread";
      LogMessage(message.c_str());
      break;
    }
    
    // Record the file descriptor assigned by the Operating System
    s->client_socket = GetRemoteSocket();
    s->seq_num = seq_num;
    s->client_name = client_name;
    s->r_port = r_port;

    if(servercfg->debug && servercfg->debug_level == 5) LogDebugMessage("LB server rebuilding thread pool");
    RebuildThreadPool(servercfg->client_request_pool);

    if(servercfg->debug && servercfg->debug_level == 5) LogDebugMessage("LB server starting client request thread");
    gxThread_t *rthread = request_thread.CreateThread(servercfg->client_request_pool, (void *)s);
    if(!rthread) {
      message << clear << "[" << seq_num << "]: ERROR - Memory allocation main server thread, cannot allocate client thread";
      LogMessage(message.c_str());
      delete s;
      break;
    }

    if(CheckThreadError(rthread, seq_num)) {
      message << clear << "[" << seq_num << "]: ERROR - Fatal error starting client thread";
      LogMessage(message.c_str());
      delete s;
      break;
    }
  }

  if(servercfg->debug && servercfg->debug_level > 1) NT_print("Main server thread has exited");
  return (void *)0;
}

void LBServerThread::ThreadExitRoutine(gxThread_t *thread)
// Thread exit function used to close the server thread.
{
  if(servercfg->debug && servercfg->debug_level == 5) { 
    LogDebugMessage("Server thread exit routine closing connections");
  }
  CloseSocket(); // Close the server side socket
}

void LBServerThread::ThreadCleanupHandler(gxThread_t *thread)
// Thread cleanup handler used in the event that the thread is
// canceled.
{
  if(servercfg->debug && servercfg->debug_level == 5) { 
    LogDebugMessage("Server thread clean up handler closing connections");
  }
  CloseSocket(); // Close the server side socket
}

void *LBClientRequestThread::ThreadEntryRoutine(gxThread_t *thread)
{
  // Extract the client socket from the thread parameter
  ClientSocket_t *s = (ClientSocket_t *)thread->GetThreadParm();
    
  // Process the client request
  HandleClientRequest(s);

  return (void *)0;
}

void LBClientRequestThread::ThreadExitRoutine(gxThread_t *thread)
// Thread exit function used to close the client thread.
{
  gxString message;

  // Extract the client socket from the thread parameter
  ClientSocket_t *s = (ClientSocket_t *)thread->GetThreadParm();

  if(servercfg->debug && servercfg->debug_level == 5) { 
    message << clear << "[" << s->seq_num << "]: Client request thread exit routine closing connections";
    LogDebugMessage(message.c_str());
  }

  Close(s->client_socket);
  s->client.Close();

  if(frontend_rw_thread) {
    if(frontend_rw_thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
      if(servercfg->debug && servercfg->debug_level == 5) { 
	message << clear << "[" << s->seq_num << "]: Cancel frontend_rw_thread ";
	LogDebugMessage(message.c_str());
      }
      socket_rw_thread.CancelThread(frontend_rw_thread);
      socket_rw_thread.JoinThread(frontend_rw_thread);
    }
  }

  servercfg->NUM_CLIENT_THREADS(0, 1);

  if(backend_rw_thread) {
    if(backend_rw_thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
      if(servercfg->debug && servercfg->debug_level == 5) { 
	message << clear << "[" << s->seq_num << "]: Cancel backend_rw_thread ";
	LogDebugMessage(message.c_str());
      }
      socket_rw_thread.CancelThread(backend_rw_thread);
      socket_rw_thread.JoinThread(backend_rw_thread);
    }
  }

  servercfg->NUM_CLIENT_THREADS(0, 1);

  if(servercfg->debug && servercfg->debug_level == 5) { 
    message << clear << "[" << s->seq_num << "]: Client connections closed";
    LogDebugMessage(message.c_str());
  }

  servercfg->NUM_CLIENT_THREADS(0, 1);
  if(s->node) s->node->NUM_CONNECTIONS(0, 1);

  if(s) delete s;
  s = 0;
}

void LBClientRequestThread::ThreadCleanupHandler(gxThread_t *thread)
// Thread cleanup handler used in the event that the thread is
// canceled.
{
  gxString message;

  // Extract the client socket from the thread parameter
  ClientSocket_t *s = (ClientSocket_t *)thread->GetThreadParm();

  if(servercfg->debug && servercfg->debug_level == 5) { 
    message << clear << "[" << s->seq_num << "]: Client request thread clean up handler closing connections: ";
    LogDebugMessage(message.c_str());
  }

  Close(s->client_socket);
  s->client.Close();

  if(frontend_rw_thread) {
    if(frontend_rw_thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
      if(servercfg->debug && servercfg->debug_level == 5) { 
	message << clear << "[" << s->seq_num << "]: Cancel frontend_rw_thread ";
	LogDebugMessage(message.c_str());
      }
      socket_rw_thread.CancelThread(frontend_rw_thread);
      socket_rw_thread.JoinThread(frontend_rw_thread);
    }
  }

  servercfg->NUM_CLIENT_THREADS(0, 1);

  if(backend_rw_thread) {
    if(backend_rw_thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
      if(servercfg->debug && servercfg->debug_level == 5) { 
	message << clear << "[" << s->seq_num << "]: Cancel backend_rw_thread ";
	LogDebugMessage(message.c_str());
      }
      socket_rw_thread.CancelThread(backend_rw_thread);
      socket_rw_thread.JoinThread(backend_rw_thread);
    }
  }

  servercfg->NUM_CLIENT_THREADS(0, 1);

  if(servercfg->debug && servercfg->debug_level == 5) { 
    message << clear << "[" << s->seq_num << "]: Client connections closed";
    LogDebugMessage(message.c_str());
  }

  servercfg->NUM_CLIENT_THREADS(0, 1);
  if(s->node) s->node->NUM_CONNECTIONS(0, 1);

  if(s) delete s;
  s = 0;
}

int get_fd_error(int &error_number, gxString &sbuf)
{
  error_number = errno;
  return fd_error_to_string(error_number, sbuf);
}

void LBClientRequestThread::HandleClientRequest(ClientSocket_t *s)
// Function used to handle a client request.
{
  servercfg->NUM_CLIENT_THREADS(1);

  gxString sbuf;
  gxString message;
  unsigned seq_num = s->seq_num;
  LBnode *n;

  switch (servercfg->scheme) {
    case LB_ASSIGNED:
      n = assigned(s);
      if(!n) {
	switch(servercfg->assigned_default) {
	  case LB_RR:
	    if(servercfg->debug && servercfg->debug_level == 5) {
	      sbuf << clear << "[" << seq_num << "]: LB assigned will default to round robin";
	      LogDebugMessage(sbuf.c_str());
	    }
	    n = round_robin(s);
	    break;
	  case LB_DISTRIB:
	    if(servercfg->debug && servercfg->debug_level == 5) {
	      sbuf << clear << "[" << seq_num << "]: LB assigned will fail over to distributed";
	      LogDebugMessage(sbuf.c_str());
	    }
	    n = distrib(s);
	    break;
	  case LB_WEIGHTED:
	    if(servercfg->debug && servercfg->debug_level == 5) {
	      sbuf << clear << "[" << seq_num << "]: LB assigned will fail over to weighted";
	      LogDebugMessage(sbuf.c_str());
	    }
	    n = weighted(s);
	    break;
	  case LB_NONE:
	    if(servercfg->debug && servercfg->debug_level == 5) {
	      sbuf << clear << "[" << seq_num << "]: LB assigned will not fail over to another node";
	      LogDebugMessage(sbuf.c_str());
	    }
	    return;
	  default:
	    if(servercfg->debug && servercfg->debug_level == 5) {
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

  if(servercfg->debug && servercfg->debug_level == 5) {
    int num_connections = n->NUM_CONNECTIONS();
    message << clear << "[" << seq_num << "]: " << n->node_name << " has " << num_connections << " client connections";
    LogDebugMessage(message.c_str());
  }

  unsigned buffer_size = n->buffer_size;

  SocketWR_t *frontend_s = new SocketWR_t;
  if(!frontend_s) {
    message << clear << "[" << seq_num << "]: ERROR - A fatal memory allocation error occurred in client request thread";
    LogMessage(message.c_str());
    n->NUM_CONNECTIONS(0, 1);
    return;
  }
  SocketWR_t *backend_s = new SocketWR_t;
  if(!backend_s) {
    message << clear << "[" << seq_num << "]: ERROR - A fatal memory allocation error occurred in client request thread";
    LogMessage(message.c_str());
    n->NUM_CONNECTIONS(0, 1);
    return;
  }
  frontend_s->log_str = "Frontend";
  frontend_s->buffer_size = buffer_size;
  frontend_s->buffer = new char[buffer_size];
  frontend_s->seq_num = seq_num;
  frontend_s->read_sock = s->client_socket;
  frontend_s->write_sock = s->client.GetSocket();
  frontend_s->node = n;

  backend_s->log_str = "Backend";
  backend_s->buffer_size = buffer_size;
  backend_s->buffer = new char[buffer_size];
  backend_s->seq_num = seq_num;
  backend_s->read_sock = s->client.GetSocket();
  backend_s->write_sock = s->client_socket;
  backend_s->node = n;

  // Enter our frontend and backend I/O threads here
  frontend_rw_thread = socket_rw_thread.CreateThread((void *)frontend_s);
  if(CheckThreadError(frontend_rw_thread, seq_num)) {
    if(servercfg->debug && servercfg->debug_level == 5) {
      message << clear << "[" << seq_num << "]: Error starting frontend_rw_thread thread";
      LogDebugMessage(message.c_str());
    }
    servercfg->NUM_CLIENT_THREADS(0, 1);
    n->NUM_CONNECTIONS(0, 1);
    return;
  }

  backend_rw_thread = socket_rw_thread.CreateThread((void *)backend_s);
  if(CheckThreadError(backend_rw_thread, seq_num)) {
    if(servercfg->debug && servercfg->debug_level == 5) {
      message << clear << "[" << seq_num << "]: Error starting backend_rw_thread thread";
      LogDebugMessage(message.c_str());
    }
    servercfg->NUM_CLIENT_THREADS(0, 1);
    n->NUM_CONNECTIONS(0, 1);
    return;
  }

  if(socket_rw_thread.JoinThread(frontend_rw_thread) != 0) {
    if(servercfg->debug && servercfg->debug_level == 5) {
      message << clear << "[" << seq_num << "]: ERROR - Error joining frontend read/write thread";
      LogDebugMessage(message.c_str());
    }
  }

  if(frontend_rw_thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
    if(socket_rw_thread.JoinThread(backend_rw_thread) != 0) {
      if(servercfg->debug && servercfg->debug_level == 5) {
	message << clear << "[" << seq_num << "]: ERROR - Error joining backend read/write thread";
	LogDebugMessage(message.c_str());
      } 
    }
  }

  servercfg->NUM_CLIENT_THREADS(0, 1);
  if(servercfg->debug && servercfg->debug_level == 5) {
    message << clear << "[" << seq_num << "]: Client has disconnected, exiting client request thread";
    LogDebugMessage(message.c_str());
  }

  return;
}

void SocketWRThread::ThreadExitRoutine(gxThread_t *thread)
// Thread exit function used to close the client thread.
{
  gxString message;
  SocketWR_t *s = (SocketWR_t *)thread->GetThreadParm();

  if(servercfg->debug && servercfg->debug_level == 5) { 
    message << clear << "[" << s->seq_num << "]: " << s->log_str << " read/write thread exiting";
    LogDebugMessage(message.c_str());
  }
  if(s) delete s;
  s = 0;
  servercfg->NUM_CLIENT_THREADS(0, 1);
}

void SocketWRThread::ThreadCleanupHandler(gxThread_t *thread)
// Thread cleanup handler used in the event that the thread is
// canceled.
{
  gxString message;
  SocketWR_t *s = (SocketWR_t *)thread->GetThreadParm();
  if(servercfg->debug && servercfg->debug_level == 5) { 
    message << clear << "[" << s->seq_num << "]: " << s->log_str << " read/write thread clean up handler";
    LogDebugMessage(message.c_str());
  }
  if(s) delete s;
  s = 0; 
  servercfg->NUM_CLIENT_THREADS(0, 1);
}

void *SocketWRThread::ThreadEntryRoutine(gxThread_t *thread)
{
  SocketWR_t *s = (SocketWR_t *)thread->GetThreadParm();
  int rv;

  servercfg->NUM_CLIENT_THREADS(1);

  if(servercfg->use_buffer_cache) {
    while(servercfg->echo_loop) {
      rv = read_socket_cached(s);
      if(rv < 0) break;
      rv = write_socket_cached(s);
      if(rv < 0) break;
    }
  }
  else {
    while(servercfg->echo_loop) {
      rv = read_socket(s);
      if(rv < 0) break;
      rv = write_socket(s);
      if(rv < 0) break;
    }
  }
  return (void *)0;
}

int SocketWRThread::read_socket(SocketWR_t *s)
{
  int br = 0;
  gxString message, sbuf;
  unsigned seq_num = s->seq_num;
  int error_number = 0;

  if(servercfg->use_timeout && (!ReadSelect(s->read_sock, servercfg->timeout_secs, servercfg->timeout_usecs))) {
    if(servercfg->log_level >= 4) {
      message << clear << "[" << seq_num << "]: " << s->log_str << " read socket timeout after " 
	      << servercfg->timeout_secs << " secs " << servercfg->timeout_usecs << " usecs";
      LogMessage(message.c_str());
      message << clear << "[" << seq_num << "]: Closing " << s->log_str << " connection"; 
      LogMessage(message.c_str());
    }
    s->bytes_read = 0;
    return -1;
  }
  br = read(s->read_sock, s->buffer, s->buffer_size);
  if(servercfg->debug && servercfg->debug_level == 5) { 
    get_fd_error(error_number, sbuf);
    message << clear << "[" << seq_num << "]: " << s->log_str << " read() Return: " << br << " ENUM: " << error_number << " Message: " << sbuf;
    LogMessage(message.c_str());
  }
  if(br == 0) {
    if(servercfg->log_level > 3) {
      message << clear << "[" << seq_num << "]: " << s->log_str << " client has disconnected";
      LogMessage(message.c_str());
    }
    s->bytes_read = 0;
    return -1;
  }
  if(br < 0) {
    if(servercfg->log_level >= 3) {
      message << clear << "[" << seq_num << "]: Error reading from " << s->log_str << " client socket";
      LogMessage(message.c_str());
      message << clear << "[" << seq_num << "]: " << s->log_str << " read returned " << br << " ERROR: " << error_number << ": " << sbuf;
      LogMessage(message.c_str());
    }
    s->bytes_read = 0;
    return -1;
  }
  s->bytes_read = br;
  return br;
}

int SocketWRThread::write_socket(SocketWR_t *s)
{
  gxString message, sbuf;
  unsigned seq_num = s->seq_num;
  int error_number = 0;

  if(s->bytes_read == 0) return 0;
  if(s->bytes_read < 0) return -1;

  int bm = write(s->write_sock, s->buffer, s->bytes_read);
  get_fd_error(error_number, sbuf);
  if(servercfg->debug && servercfg->debug_level == 5) { 
    message << clear << "[" << seq_num << "]: " << s->log_str << " write() Return: " << bm << " ENUM: " << error_number << " Message: " << sbuf;
    LogDebugMessage(message.c_str());
  }
  
  if(bm < 0) {
    if(servercfg->log_level >= 3) {
      message << clear << "[" << seq_num << "]: Error writing to " << s->log_str << " client socket";
      LogMessage(message.c_str());
      message << clear << "[" << seq_num << "]: " << s->log_str << " write returned " << bm << " ERROR: " << error_number << ": " << sbuf;
      LogMessage(message.c_str());
    }
    return -1;
  }
  return bm;
}

int SocketWRThread::read_socket_cached(SocketWR_t *s)
{
  int br = 0;
  gxString message, sbuf;
  unsigned seq_num = s->seq_num;
  int error_number = 0;
  int ready = 1;
  fd_set sock_fds;
  struct timeval timeo;

  while(ready > 0) {
    if(servercfg->use_timeout && (!ReadSelect(s->read_sock, servercfg->timeout_secs, servercfg->timeout_usecs))) {
      if(servercfg->log_level >= 4) {
	message << clear << "[" << seq_num << "]: " << s->log_str << " read socket timeout after " 
		<< servercfg->timeout_secs << " secs " << servercfg->timeout_usecs << " usecs";
	LogMessage(message.c_str());
	message << clear << "[" << seq_num << "]: Closing " << s->log_str << " connection"; 
	LogMessage(message.c_str());
      }
      s->bytes_read = 0;
      return -1;
    }
    br = read(s->read_sock, s->buffer, s->buffer_size);
    get_fd_error(error_number, sbuf);
    if(servercfg->debug && servercfg->debug_level == 5) { 
      message << clear << "[" << seq_num << "]: Cached " << s->log_str << " read() Return: " << br << " ENUM: " << error_number << " Message: " << sbuf;
      LogDebugMessage(message.c_str());
    }
    if(br == 0) {
      if(servercfg->log_level > 3) {
	message << clear << "[" << seq_num << "]: " << s->log_str << " client has disconnected";
	LogMessage(message.c_str());
      }
      s->bytes_read = 0;
      return -1;
    }
    if(br < 0) {
      if(servercfg->log_level >= 3) {
	message << clear << "[" << seq_num << "]: Error reading from " << s->log_str << " client socket";
	LogMessage(message.c_str());
	message << clear << "[" << seq_num << "]: " << s->log_str << " read returned " << br << " ERROR: " << error_number << ": " << sbuf;
	LogMessage(message.c_str());
      }
      s->bytes_read = 0;
      return -1;
    }
    s->bytes_read += br;

    if(s->bytes_read >= servercfg->buffer_overflow_size) {
      if(servercfg->log_level >= 3) {
	message << clear << "[" << seq_num << "]: " << s->log_str << " read " << s->bytes_read << " bytes from client";
	LogMessage(message.c_str());
	message << clear << "[" << seq_num << "]: " << s->log_str << " Closing connection due to buffer overflow";
	LogMessage(message.c_str());
      }
    }

    MemoryBuffer *inbuf = new MemoryBuffer(s->buffer, br);
    s->cache.Add(inbuf);

    // Check for bytes waiting to be read
    int flags1 = fcntl(s->read_sock, F_GETFL, 0);
    fcntl(s->read_sock, F_SETFL, flags1 | O_NONBLOCK); // Set read not to block
    ready = recv(s->read_sock, s->buffer, s->buffer_size, MSG_PEEK); 
    if(errno == EAGAIN) { // EAGAIN and EWOULDBLOCK are the same value
      errno = 0;
    }
    fcntl(s->read_sock, F_SETFL, flags1); // Set socket back to blocking read
  }
  return s->bytes_read;
}

int SocketWRThread::write_socket_cached(SocketWR_t *s)
{
  gxString message, sbuf;
  unsigned seq_num = s->seq_num;
  int error_number = 0;
  int bytes_moved = 0;
  int bm = 0;

  if(s->bytes_read == 0) return 0;
  if(s->bytes_read < 0) return -1;

  gxListNode<MemoryBuffer *> *ptr = s->cache.GetHead();
  bytes_moved = 0;
  while(ptr) {
    bm = write(s->write_sock, ptr->data->m_buf(), ptr->data->length());
    get_fd_error(error_number, sbuf);
    if(bm > 0) bytes_moved += bm;
    if(bm < 0 && error_number == EAGAIN) { // EAGAIN and EWOULDBLOCK are the same value
      bm = bytes_moved;
      break;
    }
    if(bm < 0) {
      if(servercfg->log_level >= 3) {
	message << clear << "[" << seq_num << "]: Error writing to " << s->log_str << " client socket";
	LogMessage(message.c_str());
	message << clear << "[" << seq_num << "]: " << s->log_str << " write returned " << bm << " ERROR: " << error_number << ": " << sbuf;
	LogMessage(message.c_str());
      }
      break;
    }
    if(servercfg->debug && servercfg->debug_level == 5) { 
      message << clear << "[" << seq_num << "]: " << s->log_str << " cached write() Return: " << bm << " ENUM: " << error_number << " Message: " << sbuf;
      LogDebugMessage(message.c_str());
    }
    ptr = ptr->next;
  }

  ptr = s->cache.GetHead();
  while(ptr) {
    delete ptr->data;
    ptr->data = 0;
    ptr = ptr->next;
  }
  s->cache.ClearList();

  if(bm < 0) return -1;

  if(servercfg->debug && servercfg->debug_level == 5) { 
    message << clear << "[" << seq_num << "]: Cached " << s->log_str << " write() bytes moved: " << bytes_moved; 
    LogDebugMessage(message.c_str());
  }
  
  return bytes_moved;
}

LBnode *LBClientRequestThread::round_robin(ClientSocket_t *s)
{
  return round_robin(s, servercfg->nodes);
}

LBnode *LBClientRequestThread::round_robin(ClientSocket_t *s,  gxList<LBnode *> &node_list)
{
  gxString sbuf;
  gxString message;
  int error_number;
  unsigned seq_num = s->seq_num;
  gxListNode<LBnode *> *ptr = node_list.GetHead();
  unsigned count = 0;
  unsigned retries = servercfg->retries;
  unsigned retry_wait = servercfg->retry_wait;
  unsigned error_flag = 0;
  unsigned num_connects = 0;
  unsigned num_nodes = servercfg->NUM_NODES();
  gxSocket *client = &s->client;

  while(ptr) {
    if(ptr->data->LB_FLAG() == 0 && servercfg->check_node_max_clients(ptr->data, seq_num) == 0) {
      if(servercfg->log_level >= 2) {
	sbuf << clear << "[" << seq_num << "]: " << ptr->data->node_name << " RR connect to " << ptr->data->hostname 
	     << " port " << ptr->data->port_number;
	LogMessage(sbuf.c_str());
      }
      client->Clear();
      if(client->InitSocket(SOCK_STREAM, ptr->data->port_number,  ptr->data->hostname.c_str()) < 0) {
	if(servercfg->log_level >= 1) {
	  sbuf << clear << "[" << seq_num << "]: ERROR - " << ptr->data->node_name << " cannot open connection to backend host " << ptr->data->hostname 
	       << " port " << ptr->data->port_number;
	  LogMessage(sbuf.c_str());
	  CheckSocketError(client, seq_num);
	}
	ptr->data->LB_FLAG(0);
	error_flag++;
      }
      if(client->Connect() < 0) {
	if(servercfg->log_level >= 1) {
	  sbuf << clear << "[" << seq_num << "]: ERROR - " << ptr->data->node_name << " cannot connect to backend host " 
	       << ptr->data->hostname << " port " << ptr->data->port_number;
	  LogMessage(sbuf.c_str());
	  CheckSocketError(client, seq_num);
	}
	client->Close();
	ptr->data->LB_FLAG(0);
	error_flag++;
      }
      else {
	num_connects++;
	error_flag = 0;
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
  int error_number;
  unsigned seq_num = s->seq_num;
  unsigned count = 0;
  unsigned retries = servercfg->retries;
  unsigned retry_wait = servercfg->retry_wait;
  unsigned error_level = 0;
  unsigned num_connects = 0;
  unsigned num_nodes = servercfg->NUM_NODES();
  gxSocket *client = &s->client;
  gxString client_name = s->client_name;

  if(servercfg->dynamic_rules_read) error_level = servercfg->ReadRulesConfig();
  if(error_level == 1) {
    if(servercfg->debug && servercfg->debug_level == 5) {
      sbuf << clear << "[" << seq_num << "]: ERROR - Fatal error reading " << servercfg->rules_config_file;
      LogDebugMessage(sbuf.c_str());
    }
    return 0;
  }
  
  regex_t regex;
  int reti;
  char mbuf[255];
  gxListNode<LB_rule *> *lptr = servercfg->rules_config_list.GetHead();
  error_level = 0;
  while(lptr) {
    if(lptr->data->rule == LB_ROUTE) {
      reti = regcomp(&regex, lptr->data->front_end_client_ip.c_str(), REG_EXTENDED|REG_NOSUB);
      if(reti != 0) { // This should have been checked in config read
	if(servercfg->debug && servercfg->debug_level == 5) {
	  sbuf << clear << "[" << seq_num << "]: ERROR - Bad regular expression in rules file: " << lptr->data->front_end_client_ip.c_str();
	  LogDebugMessage(sbuf.c_str());
	}
      }
      else {
	reti = regexec(&regex, s->client_name.c_str(), 0, NULL, 0);
	regfree(&regex);
	if(reti == 0) {
	  if(servercfg->debug && servercfg->debug_level == 5) {
	    sbuf << clear << "[" << seq_num << "]: INFO - Frontend IP matches regex: " << s->client_name << " match " << lptr->data->front_end_client_ip;
	    LogDebugMessage(sbuf.c_str());
	  }
	  error_level = 0;
	  break;
	}
	else if(reti == REG_NOMATCH) {
	  if(servercfg->debug && servercfg->debug_level == 5) {
	    sbuf << clear << "[" << seq_num << "]: INFO - Frontend IP does not match regex: " << s->client_name << " no match " << lptr->data->front_end_client_ip;
	    LogDebugMessage(sbuf.c_str());
	  }
	  error_level = 0;
	}
	else {
	  regerror(reti, &regex, mbuf, sizeof(mbuf));
	  if(servercfg->debug && servercfg->debug_level == 5) {
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
    if(servercfg->debug && servercfg->debug_level == 5) {
      if(error_level != 0) {
	if(servercfg->debug && servercfg->debug_level == 5) {
	  sbuf << clear << "[" << seq_num << "]: ERROR - Parsing errors reading " << servercfg->rules_config_file;
	  LogDebugMessage(sbuf.c_str());
	}
      }
      else {
	if(servercfg->debug && servercfg->debug_level == 5) {
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
      if(servercfg->debug && servercfg->debug_level == 5) {
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
      client->Close();
      return 0;
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

  int num_connections = servercfg->get_num_server_connections();
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
    if(servercfg->debug && servercfg->debug_level == 5) {
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
    client->Close();
    if(servercfg->debug && servercfg->debug_level == 5) {
      sbuf << clear << "[" << seq_num << "]: Connection error will default to distributed round robin";
      LogDebugMessage(sbuf.c_str());
    }
    return round_robin(s, servercfg->distributed_rr_node_list);
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

  int num_connections = servercfg->get_num_server_connections();
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
    if(servercfg->debug && servercfg->debug_level == 5) {
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
    client->Close();
    if(servercfg->debug && servercfg->debug_level == 5) {
      sbuf << clear << "[" << seq_num << "]: Connection error will default to weighted round robin";
      LogDebugMessage(sbuf.c_str());
    }
    return round_robin(s, servercfg->weighted_rr_node_list);
  }

  return limit_ptr->data.active_ptr;
}


// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
