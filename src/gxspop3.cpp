// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxspop3.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/23/2001
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

Post Office Protocol 3 (POP3) classes used with applications
that require use of embedded POP3 client/server functions.

Changes:
==============================================================
09/22/2005:Modified gxsPOP3Client::RecvResponse() to signal 
a graceful socket disconnect.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "gxspop3.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

gxSocketError gxsPOP3Client::ConnectClient(const char *host, int port)
// Function used to connect a POP3 client to a server. Returns zero if no
// errors occur.
{
  if(InitSocketLibrary() == 0) {
    if(InitSocket(SOCK_STREAM, port, (char *)host) < 0) return socket_error;
  }
  else {
    return socket_error;
  }

  if(Connect() < 0) return socket_error;

  // Read the server's response
  if(!RecvResponse(reply_buf, gxsBUF_SIZE, "+OK")) return socket_error;
  return socket_error = gxSOCKET_NO_ERROR;
}

gxSocketError gxsPOP3Client::SendCommand(const char *command, 
					 const char *response, 
					 const char *args)
// Function used to send a command to a POP3 server and read the server's
// response. Returns zero if no errors occur.
{
  if(args) {
    sprintf(command_buf, "%s %s\r\n", command, args);
  }
  else {
    sprintf(command_buf, "%s\r\n", command);
  }
  int len = strlen(command_buf);
  
  // Send POP3 command using a blocking write
  if(Send(command_buf, len) < 0) return socket_error;
  
  // Read the server's response
  if(!RecvResponse(reply_buf, gxsBUF_SIZE, response)) return socket_error;
  return socket_error = gxSOCKET_NO_ERROR;
}

gxSocketError gxsPOP3Client::POP3Login(const char *username, 
				       const char *password) 
// Function used to logon to a POP3 server. Returns zero 
// no errors occur.
{
  if(SendCommand("USER", "+OK", username) != gxSOCKET_NO_ERROR) 
    return socket_error;
  if(SendCommand("PASS", "+OK", password) != gxSOCKET_NO_ERROR) 
    return socket_error;

  return socket_error = gxSOCKET_NO_ERROR;
}

gxSocketError gxsPOP3Client::POP3Logout()
// Function used to send the POP3 "QUIT" command. Returns zero 
// if no errors occur.
{
  return SendCommand("QUIT", "+OK");
}

gxSocketError gxsPOP3Client::POP3RSet()
// Function used to send the POP3 "Reset" command. Returns zero 
// if no errors occur.
{
  return SendCommand("RSET", "+OK");
}

gxSocketError gxsPOP3Client::POP3List(char *buf, int bytes)
// Function used to list all the message numbers on the POP3 server.
// Returns zero if no errors occur.
{
  strcpy(command_buf, "LIST\r\n");
  int len = strlen(command_buf);
  
  // Send POP3 command using a blocking write
  if(Send(command_buf, len) < 0) return socket_error;

  // Read the server's response
  if(!RecvResponse(buf, bytes, "\r\n.\r\n")) return socket_error;
  return socket_error = gxSOCKET_NO_ERROR;
}

gxSocketError gxsPOP3Client::POP3Retr(int message_number, 
				      char *buf, int bytes)
// Function used to retrieve a message from the POP3 server.
// Returns zero if no errors occur.
{
  sprintf(command_buf, "RETR %d\r\n", message_number);
  int len = strlen(command_buf);
  
  // Send POP3 command using a blocking write
  if(Send(command_buf, len) < 0) return socket_error;

  // Read the server's response
  if(!RecvResponse(buf, bytes, "\r\n.\r\n")) return socket_error;
  return socket_error = gxSOCKET_NO_ERROR;
}

gxSocketError gxsPOP3Client::POP3Delete(int message_number,
					char *buf, int bytes)
// Function used to delete a message from the POP3 server.
// Returns zero if no errors occur.
{
  sprintf(command_buf, "DELE %d\r\n", message_number);
  int len = strlen(command_buf);
  
  // Send POP3 command using a blocking write
  if(Send(command_buf, len) < 0) return socket_error;

  // Read the server's response
  if(!RecvResponse(buf, bytes, "+OK")) return socket_error;
  return socket_error = gxSOCKET_NO_ERROR;
}

gxSocketError gxsPOP3Client::POP3Top(int message_number, char *buf, int bytes)
// Function used to retrieve a message header from the POP3 server.
// Returns zero if no errors occur.
{
  sprintf(command_buf, "TOP %d 0\r\n", message_number);
  int len = strlen(command_buf);
  
  // Send POP3 command using a blocking write
  if(Send(command_buf, len) < 0) return socket_error;

  // Read the server's response
  if(!RecvResponse(buf, bytes, "\r\n.\r\n")) return socket_error;
  return socket_error = gxSOCKET_NO_ERROR;
}

int gxsPOP3Client::WaitForReply()
// Returns false if a reply time is longer then the timeout values. 
{
  return ReadSelect(gxsocket, time_out_sec, time_out_usec);
}

int gxsPOP3Client::RecvResponse(char *buf, int bytes, const char *response)
// Blocking receive function used to read a reply from an POP3 server
// following a command. If the specified response is not received within
// the timeout period this function will return false to indicate an error.
// Returns true if successful.
{
  bytes_read = 0;           // Reset the byte counter
  int num_read = 0;         // Actual number of bytes read
  int num_req = (int)bytes; // Number of bytes requested 
  char *p = buf;            // Pointer to the buffer

  while(bytes_read < bytes) { // Loop until the buffer is full
    if(!WaitForReply()) { 
      socket_error = gxSOCKET_REQUEST_TIMEOUT;      
      if(bytes_read >= 0) buf[bytes_read] = 0;
      return 0;
    }
    if((num_read = recv(gxsocket, p, num_req-bytes_read, 0)) > 0) {
      bytes_read += num_read;   // Increment the byte counter
      p += num_read;            // Move the buffer pointer for the next read
      
      // Search for a matching string
      char *pattern = (char *)response;
      char *next = buf;
      int i = 0;
      while(i < bytes_read && *pattern) {
	if(*next == *pattern) {
	  pattern++;
	  if(*pattern == 0) {
	    if(bytes_read >= 0) buf[bytes_read] = 0;
	    return 1; // Found matching string
	  }
	  next++;
	}
	else {
	  i++;
	  next++;
	  pattern = (char *)response;
	}
      } 
    }
    if(num_read == 0) {
      if(bytes_read >= 0) buf[bytes_read] = 0;
      socket_error = gxSOCKET_DISCONNECTED_ERROR;
      return 0; // An error occurred during the read
    }
    if(num_read < 0) {
      if(bytes_read >= 0) buf[bytes_read] = 0;
      socket_error = gxSOCKET_RECEIVE_ERROR;
      return 0; // An error occurred during the read
    }
  }

  // The receieve buffer is full - buffer overflow
  socket_error = gxSOCKET_BUFOVER_ERROR;
  if(bytes_read >= 0) buf[bytes_read] = 0;
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
