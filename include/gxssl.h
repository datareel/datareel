// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxssl.h
// C++ Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 10/17/2001
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

The gxSSL class is an OpenSSL wrapper used to support SSL and TLS 
socket applications. 10/24/2026: On Windows the gxSSL class can also
be built with the native Windows Schannel TLS library by defining
__USE_GX_SCHANNEL__, which does not need OpenSSL. The gxSSL class requires openssl-0.9.8a or higher. 
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
#ifndef __GX_SECURE_SOCKET_LAYER_HPP__
#define __GX_SECURE_SOCKET_LAYER_HPP__

#include "gxdlcode.h"

// --------------------------------------------------------------
// DataReel include files
// --------------------------------------------------------------
#include "gxdfptr.h"
#include "gxsocket.h"
#include <string>
#include <vector>
// --------------------------------------------------------------

#if defined (__USE_GX_SCHANNEL__)
// --------------------------------------------------------------
// 10/24/2026: Windows native TLS (Schannel) include files. Define
// __USE_GX_SCHANNEL__ to build the gxSSL
// class with the Windows Schannel SSP instead of OpenSSL. Link with
// secur32.lib, crypt32.lib and ncrypt.lib.
// --------------------------------------------------------------
#if !defined (__WIN32__)
#error __USE_GX_SCHANNEL__ is only supported for __WIN32__ builds
#endif
#ifndef SECURITY_WIN32
#define SECURITY_WIN32
#endif
#include <windows.h>
#include <wincrypt.h>
#include <security.h>
// SCH_CREDENTIALS, needed for TLS 1.3, is only declared when
// SCHANNEL_USE_BLACKLISTS is defined, and uses UNICODE_STRING from
// subauth.h
#ifndef SCHANNEL_USE_BLACKLISTS
#define SCHANNEL_USE_BLACKLISTS
#endif
#include <subauth.h>
#include <schannel.h>
#include <ncrypt.h>

#else // Use OpenSSL by default
// --------------------------------------------------------------
// Open SSL include files 
// --------------------------------------------------------------
#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/opensslv.h>
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
// 10/14/2026: OpenSSL 3 key and parameter interfaces
#include <openssl/core_names.h>
#include <openssl/param_build.h>
#include <openssl/evp.h>
#endif
// --------------------------------------------------------------
#endif // __USE_GX_SCHANNEL__

namespace datareel {

#if defined (__USE_GX_SCHANNEL__)
// Schannel context and connection objects. The gxSSL interface uses
// these in place of the OpenSSL SSL_CTX and SSL objects.
struct gxSchannelCTX;
struct gxSchannelConn;
typedef gxSchannelCTX gxSSL_CTX_t;
typedef gxSchannelConn gxSSL_t;
#else
// 10/24/2026: Types used by the gxSSL interface for both SSL libraries
typedef SSL_CTX gxSSL_CTX_t;
typedef SSL gxSSL_t;
#endif

// --------------------------------------------------------------
// Enumerations
// --------------------------------------------------------------
// 10/26/2026: SSL 2, SSL 3, TLS 1.0 and TLS 1.1 are no longer supported
enum gxSSLMethod {
  gxSSL_TLS,     // TLS 1.2 or TLS 1.3 (default)
  gxSSL_TLSv1_2, // Only TLS 1.2
  gxSSL_TLSv1_3, // Only TLS 1.3
  gxSSL_DTLS     // DTLS 1.2 (OpenSSL only)
};

enum gxSSLCertType {
  gxSSL_PEM, // Privacy Enhancement Mechanisms
  gxSSL_ASN1 // Abstract Syntax Notation One
};

enum gxSSLError { // Open SSL exception codes
  gxSSL_NO_ERROR = 0,       // No errors reported
  gxSSL_INVALID_ERROR_CODE, // Invalid socket error code

