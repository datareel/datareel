// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxstream.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
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

The gxStream class is used to stream database blocks from a 
client to a server over a TCP/IP connection.

Changes:
==============================================================
08/02/2003: Modified the gxStream::RequestBlock() not to accept
null block values.

08/03/2003: Fixed potential memory leak in the gxStream::RequestBlock() 
function by deleting allocated buffer following a receive error.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <string.h>
#include "gxstream.h"

#ifdef __BCC32__
#pragma warn -8080
#endif

// TCP/IP Stream functions
// --------------------------------------------------------------
int gxStream::StreamServer(int port, int max_connections)
// Initialize a stream server. Please note all ports below 1024 are
// reserved. Returns a non-zero value if any errors occur during
// initialization. The "max_connections" variable determines how many
// pending connections the queue will hold.
{
  if(InitSocketLibrary() == 0) {
    if(InitSocket(SOCK_STREAM, port) < 0) return socket_error;
  }
  else {
    return socket_error;
  }


  // Bind the name to the socket
  if(Bind() < 0) {
    Close();
    return socket_error;
  }

  // Listen for connections with a specified backlog
  if(Listen(max_connections) < 0) {
    Close();
    return socket_error;
  }

  return socket_error = gxSOCKET_NO_ERROR;
}

int gxStream::StreamClient(int port, char *host)
// Connect a stream client. Please note all ports below 1024 are
// reserved. Returns zero if no errors occur during initialization.
{
  if(InitSocketLibrary() == 0) {
    if(InitSocket(SOCK_STREAM, port, host) < 0) return socket_error;
  }
  else {
    return socket_error;
  }

  // Connect to the server
  if(Connect() < 0) {
    return socket_error;
  }

  return socket_error = gxSOCKET_NO_ERROR;  
}

void gxStream::SetBlockStatus(gxBlockHeader &gx, __SBYTE__ net_status,
			      __SBYTE__ file_status)
// Set the database block header status member control byte.
// NOTE: The block's next deleted variable must be set, if
// needed, after a call to this function. The block length
// will not be set here.
{
  __ULWORD__ block_status = net_status;
  block_status = (block_status<<8) & 0xff00;
  block_status += file_status;
  gx.block_status = block_status;
  gx.block_check_word = gxCheckWord;
  gx.block_nd_fptr = (FAU)0;
}

int gxStream::WriteBlock(gxsSocket_t s, const void *buf, int bytes)
// Write a block of raw data to the socket. NOTE: All block read and write
// operations must operate in blocking mode to maintain synchronization
// between the transmitter and receiver. Returns zero if no errors
// occur.
{
  gxBlockHeader gx;
  SetBlockStatus(gx, gxSendBlock);
  gx.block_length = bytes;

  // Write the block header
  if(WriteBlock(s, buf, gx) != gxSOCKET_NO_ERROR) return socket_error;
  
  return socket_error = gxSOCKET_NO_ERROR;  
}

int gxStream::WriteBlock(gxsSocket_t s, const void *buf, gxBlockHeader &gx)
// Write a database block to the specified socket. Returns zero if no
// errors occur. NOTE: This function assumes that the block header has
// already been initialized.
{
  // Write the block header
  if(WriteHeader(s, gx) != gxSOCKET_NO_ERROR) return socket_error;

  // Write the block
  if(Send(s, (char *)buf, gx.block_length) < 0)
    return socket_error;
  
  if(bytes_moved != gx.block_length) 
      return socket_error = gxSOCKET_BLOCKSIZE_ERROR;

  return socket_error = gxSOCKET_NO_ERROR;  
}

int gxStream::WriteHeader(gxsSocket_t s, gxBlockHeader &gx)
// Write a database block header to the specified socket.
// returns zero if no errors occur.
{
  // Write the block header
  if(Send(s, (char *)&gx, sizeof(gxBlockHeader)) < 0)
    return socket_error;

  // If the bytes sent do not equal the bytes moved then
  // the transmitter and receiver are no longer synchronized.
  if(bytes_moved != sizeof(gxBlockHeader)) 
    return socket_error = gxSOCKET_BLOCKHEADER_ERROR;

  return socket_error = gxSOCKET_NO_ERROR;
}

