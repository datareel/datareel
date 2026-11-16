// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxstream.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
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

The gxStream class is used to stream database blocks from a 
client to a server over a TCP/IP connection.

Changes:
==============================================================
08/02/2003: Modified the gxStream::RequestBlock() not to accept
null block values.

08/03/2003: Fixed potential memory leak in the gxStream::RequestBlock() 
function by deleting allocated buffer following a receive error.

10/31/2026: Block lengths from the network are checked against
SetMaxBlockSize() before RequestBlock() allocates memory (a peer could
make it allocate up to the whole address space: block_length is 64
bits wide on 64-bit UNIX) and before ReadBlock() reads. New ReadBlock()
and ReadRemoteBlock() overloads take the size of the caller's buffer;
the old ones wrote as many bytes as the peer's header said. WriteBlock()
rejects negative lengths, lengths over INT_MAX (Send() takes an int)
and a null buffer. Added WriteResultBlock64()/ReadResultBlock64().

10/31/2026: TLS mode with client certificates (mutual TLS), required
by default for servers: StreamServer(), Accept() and AcceptClient()
refuse to run unless the server checks client certificates against a
client CA file and an allow list of SHA-256 fingerprints and/or CNs,
or the program calls AllowAnonymousClients(true). See gxstream.h. All
block I/O goes through stream_send()/stream_recv(), which use the TLS
connection in TLS mode and refuse any other socket, so nothing is sent
in cleartext after a failed handshake.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <string.h>
#include <limits.h>
#include <new> // 10/31/2026: std::nothrow
#include <vector>
#include "gxstream.h"
#include "gxstring.h"
#if defined (__USE_GX_SSL_EXTENSIONS__)
#include "gxssl.h"
#endif
#if defined (__UNIX__)
#include <poll.h>
#include <sys/socket.h>
#endif

// --------------------------------------------------------------
// 10/31/2026: TLS settings and connections of a gxStream
// --------------------------------------------------------------
struct gxStreamTLS
{
  gxStreamTLS() {
    allow_anonymous = 0;
    use_tls = 0;
    accepted_tls = 0;
    verify_server = 1;
#if defined (__USE_GX_SSL_EXTENSIONS__)
    handshake_timeout = gxSSL_DEF_HANDSHAKE_TIMEOUT;
    listen_ctx = 0;
    client_conn = 0;
    remote_conn = 0;
#else
    handshake_timeout = 60;
#endif
    client_fd = remote_fd = (gxsSocket_t)-1;
  }
  ~gxStreamTLS();

  // Server settings
  gxString server_cert, server_key, server_pass, client_ca_file;
  std::vector<gxString> allow_fp; // Normalized SHA-256 fingerprints
  std::vector<gxString> allow_cn;
  int allow_anonymous;
  int accepted_tls; // Set by AcceptClient() on a TLS listener

  // Client settings
  int use_tls;
  int verify_server;
  gxString client_cert, client_key, client_pass, server_ca_file, server_name;

  int handshake_timeout;
#if defined (__USE_GX_SSL_EXTENSIONS__)
  gxSSL *listen_ctx;  // Server context, owned
  gxSSL *client_conn; // TLS on gxsocket (client); owns its context
  gxSSL *remote_conn; // TLS on remote_socket; uses the listener's context
#endif
  gxsSocket_t client_fd, remote_fd;

  gxString peer_fp, peer_cn; // Identity of the accepted client
  gxString error;            // See gxStream::TLSError()

  int server_tls() { return !server_cert.is_null(); }
  // True if the block functions must use TLS
  int tls_mode() { return use_tls || accepted_tls || server_tls(); }
};

#if defined (__USE_GX_SSL_EXTENSIONS__) // 11/16/2026 (6.4 beta 2): used only with TLS
static void gxs_linger_close(gxsSocket_t s)
// 10/31/2026: Used after the TLS close_notify, before the socket is
// closed. Closing a socket with unread data in it (TLS 1.3 session
// tickets, the peer's close_notify) sends a TCP reset, and a reset
// makes the peer's kernel drop data it has not read yet. Send a FIN,
// then read and discard what the peer sends until it closes, for at
// most about one second and 1 MB, so a silent peer cannot hold up
// Close() (as DataReel 7.3.1).
{
  if(s == (gxsSocket_t)-1) return;
#if defined (__WIN32__)
  shutdown(s, 1); // SD_SEND
#else
  shutdown(s, SHUT_WR);
#endif
  char buf[4096];
  long total = 0;
  for(int i = 0; i < 10 && total < 1024L*1024L; i++) {
#if defined (__UNIX__)
    struct pollfd pfd;
    pfd.fd = s; pfd.events = POLLIN; pfd.revents = 0;
    if(poll(&pfd, 1, 100) != 1) break;
#else
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(s, &fds);
    struct timeval tv;
    tv.tv_sec = 0; tv.tv_usec = 100000;
    if(select((int)s+1, &fds, 0, 0, &tv) != 1) break;
#endif
    int n = (int)recv(s, buf, (int)sizeof(buf), 0);
    if(n <= 0) break; // The peer closed, or an error
    total += n;
  }
}
#endif // __USE_GX_SSL_EXTENSIONS__

#if defined (__USE_GX_SSL_EXTENSIONS__)
static void gxs_free_conn(gxSSL *&c, int owns_ctx, gxsSocket_t s, 
			  int send_close)
// 10/31/2026: End and free a TLS connection. A connection made by
// Accept() uses the listener's context, which must not be freed here.
{
  if(!c) return;
  if(send_close && c->GetSSL()) {
    c->CloseSSLSocket(0);
    gxs_linger_close(s);
  }
  if(!owns_ctx) c->ReleaseCTX();
  delete c;
  c = 0;
}
#endif

gxStreamTLS::~gxStreamTLS()
{
#if defined (__USE_GX_SSL_EXTENSIONS__)
  gxs_free_conn(client_conn, 1, client_fd, 0);
  gxs_free_conn(remote_conn, 0, remote_fd, 0);
  if(listen_ctx) delete listen_ctx;
  listen_ctx = 0;
#endif
}

