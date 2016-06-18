// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxftp.cpp
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
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <string.h>
#include <stdio.h>
#include "gxsftp.h"

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

    // NOTE: The server may send back a 226 reply code so the 
    // client must check the reply code be parsing the 277 reply.
    int reply_code;
    sscanf(reply_buf, "%d ", &reply_code); 
    if(reply_code == 226) { // Remove the first line of text.
      int len = strlen(reply_buf);
      int offset = 0;
      for(int i = 0; i < len; i++) {
	offset++;
	// The server's reply should always be terminated with a
	// \r\n at the end of each reply.
	if(reply_buf[i] == '\n') break;
      }
      // Remove the 226 reply line
      memmove(reply_buf, (reply_buf+offset), (len-offset));

      // Null terminate the string
      reply_buf[(len-offset)] = 0; 
    }

    // Validate the server's reply account for the \r\n terminator
    if(reply_buf[strlen(reply_buf)-3] != ')') {
      if(reply_buf[strlen(reply_buf)-3] != '.') {
	return socket_error = gxSOCKET_FTP_ERROR;
      }
    }
    
    char s1[25]; char s2[25]; char s3[25]; char s4[25];
    char ip_port[255];
    // Parse the server's reply and check for errors
    sscanf(reply_buf, "%d %s %s %s %s%[^\r\n]", 
    	   &reply_code, s1, s2, s3, ip_port, s4); 

    if(reply_code != 227) {
      return socket_error = gxSOCKET_FTP_ERROR;
    }
    
    // Parse the IP and port information from the ip_port string.
    // The server will issue a PORT command formatted as a series of six 
    // numbers separated by commas. The first four octets is the IP 
    // address of the server and last two octets comprise the port that 
    // will be used for the data connection. To find the port multiply 
    // the fifth octet by 256 and then add the sixth octet to the total.
    int ip1, ip2, ip3, ip4, p1, p2;
    sscanf(ip_port, "(%d,%d,%d,%d,%d,%d)", &ip1, &ip2, &ip3, &ip4, &p1, &p2);

    // Construct the server's host IP and port number
    sprintf(ip_port, "%d.%d.%d.%d", ip1, ip2, ip3, ip4); 
    int port = (p1*256) + p2;
    
    if(ftp_data.InitSocket(SOCK_STREAM, port, ip_port) < 0) {
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

int gxsFTPClient::RecvResponse(gxsSocket_t s, char *buf, int bytes, 
			       const char *response)
// Blocking receive function used to read a reply from an FTP server
// following a command. If the specified response is not received within
// the timeout period this function will return false to indicate an error.
// Returns true if successful.
{
  int byte_count = 0;       // Byte counter
  int num_read = 0;         // Actual number of bytes read
  int num_req = (int)bytes; // Number of bytes requested 
  char *p = buf;            // Pointer to the buffer
  int reply_code = 0;       // Reply code 

  while(byte_count < bytes) { // Loop until the buffer is full
    if(!WaitForReply(s)) { 
      socket_error = gxSOCKET_REQUEST_TIMEOUT;      
      if(byte_count >= 0) buf[byte_count] = 0;
      return 0;
    }
    if((num_read = recv(s, p, num_req-byte_count, 0)) > 0) {
      byte_count += num_read; // Increment the byte counter
      p += num_read;          // Move the buffer pointer for the next read

      if(byte_count >= 4) {
	sscanf(buf, "%d", &reply_code);
	// Check for FTP error codes 400 and higher
	if(reply_code >= 400) {
	  buf[byte_count] = 0; // Null terminate the reply buffer
	  socket_error = gxSOCKET_FTP_ERROR;
	  return 0;
	}
	if((reply_code == 110) || (reply_code == 120) ||
	   (reply_code == 202) || (reply_code == 332)) {
	  buf[byte_count] = 0; // Null terminate the reply buffer
	  socket_error = gxSOCKET_FTP_ERROR;
	  return 0;
	}
	if((reply_code == 125) || (reply_code == 225)) {
	  buf[byte_count] = 0; // Null terminate the reply buffer
	  return 1; // Data connection already open
	}
      }

      // Search for a matching string
      char *pattern = (char *)response;
      char *next = buf;
      int i = 0;
      while(i < byte_count && *pattern) {
	if(*next == *pattern) {
	  pattern++;
	  if(*pattern == 0) {
	    if(byte_count >= 0) buf[byte_count] = 0;
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
      if(byte_count >= 0) buf[byte_count] = 0;
      socket_error = gxSOCKET_RECEIVE_ERROR;
      return 0; // An error occurred during the read
    }
  }

  // The receieve buffer is full - buffer overflow
  socket_error = gxSOCKET_BUFOVER_ERROR;
  if(byte_count >= 0) buf[byte_count] = 0;
  return 0;
}

#ifdef __BCC32__
#pragma warn .8080
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
