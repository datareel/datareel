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
  dgram_request  gxDatagram::RequestBlock() refuses a huge block length
              from the peer before allocating (11/14/2026).
  dgram_sized gxDatagram ReadBlock()/ReadRemoteBlock() with a buffer
              size read nothing if the block does not fit.
  dgram_maxblock The old gxDatagram ReadBlock() refuses lengths over
              MaxBlockSize().
  dgram_negative gxDatagram WriteBlock() with a negative length or a
              null buffer sends nothing.
  plain       Plain TCP data path between StreamClient() and
              StreamServer() when the server opts out with
              AllowAnonymousClients(true).

Client authentication (mutual TLS, throwaway certificates made with
the openssl program):
  refuse      StreamServer()/Accept() refuse to run without client
              authentication: plain, TLS only, CA file without an
              allow list, allow list without a CA file.
  mtls_fp     Allowed client certificate by SHA-256 fingerprint: data
              path works, the server sees the client's CN.
  mtls_cn     Allowed client certificate by CN, with AcceptClient().
  no_cert     A TLS client without a certificate is refused.
  wrong_cert  A certificate from the client CA that is not in the
              allow list is refused.
  rogue_cert  A self-signed certificate with an allowed CN is refused.
  plain_cli   A plain TCP client to a mutual TLS server is refused.
  open_tls    AllowAnonymousClients(true) with a server certificate: TLS
              without client certificates.
  server_ver  A client refuses a server certificate from another CA or
              for another name, and its socket is closed.
  fail_closed In TLS mode the block functions refuse a socket without
              a TLS connection (nothing is sent in cleartext).

Returns 0 if every check passes.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <string.h>
#include "gxstream.h"
#include "gxdatagm.h"

#if defined (__UNIX__)
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "testcerts.h"

static int failures = 0;

static void check(int ok, const char *what)
{
  printf("%s %s\n", ok ? "PASS" : "FAIL", what);
  fflush(stdout);
  if(!ok) failures++;
}

typedef int (*TestFunc)();

static const char *only_test = 0;

