// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Header File Name: gxsftp.h
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

File Transfer Protocol (FTP) classes used with applications
that require use of embedded FTP client/server functions.

Changes:
==============================================================
03/10/2002: The gxsFTPClient class no longer uses the C++ fstream 
class as the underlying file system. The gxDatabase file pointer 
routines are now to used define the underlying file system used by 
the gxsFTPClient class. This change was made to support large files 
and NTFS file system enhancements. To enable large file support 
users must define the __64_BIT_DATABASE_ENGINE__ preprocessor 
directive.

06/10/2002: Added gxsFTP_BUF_SIZE constant used to replace
gxsBUF_SIZE, which was previously used to set the fixed FTP 
replay buffer sizes. This change was made to accommodate larger 
fixed FTP reply buffers without changing the gxsFTP_BUF_SIZE 
constant.

06/10/2002: Added the gxsFTPClient::passive_mode variable 
used internally to toggle passive FTP mode on and off.

06/10/2002: Added the gxsFTPClient::FTPPassive() and 
gxsFTPClient::FTPActive() functions used to toggle passive FTP
mode on and off.
==============================================================
*/
// ----------------------------------------------------------- // 
#ifndef __GX_EMBEDDED_FTP_LIB_HPP__
#define __GX_EMBEDDED_FTP_LIB_HPP__

// Set the reply buffer size
const unsigned gxsFTP_BUF_SIZE = 8192;

#include "gxdlcode.h"

#include "gxsocket.h"
#include "dfileb.h"

// FTP client - See RFC 959 for specifications
class GXDLCODE_API gxsFTPClient : public gxSocket
{
public:
  gxsFTPClient() { 
    time_out_sec = 5; // Default blocking timeout value   
    time_out_usec = 0;
    reply_buf[0] = 0; 
    command_buf[0] = 0;
    ftp_data_port = 0;
    passive_mode = 0;
  }
  ~gxsFTPClient() { }

public:
  gxSocketError ConnectClient(const char *host, int port = gxSOCKET_FTP_PORT);
  gxSocketError SendCommand(const char *command, const char *response, 
			    const char *args = 0);
  gxSocketError FTPLogin(const char *username, const char *password);
  gxSocketError FTPLogout();
  gxSocketError FTPImageType(char type = 'I');
  gxSocketError FTPList(char *sbuf, unsigned bytes, int full = 1,
			const char *args = 0);
  gxSocketError FTPChDir(const char *dname);
  gxSocketError FTPMkDir(const char *dname);
  gxSocketError FTPRmDir(const char *dname);
  gxSocketError FTPPWD();
  gxSocketError FTPStat();
  gxSocketError FTPSize(const char *fname);
  gxSocketError FTPDelete(const char *fname);
  gxSocketError FTPMove(const char *from, const char *to);
  gxSocketError FTPGet(const char *fname, DiskFileB &stream, FAU_t &bytes);
  gxSocketError FTPPut(const char *fname, DiskFileB &stream, FAU_t &bytes);
  gxSocketError OpenDataPort();
  void FTPPassive() { passive_mode = 1; }
  void FTPActive() { passive_mode = 0; }
  int ReadDataPort(void *buf, unsigned bytes);
  int WriteDataPort(void *buf, unsigned bytes);
  void CloseDataPort();
  int RecvResponse(char *buf, int bytes, const char *response);
  int RecvResponse(gxsSocket_t s, char *buf, int bytes, const char *response);
  int WaitForReply(gxsSocket_t s);
  void SetTimeOut(int seconds, int useconds) {
    time_out_sec = seconds;
    time_out_usec = useconds;
  }

public:
  gxSocket ftp_data; // Socket used to transfer data to this client
  int ftp_data_port; // FTP data port client is listening on
  char reply_buf[gxsFTP_BUF_SIZE]; // Buffer used to hold the last reply
  char command_buf[gxsBUF_SIZE];   // Buffer used to hold the last command
  int time_out_sec;  // Number of seconds before a blocking timeout
  int time_out_usec; // Number of microseconds before a blocking timeout
  int passive_mode;  // True if this client is in passive FTP mode
};

#endif // __GX_EMBEDDED_FTP_LIB_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
