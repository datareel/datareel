// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxssl.h
// C++ Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 10/17/2001
// Date Last Modified: 10/24/2026
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

The gxSSL class is an OpenSSL wrapper used to support SSL and TLS 
socket applications. The gxSSL class requires openssl-0.9.8a or higher. 
Some of the class functions and supporting code was derived from the 
OpenSSL implementation described in the following book: "SSL and TLS 
Designing and Building Secure Systems" written by Eric Rescorla, 
published by Addison Wesley, 2001. Further adaptations were derived 
from the OpenSSL documentation and example programs. In order to build 
an application using the gxSSL class you have to download the latest 
version of OpenSSL from http://www.openssl.org and build the third party 
library in accordance with the documentation supplied with the 
distribution. NOTE: Any third party libraries used to build DataReel 
applications are also subject to the licensing agreement stated within 
the source code and documentation supplied with the third party library.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "gxssl.h"
#include "strutil.h"

using namespace datareel;

// Initialize all static variables and data members
int gxSSL::lib_init = 0;

// NOTE: This array must contain the same number of exceptions as the
// gxSSLError enumeration. 
const int gxsMaxOpenSSLExceptionMessages = 71;
const char *gxsOpenSSLExceptionMessages[gxsMaxOpenSSLExceptionMessages] = {
  "SSL exception: No errors reported",                 // NO_ERROR
  "SSL exception: Invalid exception code",             // INVALID_ERROR_CODE 
  "SSL exception: Error accepting remote socket",      // ACCEPT_ERROR
  "SSL exception: AES crypto error",                   // AES_ERROR
  "SSL exception: Error generating AES key",           // AESGEN_ERROR
  "SSL exception: Error processing AES key",           // AESKEY_ERROR
  "SSL exception: ASN.1 error",                        // ASN1_ERROR
  "SSL exception: Error processing ASN.1 cert",        // ASN1_CERT_ERROR
  "SSL exception: Error processing ASN.1 cert req",    // ASN1_REQ_ERROR
  "SSL exception: BIO I/O error occurred",             // BIO_ERROR
  "SSL exception: Could not bind socket",              // BIND_ERROR
  "SSL exception: Blowfish crypto error",              // BLOWFISH_ERROR
  "SSL exception: Error generating blowfish key",      // BLOWFISHGEN_ERROR
  "SSL exception: Error processing blowfish key",      // BLOWFISHKEY_ERROR
  "SSL exception: Buffer overflow error",              // BUFOVER_ERROR
  "SSL exception: Error processing CA list",           // CALIST_ERROR,
  "SSL exception: Error processing certificate",       // CERT_ERROR,
  "SSL exception: Cannot verify certificate",          // CERT_VER_ERROR
  "SSL exception: TLS/SSL connection has been closed", // CLOSE_ERROR
  "SSL exception: Could not connect socket",           // CONNECT_ERROR
  "SSL exception: Context object not initialized",     // CTX_INIT_ERROR
  "SSL exception: DES crypto error",                   // DES_ERROR
  "SSL exception: Error processing DH parameters",     // DHPARMS_ERROR
  "SSL exception: Error generating DH parameters",     // DHPARMS_GEN_ERROR
  "SSL exception: Socket has been disconnected",       // DISCONNECTED_ERROR
  "SSL exception: DSA crypto error",                   // DSA_ERROR
  "SSL exception: Error generating DSA key",           // DSAGEN_ERROR
  "SSL exception: Error processing DSA key",           // DSAKEY_ERROR
  "SSL exception: File BIO not initialized",           // FBIO_INIT_ERROR
  "SSL exception: File length does not match expected value", // FILELENGTH
  "SSL exception: File modification time dates do not match", // FILEMODTIME
  "SSL exception: A file system error occurred",       // FILESYSTEM_ERROR
  "SSL exception: Could not resolve hostname",         // HOSTNAME_ERROR
  "SSL exception: Initialization error",               // INIT_ERROR
  "SSL exception: Kerberos crypto error",              // KRB_ERROR
  "SSL exception: Library error occurred",             // LIB_ERROR
  "SSL exception: Listen error",                       // LISTEN_ERROR
  "SSL exception: Error processing private key",       // KEY_ERROR
  "SSL exception: MAC crypto error",                   // MAC_ERROR
  "SSL exception: MD5 hash error",                     // MD5_ERROR
  "SSL exception: Memory BIO not initialized",         // MBIO_INIT_ERROR
  "SSL exception: Accessing a null pointer",           // NULL_PTR_ERROR
  "SSL exception: Error processing password",          // PASSWD_ERROR
  "SSL exception: Certificate was not issued for this host", // PEERCN_ERROR
  "SSL exception: Peer did not present certificate",   // PEERCERT_ERROR
  "SSL exception: Pseudo random number error",         // PRNG_ERROR   
  "SSL exception: Error setting requested protocol",   // PROTOCOL_ERROR
  "SSL exception: Error processing public key",        // PUBKEY_ERROR
  "SSL exception: Read operation did not complete",    // READ_ERROR
  "SSL exception: Receive error",                      // RECEIVE_ERROR
  "SSL exception: Request timed out",                  // REQUEST_TIMEOUT
  "SSL exception: RSA crypto error",                   // RSA_ERROR
  "SSL exception: Error generating RSA key",           // RSAGEN_ERROR
  "SSL exception: Error processing RSA key",           // RSAKEY_ERROR
  "SSL exception: Socket BIO not initialized",         // SBIO_INIT_ERROR
  "SSL exception: SHA crypto error",                   // SHA_ERROR
  "SSL exception: SSL shutdown failed",                // SHUTDOWN_ERROR
  "SSL exception: SSL object not initialized",         // SSL_INIT_ERROR
  "SSL exception: Transmit error",                     // TRANSMIT_ERROR
  "SSL exception: Write operation did not complete",   // WRITE_ERROR
  "SSL exception: X509 error",                         // X509_ERROR
  "SSL exception: Error processing X509 cert",         // X509_CERT_ERROR
  "SSL exception: Error generating X509 cert",         // X509_GEN_ERROR
  "SSL exception: X509 lookup did not complete",       // X509_LOOKUP_ERROR
  "SSL exception: Error processing X509 cert req",     // X509_REQ_ERROR
  "SSL exception: Error generating X509 cert req",     // X509_REQGEN_ERROR

  // Additional SSL and crypto error codes 

  // Exception codes for database block errors
  "SSL exception: Database block acknowledgment error",  // BLOCKACK
  "SSL exception: Bad database block header",            // BLOCKHEADER
  "SSL exception: Bad database block size",              // BLOCKSIZE
  "SSL exception: Database block synchronization error", // BLOCKSYNC

  // Exception code used to allow application to set error string
  "SSL exception: Application error" // APPDEF_ERROR
};

