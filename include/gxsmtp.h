// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Header File Name: gxsmtp.h
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

Simple Mail Transfer Protocol (SMTP) classes used with applications
that require use of embedded SMTP client/server functions.

Changes:
==============================================================
09/03/2002: The return type on the gxsSMTPClient::GetSMTPTimeStamp()
was changed from void to char * to allow this function to be 
returned from another calling function.

03/10/2003: Changed the default blocking timeout value from 5
seconds to 60 seconds to allow for slower mail server to respond
to SMTP commands. 

06/09/2003: Added the gxsSMTPClient::ESMTPLogin() and 
gxsSMTPClient::ESMTPAuthLogin() functions to allow mail clients 
the use SMTP authentication as require by ESMTP servers when 
a client is accessing the mail server from a different domain.
==============================================================
*/
// ----------------------------------------------------------- // 
#ifndef __GX_EMBEDDED_SMTP_LIB_HPP__
#define __GX_EMBEDDED_SMTP_LIB_HPP__

#include "gxdlcode.h"

#include "gxsocket.h"

// SMTP client - See RFC 821 for specifications
// http://www.sendmail.org
class GXDLCODE_API gxsSMTPClient : public gxSocket
{
public:
  gxsSMTPClient() { 
    time_out_sec = 60; // Default blocking timeout value   
    time_out_usec = 0;
    reply_buf[0] = 0; 
    command_buf[0] = 0;
  }
  ~gxsSMTPClient() { }

public:
  gxSocketError ConnectClient(const char *host, 
			       int port = gxSOCKET_SMTP_PORT);
  gxSocketError SendCommand(const char *command, const char *response, 
			     const char *args = 0);
  gxSocketError SMTPLogin(const char *domain_name);
  gxSocketError ESMTPLogin(const char *client_name);
  gxSocketError ESMTPAuthLogin(const char *b64_username, 
			       const char *b64_password);
  gxSocketError SMTPLogout();
  gxSocketError SMTPRSet();
  gxSocketError SMTPMailFrom(const char *from_email_address);
  gxSocketError SMTPRcptTo(const char *to_email_address);
  gxSocketError SMTPData(const char *body_text, int body_len);
  int RecvResponse(char *buf, int bytes, const char *response);
  int WaitForReply();
  void SetTimeOut(int seconds, int useconds) {
    time_out_sec = seconds;
    time_out_usec = useconds;
  }
  gxSocketError SMTPSendMessage(const char *to, const char *from, 
				const char *subject, const char *body);
  char *GetSMTPTimeStamp(char *sbuf);

public:
  char reply_buf[gxsBUF_SIZE];   // Buffer used to hold the last reply
  char command_buf[gxsBUF_SIZE]; // Buffer used to hold the last command
  int time_out_sec;  // Number of seconds before a blocking timeout
  int time_out_usec; // Number of microseconds before a blocking timeout
};

#endif // __GX_EMBEDDED_SMTP_LIB_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
