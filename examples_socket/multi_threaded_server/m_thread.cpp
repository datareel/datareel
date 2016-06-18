// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: m_thread.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
// Date Last Modified: 06/17/2016
// Copyright (c) 2001-2016 DataReel Software Development
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

Multi-threaded server framework using the gxSocket and gxThread 
libraries.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include "m_thread.h"

// --------------------------------------------------------------
// Globals variable initialzation
// --------------------------------------------------------------
ServerConfig ServerConfigSruct;
ServerConfig *servercfg = &ServerConfigSruct;
// --------------------------------------------------------------

int CheckSocketError(gxSocket *s, const char *mesg, int report_error)
// Test the socket for an error condition and report the message
// if the "report_error" variable is true. The "mesg" string is used
// display a message with the reported error. Returns true if an
// error was detected for false of no errors where detected. 
{
  if(s->GetSocketError() == gxSOCKET_NO_ERROR) {
    // No socket errors reported
    return 0;
  }
  if(report_error) { // Reporting the error to an output device
    if(mesg) {
      PrintMessage(mesg, "\n", s->SocketExceptionMessage());
    }
    else {
      PrintMessage(s->SocketExceptionMessage());
    }
  }
  return 1;
}

int CheckThreadError(gxThread_t *thread, const char *mesg, int report_error)
// Test the thread for an error condition and report the message
// if the "report_error" variable is true. The "mesg" string is used
// display a message with the reported error. Returns true if an
// error was detected for false of no errors where detected. 
{
  if(thread->GetThreadError() == gxTHREAD_NO_ERROR) {
    // No thread errors reported
    return 0;
  }
  if(report_error) { // Reporting the error to an output device
    if(mesg) {
      PrintMessage(mesg, "\n", thread->ThreadExceptionMessage()); 
    }
    else {
      PrintMessage(thread->ThreadExceptionMessage());
    }
  }
  return 1;
}

void ReportError(const char *s1, const char *s2, const char *s3)
// Thread safe error reporting function.
{
  PrintMessage(s1, s2, s3);
}

void PrintMessage(const char *s1, const char *s2, const char *s3)
// Thread safe write function that will not allow access to
// the critical section until the write operation is complete.
{
  servercfg->display_lock.MutexLock();

  // Ensure that all threads have finished writing to the device
  int num_try = 0;
  while(servercfg->display_is_locked != 0) {
    // Block this thread from its own execution if a another thread
    // is writing to the device
    if(++num_try < DISPLAY_THREAD_RETRIES) {
      servercfg->display_cond.ConditionWait(&servercfg->display_lock);
    }
    else {
      return; // Could not write string to the device
    }
  }

  // Tell other threads to wait until write is complete
  servercfg->display_is_locked = 1; 

  // ********** Enter Critical Section ******************* //
  GXSTD::cout << "\n";
  if(s1) GXSTD::cout << s1;
  if(s2) GXSTD::cout << s2;
  if(s3) GXSTD::cout << s3;
  GXSTD::cout << "\n";
  GXSTD::cout.flush(); // Flush the ostream buffer to the stdio
  // ********** Leave Critical Section ******************* //

  // Tell other threads that this write is complete
  servercfg->display_is_locked = 0; 
 
  // Wake up the next thread waiting on this condition
  servercfg->display_cond.ConditionSignal();
  servercfg->display_lock.MutexUnlock();
}

ServerConfig::ServerConfig()
{
  port = 80;
  accept_clients = 1;
  echo_loop = 1;
  display_is_locked = 0;
  client_request_pool = new thrPool;
}

ServerConfig::~ServerConfig()
{
  if(client_request_pool) delete client_request_pool;
}

int ServerThread::InitServer(int max_connections)
// Initialize the server. Returns a non-zero value if any
// errors ocurr.
{
  if(InitSocketLibrary() == 0) {
    if(InitSocket(SOCK_STREAM, servercfg->port) < 0) return 1;
  }
  else {
    return 1;
  }

  // Bind the name to the socket
  if(Bind() < 0) {
    CheckSocketError((gxSocket *)this, "Error initializing server"); 
    Close();
    return 1;
  }
    
  // Listen for connections with a specified backlog
  if(Listen(max_connections) < 0) {
    CheckSocketError((gxSocket *)this, "Error initializing server");
    Close();
    return 1;
  }

  return 0;
}

