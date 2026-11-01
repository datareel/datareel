// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxftp.cpp
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

File Transfer Protocol (FTP) classes used with applications
that require use of embedded FTP client/server functions.

Changes:
==============================================================
06/10/2002: Modified the gxsFTPClient::RecvResponse(gxsSocket_t s)
function to check for FTP error codes 400 and higher instead of
waiting for a timeout error.

06/10/2002: Modified the following functions to support
passive FTP mode: gxsFTPClient::FTPGet(), gxsFTPClient::FTPPut(),
gxsFTPClient::FTPList(), gxsFTPClient::OpenDataPort(),
gxsFTPClient::ReadDataPort(), gxsFTPClient::WriteDataPort()

06/17/2002: Modified the gxsFTPClient::SendCommand() function to
clear the command and reply buffers before send a command. This 
change was made to ensure that strings are always null-terminated.

06/17/2002: Modified the gxsFTPClient::FTPGet() and 
gxsFTPClient::FTPList() functions not to read the server 226
reply string following a list or a get. This change was made 
because the FTP client was hanging on the blocking receive 
while waiting for the servers reply. This problem occurred 
when connecting to Windows based FTP servers such as the WAR
FTP server and the IIS FTP server. 

06/17/2002: Modified the gxsFTPClient::OpenDataPort() function
to check for 226 and 227 replies from the server when in passive
FTP mode.

06/20/2002: Modified the gxsFTPClient::RecvResponse() function 
to handle 110, 120, 125, 202, 225, and 332 FTP reply codes.

12/03/2002: Modified the gxsFTPClient:::FTPLogin() function to 
handle Welcome messages that send multiple 230 FTP reply codes. 
The last line of the Welcome message will be acknowledged when 
the terminating "230 " is received.

12/03/2002: Modified the gxsFTPClient::: FTPLogout () function to 
handle Exit messages that send multiple 221 FTP reply codes. The 
last line of the Exit message will be acknowledged when the 
terminating "221 " is received.

09/22/2005:Modified gxsFTPClient::RecvResponse() to signal 
a graceful socket disconnect.

10/31/2026: Security fixes. SendCommand(), FTPGet(), FTPPut() and
FTPList() used sprintf() into fixed buffers, so a long file name or
argument overflowed them; commands with CR or LF are refused.
RecvResponse() wrote the NUL one byte past a full buffer, used the
wrong byte counter (bytes_read) on a disconnect, read the reply code
from data that was not NUL terminated, and took the expected code
anywhere in the reply. Replies are now parsed by ParseReply() (3 digit
codes, multi-line replies up to the last line); a 226 reply left over
from the previous transfer is skipped. The PASV reply was parsed with
sscanf("%s") into 25 byte stack buffers (a long reply overflowed
them) and "(p1*256)+p2" with unchecked numbers; ParsePASVPort() 
checks each field (0-255), and the data connection now goes to the
server's own address, never to the address in the PASV reply (a
server could point the client at another host). FTPList() wrote the
NUL one byte past a full buffer.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <string.h>
#include <stdio.h>
#include "gxsftp.h"

// 10/31/2026: Local helpers
static int ftp_has_crlf(const char *s)
{
  if(!s) return 0;
  for(; *s; s++) if(*s == '\r' || *s == '\n') return 1;
  return 0;
}

static int ftp_is_digit(char c) { return c >= '0' && c <= '9'; }

#ifdef __BCC32__
#pragma warn -8080
#endif

gxSocketError gxsFTPClient::ConnectClient(const char *host, int port)
// Function used to connect a FTP client to a server. Returns zero if no
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
  if(!RecvResponse(reply_buf, gxsFTP_BUF_SIZE, "220 ")) return socket_error;

  return socket_error = gxSOCKET_NO_ERROR;
}

gxSocketError gxsFTPClient::SendCommand(const char *command, 
					const char *response, 
					const char *args)
