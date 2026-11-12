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
version 3 of the License, or (at your option) any later version.
 
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library. If not, see
<https://www.gnu.org/licenses/>.

gxStream database block server. Stores the blocks that clients send
in a gxDatabase file and sends them back on request, all over TLS.
gxAddRemoteBlock stores a block and answers with a result block that
holds its file address. gxRequestBlock asks for the block at the
8-byte file address in the request. gxCloseConnection ends the
client session. gxKillServer stops the server only when it was
started with --allow-remote-shutdown.

Usage: stream_server port cert.pem key.pem database_file 
       [--allow-remote-shutdown]
       [--client-ca client_ca.pem] [--allow-fingerprint SHA256]...
       [--allow-cn NAME]... | [--open]

11/01/2026: Clients must present a certificate issued by a CA in
--client-ca whose SHA-256 fingerprint (--allow-fingerprint, from
"openssl x509 -in client.pem -noout -fingerprint -sha256") or subject
CN (--allow-cn) is allowed. Both options can be repeated. Without
them the server refuses to start, unless --open is given to accept
clients without certificates. Each client's identity is logged.

11/01/2026: A requested address must be the object address of a
normal block inside the file, no larger than the stream's maximum
block size; anything else gets an empty reply. Remote shutdown is
off by default. Results carry the full 64-bit file address. Define
STREAM_SERVER_NO_MAIN to reuse serve_client() (tests/fuzz).
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include "gxdbase.h"
#include "gxstream.h"

static __ULWORD__ valid_object_length(gxDatabase &db, FAU_t addr, 
					gxUINT32 max_block)
// 11/01/2026: The address comes from the client. Returns the object
// length if addr is the object address of a normal (not deleted or
// removed) block that lies inside the file, or 0 otherwise.
{
  FAU_t hdr_size = (FAU_t)db.BlockHeaderSize();
  FAU_t heap_start = (FAU_t)db.GetHeapStart();
  FAU_t eof = (FAU_t)db.GetEOF();
  if(addr <= (FAU_t)0 || addr < heap_start + hdr_size || addr > eof) {
    return 0;
  }
  FAU_t block_addr = addr - hdr_size;
  gxBlockHeader blk;
  if(db.ReadBlockHdr(blk, block_addr) != gxDBASE_NO_ERROR) return 0;
  if(!db.TestBlockHeader(blk)) return 0;
  if((__SBYTE__)(blk.block_status & 0xFF) != gxNormalBlock) return 0;
  if((FAU_t)blk.block_length < hdr_size) return 0;
  if((FAU_t)blk.block_length > eof - block_addr) return 0;
  __ULWORD__ len = db.ObjectLength(addr);
  if(len == 0 || len > (__ULWORD__)max_block) return 0;
  return len;
}

static bool serve_client(gxStream &stream, gxDatabase &db, 
			 bool allow_remote_shutdown = false)