void gxCertIssuerInfo::ResetCert() 
{
  C.clear();
  ST.clear();
  L.clear();
  O.clear();
  OU.clear();
  CN.clear();
  sn = 0;
  days = 365;
  passwd.clear();
  company_name.clear();
  email_address.clear();
  basic_constraints.clear();
  key_usage.clear();
  subject_key_identifier.clear();
  netscape_cert_type.clear();
  netscape_comment.clear();
}

void gxCertIssuerInfo::CopyCert(gxCertIssuerInfo &ob) {
  C = ob.C;
  ST = ob.ST;
  L = ob.L;
  O = ob.O;
  OU = ob.OU;
  CN = ob.CN;
  sn = ob.sn;
  days = ob.days;
  passwd= ob.passwd;
  company_name = ob.company_name;
  email_address = ob.email_address;
  basic_constraints = ob.basic_constraints;
  key_usage = ob.key_usage;
  subject_key_identifier = ob.subject_key_identifier;
  netscape_cert_type = ob.netscape_cert_type;
  netscape_comment = ob.netscape_comment;
}

#if !defined (__USE_GX_SCHANNEL__)
// --------------------------------------------------------------
// OpenSSL version of the gxSSL class
// --------------------------------------------------------------
int password_cb2(char *buf,int num, int /* rwflag */, void *userdata)
// Password callback function.
{
  gxSSL *openssl = (gxSSL *)userdata;

  if(openssl->passwd.empty()) return 0;
  if(num < (int)(openssl->passwd.length()+1)) return 0;
  memset(buf, 0, num);
  memmove(buf, openssl->passwd.c_str(), openssl->passwd.length());
  return (int)openssl->passwd.length();
}


gxSSL::~gxSSL()
{
  if(ssl) {
    SSL_shutdown(ssl);
    DestroySSL();    
  }
  DestroyCTX();
}

int gxSSL::SetAppError(const char *s) 
// Function used to set application error strings.
{
  app_err_str = str_cat(s);
  return ssl_error = gxSSL_APPDEF_ERROR;
}

void gxSSL::GlobalSSLError(std::string &err_str)
// Passes back the global SSL error string in the
// err_str variable.
{
  char sbuf[1024];
  memset(sbuf, 0, 1024);
  ERR_error_string(ERR_get_error(), sbuf);
  err_str = str_cat("SSL global exception: ", sbuf);
}

int gxSSL::OpenSSLSocket(gxsSocket_t s)
// Function used to open an SSL socket.
// Returns 0 if successful. 
// Returns a non-zero value if the function fails.
{
  if(!ctx) {
    if(!InitCTX()) return ssl_error; 
  }

  ssl = SSL_new(ctx);
  if(is_ssl_null()) return ssl_error;

  socket_bio = BIO_new_socket(s, BIO_NOCLOSE);
  if(is_socket_bio_null()) {
    DestroySSL();
    return ssl_error = gxSSL_BIO_ERROR;
  }
  SSL_set_bio(ssl, socket_bio, socket_bio);

  return ssl_error = gxSSL_NO_ERROR;
}

int gxSSL::ConnectSSLSocket()
// Function used to connect an SSL socket.
// Returns 0 if successful. 
// Returns a non-zero value if the function fails.
{
  if(is_ssl_null()) return ssl_error;
  // 10/24/2026: Send the server name (SNI) if the caller set one
  if(!host_name.empty()) {
    SSL_set_tlsext_host_name(ssl, host_name.c_str());
  }
  if(check_ssl_error(SSL_connect(ssl)) != SSL_ERROR_NONE) {
    return ssl_error;
  }
  return ssl_error = gxSSL_NO_ERROR;
}

int gxSSL::AcceptSSLSocket()
// Function used to accept SSL client connections.
// Returns 0 if successful. 
// Returns a non-zero value if the function fails.
{
  if(is_ssl_null()) return ssl_error;
  if(SSL_accept(ssl) <= 0) {
    return ssl_error = gxSSL_ACCEPT_ERROR;
  }
  return ssl_error = gxSSL_NO_ERROR;
}

int gxSSL::CloseSSLSocket(int destroy_ssl)
// Function used to close an SSL socket connection.
// Returns 0 if successful. 
// Returns a non-zero value if the function fails.
{
  if(!ssl) return ssl_error;
  int rv = SSL_shutdown(ssl);
  if(!rv) {
    int open_socket = -1;
    BIO_get_fd(SSL_get_rbio(ssl), &open_socket);
    if(open_socket >= 0) shutdown(open_socket, 1);
    rv = SSL_shutdown(ssl);
  }
  if(rv != 1) {
    return ssl_error = gxSSL_SHUTDOWN_ERROR;
  }
  if(destroy_ssl) DestroySSL();
  return ssl_error = gxSSL_NO_ERROR;
}