  // SSL error codes
  gxSSL_ACCEPT_ERROR,       // Error accepting remote socket
  gxSSL_AES_ERROR,          // AES crypto error
  gxSSL_AESGEN_ERROR,       // Error generating AES key
  gxSSL_AESKEY_ERROR,       // Error processing AES key
  gxSSL_ASN1_ERROR,         // ASN.1 error
  gxSSL_ASN1_CERT_ERROR,    // Error processing ASN.1 cert
  gxSSL_ASN1_REQ_ERROR,     // Error processing ASN.1 cert req
  gxSSL_BIO_ERROR,          // BIO I/O error 
  gxSSL_BIND_ERROR,         // Could not bind socket
  gxSSL_BLOWFISH_ERROR,     // Blowfish crypto error
  gxSSL_BLOWFISHGEN_ERROR,  // Error generating blowfish key
  gxSSL_BLOWFISHKEY_ERROR,  // Error processing blowfish key
  gxSSL_BUFOVER_ERROR,      // Buffer overflow 
  gxSSL_CALIST_ERROR,       // Error processing CA list
  gxSSL_CERT_ERROR,         // Error processing certificate
  gxSSL_CERT_VER_ERROR,     // Cert doesn't verify
  gxSSL_CLOSE_ERROR,        // TLS/SSL connection has been closed
  gxSSL_CONNECT_ERROR,      // Could not connect socket
  gxSSL_CTX_INIT_ERROR,     // Context object not initialized
  gxSSL_DES_ERROR,          // DES crypto error
  gxSSL_DHPARMS_ERROR,      // Error processing DH parameters
  gxSSL_DHPARMS_GEN_ERROR,  // Error generating DH parameters
  gxSSL_DISCONNECTED_ERROR, // Socket has been disconnected
  gxSSL_DSA_ERROR,          // DSA crypto error
  gxSSL_DSAGEN_ERROR,       // Error generating DSA key
  gxSSL_DSAKEY_ERROR,       // Error processing DSA key
  gxSSL_FBIO_INIT_ERROR,    // File BIO not initialized
  gxSSL_FILELENGTH_ERROR,   // File length does not match expected value
  gxSSL_FILEMODTIME_ERROR,  // File modification time dates do not match
  gxSSL_FILESYSTEM_ERROR,   // A file system error occurred
  gxSSL_HOSTNAME_ERROR,     // Could not resolve hostname
  gxSSL_INIT_ERROR,         // Initialization error
  gxSSL_KRB_ERROR,          // Kerberos crypto error
  gxSSL_LIB_ERROR,          // Library error
  gxSSL_LISTEN_ERROR,       // Listen error
  gxSSL_KEY_ERROR,          // Error processing private key
  gxSSL_MAC_ERROR,          // MAC crypto error
  gxSSL_MD5_ERROR,          // MD5 hash error
  gxSSL_MBIO_INIT_ERROR,    // Memory BIO not initialized
  gxSSL_NULL_PTR_ERROR,     // Accessing a null pointer 
  gxSSL_PASSWD_ERROR,       // Error processing password
  gxSSL_PEERCN_ERROR,       // CN doesn't match hostname
  gxSSL_PEERCERT_ERROR,     // Error processing peer cert
  gxSSL_PRNG_ERROR,         // Pseudo random number error
  gxSSL_PROTOCOL_ERROR,     // Error setting requested protocol
  gxSSL_PUBKEY_ERROR,       // Error processing public key
  gxSSL_READ_ERROR,         // Read operation did not complete
  gxSSL_RECEIVE_ERROR,      // Receive error
  gxSSL_REQUEST_TIMEOUT,    // Request timed out
  gxSSL_RSA_ERROR,          // RSA crypto error
  gxSSL_RSAGEN_ERROR,       // Error generating RSA key
  gxSSL_RSAKEY_ERROR,       // Error processing RSA key
  gxSSL_SBIO_INIT_ERROR,    // Socket BIO not initialized
  gxSSL_SHA_ERROR,          // SHA crypto error
  gxSSL_SHUTDOWN_ERROR,     // Shutdown failed
  gxSSL_SSL_INIT_ERROR,     // SSL object not initialized
  gxSSL_TRANSMIT_ERROR,     // Transmit error
  gxSSL_WRITE_ERROR,        // Write operation did not complete
  gxSSL_X509_ERROR,         // X509 error
  gxSSL_X509_CERT_ERROR,    // Error processing X509 cert
  gxSSL_X509_GEN_ERROR,     // Error generating X509 cert
  gxSSL_X509_LOOKUP_ERROR,  // X509 lookup error
  gxSSL_X509_REQ_ERROR,     // Error processing X509 cert req
  gxSSL_X509_REQGEN_ERROR,  // Error generating X509 cert req

