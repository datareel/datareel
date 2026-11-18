// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxspop3.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/23/2001
// Date Last Modified: 10/31/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- // 
// ------------- Program Description and Details ------------- // 
// ----------------------------------------------------------- // 
/*
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.
 
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library. If not, see
<https://www.gnu.org/licenses/>.

Post Office Protocol 3 (POP3) classes used with applications
that require use of embedded POP3 client/server functions.

Changes:
==============================================================
09/22/2005:Modified gxsPOP3Client::RecvResponse() to signal 
a graceful socket disconnect.

10/31/2026: Security fixes. SendCommand() used sprintf() into the
1024 byte command_buf, so a long user name or password overflowed it;
commands with CR or LF are refused. RecvResponse() searched for the
expected text anywhere in what it had read ("-ERR no +OK" was taken
as +OK), its search could read past the received data, a "-ERR" reply
to a multi-line command waited for a "." line until the timeout, and
the NUL was written one byte past a full buffer. Replies are now
checked by CheckReply(): the status line must start with "+OK", the
end of a multi-line reply is searched for after the status line, and
at most bytes-1 bytes are read.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "gxspop3.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

// 10/31/2026: Local helper
static int pop3_has_crlf(const char *s)
{
  if(!s) return 0;
  for(; *s; s++) if(*s == '\r' || *s == '\n') return 1;
  return 0;
}

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
  // 10/31/2026: sprintf() into command_buf overflowed with a long
  // argument, and CR LF in an argument added commands
  if(!command || !response) return socket_error = gxSOCKET_POP3_ERROR;
  if(pop3_has_crlf(command) || pop3_has_crlf(args)) {
    return socket_error = gxSOCKET_POP3_ERROR;
  }
  unsigned need = (unsigned)strlen(command) + 3;
  if(args) need += (unsigned)strlen(args) + 1;
  if(need > gxsBUF_SIZE) return socket_error = gxSOCKET_BUFOVER_ERROR;
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

int gxsPOP3Client::CheckReply(const char *buf, int len, 
				const char *response)
// 10/31/2026: Check a reply read so far. Returns 1 if buf holds the
// complete expected reply, 0 if more data is needed, or -1 if the
// server replied with an error or a status line that does not start
// with "+OK". A response starting with '+' or '-' ("+OK") is matched
// against the start of the status line. Any other response, such as
// "\r\n.\r\n" for a multi-line reply, is searched for from the end
// of a "+OK" status line on.
{
  if(!buf || len <= 0 || !response || !*response) return 0;
  int nl = 0;
  while(nl < len && buf[nl] != '\n') nl++;
  if(nl >= len) { // The status line has not ended
    if(len >= 4 && strncmp(buf, "-ERR", 4) == 0) return 0;
    if(buf[0] != '+' && buf[0] != '-') return -1;
    return 0;
  }
  if(response[0] == '+' || response[0] == '-') {
    unsigned n = (unsigned)strlen(response);
    if((unsigned)nl < n || strncmp(buf, response, n) != 0) return -1;
    return 1;
  }
  if(strncmp(buf, "+OK", 3) != 0) return -1;
  // Search from the CR LF that ends the status line, so the end of
  // an empty multi-line reply ("+OK\r\n.\r\n") is found
  int from = (nl > 0 && buf[nl-1] == '\r') ? nl - 1 : nl;
  int n = (int)strlen(response);
  for(int i = from; i + n <= len; i++) {
    if(memcmp(buf + i, response, n) == 0) return 1;
  }
  return 0;
}

int gxsPOP3Client::RecvResponse(char *buf, int bytes, const char *response)
// Blocking receive function used to read a reply from an POP3 server
// following a command. If the specified response is not received within
// the timeout period this function will return false to indicate an error.
// Returns true if successful.
// 10/31/2026: Replies are checked by CheckReply(), and at most bytes-1
// bytes are read so the NUL fits.
{
  bytes_read = 0;           // Reset the byte counter
  int num_read = 0;         // Actual number of bytes read
  if(!buf || bytes < 2 || !response) {
    socket_error = gxSOCKET_BUFOVER_ERROR;
    if(buf && bytes > 0) buf[0] = 0;
    return 0;
  }
  int limit = bytes - 1; // Room for the NUL
  buf[0] = 0;

  while(bytes_read < limit) { // Loop until the buffer is full
    if(!WaitForReply()) { 
      socket_error = gxSOCKET_REQUEST_TIMEOUT;      
      return 0;
    }
    num_read = recv(gxsocket, buf+bytes_read, limit-bytes_read, 0);
    if(num_read == 0) {
      socket_error = gxSOCKET_DISCONNECTED_ERROR;
      return 0; // An error occurred during the read
    }
    if(num_read < 0) {
      socket_error = gxSOCKET_RECEIVE_ERROR;
      return 0; // An error occurred during the read
    }
    bytes_read += num_read; // Increment the byte counter
    buf[bytes_read] = 0;
    int rv = CheckReply(buf, bytes_read, response);
    if(rv > 0) return 1; // Found matching string
    if(rv < 0) {
      socket_error = gxSOCKET_POP3_ERROR;
      return 0;
    }
  }

  // The receieve buffer is full - buffer overflow
  socket_error = gxSOCKET_BUFOVER_ERROR;
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