static gxString gxs_normalize_fingerprint(const char *s)
// 10/31/2026: Returns the fingerprint as 32 upper case hex bytes
// separated by colons, or an empty string if s is not one. Skips the
// "sha256 Fingerprint=" prefix printed by openssl.
{
  gxString out;
  if(!s) return out;
  const char *start = strrchr(s, '=');
  start = start ? start + 1 : s;
  char digits[65];
  int n = 0;
  for(const char *p = start; *p; p++) {
    char c = *p;
    if(c == ':' || c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
    if(c >= 'a' && c <= 'f') c = (char)(c - 'a' + 'A');
    if(!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) return out;
    if(n >= 64) return out;
    digits[n++] = c;
  }
  if(n != 64) return out;
  char sbuf[32 * 3];
  int pos = 0;
  for(int i = 0; i < 64; i += 2) {
    if(i > 0) sbuf[pos++] = ':';
    sbuf[pos++] = digits[i];
    sbuf[pos++] = digits[i+1];
  }
  sbuf[pos] = 0;
  out = sbuf;
  return out;
}

#if defined (__USE_GX_SSL_EXTENSIONS__) // 11/16/2026 (6.4 beta 2): used only with TLS
static gxString gxs_printable(const gxString &s, unsigned max_len = 256)
// 10/31/2026: Copy of a string from a certificate that is safe to put
// in an error message or log: control characters, DEL and backslash
// are escaped as \xNN, and long strings are cut.
{
  static const char hex[] = "0123456789ABCDEF";
  gxString out;
  const char *p = s.c_str();
  unsigned len = s.length();
  for(unsigned i = 0; i < len && i < max_len; i++) {
    unsigned char c = (unsigned char)p[i];
    if(c < 0x20 || c == 0x7F || c == '\\') {
      char esc[5];
      esc[0] = '\\'; esc[1] = 'x'; 
      esc[2] = hex[(c >> 4) & 0x0F]; esc[3] = hex[c & 0x0F]; esc[4] = 0;
      out << esc;
    }
    else {
      out << (char)c;
    }
  }
  if(len > max_len) out << "...";
  return out;
}
#endif // __USE_GX_SSL_EXTENSIONS__

static int gxs_in_list(const std::vector<gxString> &v, const gxString &s)
{
  for(size_t i = 0; i < v.size(); i++) {
    if(v[i].length() == s.length() && 
       memcmp(v[i].c_str(), s.c_str(), s.length()) == 0) return 1;
  }
  return 0;
}

#ifdef __BCC32__
#pragma warn -8080
#endif

// TCP/IP Stream functions
// --------------------------------------------------------------
int gxStream::StreamServer(int port, int max_connections)
// Initialize a stream server. Please note all ports below 1024 are
// reserved. Returns a non-zero value if any errors occur during
// initialization. The "max_connections" variable determines how many
// pending connections the queue will hold.
{
  // 10/31/2026: A server must authenticate its clients or explicitly
  // opt out (see gxstream.h). Checked before anything is opened.
  if(check_server_policy() != gxSOCKET_NO_ERROR) return socket_error;
#if defined (__USE_GX_SSL_EXTENSIONS__)
  if(tls && tls->server_tls() && !tls->listen_ctx) {
    gxSSL *c = new gxSSL;
    c->MakeServer();
    if(!tls->client_ca_file.is_null()) {
      c->SetClientCAList(tls->client_ca_file.c_str());
    }
    if(!c->InitCTX(tls->server_cert.c_str(), tls->server_key.c_str(),
		   tls->server_pass.c_str())) {
      gxString msg;
      msg << clear << c->SSLExceptionMessage() << " (server certificate, key"
	  << " or client CA file)";
      delete c;
      return tls_fail(msg.c_str());
    }
    tls->listen_ctx = c;
  }
#endif

  if(InitSocketLibrary() == 0) {
    if(InitSocket(SOCK_STREAM, port) < 0) return socket_error;
  }
  else {
    return socket_error;
  }


  // Bind the name to the socket
  if(Bind() < 0) {
    Close();
    return socket_error;
  }

  // Listen for connections with a specified backlog
  if(Listen(max_connections) < 0) {
    Close();
    return socket_error;
  }

  return socket_error = gxSOCKET_NO_ERROR;
}

int gxStream::StreamClient(int port, char *host)
// Connect a stream client. Please note all ports below 1024 are
// reserved. Returns zero if no errors occur during initialization.
{
  if(InitSocketLibrary() == 0) {
    if(InitSocket(SOCK_STREAM, port, host) < 0) return socket_error;
  }
  else {
    return socket_error;
  }

  // Connect to the server
  if(Connect() < 0) {
    return socket_error;
  }

  // 10/31/2026: TLS handshake and server check. A failure closes the
  // socket, so the caller cannot fall back to cleartext.
  if(tls && tls->use_tls) {
    if(tls->server_name.is_null() && host) {
      // The name the server certificate is checked against
      tls->server_name = host;
      int rv = tls_connect();
      tls->server_name.Clear();
      return rv;
    }
    return tls_connect();
  }

  return socket_error = gxSOCKET_NO_ERROR;  
}

void gxStream::SetBlockStatus(gxBlockHeader &gx, __SBYTE__ net_status,
			      __SBYTE__ file_status)
// Set the database block header status member control byte.
// NOTE: The block's next deleted variable must be set, if
// needed, after a call to this function. The block length
// will not be set here.
{
  __ULWORD__ block_status = net_status;
  block_status = (block_status<<8) & 0xff00;
  block_status += file_status;
  gx.block_status = block_status;
  gx.block_check_word = gxCheckWord;
  gx.block_nd_fptr = (FAU)0;
}

int gxStream::check_block_length(const gxBlockHeader &gx, int buf_size)
// 10/31/2026: Internal function used to check a block length read from
// the network before memory is allocated or data is read. buf_size is
// the size of the caller's buffer, or -1 if not known. Returns zero if
// the length can be read.
{
  __ULWORD__ len = (__ULWORD__)gx.block_length;
  if(len > (__ULWORD__)max_block) return socket_error = gxSOCKET_BLOCKSIZE_ERROR;
  if(len > (__ULWORD__)INT_MAX) return socket_error = gxSOCKET_BLOCKSIZE_ERROR;
  if(buf_size >= 0 && len > (__ULWORD__)buf_size) {
    return socket_error = gxSOCKET_BLOCKSIZE_ERROR;
  }
  return gxSOCKET_NO_ERROR;
}

int gxStream::WriteBlock(gxsSocket_t s, const void *buf, int bytes)
// Write a block of raw data to the socket. NOTE: All block read and write
// operations must operate in blocking mode to maintain synchronization
// between the transmitter and receiver. Returns zero if no errors
// occur.
{
  // 10/31/2026: A negative count became a huge block length in the
  // header followed by no data
  if(bytes < 0) return socket_error = gxSOCKET_BLOCKSIZE_ERROR;
  gxBlockHeader gx;
  SetBlockStatus(gx, gxSendBlock);
  gx.block_length = bytes;

  // Write the block header
  if(WriteBlock(s, buf, gx) != gxSOCKET_NO_ERROR) return socket_error;
  
  return socket_error = gxSOCKET_NO_ERROR;  
}

int gxStream::WriteBlock(gxsSocket_t s, const void *buf, gxBlockHeader &gx)
// Write a database block to the specified socket. Returns zero if no
// errors occur. NOTE: This function assumes that the block header has
// already been initialized.
{
  // 10/31/2026: Check before anything is sent. Send() takes an int, so
  // a longer block was cut short after a header with the full length.
  if((__ULWORD__)gx.block_length > (__ULWORD__)INT_MAX) {
    return socket_error = gxSOCKET_BLOCKSIZE_ERROR;
  }
  if(gx.block_length > 0 && !buf) {
    return socket_error = gxSOCKET_BLOCKSIZE_ERROR;
  }

  // Write the block header
  if(WriteHeader(s, gx) != gxSOCKET_NO_ERROR) return socket_error;

  // Write the block
  if(stream_send(s, (char *)buf, (int)gx.block_length) < 0)
    return socket_error;
  
  if((gxUINT32)bytes_moved != gx.block_length) 
      return socket_error = gxSOCKET_BLOCKSIZE_ERROR;

  return socket_error = gxSOCKET_NO_ERROR;  
}

int gxStream::WriteHeader(gxsSocket_t s, gxBlockHeader &gx)
// Write a database block header to the specified socket.
// returns zero if no errors occur.
{
  // Write the block header
  if(stream_send(s, (char *)&gx, sizeof(gxBlockHeader)) < 0)
    return socket_error;

  // If the bytes sent do not equal the bytes moved then
  // the transmitter and receiver are no longer synchronized.
  if(bytes_moved != sizeof(gxBlockHeader)) 
    return socket_error = gxSOCKET_BLOCKHEADER_ERROR;

  return socket_error = gxSOCKET_NO_ERROR;
}

int gxStream::ReadBlock(gxsSocket_t s, void *buf, gxBlockHeader &gx,
			int seconds, int useconds)
// Read database block from the specified socket. Returns zero
// if no errors occur. NOTE: buf must hold gx.block_length bytes, a
// length sent by the peer. Use the overload with the buffer size.
{
  // 10/31/2026: The length comes from the peer
  if(check_block_length(gx) != gxSOCKET_NO_ERROR) return socket_error;
  if(gx.block_length > 0 && !buf) return socket_error = gxSOCKET_BLOCKSIZE_ERROR;
  if(stream_recv(s, buf, (int)gx.block_length, seconds, useconds) < 0)
    return socket_error;

  // If the bytes read do not equal the bytes received then
  // the transmitter and receiver are no longer synchronized.
  if((gxUINT32)bytes_read != gx.block_length) 
    return socket_error = gxSOCKET_BLOCKSIZE_ERROR;

  return socket_error = gxSOCKET_NO_ERROR;
}

int gxStream::ReadBlock(gxsSocket_t s, void *buf, int buf_size,
			gxBlockHeader &gx, int seconds, int useconds)
// 10/31/2026: Read a database block into a buffer of buf_size bytes.
// Nothing is read if the block does not fit (gxSOCKET_BLOCKSIZE_ERROR).
// Returns zero if no errors occur.
{
  if(buf_size < 0) return socket_error = gxSOCKET_BLOCKSIZE_ERROR;
  if(check_block_length(gx, buf_size) != gxSOCKET_NO_ERROR) return socket_error;
  return ReadBlock(s, buf, gx, seconds, useconds);
}

int gxStream::ReadHeader(gxsSocket_t s, gxBlockHeader &gx,
			 int seconds, int useconds)
// Read a database block header from the specified socket.
// Returns zero if no errors occur or an error number
// corresponding to a value defined in the gxSocketError
// enumeration.
{
  if(stream_recv(s, (char *)&gx, sizeof(gxBlockHeader), seconds, useconds) < 0)
    return socket_error;

  // If the bytes read do not equal the bytes received then
  // the transmitter and receiver are no longer synchronized.
  if(bytes_read != sizeof(gxBlockHeader)) 
    return socket_error = gxSOCKET_BLOCKHEADER_ERROR;
      
  if(gx.block_check_word != gxCheckWord) 
    return socket_error = gxSOCKET_BLOCKSYNC_ERROR;
  	
  return socket_error = gxSOCKET_NO_ERROR;    
}

void *gxStream::RequestBlock(gxsSocket_t s, const void *request,
			     gxBlockHeader &request_header, 
			     gxBlockHeader &requested_block_header,
			     int seconds, int useconds)
// Request a block from the specified socket. Returns the requested block
// or a null value if an error occurs.
{
  SetBlockStatus(request_header, gxRequestBlock);

  // Write the request header and the request itself
  if(WriteBlock(s, request, request_header) != gxSOCKET_NO_ERROR)
    return 0;
  
  // Read the returned request header 
  if(ReadHeader(s, requested_block_header, seconds, useconds) != 
     gxSOCKET_NO_ERROR) {
    return 0;
  }

  // Return null for all zero length blocks
  if(requested_block_header.block_length == (__ULWORD__)0) {
    return 0;
  }

  // 10/31/2026: Check the length sent by the peer before allocating
  // memory for it
  if(check_block_length(requested_block_header) != gxSOCKET_NO_ERROR) {
    return 0;
  }

  // Read the returned requested block
  char *buf = new (std::nothrow) 
    char[(__ULWORD__)requested_block_header.block_length];
  if(!buf) {
    socket_error = gxSOCKET_BUFOVER_ERROR; // 10/31/2026
    return 0;
  }

  if(stream_recv(s, buf, (int)requested_block_header.block_length, seconds, 
	  useconds) < 0) {
    delete[] buf;
    return 0;
  }

  // Check the byte count to ensure that all blocks were received.
  if((gxUINT32)bytes_read != requested_block_header.block_length) {
    delete[] buf;
    return 0; 
  }

  return (void *)(buf); // Return the requested block
}

int gxStream::DeleteBlock(gxsSocket_t s, const void *request,
			  gxBlockHeader &request_header)
// Request the a block be deleted on the remote end. Returns
// zero if no error occur.
{
  SetBlockStatus(request_header, gxDeleteRemoteBlock);

  // Write the delete request and the request itself
  if(WriteBlock(s, request, request_header) != gxSOCKET_NO_ERROR)
    return socket_error;

  return socket_error = gxSOCKET_NO_ERROR;    
}

int gxStream::AddBlock(gxsSocket_t s, const void *block,
		       gxBlockHeader &block_header)
// Request that this block be added. Returns zero if no error 
// occur.
{
  SetBlockStatus(block_header, gxAddRemoteBlock);

  // Write the add request and then the block to add
  if(WriteBlock(s, block, block_header) != gxSOCKET_NO_ERROR)
    return socket_error;

  return socket_error = gxSOCKET_NO_ERROR;    
}

int gxStream::ChangeBlock(gxsSocket_t s, const void *request, 
			  const void *block, gxBlockHeader &request_header,
			  gxBlockHeader &block_header)
// Request that this block be changed. Returns zero if no error 
// occur.
{
  // Write the change request and the request itself
  SetBlockStatus(request_header, gxChangeRemoteBlock);
  if(WriteBlock(s, request, request_header) != gxSOCKET_NO_ERROR)
    return socket_error;

  // Write the changes
  SetBlockStatus(block_header, gxSendBlock);
  if(WriteBlock(s, block, block_header) != gxSOCKET_NO_ERROR)
    return socket_error;

  return socket_error = gxSOCKET_NO_ERROR;    
}

int gxStream::WriteAckBlock(gxsSocket_t s)
// Send an acknowledgment header to the specified socket.
// Returns zero if no errors occur.
{
  gxBlockHeader ack;
  SetBlockStatus(ack, gxAcknowledgeBlock);
  
  if(WriteHeader(s, ack) != gxSOCKET_NO_ERROR)
    return socket_error = gxSOCKET_BLOCKACK_ERROR;
  
  return socket_error = gxSOCKET_NO_ERROR;    
}

int gxStream::ReadAckBlock(gxsSocket_t s, int seconds, int useconds)
// Read an acknowledgment block from the specified socket.
// Returns zero if no errors occur.
{
  // Wait for an acknowledgment
  gxBlockHeader ack;
  if(ReadHeader(s, ack, seconds, useconds) != gxSOCKET_NO_ERROR)
    return socket_error = gxSOCKET_BLOCKACK_ERROR;

  // Read the status byte to determine what to do with this block
  __ULWORD__ block_status = ack.block_status;
  __SBYTE__ status = (__SBYTE__)((block_status & 0xFF00)>>8);
      
  if(status != gxAcknowledgeBlock)
    return socket_error = gxSOCKET_BLOCKACK_ERROR;

  return socket_error = gxSOCKET_NO_ERROR; 
}

int gxStream::TerminateConnection(gxsSocket_t s)
// Block command used to shutdown a server or client.
// Returns zero if no errors occur.
{
  gxBlockHeader gx;
  SetBlockStatus(gx, gxKillServer);

  if(WriteHeader(s, gx) != gxSOCKET_NO_ERROR) return socket_error;

  return socket_error = gxSOCKET_NO_ERROR;  
}

int gxStream::CloseConnection(gxsSocket_t s)
// Block command used to close a persistent client or
// server connection. Returns zero if no errors occur.
{
  gxBlockHeader gx;
  SetBlockStatus(gx, gxCloseConnection);

  if(WriteHeader(s, gx) != gxSOCKET_NO_ERROR) return socket_error;

  return socket_error = gxSOCKET_NO_ERROR;  
}

int gxStream::PortNumber(int &port)
// Pass back the port number actually set by the system
// in the "port" variable.
{
  if(GetSockName() < 0) {
    return socket_error;
  }
  port = GetPortNumber();
  return socket_error = gxSOCKET_NO_ERROR;
}

int gxStream::RemotePortNumber(int &port)
// Pass back the port number actually set by the system
// in the "port" variable.
{
  port = GetRemotePortNumber();
  return socket_error = gxSOCKET_NO_ERROR;
}

int gxStream::HostName(char *hs)
{
  if(GetHostName(hs) < 0) {
    return socket_error;
  }
  return socket_error = gxSOCKET_NO_ERROR;
}

int gxStream::RemoteHostName(char *hs)
{
  if(GetRemoteHostName(hs) < 0) {
    return socket_error;
  }
  return socket_error = gxSOCKET_NO_ERROR;
}

int gxStream::WriteBlock(const void *buf, int bytes)
{
  return WriteBlock(gxsocket, buf, bytes);
}

int gxStream::TerminateConnection()
{
  return TerminateConnection(gxsocket);
}

int gxStream::CloseConnection()
{
  return CloseConnection(gxsocket);
}

int gxStream::ReadRemoteBlock(void *buf, gxBlockHeader &gx)
{
  return ReadBlock(remote_socket, buf, gx, time_out_sec, time_out_usec);
}

int gxStream::ReadRemoteBlock(void *buf, int buf_size, gxBlockHeader &gx)
// 10/31/2026: Reads the block only if it fits in buf_size bytes
{
  return ReadBlock(remote_socket, buf, buf_size, gx, 
		   time_out_sec, time_out_usec);
}

int gxStream::ReadClientHeader(gxBlockHeader &gx)
{
  return ReadHeader(remote_socket, gx, time_out_sec, time_out_usec);
}

int gxStream::WriteRemoteBlock(void *buf, int bytes)
{
  return WriteBlock(remote_socket, buf, bytes);
}

void *gxStream::RequestBlock(const void *request,
			     gxBlockHeader &request_header, 
			     gxBlockHeader &requested_block_header)
{
  return RequestBlock(gxsocket, request, request_header,
		      requested_block_header, time_out_sec, time_out_usec);
}

int gxStream::DeleteBlock(const void *request, gxBlockHeader &request_header)
{
  return DeleteBlock(gxsocket, request, request_header);
}

int gxStream::WriteAckBlock()
{
  return WriteAckBlock(gxsocket);
}

int gxStream::WriteRemoteAckBlock()
{
  return WriteAckBlock(remote_socket);
}

int gxStream::ReadAckBlock()
{
  return ReadAckBlock(gxsocket, time_out_sec, time_out_usec);
}

int gxStream::ReadRemoteAckBlock()
{
  return ReadAckBlock(remote_socket, time_out_sec, time_out_usec);
}

int gxStream::AddBlock(const void *block, gxBlockHeader &block_header)
{
  return AddBlock(gxsocket, block, block_header);
}

int gxStream::ChangeBlock(const void *request, const void *block, 
			  gxBlockHeader &request_header, 
			  gxBlockHeader &block_header)
{
  return ChangeBlock(gxsocket, request, block, request_header, block_header);
}

int gxStream::RestartConnection()
{
  return RestartConnection(gxsocket);
}

int gxStream::RestartConnection(gxsSocket_t s)
// Block command used to restart a server or client.
// Returns zero if no errors occur.
{
  gxBlockHeader gx;
  SetBlockStatus(gx, gxRestartServer);

  if(WriteHeader(s, gx) != gxSOCKET_NO_ERROR) return socket_error;

  return socket_error = gxSOCKET_NO_ERROR;  
}

int gxStream::WriteResultBlock(gxsSocket_t s, int result_code)
{
  return WriteResultBlock64(s, (FAU_t)result_code);
}

int gxStream::WriteResultBlock64(gxsSocket_t s, FAU_t result_code)
// 10/31/2026: The header's block_nd_fptr field is as wide as FAU, so
// a 64-bit file address travels whole (the int version truncates it)
{
  gxBlockHeader ack;
  SetBlockStatus(ack, gxAcknowledgeBlock);
  
  // Use the header's length field to store the result
  ack.block_nd_fptr = (FAU)result_code;

  if(WriteHeader(s, ack) != gxSOCKET_NO_ERROR)
    return socket_error = gxSOCKET_BLOCKACK_ERROR;
  
  return socket_error = gxSOCKET_NO_ERROR;    
}

int gxStream::ReadResultBlock(gxsSocket_t s, int &result_code,
			      int seconds, int useconds)
{
  FAU_t code = (FAU_t)0;
  if(ReadResultBlock64(s, code, seconds, useconds) != gxSOCKET_NO_ERROR) {
    return socket_error;
  }
  result_code = (int)code;
  return socket_error = gxSOCKET_NO_ERROR;
}

int gxStream::ReadResultBlock64(gxsSocket_t s, FAU_t &result_code,
				int seconds, int useconds)
// 10/31/2026: Full width result, see WriteResultBlock64()
{
  // Wait for an acknowledgment
  gxBlockHeader ack;
  if(ReadHeader(s, ack, seconds, useconds) != gxSOCKET_NO_ERROR) {
    return socket_error = gxSOCKET_BLOCKACK_ERROR;
  }

  // Read the status byte to determine what to do with this block
  __ULWORD__ block_status = ack.block_status;
  __SBYTE__ status = (__SBYTE__)((block_status & 0xFF00)>>8);
      
  if(status != gxAcknowledgeBlock) {
    return socket_error = gxSOCKET_BLOCKACK_ERROR;
  }

  // Read the result code stored in the header;s length field
  result_code = (FAU_t)ack.block_nd_fptr;

  return socket_error = gxSOCKET_NO_ERROR; 
}

int gxStream::WriteResultBlock(int result_code)
{
  return WriteResultBlock(gxsocket, result_code);
}

int gxStream::WriteRemoteResultBlock(int result_code)
{
  return WriteResultBlock(remote_socket, result_code);
}

int gxStream::ReadResultBlock(int &result_code)
{
  return ReadResultBlock(gxsocket, result_code,
			 time_out_sec, time_out_usec);
}

int gxStream::ReadRemoteResultBlock(int &result_code)
{
  return ReadResultBlock(remote_socket, result_code,
			 time_out_sec, time_out_usec);
}

// 10/31/2026: 64-bit results
int gxStream::WriteResultBlock64(FAU_t result_code)
{
  return WriteResultBlock64(gxsocket, result_code);
}

int gxStream::WriteRemoteResultBlock64(FAU_t result_code)
{
  return WriteResultBlock64(remote_socket, result_code);
}

int gxStream::ReadResultBlock64(FAU_t &result_code)
{
  return ReadResultBlock64(gxsocket, result_code,
			   time_out_sec, time_out_usec);
}

int gxStream::ReadRemoteResultBlock64(FAU_t &result_code)
{
  return ReadResultBlock64(remote_socket, result_code,
			   time_out_sec, time_out_usec);
}

// --------------------------------------------------------------
// 10/31/2026: TLS mode and client authentication
// --------------------------------------------------------------
gxStream::~gxStream()
{
  Close();
  if(tls) delete tls;
  tls = 0;
}

gxStreamTLS *gxStream::tls_state()
{
  if(!tls) tls = new gxStreamTLS;
  return tls;
}

int gxStream::tls_fail(const char *msg)
// Record why TLS or a client check failed. Returns gxSOCKET_SSL_ERROR.
{
  tls_state()->error = msg ? msg : "";
  return socket_error = gxSOCKET_SSL_ERROR;
}

const char *gxStream::TLSError()
{
  return tls ? tls->error.c_str() : "";
}

int gxStream::SetServerCert(const char *cert_file, const char *key_file,
			    const char *passphrase)
{
#if defined (__USE_GX_SSL_EXTENSIONS__)
  if(!cert_file || !*cert_file || !key_file || !*key_file) {
    return tls_fail("SetServerCert(): certificate and key files are needed");
  }
  gxStreamTLS *t = tls_state();
  t->server_cert = cert_file;
  t->server_key = key_file;
  t->server_pass = passphrase ? passphrase : "";
  return gxSOCKET_NO_ERROR;
#else
  (void)cert_file; (void)key_file; (void)passphrase;
  return tls_fail("This DataReel library was built without SSL");
#endif
}

void gxStream::SetClientCAFile(const char *fname)
{
  tls_state()->client_ca_file = fname ? fname : "";
}

int gxStream::AllowClientFingerprint(const char *fp)
{
  gxString n = gxs_normalize_fingerprint(fp);
  if(n.is_null()) {
    tls_fail("Not a SHA-256 certificate fingerprint");
    return -1;
  }
  gxStreamTLS *t = tls_state();
  if(!gxs_in_list(t->allow_fp, n)) t->allow_fp.push_back(n);
  return 0;
}

int gxStream::AllowClientCN(const char *cn)
{
  if(!cn || !*cn) {
    tls_fail("Empty client certificate name");
    return -1;
  }
  gxString n(cn);
  gxStreamTLS *t = tls_state();
  if(!gxs_in_list(t->allow_cn, n)) t->allow_cn.push_back(n);
  return 0;
}

void gxStream::ClearClientAllowList()
{
  if(!tls) return;
  tls->allow_fp.clear();
  tls->allow_cn.clear();
}

void gxStream::AllowAnonymousClients(bool allow)
{
  tls_state()->allow_anonymous = allow ? 1 : 0;
}

int gxStream::AnonymousClientsAllowed()
{
  return tls ? tls->allow_anonymous : 0;
}

void gxStream::UseTLS(int on)
{
  tls_state()->use_tls = on ? 1 : 0;
}

int gxStream::SetClientCert(const char *cert_file, const char *key_file,
			    const char *passphrase)
{
#if defined (__USE_GX_SSL_EXTENSIONS__)
  if(!cert_file || !*cert_file || !key_file || !*key_file) {
    return tls_fail("SetClientCert(): certificate and key files are needed");
  }
  gxStreamTLS *t = tls_state();
  t->client_cert = cert_file;
  t->client_key = key_file;
  t->client_pass = passphrase ? passphrase : "";
  t->use_tls = 1;
  return gxSOCKET_NO_ERROR;
#else
  (void)cert_file; (void)key_file; (void)passphrase;
  return tls_fail("This DataReel library was built without SSL");
#endif
}

void gxStream::SetServerCAFile(const char *fname)
{
  tls_state()->server_ca_file = fname ? fname : "";
}

void gxStream::SetServerName(const char *name)
{
  tls_state()->server_name = name ? name : "";
}

void gxStream::SetVerifyServer(int on)
{
  tls_state()->verify_server = on ? 1 : 0;
}

void gxStream::SetHandshakeTimeout(int seconds)
{
  tls_state()->handshake_timeout = seconds;
}

int gxStream::IsTLS()
{
  return tls && tls->tls_mode();
}

int gxStream::IsSecure()
{
#if defined (__USE_GX_SSL_EXTENSIONS__)
  return tls && tls->client_conn && gxsocket != (gxsSocket_t)-1 &&
    tls->client_fd == gxsocket;
#else
  return 0;
#endif
}

int gxStream::IsRemoteSecure()
{
#if defined (__USE_GX_SSL_EXTENSIONS__)
  return tls && tls->remote_conn && remote_socket != (gxsSocket_t)-1 &&
    tls->remote_fd == remote_socket;
#else
  return 0;
#endif
}

int gxStream::ClientAuthenticated()
{
  return tls && !tls->peer_fp.is_null();
}

const char *gxStream::PeerFingerprint()
{
  return tls ? tls->peer_fp.c_str() : "";
}

const char *gxStream::PeerCN()
{
  return tls ? tls->peer_cn.c_str() : "";
}

int gxStream::check_server_policy()
// Returns 0 if this server authenticates its clients or has opted out
{
  gxStreamTLS *t = tls;
  int have_allow = t && (!t->allow_fp.empty() || !t->allow_cn.empty());
  int have_ca = t && !t->client_ca_file.is_null();
  int server_tls = t && t->server_tls();
  if(have_allow && !have_ca) {
    return tls_fail("gxStream server: a client allow list needs "
		    "SetClientCAFile()");
  }
  if(have_ca && !server_tls) {
    return tls_fail("gxStream server: client certificates need TLS, "
		    "call SetServerCert()");
  }
  if(t && t->allow_anonymous) return gxSOCKET_NO_ERROR;
  if(server_tls && have_ca && have_allow) return gxSOCKET_NO_ERROR;
  return tls_fail("gxStream server refused to run: clients are not "
		  "authenticated. Call SetServerCert(), SetClientCAFile() "
		  "and AllowClientFingerprint() or AllowClientCN(), or "
		  "AllowAnonymousClients(true) for an open server");
}

gxsSocket_t gxStream::Accept()
{
  if(check_server_policy() != gxSOCKET_NO_ERROR) return -1;
  if(tls && tls->server_tls()) {
#if defined (__USE_GX_SSL_EXTENSIONS__)
    if(!tls->listen_ctx) {
      // The listener was set up without StreamServer()
      gxSSL *c = new gxSSL;
      c->MakeServer();
      if(!tls->client_ca_file.is_null()) {
	c->SetClientCAList(tls->client_ca_file.c_str());
      }
      if(!c->InitCTX(tls->server_cert.c_str(), tls->server_key.c_str(),
		     tls->server_pass.c_str())) {
	tls_fail(c->SSLExceptionMessage());
	delete c;
	return -1;
      }
      tls->listen_ctx = c;
    }
#endif
  }
  // In TLS mode the TLS connection belongs to remote_socket, so a client
  // accepted before is closed (use AcceptClient() for one stream per
  // client). Plain TCP servers keep the 6.2 behaviour: the old socket
  // is left to the caller.
  if(tls && tls->server_tls()) CloseRemoteSocket();
  if(gxSocket::Accept() < 0) return -1;
  if(tls && tls->server_tls()) {
    if(tls_accept(remote_socket, tls, tls) != gxSOCKET_NO_ERROR) {
      gxSocket::CloseRemoteSocket(); // Fail closed
      return -1;
    }
  }
  return remote_socket;
}

int gxStream::AcceptClient(gxStream &client)
{
  if(&client == this) {
    if(Accept() < 0) return socket_error;
    return socket_error = gxSOCKET_NO_ERROR;
  }
  if(check_server_policy() != gxSOCKET_NO_ERROR) return socket_error;
  if(tls && tls->server_tls()) {
#if defined (__USE_GX_SSL_EXTENSIONS__)
    if(!tls->listen_ctx) {
      gxSSL *c = new gxSSL;
      c->MakeServer();
      if(!tls->client_ca_file.is_null()) {
	c->SetClientCAList(tls->client_ca_file.c_str());
      }
      if(!c->InitCTX(tls->server_cert.c_str(), tls->server_key.c_str(),
		     tls->server_pass.c_str())) {
	tls_fail(c->SSLExceptionMessage());
	delete c;
	return socket_error;
      }
      tls->listen_ctx = c;
    }
#endif
  }
  client.CloseRemoteSocket();
  gxsSocket_t s = gxSocket::Accept(gxsocket, &client.remote_sin);
  if(s == (gxsSocket_t)-1) return socket_error = gxSOCKET_ACCEPT_ERROR;
  client.remote_socket = s;
  client.SetTimeOut(time_out_sec, time_out_usec);
  client.SetMaxBlockSize(max_block);
  if(tls && tls->server_tls()) {
    client.tls_state()->accepted_tls = 1; // Fail closed from now on
    if(client.tls_accept(s, client.tls_state(), tls) != gxSOCKET_NO_ERROR) {
      tls_fail(client.TLSError());
      client.gxSocket::CloseRemoteSocket(); // Fail closed
      return socket_error;
    }
  }
  return socket_error = gxSOCKET_NO_ERROR;
}

int gxStream::tls_accept(gxsSocket_t s, gxStreamTLS *st, gxStreamTLS *cfg)
// Run the server side handshake on s with the settings of cfg (the
// listening stream) and keep the connection in st. Returns 0 if the
// client is accepted.
{
  st->peer_fp.Clear();
  st->peer_cn.Clear();
#if defined (__USE_GX_SSL_EXTENSIONS__)
  gxs_free_conn(st->remote_conn, 0, st->remote_fd, 0);
  st->remote_fd = (gxsSocket_t)-1;
  if(!cfg->listen_ctx) return tls_fail("No server TLS context");

  char who[gxsMAX_NAME_LEN];
  memset(who, 0, sizeof(who));
  if(GetRemoteHostName(who) != 0) strcpy(who, "UNKNOWN");
  gxString msg;

  gxSSL *c = new gxSSL;
  c->MakeServer();
  c->SetCTX(cfg->listen_ctx->GetCTX());
  c->SetHandshakeTimeout(cfg->handshake_timeout);
  if(c->OpenSSLSocket(s) != gxSSL_NO_ERROR ||
     c->AcceptSSLSocket() != gxSSL_NO_ERROR) {
    msg << clear << "TLS handshake with client " << who << " failed: "
	<< c->SSLExceptionMessage();
    gxs_free_conn(c, 0, s, 0);
    return tls_fail(msg.c_str());
  }

  if(!cfg->client_ca_file.is_null()) {
    long code = 0;
    int rv = c->VerifyClientCert(&code);
    if(rv != gxSSL_NO_ERROR) {
      msg << clear << "Client " << who;
      if(rv == gxSSL_PEERCERT_ERROR) {
	msg << " sent no client certificate";
      }
      else {
	msg << " client certificate check failed: " 
	    << c->SSLExceptionMessage();
	if(code != 0) {
	  msg << " (" << gxSSL::CertVerifyErrorString(code) << ")";
	}
      }
      gxs_free_conn(c, 0, s, 1);
      return tls_fail(msg.c_str());
    }
    gxString fp, cn;
    if(c->GetPeerCertIdentity(fp, cn) != gxSSL_NO_ERROR || fp.is_null()) {
      msg << clear << "Client " << who 
	  << " certificate fingerprint could not be read";
      gxs_free_conn(c, 0, s, 1);
      return tls_fail(msg.c_str());
    }
    int allowed = cfg->allow_fp.empty() && cfg->allow_cn.empty();
    if(!allowed) allowed = gxs_in_list(cfg->allow_fp, fp);
    if(!allowed && !cn.is_null()) allowed = gxs_in_list(cfg->allow_cn, cn);
    if(!allowed) {
      msg << clear << "Client " << who 
	  << " certificate is not in the allow list (CN=" 
	  << gxs_printable(cn) << ", SHA-256 " << fp << ")";
      gxs_free_conn(c, 0, s, 1);
      return tls_fail(msg.c_str());
    }
    st->peer_fp = fp;
    st->peer_cn = cn;
  }
  st->remote_conn = c;
  st->remote_fd = s;
  return socket_error = gxSOCKET_NO_ERROR;
#else
  (void)s; (void)cfg;
  return tls_fail("This DataReel library was built without SSL");
#endif
}

int gxStream::tls_connect()
// Client side handshake on gxsocket. Closes the socket on a failure.
{
#if defined (__USE_GX_SSL_EXTENSIONS__)
  gxStreamTLS *t = tls_state();
  gxs_free_conn(t->client_conn, 1, t->client_fd, 0);
  t->client_fd = (gxsSocket_t)-1;
  gxString msg;
  gxSSL *c = new gxSSL;
  c->MakeClient();
  if(!t->client_cert.is_null()) {
    c->SetCertFile(t->client_cert.c_str());
    c->SetKeyFile(t->client_key.c_str());
    if(!t->client_pass.is_null()) c->SetPasswd(t->client_pass.c_str());
  }
  if(!t->server_ca_file.is_null()) c->SetCAList(t->server_ca_file.c_str());
  c->SetHandshakeTimeout(t->handshake_timeout);
  int ok = c->InitCTX() != 0;
  if(ok && t->verify_server && t->server_ca_file.is_null()) {
    ok = c->SetDefaultCAPaths() == gxSSL_NO_ERROR;
  }
  if(ok && !t->server_name.is_null()) c->SetHostName(t->server_name.c_str());
  if(!ok || c->OpenSSLSocket(gxsocket) != gxSSL_NO_ERROR ||
     c->ConnectSSLSocket() != gxSSL_NO_ERROR) {
    msg << clear << "TLS connection failed: " << c->SSLExceptionMessage();
    delete c;
    gxSocket::Close(); // Fail closed
    return tls_fail(msg.c_str());
  }
  if(t->verify_server) {
    long code = 0;
    const char *name = t->server_name.c_str();
    if(t->server_name.is_null() || 
       c->VerifyCert(name, &code) != gxSSL_NO_ERROR) {
      msg << clear << "Server certificate check failed: " 
	  << c->SSLExceptionMessage();
      if(code != 0) {
	msg << " (" << gxSSL::CertVerifyErrorString(code) << ")";
      }
      msg << " for " << name;
      c->CloseSSLSocket(0);
      delete c;
      gxSocket::Close(); // Fail closed
      return tls_fail(msg.c_str());
    }
  }
  t->client_conn = c;
  t->client_fd = gxsocket;
  return socket_error = gxSOCKET_NO_ERROR;
#else
  gxSocket::Close();
  return tls_fail("This DataReel library was built without SSL");
#endif
}

int gxStream::stream_send(gxsSocket_t s, const void *buf, int bytes)
// Send all bytes on s: on its TLS connection in TLS mode, or plain.
// Returns the number of bytes sent or -1.
{
  if(!tls || !tls->tls_mode()) {
    return Send(s, buf, bytes);
  }
#if defined (__USE_GX_SSL_EXTENSIONS__)
  gxSSL *c = 0;
  if(s != (gxsSocket_t)-1) {
    if(s == gxsocket && tls->client_conn && tls->client_fd == s) {
      c = tls->client_conn;
    }
    else if(s == remote_socket && tls->remote_conn && tls->remote_fd == s) {
      c = tls->remote_conn;
    }
  }
  if(c) {
    bytes_moved = 0;
    if(bytes <= 0) return 0;
    int n = c->SendSSLSocket(buf, bytes);
    if(n != bytes) {
      bytes_moved = n > 0 ? n : 0;
      socket_error = gxSOCKET_TRANSMIT_ERROR;
      return -1;
    }
    bytes_moved = n;
    return n;
  }
#endif
  // 10/31/2026: In TLS mode nothing is sent in cleartext
  tls_fail("Not a TLS connection");
  return -1;
}

int gxStream::stream_recv(gxsSocket_t s, void *buf, int bytes, int seconds,
			  int useconds)
// Receive bytes from s: from its TLS connection in TLS mode, or plain.
// Returns the number of bytes read (fewer if the connection closed or
// timed out after some were read) or -1.
{
  if(!tls || !tls->tls_mode()) {
    return Recv(s, buf, bytes, seconds, useconds);
  }
#if defined (__USE_GX_SSL_EXTENSIONS__)
  gxSSL *c = 0;
  if(s != (gxsSocket_t)-1) {
    if(s == gxsocket && tls->client_conn && tls->client_fd == s) {
      c = tls->client_conn;
    }
    else if(s == remote_socket && tls->remote_conn && tls->remote_fd == s) {
      c = tls->remote_conn;
    }
  }
  if(c) {
    bytes_read = 0;
    if(bytes <= 0) return 0;
    int n = c->RecvSSLSocket(buf, bytes, seconds, useconds);
    if(n > 0) {
      bytes_read = n;
      if(n < bytes) socket_error = gxSOCKET_DISCONNECTED_ERROR;
      return n;
    }
    if(n == 0) {
      socket_error = gxSOCKET_DISCONNECTED_ERROR;
    }
    else if(c->GetSSLError() == gxSSL_REQUEST_TIMEOUT) {
      socket_error = gxSOCKET_REQUEST_TIMEOUT;
    }
    else {
      socket_error = gxSOCKET_RECEIVE_ERROR;
    }
    return -1;
  }
#endif
  // 10/31/2026: In TLS mode nothing is read in cleartext
  tls_fail("Not a TLS connection");
  return -1;
}

void gxStream::Close()
{
#if defined (__USE_GX_SSL_EXTENSIONS__)
  if(tls) {
    gxs_free_conn(tls->client_conn, 1, tls->client_fd, 
		  tls->client_fd == gxsocket);
    gxs_free_conn(tls->remote_conn, 0, tls->remote_fd, 
		  tls->remote_fd == remote_socket);
    tls->client_fd = tls->remote_fd = (gxsSocket_t)-1;
  }
#endif
  if(tls) {
    tls->peer_fp.Clear();
    tls->peer_cn.Clear();
  }
  gxSocket::Close();
}

void gxStream::CloseRemoteSocket()
{
#if defined (__USE_GX_SSL_EXTENSIONS__)
  if(tls) {
    gxs_free_conn(tls->remote_conn, 0, tls->remote_fd, 
		  tls->remote_fd == remote_socket);
    tls->remote_fd = (gxsSocket_t)-1;
  }
#endif
  if(tls) {
    tls->peer_fp.Clear();
    tls->peer_cn.Clear();
  }
  gxSocket::CloseRemoteSocket();
}

#ifdef __BCC32__
#pragma warn .8080
#endif
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