void *ServerThread::ThreadEntryRoutine(gxThread_t *thread)
{
  while(servercfg->accept_clients) { // Loop accepting client connections

    // Block the server until a client requests service
    if(Accept() < 0) continue;

    // NOTE: Getting client info for statistical purposes only
    char client_name[gxsMAX_NAME_LEN]; int r_port = -1;
    GetClientInfo(client_name, r_port);
    PrintMessage("Connecting to client: ", client_name);

    ClientSocket_t *s = new ClientSocket_t;
    if(!s) {
      ReportError("A fatal memory allocation error occurred\n \
Shutting down the server");
      CloseSocket();
      return (void *)0;
    }
    
    // Record the file descriptor assigned by the Operating System
    s->client_socket = GetRemoteSocket();

    // Destroy all the client threads that have exited
    RebuildThreadPool(servercfg->client_request_pool);

    // Create thread per cleint
    gxThread_t *rthread = \
      request_thread.CreateThread(servercfg->client_request_pool, (void *)s);
    if(CheckThreadError(rthread, "Error starting client request thread")) {
      delete s;
      return (void *)0;
    }
  }

  return (void *)0;
}

void ServerThread::ThreadExitRoutine(gxThread_t *thread)
// Thread exit function used to close the server thread.
{
  CloseSocket(); // Close the server side socket
}

void ServerThread::ThreadCleanupHandler(gxThread_t *thread)
// Thread cleanup handler used in the event that the thread is
// canceled.
{
  CloseSocket(); // Close the server side socket
}

void *ClientRequestThread::ThreadEntryRoutine(gxThread_t *thread)
{
  // Extract the client socket from the thread parameter
  ClientSocket_t *s = (ClientSocket_t *)thread->GetThreadParm();
  
  // Process the client request
  HandleClientRequest(s);

  return (void *)0;
}

void ClientRequestThread::ThreadExitRoutine(gxThread_t *thread)
// Thread exit function used to close the client thread.
{
  // Extract the client socket from the thread parameter
  ClientSocket_t *s = (ClientSocket_t *)thread->GetThreadParm();

  // Close the client socket and free its resources
  Close(s->client_socket);
  delete s;
}

void ClientRequestThread::ThreadCleanupHandler(gxThread_t *thread)
// Thread cleanup handler used in the event that the thread is
// canceled.
{
  // Extract the client socket from the thread parameter
  ClientSocket_t *s = (ClientSocket_t *)thread->GetThreadParm();

  // Close the client socket and free its resources
  Close(s->client_socket);
  delete s;
}

void ClientRequestThread::HandleClientRequest(ClientSocket_t *s)
// Function used to handle a client request.
{
  const int buf_len = 1500; // Receive buffer size
  char echo_buffer[buf_len];

  while(servercfg->echo_loop) {
    // Block until the client sends some data
    int br = RawRead(s->client_socket, echo_buffer, buf_len);
    if(br < 0) {
      if(GetSocketError() != gxSOCKET_RECEIVE_ERROR) {
	CheckSocketError((gxSocket *)this, "Error reading client socket");
      }
      else {
	PrintMessage("Client has disconnected during a socket read");
      }
      return;
    }
    
    while(br > 0) {
      // Echo the message back to the client
      int bm = RawWrite(s->client_socket, echo_buffer, br);
      if(bm < 0) {
	if(GetSocketError() != gxSOCKET_TRANSMIT_ERROR) {
	  CheckSocketError((gxSocket *)this, "Error writing to client socket");
	}
	else {
	  PrintMessage("Client has disconnected during a socket write");
	}
	return;
      }
      
      // Keep receiving data from the client
      br = RawRead(s->client_socket, echo_buffer, buf_len);
      if(br < 0) {
	if(GetSocketError() != gxSOCKET_RECEIVE_ERROR) {
	  CheckSocketError((gxSocket *)this, "Error reading client socket");
	}
	else {
	  PrintMessage("Client has disconnected during a socket read");
	}
	return;
      }
    }
  }
  PrintMessage("Client has disconnected");
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
