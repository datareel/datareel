// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: gxstls.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 10/26/2026
// Date Last Modified: 10/26/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- //
// ---------- Include File Description and Details  ---------- //
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

TLS connections used by the DataReel protocol clients and servers.

Changes:
==============================================================
10/26/2026: Added for DataReel 7.

11/01/2026: A failed handshake, certificate check or context setup
now closes the TCP connection, so a caller that ignores the error
cannot send or read in cleartext. Every handshake runs with the
socket read/write timeout set, so a silent peer cannot block
SSL_connect()/SSL_accept() forever. AttachClient() with a session
checks the server certificate unless the TLS session was resumed.
Send(std::string_view) sends in chunks and Read() refuses requests
larger than INT_MAX.

11/01/2026: Mutual TLS for servers. gxsTLSServer::SetClientCAFile()
makes the server ask every client for a certificate that chains to
the client CAs, and AllowClientFingerprint()/AllowClientCN() limit it
to known clients. Accept() checks the certificate after the handshake
and closes the client if it is missing, does not verify or is not in
the allow list (fail closed). With SetRequireClientAuth(true), which
gxStreamServer sets, Listen() refuses to start without a client CA
file and an allow list unless AllowAnonymousClients(true) was called.
==============================================================
*/
// ----------------------------------------------------------- //   
#include "gxdlcode.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <cstring>
#include <mutex>
#include "gxstls.h"

#if defined (__UNIX__)
#include <signal.h>
#include <sys/socket.h>
#endif

