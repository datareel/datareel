// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Header File Name: gxspop3.h 
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/23/2001
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

Post Office Protocol 3 (POP3) classes used with applications
that require use of embedded POP3 client/server functions.
*/
// ----------------------------------------------------------- // 
#ifndef __GX_EMBEDDED_POP3_LIB_HPP__
#define __GX_EMBEDDED_POP3_LIB_HPP__

#include "gxdlcode.h"

#include "gxsocket.h"

// POP3 client - See RFC 1225, 1460, 1725 for specifications
class GXDLCODE_API gxsPOP3Client : public gxSocket
{
public:
  gxsPOP3Client() { 
    time_out_sec = 60; // Default blocking timeout value   
    time_out_usec = 0;
    reply_buf[0] = 0; 
    command_buf[0] = 0;
  }
  ~gxsPOP3Client() { }

public:
  gxSocketError ConnectClient(const char *host, 
			       int port = gxSOCKET_POP3_PORT);
  gxSocketError SendCommand(const char *command, const char *response, 
			     const char *args = 0);
  gxSocketError POP3Login(const char *username, const char *password);
  gxSocketError POP3Logout();
  gxSocketError POP3RSet();
  gxSocketError POP3List(char *buf, int bytes);
  gxSocketError POP3Retr(int message_number, char *buf, int bytes);
  gxSocketError POP3Delete(int message_number, char *buf, int bytes);
  gxSocketError POP3Top(int message_number, char *buf, int bytes);
  int RecvResponse(char *buf, int bytes, const char *response);
  int WaitForReply();
  void SetTimeOut(int seconds, int useconds) {
    time_out_sec = seconds;
    time_out_usec = useconds;
  }

public:
  char reply_buf[gxsBUF_SIZE];   // Buffer used to hold the last reply
  char command_buf[gxsBUF_SIZE]; // Buffer used to hold the last command
  int time_out_sec;  // Number of seconds before a blocking timeout
  int time_out_usec; // Number of microseconds before a blocking timeout
};

#endif // __GX_EMBEDDED_POP3_LIB_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