  // Additional SSL and crypto error codes 

  // Exception codes for database block errors
  gxSSL_BLOCKACK_ERROR,    // Database block acknowledgment error
  gxSSL_BLOCKHEADER_ERROR, // Bad database block header
  gxSSL_BLOCKSIZE_ERROR,   // Bad database block size
  gxSSL_BLOCKSYNC_ERROR,   // Database block synchronization error

  // Exception code used to allow application to set error string
  gxSSL_APPDEF_ERROR // Application defined error
};
// --------------------------------------------------------------

// --------------------------------------------------------------
// Function pointer variables for callback functions
// --------------------------------------------------------------
typedef void ((*KeyGenCallbackFunc)(int,int,void *));
// --------------------------------------------------------------

// --------------------------------------------------------------
// Data Structures
// --------------------------------------------------------------
struct gxCertIssuerInfo
{
  gxCertIssuerInfo() { ResetCert(); }
  ~gxCertIssuerInfo() { }
  gxCertIssuerInfo(gxCertIssuerInfo &ob) { CopyCert(ob); }
  void operator=(gxCertIssuerInfo &ob) {
    if(&ob != this) CopyCert(ob);
  } 
  void ResetCert();
  void CopyCert(gxCertIssuerInfo &ob);

  // Certificate issuer fields
  std::string C;  // Country Name = C
  std::string ST; // State or Province Name = ST
  std::string L;  // Locality Name = L
  std::string O;  // Organization Name = O
  std::string OU; // Organizational Unit Name = OU
  std::string CN; // Common Name = CN
  int sn;      // Cert serial number
  int days;    // Days to expire 

  // Optional values and or settings
  std::string passwd;        // Optional challenge password
  std::string company_name;  // Optional company name 
  std::string email_address; // Optional Email address
  
  // Standard extensions
  std::string basic_constraints;
  std::string key_usage;
  std::string subject_key_identifier;
  
  // Netscape specific extensions
  std::string netscape_cert_type;
  std::string netscape_comment;
};
// --------------------------------------------------------------

class GXDLCODE_API gxSSL
{
public:
  gxSSL() { init_data_members(); }
  // Use a context owned by another gxSSL object, such as a server's.
  // The context is not freed when this object is destroyed.
  gxSSL(gxSSL_CTX_t *p) { init_data_members(); ctx = p; }
  virtual ~gxSSL();

private: // Disallow copying and assignment
  gxSSL(const gxSSL &) { }
  void operator=(const gxSSL &) { }

public: // External exception handling functions
  gxSSLError GetSSLError() { return ssl_error; }
  gxSSLError GetSSLError() const { return ssl_error; }
  gxSSLError SetSSLError(gxSSLError err) { return ssl_error = err; }
  gxSSLError ResetSSLError() { return ssl_error = gxSSL_NO_ERROR; }
  gxSSLError ResetError() { return ssl_error = gxSSL_NO_ERROR; }
  const char *SSLExceptionMessage();
  const char *SSLErrorMessage() { return SSLExceptionMessage(); } 
  int SetAppError(const char *s);
  int HasError() { return ssl_error != gxSSL_NO_ERROR; } 
  void ResetAppError() { app_err_str.clear(); }
  static void GlobalSSLError(std::string &err_str);

public: // SSL and Context functions
  gxSSL_CTX_t *InitCTX();
  void DestroyCTX();
  void DestroySSL();
  void SetSSLProto(gxSSLMethod m) { ssl_proto = m; }
  gxSSLMethod SSLProto() { return ssl_proto; }

public: // Socket functions
  int OpenSSLSocket(gxsSocket_t s);
  int ConnectSSLSocket();
  int AcceptSSLSocket();
  int RawWriteSSLSocket(const void *buf, int bytes);
  int RawReadSSLSocket(void *buf, int bytes);
  int ReadSomeSSLSocket(void *buf, int bytes, int timeout_ms = -1);
  int SendSSLSocket(const void *buf, int bytes);
  int RecvSSLSocket(void *buf, int bytes);
  int RecvSSLSocket(void *buf, int bytes, int seconds, int useconds);
  int RecvSSLSocket(void *buf, int bytes, int useTimeout, int seconds,
		    int useconds);
  int CloseSSLSocket(int destroy_ssl = 1);
  int IsServer() { return is_server == 1; }
  int IsClient() { return is_client == 1; }
  int MakeServer() { is_client = 0; return is_server = 1; }
  int MakeClient() { is_server = 0; return is_client = 1; }

