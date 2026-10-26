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
==============================================================
*/
// ----------------------------------------------------------- //   
#include "gxdlcode.h"

#include <cstring>
#include "gxstls.h"

int gxsTLSConnection::fail(const std::string &msg)
{
  error = msg;
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
  if(!tls.InitCTX()) return fail(tls.SSLExceptionMessage());
  return tls_handshake(false);
}

int gxsTLSConnection::tls_handshake(bool as_server)
{
  if(tls.OpenSSLSocket(sock.GetSocket()) != gxSSL_NO_ERROR) {
    return fail(tls.SSLExceptionMessage());
  }
  if(as_server) {
    if(tls.AcceptSSLSocket() != gxSSL_NO_ERROR) {
      std::string msg = tls.SSLExceptionMessage();
      tls.DestroySSL();
      return fail(msg);
    }
  }
  else {
    tls.SetHostName(host_name.c_str());
    if(tls.ConnectSSLSocket() != gxSSL_NO_ERROR) {
      std::string msg = tls.SSLExceptionMessage();
      tls.DestroySSL();
      return fail(msg);
    }
    if(verify_peer) {
      long code = 0;
      if(tls.VerifyCert(host_name.c_str(), &code) != gxSSL_NO_ERROR) {
	std::string msg = tls.SSLExceptionMessage();
	if(code != 0) {
	  msg += " (";
	  msg += gxSSL::CertVerifyErrorString(code);
	  msg += ")";
	}
	msg += " for " + host_name;
	tls.CloseSSLSocket();
	return fail(msg);
      }
    }
  }
  secure = true;
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
  if(tls.OpenSSLSocket(sock.GetSocket()) != gxSSL_NO_ERROR) {
    return fail(tls.SSLExceptionMessage());
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
    std::string msg = tls.SSLExceptionMessage();
    tls.DestroySSL();
    return fail(msg);
  }
  // The control connection already checked this server's certificate
  secure = true;
  return 0;
}

int gxsTLSConnection::AttachServer(gxSocket &&client, gxSSL_CTX_t *server_ctx)
{
  Close();
  error.clear();
  sock = std::move(client);
  host_name = sock.PeerIPAddress();
  tls.MakeServer();
  tls.SetCTX(server_ctx);
  return tls_handshake(true);
}

void gxsTLSConnection::Close()
{
  if(secure) {
    tls.CloseSSLSocket();
    secure = false;
  }
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

int gxsTLSServer::Listen(int port, const std::string &cert_fname, 
			 const std::string &key_fname,
			 const std::string &passphrase,
			 const std::string &bind_address)
{
  error.clear();
  tls.MakeServer();
  tls.SetCertFile(cert_fname.c_str());
  tls.SetKeyFile(key_fname.c_str());
  if(!passphrase.empty()) tls.SetPasswd(passphrase.c_str());
  if(!tls.InitCTX()) {
    error = tls.SSLExceptionMessage();
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
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