int gxSSL::RawWriteSSLSocket(const void *buf, int bytes)
// Send a block of data to the bound socket. NOTE: A raw write
// may return before all the bytes have been written.
// Returns the total number of bytes sent or -1 if an error occurs.
{
  bytes_moved = 0;
  if(is_ssl_null()) return -1;

  int rv = SSL_write(ssl, buf, bytes);
  int err = check_ssl_error(rv);
  if(err != SSL_ERROR_NONE) {
    // Signal to the caller that the connection has closed
    if(err == SSL_ERROR_ZERO_RETURN) {
      return 0;
    }
    return -1;
  }

  ssl_error = gxSSL_NO_ERROR;
  bytes_moved = rv;
  return rv;
}

int gxSSL::RawReadSSLSocket(void *buf, int bytes)
// Receive a block of data from a specified socket. NOTE: A
// raw read may return before all the bytes have been read.
// Returns the total number of bytes received or -1 if an 
// error occurs.
{
  bytes_read = 0; // Reset the byte counter
  if(is_ssl_null()) return -1;

  int rv = SSL_read(ssl, buf, bytes);
  int err = check_ssl_error(rv);
  if(err != SSL_ERROR_NONE) {
    // Signal to the caller that the connection has closed
    if(err == SSL_ERROR_ZERO_RETURN) {
      return 0;
    }
    return -1;
  }
  ssl_error = gxSSL_NO_ERROR;
  bytes_read = rv;
  return rv;
}

int gxSSL::ReadSomeSSLSocket(void *buf, int bytes, int timeout_ms)
// Return up to bytes of decrypted data, waiting at most timeout_ms
// (-1 waits forever). Returns the number of bytes read, 0 if the peer
// closed the connection, or -1 on an error or timeout.
{
  bytes_read = 0;
  if(is_ssl_null()) return -1;
  int fd = -1;
  BIO_get_fd(SSL_get_rbio(ssl), &fd);
  for(;;) {
    // Data already decrypted by OpenSSL does not make the socket readable
    if(timeout_ms >= 0 && SSL_pending(ssl) == 0) {
      int w = gxSocket::WaitReadable((gxsSocket_t)fd, timeout_ms);
      if(w == 0) { ssl_error = gxSSL_REQUEST_TIMEOUT; return -1; }
      if(w < 0) { ssl_error = gxSSL_RECEIVE_ERROR; return -1; }
    }
    int rv = SSL_read(ssl, buf, bytes);
    if(rv > 0) {
      ssl_error = gxSSL_NO_ERROR;
      bytes_read = rv;
      return rv;
    }
    int err = check_ssl_error(rv);
    if(err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
      continue; // A partial record or a TLS 1.3 ticket, read again
    }
    if(err == SSL_ERROR_ZERO_RETURN) return 0;
    return -1;
  }
}

int gxSSL::SendSSLSocket(const void *buf, int bytes)
// Send a block of data to the bound socket and do not return
// until all the bytes have been written. Returns the total number 
// of bytes sent or -1 if an error occurs.
{
  bytes_moved = 0; // Reset the byte counter
  if(is_ssl_null()) return -1;

  int num_moved = 0;     // Actual number of bytes written
  int num_req = bytes;   // Number of bytes requested 
  char *p = (char *)buf; // Pointer to the buffer

  while(bytes_moved < bytes) { // Loop until the buffer is empty
    if((num_moved = SSL_write(ssl, p, num_req-bytes_moved)) > 0) {
      bytes_moved += num_moved;  // Increment the byte counter
      p += num_moved;            // Move the buffer pointer for the next read
    }
    else {
      int err = check_ssl_error(num_moved);
      // Signal to the caller that the connection has closed
      if(err == SSL_ERROR_ZERO_RETURN) {
	if(bytes_moved == bytes) {
	  return bytes_moved;
	}
	else {
	  return 0;
	}
      }
      return -1;
    }
  }

  ssl_error = gxSSL_NO_ERROR;
  return bytes_moved;
}

int gxSSL::RecvSSLSocket(void *buf, int bytes)
// Receive a block of data from a specified socket and do not return
// until all the bytes have been read. Returns the total number of
// bytes received or -1 if an error occurs.
{
  return RecvSSLSocket(buf, bytes, 0, 0, 0);
}

int gxSSL::RecvSSLSocket(void *buf, int bytes, int seconds, int useconds)
// Receive a block of data from a specified socket and do not return
// until all the bytes have been read or the timeout value has been 
// exceeded. Returns the total number of bytes received or -1 if an 
// error occurs.
{
  return RecvSSLSocket(buf, bytes, 1, seconds, useconds);
}

int gxSSL::RecvSSLSocket(void *buf, int bytes, int useTimeout, int seconds,
			 int useconds)