namespace datareel {


static void gxs_ignore_sigpipe()
// 11/01/2026: OpenSSL writes to the socket with write(), not with
// gxSocket's MSG_NOSIGNAL sends, so a peer that resets the connection
// (for example a client that drops right after a failed certificate
// check, now that failed handshakes close at once) raised SIGPIPE and
// killed the whole server. Ignore SIGPIPE, but only if the program
// left it at the default action; the write then fails with EPIPE.
{
#if defined (__UNIX__)
  static std::once_flag once;
  std::call_once(once, []() {
    struct sigaction old_sa;
    if(sigaction(SIGPIPE, 0, &old_sa) == 0 && old_sa.sa_handler == SIG_DFL) {
      struct sigaction sa;
      memset(&sa, 0, sizeof(sa));
      sa.sa_handler = SIG_IGN;
      sigemptyset(&sa.sa_mask);
      sigaction(SIGPIPE, &sa, 0);
    }
  });
#endif
}


int gxsTLSConnection::fail(const std::string &msg)
{
  error = msg;
  return -1;
}

int gxsTLSConnection::fail_closed(const std::string &msg)
// 11/01/2026: Used when TLS could not be set up. Closes the TCP
// connection too, so nothing can fall back to cleartext.
{
  error = msg;
  secure = false;
  tls.DestroySSL();
  sock.Close();
  rbuf.clear();
  return -1;
}

int gxsTLSConnection::Connect(const std::string &host, int port, 
			      bool implicit_tls)
{
  Close();
  error.clear();
  host_name = host;
  if(sock.Connect(host, port, timeout_sec) != 0) {
    std::string msg = sock.SocketExceptionMessage();
    return fail(msg + ": " + host + " port " + std::to_string(port));
  }
  // 11/01/2026: Bound every blocking send and receive, including the
  // ones inside the TLS library
  sock.SetIOTimeout(timeout_sec);
  if(implicit_tls) return StartTLS();
  return 0;
}

int gxsTLSConnection::StartTLS()
{
  if(!sock.IsOpen()) return fail("TLS exception: The connection is not open");
  if(secure) return 0;
  rbuf.clear(); // Nothing sent before the handshake may be used after it
  tls.MakeClient();
  tls.SetSSLProto(protocol);
  if(!ca_file.empty()) tls.SetCAList(ca_file.c_str());
  if(!client_cert.empty()) {
    tls.SetCertFile(client_cert.c_str());
    tls.SetKeyFile(client_key.c_str());
    if(!key_pass.empty()) tls.SetPasswd(key_pass.c_str());
  }
  // 11/01/2026: Close the connection if the context cannot be made
  if(!tls.InitCTX()) return fail_closed(tls.SSLExceptionMessage());
  return tls_handshake(false);
}

int gxsTLSConnection::tls_handshake(bool as_server)
// 11/01/2026: Every failure closes the TCP connection (fail_closed)
{
  // 11/01/2026: A peer that accepts TCP and then says nothing must
  // not block the handshake forever
  sock.SetIOTimeout(timeout_sec);
  gxs_ignore_sigpipe(); // 11/01/2026
  if(tls.OpenSSLSocket(sock.GetSocket()) != gxSSL_NO_ERROR) {
    return fail_closed(tls.SSLExceptionMessage());
  }
  if(as_server) {
    if(tls.AcceptSSLSocket() != gxSSL_NO_ERROR) {
      return fail_closed(tls.SSLExceptionMessage());
    }
  }
  else {
    tls.SetHostName(host_name.c_str());
    if(tls.ConnectSSLSocket() != gxSSL_NO_ERROR) {
      return fail_closed(tls.SSLExceptionMessage());
    }
    if(verify_peer && verify_server() != 0) {
      tls.CloseSSLSocket();
      return fail_closed(error);
    }
  }
  secure = true;
  return 0;
}

int gxsTLSConnection::verify_server()
// 11/01/2026: Check the server certificate and host name. Sets the
// error message and returns -1 if the check fails.
{
  long code = 0;
  if(tls.VerifyCert(host_name.c_str(), &code) != gxSSL_NO_ERROR) {
    std::string msg = tls.SSLExceptionMessage();
    if(code != 0) {
      msg += " (";
      msg += gxSSL::CertVerifyErrorString(code);
      msg += ")";
    }
    msg += " for " + host_name;
    return fail(msg);
  }
  return 0;
}

int gxsTLSConnection::AttachClient(gxSocket &&s, const std::string &host,
				   gxsTLSConnection *session_from)
{
  Close();
  error.clear();
  sock = std::move(s);
  host_name = host;
  if(!session_from || !session_from->secure) return StartTLS();

  // Share the context and TLS session of the other connection
  tls.MakeClient();
  tls.SetCTX(session_from->tls.GetCTX());
  sock.SetIOTimeout(timeout_sec); // 11/01/2026: Bound the handshake
  gxs_ignore_sigpipe(); // 11/01/2026
  if(tls.OpenSSLSocket(sock.GetSocket()) != gxSSL_NO_ERROR) {
    return fail_closed(tls.SSLExceptionMessage());
  }
#if !defined (__USE_GX_SCHANNEL__)
  SSL_SESSION *sess = SSL_get1_session(session_from->tls.GetSSL());
  if(sess) {
    SSL_set_session(tls.GetSSL(), sess);
    SSL_SESSION_free(sess);
  }
#endif
  tls.SetHostName(host_name.c_str());
  if(tls.ConnectSSLSocket() != gxSSL_NO_ERROR) {
    return fail_closed(tls.SSLExceptionMessage());
  }
  // 11/01/2026: Only a resumed session proves this is the server the
  // control connection checked. A full handshake (the server did not
  // resume, or Schannel, which does not resume here) gets the same
  // certificate and host name check as any other connection.
  if(verify_peer) {
#if !defined (__USE_GX_SCHANNEL__)
    bool reused = SSL_session_reused(tls.GetSSL()) == 1;
#else
    bool reused = false;
#endif
    if(!reused && verify_server() != 0) {
      tls.CloseSSLSocket();
      return fail_closed(error);
    }
  }
  secure = true;
  return 0;
}

int gxsTLSConnection::AttachServer(gxSocket &&client, gxSSL_CTX_t *server_ctx)
{
  Close();
  error.clear();
  sock = std::move(client);
  host_name = sock.PeerIPAddress();
  // tls_handshake() sets the socket timeout before SSL_accept()
  tls.MakeServer();
  tls.SetCTX(server_ctx);
  return tls_handshake(true);
}

static void gxs_linger_close(gxsSocket_t s)
// 11/01/2026: CloseSSLSocket() no longer waits for the peer's
// close_notify. Closing a socket with unread data in it (TLS 1.3
// session tickets, the peer's close_notify) sends a TCP reset, and a
// reset makes the peer's kernel drop data it has not read yet, such
// as the end of an FTPS upload. Send a FIN, then read and discard
// what the peer sends until it closes, for at most about one second,
// so a silent peer cannot hold up Close().
{
#if defined (__WIN32__)
  shutdown(s, SD_SEND);
#else
  shutdown(s, SHUT_WR);
#endif
  char buf[4096];
  auto end = std::chrono::steady_clock::now() + std::chrono::seconds(1);
  std::size_t total = 0;
  while(total < 1024*1024) { // Stop a peer that floods us too
    auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
      end - std::chrono::steady_clock::now()).count();
    if(left <= 0) break;
    if(gxSocket::WaitReadable(s, (int)left) != 1) break;
    int n = (int)recv(s, buf, (int)sizeof(buf), 0);
    if(n <= 0) break; // The peer closed, or an error
    total += (std::size_t)n;
  }
}

