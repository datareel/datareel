// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxsmtp.cpp
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

10/31/2026: Security fixes. SendCommand() used sprintf() into the
1024 byte command_buf, so a long address or argument overflowed it;
commands with CR or LF are refused. RecvResponse() stopped at the end
of the first line, so the rest of a multi-line reply ("250-...") was
read as the reply to the next command; replies are now parsed by
ParseReply() up to the last line, the code must be 3 digits, and
RecvResponse() no longer writes one byte past the buffer when it is
full. SMTPMailFrom() and SMTPRcptTo() send "MAIL FROM:<address>" made
by NormalizeAddress(), which refuses addresses with spaces, angle 
brackets or control characters inside or anything after the '>' (ESMTP
parameter injection such as "a@b> NOTIFY=..." or CR LF). SMTPData()
and SMTPSendMessage() dot-stuff the message and send every line end as
CR LF, so a "." line or a bare LF in the body cannot end the message 
early (SMTP smuggling); SMTPSendMessage() refuses CR or LF in the To,
From and Subject values (header injection).
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "gxsmtp.h"
#include "systime.h"
#include "membuf.h"
#include <string.h>
#include <stdio.h>

// 10/31/2026: Local helpers
static int smtp_has_crlf(const char *s)
{
  if(!s) return 0;
  for(; *s; s++) if(*s == '\r' || *s == '\n') return 1;
  return 0;
}

static int smtp_is_digit(char c) { return c >= '0' && c <= '9'; }

