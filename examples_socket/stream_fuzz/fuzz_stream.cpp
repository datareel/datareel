// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: fuzz_stream.cpp
// Compiler Used: CLANG (libFuzzer)
// Produced By: DataReel Software Development Team
// File Creation Date: 10/31/2026
// Date Last Modified: 10/31/2026
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

6.3 libFuzzer target (UNIX) for the gxStream code that reads data
sent by the peer: block headers, blocks, acknowledgments, result
blocks and RequestBlock() replies. The fuzz input is what the peer
sends, over a socket pair, closed after the input. The first byte
picks the side:

  even  Server: a loop of ReadClientHeader() and, for a block, a
        sized ReadRemoteBlock() and an unsized ReadBlock() into a
        buffer of the length the header gives (MaxBlockSize() checked).
  odd   Client: RequestBlock(), ReadResultBlock64(), ReadAckBlock().

Build with build_fuzz.sh in this directory.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include "gxstream.h"

static const gxUINT32 fuzz_max_block = 1 << 20;

static void fuzz_server(gxStream &st, gxsSocket_t s)
{
  st.SetRemoteSocket(s);
  for(int i = 0; i < 16; i++) {
    gxBlockHeader gx;
    if(st.ReadClientHeader(gx) != 0) break;
    __ULWORD__ block_status = gx.block_status;
    __SBYTE__ status = (__SBYTE__)((block_status & 0xFF00)>>8);
    if(status == gxSendBlock || status == gxAddRemoteBlock ||
       status == gxChangeRemoteBlock || status == gxRequestBlock ||
       status == gxDeleteRemoteBlock) {
      if(i & 1) {
	char buf[512];
	if(st.ReadRemoteBlock(buf, (int)sizeof(buf), gx) != 0) break;
      }
      else {
	// The old form, used the way a careful caller must: allocate
	// what the header says only after checking it
	if((__ULWORD__)gx.block_length > (__ULWORD__)st.MaxBlockSize()) break;
	char *buf = new char[(__ULWORD__)gx.block_length + 1];
	int rv = st.ReadRemoteBlock(buf, gx);
	delete[] buf;
	if(rv != 0) break;
      }
    }
    else if(status == gxAcknowledgeBlock) {
      FAU_t code = 0;
      (void)code;
    }
    else {
      break;
    }
  }
  st.ReleaseRemoteSocket();
}

static void fuzz_client(gxStream &st, gxsSocket_t s)
{
  gxBlockHeader req, got;
  st.SetBlockStatus(req, gxRequestBlock);
  req.block_length = 4;
  void *p = st.RequestBlock(s, "key1", req, got, 1, 0);
  if(p) delete[] (char *)p;
  FAU_t code = 0;
  st.ReadResultBlock64(s, code, 1, 0);
  int icode = 0;
  st.ReadResultBlock(s, icode, 1, 0);
  st.ReadAckBlock(s, 1, 0);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  static int init = 0;
  if(!init) {
    signal(SIGPIPE, SIG_IGN);
    init = 1;
  }
  if(size < 1 || size > 16384) return 0;
  int sv[2];
  if(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) return 0;
  int bufsize = 256 * 1024;
  setsockopt(sv[1], SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize));
  setsockopt(sv[0], SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));
  size_t left = size - 1;
  const uint8_t *p = data + 1;
  while(left > 0) {
    ssize_t n = write(sv[1], p, left);
    if(n <= 0) break;
    p += n;
    left -= (size_t)n;
  }
  shutdown(sv[1], SHUT_WR); // The peer has said everything

  gxStream st;
  st.SetMaxBlockSize(fuzz_max_block);
  st.SetTimeOut(1, 0);
  if(data[0] & 1) {
    fuzz_client(st, sv[0]);
  }
  else {
    fuzz_server(st, sv[0]);
  }
  close(sv[0]);
  close(sv[1]);
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