void gxsTLSConnection::Close()
{
  peer_fingerprint.clear(); // 11/01/2026
  peer_cn.clear();
  if(secure) {
    tls.CloseSSLSocket();
    secure = false;
    if(sock.IsOpen()) gxs_linger_close(sock.GetSocket()); // 11/01/2026
  }
  fuzz_plain = false; // 11/01/2026: See gxstls.h
  tls.DestroySSL();
  sock.Close();
  rbuf.clear();
}

int gxsTLSConnection::Send(const void *buf, int bytes)
{
  if(!sock.IsOpen()) { fail("TLS exception: The connection is not open"); return -1; }
  int rv = secure ? tls.SendSSLSocket(buf, bytes) : sock.Send(buf, bytes);
  if(rv != bytes) {
    fail(secure ? tls.SSLExceptionMessage() : sock.SocketExceptionMessage());
    return -1;
  }
  return rv;
}

int gxsTLSConnection::Send(std::string_view s)
// 11/01/2026: Was Send(s.data(), (int)s.size()) in the header, which
// truncated a string larger than INT_MAX. Sends in chunks. Returns
// the number of bytes sent (at most INT_MAX), or -1 on an error.
{
  const char *p = s.data();
  std::size_t left = s.size();
  while(left > 0) {
    int n = left > (std::size_t)gxsTLS_SEND_CHUNK ? 
      gxsTLS_SEND_CHUNK : (int)left;
    if(Send(p, n) != n) return -1;
    p += n;
    left -= (std::size_t)n;
  }
  return s.size() > (std::size_t)INT_MAX ? INT_MAX : (int)s.size();
}

int gxsTLSConnection::SendLine(std::string_view s)
{
  std::string line(s);
  line += "\r\n";
  return Send(line);
}

int gxsTLSConnection::fill()
{
  char buf[16384];
  int rv;
  if(secure) {
    rv = tls.ReadSomeSSLSocket(buf, sizeof(buf), timeout_sec * 1000);
    if(rv < 0) fail(tls.SSLExceptionMessage());
  }
  else {
    rv = sock.RecvSome(buf, sizeof(buf), timeout_sec);
    if(rv < 0) fail(sock.SocketExceptionMessage());
  }
  if(rv > 0) rbuf.append(buf, (std::size_t)rv);
  return rv;
}

int gxsTLSConnection::ReadSome(void *buf, int bytes)
{
  if(bytes <= 0) return 0;
  if(rbuf.empty()) {
    int rv = fill();
    if(rv <= 0) return rv;
  }
  int n = (int)rbuf.size() < bytes ? (int)rbuf.size() : bytes;
  memcpy(buf, rbuf.data(), (std::size_t)n);
  rbuf.erase(0, (std::size_t)n);
  return n;
}

int gxsTLSConnection::Read(std::string &s, std::size_t bytes)
{
  s.clear();
  // 11/01/2026: The count is returned as an int, so a larger request
  // could only be reported truncated. Refuse it.
  if(bytes > (std::size_t)INT_MAX) {
    return fail("TLS exception: Read request is too large");
  }
  while(rbuf.size() < bytes) {
    if(fill() <= 0) break;
  }
  std::size_t n = rbuf.size() < bytes ? rbuf.size() : bytes;
  s.assign(rbuf, 0, n);
  rbuf.erase(0, n);
  return (int)n;
}

