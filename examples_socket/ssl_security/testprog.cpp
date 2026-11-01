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

6.3 security sweep tests for the OpenSSL gxSSL class (UNIX only, over
127.0.0.1 with throwaway certificates made by the openssl program).
Each check runs in a child process, so a hang (alarm) or a killing
signal is reported as a FAIL.

  hs_client   ConnectSSLSocket() to a server that never answers ends
              after SetHandshakeTimeout() instead of waiting for ever.
  hs_server   AcceptSSLSocket() from a client that never speaks ends
              after the handshake timeout.
  close       CloseSSLSocket() returns at once when the peer does not
              answer the close_notify (it waited for the peer).
  pending     RecvSSLSocket() with a timeout returns the rest of a
              record OpenSSL has already decrypted (it timed out).
  verify      VerifyCert() checks the subject alternative names and IP
              addresses (only the CN was compared), and does not crash
              without a connection.
  sigpipe     SendSSLSocket() to a peer that has gone returns -1
              instead of killing the program with SIGPIPE.
  message     SSLExceptionMessage() with a negative code.

Returns 0 if every check passes.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <string.h>
#include "gxsocket.h"

#if defined (__UNIX__) && defined (__USE_GX_SSL_EXTENSIONS__) && \
  !defined (__USE_GX_SCHANNEL__)
#include "gxssl.h"
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include "testcerts.h"

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
    snprintf(sbuf, sizeof(sbuf), "%s (killed by signal %d)",
	     name, WTERMSIG(status));
    check(0, sbuf);
    return;
  }
  check(WIFEXITED(status) && WEXITSTATUS(status) == 0, name);
}

static int listen_local(gxSocket &server)
// Listen on 127.0.0.1 and a free port. Returns the port or -1.
{
  if(server.InitSocket(SOCK_STREAM, 0, (char *)"127.0.0.1") < 0) return -1;
  if(server.Bind() < 0) return -1;
  if(server.Listen(4) < 0) return -1;
  if(server.GetSockName() < 0) return -1;
  return server.GetPortNumber();
}

static int connect_local(gxSocket &client, int port)
{
  if(client.InitSocket(SOCK_STREAM, port, (char *)"127.0.0.1") < 0) return -1;
  return client.Connect();
}

enum ServerAction { SEND_AND_WAIT, WAIT, CLOSE_AT_ONCE };

static pid_t fork_tls_server(gxSocket &listener, ServerAction action)
// Accept one TLS client in a child process and do the action
{
  pid_t pid = fork();
  if(pid != 0) return pid;
  alarm(30);
  gxsSocket_t s = listener.Accept();
  if(s < 0) _exit(2);
  gxSSL tls;
  tls.MakeServer();
  if(!tls.InitCTX(gxTestCertPath("server.pem"),
		  gxTestCertPath("server.key"))) _exit(2);
  if(tls.OpenSSLSocket(s) != 0) _exit(2);
  if(tls.AcceptSSLSocket() != 0) _exit(2);
  if(action == SEND_AND_WAIT) {
    tls.SendSSLSocket("abcdef", 6); // One TLS record
    sleep(30);
  }
  else if(action == WAIT) {
    sleep(30); // Never reads, never answers close_notify
  }
  _exit(0); // CLOSE_AT_ONCE: the socket closes with the process
}

static int client_connect(gxSocket &client, gxSSL &tls, int port)
{
  if(connect_local(client, port) < 0) return -1;
  tls.MakeClient();
  tls.SetCAList(gxTestCertPath("ca.pem"));
  if(!tls.InitCTX()) return -1;
  if(tls.OpenSSLSocket(client.GetSocket()) != 0) return -1;
  return tls.ConnectSSLSocket();
}

static int test_hs_client()
{
  gxSocket server;
  int port = listen_local(server);
  if(port < 0) return 2;
  gxSocket client; // The kernel completes TCP, nobody answers TLS
  if(connect_local(client, port) < 0) return 2;
  gxSSL tls;
  if(tls.HandshakeTimeout() != gxSSL_DEF_HANDSHAKE_TIMEOUT) return 1;
  tls.MakeClient();
  if(!tls.InitCTX()) return 2;
  if(tls.OpenSSLSocket(client.GetSocket()) != 0) return 2;
  tls.SetHandshakeTimeout(2);
  time_t start = time(0);
  int rv = tls.ConnectSSLSocket();
  time_t took = time(0) - start;
  if(rv == 0 || took > 8) {
    printf("  ConnectSSLSocket() returned %d after %d seconds\n",
	   rv, (int)took);
    return 1;
  }
  return 0;
}

static int test_hs_server()
{
  gxSocket server;
  int port = listen_local(server);
  if(port < 0) return 2;
  gxSocket client;
  if(connect_local(client, port) < 0) return 2; // Says nothing
  gxsSocket_t s = server.Accept();
  if(s < 0) return 2;
  gxSSL tls;
  tls.MakeServer();
  if(!tls.InitCTX(gxTestCertPath("server.pem"),
		  gxTestCertPath("server.key"))) return 2;
  if(tls.OpenSSLSocket(s) != 0) return 2;
  tls.SetHandshakeTimeout(2);
  time_t start = time(0);
  int rv = tls.AcceptSSLSocket();
  time_t took = time(0) - start;
  if(rv == 0 || took > 8) {
    printf("  AcceptSSLSocket() returned %d after %d seconds\n",
	   rv, (int)took);
    return 1;
  }
  return 0;
}

