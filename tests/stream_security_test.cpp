// stream_security_test.cpp - regression tests for the DataReel 7.3.1 security sweep.
// Prints "PASS <name>" per test and returns non-zero on a failure.
//
// 11/01/2026: gxStream and gxsTLSConnection tests. None of them needs
// a certificate:
//   stream_refuses_plaintext  gxStream will not send or read blocks on
//                             a connection that never ran TLS
//   starttls_fail_closes      a failed handshake closes the socket
//   writeblock_huge_length    WriteBlock() rejects lengths > INT_MAX
//   objectlength_damaged      ObjectLength()/BlockLength() return 0 for
//                             a header shorter than its own overhead
//   send_string_view          Send(std::string_view) of a large string
//   read_too_large            Read() refuses a request over INT_MAX
//   connect_timeout           a TLS Connect() to a server that accepts
//                             TCP and stays silent gives up in time
#include "gxdlcode.h"

#include <chrono>
#include <climits>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

#include "gxdbase.h"
#include "gxstream.h"

#if defined (__UNIX__)
#include <arpa/inet.h>
#include <csignal>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#if defined (__WIN32__)
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

using namespace datareel;

static int failures = 0;

static void result(const char *name, bool ok, const char *why = "")
{
  if(ok) printf("PASS %s\n", name);
  else { printf("FAIL %s %s\n", name, why); failures++; }
}

static void close_raw(gxsSocket_t s)
{
#if defined (__WIN32__)
  closesocket(s);
#else
  close(s);
#endif
}

// A loopback TCP listener on 127.0.0.1 with an OS chosen port.
// Returns the socket or -1 (INVALID_SOCKET).
static gxsSocket_t listen_loopback(int &port)
{
  gxSocket::InitSocketLibrary();
  gxsSocket_t l = socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in sa;
  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  sa.sin_port = 0;
  if(bind(l, (sockaddr *)&sa, sizeof(sa)) != 0 || listen(l, 4) != 0) {
    close_raw(l);
    return (gxsSocket_t)-1;
  }
  socklen_t len = sizeof(sa);
  getsockname(l, (sockaddr *)&sa, &len);
  port = ntohs(sa.sin_port);
  return l;
}

// A connected pair of stream sockets: socketpair() on POSIX, a
// loopback TCP connection on Windows. Returns 0 if successful.
static int socket_pair(gxsSocket_t sv[2])
{
#if defined (__UNIX__)
  int fds[2];
  if(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) return -1;
  sv[0] = fds[0]; sv[1] = fds[1];
  return 0;
#else
  int port = 0;
  gxsSocket_t l = listen_loopback(port);
  if(l == (gxsSocket_t)-1) return -1;
  sv[0] = socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in sa;
  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  sa.sin_port = htons((unsigned short)port);
  if(connect(sv[0], (sockaddr *)&sa, sizeof(sa)) != 0) {
    close_raw(sv[0]); close_raw(l);
    return -1;
  }
  sv[1] = accept(l, 0, 0);
  close_raw(l);
  return sv[1] == (gxsSocket_t)-1 ? -1 : 0;
#endif
}

// Bytes waiting on s without blocking (0 if none)
static int pending_bytes(gxsSocket_t s)
{
  if(gxSocket::WaitReadable(s, 100) != 1) return 0;
  char buf[256];
  int n = (int)recv(s, buf, sizeof(buf), 0);
  return n > 0 ? n : 0;
}