// Function used to send a command to an FTP server and read the server's
// response. Returns zero if no errors occur.
{
  // 10/31/2026: sprintf() into command_buf overflowed with a long
  // argument, and CR LF in an argument added commands
  if(!command || !response) return socket_error = gxSOCKET_FTP_ERROR;
  if(ftp_has_crlf(command) || ftp_has_crlf(args)) {
    return socket_error = gxSOCKET_FTP_ERROR;
  }
  unsigned need = (unsigned)strlen(command) + 3;
  if(args) need += (unsigned)strlen(args) + 1;
  if(need > gxsBUF_SIZE) return socket_error = gxSOCKET_BUFOVER_ERROR;

  // Clear the command and response buffers
  unsigned i;
  for(i = 0; i < gxsBUF_SIZE; i++) command_buf[i] = 0;
  for(i = 0; i < gxsFTP_BUF_SIZE; i++) reply_buf[i] = 0;

 if(args) {
    sprintf(command_buf, "%s %s\r\n", command, args);
  }
  else {
    sprintf(command_buf, "%s\r\n", command);
  }
  int len = strlen(command_buf);
  
  // Send command using a blocking write
  if(Send(command_buf, len) < 0) return socket_error;
  
  // Read the server's response
  if(!RecvResponse(reply_buf, gxsFTP_BUF_SIZE, response)) return socket_error;
  return socket_error = gxSOCKET_NO_ERROR;
}


gxSocketError gxsFTPClient::FTPLogin(const char *username, 
				     const char *password) 
// Function used to logon to an FTP server. Returns zero 
// if no errors occur.
{
  if(SendCommand("USER", "331", username) != gxSOCKET_NO_ERROR) 
    return socket_error;
  if(SendCommand("PASS", "230 ", password) != gxSOCKET_NO_ERROR) 
    return socket_error;
  return socket_error = gxSOCKET_NO_ERROR;
}

gxSocketError gxsFTPClient::FTPLogout()
// Function used to send the FTP "bye" command. Returns zero if
// no errors occur.
{
  return SendCommand("QUIT", "221 ");
}

gxSocketError gxsFTPClient::FTPImageType(char type)
{
  if(type == 'a' || type == 'A') // ASCII
    return SendCommand("TYPE", "200", "A");
  else // Default to binary
    return SendCommand("TYPE", "200", "I");
}

gxSocketError gxsFTPClient::FTPChDir(const char *dname)
{
  return SendCommand("CWD", "250", dname);
}

gxSocketError gxsFTPClient::FTPPWD()
{
  return SendCommand("PWD", "257");
}

gxSocketError gxsFTPClient::FTPStat()
{
  return SendCommand("STAT", "211 ");
}

gxSocketError gxsFTPClient::FTPMkDir(const char *dname)
{
  return SendCommand("MKD", "257", dname);
}

gxSocketError gxsFTPClient::FTPRmDir(const char *dname)
{
  return SendCommand("RMD", "250", dname);
}

gxSocketError gxsFTPClient::FTPSize(const char *fname)
{
  return SendCommand("SIZE", "213", fname);
}

gxSocketError gxsFTPClient::FTPDelete(const char *fname)
{
 return SendCommand("DELE", "250", fname);
}

gxSocketError gxsFTPClient::FTPMove(const char *from, const char *to)
{
  if(SendCommand("RNFR", "350", from) != gxSOCKET_NO_ERROR) 
    return socket_error;
  return SendCommand("RNTO", "250", to);
}

gxSocketError gxsFTPClient::FTPGet(const char *fname, DiskFileB &stream,
				   FAU_t &bytes)
{
  char get_buf[gxsFTP_BUF_SIZE];
  bytes = (FAU_t)0;
  // 10/31/2026: A long file name overflowed get_buf
  if(!fname || strlen(fname) > gxsBUF_SIZE) {
    return socket_error = gxSOCKET_BUFOVER_ERROR;
  }
  sprintf(get_buf, "RETR %s", fname);
  if(OpenDataPort() != gxSOCKET_NO_ERROR) return socket_error; 

  char rx_buf[gxsFTP_BUF_SIZE]; // Receive buffer
  int num_read = 0;         // Actual number of bytes read
  
  if(SendCommand(get_buf, "150") != gxSOCKET_NO_ERROR) {
    CloseDataPort();
    return socket_error; 
  }

  reply_buf[0] = 0; // Reset the reply buffer

  if(passive_mode == 0) { // Active FTP mode
    // Block execution until data is received
    if(ftp_data.Accept() < 0) {
      CloseDataPort();
      return SetSocketError(ftp_data.GetSocketError());
    }
  }

  while(1){
    num_read = ReadDataPort(rx_buf, sizeof(rx_buf));
    if(num_read > 0) {
      if(stream.df_Write(rx_buf, num_read) != DiskFileB::df_NO_ERROR) {
	CloseDataPort();
	return socket_error = gxSOCKET_FILESYSTEM_ERROR;
      }
      bytes += num_read;
    }
    else {
      break;
    }
  }
  if(num_read < 0) {
    CloseDataPort();
    return socket_error = gxSOCKET_RECEIVE_ERROR;
  }

  // Close the data port before the next read
  CloseDataPort();

  // Hanging intermittently when used with WIN32 ftp server's
  // so do not read the server's reply 
  strcpy(reply_buf, "226 Transfer complete.\r\n");

  return socket_error = gxSOCKET_NO_ERROR;
}

