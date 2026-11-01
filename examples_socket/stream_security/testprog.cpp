// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: testprog.cpp
// Compiler Used: MSVC, GCC
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
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
USA

6.3 security sweep tests for the gxStream class (UNIX only; socket
pairs and 127.0.0.1). Each check runs in a child process, so a hang
(alarm), a crash or an abort is reported as a FAIL.

Data path:
  request     RequestBlock() with a huge block length from the peer
              returns 0 without allocating it (it allocated whatever
              the peer asked for, std::bad_alloc killed the program).
  sized       ReadRemoteBlock()/ReadBlock() with the buffer size read
              nothing when the block does not fit.
  maxblock    The old ReadBlock() refuses lengths over MaxBlockSize().
  negative    WriteBlock() with a negative length sends nothing (it
              sent a header with a huge length).
  result64    WriteResultBlock64()/ReadResultBlock64() keep a 64-bit
              file address.
  plain       Plain TCP data path between StreamClient() and
              StreamServer().

Returns 0 if every check passes.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <string.h>
#include "gxstream.h"

#if defined (__UNIX__)
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/socket.h>

static int failures = 0;

static void check(int ok, const char *what)
{
  printf("%s %s\n", ok ? "PASS" : "FAIL", what);
  fflush(stdout);
  if(!ok) failures++;
}

typedef int (*TestFunc)();

static void run(const char *name, TestFunc f, int max_seconds = 30)
// Run one check in a child process. The child returns 0 if it passed.
{
  fflush(stdout);
  pid_t pid = fork();
  if(pid == 0) {
    signal(SIGPIPE, SIG_DFL);
    alarm(max_seconds);
    _exit(f());
  }
  int status = 0;
  waitpid(pid, &status, 0);
  if(WIFSIGNALED(status)) {
    char sbuf[256];
    snprintf(sbuf, sizeof(sbuf), "%s (killed by signal %d)",
	     name, WTERMSIG(status));
    check(0, sbuf);
    return;
  }
  check(WIFEXITED(status) && WEXITSTATUS(status) == 0, name);
}

static void make_header(gxStream &st, gxBlockHeader &gx, __SBYTE__ status,
			__ULWORD__ len)
{
  st.SetBlockStatus(gx, status);
  gx.block_length = len;
}

static int test_request()
{
  int sv[2];
  if(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) return 2;
  gxStream peer, client;
  // The peer answers with a header that claims a huge block
  gxBlockHeader reply;
  __ULWORD__ huge = (__ULWORD__)0x7ffffff0;
  if(sizeof(__ULWORD__) >= 8) huge = (__ULWORD__)(((__ULWORD__)1) << 46);
  make_header(peer, reply, gxSendBlock, huge);
  if(peer.WriteHeader(sv[1], reply) != 0) return 2;
  gxBlockHeader req, got;
  void *p = client.RequestBlock(sv[0], "k", req, got, 2, 0);
  if(p) {
    delete[] (char *)p;
    return 1;
  }
  if(client.GetSocketError() != gxSOCKET_BLOCKSIZE_ERROR) {
    printf("  error %d: %s\n", (int)client.GetSocketError(),
	   client.SocketExceptionMessage());
    return 1;
  }
  return 0;
}

static int test_sized()
{
  int sv[2];
  if(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) return 2;
  gxStream peer, server;
  char data[100];
  memset(data, 'A', sizeof(data));
  if(peer.WriteBlock(sv[1], data, (int)sizeof(data)) != 0) return 2;
  gxBlockHeader gx;
  if(server.ReadHeader(sv[0], gx, 2, 0) != 0) return 2;
  char buf[20];
  memset(buf, 'z', sizeof(buf));
  if(server.ReadBlock(sv[0], buf, 10, gx, 2, 0) != gxSOCKET_BLOCKSIZE_ERROR) {
    return 1;
  }
  for(int i = 0; i < (int)sizeof(buf); i++) if(buf[i] != 'z') return 1;
  // The same through ReadRemoteBlock() on the remote socket
  server.SetRemoteSocket(sv[0]);
  if(server.ReadRemoteBlock(buf, 10, gx) != gxSOCKET_BLOCKSIZE_ERROR) return 1;
  server.ReleaseRemoteSocket();
  // A block that fits is read
  char big[100];
  if(server.ReadBlock(sv[0], big, (int)sizeof(big), gx, 2, 0) != 0) return 1;
  if(memcmp(big, data, sizeof(big)) != 0) return 1;
  return 0;
}

