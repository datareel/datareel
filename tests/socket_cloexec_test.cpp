// socket_cloexec_test.cpp - 11/17/2026 (7.4, from 6.4.1): sockets are not passed to programs this process runs.
// Every socket gxSocket opens (Listen, Connect, Accept, OpenDatagram) is close-on-exec (POSIX) or not inheritable
// (Windows). POSIX also checks the effect: a child started (fork and exec) while the server listens does not keep
// the port, so the server can close and bind it again while the child still runs. Usage: socket_cloexec_test PORT.
#include "gxdlcode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "gxsocket.h"
#if defined (_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#endif
using namespace datareel;

static int pass = 0, fail = 0;
static void check(bool ok, const std::string &what) { printf("%s %s\n", ok ? "PASS" : "FAIL", what.c_str()); fflush(stdout); (ok ? pass : fail)++; }

static bool not_inherited(gxsSocket_t s)
{
#if defined (_WIN32)
  DWORD fl = 0;
  return GetHandleInformation((HANDLE)s, &fl) && !(fl & HANDLE_FLAG_INHERIT);
#else
  int fl = fcntl(s, F_GETFD, 0);
  return fl >= 0 && (fl & FD_CLOEXEC);
#endif
}

int main(int argc, char **argv)
{
  int port = argc > 1 ? atoi(argv[1]) : 48210;
  gxSocket srv;
  check(srv.Listen(port, "127.0.0.1") == 0, "Listen() on 127.0.0.1:" + std::to_string(port));
  check(not_inherited(srv.GetSocket()), "the listening socket is not passed to child programs");
  gxSocket cli;
  check(cli.Connect("127.0.0.1", port, 5) == 0, "Connect()");
  check(not_inherited(cli.GetSocket()), "the connected socket is not passed to child programs");
  gxSocket acc = srv.Accept();
  check(acc.IsOpen() && not_inherited(acc.GetSocket()), "the accepted socket is not passed to child programs");
  gxSocket dg;
  check(dg.OpenDatagram(AF_INET, 0) == 0 && not_inherited(dg.GetSocket()), "a datagram socket is not passed to child programs");
#if !defined (_WIN32)
  // the effect: a child started now (exec'd sleep) must not keep the listening port
  pid_t k = fork();
  if(k == 0) { execlp("sleep", "sleep", "5", (char *)0); _exit(127); }
  usleep(300000);                       // the child has exec'd (until then it holds copies, as any fork does)
  acc.Close(); cli.Close(); srv.Close();
  gxSocket again;
  bool rebound = again.Listen(port, "127.0.0.1") == 0;
  check(rebound, "the server closes and binds its port again while an exec'd child still runs");
  again.Close();
  kill(k, SIGKILL); waitpid(k, 0, 0);
#endif
  printf("%d passed, %d failed\n", pass, fail);
  return fail ? 1 : 0;
}
