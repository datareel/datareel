// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: scomserv.h
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

The gxSerialCommServer class is a used to establish a client
or server connection that can transmit or receive database
blocks over a serial port.
*/
// ----------------------------------------------------------- //   
#ifndef __GX_COMMSERVER_HPP__
#define __GX_COMMSERVER_HPP__

#include "gxdlcode.h"

#include "gxscomm.h"
#include "gxheader.h"

// Serial communications server/client
class GXDLCODE_API gxSerialCommServer : public gxSerialComm
{
public:
  gxSerialCommServer() { }
  ~gxSerialCommServer() { }
  
public: // Client/Server configuration
  int InitCommServer(char *dev, int sp = 9600, char pr = 'N', int cs = 8,
		     int sb = 1,
		     int flow = gxSerialComm::scommNO_FLOW_CONTROL);

public: // Database block functions
  void SetBlockStatus(gxBlockHeader &gx, __SBYTE__ dev_status,
		      __SBYTE__ file_status = gxRemoteDeviceBlock);
  int WriteBlock(const void *buf, int bytes);
  int WriteBlock(const void *buf, gxBlockHeader &gx);
  int WriteHeader(gxBlockHeader &gx);
  int ReadBlock(void *buf, gxBlockHeader &gx);
  int ReadHeader(gxBlockHeader &gx);
  void *RequestBlock(const void *request, gxBlockHeader &request_header, 
		     gxBlockHeader &requested_block_header);
  int AddBlock(const void *block, gxBlockHeader &block_header);
  int DeleteBlock(const void *request, gxBlockHeader &request_header);
  int ChangeBlock(const void *request, const void *block,
		  gxBlockHeader &request_header, gxBlockHeader &block_header);
  int WriteAckBlock();
  int ReadAckBlock();
  int TerminateConnection();
  int CloseConnection();
};

#endif // __GX_COMMSERVER_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
