// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxstream.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
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

The GXStream class is used to stream data from a client to a 
server over a TCP/IP connection.
*/
// ----------------------------------------------------------- //   
#ifndef __GX_STREAM_HPP__
#define __GX_STREAM_HPP__

#include "gxdlcode.h"

#include "gxsocket.h"
#include "gxheader.h"

const int gxSTREAM_DEF_TIMEOUT = 7200;

class GXDLCODE_API gxStream : public gxSocket
{
public:
  gxStream() { 
    time_out_sec = gxSTREAM_DEF_TIMEOUT; // Blocking timeout value   
    time_out_usec = 0;
  }
  ~gxStream() { }

public: // Client/Server configuration
  int StreamServer(int port, int max_connections = SOMAXCONN);
  int StreamClient(int port, char *host);
  int PortNumber(int &port);
  int RemotePortNumber(int &port);
  int HostName(char *hs);
  int RemoteHostName(char *hs);
  
public: // High-level database block functions
  int WriteBlock(const void *buf, int bytes);
  int TerminateConnection();
  int RestartConnection();
  int CloseConnection();
  int ReadClientHeader(gxBlockHeader &gxs);
  int ReadRemoteBlock(void *buf, gxBlockHeader &gx);
  int WriteRemoteBlock(void *buf, int bytes);
  void *RequestBlock(const void *request, gxBlockHeader &request_header, 
		     gxBlockHeader &requested_block_header);
  int AddBlock(const void *block, gxBlockHeader &block_header);
  int ChangeBlock(const void *request, const void *block, 
		  gxBlockHeader &request_header, gxBlockHeader &block_header);
  int DeleteBlock(const void *request, gxBlockHeader &request_header);
  int WriteAckBlock();
  int WriteRemoteAckBlock();
  int ReadAckBlock();
  int ReadRemoteAckBlock();

  // Optional status information exchange between TCP client and server.
  int WriteResultBlock(int result_code);
  int WriteRemoteResultBlock(int result_code);
  int ReadResultBlock(int &result_code);
  int ReadRemoteResultBlock(int &result_code);
  
public: // Low-level database block functions
  void SetBlockStatus(gxBlockHeader &gx, __SBYTE__ net_status,
		      __SBYTE__ file_status = gxRemoteDeviceBlock);
  int WriteBlock(gxsSocket_t s, const void *buf, int bytes);
  int WriteBlock(gxsSocket_t s, const void *buf, gxBlockHeader &gx);
  int WriteHeader(gxsSocket_t s, gxBlockHeader &gx);
  int ReadBlock(gxsSocket_t s, void *buf, gxBlockHeader &gx,
		int seconds = gxSTREAM_DEF_TIMEOUT, int useconds = 0);
  int ReadHeader(gxsSocket_t s, gxBlockHeader &gx, 
		 int seconds = gxSTREAM_DEF_TIMEOUT, int useconds = 0);
  void *RequestBlock(gxsSocket_t s, const void *request,
		     gxBlockHeader &request_header, 
		     gxBlockHeader &requested_block_header,
		     int seconds = gxSTREAM_DEF_TIMEOUT, int useconds = 0);
  int AddBlock(gxsSocket_t s, const void *block, gxBlockHeader &block_header);
  int DeleteBlock(gxsSocket_t s, const void *request,
		  gxBlockHeader &request_header);
  int ChangeBlock(gxsSocket_t s, const void *request, 
		  const void *block, gxBlockHeader &request_header,
		  gxBlockHeader &block_header);
  int WriteAckBlock(gxsSocket_t s);
  int ReadAckBlock(gxsSocket_t s, 
		   int seconds = gxSTREAM_DEF_TIMEOUT, int useconds = 0);

  // Optional status information exchange between TCP client and server.
  int WriteResultBlock(gxsSocket_t s, int result_code);
  int ReadResultBlock(gxsSocket_t s, int &result_code,
		      int seconds = gxSTREAM_DEF_TIMEOUT, int useconds = 0);
  
  int TerminateConnection(gxsSocket_t s);
  int RestartConnection(gxsSocket_t s);
  int CloseConnection(gxsSocket_t s);

public: // Timer functions
  void SetTimeOut(int seconds = gxSTREAM_DEF_TIMEOUT, int useconds = 0) {
    time_out_sec = seconds;
    time_out_usec = useconds;
  }

public:
  int time_out_sec;  // Number of seconds before a blocking timeout
  int time_out_usec; // Number of microseconds before a blocking timeout
};

#endif // __GX_STREAM_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