static void test_stream_refuses_plaintext()
{
  gxsSocket_t sv[2];
  if(socket_pair(sv) != 0) {
    result("stream_refuses_plaintext", false, "(no socket pair)");
    return;
  }
  gxStream stream;
  stream.Connection().Socket() = gxSocket(sv[0]); // Open, never upgraded
  bool ok = stream.Connection().IsOpen() && !stream.Connection().IsSecure();

  gxBlockHeader hdr;
  stream.SetBlockStatus(hdr, gxSendBlock);
  ok = ok && stream.WriteHeader(hdr) == gxSOCKET_SSL_ERROR;
  ok = ok && stream.WriteBlock("secret", 6) == gxSOCKET_SSL_ERROR;
  ok = ok && stream.WriteResultBlock((FAU_t)1234) == gxSOCKET_BLOCKACK_ERROR;
  ok = ok && stream.CloseConnection() == gxSOCKET_SSL_ERROR;
  ok = ok && pending_bytes(sv[1]) == 0; // Nothing went out in cleartext

  // A peer writing a valid header in cleartext must not be read
  gxBlockHeader peer;
  stream.SetBlockStatus(peer, gxSendBlock);
  peer.block_length = 4;
  send(sv[1], (const char *)&peer, (int)sizeof(peer), 0);
  send(sv[1], "data", 4, 0);
  gxBlockHeader in;
  ok = ok && stream.ReadHeader(in) == gxSOCKET_SSL_ERROR;
  std::vector<unsigned char> buf;
  ok = ok && stream.ReadBlock(buf, peer) == gxSOCKET_SSL_ERROR && buf.empty();
  int code = 0;
  ok = ok && stream.ReadResultBlock(code) == gxSOCKET_BLOCKACK_ERROR;
  stream.Close();
  close_raw(sv[1]);
  result("stream_refuses_plaintext", ok);
}

static void test_starttls_fail_closes()
{
  gxsSocket_t sv[2];
  if(socket_pair(sv) != 0) {
    result("starttls_fail_closes", false, "(no socket pair)");
    return;
  }
  // The "server" answers the ClientHello with plain text and hangs up
  const char *junk = "220 not a TLS server\r\n";
  send(sv[1], junk, (int)strlen(junk), 0);
  close_raw(sv[1]);

  gxsTLSConnection conn;
  conn.SetTimeout(5);
  conn.SetVerifyPeer(false);
  conn.Socket() = gxSocket(sv[0]);
  bool ok = conn.StartTLS() != 0;
  ok = ok && !conn.IsSecure() && !conn.IsOpen();
  ok = ok && conn.Send("cleartext", 9) == -1; // No fallback to plain TCP
  result("starttls_fail_closes", ok);
}

static void test_writeblock_huge_length()
{
  gxsSocket_t sv[2];
  if(socket_pair(sv) != 0) {
    result("writeblock_huge_length", false, "(no socket pair)");
    return;
  }
  gxStream stream;
  stream.Connection().Socket() = gxSocket(sv[0]);
  char byte = 'x';
  gxBlockHeader hdr;
  stream.SetBlockStatus(hdr, gxSendBlock);
  hdr.block_length = (gxUINT32)0x80000000u; // INT_MAX + 1
  bool ok = stream.WriteBlock(&byte, hdr) == gxSOCKET_BLOCKSIZE_ERROR;
  hdr.block_length = (gxUINT32)0xFFFFFFFFu;
  ok = ok && stream.WriteBlock(&byte, hdr) == gxSOCKET_BLOCKSIZE_ERROR;
  ok = ok && stream.WriteBlock(&byte, -1) == gxSOCKET_BLOCKSIZE_ERROR;
  hdr.block_length = 16;
  ok = ok && stream.WriteBlock(0, hdr) == gxSOCKET_BLOCKSIZE_ERROR;
  ok = ok && pending_bytes(sv[1]) == 0;
  stream.Close();
  close_raw(sv[1]);
  result("writeblock_huge_length", ok);
}

