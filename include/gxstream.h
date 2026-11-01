// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxstream.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
// Date Last Modified: 10/31/2026
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

The GXStream class is used to stream data from a client to a 
server over a TCP/IP connection.

Changes:
==============================================================
10/31/2026: Block lengths read from the network are checked against
SetMaxBlockSize() (64 MB by default) before any memory is allocated
or any data is read. ReadBlock()/ReadRemoteBlock() have overloads that
take the size of the caller's buffer; the old forms read as many bytes
as the peer's header says. WriteBlock() rejects negative lengths and
lengths over INT_MAX. Added WriteResultBlock64()/ReadResultBlock64()
so a 64-bit file address survives the round trip.

10/31/2026: TLS mode and client authentication. A networked database
server must authenticate its clients, so StreamServer(), Accept() and
AcceptClient() refuse to run (gxSOCKET_SSL_ERROR, see TLSError())
unless the server either
  - uses TLS with client certificates: SetServerCert(),
    SetClientCAFile() and at least one AllowClientFingerprint() or
    AllowClientCN() entry, or
  - opts out with AllowAnonymousClients(true): an open server, plain TCP
    as before 6.3 without SetServerCert(), TLS without client
    certificates with it.
Clients use TLS after UseTLS(1) or SetClientCert() and check the
server certificate (SetServerCAFile(), or the system store) and host
name unless SetVerifyServer(0). In TLS mode the block functions only
work on the TLS connection: a failed handshake or certificate check
closes the socket, so nothing is ever sent in cleartext. Clients that
stay on plain TCP are unchanged.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_STREAM_HPP__
#define __GX_STREAM_HPP__

#include "gxdlcode.h"

#include "gxsocket.h"
#include "gxheader.h"

const int gxSTREAM_DEF_TIMEOUT = 7200;

// 10/31/2026: Largest block length accepted from the network by
// default. See gxStream::SetMaxBlockSize().
const gxUINT32 gxSTREAM_DEF_MAX_BLOCK = 64*1024*1024;

// 10/31/2026: TLS settings and connections of a gxStream, defined in
// gxstream.cpp so the class layout does not depend on the SSL build
struct gxStreamTLS;

class GXDLCODE_API gxStream : public gxSocket
{
public:
  gxStream() { 
    time_out_sec = gxSTREAM_DEF_TIMEOUT; // Blocking timeout value   
    time_out_usec = 0;
    max_block = gxSTREAM_DEF_MAX_BLOCK; // 10/31/2026
    tls = 0; // 10/31/2026: Made when a TLS setting is first used
  }
  ~gxStream(); // 10/31/2026: Closes TLS connections

public: // Client/Server configuration
  int StreamServer(int port, int max_connections = SOMAXCONN);
  int StreamClient(int port, char *host);
  int PortNumber(int &port);
  int RemotePortNumber(int &port);
  int HostName(char *hs);
  int RemoteHostName(char *hs);
  
public: // High-level database block functions
  int WriteBlock(const void *buf, int bytes);
  int TerminateConnection();
  int RestartConnection();
  int CloseConnection();
  int ReadClientHeader(gxBlockHeader &gxs);
  int ReadRemoteBlock(void *buf, gxBlockHeader &gx);
  // 10/31/2026: Reads the block only if it fits in buf_size bytes
  int ReadRemoteBlock(void *buf, int buf_size, gxBlockHeader &gx);
  int WriteRemoteBlock(void *buf, int bytes);
  void *RequestBlock(const void *request, gxBlockHeader &request_header, 
		     gxBlockHeader &requested_block_header);
  int AddBlock(const void *block, gxBlockHeader &block_header);
  int ChangeBlock(const void *request, const void *block, 
		  gxBlockHeader &request_header, gxBlockHeader &block_header);
  int DeleteBlock(const void *request, gxBlockHeader &request_header);
  int WriteAckBlock();
  int WriteRemoteAckBlock();
  int ReadAckBlock();
  int ReadRemoteAckBlock();

  // Optional status information exchange between TCP client and server.
  int WriteResultBlock(int result_code);
  int WriteRemoteResultBlock(int result_code);
  int ReadResultBlock(int &result_code);
  int ReadRemoteResultBlock(int &result_code);