bool gxsTLSConnection::ReadLine(std::string &line, std::size_t max_len)
{
  line.clear();
  std::size_t pos;
  while((pos = rbuf.find('\n')) == std::string::npos) {
    if(rbuf.size() > max_len) {
      fail("TLS exception: Line is too long");
      return false;
    }
    if(fill() <= 0) return false;
  }
  if(pos > max_len) {
    fail("TLS exception: Line is too long");
    return false;
  }
  line.assign(rbuf, 0, pos);
  rbuf.erase(0, pos + 1);
  if(!line.empty() && line.back() == '\r') line.pop_back();
  return true;
}

bool gxsTLSConnection::ReadAll(std::string &s, std::size_t max_len)
{
  for(;;) {
    if(rbuf.size() > max_len) {
      fail("TLS exception: Response is too large");
      return false;
    }
    int rv = fill();
    if(rv == 0) break;
    if(rv < 0) return false;
  }
  s.swap(rbuf);
  rbuf.clear();
  return true;
}

static std::string gxs_printable(const std::string &s, std::size_t max_len = 256)
// 11/01/2026: Copy of a string from a certificate that is safe to put
// in an error message or log: control characters, DEL and backslash
// are escaped as \xNN, and long strings are cut.
{
  static const char hex[] = "0123456789ABCDEF";
  std::string out;
  for(std::size_t i = 0; i < s.size() && i < max_len; i++) {
    unsigned char c = (unsigned char)s[i];
    if(c < 0x20 || c == 0x7F || c == '\\') {
      out += "\\x";
      out += hex[(c >> 4) & 0x0F];
      out += hex[c & 0x0F];
    }
    else {
      out += (char)c;
    }
  }
  if(s.size() > max_len) out += "...";
  return out;
}