// Receive a block of data from a specified socket and do not return
// until all the bytes have been read. Returns the total number of
// bytes received or -1 if an error occurs and 0 bytes read.
// CWS - optional timeout specified; may have 0 timeout to return immediately
// so added 'useTimeout' flag rather than checking for 0 seconds to indicate 
// no timeout.
{
  bytes_read = 0; // Reset the byte counter
  if(is_ssl_null()) return -1;

  int num_read = 0;      // Actual number of bytes read
  int num_req = bytes;   // Number of bytes requested 
  char *p = (char *)buf; // Pointer to the buffer

  int open_socket = -1;
  BIO_get_fd(SSL_get_rbio(ssl), &open_socket);
  int timeout_ms = seconds * 1000 + useconds / 1000;
  
  while(bytes_read < bytes) { // Loop until the buffer is full
    // Data already decrypted by OpenSSL does not make the socket readable
    if(useTimeout && SSL_pending(ssl) == 0 &&
       gxSocket::WaitReadable((gxsSocket_t)open_socket, timeout_ms) != 1) {
      ssl_error = gxSSL_REQUEST_TIMEOUT;
      if(bytes_read > 0) {
	return bytes_read; 
      }
      return -1; // Exceeded the timeout value
    }
    if((num_read = SSL_read(ssl, p, num_req-bytes_read)) > 0) {
      bytes_read += num_read; // Increment the byte counter
      p += num_read;          // Move the buffer pointer for the next read
    }
    else {
      int err = check_ssl_error(num_read);

      // Signal to the caller that the connection has closed
      if(err == SSL_ERROR_ZERO_RETURN) {
	if(bytes_read > 0) {
	  return bytes_read;
	}
	else {
	  return 0;
	}
      }
      return -1; // An error occurred during the read
    }
  }

  ssl_error = gxSSL_NO_ERROR;
  return bytes_read;
}

SSL_CTX *gxSSL::InitCTX(const char *cert_fname, const char *key_fname)
// Function used to initialize an SSL context object using a certificate 
// and key file. Returns a pointer to the context object or a null value 
// if an error occurs.
{
  cert_file = cert_fname;
  key_file = key_fname;
  return InitCTX();
} 

SSL_CTX *gxSSL::InitCTX(const char *cert_fname, const char *key_fname, 
			const char *key_passphrase)
// Function used to initialize an SSL context object using a certificate 
// and key file containing a pass-phrase protected key. Returns a pointer 
// to the context object or a null value if an error occurs.
{
  cert_file = cert_fname;
  key_file = key_fname;
  passwd = key_passphrase;
  return InitCTX();
} 

SSL_CTX *gxSSL::InitCTX() 
// Function used to initialize an SSL context object. TLS 1.2 is the
// lowest protocol version allowed. Returns a pointer to the context
// object or a null value if an error occurs.
{
  InitSSLibrary();
  DestroyCTX();

  const SSL_METHOD *meth = 0;
  bool datagram = (ssl_proto == gxSSL_DTLS);
  if(is_client) {
    meth = datagram ? DTLS_client_method() : TLS_client_method();
  }
  else if(is_server) {
    meth = datagram ? DTLS_server_method() : TLS_server_method();
  }
  else {
    meth = datagram ? DTLS_method() : TLS_method();
  }
  if(!meth) {
    ssl_error = gxSSL_PROTOCOL_ERROR;
    return 0;
  }

  ctx = SSL_CTX_new(meth);
  if(!ctx) {
    ssl_error = gxSSL_CTX_INIT_ERROR;
    return 0;
  }
  owns_ctx = 1;

  // 10/26/2026: SSL 2, SSL 3, TLS 1.0 and TLS 1.1 are no longer allowed
  int min_version = TLS1_2_VERSION, max_version = 0;
  switch(ssl_proto) {
    case gxSSL_TLSv1_2: min_version = max_version = TLS1_2_VERSION; break;
    case gxSSL_TLSv1_3: min_version = TLS1_3_VERSION; break;
    case gxSSL_DTLS: min_version = DTLS1_2_VERSION; break;
    default: break;
  }
  if(!SSL_CTX_set_min_proto_version(ctx, min_version) ||
     (max_version && !SSL_CTX_set_max_proto_version(ctx, max_version))) {
    ssl_error = gxSSL_PROTOCOL_ERROR;
    DestroyCTX();
    return 0;
  }

  int ssl_cert_type = (cert_type == gxSSL_ASN1) ? SSL_FILETYPE_ASN1 : 
    SSL_FILETYPE_PEM;

  if(!passwd.empty()) {
    SSL_CTX_set_default_passwd_cb_userdata(ctx, (void *)this);
    SSL_CTX_set_default_passwd_cb(ctx, password_cb2);
  }
  if(!cert_file.empty()) {
    int rv = (ssl_cert_type == SSL_FILETYPE_PEM) ?
      SSL_CTX_use_certificate_chain_file(ctx, cert_file.c_str()) :
      SSL_CTX_use_certificate_file(ctx, cert_file.c_str(), ssl_cert_type);
    if(rv <= 0) {
      ssl_error = gxSSL_CERT_ERROR;
      DestroyCTX();
      return 0;
    }
  }
  if(!key_file.empty()) {
    if(SSL_CTX_use_PrivateKey_file(ctx, key_file.c_str() , ssl_cert_type) <= 0) {
      ssl_error = gxSSL_KEY_ERROR;
      DestroyCTX();
      return 0;
    }
    if(!SSL_CTX_check_private_key(ctx)) {
      ssl_error = gxSSL_KEY_ERROR;
      DestroyCTX();
      return 0;
    }
  }

  // Trusted CA certificates used by VerifyCert(): the CA list if one
  // was set, or the system certificate store
  if(!ca_list.empty()) {
    if(!(SSL_CTX_load_verify_locations(ctx, ca_list.c_str(), 0))) {
      ssl_error = gxSSL_CALIST_ERROR;
      DestroyCTX();
      return 0;
    }
  }
  else {
    SSL_CTX_set_default_verify_paths(ctx);
  }

  ssl_error = gxSSL_NO_ERROR;
  return ctx;
}

void gxSSL::DestroyCTX()
// Destroys the context object if this object created it.
{
  if(ctx && owns_ctx) SSL_CTX_free(ctx);
  ctx = 0;
  owns_ctx = 0;
}

