// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: client.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
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
#include <stdio.h>
#include "m_thread.h"

// --------------------------------------------------------------
// Globals
// --------------------------------------------------------------
char *server_ip;
gxsPort_t port = 7;
int echo_loop = 1;
int num_clients = 1;
int packet_size = 1500;
// --------------------------------------------------------------

class ConsoleThread : public gxThread
{
public:
  ConsoleThread() { }
  ~ConsoleThread() { }

private:
  void *ThreadEntryRoutine(gxThread_t *thread);
};

class EchoThread : public gxThread
{
public:
  EchoThread() { }
  ~EchoThread() { }

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

void *EchoThread::ThreadEntryRoutine(gxThread_t *thread)
{
  // Extract the client number from the thread parameter
  int num = *((int *)thread->GetThreadParm());
  char sbuf[255];
  sprintf(sbuf, "%i", num);

  PrintMessage("Constructing stream client ", sbuf);
  gxSocket client(SOCK_STREAM, port, server_ip);
  if(!client) {
    CheckSocketError(&client);
    return (void *)0;
  }

  // Connect to the server
  if(client.Connect() < 0) {
    CheckSocketError(&client, "Could not connect to the server");
    client.Close();
    return (void *)0;
  }

  char *packet = new char[packet_size];

  while(echo_loop) {
    // Blocking write used to send the packet to the server
    int rv = client.Send((char *)packet, packet_size);
    if(rv < 0) {
      if(client.GetSocketError() != gxSOCKET_TRANSMIT_ERROR) {
	CheckSocketError(&client, "Error writing to server socket");
      }
      else {
	PrintMessage("Server has disconnected during a socket write");
      }
      delete[] packet;
      client.Close();
      return (void *)0;
    }
    
    int bytes_read = 0; // Total bytes read
 
    // Read the bytes sent back from the server
    while(bytes_read < packet_size) {

      // Block until the server sends some data
      rv = client.RawRead((char *)packet, packet_size);
      if(rv < 0) {
	if(client.GetSocketError() != gxSOCKET_RECEIVE_ERROR) {
	  CheckSocketError(&client, "Error reading from server socket");
	}
	else {
	  PrintMessage("Server has disconnected during a socket read");
	}
	delete[] packet;
	client.Close();
	return (void *)0;
      }
      bytes_read += rv;
    }

    // Client send and receive complete
    
    // Sleep to conserve CPU resources
    mSleep(500); // Sleep for 500 ms before sending the next packet
  }

  client.Close();
  delete[] packet;
  return (void *)0;
}

int main(int argc, char **argv)
{
  if((argc < 2) || (argc > 5)) {
    cerr << "Usage: " << argv[0] 
	 << " server [port] [num_clients] [packet_size]" << "\n" << flush;
    return 1;
  }

  server_ip = argv[1];
  if(argc >= 3) port = (unsigned short)atoi(argv[2]);
  if(argc >= 4) num_clients = (unsigned short)atoi(argv[3]);
  if(argc == 5) packet_size = (unsigned short)atoi(argv[4]);

  cout << "Multi-client connection and bandwidth tester" << "\n" << flush;
  cout << "Connecting " << num_clients << " to " << server_ip << "\n" << flush;
  cout << "Data packet size minus overhead: " << packet_size << "\n" << flush;
  cout << "\n" << flush;
  cout << "Press Enter to start test or C-Ctrl to exit" << "\n" << flush;
  cout << "Enter quit to exit program after the entering connection test" 
       << "\n" << flush;

  EchoThread client;
  thrPool *client_pool = new thrPool;
  if(!client_pool) {
    cout << "Could not allocate memory for the client thread pool" << "\n" 
	 << flush;
    cout << "Exiting..." << "\n" << flush;
    return 1;
  }

  for(int i = 0; i < num_clients; i++) {
    gxThread_t *client_thread = client.CreateThread(client_pool, 
						    (void *)&i);
    if(client_thread->GetThreadError() != gxTHREAD_NO_ERROR) {
      cout << client_thread->ThreadExceptionMessage() << "\n" << flush;
      echo_loop = 0;
      client.DestroyThreadPool(client_pool, 0);
      return 1;
    }
    client.sSleep(1);
  }
  
  ConsoleThread console;
  gxThread_t *console_thread = console.CreateThread();
  if(console_thread->GetThreadError() != gxTHREAD_NO_ERROR) {
    cout << console_thread->ThreadExceptionMessage() << "\n" << flush;
    echo_loop = 0;
    client.DestroyThreadPool(client_pool, 0);
    return 1;
  }
    
  if(console.JoinThread(console_thread) != 0) {
    cout << "Could not join the console thread" << "\n" << flush;
  }

  // Destroy the serial port receiver thread
  cout << "Stopping all client threads..." << "\n" << flush;
  echo_loop = 0;
  client.DestroyThreadPool(client_pool, 0);

  cout << "Exiting..." << "\n" << flush;
  delete console_thread;
  return 0;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
