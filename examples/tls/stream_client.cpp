// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: stream_client.cpp
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

gxStream database block client. Sends records to stream_server over
TLS, reads them back by file address and checks that they match.

Usage: stream_client host port [--ca ca.pem] [--insecure] [--stop]
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include "gxstream.h"

int main(int argc, char **argv)
{
  if(argc < 3) {
    std::cerr << "Usage: " << argv[0] 
	      << " host port [--ca ca.pem] [--insecure] [--stop]\n";
    return 1;
  }
  gxStream stream;
  bool stop = false;
  for(int i = 3; i < argc; i++) {
    if(strcmp(argv[i], "--ca") == 0 && i + 1 < argc) {
      stream.Connection().SetCAFile(argv[++i]);
    }
    else if(strcmp(argv[i], "--insecure") == 0) {
      stream.Connection().SetVerifyPeer(false);
    }
    else if(strcmp(argv[i], "--stop") == 0) {
      stop = true;
    }
  }
  stream.SetTimeOut(30);
  if(stream.Connect(argv[1], std::atoi(argv[2])) != gxSOCKET_NO_ERROR) {
    std::cerr << stream.Error() << "\n";
    return 1;
  }

  const char *records[] = { "First record", "Second record, a bit longer",
			    "Third record" };
  int failures = 0;
  for(const char *rec : records) {
    gxBlockHeader hdr;
    hdr.block_length = (__ULWORD__)strlen(rec);
    int addr = 0;
    if(stream.AddBlock(rec, hdr) != gxSOCKET_NO_ERROR ||
       stream.ReadResultBlock(addr) != gxSOCKET_NO_ERROR || addr == 0) {
      std::cerr << "Add failed: " << stream.SocketExceptionMessage() << "\n";
      return 1;
    }

    std::int64_t req_addr = addr;
    gxBlockHeader req_hdr, reply_hdr;
    req_hdr.block_length = sizeof(req_addr);
    std::vector<unsigned char> reply;
    if(stream.RequestBlock(&req_addr, req_hdr, reply_hdr, reply) != 
       gxSOCKET_NO_ERROR) {
      std::cerr << "Request failed: " << stream.SocketExceptionMessage() << "\n";
      return 1;
    }
    std::string got(reply.begin(), reply.end());
    bool ok = (got == rec);
    if(!ok) failures++;
    std::cout << "Address " << addr << ": \"" << got << "\" " 
	      << (ok ? "OK" : "MISMATCH") << "\n";
  }
  if(stop) stream.TerminateConnection();
  else stream.CloseConnection();
  stream.Close();
  return failures == 0 ? 0 : 1;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
