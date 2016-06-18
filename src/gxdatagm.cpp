// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxdatagm.cpp
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

The gxDatagram class is derived from the gxSocket class and is 
used to send and receive database blocks over a UDP connection.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <string.h>
#include "gxdatagm.h"

#ifdef __BCC32__
#pragma warn -8080
#endif

// Datagram functions
// --------------------------------------------------------------
int gxDatagram::DatagramServer(int port)
// Initialize a datagram server. Please note all ports below 1024 are
// reserved. Returns a non-zero value if any errors occur during
// initialization. 
{
  if(InitSocketLibrary() == 0) {
    if(InitSocket(SOCK_DGRAM, port) < 0) return socket_error;
  }
  else {
    return socket_error;
  }

  // Bind the name to the socket
  if(Bind() < 0) {
    Close();
    return socket_error;
  }

  return socket_error = gxSOCKET_NO_ERROR;
}

int gxDatagram::DatagramClient(int port, char *host)
// Connect a Datagram client. Please note all ports below 1024 are
// reserved. Returns zero if no errors occur during initialization.
{
  if(InitSocketLibrary() == 0) {
    if(InitSocket(SOCK_DGRAM, port, host) < 0) return socket_error;
  }
  else {
    return socket_error;
  }

  return socket_error = gxSOCKET_NO_ERROR;  
}