// Returns false when the client asks the server to stop
{
  std::vector<unsigned char> block;
  for(;;) {
    gxBlockHeader hdr;
    if(stream.ReadHeader(hdr) != gxSOCKET_NO_ERROR) {
#if !defined (STREAM_SERVER_NO_MAIN)
      std::cout << "Client gone: " << stream.SocketExceptionMessage() << "\n";
#endif
      return true;
    }
    switch(gxStream::BlockStatus(hdr)) {
      case gxAddRemoteBlock: {
	if(stream.ReadBlock(block, hdr) != gxSOCKET_NO_ERROR) return true;
	FAU_t addr = db.Alloc((__ULWORD__)block.size());
	if(addr == (FAU_t)0 || 
	   db.Write(block.data(), (__ULWORD__)block.size(), addr, 1) != 
	   gxDBASE_NO_ERROR) {
	  stream.WriteResultBlock((FAU_t)0);
	  break;
	}
#if !defined (STREAM_SERVER_NO_MAIN)
	std::cout << "Stored " << block.size() << " bytes at " 
		  << (long long)addr << "\n";
#endif
	// 11/01/2026: (int)addr truncated addresses past 2 GB
	stream.WriteResultBlock(addr);
	break;
      }
      case gxRequestBlock: {
	if(stream.ReadBlock(block, hdr) != gxSOCKET_NO_ERROR) return true;
	std::int64_t addr = 0;
	if(block.size() == sizeof(addr)) memcpy(&addr, block.data(), sizeof(addr));
	std::vector<unsigned char> data;
	// 11/01/2026: Read only real, live blocks (was any address > 0)
	__ULWORD__ len = valid_object_length(db, (FAU_t)addr, 
					     stream.MaxBlockSize());
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
#if !defined (STREAM_SERVER_NO_MAIN)
	std::cout << "Client closed the session\n";
#endif
	return true;
      case gxKillServer:
	// 11/01/2026: Any client could stop the server. Now only when
	// the operator allows it.
	if(!allow_remote_shutdown) {
#if !defined (STREAM_SERVER_NO_MAIN)
	  std::cout << "Remote shutdown refused\n";
#endif
	  return true;
	}
#if !defined (STREAM_SERVER_NO_MAIN)
	std::cout << "Client stopped the server\n";
#endif
	return false;
      default:
#if !defined (STREAM_SERVER_NO_MAIN)
	std::cout << "Unknown block type\n";
#endif
	return true;
    }
  }
}

#if !defined (STREAM_SERVER_NO_MAIN)
static std::string printable(const std::string &s)
// 11/01/2026: Client certificate names come from the client. Escape
// control characters so they cannot forge log lines.
{
  static const char hex[] = "0123456789ABCDEF";
  std::string out;
  for(unsigned char c : s) {
    if(c < 0x20 || c == 0x7F || c == '\\') {
      out += "\\x";
      out += hex[(c >> 4) & 0x0F];
      out += hex[c & 0x0F];
    }
    else {
      out += (char)c;
    }
  }
  return out;
}

static void usage(const char *prog)
{
  std::cerr << "Usage: " << prog 
	    << " port cert.pem key.pem database_file"
	    << " [--allow-remote-shutdown]\n"
	    << "       [--client-ca client_ca.pem]"
	    << " [--allow-fingerprint SHA256]... [--allow-cn NAME]..."
	    << " | [--open]\n";
}

int main(int argc, char **argv)
{
  if(argc < 5) {
    usage(argv[0]);
    return 1;
  }
  bool allow_remote_shutdown = false; // 11/01/2026: Off by default
  // 11/01/2026: Client certificates are required unless --open is given
  gxStreamServer server;
  bool open_server = false, client_auth = false;
  for(int i = 5; i < argc; i++) {
    if(strcmp(argv[i], "--allow-remote-shutdown") == 0) {
      allow_remote_shutdown = true;
    }
    else if(strcmp(argv[i], "--client-ca") == 0 && i + 1 < argc) {
      server.SetClientCAFile(argv[++i]);
      client_auth = true;
    }
    else if(strcmp(argv[i], "--allow-fingerprint") == 0 && i + 1 < argc) {
      if(server.AllowClientFingerprint(argv[++i]) != 0) {
	std::cerr << "Bad fingerprint " << printable(argv[i]) << ": "
		  << server.Error() << "\n";
	return 1;
      }
      client_auth = true;
    }
    else if(strcmp(argv[i], "--allow-cn") == 0 && i + 1 < argc) {
      if(server.AllowClientCN(argv[++i]) != 0) {
	std::cerr << server.Error() << "\n";
	return 1;
      }
      client_auth = true;
    }
    else if(strcmp(argv[i], "--open") == 0) {
      open_server = true;
    }
    else {
      std::cerr << "Unknown option " << argv[i] << "\n";
      usage(argv[0]);
      return 1;
    }
  }
  if(open_server && client_auth) {
    std::cerr << "--open cannot be used with --client-ca, "
	      << "--allow-fingerprint or --allow-cn\n";
    return 1;
  }
  if(open_server) {
    // The explicit opt-out: any client with TLS may connect
    server.AllowAnonymousClients(true);
    std::cout << "WARNING: open server, client certificates are not checked"
	      << std::endl;
  }

  // 11/01/2026: Listen before the database is opened or created, so a
  // server that refuses to start leaves nothing behind
  if(server.Listen(std::atoi(argv[1]), argv[2], argv[3]) != 0) {
    std::cerr << server.Error() << "\n";
    if(server.RequireClientAuth() && !open_server &&
       (server.ClientCAFile().empty() || 
	(server.AllowedFingerprints().empty() && server.AllowedCNs().empty()))) {
      std::cerr << "Use --client-ca FILE with --allow-fingerprint SHA256 "
		<< "and/or --allow-cn NAME, or --open for a server that "
		<< "accepts clients without certificates\n";
    }
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

  std::cout << "gxStream server on port " << server.Port() << std::endl;
  bool running = true;
  while(running) {
    gxStream stream;
    if(server.Accept(stream) != 0) {
      std::cout << "Accept failed: " << printable(server.Error()) << std::endl;
      continue;
    }
    // 11/01/2026: Log who connected
    gxsTLSConnection &conn = stream.Connection();
    if(conn.ClientAuthenticated()) {
      std::cout << "Client " << printable(conn.Host()) << " authenticated: CN=" 
		<< printable(conn.PeerCN()) << " SHA-256=" 
		<< printable(conn.PeerFingerprint()) << std::endl;
    }
    else {
      std::cout << "Client " << printable(conn.Host()) 
		<< " connected without a client certificate" << std::endl;
    }
    running = serve_client(stream, db, allow_remote_shutdown);
    stream.Close();
  }
  db.Close();
  return 0;
}
#endif // !STREAM_SERVER_NO_MAIN
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