static void run(const char *name, TestFunc f, int max_seconds = 30)
// Run one check in a child process. The child returns 0 if it passed.
{
  if(only_test && strncmp(name, only_test, strlen(only_test)) != 0) return;
  fflush(stdout);
  pid_t pid = fork();
  if(pid == 0) {
    signal(SIGPIPE, SIG_DFL);
    alarm(max_seconds);
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

// 11/14/2026: gxDatagram checks over two UDP sockets on 127.0.0.1
static int udp_socket(gxsSocketAddress &sa)
{
  int s = socket(AF_INET, SOCK_DGRAM, 0);
  if(s < 0) return -1;
  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  sa.sin_port = 0;
  if(bind(s, (struct sockaddr *)&sa, sizeof(sa)) != 0) { close(s); return -1; }
  socklen_t len = sizeof(sa);
  if(getsockname(s, (struct sockaddr *)&sa, &len) != 0) { close(s); return -1; }
  return s;
}

static int test_dgram_request()
{
  gxsSocketAddress a, b;
  int sa = udp_socket(a), sb = udp_socket(b);
  if(sa < 0 || sb < 0) return 2;
  gxDatagram peer, client;
  // The peer's reply header claims a huge block, queued before the request
  gxBlockHeader reply;
  __ULWORD__ huge = (__ULWORD__)0x7ffffff0;
  if(sizeof(__ULWORD__) >= 8) huge = (__ULWORD__)(((__ULWORD__)1) << 46);
  peer.SetBlockStatus(reply, gxSendBlock);
  reply.block_length = huge;
  if(peer.WriteHeader(sb, &a, reply) != 0) return 2;
  gxBlockHeader req, got;
  client.SetBlockStatus(req, gxRequestBlock);
  req.block_length = 1;
  gxsSocketAddress to = b;
  void *p = client.RequestBlock(sa, &to, "k", req, got);
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

static int test_dgram_sized()
{
  gxsSocketAddress a, b;
  int sa = udp_socket(a), sb = udp_socket(b);
  if(sa < 0 || sb < 0) return 2;
  gxDatagram peer, server;
  char data[100];
  memset(data, 'A', sizeof(data));
  if(peer.WriteBlock(sb, &a, data, (int)sizeof(data)) != 0) return 2;
  gxsSocketAddress from;
  gxBlockHeader gx;
  if(server.ReadHeader(sa, &from, gx) != 0) return 2;
  char buf[20];
  memset(buf, 'z', sizeof(buf));
  if(server.ReadBlock(sa, &from, buf, 10, gx) != gxSOCKET_BLOCKSIZE_ERROR) {
    return 1;
  }
  for(int i = 0; i < (int)sizeof(buf); i++) if(buf[i] != 'z') return 1;
  // A block that fits is read (the data datagram is still queued)
  char big[100];
  if(server.ReadBlock(sa, &from, big, (int)sizeof(big), gx) != 0) return 1;
  if(memcmp(big, data, sizeof(big)) != 0) return 1;
  return 0;
}

static int test_dgram_maxblock()
{
  gxsSocketAddress a, b;
  int sa = udp_socket(a), sb = udp_socket(b);
  if(sa < 0 || sb < 0) return 2;
  gxDatagram peer, server;
  if(server.MaxBlockSize() != gxDATAGRAM_DEF_MAX_BLOCK) return 1;
  server.SetMaxBlockSize(16);
  char data[100];
  memset(data, 'A', sizeof(data));
  if(peer.WriteBlock(sb, &a, data, (int)sizeof(data)) != 0) return 2;
  gxsSocketAddress from;
  gxBlockHeader gx;
  if(server.ReadHeader(sa, &from, gx) != 0) return 2;
  char buf[100];
  if(server.ReadBlock(sa, &from, buf, gx) != gxSOCKET_BLOCKSIZE_ERROR) {
    return 1;
  }
  return 0;
}

static int test_dgram_negative()
{
  gxsSocketAddress a, b;
  int sa = udp_socket(a), sb = udp_socket(b);
  if(sa < 0 || sb < 0) return 2;
  gxDatagram st;
  char data[16];
  memset(data, 0, sizeof(data));
  if(st.WriteBlock(sb, &a, data, -1) == 0) return 1;
  gxBlockHeader gx;
  st.SetBlockStatus(gx, gxSendBlock);
  gx.block_length = 10;
  if(st.WriteBlock(sb, &a, 0, gx) == 0) return 1;
  struct pollfd pfd;
  pfd.fd = sa; pfd.events = POLLIN; pfd.revents = 0;
  if(poll(&pfd, 1, 200) != 0) {
    printf("  a refused WriteBlock() sent data\n");
    return 1;
  }
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
  // A server that is not set up for TLS client certificates must opt
  // out of client authentication
  if(!server.IsTLS()) server.AllowAnonymousClients(true);
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

// ---------------- Client authentication ----------------
#if defined (__USE_GX_SSL_EXTENSIONS__)
static const char *CP(const char *f) { return gxTestCertPath(f); }

static void mtls_server_config(gxStream &server)
{
  server.SetServerCert(CP("server.pem"), CP("server.key"));
  server.SetClientCAFile(CP("ca.pem"));
}

static int serve_one(gxStream &server, int use_accept_client, 
		     const char *expect_cn)
// Server child: accept one client and answer one block. Returns 0 if
// the client was served, 10 if it was refused.
{
  gxStream conn_obj;
  gxStream *conn = &server;
  if(use_accept_client) {
    if(server.AcceptClient(conn_obj) != 0) {
      printf("  server refused the client: %s\n", server.TLSError());
      return 10;
    }
    conn = &conn_obj;
  }
  else if(server.Accept() < 0) {
    printf("  server refused the client: %s\n", server.TLSError());
    return 10;
  }
  if(expect_cn && strcmp(conn->PeerCN(), expect_cn) != 0) {
    printf("  client CN \"%s\", expected %s\n", conn->PeerCN(), expect_cn);
    return 11;
  }
  if(expect_cn && !conn->ClientAuthenticated()) return 11;
  if(!conn->IsRemoteSecure()) return 11;
  gxBlockHeader gx;
  if(conn->ReadClientHeader(gx) != 0) return 12;
  char buf[256];
  if(conn->ReadRemoteBlock(buf, (int)sizeof(buf), gx) != 0) return 13;
  if(conn->WriteRemoteResultBlock((int)gx.block_length) != 0) return 14;
  conn->CloseRemoteSocket();
  return 0;
}

static int client_roundtrip(gxStream &client, int port, const char *host)
// Returns 0 if the block went through, 1 if not
{
  if(client.StreamClient(port, (char *)host) != 0) {
    printf("  client: %s %s\n", client.SocketExceptionMessage(), 
	   client.TLSError());
    return 1;
  }
  char msg[] = "mutual TLS data path";
  int result = -1;
  if(client.WriteBlock(msg, (int)sizeof(msg)) != 0) return 1;
  if(client.ReadResultBlock(result) != 0) return 1;
  if(result != (int)sizeof(msg)) return 1;
  client.Close();
  return 0;
}

static int mtls_case(gxStream &server, gxStream &client, int expect_ok,
		     int use_accept_client = 0, const char *expect_cn = 0)
// Run one client against one server. Returns 0 if the outcome is the
// expected one on both sides.
{
  int port = start_server(server);
  if(port < 0) {
    printf("  StreamServer(): %s\n", server.TLSError());
    return 2;
  }
  pid_t pid = fork();
  if(pid == 0) {
    int rv = serve_one(server, use_accept_client, expect_cn);
    fflush(stdout);
    _exit(rv);
  }
  server.Close();
  int crc = client_roundtrip(client, port, "localhost");
  int status = 0;
  waitpid(pid, &status, 0);
  int src = WIFEXITED(status) ? WEXITSTATUS(status) : 99;
  if(expect_ok) {
    if(crc != 0 || src != 0) {
      printf("  expected success: client %d, server %d\n", crc, src);
      return 1;
    }
  }
  else {
    if(crc == 0 || src != 10) {
      printf("  expected the client to be refused: client %d, server %d\n",
	     crc, src);
      return 1;
    }
  }
  return 0;
}

static int test_refuse()
{
  int rc = 0;
  {
    gxStream s; // 6.2 style plain server
    if(s.StreamServer(0) == 0) { printf("  plain server started\n"); rc = 1; }
    else if(s.GetSocketError() != gxSOCKET_SSL_ERROR || !*s.TLSError()) rc = 1;
    if(s.Accept() != -1) rc = 1;
  }
  {
    gxStream s; // TLS without client certificates
    s.SetServerCert(CP("server.pem"), CP("server.key"));
    if(s.StreamServer(0) == 0) { printf("  TLS only server started\n"); rc = 1; }
  }
  {
    gxStream s; // CA file without an allow list
    mtls_server_config(s);
    if(s.StreamServer(0) == 0) { printf("  CA only server started\n"); rc = 1; }
  }
  {
    gxStream s; // Allow list without a CA file
    s.SetServerCert(CP("server.pem"), CP("server.key"));
    s.AllowClientCN("client1");
    if(s.StreamServer(0) == 0) { printf("  allow list only started\n"); rc = 1; }
  }
  {
    gxStream s; // Malformed allow list entries
    if(s.AllowClientFingerprint("12:34") != -1) rc = 1;
    if(s.AllowClientCN("") != -1) rc = 1;
  }
  return rc;
}

static int test_mtls_fp()
{
  char fp[256];
  if(ReadTestFile("client1.fp", fp, (int)sizeof(fp)) != 0) return 2;
  gxStream server, client;
  mtls_server_config(server);
  if(server.AllowClientFingerprint(fp) != 0) return 2;
  client.SetClientCert(CP("client1.pem"), CP("client1.key"));
  client.SetServerCAFile(CP("ca.pem"));
  return mtls_case(server, client, 1, 0, "client1");
}

static int test_mtls_cn()
{
  gxStream server, client;
  mtls_server_config(server);
  server.AllowClientCN("client2");
  client.SetClientCert(CP("client2.pem"), CP("client2.key"));
  client.SetServerCAFile(CP("ca.pem"));
  return mtls_case(server, client, 1, 1, "client2");
}

static int test_no_cert()
{
  gxStream server, client;
  mtls_server_config(server);
  server.AllowClientCN("client1");
  client.UseTLS(1);
  client.SetServerCAFile(CP("ca.pem"));
  return mtls_case(server, client, 0);
}

static int test_wrong_cert()
{
  char fp[256];
  if(ReadTestFile("client1.fp", fp, (int)sizeof(fp)) != 0) return 2;
  gxStream server, client;
  mtls_server_config(server);
  server.AllowClientFingerprint(fp);
  client.SetClientCert(CP("client2.pem"), CP("client2.key"));
  client.SetServerCAFile(CP("ca.pem"));
  return mtls_case(server, client, 0);
}

static int test_rogue_cert()
{
  gxStream server, client;
  mtls_server_config(server);
  server.AllowClientCN("client1");
  client.SetClientCert(CP("rogue.pem"), CP("rogue.key"));
  client.SetServerCAFile(CP("ca.pem"));
  return mtls_case(server, client, 0);
}

static int test_plain_cli()
{
  gxStream server, client; // client stays on plain TCP
  mtls_server_config(server);
  server.AllowClientCN("client1");
  server.SetHandshakeTimeout(5);
  client.SetTimeOut(10, 0);
  return mtls_case(server, client, 0);
}

static int test_open_tls()
{
  gxStream server, client;
  server.SetServerCert(CP("server.pem"), CP("server.key"));
  server.AllowAnonymousClients(true);
  client.UseTLS(1);
  client.SetServerCAFile(CP("ca.pem"));
  return mtls_case(server, client, 1);
}

static int test_server_ver()
{
  int rc = 0;
  for(int round = 0; round < 2; round++) {
    gxStream server, client;
    server.SetServerCert(CP("server.pem"), CP("server.key"));
    server.AllowAnonymousClients(true);
    int port = start_server(server);
    if(port < 0) return 2;
    pid_t pid = fork();
    if(pid == 0) {
      server.Accept(); // The client gives up after the handshake
      _exit(0);
    }
    server.Close();
    client.UseTLS(1);
    if(round == 0) {
      client.SetServerCAFile(CP("other_ca.pem")); // Not the server's CA
    }
    else {
      client.SetServerCAFile(CP("ca.pem"));
      client.SetServerName("other.example"); // Not the server's name
    }
    if(client.StreamClient(port, (char *)"127.0.0.1") == 0) {
      printf("  round %d: the server certificate was accepted\n", round);
      rc = 1;
    }
    else if(client.GetSocket() != -1 || client.IsSecure()) {
      printf("  round %d: the socket was left open\n", round);
      rc = 1;
    }
    char msg[] = "x";
    if(client.WriteBlock(msg, 1) == 0) rc = 1;
    kill(pid, SIGKILL);
    waitpid(pid, 0, 0);
  }
  return rc;
}

static int test_fail_closed()
{
  int sv[2];
  if(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) return 2;
  gxStream st;
  st.UseTLS(1);
  char data[8];
  memset(data, 'x', sizeof(data));
  if(st.WriteBlock(sv[0], data, (int)sizeof(data)) == 0) return 1;
  if(st.GetSocketError() != gxSOCKET_SSL_ERROR) return 1;
  struct pollfd pfd;
  pfd.fd = sv[1]; pfd.events = POLLIN; pfd.revents = 0;
  if(poll(&pfd, 1, 200) != 0) {
    printf("  data was sent in cleartext\n");
    return 1;
  }
  gxBlockHeader gx;
  if(write(sv[1], data, sizeof(data)) != (ssize_t)sizeof(data)) return 2;
  if(st.ReadHeader(sv[0], gx, 1, 0) == 0) return 1;
  return 0;
}
#endif // __USE_GX_SSL_EXTENSIONS__

int main(int argc, char **argv)
{
  // An argument runs only the checks whose names start with it
  if(argc > 1) only_test = argv[1];
  run("request: RequestBlock() refuses a huge block length", test_request);
  run("sized: ReadBlock()/ReadRemoteBlock() with a buffer size", test_sized);
  run("maxblock: ReadBlock() refuses lengths over MaxBlockSize()",
      test_maxblock);
  run("negative: WriteBlock() with a negative length", test_negative);
  run("result64: 64-bit result blocks", test_result64);
  run("dgram_request: gxDatagram RequestBlock() refuses a huge block length",
      test_dgram_request);
  run("dgram_sized: gxDatagram ReadBlock() with a buffer size",
      test_dgram_sized);
  run("dgram_maxblock: gxDatagram ReadBlock() refuses lengths over "
      "MaxBlockSize()", test_dgram_maxblock);
  run("dgram_negative: gxDatagram WriteBlock() with a negative length",
      test_dgram_negative);
  run("plain: TCP data path (AllowAnonymousClients(true))", test_plain);
#if defined (__USE_GX_SSL_EXTENSIONS__)
  if(MakeTestCerts() != 0) {
    printf("FAIL could not make the test certificates with openssl\n");
    failures++;
  }
  else {
    run("refuse: servers without client authentication do not start",
	test_refuse);
    run("mtls_fp: allowed client certificate (fingerprint)", test_mtls_fp);
    run("mtls_cn: allowed client certificate (CN, AcceptClient())", 
	test_mtls_cn);
    run("no_cert: TLS client without a certificate is refused", 
	test_no_cert);
    run("wrong_cert: certificate not in the allow list is refused", 
	test_wrong_cert);
    run("rogue_cert: self-signed certificate with an allowed CN is refused",
	test_rogue_cert);
    run("plain_cli: plain TCP client to a mutual TLS server is refused",
	test_plain_cli);
    run("open_tls: AllowAnonymousClients(true) TLS server", test_open_tls);
    run("server_ver: client checks the server certificate and name",
	test_server_ver);
    run("fail_closed: TLS mode refuses a socket without TLS", 
	test_fail_closed);
  }
  RemoveTestCerts();
#endif
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
