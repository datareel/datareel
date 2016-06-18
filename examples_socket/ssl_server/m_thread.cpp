// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: m_thread.cpp
// C++ Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 10/17/2001
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

Multi-threaded server framework using the gxSSL and gxThread 
libraries.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include <string.h>
#include "m_thread.h"

// --------------------------------------------------------------
// Globals variable initialzation
// --------------------------------------------------------------
ServerConfig ServerConfigSruct;
ServerConfig *servercfg = &ServerConfigSruct;
// --------------------------------------------------------------

void ClientSocket_t::FreeSSL() 
{
  if(openssl) {
    openssl->ReleaseCTX(); // Do not free Context by ~gxSSL() call
    delete openssl; 
    openssl = 0;
  }
}

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

  int val = 1;
  if(SetSockOpt(gxsocket, SOL_SOCKET, SO_REUSEADDR,
		&val, sizeof(val)) < 0) {
    CheckSocketError((gxSocket *)this, "Error initializing server"); 
    Close();
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
    if(Accept() < 0) {
      PrintMessage("Error accepting client connections");
      return 0;
    }

    // NOTE: Getting client info for statistical purposes only
    char client_name[gxsMAX_NAME_LEN]; int r_port = -1;
    GetClientInfo(client_name, r_port);
    PrintMessage("Received client request from ", client_name);

    ClientSocket_t *s = new ClientSocket_t;
    if(!s) {
      ReportError("A fatal memory allocation error occurred\n \
Shutting down the server");
      CloseSocket();
      return (void *)0;
    }
    
    SSL_CTX_set_cipher_list(openssl.GetCTX(), "ALL");

    // Record the file descriptor assigned by the Operating System
    s->client_socket = GetRemoteSocket();

    s->openssl = new gxSSL(openssl.GetCTX());
    if(!s->openssl) {
      PrintMessage("A fatal memory allocation error occurred\n \
Shutting down the server");
      delete s;
      return (void *)0;
    }

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
  s->openssl->CloseSSLSocket();
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
  s->openssl->CloseSSLSocket();
  Close(s->client_socket);
  delete s;
}

void ClientRequestThread::HandleClientRequest(ClientSocket_t *s)
// Function used to handle a client request.
{
  if(s->openssl->OpenSSLSocket(s->client_socket) != gxSSL_NO_ERROR) {
    PrintMessage("Error opening SSL socket");
    PrintMessage(s->openssl->SSLErrorMessage());
    socket_error = gxSOCKET_SSL_ERROR;
    return;
  }
  
  if(s->openssl->AcceptSSLSocket() != gxSSL_NO_ERROR) {
    PrintMessage("Error accepting SSL socket");
    PrintMessage(s->openssl->SSLErrorMessage());
    socket_error = gxSOCKET_SSL_ERROR;
    gxString err;
    gxSSL::GlobalSSLError(err);
    PrintMessage(err.c_str());
    return;
  }

  // NOTE: The demo processes a single HTTP GET request per client
  const int rxbuf = 1024;
  char request[rxbuf];

  // Block until the client sends some data
  int rv = s->openssl->RawReadSSLSocket(request, rxbuf);

  // A socket error occurred
  if(rv <= 0) {
    PrintMessage("Error reading SSL socket");
    PrintMessage(s->openssl->SSLErrorMessage());
    socket_error = gxSOCKET_RECEIVE_ERROR;
    return;
  }

  request[rv] = 0; // Null terminate the request string

  // At this point we need to parse the client request and process it.
  // For simplicity sake these operations have been omitted.
  // ... (parsing operation)
  // ... (processing operation)

  // Simply send a test page (with a header) back to the client.
  const char *test_page = "HTTP/1.0 200 OK\r\nServer: gxThread\r\nConnection: \
close\r\n\r\n<HTML>\n<HEAD>\n<TITLE> gxThread Test Page </TITLE>\n</HEAD>\
<BODY><CENTER><H1>gxThread Test Page</H1></CENTER><HR><PRE>Response from the \
multi-threaded HTTP server</PRE><HR><CENTER>End of document</CENTER>\n</BODY>\
\n</HTML>";

  // Blocking send that will not return until all bytes are written  
  //  Send(s->client_socket, test_page, strlen(test_page));
  //  CheckSocketError((gxSocket *)this, "Error sending page to client");

  rv = s->openssl->SendSSLSocket(test_page, strlen(test_page));
  if(s->openssl->HasError()) {
    if(s->openssl->GetSSLError() == gxSSL_DISCONNECTED_ERROR) {
      PrintMessage("Error writing to SSL socket");
      PrintMessage(s->openssl->SSLErrorMessage());
      socket_error = gxSOCKET_DISCONNECTED_ERROR; 
    }
    else {
      PrintMessage("Error writing to SSL socket");
      PrintMessage(s->openssl->SSLErrorMessage());
      socket_error = gxSOCKET_TRANSMIT_ERROR; 
    }
  }
  if(rv != (int)strlen(test_page)) {
    PrintMessage("Error writing to SSL socket");
    PrintMessage(s->openssl->SSLErrorMessage());
    socket_error = gxSOCKET_TRANSMIT_ERROR; 
  }
  CheckSocketError((gxSocket *)this, "Error sending page to client");
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