int gxStream::ReadBlock(gxsSocket_t s, void *buf, gxBlockHeader &gx,
			int seconds, int useconds)
// Read database block from the specified socket. Returns zero
// if no errors occur.
{
  if(Recv(s, buf, gx.block_length, seconds, useconds) < 0)
    return socket_error;

  // If the bytes read do not equal the bytes received then
  // the transmitter and receiver are no longer synchronized.
  if(bytes_read != gx.block_length) 
    return socket_error = gxSOCKET_BLOCKSIZE_ERROR;

  return socket_error = gxSOCKET_NO_ERROR;
}

int gxStream::ReadHeader(gxsSocket_t s, gxBlockHeader &gx,
			 int seconds, int useconds)
// Read a database block header from the specified socket.
// Returns zero if no errors occur or an error number
// corresponding to a value defined in the gxSocketError
// enumeration.
{
  if(Recv(s, (char *)&gx, sizeof(gxBlockHeader), seconds, useconds) < 0)
    return socket_error;

  // If the bytes read do not equal the bytes received then
  // the transmitter and receiver are no longer synchronized.
  if(bytes_read != sizeof(gxBlockHeader)) 
    return socket_error = gxSOCKET_BLOCKHEADER_ERROR;
      
  if(gx.block_check_word != gxCheckWord) 
    return socket_error = gxSOCKET_BLOCKSYNC_ERROR;
  	
  return socket_error = gxSOCKET_NO_ERROR;    
}

void *gxStream::RequestBlock(gxsSocket_t s, const void *request,
			     gxBlockHeader &request_header, 
			     gxBlockHeader &requested_block_header,
			     int seconds, int useconds)
// Request a block from the specified socket. Returns the requested block
// or a null value if an error occurs.
{
  SetBlockStatus(request_header, gxRequestBlock);

  // Write the request header and the request itself
  if(WriteBlock(s, request, request_header) != gxSOCKET_NO_ERROR)
    return 0;
  
  // Read the returned request header 
  if(ReadHeader(s, requested_block_header, seconds, useconds) != 
     gxSOCKET_NO_ERROR) {
    return 0;
  }

  // Return null for all zero length blocks
  if(requested_block_header.block_length == (__ULWORD__)0) {
    return 0;
  }

  // Read the returned requested block
  char *buf = new char[(__ULWORD__)requested_block_header.block_length];
  if(!buf) return 0;

  if(Recv(s, buf, requested_block_header.block_length, seconds, 
	  useconds) < 0) {
    delete[] buf;
    return 0;
  }

  // Check the byte count to ensure that all blocks were received.
  if(bytes_read != requested_block_header.block_length) {
    delete[] buf;
    return 0; 
  }

  return (void *)(buf); // Return the requested block
}

int gxStream::DeleteBlock(gxsSocket_t s, const void *request,
			  gxBlockHeader &request_header)
// Request the a block be deleted on the remote end. Returns
// zero if no error occur.
{
  SetBlockStatus(request_header, gxDeleteRemoteBlock);

  // Write the delete request and the request itself
  if(WriteBlock(s, request, request_header) != gxSOCKET_NO_ERROR)
    return socket_error;

  return socket_error = gxSOCKET_NO_ERROR;    
}

int gxStream::AddBlock(gxsSocket_t s, const void *block,
		       gxBlockHeader &block_header)
// Request that this block be added. Returns zero if no error 
// occur.
{
  SetBlockStatus(block_header, gxAddRemoteBlock);

  // Write the add request and then the block to add
  if(WriteBlock(s, block, block_header) != gxSOCKET_NO_ERROR)
    return socket_error;

  return socket_error = gxSOCKET_NO_ERROR;    
}

int gxStream::ChangeBlock(gxsSocket_t s, const void *request, 
			  const void *block, gxBlockHeader &request_header,
			  gxBlockHeader &block_header)