static void smtp_dot_stuff(const char *text, int len, MemoryBuffer &out)
// Copy a message body to out with every line end sent as CR LF (a 
// bare CR or LF too) and a '.' added before a line that starts with
// '.' (RFC 5321 4.5.2). A body line "." ended the message early and
// what followed was read by the server as commands.
{
  out.Clear();
  int line_start = 1;
  for(int i = 0; i < len; i++) {
    char c = text[i];
    if(c == '\r' || c == '\n') {
      if(c == '\r' && i + 1 < len && text[i+1] == '\n') i++;
      out.Cat("\r\n", 2);
      line_start = 1;
      continue;
    }
    if(line_start && c == '.') out.Cat('.');
    out.Cat(c);
    line_start = 0;
  }
}

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
  // 10/31/2026: sprintf() into command_buf overflowed with a long
  // argument, and CR LF in an argument added commands
  if(!command || !response) return socket_error = gxSOCKET_SMTP_ERROR;
  if(smtp_has_crlf(command) || smtp_has_crlf(args)) {
    return socket_error = gxSOCKET_SMTP_ERROR;
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

int gxsSMTPClient::NormalizeAddress(const char *address, gxString &out,
				    int allow_empty)
// 10/31/2026: Turn "user@host", "<user@host>" or "Name <user@host>"
// into "<user@host>". Returns false if the address holds spaces, '<',
// '>' or control characters, has more than one <...> or anything after
// the '>'. "<>" (the null sender) needs allow_empty. SMTPMailFrom() 
// and SMTPRcptTo() sent the address as given, so "a@b> NOTIFY=..."
// added ESMTP parameters.
{
  out.Clear();
  if(!address) return 0;
  const char *b = address;
  const char *e = address + strlen(address);
  while(b < e && (*b == ' ' || *b == '\t')) b++;
  while(e > b && (e[-1] == ' ' || e[-1] == '\t')) e--;
  const char *lt = 0;
  for(const char *p = b; p < e; p++) if(*p == '<') { lt = p; break; }
  const char *ib = b, *ie = e; // The address inside the brackets
  if(lt) {
    // "Name <user@host>" keeps only the address part. There must be
    // one '>' and it must end the string.
    const char *gt = 0;
    for(const char *p = b; p < e; p++) if(*p == '>') { gt = p; break; }
    if(!gt || gt < lt || gt + 1 != e) return 0;
    for(const char *p = b; p < lt; p++) { // Display name
      unsigned char c = (unsigned char)*p;
      if(c < 0x20 || c == 0x7F) return 0;
    }
    ib = lt + 1;
    ie = gt;
  }
  for(const char *p = ib; p < ie; p++) {
    unsigned char c = (unsigned char)*p;
    if(c <= 0x20 || c == 0x7F || c == '<' || c == '>') return 0;
  }
  if(ib == ie && !allow_empty) return 0;
  out = "<";
  if(ie > ib) out.Cat(ib, (unsigned)(ie - ib));
  out += ">";
  return 1;
}

int gxsSMTPClient::ParseReply(const char *buf, int len, int *code, 
			      int *consumed)
// 10/31/2026: Parse one reply ("250-..." lines up to "250 ...") from
// the start of buf. Returns 1 if buf holds a complete reply, with the
// code and the number of bytes used passed back, 0 if the lines so far
// are valid but the reply has not ended, or -1 if a line does not start
// with a 3 digit code followed by a space, '-' or the line end.
{
  if(code) *code = 0;
  if(consumed) *consumed = 0;
  if(!buf || len <= 0) return 0;
  int pos = 0;
  for(int lines = 0; lines < 1000; lines++) {
    int nl = pos;
    while(nl < len && buf[nl] != '\n') nl++;
    if(nl >= len) { // An incomplete line: check what there is of it
      int n = len - pos;
      for(int i = 0; i < n && i < 3; i++) {
	if(!smtp_is_digit(buf[pos+i])) return -1;
      }
      if(n > 3 && buf[pos+3] != ' ' && buf[pos+3] != '-' && 
	 buf[pos+3] != '\r') return -1;
      return 0;
    }
    int line_len = nl - pos;
    if(line_len > 0 && buf[nl-1] == '\r') line_len--;
    const char *line = buf + pos;
    if(line_len < 3 || !smtp_is_digit(line[0]) || !smtp_is_digit(line[1]) ||
       !smtp_is_digit(line[2]) || line[0] < '1' || line[0] > '5' ||
       (line_len > 3 && line[3] != ' ' && line[3] != '-')) return -1;
    pos = nl + 1;
    if(line_len == 3 || line[3] == ' ') {
      if(code) *code = ((line[0]-'0')*100) + ((line[1]-'0')*10) + (line[2]-'0');
      if(consumed) *consumed = pos;
      return 1;
    }
  }
  return -1; // Too many lines
}

gxSocketError gxsSMTPClient::SMTPMailFrom(const char *from_email_address)
// Function used to send the SMTP "MAIL FROM" command. Returns zero if 
// no errors occur.
{
  // 10/31/2026: "MAIL FROM:<address>", refusing addresses that would
  // add ESMTP parameters or commands
  gxString a;
  if(!NormalizeAddress(from_email_address, a, 1)) {
    return socket_error = gxSOCKET_SMTP_ERROR;
  }
  gxString cmd("MAIL FROM:");
  cmd += a;
  return SendCommand(cmd.c_str(), "250");
}

gxSocketError gxsSMTPClient::SMTPRcptTo(const char *to_email_address)
// Function used to send the SMTP "RCPT TO" command. Returns zero if
// no errors occur.
{
  // 10/31/2026: "RCPT TO:<address>", refusing addresses that would
  // add ESMTP parameters or commands
  gxString a;
  if(!NormalizeAddress(to_email_address, a, 0)) {
    return socket_error = gxSOCKET_SMTP_ERROR;
  }
  gxString cmd("RCPT TO:");
  cmd += a;
  return SendCommand(cmd.c_str(), "250");
}

gxSocketError gxsSMTPClient::SMTPData(const char *body_text, int body_len)
// Function used to send a text body. Returns zero if no errors occur.
{
  if(!body_text || body_len < 0) return socket_error = gxSOCKET_SMTP_ERROR;
  if(SendCommand("DATA", "354") != gxSOCKET_NO_ERROR) return socket_error;

  // Send the message body
  // 10/31/2026: dot-stuffed with CR LF line ends
  MemoryBuffer body;
  smtp_dot_stuff(body_text, body_len, body);
  if(body.length() > 0) {
    if(Send(body.m_buf(), (int)body.length()) < 0) return socket_error;
  }

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
// 10/31/2026: Reads the whole reply, up to the line with a space after
// the code, using ParseReply(). It returned at the end of the first
// line of a multi-line reply ("250-..."), so the rest was read as the
// reply to the next command. The code (3 digits) is compared with the
// first characters of response. At most bytes-1 bytes are kept (the 
// NUL was written one byte past a full buffer); when a long reply
// fills the buffer, its earlier complete lines are dropped.
{
  bytes_read = 0;  // Reset the byte counter
  int num_read = 0; // Actual number of bytes read
  int dropped = 0;  // Bytes of earlier lines dropped from a long reply
  if(!buf || bytes < 2 || !response) {
    socket_error = gxSOCKET_BUFOVER_ERROR;
    if(buf && bytes > 0) buf[0] = 0;
    return 0;
  }
  int limit = bytes - 1; // Room for the NUL
  buf[0] = 0;

  for(;;) {
    if(bytes_read >= limit) {
      // Keep the incomplete last line and drop the complete lines
      // before it, which ParseReply() has checked
      int last = bytes_read;
      while(last > 0 && buf[last-1] != '\n') last--;
      dropped += last;
      if(last == 0 || dropped > 1024*1024) { 
	// The receieve buffer is full - buffer overflow
	socket_error = gxSOCKET_BUFOVER_ERROR;
	return 0;
      }
      memmove(buf, buf+last, bytes_read-last);
      bytes_read -= last;
      buf[bytes_read] = 0;
    }
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
    buf[bytes_read] = 0;    // NULL terminate the reply

    int code = 0;
    int rv = ParseReply(buf, bytes_read, &code);
    if(rv < 0) { // Invalid reply code
      socket_error = gxSOCKET_SMTP_ERROR;
      return 0;
    }
    if(rv == 0) continue; // The reply has not ended
    char code_str[4];
    code_str[0] = (char)('0' + (code / 100));
    code_str[1] = (char)('0' + ((code / 10) % 10));
    code_str[2] = (char)('0' + (code % 10));
    code_str[3] = 0;
    unsigned n = (unsigned)strlen(response);
    if(n > 3) n = 3;
    if(strncmp(response, code_str, n) == 0) {
      return 1; // Found matching numeric reply code
    }
    // Server replied with an error
    socket_error = gxSOCKET_SMTP_ERROR;
    return 0;
  }
}


gxSocketError gxsSMTPClient::SMTPSendMessage(const char *to, 
					     const char *from, 
					     const char *subject, 
					     const char *body)
// Function used to send a formatted message. Returns zero if no errors
// occur.
{
  // 10/31/2026: CR or LF in these values added message headers
  if(!to || !from || !subject || !body) {
    return socket_error = gxSOCKET_SMTP_ERROR;
  }
  if(smtp_has_crlf(to) || smtp_has_crlf(from) || smtp_has_crlf(subject)) {
    return socket_error = gxSOCKET_SMTP_ERROR;
  }
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
  // 10/31/2026: dot-stuffed with CR LF line ends
  MemoryBuffer stuffed;
  smtp_dot_stuff(body, (int)strlen(body), stuffed);
  if(stuffed.length() > 0 && 
     Send(stuffed.m_buf(), (int)stuffed.length()) < 0) {
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
