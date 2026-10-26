// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxstream.h
// C++ Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
// Date Last Modified: 10/26/2026
// Copyright (c) 2001-2026 DataReel Software Development
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

Changes:
==============================================================
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
#ifndef __GX_STREAM_HPP__
#define __GX_STREAM_HPP__

#include "gxdlcode.h"

#include <string>
#include <vector>

#include "gxheader.h"
#include "gxstls.h"

const int gxSTREAM_DEF_TIMEOUT = 7200;
const gxUINT32 gxSTREAM_DEF_MAX_BLOCK = 64*1024*1024;

// Database block client/server protocol over TLS. Each block is a
// gxBlockHeader followed by block_length bytes.
class GXDLCODE_API gxStream
{
public:
  gxStream() { conn.SetTimeout(gxSTREAM_DEF_TIMEOUT); }
  ~gxStream() { }

public: // Connection
  // Client: connect to a gxStream server over TLS. Returns 0 if
  // successful.
  int Connect(const std::string &host, int port = gxsDATA_PORT);
  void Close() { conn.Close(); }
  gxsTLSConnection &Connection() { return conn; }
  void SetTimeOut(int seconds = gxSTREAM_DEF_TIMEOUT) { 
    conn.SetTimeout(seconds); 
  }
  void SetMaxBlockSize(gxUINT32 bytes) { max_block = bytes; }

public: // Block functions. Each returns 0 if successful.
  void SetBlockStatus(gxBlockHeader &gx, __SBYTE__ net_status,
		      __SBYTE__ file_status = gxRemoteDeviceBlock);
  static __SBYTE__ BlockStatus(const gxBlockHeader &gx) {
    return (__SBYTE__)((((__ULWORD__)gx.block_status) & 0xFF00) >> 8);
  }
  int WriteHeader(gxBlockHeader &gx);
  int WriteBlock(const void *buf, int bytes); // gxSendBlock
  int WriteBlock(const void *buf, gxBlockHeader &gx);
  int ReadHeader(gxBlockHeader &gx);
  int ReadBlock(std::vector<unsigned char> &buf, const gxBlockHeader &gx);

  // Send a request block and read the block sent back
  int RequestBlock(const void *request, gxBlockHeader &request_header,
		   gxBlockHeader &reply_header, 
		   std::vector<unsigned char> &reply);
  int AddBlock(const void *block, gxBlockHeader &block_header);
  int ChangeBlock(const void *request, const void *block, 
		  gxBlockHeader &request_header, gxBlockHeader &block_header);
  int DeleteBlock(const void *request, gxBlockHeader &request_header);
  int WriteAckBlock();
  int ReadAckBlock();
  int WriteResultBlock(int result_code);
  int ReadResultBlock(int &result_code);
  int TerminateConnection(); // Ask the server to shut down
  int RestartConnection();   // Ask the server to restart
  int CloseConnection();     // Tell the server this client is done

public:
  gxSocketError GetSocketError() const { return stream_error; }
  const char *SocketExceptionMessage() const {
    return gxSocket::ExceptionMessage(stream_error);
  }
  const std::string &Error() const { return conn.Error(); }

private:
  int set_error(gxSocketError err) { return stream_error = err; }

private:
  gxsTLSConnection conn;
  gxUINT32 max_block = gxSTREAM_DEF_MAX_BLOCK;
  gxSocketError stream_error = gxSOCKET_NO_ERROR;
};

#endif // __GX_STREAM_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
