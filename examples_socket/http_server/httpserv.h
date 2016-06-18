// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: httpserv.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
// Date Last Modified: 06/17/2016
// Copyright (c) 2001-2016 DataReel Software Development
// ----------------------------------------------------------- // 
// ---------- Include File Description and Details  ---------- // 
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

Multi-threaded server framework using the gxSocket and gxThread 
libraries.
*/
// ----------------------------------------------------------- //   
#ifndef __GX_S_MTHREAD_SERVER_HPP__
#define __GX_S_MTHREAD_SERVER_HPP__

#include "gxdlcode.h"

#include "gxmutex.h"
#include "gxcond.h"
#include "gxthread.h"
#include "gxsocket.h"

// --------------------------------------------------------------
// Constants
// --------------------------------------------------------------
// Thread constants
const int DISPLAY_THREAD_RETRIES = 3;
// --------------------------------------------------------------

// Server configuration
struct  ServerConfig {
  ServerConfig();
  ~ServerConfig();
  
  // Server configuration variables
  gxsPort_t port;     // Server's port number
  int accept_clients; // True while accepting 

  // gxThread variables
  thrPool *client_request_pool; // Worker threads processing client requests

  // gxThread synchronization interface
  gxMutex display_lock;      // Mutex object used to lock the display
  gxCondition display_cond;  // Condition variable used with the display lock
  int display_is_locked;     // Display lock Boolean predicate
};

// Client socket type used to associate client sockets other
// data types.
struct ClientSocket_t
{
  gxsSocket_t client_socket; // Client socket
};

class ClientRequestThread : public gxSocket, public gxThread
{
public:
  ClientRequestThread() { }
  ~ClientRequestThread() { }

public: // Client routines
  void HandleClientRequest(ClientSocket_t *s);

private: // gxThread Interface
  void *ThreadEntryRoutine(gxThread_t *thread);
  void ThreadExitRoutine(gxThread_t *thread);
  void ThreadCleanupHandler(gxThread_t *thread);
};

class ServerThread : public gxSocket, public gxThread
{
public:
  ServerThread() { }
  ~ServerThread() { }

public: // Server functions
  int InitServer(int max_connections = SOMAXCONN);

private: // gxThread Interface
  void *ThreadEntryRoutine(gxThread_t *thread);
  void ThreadExitRoutine(gxThread_t *thread);
  void ThreadCleanupHandler(gxThread_t *thread);
  
public: // Server data members
  ClientRequestThread request_thread;
};

// --------------------------------------------------------------
// Standalone functions
// --------------------------------------------------------------
void PrintMessage(const char *s1 = " ", const char *s2 = " ", 
		  const char *s3 = " ");
void ReportError(const char *s1 = " ", const char *s2 = " ", 
		 const char *s3 = " ");
int CheckSocketError(gxSocket *s, const char *mesg = 0, 
		     int report_error = 1);
int CheckThreadError(gxThread_t *thread, const char *mesg = 0, 
		     int report_error = 1);
// --------------------------------------------------------------

// --------------------------------------------------------------
// Globals configuration variables
// --------------------------------------------------------------
extern ServerConfig ServerConfigSruct;
extern ServerConfig *servercfg;
// --------------------------------------------------------------

#endif // __GX_S_MTHREAD_SERVER_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