// Request that this block be changed. Returns zero if no error 
// occur.
{
  // Write the change request and the request itself
  SetBlockStatus(request_header, gxChangeRemoteBlock);
  if(WriteBlock(s, request, request_header) != gxSOCKET_NO_ERROR)
    return socket_error;

  // Write the changes
  SetBlockStatus(block_header, gxSendBlock);
  if(WriteBlock(s, block, block_header) != gxSOCKET_NO_ERROR)
    return socket_error;

  return socket_error = gxSOCKET_NO_ERROR;    
}

int gxStream::WriteAckBlock(gxsSocket_t s)
// Send an acknowledgment header to the specified socket.
// Returns zero if no errors occur.
{
  gxBlockHeader ack;
  SetBlockStatus(ack, gxAcknowledgeBlock);
  
  if(WriteHeader(s, ack) != gxSOCKET_NO_ERROR)
    return socket_error = gxSOCKET_BLOCKACK_ERROR;
  
  return socket_error = gxSOCKET_NO_ERROR;    
}

int gxStream::ReadAckBlock(gxsSocket_t s, int seconds, int useconds)
// Read an acknowledgment block from the specified socket.
// Returns zero if no errors occur.
{
  // Wait for an acknowledgment
  gxBlockHeader ack;
  if(ReadHeader(s, ack, seconds, useconds) != gxSOCKET_NO_ERROR)
    return socket_error = gxSOCKET_BLOCKACK_ERROR;

  // Read the status byte to determine what to do with this block
  __ULWORD__ block_status = ack.block_status;
  __SBYTE__ status = (__SBYTE__)((block_status & 0xFF00)>>8);
      
  if(status != gxAcknowledgeBlock)
    return socket_error = gxSOCKET_BLOCKACK_ERROR;

  return socket_error = gxSOCKET_NO_ERROR; 
}

int gxStream::TerminateConnection(gxsSocket_t s)
// Block command used to shutdown a server or client.
// Returns zero if no errors occur.
{
  gxBlockHeader gx;
  SetBlockStatus(gx, gxKillServer);

  if(WriteHeader(s, gx) != gxSOCKET_NO_ERROR) return socket_error;

  return socket_error = gxSOCKET_NO_ERROR;  
}

int gxStream::CloseConnection(gxsSocket_t s)
// Block command used to close a persistent client or
// server connection. Returns zero if no errors occur.
{
  gxBlockHeader gx;
  SetBlockStatus(gx, gxCloseConnection);

  if(WriteHeader(s, gx) != gxSOCKET_NO_ERROR) return socket_error;

  return socket_error = gxSOCKET_NO_ERROR;  
}

int gxStream::PortNumber(int &port)
// Pass back the port number actually set by the system
// in the "port" variable.
{
  if(GetSockName() < 0) {
    return socket_error;
  }
  port = GetPortNumber();
  return socket_error = gxSOCKET_NO_ERROR;
}

int gxStream::RemotePortNumber(int &port)
// Pass back the port number actually set by the system
// in the "port" variable.
{
  port = GetRemotePortNumber();
  return socket_error = gxSOCKET_NO_ERROR;
}

int gxStream::HostName(char *hs)
{
  if(GetHostName(hs) < 0) {
    return socket_error;
  }
  return socket_error = gxSOCKET_NO_ERROR;
}

int gxStream::RemoteHostName(char *hs)
{
  if(GetRemoteHostName(hs) < 0) {
    return socket_error;
  }
  return socket_error = gxSOCKET_NO_ERROR;
}

int gxStream::WriteBlock(const void *buf, int bytes)
{
  return WriteBlock(gxsocket, buf, bytes);
}

int gxStream::TerminateConnection()
{
  return TerminateConnection(gxsocket);
}

int gxStream::CloseConnection()
{
  return CloseConnection(gxsocket);
}

int gxStream::ReadRemoteBlock(void *buf, gxBlockHeader &gx)
{
  return ReadBlock(remote_socket, buf, gx, time_out_sec, time_out_usec);
}

