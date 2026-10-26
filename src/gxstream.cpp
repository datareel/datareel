// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxstream.cpp
// C++ Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
// Date Last Modified: 10/26/2026
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

The gxStream class is used to stream database blocks from a 
client to a server over a TCP/IP connection.

Changes:
==============================================================
08/02/2003: Modified the gxStream::RequestBlock() not to accept
null block values.

08/03/2003: Fixed potential memory leak in the gxStream::RequestBlock() 
function by deleting allocated buffer following a receive error.

10/26/2026: DataReel 7 rewrites gxStream on gxsTLSConnection, so 
database blocks always travel over TLS. The block header format 
is unchanged. A client calls Connect(); a server accepts a client 
with gxsTLSServer::Accept(stream.Connection()). Block lengths read 
from the network are checked against SetMaxBlockSize() before any 
memory is allocated, and requested blocks are returned in a 
std::vector instead of a new[] buffer.
==============================================================
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <cstring>
#include "gxstream.h"

namespace datareel {


int gxStream::Connect(const std::string &host, int port)
{
  if(conn.Connect(host, port, true) != 0) {
    return set_error(gxSOCKET_CONNECT_ERROR);
  }
  return set_error(gxSOCKET_NO_ERROR);
}

void gxStream::SetBlockStatus(gxBlockHeader &gx, __SBYTE__ net_status,
			      __SBYTE__ file_status)
{
  __ULWORD__ block_status = (__UBYTE__)net_status;
  block_status = (block_status<<8) & 0xff00;
  block_status += (__UBYTE__)file_status;
  gx.block_status = block_status;
  gx.block_check_word = gxCheckWord;
  gx.block_nd_fptr = (FAU)0;
}

int gxStream::WriteHeader(gxBlockHeader &gx)
{
  if(conn.Send(&gx, (int)sizeof(gxBlockHeader)) != (int)sizeof(gxBlockHeader)) {
    return set_error(gxSOCKET_BLOCKHEADER_ERROR);
  }
  return set_error(gxSOCKET_NO_ERROR);
}

int gxStream::WriteBlock(const void *buf, gxBlockHeader &gx)
{
  if(WriteHeader(gx) != gxSOCKET_NO_ERROR) return stream_error;
  int len = (int)(__ULWORD__)gx.block_length;
  if(len > 0 && conn.Send(buf, len) != len) {
    return set_error(gxSOCKET_BLOCKSIZE_ERROR);
  }
  return set_error(gxSOCKET_NO_ERROR);
}

int gxStream::WriteBlock(const void *buf, int bytes)
{
  gxBlockHeader gx;
  SetBlockStatus(gx, gxSendBlock);
  gx.block_length = (__ULWORD__)bytes;
  return WriteBlock(buf, gx);
}

int gxStream::ReadHeader(gxBlockHeader &gx)
{
  std::string s;
  if(conn.Read(s, sizeof(gxBlockHeader)) != (int)sizeof(gxBlockHeader)) {
    return set_error(conn.IsOpen() ? gxSOCKET_BLOCKHEADER_ERROR : 
		     gxSOCKET_DISCONNECTED_ERROR);
  }
  memcpy((void *)&gx, s.data(), sizeof(gxBlockHeader));
  if(gx.block_check_word != gxCheckWord) {
    return set_error(gxSOCKET_BLOCKSYNC_ERROR);
  }
  // 10/26/2026: The length comes from the peer, so check it before use
  if(gx.block_length > max_block) {
    return set_error(gxSOCKET_BLOCKSIZE_ERROR);
  }
  return set_error(gxSOCKET_NO_ERROR);
}

int gxStream::ReadBlock(std::vector<unsigned char> &buf, 
			const gxBlockHeader &gx)
{
  buf.clear();
  __ULWORD__ len = gx.block_length;
  if(len > max_block) return set_error(gxSOCKET_BLOCKSIZE_ERROR);
  if(len == 0) return set_error(gxSOCKET_NO_ERROR);
  std::string s;
  if(conn.Read(s, len) != (int)len) return set_error(gxSOCKET_BLOCKSIZE_ERROR);
  buf.assign(s.begin(), s.end());
  return set_error(gxSOCKET_NO_ERROR);
}

int gxStream::RequestBlock(const void *request, gxBlockHeader &request_header,
			   gxBlockHeader &reply_header, 
			   std::vector<unsigned char> &reply)
{
  reply.clear();
  SetBlockStatus(request_header, gxRequestBlock);
  if(WriteBlock(request, request_header) != gxSOCKET_NO_ERROR) {
    return stream_error;
  }
  if(ReadHeader(reply_header) != gxSOCKET_NO_ERROR) return stream_error;
  return ReadBlock(reply, reply_header);
}

int gxStream::AddBlock(const void *block, gxBlockHeader &block_header)
{
  SetBlockStatus(block_header, gxAddRemoteBlock);
  return WriteBlock(block, block_header);
}

int gxStream::ChangeBlock(const void *request, const void *block, 
			  gxBlockHeader &request_header,
			  gxBlockHeader &block_header)
{
  SetBlockStatus(request_header, gxChangeRemoteBlock);
  if(WriteBlock(request, request_header) != gxSOCKET_NO_ERROR) {
    return stream_error;
  }
  SetBlockStatus(block_header, gxSendBlock);
  return WriteBlock(block, block_header);
}

int gxStream::DeleteBlock(const void *request, gxBlockHeader &request_header)
{
  SetBlockStatus(request_header, gxDeleteRemoteBlock);
  return WriteBlock(request, request_header);
}

int gxStream::WriteAckBlock()
{
  gxBlockHeader ack;
  SetBlockStatus(ack, gxAcknowledgeBlock);
  if(WriteHeader(ack) != gxSOCKET_NO_ERROR) {
    return set_error(gxSOCKET_BLOCKACK_ERROR);
  }
  return set_error(gxSOCKET_NO_ERROR);
}

int gxStream::ReadAckBlock()
{
  gxBlockHeader ack;
  if(ReadHeader(ack) != gxSOCKET_NO_ERROR || 
     BlockStatus(ack) != gxAcknowledgeBlock) {
    return set_error(gxSOCKET_BLOCKACK_ERROR);
  }
  return set_error(gxSOCKET_NO_ERROR);
}

int gxStream::WriteResultBlock(int result_code)
{
  gxBlockHeader ack;
  SetBlockStatus(ack, gxAcknowledgeBlock);
  ack.block_nd_fptr = (FAU)result_code; // The result travels in the header
  if(WriteHeader(ack) != gxSOCKET_NO_ERROR) {
    return set_error(gxSOCKET_BLOCKACK_ERROR);
  }
  return set_error(gxSOCKET_NO_ERROR);
}

int gxStream::ReadResultBlock(int &result_code)
{
  gxBlockHeader ack;
  if(ReadHeader(ack) != gxSOCKET_NO_ERROR || 
     BlockStatus(ack) != gxAcknowledgeBlock) {
    return set_error(gxSOCKET_BLOCKACK_ERROR);
  }
  result_code = (int)ack.block_nd_fptr;
  return set_error(gxSOCKET_NO_ERROR);
}

int gxStream::TerminateConnection()
{
  gxBlockHeader gx;
  SetBlockStatus(gx, gxKillServer);
  return WriteHeader(gx);
}

int gxStream::RestartConnection()
{
  gxBlockHeader gx;
  SetBlockStatus(gx, gxRestartServer);
  return WriteHeader(gx);
}

int gxStream::CloseConnection()
{
  gxBlockHeader gx;
  SetBlockStatus(gx, gxCloseConnection);
  return WriteHeader(gx);
}
} // namespace datareel

// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