void gxSSL::DestroySSL()
// Destroys the SSL object.
{
  if(ssl) SSL_free(ssl);
  ssl = 0;
}

int gxSSL::InitSSLibrary()
// Function used to initialize the SSL library. 
// Returns true after initialization or if the 
// library was previously initialized.
{
  if(!gxSSL::lib_init) {
    OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | 
		     OPENSSL_INIT_LOAD_CRYPTO_STRINGS, 0);
    gxSSL::lib_init = 1;
  }
  return 1;
}

int gxSSL::ReleaseSSLLibrary()
// Function used to release the SSL library. 
// Returns true after releasing the library. 
{
  // Global SSL library release
  gxSSL::lib_init = 0;
  return 1;
}

void gxSSL::init_data_members()
// Internal function used to initialized data members.
{
  ctx = 0;
  owns_ctx = 0;
  ssl = 0;
  socket_bio = file_bio = memory_bio = 0;
  bytes_moved = bytes_read = 0;
  ssl_proto = gxSSL_TLS;
  cert_type = gxSSL_PEM;
  ssl_error = gxSSL_NO_ERROR;
  verify_cert = 1;
  is_client = is_server = 0;
  clear_strings();
  ResetAppError();
}

void gxSSL::clear_strings()
// Internal function used to clear all string members.
{
  random_file.clear();
  key_file.clear();
  ca_list.clear();
  passwd.clear();
  cert_file.clear();
}

int gxSSL::is_ssl_null()
// Returns true if the SSL object is null.
{
  if(!ssl) {
    ssl_error = gxSSL_SSL_INIT_ERROR;
    return 1;
  }
  return 0;
}

int gxSSL::is_ctx_null()
// Returns true if the SSL context object is null.
{
  if(!ctx) {
    ssl_error = gxSSL_CTX_INIT_ERROR;
    return 1;
  }
  return 0;
}

int gxSSL::is_socket_bio_null()
// Returns true if the socket BIO is null.
{
  if(!socket_bio) {
    ssl_error = gxSSL_SBIO_INIT_ERROR;
    return 1;
  }
  return 0;
}

int gxSSL::is_file_bio_null()
// Returns true if the file BIO is null.
{
  if(!file_bio) {
    ssl_error = gxSSL_FBIO_INIT_ERROR;
    return 1;
  }
  return 0;
}

int gxSSL::is_memory_bio_null()
// Returns true if the memory BIO is null.
{
  if(!memory_bio) {
    ssl_error = gxSSL_MBIO_INIT_ERROR;
    return 1;
  }
  return 0;
}

int gxSSL::check_ssl_error(int rv)
// Internal function used to check for SSL library errors.
{
  if(is_ssl_null()) return SSL_ERROR_SSL;

  switch(SSL_get_error(ssl, rv)) {
    case SSL_ERROR_NONE:
      ssl_error = gxSSL_NO_ERROR;
      return SSL_ERROR_NONE;

    case SSL_ERROR_ZERO_RETURN:
      ssl_error = gxSSL_CLOSE_ERROR;
      return SSL_ERROR_ZERO_RETURN;

    case SSL_ERROR_WANT_READ: 
      ssl_error = gxSSL_READ_ERROR;
      return SSL_ERROR_WANT_READ;

    case SSL_ERROR_WANT_WRITE:
      ssl_error = gxSSL_WRITE_ERROR;
      return SSL_ERROR_WANT_WRITE;

    case SSL_ERROR_WANT_CONNECT:
      ssl_error = gxSSL_CONNECT_ERROR;
      return SSL_ERROR_WANT_CONNECT;

    case SSL_ERROR_WANT_ACCEPT:
      ssl_error = gxSSL_ACCEPT_ERROR;
      return SSL_ERROR_WANT_ACCEPT;

    case SSL_ERROR_WANT_X509_LOOKUP:
      ssl_error = gxSSL_X509_LOOKUP_ERROR;
      return SSL_ERROR_WANT_X509_LOOKUP;

    case SSL_ERROR_SYSCALL:
      ssl_error = gxSSL_BIO_ERROR;
      return SSL_ERROR_SYSCALL;
      
    case SSL_ERROR_SSL:
      ssl_error = gxSSL_LIB_ERROR;
      return SSL_ERROR_SSL;

    default:
      ssl_error = gxSSL_LIB_ERROR;
      break;
  }

  return SSL_ERROR_SSL;
}

const char *gxSSL::SSLExceptionMessage()
// Returns a null terminated string that can
// be use to log or print an SSL exception.
{
  // Check to application defined errors
  if(ssl_error == gxSSL_APPDEF_ERROR) {
    return app_err_str.c_str();
  }

  if((int)ssl_error > (gxsMaxOpenSSLExceptionMessages-1)) {
    ssl_error = gxSSL_INVALID_ERROR_CODE;
  }

  // Find the corresponding message in the exception array
  int error = (int)ssl_error;
  return gxsOpenSSLExceptionMessages[error];
}

