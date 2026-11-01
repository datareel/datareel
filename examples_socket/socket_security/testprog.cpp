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

6.3 security sweep tests for the gxSocket class (UNIX only, loopback
and socket pairs, no other hosts). Each check runs in a child process
so a crash or a killing signal is reported as a FAIL.

  close       Close() forgets its descriptors: a second Close() (or
              the destructor) must not close a descriptor the program
              opened after the first Close().
  select      ReadSelect()/Recv() with a timeout on a descriptor above
              FD_SETSIZE (select() wrote past the fd_set).
  sigpipe     Send() to a closed peer returns -1 instead of killing the
              program with SIGPIPE (Linux, MSG_NOSIGNAL).
  iotimeout   SetIOTimeout(): a blocking Recv() from a silent peer
              returns -1 after the timeout instead of waiting for ever.
  loopback    A TCP Send()/Recv() round trip over 127.0.0.1.
  address     InitSIN() fills in the structure passed to it, and
              SetRemoteIPAddress() sets the remote address.

Returns 0 if every check passes.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <string.h>
#include "gxsocket.h"

#if defined (__UNIX__)
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/socket.h>

static int failures = 0;

static void check(int ok, const char *what)
{
  printf("%s %s\n", ok ? "PASS" : "FAIL", what);
  fflush(stdout);
  if(!ok) failures++;
}

typedef int (*TestFunc)();

static void run(const char *name, TestFunc f, int max_seconds = 20)
// Run one check in a child process. The child returns 0 if it passed.
{
  fflush(stdout);
  pid_t pid = fork();
  if(pid == 0) {
    signal(SIGPIPE, SIG_DFL); // The default, as in most programs
    alarm(max_seconds);       // A hang is a failure
    int rv = f();
    fflush(stdout);
    _exit(rv);
  }
  int status = 0;
  waitpid(pid, &status, 0);
  if(WIFSIGNALED(status)) {
    char sbuf[256];
    sprintf(sbuf, "%s (killed by signal %d)", name, WTERMSIG(status));
    check(0, sbuf);
    return;
  }
  check(WIFEXITED(status) && WEXITSTATUS(status) == 0, name);
}

static int test_close()
{
  int rv = 0;
  {
    gxSocket a;
    if(a.InitSocket(SOCK_STREAM, 0) < 0) return 2;
    int fd = a.GetSocket();
    a.Close();
    if(a.GetSocket() != -1) {
      printf("  Close() left the descriptor %d set\n", fd);
      rv = 1;
    }
    // The lowest free number is the one Close() just freed
    int fd2 = open("/dev/null", O_RDONLY);
    if(fd2 < 0) return 2;
    a.Close(); // Must not close fd2
    if(fcntl(fd2, F_GETFD) == -1) {
      printf("  The second Close() closed descriptor %d\n", fd2);
      rv = 1;
    }
    // The destructor must not close it either
  }
  return rv;
}

static int test_select()
{
  struct rlimit rl;
  if(getrlimit(RLIMIT_NOFILE, &rl) != 0) return 2;
  const int high_fd = 1500;
  if(rl.rlim_cur <= (rlim_t)high_fd) {
    if(rl.rlim_max <= (rlim_t)high_fd) {
      printf("  SKIP: RLIMIT_NOFILE hard limit is too low\n");
      return 0;
    }
    rl.rlim_cur = high_fd + 1;
    if(setrlimit(RLIMIT_NOFILE, &rl) != 0) return 2;
  }
  int sv[2];
  if(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) return 2;
  if(dup2(sv[0], high_fd) != high_fd) return 2;
  gxSocket s;
  int rv = 0;
  if(s.ReadSelect(high_fd, 0, 200000) != 0) {
    printf("  ReadSelect() with no data did not time out\n");
    rv = 1;
  }
  if(write(sv[1], "xy", 2) != 2) return 2;
  if(s.ReadSelect(high_fd, 2, 0) != 1) {
    printf("  ReadSelect() did not see the data\n");
    rv = 1;
  }
  char buf[2];
  if(s.Recv(high_fd, buf, 2, 2, 0) != 2 || memcmp(buf, "xy", 2) != 0) {
    printf("  Recv() with a timeout failed\n");
    rv = 1;
  }
  return rv;
}

