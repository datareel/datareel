// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: server.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 04/28/2000
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

This is a test program used demonstrate the use of the gxSocket
class in a multi-threaded application.
*/
// ----------------------------------------------------------- //   
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include <string.h>
#include <stdlib.h>
#include "httpserv.h"

class ConsoleThread : public gxThread
{
public:
  ConsoleThread() { }
  ~ConsoleThread() { }

private:
  void *ThreadEntryRoutine(gxThread_t *thread);
};

void *ConsoleThread::ThreadEntryRoutine(gxThread_t *thread)
{
  const int cmd_len = 255;
  char sbuf[cmd_len];
  
  while(1) {
    for(int i = 0; i < cmd_len; i++) sbuf[i] = 0;
    cin >> sbuf;
    if(strcmp(sbuf, "quit") == 0) break;
    if(strcmp(sbuf, "exit") == 0) break;
    cout << "Invalid command" << "\n" << flush;
    cout << "Enter quit to exit" << "\n" << flush;
  }
  return (void *)0;
}

int main(int argc, char **argv)
{
  // Check arguments. Should be only one: the port number to bind to.
  if(argc != 2) {
    cerr << "Usage: " << argv[0] << " port" << "\n" << flush;
    return 1;
  }

  ServerThread server;
  servercfg->port = (gxsPort_t) atoi(argv[1]);

  cout << "Initializing HTTP server..." << "\n" << flush;
  int rv = server.InitServer(servercfg->port);
  if(rv != 0) {
    cout << server.SocketExceptionMessage() << "\n" << flush;
    return 1;
  }

  // Get the host name assigned to this machine
  char hostname[gxsMAX_NAME_LEN];
  rv = server.GetHostName(hostname);
  if(rv != 0) {
    cout << server.SocketExceptionMessage() << "\n" << flush;
    return 1;
  }

  cout << "Opening server on host " << hostname << "\n" << flush;
  cout << "Enter quit to exit" << "\n" << flush;
  cout << "\n" << flush;
  
  cout << "Listening on port " << servercfg->port << "\n" << flush;
  cout.flush();

 gxThread_t *server_thread = server.CreateThread();
  if(server_thread->GetThreadError() != gxTHREAD_NO_ERROR) {
    cout << server_thread->ThreadExceptionMessage() << "\n" << flush;
    return 1;
  }
  
  ConsoleThread console;
  gxThread_t *console_thread = console.CreateThread();
  if(console_thread->GetThreadError() != gxTHREAD_NO_ERROR) {
    cout << console_thread->ThreadExceptionMessage() << "\n" << flush;
    return 1;
  }
    
  if(console.JoinThread(console_thread) != 0) {
    cout << "Could not join the console thread" << "\n" << flush;
  }

  // Destroy the serial port receiver thread
  cout << "Stopping the server thread..." << "\n" << flush;
  servercfg->accept_clients = 0;

  // Cannot the server thread due to the blocking accept() function, so
  // cancel the thread before it is destroyed
  server.CancelThread(server_thread);
  if(server_thread->GetThreadState() == gxTHREAD_STATE_CANCELED) {
    cout << "The server thread was stopped" << "\n" << flush;
  }
  else {
    cout << "Could not cancel the server thread" << "\n" << flush;
  }

  if(server.JoinThread(server_thread) != 0) {
    cout << "Could not join the server thread" << "\n" << flush;
  }
    
  delete server_thread;
  delete console_thread;

  cout << "Exiting..." << "\n" << flush;
  return 0;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //


  

