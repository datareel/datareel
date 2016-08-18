// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 06/17/2016
// Date Last Modified: 07/16/2016
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

Multi-threaded framework Datareel load balancer.

*/
// ----------------------------------------------------------- //   
#ifndef __DRLB_MTHREAD_HPP__
#define __DRLB_MTHREAD_HPP__

#include "gxdlcode.h"

#include "gxmutex.h"
#include "gxcond.h"
#include "gxthread.h"
#include "gxsocket.h"
#include "dfileb.h"
#include "devcache.h"
#include "memblock.h"
#include "membuf.h"

#include "drlb_server.h"
#include "m_socket.h"

// Client socket type used to associate client sockets other data types.
struct ClientSocket_t
{
  ClientSocket_t() {
    seq_num = 0;
    r_port = 1;
    node = 0;
  }
  ~ClientSocket_t();

  gxsSocket_t client_socket; // Frontend client socket
  gxSocket client;  // Backend client 
  int seq_num; // Sequence number for log entries
  gxString client_name;
  LBnode *node;
  int r_port;
};

// Hold vars for socket read/write threads
struct SocketWR_t
{
  SocketWR_t() {
    log_str = "Socket";
    read_sock = write_sock = -1; 
    seq_num = 0;
    buffer = 0;
    buffer_size = 0;
    bytes_read = 0;
    node = 0;
 }
  ~SocketWR_t();

  gxString log_str;
  gxsSocket_t read_sock; 
  gxsSocket_t write_sock; 
  int seq_num;
  char *buffer;
  unsigned buffer_size;
  gxList<MemoryBuffer *> cache;
  int bytes_read;
  LBnode *node;
};

class SocketWRThread : public gxSocket, public gxThread
{
public:
  SocketWRThread() { }
  ~SocketWRThread() { }

public:
  int read_socket(SocketWR_t *s);
  int write_socket(SocketWR_t *s);
  int read_socket_cached(SocketWR_t *s);
  int write_socket_cached(SocketWR_t *s);

private: // gxThread Interface
  void *ThreadEntryRoutine(gxThread_t *thread);
  void ThreadExitRoutine(gxThread_t *thread);
  void ThreadCleanupHandler(gxThread_t *thread);
};

class LBClientRequestThread : public gxSocket, public gxThread
{
public:
  LBClientRequestThread() { 
    frontend_rw_thread = 0;
    backend_rw_thread = 0;
  }
  ~LBClientRequestThread() { 
    if(frontend_rw_thread) delete frontend_rw_thread;
    if(backend_rw_thread) delete backend_rw_thread;
  }

public: // Client routines
  void HandleClientRequest(ClientSocket_t *s);
  LBnode *round_robin(ClientSocket_t *s, gxList<LBnode *> &node_list);
  LBnode *round_robin(ClientSocket_t *s);
  LBnode *assigned(ClientSocket_t *s);
  LBnode *distrib(ClientSocket_t *s);
  LBnode *weighted(ClientSocket_t *s);
  int LB_ReadWrite(ClientSocket_t *s, int buffer_size);
  int LB_CachedReadWrite(ClientSocket_t *s, int buffer_size);

private: // gxThread Interface
  void *ThreadEntryRoutine(gxThread_t *thread);
  void ThreadExitRoutine(gxThread_t *thread);
  void ThreadCleanupHandler(gxThread_t *thread);

private:
  SocketWRThread socket_rw_thread;
  gxThread_t *frontend_rw_thread;
  gxThread_t *backend_rw_thread;
};

class LBServerThread : public gxSocket, public gxThread
{
public:
  LBServerThread() { }
  ~LBServerThread() { }

public: // Server functions
  int InitServer(int max_connections = SOMAXCONN);

private: // gxThread Interface
  void *ThreadEntryRoutine(gxThread_t *thread);
  void ThreadExitRoutine(gxThread_t *thread);
  void ThreadCleanupHandler(gxThread_t *thread);
  
public: // Server data members
  LBClientRequestThread request_thread;
};

// --------------------------------------------------------------
// Standalone functions
// --------------------------------------------------------------
int get_fd_error(int &error_number, gxString &sbuf);
// --------------------------------------------------------------

#endif // __DRLB_MTHREAD_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