gxSocketError gxsFTPClient::FTPPut(const char *fname, DiskFileB &stream,
				   FAU_t &bytes)
{
  char put_buf[gxsFTP_BUF_SIZE];
  char tx_buf[gxsFTP_BUF_SIZE];
  bytes = (FAU_t)0;
  // 10/31/2026: A long file name overflowed put_buf
  if(!fname || strlen(fname) > gxsBUF_SIZE) {
    return socket_error = gxSOCKET_BUFOVER_ERROR;
  }
  sprintf(put_buf, "STOR %s", fname);
  if(OpenDataPort() != gxSOCKET_NO_ERROR) return socket_error; 

  if(SendCommand(put_buf, "150") != gxSOCKET_NO_ERROR) {
    CloseDataPort();
    return socket_error; 
  }

  reply_buf[0] = 0; // Reset the reply buffer

  // Block execution until data is received
  if(passive_mode == 0) { // Active FTP mode
    if(ftp_data.Accept() < 0) {
      CloseDataPort();
      return SetSocketError(ftp_data.GetSocketError());
    }
  }

  while(!stream.df_EOF()) {
    if(stream.df_Read(tx_buf, gxsFTP_BUF_SIZE) != DiskFileB::df_NO_ERROR) {
      if(stream.df_GetError() == DiskFileB::df_EOF_ERROR) {
	stream.df_ClearError(); // Clear EOF errors
      }
      else { // Fatal read error
	CloseDataPort();
	return socket_error = gxSOCKET_FILESYSTEM_ERROR;
      }
    }
    unsigned byte_count = stream.df_gcount();
    if(WriteDataPort(tx_buf, byte_count) < 0) {
      CloseDataPort();
      return socket_error;
    }
    bytes += byte_count;
  }
  CloseDataPort();
  return socket_error = gxSOCKET_NO_ERROR;
}

gxSocketError gxsFTPClient::FTPList(char *sbuf, unsigned bytes, int full,
				    const char *args)
// FTP command used to list the contents of the current working
// directory. Returns zero if no errors occur.
{
  char ls_buf[gxsFTP_BUF_SIZE];

  // 10/31/2026: A long argument overflowed ls_buf, and the NUL was
  // written one byte past a full sbuf
  if(args && strlen(args) > gxsBUF_SIZE) {
    return socket_error = gxSOCKET_BUFOVER_ERROR;
  }
  if(!sbuf || bytes < 2) return socket_error = gxSOCKET_BUFOVER_ERROR;
  bytes--; // Room for the NUL

  if(full) {
    if(args) {
      sprintf(ls_buf, "LIST %s", args);
    }
    else {
      strcpy(ls_buf, "LIST");
    }
  }
  else {
    if(args) {
      sprintf(ls_buf, "NLST %s", args);
    }
    else {
      strcpy(ls_buf, "NLST");
    }
  }

  if(OpenDataPort() != gxSOCKET_NO_ERROR) {
    return socket_error; 
  }

  int byte_count = 0;       // Data port byte counter
  int num_read = 0;         // Actual number of bytes read
  int num_req = (int)bytes; // Number of bytes requested 
  char *p = sbuf;           // Pointer to the buffer
  

  if(SendCommand(ls_buf, "150") != gxSOCKET_NO_ERROR) {
    CloseDataPort();
    return socket_error; 
  }

  reply_buf[0] = 0; // Reset the reply buffer

  if(passive_mode == 0) { // Active FTP mode
    // Block execution until data is received
    if(ftp_data.Accept() < 0) {
      CloseDataPort();
      return SetSocketError(ftp_data.GetSocketError());
    }
  }
  
  while(byte_count < (int)bytes) { // Loop until the buffer is full
    num_read = ReadDataPort(p, num_req-byte_count);
    if(num_read > 0) {
      byte_count += num_read; // Increment the byte counter
      if(byte_count >= (int)bytes) {
	// The receieve buffer is full - buffer overflow
	if(byte_count >= 0) sbuf[byte_count] = 0;
	CloseDataPort();
	return socket_error = gxSOCKET_BUFOVER_ERROR;
      } 
      p += num_read; // Move the buffer pointer for the next read
      if(num_read < 0) {
	if(byte_count >= 0) sbuf[byte_count] = 0;
	CloseDataPort();
	return socket_error = gxSOCKET_RECEIVE_ERROR;
      }
    }
    else {
      break;
    }
  }
  
  // Null terminate the string buffer
  if(byte_count >= 0) sbuf[byte_count] = 0;
  if(num_read < 0) {
    CloseDataPort();
    return socket_error = gxSOCKET_RECEIVE_ERROR;
  }

  // Close the data port before the next read
  CloseDataPort();

  // Hanging intermittently when used with WIN32 ftp server's
  // so do not read the server's reply 
  strcpy(reply_buf, "226 Transfer complete.\r\n");

  return socket_error = gxSOCKET_NO_ERROR;
}