int gxSSL::VerifyCert(const char *hostname, long *X509_return_code)
// Function used to verify a certificate from the specified hostname. 
// Returns 0 if successful. 
// Returns a non-zero value if the function fails.
{
  if(!hostname) return ssl_error = gxSSL_NULL_PTR_ERROR;

  int is_valid = 0;
  X509 *cert;
  long rv;
  
  cert = SSL_get_peer_certificate(ssl);
  if(cert) {
    rv = SSL_get_verify_result(ssl);
    // The caller requested the X509 return code
    if(X509_return_code) *X509_return_code = rv;
    if(rv == X509_V_OK) is_valid = 1;
  }
  else {
    // No cert was presented by host
    return ssl_error = gxSSL_PEERCERT_ERROR;
  }
  if(!is_valid) {
    X509_free(cert);
    return ssl_error = gxSSL_CERT_VER_ERROR;
  }

  // 10/26/2026: Check the subject alternative names, with wildcards, as
  // well as the common name. A numeric host is checked as an IP address.
  int host_ok;
  ASN1_OCTET_STRING *ip = a2i_IPADDRESS(hostname);
  if(ip) {
    host_ok = X509_check_ip(cert, ASN1_STRING_get0_data(ip), 
			    ASN1_STRING_length(ip), 0) == 1;
    ASN1_OCTET_STRING_free(ip);
  }
  else {
    host_ok = X509_check_host(cert, hostname, 0, 
			      X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS, 0) == 1;
  }
  X509_free(cert);
  if(!host_ok) {
    return ssl_error = gxSSL_PEERCN_ERROR;
  }

  return ssl_error = gxSSL_NO_ERROR;
}

int gxSSL::GetPeerCertInfo(std::string &subject, std::string &issuer)
// 10/24/2026: Passes back the subject and issuer names of the peer's
// certificate. Returns 0 if successful or a non-zero value if the peer
// did not present a certificate.
{
  subject.clear(); issuer.clear();
  if(is_ssl_null()) return ssl_error;
  X509 *cert = SSL_get_peer_certificate(ssl);
  if(!cert) return ssl_error = gxSSL_PEERCERT_ERROR;
  char buf[4096];
  memset(buf, 0, sizeof(buf));
  X509_NAME_oneline(X509_get_subject_name(cert), buf, sizeof(buf));
  subject = buf;
  memset(buf, 0, sizeof(buf));
  X509_NAME_oneline(X509_get_issuer_name(cert), buf, sizeof(buf));
  issuer = buf;
  X509_free(cert);
  return ssl_error = gxSSL_NO_ERROR;
}

const char *gxSSL::CertVerifyErrorString(long X509_return_code)
// 10/24/2026: Returns a string for a VerifyCert() return code
{
  return X509_verify_cert_error_string(X509_return_code);
}

int gxSSL::InitPRNG(const char *r_file, int check_prng_init)
// Function used to initialize the pseudo random number generator 
// using a random number file. If check_prng_init is true the 
// current status of the random number generator will be checked
// before initialization.
// Returns 0 if successful. 
// Returns a non-zero value if the function fails.
{
  random_file = r_file;
  return InitPRNG(check_prng_init);
}

int gxSSL::InitPRNG(int check_prng_init)
// Function used to initialize the pseudo random number generator. 
// If check_prng_init is true the current status of the random number 
// generator will be checked before initialization.
// Returns 0 if successful. 
// Returns a non-zero value if the function fails.
{
  if(check_prng_init) {
    if(RAND_status()) {
      return ssl_error = gxSSL_NO_ERROR;
    }
  }

  if(!random_file.empty()) {
    RAND_load_file(random_file.c_str(), 16384);
    if(RAND_status()) { 
      return ssl_error = gxSSL_NO_ERROR;
    }
    else {
      return ssl_error = gxSSL_PRNG_ERROR;
    }
  }

  // 10/26/2026: OpenSSL 1.1 and later seed the generator from the
  // operating system. The table of gxRandom values mixed in here added
  // no real entropy and is no longer used.
  RAND_poll();

  if(RAND_status() == 0) {
    return ssl_error = gxSSL_PRNG_ERROR;
  }

  return ssl_error = gxSSL_NO_ERROR;
}
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
// Passes OpenSSL 3 key generation progress to a KeyGenCallbackFunc
struct gxsslKeyGenCallbackData {
  KeyGenCallbackFunc cb_func;
  void *cb_arg;
};

static int gxssl_keygen_cb(EVP_PKEY_CTX *kctx)
{
  gxsslKeyGenCallbackData *d = (gxsslKeyGenCallbackData *)EVP_PKEY_CTX_get_app_data(kctx);
  if(d && d->cb_func) {
    d->cb_func(EVP_PKEY_CTX_get_keygen_info(kctx, 0), EVP_PKEY_CTX_get_keygen_info(kctx, 1), d->cb_arg);
  }
  return 1; // Continue key generation
}
#endif

int gxSSL::MakeRSAPrivateKey(const char *key_fname, int key_len,
			     KeyGenCallbackFunc cb_func, void *cb_arg)