void gxDatagram::SetBlockStatus(gxBlockHeader &gx, __SBYTE__ net_status,
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

int gxDatagram::WriteBlock(gxsSocket_t s, gxsSocketAddress *sa, 
			   const void *buf, int bytes)
// Write a block of raw data to the socket. NOTE: All block read and write
// operations must operate in blocking mode to maintain synchronization
// between the transmitter and receiver. Returns zero if no errors
// occur.
{
  gxBlockHeader gx;
  SetBlockStatus(gx, gxSendBlock);
  gx.block_length = bytes;

  // Write the block header
  if(WriteBlock(s, sa, buf, gx) != gxSOCKET_NO_ERROR) 
    return socket_error;
  
  return socket_error = gxSOCKET_NO_ERROR;  
}

int gxDatagram::WriteBlock(gxsSocket_t s, gxsSocketAddress *sa, 
			   const void *buf, gxBlockHeader &gx)
// Write a database block to the specified socket. Returns zero if no
// errors occur. NOTE: This function assumes that the block header has
// already been initialized.
{
  // Write the block header
  if(WriteHeader(s, sa, gx) != gxSOCKET_NO_ERROR) return socket_error;

  // Write the block
  if(SendTo(s, sa, (char *)buf, gx.block_length) < 0)
    return socket_error;
  
  if(bytes_moved != gx.block_length) 
      return socket_error = gxSOCKET_BLOCKSIZE_ERROR;

  return socket_error = gxSOCKET_NO_ERROR;  
}

int gxDatagram::WriteHeader(gxsSocket_t s, gxsSocketAddress *sa, 
			    gxBlockHeader &gx)
// Write a database block header to the specified socket.
// returns zero if no errors occur.
{
  // Write the block header
  if(SendTo(s, sa, (char *)&gx, sizeof(gxBlockHeader)) < 0)
    return socket_error;

  // If the bytes sent do not equal the bytes moved then
  // the transmitter and receiver are no longer synchronized.
  if(bytes_moved != sizeof(gxBlockHeader)) 
    return socket_error = gxSOCKET_BLOCKHEADER_ERROR;

  return socket_error = gxSOCKET_NO_ERROR;
}

int gxDatagram::ReadBlock(gxsSocket_t s, gxsSocketAddress *sa, void *buf, 
			  gxBlockHeader &gx)
// Read database block from the specified socket. Returns zero
// if no errors occur.
{
  if(RecvFrom(s, sa, buf, gx.block_length) < 0)
    return socket_error;

  // If the bytes read do not equal the bytes received then
  // the transmitter and receiver are no longer synchronized.
  if(bytes_read != gx.block_length) 
    return socket_error = gxSOCKET_BLOCKSIZE_ERROR;

  return socket_error = gxSOCKET_NO_ERROR;
}

int gxDatagram::ReadHeader(gxsSocket_t s, gxsSocketAddress *sa, 
			   gxBlockHeader &gx)
// Read a database block header from the specified socket.
// Returns zero if no errors occur or an error number
// corresponding to a value defined in the gxSocketError
// enumeration.
{
  if(RecvFrom(s, sa, (char *)&gx, sizeof(gxBlockHeader)) < 0)
    return socket_error;

  // If the bytes read do not equal the bytes received then
  // the transmitter and receiver are no longer synchronized.
  if(bytes_read != sizeof(gxBlockHeader)) 
    return socket_error = gxSOCKET_BLOCKHEADER_ERROR;
      
  if(gx.block_check_word != gxCheckWord) 
    return socket_error = gxSOCKET_BLOCKSYNC_ERROR;
  	
  return socket_error = gxSOCKET_NO_ERROR;    
}

void *gxDatagram::RequestBlock(gxsSocket_t s, gxsSocketAddress *sa, 
			       const void *request,
			       gxBlockHeader &request_header, 
			       gxBlockHeader &requested_block_header)
// Request a block from the specified socket. Returns the requested block
// or a null value if an error occurs.
{
  SetBlockStatus(request_header, gxRequestBlock);

  // Write the request header and the request itself
  if(WriteBlock(s, sa, request, request_header) != gxSOCKET_NO_ERROR)
    return 0;
  
  // Read the returned request header 
  if(ReadHeader(s, sa, requested_block_header) != gxSOCKET_NO_ERROR) 
    return 0;

  // Read the returned requested block
  char *buf = new char[(__ULWORD__)requested_block_header.block_length];
  if(!buf) return 0;
  
  if(RecvFrom(s, sa, buf, requested_block_header.block_length) < 0) {
    // PC-lint 09/08/2005: Memory leak if buf is not freeed before returning
    // on error condition.
    delete[] buf;
    return 0;
  }

  // Check the byte count to ensure that all blocks were received.
  if(bytes_read != requested_block_header.block_length) {
    // PC-lint 09/08/2005: Memory leak if buf is not freeed before returning
    // on error condition.
    delete[] buf;
    return 0; 
  }

  return (void *)(buf); // Return the requested block
}

int gxDatagram::DeleteBlock(gxsSocket_t s, gxsSocketAddress *sa, 
			    const void *request,
			    gxBlockHeader &request_header)
// Request the a block be deleted on the remote end. Returns
// zero if no error occur.
{
  SetBlockStatus(request_header, gxDeleteRemoteBlock);

  // Write the delete request and the request itself
  if(WriteBlock(s, sa, request, request_header) != gxSOCKET_NO_ERROR)
    return socket_error;

  return socket_error = gxSOCKET_NO_ERROR;    
}

int gxDatagram::AddBlock(gxsSocket_t s, gxsSocketAddress *sa, 
			 const void *block, gxBlockHeader &block_header)
// Request that this block be added. Returns zero if no error 
// occur.
{
  SetBlockStatus(block_header, gxAddRemoteBlock);

  // Write the add request and then the block to add
  if(WriteBlock(s, sa, block, block_header) != gxSOCKET_NO_ERROR)
    return socket_error;

  return socket_error = gxSOCKET_NO_ERROR;    
}

int gxDatagram::ChangeBlock(gxsSocket_t s, gxsSocketAddress *sa, 
			    const void *request, const void *block, 
			    gxBlockHeader &request_header,  
			    gxBlockHeader &block_header)
// Request that this block be changed. Returns zero if no error 
// occur.
{
  // Write the change request and the request itself
  SetBlockStatus(request_header, gxChangeRemoteBlock);
  if(WriteBlock(s, sa, request, request_header) != gxSOCKET_NO_ERROR)
    return socket_error;

  // Write the changes
  SetBlockStatus(block_header, gxSendBlock);
  if(WriteBlock(s, sa, block, block_header) != gxSOCKET_NO_ERROR)
    return socket_error;

  return socket_error = gxSOCKET_NO_ERROR;    
}

int gxDatagram::WriteAckBlock(gxsSocket_t s, gxsSocketAddress *sa)
// Send an acknowledgment header to the specified socket.
// Returns zero if no errors occur.
{
  gxBlockHeader ack;
  SetBlockStatus(ack, gxAcknowledgeBlock);
  
  if(WriteHeader(s, sa, ack) != gxSOCKET_NO_ERROR)
    return socket_error = gxSOCKET_BLOCKACK_ERROR;
  
  return socket_error = gxSOCKET_NO_ERROR;    
}

int gxDatagram::ReadAckBlock(gxsSocket_t s, gxsSocketAddress *sa)
// Read an acknowledgment block from the specified socket.
// Returns zero if no errors occur.
{
  // Wait for an acknowledgment
  gxBlockHeader ack;
  if(ReadHeader(s, sa, ack) != gxSOCKET_NO_ERROR)
    return socket_error = gxSOCKET_BLOCKACK_ERROR;

  // Read the status byte to determine what to do with this block
  __ULWORD__ block_status = ack.block_status;
  __SBYTE__ status = (__SBYTE__)((block_status & 0xFF00)>>8);
      
  if(status != gxAcknowledgeBlock)
    return socket_error = gxSOCKET_BLOCKACK_ERROR;

  return socket_error = gxSOCKET_NO_ERROR; 
}

int gxDatagram::TerminateConnection(gxsSocket_t s, gxsSocketAddress *sa)
// Block command used to shutdown a server or client.
// Returns zero if no errors occur.
{
  gxBlockHeader gx;
  SetBlockStatus(gx, gxKillServer);

  if(WriteHeader(s, sa, gx) != gxSOCKET_NO_ERROR) return socket_error;

  return socket_error = gxSOCKET_NO_ERROR;  
}

int gxDatagram::CloseConnection(gxsSocket_t s, gxsSocketAddress *sa)
// Block command used to close a persistent client or
// server connection. Returns zero if no errors occur.
{
  gxBlockHeader gx;
  SetBlockStatus(gx, gxCloseConnection);

  if(WriteHeader(s, sa, gx) != gxSOCKET_NO_ERROR) return socket_error;

  return socket_error = gxSOCKET_NO_ERROR;  
}

int gxDatagram::PortNumber(int &port)
// Pass back the port number actually set by the system
// in the "port" variable.
{
  if(GetSockName() < 0) {
    return socket_error;
  }
  port = GetPortNumber();
  return socket_error = gxSOCKET_NO_ERROR;
}

int gxDatagram::RemotePortNumber(int &port)
// Pass back the port number actually set by the system
// in the "port" variable.
{
  port = GetRemotePortNumber();
  return socket_error = gxSOCKET_NO_ERROR;
}

int gxDatagram::HostName(char *hs)
{
  if(GetHostName(hs) < 0) {
    return socket_error;
  }
  return socket_error = gxSOCKET_NO_ERROR;
}

int gxDatagram::RemoteHostName(char *hs)
{
  if(GetRemoteHostName(hs) < 0) {
    return socket_error;
  }
  return socket_error = gxSOCKET_NO_ERROR;
}

int gxDatagram::WriteBlock(const void *buf, int bytes)
{
  return WriteBlock(gxsocket, &sin, buf, bytes);
}

int gxDatagram::TerminateConnection()
{
  return TerminateConnection(gxsocket, &sin);
}

int gxDatagram::CloseConnection()
{
  return CloseConnection(gxsocket, &sin);
}

int gxDatagram::ReadRemoteBlock(void *buf, gxBlockHeader &gx)
{
  return ReadBlock(gxsocket, &remote_sin, buf, gx);
}

int gxDatagram::ReadClientHeader(gxBlockHeader &gx)
{
  return ReadHeader(gxsocket, &remote_sin, gx);
}

int gxDatagram::WriteRemoteBlock(void *buf, int bytes)
{
  return WriteBlock(gxsocket, &remote_sin, buf, bytes);
}

void *gxDatagram::RequestBlock(const void *request,
			     gxBlockHeader &request_header, 
			     gxBlockHeader &requested_block_header)
{
  return RequestBlock(gxsocket, &sin, request, request_header,
		      requested_block_header);
}

int gxDatagram::DeleteBlock(const void *request, gxBlockHeader &request_header)
{
  return DeleteBlock(gxsocket, &sin, request, request_header);
}

int gxDatagram::WriteAckBlock()
{
  return WriteAckBlock(gxsocket, &sin);
}

int gxDatagram::WriteRemoteAckBlock()
{
  return WriteAckBlock(gxsocket, &remote_sin);
}

int gxDatagram::ReadAckBlock()
{
  return ReadAckBlock(gxsocket, &sin);
}

int gxDatagram::ReadRemoteAckBlock()
{
  return ReadAckBlock(gxsocket, &remote_sin);
}

int gxDatagram::AddBlock(const void *block, gxBlockHeader &block_header)
{
  return AddBlock(gxsocket, &sin, block, block_header);
}

int gxDatagram::ChangeBlock(const void *request, const void *block, 
			    gxBlockHeader &request_header, 
			    gxBlockHeader &block_header)
{
  return ChangeBlock(gxsocket, &sin, request, block, 
		     request_header, block_header);
}

#ifdef __BCC32__
#pragma warn .8080
#endif
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