static void test_objectlength_damaged()
{
  const char *fname = "stream_security_test.gxd";
  gxDatabase db;
  bool ok = db.Create(fname) == gxDBASE_NO_ERROR;
  const char rec[] = "a record of some length";
  FAU_t addr = ok ? db.Alloc((__ULWORD__)sizeof(rec)) : (FAU_t)0;
  ok = ok && addr != (FAU_t)0 &&
    db.Write(rec, (__ULWORD__)sizeof(rec), addr, 1) == gxDBASE_NO_ERROR;
  ok = ok && db.ObjectLength(addr) >= (__ULWORD__)sizeof(rec);

  // Damage the length: shorter than the block header itself
  FAU_t blk = addr - (FAU_t)db.BlockHeaderSize();
  gxBlockHeader hdr;
  ok = ok && db.ReadBlockHdr(hdr, blk) == gxDBASE_NO_ERROR;
  for(gxUINT32 bad : { (gxUINT32)0, (gxUINT32)1,
	(gxUINT32)(db.BlockHeaderSize() - 1) }) {
    hdr.block_length = bad;
    ok = ok && db.WriteBlockHdr(hdr, blk) == gxDBASE_NO_ERROR;
    ok = ok && db.ObjectLength(addr) == 0 && db.BlockLength(addr) == 0;
  }
  db.Close();
  remove(fname);
  result("objectlength_damaged", ok);
}

static void test_send_string_view()
{
  // Larger than one gxsTLS_SEND_CHUNK, so Send() loops
  gxsSocket_t sv[2];
  if(socket_pair(sv) != 0) {
    result("send_string_view", false, "(no socket pair)");
    return;
  }
  std::string big((std::size_t)gxsTLS_SEND_CHUNK * 2 + 123, 'z');
  std::size_t got = 0;
  std::thread reader([&]() {
    char buf[65536];
    for(;;) {
      int n = (int)recv(sv[1], buf, sizeof(buf), 0);
      if(n <= 0) break;
      got += (std::size_t)n;
    }
  });
  gxsTLSConnection conn;
  conn.Socket() = gxSocket(sv[0]);
  int rv = conn.Send(std::string_view(big));
  conn.Close();
  reader.join();
  close_raw(sv[1]);
  result("send_string_view", rv == (int)big.size() && got == big.size());
}

static void test_read_too_large()
{
  gxsSocket_t sv[2];
  if(socket_pair(sv) != 0) {
    result("read_too_large", false, "(no socket pair)");
    return;
  }
  gxsTLSConnection conn;
  conn.SetTimeout(2);
  conn.Socket() = gxSocket(sv[0]);
  std::string s;
  bool ok = conn.Read(s, (std::size_t)INT_MAX + 1) == -1 && s.empty();
  conn.Close();
  close_raw(sv[1]);
  result("read_too_large", ok);
}

static void test_connect_timeout()
{
  // The kernel completes the TCP handshake from the listen backlog,
  // but nobody ever answers the ClientHello
  int port = 0;
  gxsSocket_t l = listen_loopback(port);
  if(l == (gxsSocket_t)-1) {
    result("connect_timeout", false, "(no listener)");
    return;
  }
  gxsTLSConnection conn;
  conn.SetTimeout(2);
  conn.SetVerifyPeer(false);
  auto t0 = std::chrono::steady_clock::now();
  int rv = conn.Connect("127.0.0.1", port, true);
  double secs = std::chrono::duration<double>(
    std::chrono::steady_clock::now() - t0).count();
  bool ok = rv != 0 && !conn.IsOpen() && !conn.IsSecure() && secs < 6.0;
  close_raw(l);
  char why[64];
  snprintf(why, sizeof(why), "(rv %d after %.1f s)", rv, secs);
  result("connect_timeout", ok, why);
}

int main()
{
#if defined (__UNIX__)
  // A peer that hangs up during the handshake must give an error, not
  // SIGPIPE (OpenSSL writes to the socket with plain send())
  signal(SIGPIPE, SIG_IGN);
#endif
  setvbuf(stdout, 0, _IONBF, 0);
  test_stream_refuses_plaintext();
  test_starttls_fail_closes();
  test_writeblock_huge_length();
  test_objectlength_damaged();
  test_send_string_view();
  test_read_too_large();
  test_connect_timeout();
  if(failures) printf("%d test(s) failed\n", failures);
  return failures ? 1 : 0;
}
