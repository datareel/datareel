// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxsmtp.cpp
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

Simple Mail Transfer Protocol (SMTP) classes used with applications
that require use of embedded SMTP client/server functions.

Changes:
==============================================================
09/03/2002: The return type on gxsSMTPClient::GetSMTPTimeStamp()
was changed from void to char * to allow this function to be 
returned from another calling function.

09/03/2002: gxsSMTPClient::GetSMTPTimeStamp() now uses the 
SysTime class to format SMTP time/date strings.

09/19/2003: Modified gxsSMTPClient::RecvResponse() to return 
immediately if numeric response codes do not match and the entire 
SMTP response has been received.

09/22/2005: Modified gxsSMTPClient::RecvResponse() to signal 
a graceful socket disconnect.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "gxsmtp.h"
#include "systime.h"
#include <string.h>
#include <stdio.h>

gxSocketError gxsSMTPClient::ConnectClient(const char *host, int port)
// Function used to connect a SMTP client to a server. Returns zero if no
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
  // PC-lint 04/26/2005: Possible access of out-of-bounds pointer
  if(!RecvResponse(reply_buf, (gxsBUF_SIZE-1), "220")) return socket_error;
  return socket_error = gxSOCKET_NO_ERROR;
}

gxSocketError gxsSMTPClient::SendCommand(const char *command, 
					 const char *response, 
					 const char *args)
// Function used to send a command to an SMTP server and read the server's
// response. Returns zero if no errors occur.
{
  if(args) {
    sprintf(command_buf, "%s %s\r\n", command, args);
  }
  else {
    sprintf(command_buf, "%s\r\n", command);
  }
  int len = strlen(command_buf);
  
  // Send SMTP command using a blocking write
  if(Send(command_buf, len) < 0) return socket_error;

  // Read the server's response
  // PC-lint 04/26/2005: Possible access of out-of-bounds pointer
  if(!RecvResponse(reply_buf, (gxsBUF_SIZE-1), response)) return socket_error;
  return socket_error = gxSOCKET_NO_ERROR;
}

gxSocketError gxsSMTPClient::SMTPLogin(const char *domain_name) 
// Function used to send the SMTP "Hello" command. Returns zero if 
// no errors occur.
{
  return SendCommand("HELO", "250", domain_name);
}

gxSocketError gxsSMTPClient::ESMTPLogin(const char *client_name) 
// Login for ESMTP mail servers if roaming. Returns zero if 
// no errors occur.
{
  return SendCommand("EHLO", "250", client_name);
}

gxSocketError gxsSMTPClient::ESMTPAuthLogin(const char *b64_username, 
					    const char *b64_password)
{
  if(SendCommand("AUTH LOGIN", "334") != gxSOCKET_NO_ERROR) 
    return socket_error;
  if(SendCommand(b64_username, "334") != gxSOCKET_NO_ERROR) 
    return socket_error;
  if(SendCommand(b64_password, "235") != gxSOCKET_NO_ERROR) 
    return socket_error;
  
  return socket_error = gxSOCKET_NO_ERROR;
}

gxSocketError gxsSMTPClient::SMTPLogout()
// Function used to send the SMTP "QUIT" command. Returns zero if
// no errors occur.
{
  return SendCommand("QUIT", "221");
}

gxSocketError gxsSMTPClient::SMTPRSet()
// Function used to send the SMTP "Reset" command. Returns zero if
// no errors occur.
{
  return SendCommand("RSET", "250");
}

gxSocketError gxsSMTPClient::SMTPMailFrom(const char *from_email_address)
// Function used to send the SMTP "MAIL FROM" command. Returns zero if 
// no errors occur.
{
  return SendCommand("MAIL FROM:", "250", from_email_address);
}

gxSocketError gxsSMTPClient::SMTPRcptTo(const char *to_email_address)
// Function used to send the SMTP "RCPT TO" command. Returns zero if
// no errors occur.
{
  return SendCommand("RCPT TO:", "250", to_email_address);
}

gxSocketError gxsSMTPClient::SMTPData(const char *body_text, int body_len)
// Function used to send a text body. Returns zero if no errors occur.
{
  if(SendCommand("DATA", "354") != gxSOCKET_NO_ERROR) return socket_error;

  // Send the message body
  if(Send(body_text, body_len) < 0) return socket_error;

  // Send the end of message sequence
  strcpy(command_buf, "\r\n.\r\n");
  int len = strlen(command_buf);
  if(Send(command_buf, len) < 0) return socket_error;
  
  // Read the server's response
  // PC-lint 04/26/2005: Possible access of out-of-bounds pointer
  if(!RecvResponse(reply_buf, (gxsBUF_SIZE-1), "250")) return socket_error;
  return socket_error = gxSOCKET_NO_ERROR;
}