gxSocketError gxsFTPClient::OpenDataPort()
{
  int reuse_opt = 1; // Socket reuse option

  if(passive_mode == 1) {

    // Send the passive FTP mode command instead of the port command
    if(SendCommand("PASV", "227") != gxSOCKET_NO_ERROR) {
      return socket_error; 
    }

    // NOTE: A 226 reply left over from the last transfer is skipped
    // by RecvResponse().
    // 10/31/2026: The reply was parsed with sscanf("%s") into 25 byte
    // stack buffers and the port computed from unchecked numbers.
    // ParsePASVPort() checks the fields. The data connection goes to
    // the address of the control connection: the address in the reply
    // is not used, so a server cannot send the client to another host.
    int port = ParsePASVPort(reply_buf);
    if(port <= 0) return socket_error = gxSOCKET_FTP_ERROR;
    gxsSocketAddress data_sin;
    memmove(&data_sin, &sin, sizeof(data_sin));
    data_sin.sin_port = htons((unsigned short)port);
    if(ftp_data.InitSocket(SOCK_STREAM, &data_sin) < 0) {
      return SetSocketError(ftp_data.GetSocketError());
    }
    if(ftp_data.SetSockOpt(SOL_SOCKET, SO_REUSEADDR, 
			   &reuse_opt, sizeof(reuse_opt)) < 0) {
      return SetSocketError(ftp_data.GetSocketError());
    }
    if(ftp_data.Connect() < 0) { // Connect to the server
      return SetSocketError(ftp_data.GetSocketError());
    }
  }
  else {
    if(ftp_data.InitSocket(SOCK_STREAM, 0) < 0) {
      return SetSocketError(ftp_data.GetSocketError());
    }
    if(ftp_data.SetSockOpt(SOL_SOCKET, SO_REUSEADDR, 
			   &reuse_opt, sizeof(reuse_opt)) < 0) {
      return SetSocketError(ftp_data.GetSocketError());
    }
    if(ftp_data.Bind() < 0) { // Bind the socket
      return SetSocketError(ftp_data.GetSocketError());
    }
    if(ftp_data.GetSockName() < 0) {
      return SetSocketError(ftp_data.GetSocketError());
    }	      
    if(ftp_data.Listen(1) < 0) { // Listen for connection
      return SetSocketError(ftp_data.GetSocketError());
    }
    
    // Get this machine's IP address and send it to the FTP server
    char hostname[gxsMAX_NAME_LEN];
    if(GetHostName(hostname) < 0) return socket_error;
    gxsHostNameInfo *hostinfo = GetHostInformation(hostname);
    if(!hostinfo) {
      return socket_error = gxSOCKET_HOSTNAME_ERROR;
    }
    for(int i = 0; ; i++) {
      gxsInternetAddress *ialist = \
	(gxsInternetAddress *)hostinfo->h_addr_list[i];
      if(ialist == 0) break;
      memmove(&ftp_data.sin.sin_addr.s_addr, ialist, 
	      sizeof(gxsInternetAddress));
    }
    delete hostinfo;
    char *a = (char *) &ftp_data.sin.sin_addr;
    char *b = (char *) &ftp_data.sin.sin_port;
    
    char port_buf[255];
    sprintf(port_buf, "PORT %d,%d,%d,%d,%d,%d", (a[0]&0xff), (a[1]&0xff), 
	    (a[2]&0xff), (a[3]&0xff), (b[0]&0xff), (b[1]&0xff));
    
    // Port the client is listening on
    ftp_data_port = (int)ntohs(ftp_data.sin.sin_port); 
    
    // Send the port command
    if(SendCommand(port_buf, "200") != gxSOCKET_NO_ERROR) {
      return socket_error; 
    }
  }

  return socket_error = gxSOCKET_NO_ERROR;
}

