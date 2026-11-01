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

11/01/2026: Failed handshakes close the connection, handshakes and
I/O use the timeout as a socket timeout, Send(std::string_view)
sends in chunks and Read() refuses requests over INT_MAX.

11/01/2026: Mutual TLS. gxsTLSServer can require client certificates
(SetClientCAFile()) and accept only the clients in an allow list of
SHA-256 certificate fingerprints and/or subject common names
(AllowClientFingerprint(), AllowClientCN()). Accept() closes any
client that fails, and the accepted gxsTLSConnection reports the
client's identity (PeerFingerprint(), PeerCN()). SetRequireClientAuth()
makes Listen() refuse to start without that setup unless
AllowAnonymousClients(true) is called; gxStreamServer (gxstream.h)
turns it on. Plain gxsTLSServer users, such as HTTPS servers, are
unchanged: no client certificate is asked for unless a client CA file
is set.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_TLS_CONNECTION_HPP__
#define __GX_TLS_CONNECTION_HPP__

#include "gxdlcode.h"

#include <string>
#include <string_view>
#include <vector>

#include "gxsocket.h"
#include "gxssl.h"

namespace datareel {


// 11/01/2026: Largest single write Send(std::string_view) passes on
const int gxsTLS_SEND_CHUNK = 1024 * 1024;

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
  // SetClientCert() above sets the certificate (with its chain) and key
  // a client sends when the server asks for one (mutual TLS). With
  // Schannel the certificate can also be a .pfx/.p12 file with the key
  // file left empty; see gxsschan.cpp.

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
  // 11/01/2026: Sends in chunks of gxsTLS_SEND_CHUNK. Returns the size
  // (at most INT_MAX) or -1 on an error.
  int Send(std::string_view s);
  int SendLine(std::string_view s); // Adds CR LF

  // Read up to bytes, waiting at most the timeout. Returns the number
  // read, 0 if the peer closed the connection, or -1 on an error.
  int ReadSome(void *buf, int bytes);

  // Read exactly bytes into s unless the connection closes or fails.
  // Returns the number read, or -1 if bytes is larger than INT_MAX.
  int Read(std::string &s, std::size_t bytes);

  // Read one line, without its CR LF. Returns false if the connection
  // closed, failed, timed out, or the line is longer than max_len.
  bool ReadLine(std::string &line, std::size_t max_len = 65536);

  // Read until the peer closes the connection
  bool ReadAll(std::string &s, std::size_t max_len = std::string::npos);

  void Close();

public: // Status
  bool IsOpen() const { return sock.IsOpen(); }
  bool IsSecure() const { return secure || fuzz_plain; }

#if defined (DATAREEL_FUZZING)
  // 11/01/2026: Test only. Lets a fuzz target drive gxStream over a
  // plain socket pair. Data still travels in cleartext. Close()
  // clears it. Never define DATAREEL_FUZZING in a real build.
  void FuzzAssumeSecure() { fuzz_plain = true; }
#endif
  const std::string &Host() const { return host_name; }
  const std::string &Error() const { return error; }