int gxStream::ReadClientHeader(gxBlockHeader &gx)
{
  return ReadHeader(remote_socket, gx, time_out_sec, time_out_usec);
}

int gxStream::WriteRemoteBlock(void *buf, int bytes)
{
  return WriteBlock(remote_socket, buf, bytes);
}

void *gxStream::RequestBlock(const void *request,
			     gxBlockHeader &request_header, 
			     gxBlockHeader &requested_block_header)
{
  return RequestBlock(gxsocket, request, request_header,
		      requested_block_header, time_out_sec, time_out_usec);
}

int gxStream::DeleteBlock(const void *request, gxBlockHeader &request_header)
{
  return DeleteBlock(gxsocket, request, request_header);
}

int gxStream::WriteAckBlock()
{
  return WriteAckBlock(gxsocket);
}

int gxStream::WriteRemoteAckBlock()
{
  return WriteAckBlock(remote_socket);
}

int gxStream::ReadAckBlock()
{
  return ReadAckBlock(gxsocket, time_out_sec, time_out_usec);
}

int gxStream::ReadRemoteAckBlock()
{
  return ReadAckBlock(remote_socket, time_out_sec, time_out_usec);
}

int gxStream::AddBlock(const void *block, gxBlockHeader &block_header)
{
  return AddBlock(gxsocket, block, block_header);
}

int gxStream::ChangeBlock(const void *request, const void *block, 
			  gxBlockHeader &request_header, 
			  gxBlockHeader &block_header)
{
  return ChangeBlock(gxsocket, request, block, request_header, block_header);
}

int gxStream::RestartConnection()
{
  return RestartConnection(gxsocket);
}

int gxStream::RestartConnection(gxsSocket_t s)
// Block command used to restart a server or client.
// Returns zero if no errors occur.
{
  gxBlockHeader gx;
  SetBlockStatus(gx, gxRestartServer);

  if(WriteHeader(s, gx) != gxSOCKET_NO_ERROR) return socket_error;

  return socket_error = gxSOCKET_NO_ERROR;  
}

int gxStream::WriteResultBlock(gxsSocket_t s, int result_code)
{
  gxBlockHeader ack;
  SetBlockStatus(ack, gxAcknowledgeBlock);
  
  // Use the header's length field to store the result
  ack.block_nd_fptr = (FAU)result_code;

  if(WriteHeader(s, ack) != gxSOCKET_NO_ERROR)
    return socket_error = gxSOCKET_BLOCKACK_ERROR;
  
  return socket_error = gxSOCKET_NO_ERROR;    
}

int gxStream::ReadResultBlock(gxsSocket_t s, int &result_code,
			      int seconds, int useconds)
{
  // Wait for an acknowledgment
  gxBlockHeader ack;
  if(ReadHeader(s, ack, seconds, useconds) != gxSOCKET_NO_ERROR) {
    return socket_error = gxSOCKET_BLOCKACK_ERROR;
  }

  // Read the status byte to determine what to do with this block
  __ULWORD__ block_status = ack.block_status;
  __SBYTE__ status = (__SBYTE__)((block_status & 0xFF00)>>8);
      
  if(status != gxAcknowledgeBlock) {
    return socket_error = gxSOCKET_BLOCKACK_ERROR;
  }

  // Read the result code stored in the header;s length field
  result_code = (int)ack.block_nd_fptr;

  return socket_error = gxSOCKET_NO_ERROR; 
}

int gxStream::WriteResultBlock(int result_code)
{
  return WriteResultBlock(gxsocket, result_code);
}

int gxStream::WriteRemoteResultBlock(int result_code)
{
  return WriteResultBlock(remote_socket, result_code);
}

int gxStream::ReadResultBlock(int &result_code)
{
  return ReadResultBlock(gxsocket, result_code,
			 time_out_sec, time_out_usec);
}

int gxStream::ReadRemoteResultBlock(int &result_code)
{
  return ReadResultBlock(remote_socket, result_code,
			 time_out_sec, time_out_usec);
}

#ifdef __BCC32__
#pragma warn .8080
#endif
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