  // 10/31/2026: Full width results, used for 64-bit file addresses.
  // ReadResultBlock(int &) truncates a result sent with these.
  int WriteResultBlock64(FAU_t result_code);
  int WriteRemoteResultBlock64(FAU_t result_code);
  int ReadResultBlock64(FAU_t &result_code);
  int ReadRemoteResultBlock64(FAU_t &result_code);
  
public: // Low-level database block functions
  void SetBlockStatus(gxBlockHeader &gx, __SBYTE__ net_status,
		      __SBYTE__ file_status = gxRemoteDeviceBlock);
  int WriteBlock(gxsSocket_t s, const void *buf, int bytes);
  int WriteBlock(gxsSocket_t s, const void *buf, gxBlockHeader &gx);
  int WriteHeader(gxsSocket_t s, gxBlockHeader &gx);
  int ReadBlock(gxsSocket_t s, void *buf, gxBlockHeader &gx,
		int seconds = gxSTREAM_DEF_TIMEOUT, int useconds = 0);
  // 10/31/2026: Reads gx.block_length bytes into buf only if they fit
  // in buf_size bytes (and in MaxBlockSize()). Otherwise nothing is
  // read, gxSOCKET_BLOCKSIZE_ERROR is returned, and the caller should
  // close the connection (the stream is out of step).
  int ReadBlock(gxsSocket_t s, void *buf, int buf_size, gxBlockHeader &gx,
		int seconds = gxSTREAM_DEF_TIMEOUT, int useconds = 0);
  int ReadHeader(gxsSocket_t s, gxBlockHeader &gx, 
		 int seconds = gxSTREAM_DEF_TIMEOUT, int useconds = 0);
  void *RequestBlock(gxsSocket_t s, const void *request,
		     gxBlockHeader &request_header, 
		     gxBlockHeader &requested_block_header,
		     int seconds = gxSTREAM_DEF_TIMEOUT, int useconds = 0);
  int AddBlock(gxsSocket_t s, const void *block, gxBlockHeader &block_header);
  int DeleteBlock(gxsSocket_t s, const void *request,
		  gxBlockHeader &request_header);
  int ChangeBlock(gxsSocket_t s, const void *request, 
		  const void *block, gxBlockHeader &request_header,
		  gxBlockHeader &block_header);
  int WriteAckBlock(gxsSocket_t s);
  int ReadAckBlock(gxsSocket_t s, 
		   int seconds = gxSTREAM_DEF_TIMEOUT, int useconds = 0);

  // Optional status information exchange between TCP client and server.
  int WriteResultBlock(gxsSocket_t s, int result_code);
  int ReadResultBlock(gxsSocket_t s, int &result_code,
		      int seconds = gxSTREAM_DEF_TIMEOUT, int useconds = 0);
  int WriteResultBlock64(gxsSocket_t s, FAU_t result_code); // 10/31/2026
  int ReadResultBlock64(gxsSocket_t s, FAU_t &result_code,  // 10/31/2026
			int seconds = gxSTREAM_DEF_TIMEOUT, int useconds = 0);
  
