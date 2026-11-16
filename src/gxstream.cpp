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
version 3 of the License, or (at your option) any later version.
 
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.
 
You should have received a copy of the GNU Lesser General Public
License along with this library. If not, see
<https://www.gnu.org/licenses/>.

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

11/01/2026: The block functions refuse to run unless the connection
is secure (gxSOCKET_SSL_ERROR), so a failed or skipped TLS handshake
can never send database blocks in cleartext. WriteBlock() rejects a
block_length over INT_MAX instead of truncating it. Added
WriteResultBlock(FAU_t)/ReadResultBlock(FAU_t&) so a 64-bit file
address survives the round trip.
==============================================================
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <climits>
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
  // 11/01/2026: Never send database blocks in cleartext
  if(!conn.IsSecure()) return set_error(gxSOCKET_SSL_ERROR);
  unsigned char wb[gxWireBlockHeaderSize];      // 11/15/2026 (beta 5): the canonical wire header (gxheader.h)
  gxWirePackBlockHeader(gx, wb);
  if(conn.Send(wb, (int)gxWireBlockHeaderSize) != (int)gxWireBlockHeaderSize) {
    return set_error(gxSOCKET_BLOCKHEADER_ERROR);
  }
  return set_error(gxSOCKET_NO_ERROR);
}

int gxStream::WriteBlock(const void *buf, gxBlockHeader &gx)
{
  // 11/01/2026: Check before anything is sent. A length over INT_MAX
  // was truncated by the int cast below.
  if((__ULWORD__)gx.block_length > (__ULWORD__)INT_MAX) {
    return set_error(gxSOCKET_BLOCKSIZE_ERROR);
  }
  if(gx.block_length > 0 && !buf) return set_error(gxSOCKET_BLOCKSIZE_ERROR);
  if(!conn.IsSecure()) return set_error(gxSOCKET_SSL_ERROR);
  if(WriteHeader(gx) != gxSOCKET_NO_ERROR) return stream_error;
  int len = (int)(__ULWORD__)gx.block_length;
  if(len > 0 && conn.Send(buf, len) != len) {
    return set_error(gxSOCKET_BLOCKSIZE_ERROR);
  }
  return set_error(gxSOCKET_NO_ERROR);
}

int gxStream::WriteBlock(const void *buf, int bytes)
{
  // 11/01/2026: A negative count became a huge block_length
  if(bytes < 0) return set_error(gxSOCKET_BLOCKSIZE_ERROR);
  gxBlockHeader gx;
  SetBlockStatus(gx, gxSendBlock);
  gx.block_length = (__ULWORD__)bytes;
  return WriteBlock(buf, gx);
}

int gxStream::ReadHeader(gxBlockHeader &gx)
{
  // 11/01/2026: Never accept database blocks in cleartext
  if(!conn.IsSecure()) return set_error(gxSOCKET_SSL_ERROR);
  std::string s;
  if(conn.Read(s, gxWireBlockHeaderSize) != (int)gxWireBlockHeaderSize) {
    return set_error(conn.IsOpen() ? gxSOCKET_BLOCKHEADER_ERROR : 
		     gxSOCKET_DISCONNECTED_ERROR);
  }
  gxWireUnpackBlockHeader((const unsigned char *)s.data(), gx);   // 11/15/2026 (beta 5): canonical
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
  if(!conn.IsSecure()) return set_error(gxSOCKET_SSL_ERROR); // 11/01/2026
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
  return WriteResultBlock((FAU_t)result_code);
}

int gxStream::WriteResultBlock(FAU_t result_code)
// 11/01/2026: The header's block_nd_fptr field is as wide as FAU, so
// a 64-bit file address travels whole (int truncated it)
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
  FAU_t code = (FAU_t)0;
  if(ReadResultBlock(code) != gxSOCKET_NO_ERROR) return stream_error;
  result_code = (int)code;
  return set_error(gxSOCKET_NO_ERROR);
}

int gxStream::ReadResultBlock(FAU_t &result_code)
// 11/01/2026: Full width result, see WriteResultBlock(FAU_t)
{
  gxBlockHeader ack;
  if(ReadHeader(ack) != gxSOCKET_NO_ERROR || 
     BlockStatus(ack) != gxAcknowledgeBlock) {
    return set_error(gxSOCKET_BLOCKACK_ERROR);
  }
  result_code = (FAU_t)ack.block_nd_fptr;
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
