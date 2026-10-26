// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxstls.h
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
gxsTLSConnection is a TCP connection that is encrypted from the 
start (implicit TLS) or upgraded after a STARTTLS style command, 
with buffered line reading for text protocols. Clients check the 
server certificate and host name by default. gxsTLSServer listens 
on a port and accepts TLS clients.

Changes:
==============================================================
10/26/2026: Added for DataReel 7, which requires TLS for every 
network protocol except telnet.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_TLS_CONNECTION_HPP__
#define __GX_TLS_CONNECTION_HPP__

#include "gxdlcode.h"

#include <string>
#include <string_view>

#include "gxsocket.h"
#include "gxssl.h"

namespace datareel {


class GXDLCODE_API gxsTLSConnection
{
public:
  gxsTLSConnection() { }
  ~gxsTLSConnection() { Close(); }

  gxsTLSConnection(const gxsTLSConnection &) = delete;
  gxsTLSConnection &operator=(const gxsTLSConnection &) = delete;

public: // Settings used before Connect()
  void SetTimeout(int seconds) { timeout_sec = seconds; }
  int Timeout() const { return timeout_sec; }
  void SetCAFile(const std::string &fname) { ca_file = fname; }
  void SetClientCert(const std::string &cert_fname, 
		     const std::string &key_fname,
		     const std::string &passphrase = std::string()) {
    client_cert = cert_fname; client_key = key_fname; key_pass = passphrase;
  }
  void SetVerifyPeer(bool on) { verify_peer = on; }
  bool VerifyPeer() const { return verify_peer; }
  void SetProtocol(gxSSLMethod m) { protocol = m; }

public: // Client connections
  // Open a TCP connection. With implicit_tls the TLS handshake starts
  // at once; otherwise call StartTLS() after the protocol asks for it.
  // Returns 0 if successful.
  int Connect(const std::string &host, int port, bool implicit_tls = true);

  // Start TLS on an open plain connection. Returns 0 if successful.
  int StartTLS();

  // Start TLS as a client on a connected socket (an FTPS data
  // connection). session_from reuses that connection's TLS session,
  // which most FTPS servers require. Returns 0 if successful.
  int AttachClient(gxSocket &&s, const std::string &host,
		   gxsTLSConnection *session_from = 0);

public: // Server connections
  // Take an accepted client socket and run the TLS handshake with a
  // server context. Returns 0 if successful.
  int AttachServer(gxSocket &&client, gxSSL_CTX_t *server_ctx);

public: // Data transfer
  int Send(const void *buf, int bytes); // All bytes, or -1 on error
  int Send(std::string_view s) { return Send(s.data(), (int)s.size()); }
  int SendLine(std::string_view s); // Adds CR LF

  // Read up to bytes, waiting at most the timeout. Returns the number
  // read, 0 if the peer closed the connection, or -1 on an error.
  int ReadSome(void *buf, int bytes);

  // Read exactly bytes into s unless the connection closes or fails
  int Read(std::string &s, std::size_t bytes);

  // Read one line, without its CR LF. Returns false if the connection
  // closed, failed, timed out, or the line is longer than max_len.
  bool ReadLine(std::string &line, std::size_t max_len = 65536);

  // Read until the peer closes the connection
  bool ReadAll(std::string &s, std::size_t max_len = std::string::npos);

  void Close();

public: // Status
  bool IsOpen() const { return sock.IsOpen(); }
  bool IsSecure() const { return secure; }
  const std::string &Host() const { return host_name; }
  const std::string &Error() const { return error; }
  gxSocket &Socket() { return sock; }
  gxSSL &TLS() { return tls; }

private:
  int fail(const std::string &msg);
  int tls_handshake(bool as_server);
  int fill(); // Read more data into rbuf

private:
  gxSocket sock;
  gxSSL tls;
  bool secure = false;
  bool verify_peer = true;
  int timeout_sec = 60;
  gxSSLMethod protocol = gxSSL_TLS;
  std::string host_name;
  std::string ca_file;
  std::string client_cert, client_key, key_pass;
  std::string rbuf; // Data received but not yet returned
  std::string error;
};

class GXDLCODE_API gxsTLSServer
{
public:
  gxsTLSServer() { }

public:
  // Load the server certificate chain and key, then listen on port.
  // An empty bind_address listens on every IPv6 and IPv4 interface.
  // Returns 0 if successful.
  int Listen(int port, const std::string &cert_fname, 
	     const std::string &key_fname,
	     const std::string &passphrase = std::string(),
	     const std::string &bind_address = std::string());

  // Wait for a client, accept it and run the TLS handshake. Returns 0
  // if successful. A failed handshake closes only that client.
  int Accept(gxsTLSConnection &conn);

  void Close() { listener.Close(); }
  bool IsOpen() const { return listener.IsOpen(); }
  int Port() const { return listener.LocalPort(); }
  const std::string &Error() const { return error; }
  gxSocket &Listener() { return listener; }
  gxSSL &TLS() { return tls; }

private:
  gxSocket listener;
  gxSSL tls;
  std::string error;
};

} // namespace datareel

#endif // __GX_TLS_CONNECTION_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