void gxsFTPClient::CloseDataPort()
{
  ftp_data.Close();
  ftp_data_port = 0;
}

int gxsFTPClient::ReadDataPort(void *buf, unsigned bytes)
{

  if(passive_mode == 1) {
    return ftp_data.RawRead(buf, bytes);
  }
  else { // Active ftp mode
    return ftp_data.RawRemoteRead(buf, bytes);
  }
}

int gxsFTPClient::WriteDataPort(void *buf, unsigned bytes)
{
  if(passive_mode == 1) {
    return ftp_data.Send(buf, bytes); // Blocking client write
  }
  else { // Active ftp mode
    return ftp_data.RemoteSend(buf, bytes); // Blocking server write
  }
}

int gxsFTPClient::WaitForReply(gxsSocket_t s)
// Returns false if a reply time is longer then the timeout values. 
{
  return ReadSelect(s, time_out_sec, time_out_usec);
}

int gxsFTPClient::RecvResponse(char *buf, int bytes, const char *response)
{
  return RecvResponse(gxsocket, buf, bytes, response);
}

int gxsFTPClient::ParseReply(const char *buf, int len, int *code, 
			     int *consumed)
// 10/31/2026: Parse one reply ("150-..." lines up to "150 ...") from
// the start of buf. Returns 1 if buf holds a complete reply, with the
// code and the number of bytes used passed back, 0 if the reply has
// not ended, or -1 if its first line does not start with a 3 digit
// code (1-5) followed by a space, '-' or the line end. Lines of a 
// multi-line reply between the first and last line can hold any text
// (RFC 959 4.2).
{
  if(code) *code = 0;
  if(consumed) *consumed = 0;
  if(!buf || len <= 0) return 0;
  int nl = 0;
  while(nl < len && buf[nl] != '\n') nl++;
  int n = (nl < len) ? nl : len; // Length of the first line so far
  if(n > 0 && nl < len && buf[n-1] == '\r') n--;
  for(int i = 0; i < n && i < 3; i++) {
    if(!ftp_is_digit(buf[i])) return -1;
  }
  if(n > 0 && (buf[0] < '1' || buf[0] > '5')) return -1;
  if(n > 3 && buf[3] != ' ' && buf[3] != '-' && buf[3] != '\r') return -1;
  if(nl >= len) return 0; // The first line has not ended
  if(n < 3) return -1;
  int c = ((buf[0]-'0')*100) + ((buf[1]-'0')*10) + (buf[2]-'0');
  int pos = nl + 1;
  if(n > 3 && buf[3] == '-') { // Multi-line: up to "ddd " or "ddd" alone
    for(int lines = 0; ; lines++) {
      if(lines > 10000) return -1;
      int e = pos;
      while(e < len && buf[e] != '\n') e++;
      if(e >= len) return 0; // The reply has not ended
      int line_len = e - pos;
      if(line_len > 0 && buf[e-1] == '\r') line_len--;
      const char *line = buf + pos;
      pos = e + 1;
      if(line_len >= 3 && memcmp(line, buf, 3) == 0 &&
	 (line_len == 3 || line[3] == ' ')) break;
    }
  }
  if(code) *code = c;
  if(consumed) *consumed = pos;
  return 1;
}

int gxsFTPClient::ParsePASVPort(const char *reply)
// 10/31/2026: Data port from "227 Entering Passive Mode 
// (h1,h2,h3,h4,p1,p2)", with or without the parentheses. Each field
// must be a number from 0 to 255. Returns 1-65535, or -1 if the reply
// is not valid.
{
  if(!reply || strncmp(reply, "227", 3) != 0) return -1;
  const char *p = reply + 3;
  const char *open = strchr(p, '(');
  if(open) {
    p = open + 1;
  }
  else { // Some servers leave out the parentheses (RFC 1123 4.1.2.6)
    while(*p && *p != '\n' && !ftp_is_digit(*p)) p++;
  }
  long v[6];
  for(int i = 0; i < 6; i++) {
    while(*p == ' ') p++;
    if(!ftp_is_digit(*p)) return -1;
    long n = 0;
    int digits = 0;
    while(ftp_is_digit(*p)) {
      if(++digits > 3) return -1;
      n = (n * 10) + (*p - '0');
      p++;
    }
    if(n > 255) return -1;
    v[i] = n;
    while(*p == ' ') p++;
    if(i < 5) {
      if(*p != ',') return -1;
      p++;
    }
  }
  if(open && *p != ')') return -1;
  long port = (v[4] * 256) + v[5];
  if(port < 1 || port > 65535) return -1;
  return (int)port;
}

