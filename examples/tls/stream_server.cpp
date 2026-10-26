// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: stream_server.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 10/26/2026
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

gxStream database block server. Stores the blocks that clients send
in a gxDatabase file and sends them back on request, all over TLS.
gxAddRemoteBlock stores a block and answers with a result block that
holds its file address. gxRequestBlock asks for the block at the
8-byte file address in the request. gxCloseConnection ends the
client session and gxKillServer stops the server.

Usage: stream_server port cert.pem key.pem database_file
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#include "gxdbase.h"
#include "gxstream.h"

static bool serve_client(gxStream &stream, gxDatabase &db)
// Returns false when the client asks the server to stop
{
  std::vector<unsigned char> block;
  for(;;) {
    gxBlockHeader hdr;
    if(stream.ReadHeader(hdr) != gxSOCKET_NO_ERROR) {
      std::cout << "Client gone: " << stream.SocketExceptionMessage() << "\n";
      return true;
    }
    switch(gxStream::BlockStatus(hdr)) {
      case gxAddRemoteBlock: {
	if(stream.ReadBlock(block, hdr) != gxSOCKET_NO_ERROR) return true;
	FAU_t addr = db.Alloc((__ULWORD__)block.size());
	if(addr == (FAU_t)0 || 
	   db.Write(block.data(), (__ULWORD__)block.size(), addr, 1) != 
	   gxDBASE_NO_ERROR) {
	  stream.WriteResultBlock(0);
	  break;
	}
	std::cout << "Stored " << block.size() << " bytes at " 
		  << (long long)addr << "\n";
	stream.WriteResultBlock((int)addr);
	break;
      }
      case gxRequestBlock: {
	if(stream.ReadBlock(block, hdr) != gxSOCKET_NO_ERROR) return true;
	std::int64_t addr = 0;
	if(block.size() == sizeof(addr)) memcpy(&addr, block.data(), sizeof(addr));
	std::vector<unsigned char> data;
	__ULWORD__ len = addr > 0 ? db.ObjectLength((FAU_t)addr) : 0;
	if(len > 0) {
	  data.resize(len);
	  if(db.Read(data.data(), len, (FAU_t)addr) != gxDBASE_NO_ERROR) {
	    data.clear();
	  }
	}
	stream.WriteBlock(data.data(), (int)data.size());
	break;
      }
      case gxCloseConnection:
	std::cout << "Client closed the session\n";
	return true;
      case gxKillServer:
	std::cout << "Client stopped the server\n";
	return false;
      default:
	std::cout << "Unknown block type\n";
	return true;
    }
  }
}

int main(int argc, char **argv)
{
  if(argc < 5) {
    std::cerr << "Usage: " << argv[0] 
	      << " port cert.pem key.pem database_file\n";
    return 1;
  }
  gxDatabase db;
  if(db.Open(argv[4]) != gxDBASE_NO_ERROR) {
    if(db.Create(argv[4]) != gxDBASE_NO_ERROR) {
      std::cerr << "Cannot open " << argv[4] << ": " 
		<< db.DatabaseExceptionMessage() << "\n";
      return 1;
    }
  }

  gxsTLSServer server;
  if(server.Listen(std::atoi(argv[1]), argv[2], argv[3]) != 0) {
    std::cerr << server.Error() << "\n";
    return 1;
  }
  std::cout << "gxStream server on port " << server.Port() << std::endl;
  bool running = true;
  while(running) {
    gxStream stream;
    if(server.Accept(stream.Connection()) != 0) {
      std::cout << "Accept failed: " << server.Error() << "\n";
      continue;
    }
    running = serve_client(stream, db);
    stream.Close();
  }
  db.Close();
  return 0;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