  int TerminateConnection(gxsSocket_t s);
  int RestartConnection(gxsSocket_t s);
  int CloseConnection(gxsSocket_t s);

public: // Timer functions
  void SetTimeOut(int seconds = gxSTREAM_DEF_TIMEOUT, int useconds = 0) {
    time_out_sec = seconds;
    time_out_usec = useconds;
  }

public: // 10/31/2026: Server authentication of clients (see the notes
        // at the top of this file). Set before StreamServer().
  // Server certificate (with its chain) and private key; turns on TLS.
  // Returns 0, or gxSOCKET_SSL_ERROR if the library has no SSL.
  int SetServerCert(const char *cert_file, const char *key_file,
		    const char *passphrase = 0);
  // PEM file with the CAs that issue client certificates. Every client
  // must then send a certificate that chains to one of these CAs (only
  // these, not the system store). Needs SetServerCert().
  void SetClientCAFile(const char *fname);
  // Allow list. A client is accepted if its certificate's SHA-256
  // fingerprint or its subject CN (exact, case sensitive) is listed.
  // A fingerprint is 32 hex bytes, with or without colons, in either
  // case ("openssl x509 -noout -fingerprint -sha256" output works).
  // Return 0, or -1 for a malformed fingerprint or an empty CN.
  int AllowClientFingerprint(const char *fp);
  int AllowClientCN(const char *cn);
  void ClearClientAllowList();
  // Explicit opt-out: run an open server that takes clients without
  // certificates (plain TCP without SetServerCert()). A client CA file
  // or allow list that is set is still enforced.
  void AllowAnonymousClients(bool allow = true);
  int AnonymousClientsAllowed();
  // Accept a client on this listening stream into remote_socket and,
  // in TLS mode, run the handshake and the client checks. Returns the
  // client socket, or -1 (the client is closed if a check fails). In
  // TLS mode a client accepted before by Accept() is closed first.
  gxsSocket_t Accept();
  // Accept a client into another gxStream (for example one per
  // thread); use its ReadClientHeader()/Remote functions. This stream
  // must stay open while client uses the connection. Returns 0.
  int AcceptClient(gxStream &client);
  using gxSocket::Accept; // Accept(s, sa): low level, no checks

public: // 10/31/2026: Client TLS settings. Set before StreamClient().
  void UseTLS(int on = 1);
  // Certificate and key sent to servers that ask for one; turns on TLS
  int SetClientCert(const char *cert_file, const char *key_file,
		    const char *passphrase = 0);
  // CA file used to check the server certificate. Without one the
  // system certificate store is used.
  void SetServerCAFile(const char *fname);
  // Name checked against the server certificate; the host passed to
  // StreamClient() by default. SetVerifyServer(0) turns the check off.
  void SetServerName(const char *name);
  void SetVerifyServer(int on = 1);

public: // 10/31/2026: TLS status (both sides)
  int IsTLS();            // True if TLS mode is set
  int IsSecure();         // True if this stream's connection is TLS
  int IsRemoteSecure();   // True if the accepted client is on TLS
  const char *TLSError(); // Why the last TLS or client check failed
  // Identity of the accepted client's certificate: SHA-256 fingerprint
  // (upper case hex with colons) and subject CN. Empty without client
  // certificates. They come from the client, so escape control
  // characters before writing them to a log.
  int ClientAuthenticated();
  const char *PeerFingerprint();
  const char *PeerCN();
  // Socket send/receive timeout during a TLS handshake (default
  // gxSSL_DEF_HANDSHAKE_TIMEOUT, 60 seconds)
  void SetHandshakeTimeout(int seconds);
  // Close the connections, ending TLS first. Close() hides
  // gxSocket::Close(); CloseRemoteSocket() drops the accepted client.
  void Close();
  void CloseRemoteSocket();
  using gxSocket::Close; // Close(s): low level

public: // 10/31/2026: Limits
  // Largest block length accepted from the peer, checked before any
  // memory is allocated (RequestBlock()) or data is read (ReadBlock()).
  // The default is gxSTREAM_DEF_MAX_BLOCK. At most INT_MAX is used.
  void SetMaxBlockSize(gxUINT32 bytes) { max_block = bytes; }
  gxUINT32 MaxBlockSize() { return max_block; }

public:
  int time_out_sec;  // Number of seconds before a blocking timeout
  int time_out_usec; // Number of microseconds before a blocking timeout

protected:
  gxUINT32 max_block; // 10/31/2026: See SetMaxBlockSize()
  int check_block_length(const gxBlockHeader &gx, int buf_size = -1);

protected: // 10/31/2026: TLS internals
  gxStreamTLS *tls;
  gxStreamTLS *tls_state(); // Makes tls if needed
  int stream_send(gxsSocket_t s, const void *buf, int bytes);
  int stream_recv(gxsSocket_t s, void *buf, int bytes, int seconds,
		  int useconds);
  int check_server_policy();
  int tls_accept(gxsSocket_t s, gxStreamTLS *conn_state, gxStreamTLS *cfg);
  int tls_connect();
  int tls_fail(const char *msg);
};

#endif // __GX_STREAM_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