int gxsFTPClient::RecvResponse(gxsSocket_t s, char *buf, int bytes, 
			       const char *response)
// Blocking receive function used to read a reply from an FTP server
// following a command. If the specified response is not received within
// the timeout period this function will return false to indicate an error.
// Returns true if successful.
// 10/31/2026: Replies are parsed by ParseReply(). The reply code (3 
// digits) is compared with the first 3 characters of response; it was
// searched for anywhere in the reply. A 226 reply left over from the 
// last transfer (FTPGet() and FTPList() do not read it) and other 
// preliminary 1xx replies are skipped. At most bytes-1 bytes are kept
// (the NUL was written one byte past a full buffer); when a long reply
// fills the buffer, its earlier lines are dropped.
{
  int byte_count = 0;       // Byte counter
  int num_read = 0;         // Actual number of bytes read
  int dropped = 0;          // Bytes dropped from a long reply
  if(!buf || bytes < 2 || !response) {
    socket_error = gxSOCKET_BUFOVER_ERROR;
    if(buf && bytes > 0) buf[0] = 0;
    return 0;
  }
  int limit = bytes - 1; // Room for the NUL
  buf[0] = 0;
  char expect[4];
  memset(expect, 0, sizeof(expect));
  strncpy(expect, response, 3);

  for(;;) {
    // Parse the replies received so far
    int code = 0, consumed = 0;
    int rv = ParseReply(buf, byte_count, &code, &consumed);
    if(rv < 0) {
      socket_error = gxSOCKET_FTP_ERROR;
      return 0;
    }
    if(rv > 0) {
      char code_str[4];
      code_str[0] = (char)('0' + (code / 100));
      code_str[1] = (char)('0' + ((code / 10) % 10));
      code_str[2] = (char)('0' + (code % 10));
      code_str[3] = 0;
      int match = (expect[0] != 0) && 
	(strncmp(code_str, expect, strlen(expect)) == 0);
      if(!match && (code == 226 || (code >= 100 && code < 200 &&
				    code != 110 && code != 120 && 
				    code != 125))) {
	// Skip a 226 left over from the last transfer or a preliminary
	// reply, and parse what follows it
	memmove(buf, buf+consumed, byte_count-consumed);
	byte_count -= consumed;
	buf[byte_count] = 0;
	dropped += consumed;
	if(dropped > 1024*1024) {
	  socket_error = gxSOCKET_BUFOVER_ERROR;
	  return 0;
	}
	continue;
      }
      // Check for FTP error codes 400 and higher
      if(code >= 400) {
	socket_error = gxSOCKET_FTP_ERROR;
	return 0;
      }
      if(match) return 1; // Found the expected reply
      if((code == 125) || (code == 225)) {
	return 1; // Data connection already open
      }
      socket_error = gxSOCKET_FTP_ERROR; // 110, 120, 202, 332 or another
      return 0;
    }

    if(byte_count >= limit) {
      // Keep the incomplete last line and drop the complete lines
      // of this reply before it, except the first, which holds the 
      // code. ParseReply() has checked the first line.
      int first = 0;
      while(first < byte_count && buf[first] != '\n') first++;
      first++;
      int last = byte_count;
      while(last > first && buf[last-1] != '\n') last--;
      if(last <= first || dropped > 1024*1024) {
	// The receieve buffer is full - buffer overflow
	socket_error = gxSOCKET_BUFOVER_ERROR;
	return 0;
      }
      memmove(buf+first, buf+last, byte_count-last);
      dropped += last - first;
      byte_count -= last - first;
      buf[byte_count] = 0;
    }

    if(!WaitForReply(s)) { 
      socket_error = gxSOCKET_REQUEST_TIMEOUT;      
      return 0;
    }
    num_read = recv(s, buf+byte_count, limit-byte_count, 0);
    if(num_read == 0) {
      // 10/31/2026: buf[bytes_read] used the socket's byte counter
      socket_error = gxSOCKET_DISCONNECTED_ERROR;
      return 0; // An error occurred during the read
    }
    if(num_read < 0) {
      socket_error = gxSOCKET_RECEIVE_ERROR;
      return 0; // An error occurred during the read
    }
    byte_count += num_read; // Increment the byte counter
    buf[byte_count] = 0;    // Null terminate the reply buffer
  }
}

#ifdef __BCC32__
#pragma warn .8080
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