static int test_sigpipe()
{
#if defined (__LINUX__)
  int sv[2];
  if(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) return 2;
  close(sv[1]);
  gxSocket s;
  char buf[4096];
  memset(buf, 'x', sizeof(buf));
  int n = s.Send(sv[0], buf, (int)sizeof(buf));
  if(n != -1) {
    printf("  Send() to a closed peer returned %d\n", n);
    return 1;
  }
  n = s.RawWrite(sv[0], buf, (int)sizeof(buf));
  if(n != -1) {
    printf("  RawWrite() to a closed peer returned %d\n", n);
    return 1;
  }
  return 0;
#else
  printf("  SKIP: MSG_NOSIGNAL is Linux only\n");
  return 0;
#endif
}

static int test_iotimeout()
{
  int sv[2];
  if(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) return 2;
  gxSocket s;
  if(s.SetIOTimeout(sv[0], 1) != 0) {
    printf("  SetIOTimeout() failed\n");
    return 1;
  }
  time_t start = time(0);
  char buf[16];
  int n = s.Recv(sv[0], buf, (int)sizeof(buf)); // No select() timeout
  time_t took = time(0) - start;
  if(n != -1 || took > 5) {
    printf("  Recv() returned %d after %d seconds\n", n, (int)took);
    return 1;
  }
  // 0 removes the limit again
  if(s.SetIOTimeout(sv[0], 0) != 0) return 1;
  return 0;
}

static int test_loopback()
{
  gxSocket server;
  if(server.InitSocket(SOCK_STREAM, 0, (char *)"127.0.0.1") < 0) return 2;
  int on = 1;
  server.SetSockOpt(SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  if(server.Bind() < 0) return 2;
  if(server.Listen(1) < 0) return 2;
  if(server.GetSockName() < 0) return 2;
  int port = server.GetPortNumber();

  gxSocket client;
  if(client.InitSocket(SOCK_STREAM, port, (char *)"127.0.0.1") < 0) return 2;
  if(client.Connect() < 0) return 2;
  if(server.Accept() < 0) return 2;

  char out[100000], in[100000];
  for(int i = 0; i < (int)sizeof(out); i++) out[i] = (char)(i * 7);
  if(client.Send(out, (int)sizeof(out)) != (int)sizeof(out)) return 1;
  if(server.RemoteRecv(in, (int)sizeof(in), 5, 0) != (int)sizeof(in)) return 1;
  if(memcmp(in, out, sizeof(in)) != 0) return 1;
  if(server.RemoteSend("ok", 2) != 2) return 1;
  if(client.Recv(in, 2, 5, 0) != 2 || memcmp(in, "ok", 2) != 0) return 1;
  client.Close();
  // The peer closed: a timed Recv() reports it instead of waiting
  if(server.RemoteRecv(in, 1, 5, 0) != -1) return 1;
  if(server.GetSocketError() != gxSOCKET_DISCONNECTED_ERROR) return 1;
  return 0;
}

static int test_address()
{
  int rv = 0;
  gxSocket s;
  gxsSocketAddress sa;
  memset(&sa, 0xff, sizeof(sa));
  if(s.InitSIN(&sa, AF_INET, 4321, (char *)"127.0.0.1") != 0) return 1;
  if(sa.sin_addr.s_addr != htonl(0x7f000001) || ntohs(sa.sin_port) != 4321) {
    printf("  InitSIN() did not fill in the structure passed to it\n");
    rv = 1;
  }
  s.SetRemoteIPAddress((char *)"127.0.0.1");
  if(s.remote_sin.sin_addr.s_addr != htonl(0x7f000001)) {
    printf("  SetRemoteIPAddress() did not set the address\n");
    rv = 1;
  }
  // Out of range error codes give the invalid code message. (An
  // out of range gxSocketError cannot be made without undefined
  // behaviour, so SocketExceptionMessage()'s check is not tested.)
  const char *msg = s.TestExceptionMessage(-5);
  if(!msg || strcmp(msg, s.TestExceptionMessage(1)) != 0) {
    printf("  TestExceptionMessage() with a negative code\n");
    rv = 1;
  }
  return rv;
}

int main()
{
  run("close: a second Close() does not close a reused descriptor",
      test_close);
  run("select: ReadSelect()/Recv() on a descriptor above FD_SETSIZE",
      test_select);
  run("sigpipe: Send() to a closed peer returns -1", test_sigpipe);
  run("iotimeout: SetIOTimeout() ends a blocking Recv()", test_iotimeout);
  run("loopback: TCP Send()/Recv() round trip", test_loopback);
  run("address: InitSIN() and SetRemoteIPAddress()", test_address);
  printf("%d failure(s)\n", failures);
  return failures ? 1 : 0;
}

#else // Not UNIX
int main()
{
  printf("SKIP: the gxSocket security tests use fork() and socketpair()\n");
  return 0;
}
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