  // 11/01/2026: Server side. After gxsTLSServer::Accept() with client
  // certificates required, the identity of the client's certificate:
  // its SHA-256 fingerprint (upper case hex with colons) and subject
  // common name (UTF-8, empty if the certificate has none). Both are
  // empty if the server does not ask for client certificates. They
  // come from the client's certificate, so escape control characters
  // before writing them to a log.
  bool ClientAuthenticated() const { return !peer_fingerprint.empty(); }
  const std::string &PeerFingerprint() const { return peer_fingerprint; }
  const std::string &PeerCN() const { return peer_cn; }
  gxSocket &Socket() { return sock; }
  gxSSL &TLS() { return tls; }

private:
  int fail(const std::string &msg);
  int fail_closed(const std::string &msg); // 11/01/2026: Also closes sock
  int tls_handshake(bool as_server);
  int verify_server(); // 11/01/2026
  int fill(); // Read more data into rbuf
  friend class gxsTLSServer; // 11/01/2026: Sets the peer identity

private:
  gxSocket sock;
  gxSSL tls;
  bool secure = false;
  // 11/01/2026: Set only by FuzzAssumeSecure(). The member is always
  // here so the class layout does not depend on DATAREEL_FUZZING.
  bool fuzz_plain = false;
  bool verify_peer = true;
  int timeout_sec = 60;
  gxSSLMethod protocol = gxSSL_TLS;
  std::string host_name;
  std::string ca_file;
  std::string client_cert, client_key, key_pass;
  std::string rbuf; // Data received but not yet returned
  std::string error;
  std::string peer_fingerprint, peer_cn; // 11/01/2026: See PeerCN()
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
  // 11/01/2026: With client certificates required, the client must
  // send a certificate whose chain leads to a CA in the client CA file
  // and that is in the allow list (if the list is not empty), or it is
  // closed and Accept() returns -1. Call Accept() for every client:
  // AttachServer() with this server's context checks the chain but not
  // the allow list.
  int Accept(gxsTLSConnection &conn);

public: // 11/01/2026: Client certificates (mutual TLS). Set before Listen().
  // PEM file with the CAs that issue client certificates. Setting it
  // makes every client send a certificate that chains to one of these
  // CAs (only these, not the system store). Empty turns it off.
  void SetClientCAFile(const std::string &fname) { client_ca_file = fname; }
  const std::string &ClientCAFile() const { return client_ca_file; }

  // Allow list. A client is accepted if its certificate's SHA-256
  // fingerprint or its subject CN (exact, case sensitive) is in the
  // list. With an empty list any certificate from the client CAs is
  // accepted, unless SetRequireClientAuth() is on. A fingerprint is 32
  // hex bytes, with or without colons, in either case; the
  // "sha256 Fingerprint=" prefix printed by openssl is skipped.
  // AllowClientFingerprint() returns 0, or -1 if fp is not a SHA-256
  // fingerprint. AllowClientCN() returns -1 for an empty name.
  int AllowClientFingerprint(const std::string &fp);
  int AllowClientCN(const std::string &cn);
  void ClearClientAllowList() { allow_fingerprints.clear(); allow_cns.clear(); }
  const std::vector<std::string> &AllowedFingerprints() const { 
    return allow_fingerprints; 
  }
  const std::vector<std::string> &AllowedCNs() const { return allow_cns; }

  // Off by default. When on, Listen() fails unless a client CA file and
  // at least one allow list entry are set, or AllowAnonymousClients(true)
  // was called. gxStreamServer (gxstream.h) turns it on, so database
  // servers check client certificates unless the program opts out.
  void SetRequireClientAuth(bool on) { require_client_auth = on; }
  bool RequireClientAuth() const { return require_client_auth; }

  // Explicit opt-out of SetRequireClientAuth() for an open server that
  // takes clients without certificates. It does not turn off a client
  // CA file or allow list that is set; those are still enforced.
  void AllowAnonymousClients(bool on) { allow_anonymous = on; }
  bool AnonymousClientsAllowed() const { return allow_anonymous; }

  // Normalize a SHA-256 fingerprint to upper case hex with colons.
  // Returns an empty string if s is not a SHA-256 fingerprint.
  static std::string NormalizeFingerprint(const std::string &s);

  void Close() { listener.Close(); }
  bool IsOpen() const { return listener.IsOpen(); }
  int Port() const { return listener.LocalPort(); }
  const std::string &Error() const { return error; }
  gxSocket &Listener() { return listener; }
  gxSSL &TLS() { return tls; }

private:
  int check_client(gxsTLSConnection &conn); // 11/01/2026

private:
  gxSocket listener;
  gxSSL tls;
  std::string error;
  // 11/01/2026: Client certificate settings
  std::string client_ca_file;
  std::vector<std::string> allow_fingerprints; // Normalized
  std::vector<std::string> allow_cns;
  bool require_client_auth = false;
  bool allow_anonymous = false;
};

} // namespace datareel

#endif // __GX_TLS_CONNECTION_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