static int test_maxblock()
{
  int sv[2];
  if(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) return 2;
  gxStream peer, server;
  if(server.MaxBlockSize() != gxSTREAM_DEF_MAX_BLOCK) return 1;
  server.SetMaxBlockSize(16);
  char data[100];
  memset(data, 'A', sizeof(data));
  if(peer.WriteBlock(sv[1], data, (int)sizeof(data)) != 0) return 2;
  gxBlockHeader gx;
  if(server.ReadHeader(sv[0], gx, 2, 0) != 0) return 2;
  char buf[100];
  if(server.ReadBlock(sv[0], buf, gx, 2, 0) != gxSOCKET_BLOCKSIZE_ERROR) {
    return 1;
  }
  return 0;
}

static int test_negative()
{
  int sv[2];
  if(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) return 2;
  gxStream st;
  char data[16];
  memset(data, 0, sizeof(data));
  if(st.WriteBlock(sv[0], data, -1) == 0) return 1;
  struct pollfd pfd;
  pfd.fd = sv[1]; pfd.events = POLLIN; pfd.revents = 0;
  if(poll(&pfd, 1, 200) != 0) {
    printf("  WriteBlock(-1) sent data\n");
    return 1;
  }
  // A null buffer with a length is refused too
  gxBlockHeader gx;
  make_header(st, gx, gxSendBlock, 10);
  if(st.WriteBlock(sv[0], 0, gx) == 0) return 1;
  return 0;
}

static int test_result64()
{
  int sv[2];
  if(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) return 2;
  gxStream a, b;
  FAU_t addr = (FAU_t)123456789;
  if(sizeof(FAU) >= 8 && sizeof(FAU_t) >= 8) {
    addr = (FAU_t)((((FAU_t)1) << 40) + 12345);
  }
  if(a.WriteResultBlock64(sv[0], addr) != 0) return 1;
  FAU_t got = 0;
  if(b.ReadResultBlock64(sv[1], got, 2, 0) != 0) return 1;
  if(got != addr) return 1;
  // The int forms still work
  int code = 0;
  if(a.WriteResultBlock(sv[0], -42) != 0) return 1;
  if(b.ReadResultBlock(sv[1], code, 2, 0) != 0 || code != -42) return 1;
  return 0;
}

// Plain TCP server used by the plain test: reads one block and answers
// with its length as a result block
static int plain_server_child(gxStream &server)
{
  if(server.Accept() < 0) return 2;
  gxBlockHeader gx;
  if(server.ReadClientHeader(gx) != 0) return 3;
  char buf[256];
  if(server.ReadRemoteBlock(buf, (int)sizeof(buf), gx) != 0) return 4;
  if(server.WriteRemoteResultBlock((int)gx.block_length) != 0) return 5;
  return 0;
}

static int start_server(gxStream &server)
// Start a stream server on a free port. Returns the port or -1.
{
  if(server.StreamServer(0) != 0) return -1;
  int port = 0;
  if(server.PortNumber(port) != 0) return -1;
  return port;
}

static int test_plain()
{
  gxStream server;
  int port = start_server(server);
  if(port < 0) {
    printf("  StreamServer(): %s\n", server.SocketExceptionMessage());
    return 2;
  }
  pid_t pid = fork();
  if(pid == 0) _exit(plain_server_child(server));
  server.Close();
  gxStream client;
  int rc = 0;
  if(client.StreamClient(port, (char *)"127.0.0.1") != 0) {
    rc = 2;
  }
  else {
    char msg[] = "plain data path";
    int result = -1;
    if(client.WriteBlock(msg, (int)sizeof(msg)) != 0) rc = 1;
    else if(client.ReadResultBlock(result) != 0) rc = 1;
    else if(result != (int)sizeof(msg)) rc = 1;
  }
  int status = 0;
  waitpid(pid, &status, 0);
  if(!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    printf("  server child status %d\n", status);
    rc = 1;
  }
  return rc;
}

int main()
{
  run("request: RequestBlock() refuses a huge block length", test_request);
  run("sized: ReadBlock()/ReadRemoteBlock() with a buffer size", test_sized);
  run("maxblock: ReadBlock() refuses lengths over MaxBlockSize()",
      test_maxblock);
  run("negative: WriteBlock() with a negative length", test_negative);
  run("result64: 64-bit result blocks", test_result64);
  run("plain: TCP data path", test_plain);
  printf("%d failure(s)\n", failures);
  return failures ? 1 : 0;
}

#else // Not UNIX
int main()
{
  printf("SKIP: the gxStream security tests use fork() and socketpair()\n");
  return 0;
}
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