static int test_close()
{
  gxSocket server;
  int port = listen_local(server);
  if(port < 0) return 2;
  pid_t pid = fork_tls_server(server, WAIT);
  gxSocket client;
  gxSSL tls;
  int rc = 0;
  if(client_connect(client, tls, port) != 0) {
    rc = 2;
  }
  else {
    time_t start = time(0);
    tls.CloseSSLSocket();
    time_t took = time(0) - start;
    if(took > 3) {
      printf("  CloseSSLSocket() took %d seconds\n", (int)took);
      rc = 1;
    }
  }
  kill(pid, SIGKILL);
  waitpid(pid, 0, 0);
  return rc;
}

static int test_pending()
{
  gxSocket server;
  int port = listen_local(server);
  if(port < 0) return 2;
  pid_t pid = fork_tls_server(server, SEND_AND_WAIT);
  gxSocket client;
  gxSSL tls;
  int rc = 0;
  if(client_connect(client, tls, port) != 0) {
    rc = 2;
  }
  else {
    char buf[8];
    memset(buf, 0, sizeof(buf));
    int n1 = tls.RecvSSLSocket(buf, 3, 3, 0);
    int n2 = tls.RecvSSLSocket(buf + 3, 3, 3, 0);
    if(n1 != 3 || n2 != 3 || memcmp(buf, "abcdef", 6) != 0) {
      printf("  RecvSSLSocket() returned %d and %d (%s)\n", n1, n2, buf);
      rc = 1;
    }
  }
  kill(pid, SIGKILL);
  waitpid(pid, 0, 0);
  return rc;
}

static int test_verify()
{
  int rc = 0;
  {
    gxSSL none; // No connection: an error, not a crash
    if(none.VerifyCert("localhost") == 0) rc = 1;
  }
  gxSocket server;
  int port = listen_local(server);
  if(port < 0) return 2;
  pid_t pid = fork_tls_server(server, WAIT);
  gxSocket client;
  gxSSL tls;
  if(client_connect(client, tls, port) != 0) {
    rc = 2;
  }
  else {
    long code = -1;
    if(tls.VerifyCert("localhost", &code) != 0 || code != 0) {
      printf("  localhost (alternative name) did not verify: %s\n",
	     tls.SSLExceptionMessage());
      rc = 1;
    }
    if(tls.VerifyCert("LocalHost") != 0) {
      printf("  Host names are not case sensitive\n");
      rc = 1;
    }
    if(tls.VerifyCert("127.0.0.1") != 0) {
      printf("  127.0.0.1 (IP alternative name) did not verify\n");
      rc = 1;
    }
    if(tls.VerifyCert("other.example") != gxSSL_PEERCN_ERROR) {
      printf("  other.example verified\n");
      rc = 1;
    }
    if(tls.VerifyCert("127.0.0.2") != gxSSL_PEERCN_ERROR) {
      printf("  127.0.0.2 verified\n");
      rc = 1;
    }
  }
  kill(pid, SIGKILL);
  waitpid(pid, 0, 0);
  return rc;
}

static int test_sigpipe()
{
  gxSocket server;
  int port = listen_local(server);
  if(port < 0) return 2;
  pid_t pid = fork_tls_server(server, CLOSE_AT_ONCE);
  gxSocket client;
  gxSSL tls;
  if(client_connect(client, tls, port) != 0) return 2;
  waitpid(pid, 0, 0); // The server has gone
  static char buf[65536];
  memset(buf, 'x', sizeof(buf));
  int rv = 0;
  for(int i = 0; i < 20 && rv >= 0; i++) {
    rv = tls.SendSSLSocket(buf, (int)sizeof(buf));
  }
  return rv < 0 ? 0 : 1;
}

static int test_message()
{
  gxSSL tls;
  tls.SetSSLError((gxSSLError)-3);
  const char *msg = tls.SSLExceptionMessage();
  if(!msg || strstr(msg, "Invalid") == 0) return 1;
  return 0;
}

int main()
{
  if(MakeTestCerts() != 0) {
    printf("FAIL could not make the test certificates with openssl\n");
    RemoveTestCerts();
    return 1;
  }
  run("hs_client: ConnectSSLSocket() handshake timeout", test_hs_client);
  run("hs_server: AcceptSSLSocket() handshake timeout", test_hs_server);
  run("close: CloseSSLSocket() does not wait for the peer", test_close);
  run("pending: RecvSSLSocket() returns buffered data", test_pending);
  run("verify: VerifyCert() alternative names and IP addresses",
      test_verify);
  run("sigpipe: SendSSLSocket() to a closed peer returns -1", test_sigpipe);
  run("message: SSLExceptionMessage() with a negative code", test_message);
  RemoveTestCerts();
  printf("%d failure(s)\n", failures);
  return failures ? 1 : 0;
}

#else // Not UNIX with OpenSSL
int main()
{
  printf("SKIP: the gxSSL security tests need UNIX and OpenSSL\n");
  return 0;
}
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