// Function used to write a private RSA (Rivest, Shamir, Adleman) key to 
// the key_fname file. The length of the key is set by the key_len variable, 
// which must be a minimum of 2048. An optional callback function can be used 
// with an optional callback argument if any additional processing must be 
// preformed during key generation.
// Returns 0 if successful. 
// Returns a non-zero value if the function fails
{
  // 10/14/2026: Generates the key before opening the file, closes the file and
  // frees the key, exponent and callback objects on every path, and uses
  // the OpenSSL 3 EVP_PKEY interface when available (RSA_* is deprecated).
  if(key_len < 2048) key_len = 2048; // 10/26/2026: Was a 512-bit minimum
  if(InitPRNG(1) != gxSSL_NO_ERROR) return ssl_error;

  EVP_PKEY *private_key = 0;

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  gxsslKeyGenCallbackData cb_data;
  cb_data.cb_func = cb_func;
  cb_data.cb_arg = cb_arg;
  EVP_PKEY_CTX *kctx = EVP_PKEY_CTX_new_from_name(0, "RSA", 0);
  if(kctx && EVP_PKEY_keygen_init(kctx) == 1 &&
     EVP_PKEY_CTX_set_rsa_keygen_bits(kctx, key_len) == 1) {
    if(cb_func) {
      EVP_PKEY_CTX_set_app_data(kctx, &cb_data);
      EVP_PKEY_CTX_set_cb(kctx, gxssl_keygen_cb);
    }
    if(EVP_PKEY_keygen(kctx, &private_key) != 1) private_key = 0;
  }
  EVP_PKEY_CTX_free(kctx);
#else
  BIGNUM *bne = BN_new();
  BN_GENCB *pcb = BN_GENCB_new();
  RSA *rsa = RSA_new();
  private_key = EVP_PKEY_new();
  if(bne && pcb && rsa && private_key && BN_set_word(bne, RSA_F4) == 1) {
    BN_GENCB_set_old(pcb, cb_func, cb_arg);
    if(RSA_generate_key_ex(rsa, key_len, bne, pcb) == 1 &&
       EVP_PKEY_assign_RSA(private_key, rsa)) {
      rsa = 0; // Now owned by private_key
    }
    else {
      EVP_PKEY_free(private_key);
      private_key = 0;
    }
  }
  else if(private_key) {
    EVP_PKEY_free(private_key);
    private_key = 0;
  }
  if(rsa) RSA_free(rsa);
  if(pcb) BN_GENCB_free(pcb);
  if(bne) BN_free(bne);
#endif

  if(!private_key) return ssl_error = gxSSL_RSAGEN_ERROR;

  FILE *fp = fopen(key_fname, "w+b");
  if(!fp) {
    app_err_str = str_cat("SSL exception: Can't write RSA private key file ", key_fname);
    EVP_PKEY_free(private_key);
    return ssl_error = gxSSL_APPDEF_ERROR;
  }

  int written = PEM_write_PrivateKey(fp, private_key, 0, 0, 0, 0, 0);
  fclose(fp);
  EVP_PKEY_free(private_key);
  if(!written) return ssl_error = gxSSL_RSAGEN_ERROR;
  return ssl_error = gxSSL_NO_ERROR;
}

int gxSSL::add_cert_ext(X509 *cert, int nid, const char *value)
// Internal function used to add extensions to an X509 certificate.
// Returns 0 if successful. 
// Returns a non-zero value if the function fails.
{
  X509_EXTENSION *ex;
  X509V3_CTX x509_ctx;

  // Sets context of extension with no configuration database
  X509V3_set_ctx_nodb(&x509_ctx);

  // Assuming target since is self signed
  X509V3_set_ctx(&x509_ctx, cert, cert, 0, 0, 0);
  ex = X509V3_EXT_conf_nid(0, &x509_ctx, nid, value);

  if(!ex) {
    return ssl_error = gxSSL_X509_GEN_ERROR;
  }

  X509_add_ext(cert, ex, -1);
  X509_EXTENSION_free(ex);

  return ssl_error = gxSSL_NO_ERROR;
}

int gxSSL::MakeCertReq(const char *key_fname, gxCertIssuerInfo &ci,
		       const char *req_fname)
