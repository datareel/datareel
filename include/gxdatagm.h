// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxdatagm.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
// Date Last Modified: 11/14/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- // 
// ---------- Include File Description and Details  ---------- // 
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

The gxDatagram class is derived from the gxSocket class and is 
used to send and receive database blocks over a UDP connection.

11/14/2026: Block lengths from the network are checked against
SetMaxBlockSize() (64 MB by default) before RequestBlock() allocates
memory or ReadBlock() reads. New ReadBlock()/ReadRemoteBlock() overloads
take the size of the caller's buffer. NOTE: UDP has no peer
authentication; any host that can reach the port can send blocks.
*/
// ----------------------------------------------------------- //   
#ifndef __GX_DATABASE_DATAGRAM_HPP__
#define __GX_DATABASE_DATAGRAM_HPP__

#include "gxdlcode.h"

#include "gxsocket.h"
#include "gxheader.h"

// 11/14/2026: Largest block length accepted from the network by
// default. See gxDatagram::SetMaxBlockSize().
const gxUINT32 gxDATAGRAM_DEF_MAX_BLOCK = 64*1024*1024;

class GXDLCODE_API gxDatagram : public gxSocket
{
public:
  gxDatagram() { max_block = gxDATAGRAM_DEF_MAX_BLOCK; }
  ~gxDatagram() { }

public: // 11/14/2026: Limits
  // Largest block length accepted from the peer, checked before any
  // memory is allocated (RequestBlock()) or data is read (ReadBlock()).
  // At most INT_MAX is used.
  void SetMaxBlockSize(gxUINT32 bytes) { max_block = bytes; }
  gxUINT32 MaxBlockSize() { return max_block; }

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
  // 11/14/2026: Reads the block only if it fits in buf_size bytes
  int ReadRemoteBlock(void *buf, int buf_size, gxBlockHeader &gx);
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
  // 11/14/2026: Nothing is read if the block does not fit in buf_size
  int ReadBlock(gxsSocket_t s, gxsSocketAddress *sa,
		void *buf, int buf_size, gxBlockHeader &gx);
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

protected:
  gxUINT32 max_block; // 11/14/2026: See SetMaxBlockSize()
  int check_block_length(const gxBlockHeader &gx, int buf_size = -1);
};

#endif // __GX_DATABASE_DATAGRAM_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