  // 10/24/2026: Host name sent to the server in the TLS server name
  // indication (SNI) extension when a client connects. Set it before
  // calling ConnectSSLSocket().
  void SetHostName(const char *s) { host_name = s; }
  const char *HostName() { return host_name.c_str(); }

public: // OpenSSL library init functions
  static int InitSSLibrary();
  static int ReleaseSSLLibrary();
  int HasLibInit() { return gxSSL::lib_init == 1; }

public:  // Object pointer functions
  gxSSL_CTX_t *InitCTX(const char *cert_fname, const char *key_fname);
  gxSSL_CTX_t *InitCTX(const char *cert_fname, const char *key_fname, 
		       const char *key_passphrase);
  gxSSL_CTX_t *GetCTX() { return ctx; }
  gxSSL_t *GetSSL() { return ssl; }
#if !defined (__USE_GX_SCHANNEL__)
  BIO *SocketBIO() { return socket_bio; }
  BIO *FileBIO() { return file_bio; }
  BIO *MemoryBIO() { return memory_bio; }
#endif
  int is_ssl_null();
  int is_ctx_null();
#if !defined (__USE_GX_SCHANNEL__)
  int is_socket_bio_null();
  int is_file_bio_null();
  int is_memory_bio_null();
#endif

public:  // Key, cert, and passwd functions
  const char *RandomFile() { return random_file.c_str(); }
  const char *KeyFile() { return key_file.c_str(); }
  const char *CertFile() { return cert_file.c_str(); }
  const char *CAList() { return ca_list.c_str(); }
  const char *Passwd() { return passwd.c_str(); }
  void SetRandomFile(const char *s) { random_file = s; }
  void SetKeyFile(const char *s) { key_file = s; }
  void SetCertFile(const char *s) { cert_file = s; }
  void SetCAList(const char *s) { ca_list = s; }
  void SetPasswd(const char *s) { passwd = s; }
  void SetCertType(gxSSLCertType t) { cert_type = t; }
  gxSSLCertType CertType() { return cert_type; }
  // Check the peer certificate chain against the CA list or the system
  // certificate store, and check that it was issued for hostname (or
  // IP address). Call it after ConnectSSLSocket(). Clients verify by
  // default; NoVerifyCert() turns that off for test servers that use
  // self-signed certificates.
  int VerifyCert(const char *hostname, long *X509_return_code = 0);
  // 10/24/2026: Pass back the subject and issuer names of the peer's
  // certificate and return a string for a VerifyCert() return code
  int GetPeerCertInfo(std::string &subject, std::string &issuer);
  static const char *CertVerifyErrorString(long X509_return_code);
  int HasVerifyCert() { return verify_cert == 1; } 
  int RequireVerifyCert() { return verify_cert = 1; }
  int NoVerifyCert() { return verify_cert = 0; }
  int InitPRNG(const char *r_file, int check_prng_init = 1);
  int InitPRNG(int check_prng_init = 1);
  int MakeRSAPrivateKey(const char *key_fname, int key_len = 3072,
			KeyGenCallbackFunc cb_func = 0, void *cb_arg = 0);
  int MakeCertReq(const char *key_fname, gxCertIssuerInfo &ci,
		  const char *req_fname);

public: // Thread functions
  // 10/14/2026: Release() clears every pointer without freeing it. The
  // misspelled Relase() is kept for existing code; it used to copy
  // memory_bio into socket_bio and file_bio instead of clearing them.
#if defined (__USE_GX_SCHANNEL__)
  void Release() { ctx = 0; ssl = 0; }
#else
  void Release() { ctx = 0; ssl = 0; socket_bio = file_bio = memory_bio = 0; }
#endif
  void Relase() { Release(); }
  void ReleaseCTX() { ctx = 0; }
  void ReleaseSSL() { ssl = 0; }
  void SetCTX(gxSSL_CTX_t *p) { DestroyCTX(); ctx = p; } // Not owned
  void SetSSL(gxSSL_t *p) { ssl = p; }
#if !defined (__USE_GX_SCHANNEL__)
  void ReleaseSBIO() { socket_bio = 0; }
  void ReleaseFBIO() { file_bio = 0; }
  void ReleaseMBIO() { memory_bio = 0; }
  void SetSocketBIO(BIO *p) { socket_bio = p; }
  void SetFileBIO(BIO *p) { file_bio = p; }
  void SetMemoryBIO(BIO *p) { memory_bio = p; }
  
public: // Call back functions
  friend int password_cb2(char *buf, int num, int rwflag, void *userdata);
#endif
  
public: // Static data members used in place of global variables
  static int lib_init;

protected: // Internal functions
  void init_data_members();
  void clear_strings();
#if defined (__USE_GX_SCHANNEL__)
  // Schannel internal functions
  int sc_acquire_cred(int inbound);
  int sc_load_cert_and_key();
  int sc_load_pfx(const char *fname);
  int sc_handshake(int is_accept, int have_input);
  int sc_send_token(SecBuffer &token);
  int sc_recv_raw(int use_timeout, int seconds, int useconds);
  int sc_read_some(void *buf, int bytes, int use_timeout, int seconds,
		   int useconds);
  int sc_write_all(const void *buf, int bytes);
  int sc_shutdown();
  void sc_set_status(long status);
#else
  int check_ssl_error(int rv);
  int add_cert_ext(X509 *cert, int nid, const char *value);
#endif

protected: // OpenSSL passwords, keys, and certs
  std::string random_file;  // Random file used to seed random num generator
  std::string key_file;     // Private key file
  std::string cert_file;    // Certificate file
  std::string ca_list;      // Certificate authority list
  std::string passwd;       // Cert or key passphrase
  int verify_cert;

protected: // SSL library objects 
  gxSSL_CTX_t *ctx; // Context object
  int owns_ctx;     // True if DestroyCTX() frees ctx
  gxSSL_t *ssl;     // SSL object
  gxSSLMethod ssl_proto;
  gxSSLCertType cert_type;
  std::string host_name; // Server name sent by clients (SNI)

protected: // SSL I/O objects
#if !defined (__USE_GX_SCHANNEL__)
  BIO *socket_bio; // SSL Socket I/O object
  BIO *file_bio;   // SSL file I/O object
  BIO *memory_bio; // SSL memory I/O object
#endif
  int bytes_moved; // Number of bytes moved following an SSL write
  int bytes_read;  // Number of bytes received following an SSL read
  int is_client;   // True if configured for client operations only
  int is_server;   // True if configrued for server operations only

protected: // Error reporting objects
  gxSSLError ssl_error; // Holds the last error reported code
  std::string app_err_str; // Custom Application Error Strings
};


} // namespace datareel

#endif // __GX_SECURE_SOCKET_LAYER_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
