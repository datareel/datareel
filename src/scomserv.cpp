// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: scomserv.cpp
// C++ Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
// Date Last Modified: 11/01/2026
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

The gxSerialCommServer class is a used to establish a client
or server connection that can transmit or receive database
blocks over a serial port.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <new>
#include "scomserv.h"

namespace datareel {



int gxSerialCommServer::InitCommServer(char *dev, int sp, char pr, int cs,
				       int sb, int flow)
{
  int status = OpenSerialPort(dev); 
  if(status < 0) return scomm_error;

  // Make sure read/write access is permitted
  if(status != gxSerialComm::scommREAD_WRITE)
    return scomm_error = gxSerialComm::scomm_INIT_ERROR;

  SetBaudRate(sp);
  SetCharacterSize(cs);
  SetParity(pr);
  SetStopBits(sb);
  SetFlowControl(flow);
  BinaryMode();

  if(InitSerialPort() < 0) return scomm_error;
  
  return scomm_error = gxSerialComm::scomm_NO_ERROR;
}

void gxSerialCommServer::SetBlockStatus(gxBlockHeader &gx,
					__SBYTE__ dev_status,
					__SBYTE__ file_status)
// Set the database block header status member control byte.
// NOTE: The block's next deleted variable must be set, if
// needed, after a call to this function. The block length
// will not be set hear.
{
  __ULWORD__ block_status = dev_status;
  block_status = (block_status<<8) & 0xff00;
  block_status += file_status;
  gx.block_status = block_status;
  gx.block_check_word = gxCheckWord;
  gx.block_nd_fptr = (FAU)0;
}

int gxSerialCommServer::WriteBlock(const void *buf, int bytes)
// Write a block of raw data to a serial port. NOTE: All block read and 
// write operations must operate in blocking mode to maintain 
// synchronization between the transmitter and receiver. Returns zero 
// if no errors occur.
{
  gxBlockHeader gx;
  SetBlockStatus(gx, gxSendBlock);
  gx.block_length = bytes;

  // Write the block header
  return WriteBlock(buf, gx);
}

int gxSerialCommServer::WriteBlock(const void *buf, gxBlockHeader &gx)
// Write a database block. Returns zero if no errors occur. NOTE: This
// function assumes that the block header has already been initialized.
{
  // Write the block header
  if(WriteHeader(gx) != gxSerialComm::scomm_NO_ERROR) return scomm_error;

  // Write the block
  if(Send((char *)buf, gx.block_length) < 0)
    return scomm_error;

  if((gxUINT32)bytes_moved != gx.block_length) 
      return scomm_error = gxSerialComm::scomm_BLOCKSIZE_ERROR;
  
  return scomm_error = gxSerialComm::scomm_NO_ERROR;
}

int gxSerialCommServer::WriteHeader(gxBlockHeader &gx)
// Write a database block header. Returns zero if no errors occur.
{
  // Write the block header
  unsigned char wb[gxWireBlockHeaderSize];      // 11/15/2026 (beta 5): the canonical wire header (gxheader.h)
  gxWirePackBlockHeader(gx, wb);
  if(Send((char *)wb, gxWireBlockHeaderSize) < 0)
    return scomm_error;

  // If the bytes sent do not equal the bytes moved then
  // the transmitter and receiver are no longer synchronized.
  if(bytes_moved != gxWireBlockHeaderSize) 
    return scomm_error = gxSerialComm::scomm_BLOCKHEADER_ERROR;

  return scomm_error = gxSerialComm::scomm_NO_ERROR;
}

int gxSerialCommServer::ReadBlock(void *buf, gxBlockHeader &gx)
// Read database block. Returns zero if no errors occur.
// 11/01/2026: This wrote gx.block_length bytes, a value the peer sends,
// into a caller buffer of unknown size. Blocks over 
// gxsSCOMM_MAX_UNSIZED_BLOCK are now refused; call the overload that 
// takes the buffer size instead.
{
  return ReadBlock(buf, gxsSCOMM_MAX_UNSIZED_BLOCK, gx);
}

int gxSerialCommServer::ReadBlock(void *buf, __ULWORD__ buf_size,
				  gxBlockHeader &gx)
// 11/01/2026: Read a database block into a buffer of buf_size bytes.
// Returns zero if no errors occur or scomm_BUFFER_OVERFLOW if the block
// does not fit. A refused block is left unread, so the caller should
// treat the connection as out of sync.
{
  if(!buf) return scomm_error = gxSerialComm::scomm_BUFFER_OVERFLOW;
  if((__ULWORD__)gx.block_length > buf_size || 
     (__ULWORD__)gx.block_length > (__ULWORD__)0x7fffffff) {
    return scomm_error = gxSerialComm::scomm_BUFFER_OVERFLOW;
  }
  if(gx.block_length == 0) { // Nothing to read
    bytes_read = 0;
    return scomm_error = gxSerialComm::scomm_NO_ERROR;
  }
  if(Recv(buf, (int)gx.block_length) < 0)
    return scomm_error;

  // If the bytes read do not equal the bytes received then
  // the transmitter and receiver are no longer synchronized.
  if((gxUINT32)bytes_read != gx.block_length) 
    return scomm_error = gxSerialComm::scomm_BLOCKSIZE_ERROR;

  return scomm_error = gxSerialComm::scomm_NO_ERROR;
}

int gxSerialCommServer::ReadHeader(gxBlockHeader &gx)
// Read a database block header. Returns zero if no errors occur or
// an error number corresponding to a value defined in the serial
// comm internal error code enumeration.
{
  unsigned char wb[gxWireBlockHeaderSize];      // 11/15/2026 (beta 5): the canonical wire header (gxheader.h)
  if(Recv((char *)wb, gxWireBlockHeaderSize) < 0)
    return scomm_error;

  // If the bytes read do not equal the bytes received then
  // the transmitter and receiver are no longer synchronized.
  if(bytes_read != gxWireBlockHeaderSize) 
    return scomm_error = gxSerialComm::scomm_BLOCKHEADER_ERROR;
  gxWireUnpackBlockHeader(wb, gx);
      
  if(gx.block_check_word != gxCheckWord) 
    return scomm_error = gxSerialComm::scomm_BLOCKSYNC_ERROR;
  	
  return scomm_error = gxSerialComm::scomm_NO_ERROR;    
}

void *gxSerialCommServer::RequestBlock(const void *request,
				       gxBlockHeader &request_header, 
				       gxBlockHeader &requested_block_header)
// Request a block. Returns the requested block or a null value if
// an error occurs.
{
  SetBlockStatus(request_header, gxRequestBlock);

  // Write the request header and the request itself
  if(WriteBlock(request, request_header) != gxSerialComm::scomm_NO_ERROR)
    return 0;
  
  // Read the returned request header 
  if(ReadHeader(requested_block_header) != gxSerialComm::scomm_NO_ERROR) 
    return 0;

  // 10/26/2026: The length comes from the other end of the serial line;
  // refuse blocks over 64 MB instead of allocating whatever it says
  if((__ULWORD__)requested_block_header.block_length > 
     (__ULWORD__)(64*1024*1024)) {
    return 0;
  }

  // Read the returned requested block
  char *buf = new(std::nothrow) char[(__ULWORD__)requested_block_header.block_length];
  if(!buf) return 0;
  
  if(Recv(buf, requested_block_header.block_length) < 0) {
    delete[] buf;
    return 0;
  }
  // Check the byte count to ensure that all blocks were received.
  if((gxUINT32)bytes_read != requested_block_header.block_length) {
    delete[] buf;
    return 0; 
  }

  return (void *)(buf); // Return the requested block
}

int gxSerialCommServer::AddBlock(const void *block,
				 gxBlockHeader &block_header)
// Request that this block be added. Returns zero if no error 
// occur.
{
  SetBlockStatus(block_header, gxAddRemoteBlock);
  // Write the add request and then the block to add

  if(WriteBlock(block, block_header) != gxSerialComm::scomm_NO_ERROR)
    return scomm_error;

  return scomm_error = gxSerialComm::scomm_NO_ERROR;    
}

int gxSerialCommServer::DeleteBlock(const void *request,
				    gxBlockHeader &request_header)
// Request the a block be deleted on the remote end. Returns
// zero if no error occur.
{
  SetBlockStatus(request_header, gxDeleteRemoteBlock);

  // Write the delete request and the request itself
  if(WriteBlock(request, request_header) != gxSerialComm::scomm_NO_ERROR)
    return scomm_error;

  return scomm_error = gxSerialComm::scomm_NO_ERROR;    
}

int gxSerialCommServer::ChangeBlock(const void *request, const void *block,
				    gxBlockHeader &request_header,
				    gxBlockHeader &block_header)
// Request that this block be changed. Returns zero if no error 
// occur.
{
  // Write the change request and the request itself
  SetBlockStatus(request_header, gxChangeRemoteBlock);
  if(WriteBlock(request, request_header) != gxSerialComm::scomm_NO_ERROR)
    return scomm_error;

  // Write the changes
  SetBlockStatus(block_header, gxSendBlock);
  if(WriteBlock(block, block_header) != gxSerialComm::scomm_NO_ERROR)
    return scomm_error;

  return scomm_error = gxSerialComm::scomm_NO_ERROR;    
}

int gxSerialCommServer::WriteAckBlock()
// Send an acknowledgment header. Returns zero if no errors occur.
{
  gxBlockHeader ack;
  SetBlockStatus(ack, gxAcknowledgeBlock);
  
  if(WriteHeader(ack) != gxSerialComm::scomm_NO_ERROR)
    return scomm_error = gxSerialComm::scomm_BLOCKACK_ERROR;
  
  return scomm_error = gxSerialComm::scomm_NO_ERROR;    
}

int gxSerialCommServer::ReadAckBlock()
// Read an acknowledgment block. Returns zero if no errors occur.
{
  // Wait for an acknowledgment
  gxBlockHeader ack;
  if(ReadHeader(ack) != gxSerialComm::scomm_NO_ERROR)
    return scomm_error = gxSerialComm::scomm_BLOCKACK_ERROR;

  // Read the status byte to determine what to do with this block
  __ULWORD__ block_status = ack.block_status;
  __SBYTE__ status = (__SBYTE__)((block_status & 0xFF00)>>8);
      
  if(status != gxAcknowledgeBlock)
    return scomm_error = gxSerialComm::scomm_BLOCKACK_ERROR;

  return scomm_error = gxSerialComm::scomm_NO_ERROR; 
}

int gxSerialCommServer::TerminateConnection()
// Block command used to shutdown a server or client.
// Returns zero if no errors occur.
{
  gxBlockHeader gx;
  SetBlockStatus(gx, gxKillServer);

  if(WriteHeader(gx) != gxSerialComm::scomm_NO_ERROR) return scomm_error;

  return scomm_error = gxSerialComm::scomm_NO_ERROR;  
}

int gxSerialCommServer::CloseConnection()
// Block command used to close a persistent client or
// server connection. Returns zero if no errors occur.
{
  gxBlockHeader gx;
  SetBlockStatus(gx, gxCloseConnection);

  if(WriteHeader(gx) != gxSerialComm::scomm_NO_ERROR) return scomm_error;

  return scomm_error = gxSerialComm::scomm_NO_ERROR;  
}

} // namespace datareel

// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