int gxsSMTPClient::WaitForReply()
// Returns false if a reply time is longer then the timeout values. 
{
  return ReadSelect(gxsocket, time_out_sec, time_out_usec);
}

int gxsSMTPClient::RecvResponse(char *buf, int bytes, const char *response)
// Blocking receive function used to read a reply from an SMTP server
// following a command. If the specified response is not received within
// the timeout period this function will return false to indicate an error.
// Returns true if successful.
{
  bytes_read = 0;           // Reset the byte counter
  int num_read = 0;         // Actual number of bytes read
  int num_req = (int)bytes; // Number of bytes requested 
  char *p = buf;            // Pointer to the buffer
  int len = 3;

  while(bytes_read < bytes) { // Loop until the buffer is full
    if(!WaitForReply()) { 
      socket_error = gxSOCKET_REQUEST_TIMEOUT;      
      if(bytes_read >= 0) buf[bytes_read] = 0;
      return 0;
    }
    if((num_read = recv(gxsocket, p, num_req-bytes_read, 0)) > 0) {
      bytes_read += num_read;   // Increment the byte counter
      p += num_read;            // Move the buffer pointer for the next read

      if(buf[bytes_read-1] == '\n') { // We received the entire reply line
	buf[bytes_read] = 0; // NULL terminate the reply
	if(bytes_read > len) { // Look for numeric response code
	  if(strncmp(response, buf, len) == 0) {
	    return 1; // Found matching numeric reply code
	  }
	  else {
	    // Server replied with an error
	    socket_error = gxSOCKET_SMTP_ERROR;
	    return 0;
	  }
	}
	else { // Invalid reply code
	  socket_error = gxSOCKET_SMTP_ERROR;
	  return 0;
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


gxSocketError gxsSMTPClient::SMTPSendMessage(const char *to, 
					     const char *from, 
					     const char *subject, 
					     const char *body)
// Function used to send a formatted message. Returns zero if no errors
// occur.
{
  if(SMTPRSet() != gxSOCKET_NO_ERROR) return socket_error;
  if(SMTPMailFrom(from) != gxSOCKET_NO_ERROR)
    return socket_error;
  if(SMTPRcptTo(to) != gxSOCKET_NO_ERROR)
    return socket_error;
  if(SendCommand("DATA", "354") != gxSOCKET_NO_ERROR) return socket_error;

  // Construct a message header
  char systime[gxsBUF_SIZE];
  GetSMTPTimeStamp(systime);
  unsigned header_len = strlen(from) + strlen(to) + strlen(subject) + \
    strlen(systime);

  header_len += 50; // Allocate space for additional formatting
  char *message_header = new char[header_len+1];
  if(!message_header) return socket_error = gxSOCKET_BUFOVER_ERROR;
  
  // Format the message header
  // 11/17/2007: Fix for LF problem
  // SMTP protocol reported an error condition
  // 451 See http://pobox.com/~djb/docs/smtplf.html
  // http://cr.yp.to/docs/smtplf.html
  sprintf(message_header, "Date: %s\r\nFrom: %s\r\nTo: %s\r\nSubject: %s\r\n",
	  systime, from, to, subject);

  // Send the message header
  if(Send(message_header, (int)strlen(message_header)) < 0) {
    // PC-lint 04/26/2005: Avoid inappropriate de-allocation error,
    // but message_header is not a user defined type so there is no destructor
    // to call for the array members.
    delete[] message_header;
    return socket_error;
  }

  // Send the body of the message
  if(Send(body, (int)strlen(body)) < 0) {
    // PC-lint 04/26/2005: Avoid inappropriate de-allocation error,
    // but message_header is not a user defined type so there is no destructor
    // to call for the array members.
    delete[] message_header;
    return socket_error;
  }
  
  // Send the end of message sequence
  // PC-lint 04/26/2005: Avoid inappropriate de-allocation error,
  // but message_header is not a user defined type so there is no destructor
  // to call for the array members.
  delete[] message_header;

  strcpy(command_buf, "\r\n.\r\n");
  int len = strlen(command_buf);
  if(Send(command_buf, len) < 0) return socket_error;
  
  // Read the server's response
  if(!RecvResponse(reply_buf, gxsBUF_SIZE, "250")) return socket_error;
  return socket_error = gxSOCKET_NO_ERROR;
}

char *gxsSMTPClient::GetSMTPTimeStamp(char *sbuf)
// Passes back an SMTP time string in the following format:
// Weekday, Day Month Year HH:MM:SS - Timezone
{
  // Prevent program crash if sbuf is null
  if(!sbuf) {
    sbuf = new char[gxsBUF_SIZE];
    if(!sbuf) return 0;
  }
  SysTime smtp_timedate;
  strcpy(sbuf, smtp_timedate.GetSMTPTime());
  return sbuf;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