std::string gxsTLSServer::NormalizeFingerprint(const std::string &s)
// 11/01/2026: Returns the fingerprint as 32 upper case hex bytes
// separated by colons, or an empty string if s is not one
{
  std::string::size_type start = s.rfind('=');
  start = (start == std::string::npos) ? 0 : start + 1;
  std::string digits;
  for(std::size_t i = start; i < s.size(); i++) {
    char c = s[i];
    if(c == ':' || c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
    if(c >= 'a' && c <= 'f') c = (char)(c - 'a' + 'A');
    if(!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) {
      return std::string();
    }
    digits += c;
  }
  if(digits.size() != 64) return std::string();
  std::string out;
  for(std::size_t i = 0; i < digits.size(); i += 2) {
    if(i > 0) out += ':';
    out.append(digits, i, 2);
  }
  return out;
}

int gxsTLSServer::AllowClientFingerprint(const std::string &fp)
// 11/01/2026
{
  std::string n = NormalizeFingerprint(fp);
  if(n.empty()) {
    error = "TLS exception: Not a SHA-256 certificate fingerprint";
    return -1;
  }
  if(std::find(allow_fingerprints.begin(), allow_fingerprints.end(), n) ==
     allow_fingerprints.end()) {
    allow_fingerprints.push_back(n);
  }
  return 0;
}

int gxsTLSServer::AllowClientCN(const std::string &cn)
// 11/01/2026
{
  if(cn.empty()) {
    error = "TLS exception: Empty client certificate name";
    return -1;
  }
  if(std::find(allow_cns.begin(), allow_cns.end(), cn) == allow_cns.end()) {
    allow_cns.push_back(cn);
  }
  return 0;
}

int gxsTLSServer::check_client(gxsTLSConnection &conn, bool set_error)
// 11/01/2026: Called by Accept() after the handshake when client
// certificates are required. Closes the connection and sets the error
// unless the client sent a certificate that verifies against the
// client CAs and is in the allow list. Sets the connection's peer
// identity if it passes. Returns 0 if the client is accepted.
{
  std::string who = conn.Host();
  long code = 0;
  int rv = conn.tls.VerifyClientCert(&code);
  if(rv != gxSSL_NO_ERROR) {
    std::string msg = "TLS exception: Client ";
    msg += who;
    if(rv == gxSSL_PEERCERT_ERROR) {
      msg += " sent no client certificate";
    }
    else {
      msg += " client certificate check failed: ";
      msg += conn.tls.SSLExceptionMessage();
      if(code != 0) {
	msg += " (";
	msg += gxSSL::CertVerifyErrorString(code);
	msg += ")";
      }
    }
    conn.Close();
    conn.error = msg;
    if(set_error) error = msg;
    return -1;
  }
  std::string fp, cn;
  if(conn.tls.GetPeerCertIdentity(fp, cn) != gxSSL_NO_ERROR || fp.empty()) {
    std::string msg = "TLS exception: Client " + who + 
      " certificate fingerprint could not be read";
    conn.Close();
    conn.error = msg;
    if(set_error) error = msg;
    return -1;
  }
  bool allowed = allow_fingerprints.empty() && allow_cns.empty();
  if(!allowed) {
    allowed = std::find(allow_fingerprints.begin(), allow_fingerprints.end(), 
			fp) != allow_fingerprints.end();
  }
  if(!allowed && !cn.empty()) {
    allowed = std::find(allow_cns.begin(), allow_cns.end(), cn) != 
      allow_cns.end();
  }
  if(!allowed) {
    std::string msg = "TLS exception: Client " + who + 
      " certificate is not in the allow list (CN=" + gxs_printable(cn) + 
      ", SHA-256 " + fp + ")";
    conn.Close();
    conn.error = msg;
    if(set_error) error = msg;
    return -1;
  }
  conn.peer_fingerprint = fp;
  conn.peer_cn = cn;
  return 0;
}

int gxsTLSServer::Listen(int port, const std::string &cert_fname, 
			 const std::string &key_fname,
			 const std::string &passphrase,
			 const std::string &bind_address)
{
  error.clear();
  // 11/01/2026: Check the client certificate setup before anything is
  // opened. An allow list cannot be checked without the client CAs.
  bool have_allow = !allow_fingerprints.empty() || !allow_cns.empty();
  if(have_allow && client_ca_file.empty()) {
    error = "TLS exception: A client allow list needs a client CA file "
      "(SetClientCAFile())";
    return -1;
  }
  if(require_client_auth && !allow_anonymous &&
     (client_ca_file.empty() || !have_allow)) {
    error = "TLS exception: This server requires client certificates. "
      "Set a client CA file and at least one allowed client "
      "(fingerprint or CN), or call AllowAnonymousClients(true) to run "
      "an open server";
    return -1;
  }
  tls.MakeServer();
  tls.SetClientCAList(client_ca_file.c_str()); // 11/01/2026
  tls.SetCertFile(cert_fname.c_str());
  tls.SetKeyFile(key_fname.c_str());
  if(!passphrase.empty()) tls.SetPasswd(passphrase.c_str());
  if(!tls.InitCTX()) {
    error = tls.SSLExceptionMessage();
    if(!client_ca_file.empty() && tls.GetSSLError() == gxSSL_CALIST_ERROR) {
      error += ": " + client_ca_file; // 11/01/2026
    }
    return -1;
  }
  if(listener.Listen(port, bind_address) != 0) {
    error = listener.SocketExceptionMessage();
    error += ": port " + std::to_string(port);
    return -1;
  }
  return 0;
}

int gxsTLSServer::Accept(gxsTLSConnection &conn)
{
  error.clear();
  gxSocket client = listener.Accept();
  if(!client.IsOpen()) {
    error = listener.SocketExceptionMessage();
    return -1;
  }
  if(conn.AttachServer(std::move(client), tls.GetCTX()) != 0) {
    error = conn.Error();
    return -1;
  }
  // 11/01/2026: Fail closed: a client that does not pass the client
  // certificate check is closed before the caller can use it
  if(!client_ca_file.empty()) return check_client(conn);
  return 0;
}

int gxsTLSServer::Handshake(gxsTLSConnection &conn, gxSocket &&client)
// 11/07/2026: The TLS half of Accept() for a socket taken from
// Listener().Accept(), so a server can run the handshake in a worker
// thread and a client that never finishes it cannot hold up the
// accept loop. Set conn's timeout first: it bounds the handshake. Does
// not touch Error() (thread safe); the reason is in conn.Error().
{
  if(!client.IsOpen()) return -1;
  if(conn.AttachServer(std::move(client), tls.GetCTX()) != 0) return -1;
  if(!client_ca_file.empty()) return check_client(conn, false);
  return 0;
}
} // namespace datareel

// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
