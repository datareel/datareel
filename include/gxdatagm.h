// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxdatagm.h
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

The gxDatagram class is derived from the gxSocket class and is 
used to send and receive database blocks over a UDP connection.
*/
// ----------------------------------------------------------- //   
#ifndef __GX_DATABASE_DATAGRAM_HPP__
#define __GX_DATABASE_DATAGRAM_HPP__

#include "gxdlcode.h"

#include "gxsocket.h"
#include "gxheader.h"

class GXDLCODE_API gxDatagram : public gxSocket
{
public:
  gxDatagram() { }
  ~gxDatagram() { }

public: // Client/Server configuration
  int DatagramServer(int port);
  int DatagramClient(int port, char *host);
  int PortNumber(int &port);
  int RemotePortNumber(int &port);
  int HostName(char *hs);
  int RemoteHostName(char *hs);
  
public: // High-level database block functions
  int WriteBlock(const void *buf, int bytes);
  int TerminateConnection();
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
  
public: // Low-level database block functions
  void SetBlockStatus(gxBlockHeader &gx, __SBYTE__ net_status,
		      __SBYTE__ file_status = gxRemoteDeviceBlock);
  int WriteBlock(gxsSocket_t s, gxsSocketAddress *sa,
		 const void *buf, int bytes);
  int WriteBlock(gxsSocket_t s, gxsSocketAddress *sa,
		 const void *buf, gxBlockHeader &gx);
  int WriteHeader(gxsSocket_t s, gxsSocketAddress *sa, gxBlockHeader &gx);
  int ReadBlock(gxsSocket_t s, gxsSocketAddress *sa,
		void *buf, gxBlockHeader &gx);
  int ReadHeader(gxsSocket_t s, gxsSocketAddress *sa, gxBlockHeader &gx);
  void *RequestBlock(gxsSocket_t s, gxsSocketAddress *sa, const void *request,
		     gxBlockHeader &request_header, 
		     gxBlockHeader &requested_block_header);
  int AddBlock(gxsSocket_t s, gxsSocketAddress *sa, const void *block,
	       gxBlockHeader &block_header);
  int ChangeBlock(gxsSocket_t s, gxsSocketAddress *sa, 
		  const void *request, const void *block, 
		  gxBlockHeader &request_header,  
		  gxBlockHeader &block_header);
  int DeleteBlock(gxsSocket_t s, gxsSocketAddress *sa, const void *request,
		  gxBlockHeader &request_header);
  int WriteAckBlock(gxsSocket_t s, gxsSocketAddress *sa);
  int ReadAckBlock(gxsSocket_t s, gxsSocketAddress *sa);
  int TerminateConnection(gxsSocket_t s, gxsSocketAddress *sa);
  int CloseConnection(gxsSocket_t s, gxsSocketAddress *sa);
};

#endif // __GX_DATABASE_DATAGRAM_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