// Function used to write a public key and self-signed X509 certificate 
// request to the req_file. The key_file must contain the private key. 
// The ci variable is used to set the issuer information for self-signing 
// the certificate. 
// Returns 0 if successful. 
// Returns a non-zero value if the function fails.
{
  FILE *fp = fopen(key_fname, "rb");
  if(!fp) {
    app_err_str = str_cat("SSL exception: Can't read RSA private key file ", key_fname);
    return ssl_error = gxSSL_APPDEF_ERROR;
  }

  EVP_PKEY *private_key = PEM_read_PrivateKey(fp, 0, 0, 0);
  if(!private_key) {
    return ssl_error = gxSSL_KEY_ERROR;
  }
  fclose(fp);

  X509 *x = X509_new(); // 10/14/2026: Was "x = x=X509_new()" (undefined order)
  if(!x) {
    EVP_PKEY_free(private_key);
    return ssl_error = gxSSL_X509_GEN_ERROR;
  }
  
  if(ci.sn < 0) ci.sn = 0;
  if(ci.days <= 0) ci.days = 28;
  X509_set_version(x,2);
  ASN1_INTEGER_set(X509_get_serialNumber(x), ci.sn);
  X509_gmtime_adj(X509_get_notBefore(x), 0);
  X509_gmtime_adj(X509_get_notAfter(x), (long)(60*60*24*ci.days));
  X509_set_pubkey(x, private_key);

  X509_NAME *name = X509_get_subject_name(x);
  if(!name) {
    X509_free(x);
    EVP_PKEY_free(private_key);
    CRYPTO_cleanup_all_ex_data();
    return ssl_error = gxSSL_X509_REQGEN_ERROR;
  }

  // Set the certificate issuer fields
  if(!ci.C.empty()) {
    if(!X509_NAME_add_entry_by_txt(name, "C", // Country Name = C
				   MBSTRING_ASC, 
				   (unsigned char *)ci.C.c_str(), 
				   -1, -1, 0)) {
      X509_free(x);
      EVP_PKEY_free(private_key);
      CRYPTO_cleanup_all_ex_data();
      app_err_str = str_cat("SSL exception: Bad country name in cert req");
      return ssl_error = gxSSL_APPDEF_ERROR;
    }
  }
  if(!ci.ST.empty()) {
    if(!X509_NAME_add_entry_by_txt(name, "ST", // State or Province Name = ST
				   MBSTRING_ASC, 
				   (unsigned char *)ci.ST.c_str(), 
				   -1, -1, 0)) {
      X509_free(x);
      EVP_PKEY_free(private_key);
      CRYPTO_cleanup_all_ex_data();
      app_err_str = str_cat("SSL exception: Bad state or province in cert req");
      return ssl_error = gxSSL_APPDEF_ERROR;
    }
  }
  if(!ci.L.empty()) {
    if(!X509_NAME_add_entry_by_txt(name, "L", // Locality Name = L
				   MBSTRING_ASC, 
				   (unsigned char *)ci.L.c_str(), 
				   -1, -1, 0)) {
      X509_free(x);
      EVP_PKEY_free(private_key);
      CRYPTO_cleanup_all_ex_data();
      app_err_str = str_cat("SSL exception: Bad locality in cert req");
      return ssl_error = gxSSL_APPDEF_ERROR;
    }
  }
  if(!ci.O.empty()) {
    if(!X509_NAME_add_entry_by_txt(name, "O", // Organization Name = O
				   MBSTRING_ASC, 
				   (unsigned char *)ci.O.c_str(), 
				   -1, -1, 0)) {
      X509_free(x);
      EVP_PKEY_free(private_key);
      CRYPTO_cleanup_all_ex_data();
      app_err_str = str_cat("SSL exception: Bad organization name in cert req");
      return ssl_error = gxSSL_APPDEF_ERROR;
    }
  }
  if(!ci.OU.empty()) {
    if(!X509_NAME_add_entry_by_txt(name, "OU", // Organizational Unit Name = OU
				   MBSTRING_ASC, 
				   (unsigned char *)ci.OU.c_str(), 
				   -1, -1, 0)) {
      X509_free(x);
      EVP_PKEY_free(private_key);
      CRYPTO_cleanup_all_ex_data();
      app_err_str = str_cat("SSL exception: Bad organization unit in cert req");
      return ssl_error = gxSSL_APPDEF_ERROR;
    }
  }
  if(!ci.CN.empty()) {
    if(!X509_NAME_add_entry_by_txt(name, "CN", // Common Name = CN
				   MBSTRING_ASC, 
				   (unsigned char *)ci.CN.c_str(), 
				   -1, -1, 0)) {
      X509_free(x);
      EVP_PKEY_free(private_key);
      CRYPTO_cleanup_all_ex_data();
      app_err_str = str_cat("SSL exception: Bad common name in cert req");
      return ssl_error = gxSSL_APPDEF_ERROR;
    }
  }

  // Self sign by setting issuer name to same subject
  X509_set_issuer_name(x, name);
  
  // Standard extensions
  if(!ci.basic_constraints.empty()) {
    if(add_cert_ext(x, NID_basic_constraints, ci.basic_constraints.c_str()) != 
       gxSSL_NO_ERROR) {
      X509_free(x);
      EVP_PKEY_free(private_key);
      CRYPTO_cleanup_all_ex_data();
      return ssl_error;
    }
  }
  if(!ci.key_usage.empty()) {
    if(add_cert_ext(x, NID_key_usage, ci.key_usage.c_str()) != gxSSL_NO_ERROR) {
      X509_free(x);
      EVP_PKEY_free(private_key);
      CRYPTO_cleanup_all_ex_data();
      return ssl_error;
    }
  }
  if(!ci.subject_key_identifier.empty()) {
    if(add_cert_ext(x, NID_subject_key_identifier, ci.subject_key_identifier.c_str()) !=
       gxSSL_NO_ERROR) {
      X509_free(x);
      EVP_PKEY_free(private_key);
      CRYPTO_cleanup_all_ex_data();
      return ssl_error;
    }
  }

  // Netscape specific extensions
  if(!ci.netscape_cert_type.empty()) {
   if(add_cert_ext(x, NID_netscape_cert_type, ci.netscape_cert_type.c_str()) !=
      gxSSL_NO_ERROR) {
     X509_free(x);
     EVP_PKEY_free(private_key);
     CRYPTO_cleanup_all_ex_data();
     return ssl_error;
   }
  }
  if(!ci.netscape_comment.empty()) {
    if(add_cert_ext(x, NID_netscape_comment, ci.netscape_comment.c_str()) !=
       gxSSL_NO_ERROR) {
      X509_free(x);
      EVP_PKEY_free(private_key);
      CRYPTO_cleanup_all_ex_data();
      return ssl_error;
    }
  }
  
  // 10/24/2026: Sign with SHA-256. Windows Schannel and current browsers
  // reject MD5 signed certs.
  if(!X509_sign(x, private_key, EVP_sha256())) {
    X509_free(x);
    EVP_PKEY_free(private_key);
    CRYPTO_cleanup_all_ex_data();
    app_err_str = str_cat("SSL exception: Error self signing cert req");
    return ssl_error = gxSSL_APPDEF_ERROR;
  }

  fp = fopen(req_fname, "w+b");
  if(!fp) {
    app_err_str = str_cat("SSL exception: Can't write cert request file ", req_fname);
    X509_free(x);
    EVP_PKEY_free(private_key);
    CRYPTO_cleanup_all_ex_data();
    return ssl_error = gxSSL_APPDEF_ERROR;
  }
  if(!PEM_write_X509(fp, x)) {
    X509_free(x);
    EVP_PKEY_free(private_key);
    CRYPTO_cleanup_all_ex_data();
    app_err_str = str_cat("SSL exception: Error writing X509 cert req ", req_fname);
    return ssl_error = gxSSL_APPDEF_ERROR;
  }

  fclose(fp);
  X509_free(x);
  EVP_PKEY_free(private_key);
  CRYPTO_cleanup_all_ex_data();

  return ssl_error = gxSSL_NO_ERROR;
}

#endif // !__USE_GX_SCHANNEL__


#if defined(__USE_GX_SCHANNEL__)
// 10/24/2026: The Windows Schannel version of the gxSSL class is in
// gxsschan.cpp, which is compiled as part of this file so the build
// files do not need another object file.
#define __GX_SSL_SCHANNEL_SOURCE__
#include "gxsschan.cpp"
#endif
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //

