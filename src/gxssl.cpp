// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxssl.h
// C++ Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 10/17/2001
// Date Last Modified: 10/18/2016
// Copyright (c) 2001-2016 DataReel Software Development
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

// 08/06/2009: Moved gxRandom class outside of __USE_GX_SSL_EXTENSIONS__
// 08/06/2009: conditional directive. This allows the random class to 
// 08/06/2009: to be used by application not requiring the SSL functions.
gxRandom::gxRandom(unsigned max_seeds) 
{
  seed_table = 0;
  num_seeds = 0;
  curr_seed = 0;
  SeedTable(max_seeds);
}

gxRandom::~gxRandom() 
{
  if(seed_table) {
    delete seed_table;
    seed_table = 0;
  }
}

unsigned gxRandom::Seed(int reuse)
// Returns a random seed from the seed table. If  
// reuse is true the same seed table will be used
// when the number of seed in the table is exceeded.  
{
  if(!seed_table) {
    SeedTable(num_seeds);
  }
  if(seed_table) {
    if(curr_seed >= (num_seeds-1)) {
      if(reuse) {
	curr_seed = 0;
      }
      else {
	SeedTable(num_seeds);
      }
    }
    if(seed_table) {
      return seed_table[curr_seed++];
    }
  }
  return entropy_number();
}  

unsigned gxRandom::Seed(unsigned &a, unsigned &b, unsigned &c, unsigned &d)
// Returns a random seed using the entropy function. 
{
  return entropy_number(a, b, c, d);
}

unsigned gxRandom::entropy_number()
// Internal entropy function used to generate a number used to seed 
// the random number generator. Returns a random seed. 
{
  unsigned a, b, c, d;
  return entropy_number(a, b, c, d);
}

unsigned gxRandom::entropy_number(unsigned &a, unsigned &b, 
				  unsigned &c, unsigned &d)
// Internal entropy function used to generate a number used to seed 
// the random number generator. Returns a random seed. 
// Passed back 4 byte values in the a, b, c, and d variables.
{
  char *bytes = new char[4];
  a = b = c = d = 0;

  unsigned long ob_addr = (unsigned long)this;
  unsigned long arib_addr = (unsigned long)&bytes;
  ob_addr &= arib_addr;
  unsigned r_seed = (unsigned)time(0);
  r_seed -= ob_addr;
  r_seed += arib_addr;
  unsigned buf;
  memmove(&buf, bytes, 4);
  r_seed += buf;
  buf = r_seed;
  if(arib_addr > 1244948) arib_addr = 1244948;
  if(arib_addr == 0) arib_addr = 1244948;
  if(buf < 290635773) buf = 290635773;
  for(unsigned i = 0; i < (buf%arib_addr); i++) r_seed += (unsigned)clock();

  a = (unsigned)(r_seed & 0xFF);
  b = (unsigned)((r_seed & 0xFF00)>>8);
  c = (unsigned)((r_seed & 0xFF0000)>>16);
  d = (unsigned)((r_seed & 0xFF000000)>>24);

  delete[] bytes; bytes = 0;
  return r_seed;
}

void gxRandom::SeedTable(unsigned max_seeds, unsigned long r_seed)
// Function used to generate the random seed table containing max_seeds. 
// The r_seed value is the number used to seed the random number generator. 
{
  if(max_seeds < 16) max_seeds = 16;
  curr_seed = 0;
  if(seed_table) {
    if(!r_seed) r_seed = (seed_table[7]+entropy_number());
    if(num_seeds == max_seeds) {
      gen_prng_table(seed_table, max_seeds, r_seed);
      return;
    }
    delete seed_table;
    seed_table = 0;
  }
  if(!r_seed) r_seed = entropy_number();
  num_seeds = max_seeds;
  seed_table = gen_prng_table(max_seeds, r_seed);
}

unsigned gxRandom::RandomNumber(unsigned long r_seed)
// Returns a random number.
{
  if(!r_seed) r_seed = Seed();
  int random_number = gen_random_number(r_seed);

#ifndef RAND_MAX
#define RAND_MAX 32767
#endif

  random_number &= RAND_MAX;
  double f = (double)(r_seed*3) * random_number / (RAND_MAX + 1.3);
  random_number = gen_random_number((unsigned long)f);
  random_number &= RAND_MAX;
  f = (double)(r_seed*8) * random_number / (RAND_MAX + 1.8);

  return (unsigned)f;
}

unsigned gxRandom::gen_random_number(unsigned long r_seed)
// Internal function used to generate a random number using a random seed. 
{
  // Seed the random number generator using the call's
  // random seed.
#if defined (__WIN32__)
  srand((unsigned)r_seed);
  return (unsigned)rand();
#elif defined (__HPUX10__) && defined (__REENTRANT__)
  // Reentrant UNIX call added to replace srand() and rand()
  long seed = (long)r_seed; int result;
  return (unsigned)rand_r(&seed, &result);
#elif defined (__UNIX__) && defined (__REENTRANT__)
  // Reentrant UNIX call added to replace srand() and rand()
  unsigned int seed = (unsigned int)r_seed;
  return (unsigned)rand_r(&seed);
#else // Default to the using the srand() function 
  srand((unsigned)r_seed);
  return (unsigned)rand();
#endif
}

unsigned *gxRandom::gen_prng_table(unsigned table_size, unsigned long r_seed)
// Internal function used to generate a new pseudo random number table to the size of 
// table_size. The r_seed value is the number used to seed the random number 
// generator.
{
  unsigned *table = new unsigned[table_size];
  if(!table) return 0;
  return gen_prng_table(table, table_size, r_seed);
}

unsigned *gxRandom::gen_prng_table(unsigned *table, unsigned table_size, 
				   unsigned long r_seed)
// Function used to generate pseudo random numbers reusing a previously 
// allocated table the size of table_size. The r_seed value is the number 
// used to seed the random number generator.
{
  if(!table) return 0;

#if defined (__WIN32__)
  srand(r_seed); // Seed the random number generator
  for(unsigned i = 0; i < table_size; i++) {
    table[i] = (unsigned)(rand() + i);
  }
#elif defined (__HPUX10__) && defined (__REENTRANT__)
  // Reentrant safe UNIX call added to replace srand() and rand()
  long l_seed = r_seed; // Seed the random number generator
  int result;
  for(unsigned i = 0; i < table_size; i++) {
    table[i] = (unsigned)((rand_r((&l_seed + i), &result)));
  }
#elif defined (__UNIX__) && defined (__REENTRANT__)
  // Reentrant safe UNIX call added to replace srand() and rand()
  unsigned l_seed = r_seed; // Seed the random number generator
  for(unsigned i = 0; i < table_size; i++) {
    table[i] = (unsigned)(rand_r(&l_seed) + i);
  }
#else // Default to the using the srand() function 
  srand(r_seed); // Seed the random number generator
  for(unsigned i = 0; i < table_size; i++) {
    table[i] = (unsigned)(rand() + i);
  }
#endif

  return table;
}

unsigned *gxRandom::PRNGTable(unsigned *table1, unsigned table_size, 
			      unsigned long r_seed)
// Function used to generate pseudo random numbers reusing a previously 
// allocated table the size of table_size. The r_seed value is the number 
// used to seed the random number generator.
{
  if(!table1) return 0;
  if(table_size < 1) table_size = 1;
  if(!r_seed) r_seed = entropy_number();

  if(!gen_prng_table(table1, table_size, RandomNumber(r_seed))) return 0;
  unsigned *table2 = gen_prng_table(table_size, RandomNumber(r_seed));
  if(!table2) return 0;
  test_tables(table1, table2, table_size);
  delete table2;

  return table1;
}

int gxRandom::test_tables(unsigned *table1, unsigned *table2, 
			  unsigned table_size)
// Internal function used to test previously allocated pseudo 
// random number tables for randomness.  Returns true after
// adjusting the table values.
{
  if(!table1) return 0;
  if(!table2) return 0;

  unsigned i, j, matches, total_matches;
  unsigned max_loop = table_size * (table_size/2);
  if(max_loop > 4096) max_loop %= max_loop/table_size;
  if(max_loop < 256) max_loop = 64; 
  while(max_loop--) {
    total_matches = 0;
    for(i = 0; i < table_size; i++) {
      matches = 0;
      for(j = 0; j < table_size; j++) {
	// Looking for duplicate entries
	if(table1[i] == table2[j]) matches++;
	if(matches > 1) { // Found duplicate entry
	  table1[i] = RandomNumber();
	  total_matches++;
	}
      }
    }
    if(total_matches == 0) break;
  }

  return 1;
}

unsigned *gxRandom::PRNGTable(unsigned table_size, unsigned long r_seed)
// Function used to generate a new pseudo random number table to the size of 
// table_size. The r_seed value is the number used to seed the random number 
// generator.
{
  if(table_size < 1) table_size = 1;

  if(!r_seed) r_seed = entropy_number();

  unsigned *table1 = gen_prng_table(table_size, RandomNumber(r_seed));
  if(!table1) return 0;
  unsigned *table2 = gen_prng_table(table_size, RandomNumber(r_seed));
  if(!table2) {
    delete table1;
    return 0;
  }
  test_tables(table1, table2, table_size);  
  delete table2;

  return table1;
}

#if defined (__USE_GX_SSL_EXTENSIONS__)

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
  "SSL exception: Common name doesn't match hostname", // PEERCN_ERROR
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
  C.Clear();
  ST.Clear();
  L.Clear();
  O.Clear();
  OU.Clear();
  CN.Clear();
  sn = 0;
  days = 365;
  passwd.Clear();
  company_name.Clear();
  email_address.Clear();
  basic_constraints.Clear();
  key_usage.Clear();
  subject_key_identifier.Clear();
  netscape_cert_type.Clear();
  netscape_comment.Clear();
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

int password_cb2(char *buf,int num, int rwflag,void *userdata)
// Password callback function.
{
  gxSSL *openssl = (gxSSL *)userdata;

  if(openssl->passwd.is_null()) return 0;
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
  app_err_str << clear << s;
  return ssl_error = gxSSL_APPDEF_ERROR;
}

void gxSSL::GlobalSSLError(gxString &err_str)
// Passes back the global SSL error string in the
// err_str variable.
{
  char sbuf[1024];
  memset(sbuf, 0, 1024);
  ERR_error_string(ERR_get_error(), sbuf);
  err_str << clear << "SSL global exception: " << sbuf;
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
    int open_socket;
    BIO_get_fd(ssl->rbio, &open_socket);
    shutdown(open_socket, 1);
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

  int open_socket;
  BIO_get_fd(ssl->rbio, &open_socket);
  gxSocket gxsocket_object;
  
  while(bytes_read < bytes) { // Loop until the buffer is full
    if(useTimeout && (!gxsocket_object.ReadSelect(open_socket, seconds, useconds))) {
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
// Function used to initialize an SSL context object. 
// Returns a pointer to the context object or a null value 
// if an error occurs.
{
  InitSSLibrary();

  const SSL_METHOD *meth = 0;
  
  // Set the SSL protocol
  switch(ssl_proto) {
    case gxSSL_SSLv23:
      if(is_client) {
	meth = SSLv23_client_method();
      }
      else if(is_server) {
	meth = SSLv23_server_method();
      }
      else {
	meth = SSLv23_method();
      }
      break;

#if OPENSSL_VERSION_NUMBER >= 0x1000200fL
    case gxSSL_TLS:
      if(is_client) {
	meth = TLS_client_method();
      }
      else if(is_server) {
	meth = TLS_server_method();
      }
      else {
	meth = TLS_method();
      }
      break;
    case gxSSL_DTLS:
      if(is_client) {
	meth = DTLS_client_method();
      }
      else if(is_server) {
	meth = DTLS_server_method();
      }
      else {
	meth = DTLS_method();
      }
      break;
#endif

#ifndef OPENSSL_NO_SSL2
    case gxSSL_SSLv2:
      if(is_client) {
	meth = SSLv2_client_method();
      }
      else if(is_server) {
	meth = SSLv2_server_method();
      }
      else {
	meth = SSLv2_method();
      }
      break;
#endif
#ifndef OPENSSL_NO_SSL3_METHOD
    case gxSSL_SSLv3:
      if(is_client) {
	meth = SSLv3_client_method();
      }
      if(is_server) {
	meth = SSLv3_server_method();
      }
      else {
	meth = SSLv3_method();
      }
      break;
#endif

#ifndef OPENSSL_NO_TLS1_METHOD
    case gxSSL_TLSv1:
      if(is_client) {
	meth = TLSv1_client_method();
      }
      else if(is_server) {
	meth = TLSv1_server_method();
      }
      else {
	meth = TLSv1_method();
      }
      break;
#endif

#ifndef OPENSSL_NO_TLS1_1_METHOD
    case gxSSL_TLSv1_1:
      if(is_client) {
	meth = TLSv1_1_client_method();
      }
      else if(is_server) {
	meth = TLSv1_1_server_method();
      }
      else {
	meth = TLSv1_1_method();
      }
      break;
#endif

#ifndef OPENSSL_NO_TLS1_2_METHOD
    case gxSSL_TLSv1_2:
      if(is_client) {
	meth = TLSv1_2_client_method();
      }
      else if(is_server) {
	meth = TLSv1_2_server_method();
      }
      else {
	meth = TLSv1_2_method();
      }
      break;

#endif

#ifndef OPENSSL_NO_DTLS1_METHOD
    case gxSSL_DTLSv1:
      if(is_client) {
	meth = DTLSv1_client_method();
      }
      else if(is_server) {
	meth = DTLSv1_server_method();
      }
      else {
	meth = DTLSv1_method();
      }
      break;
#endif

#if OPENSSL_VERSION_NUMBER >= 0x1000200fL
#ifndef OPENSSL_NO_DTLS1_2_METHOD
    case gxSSL_DTLSv1_2:
      if(is_client) {
        meth = DTLSv1_2_client_method();
      }
      else if(is_server) {
	meth = DTLSv1_2_server_method();
      }
      else {
	meth = DTLSv1_2_method();
      }
      break;
#endif
#endif
    default:
      if(is_client) {
	meth = SSLv23_client_method();
      }
      else if(is_server) {
	meth = SSLv23_server_method();
      }
      else {
	meth = SSLv23_method();
      }
      break;
  }
  if(!meth) {
    ssl_error = gxSSL_PROTOCOL_ERROR;
    return 0;
  }

  ctx = SSL_CTX_new(meth);
  if(!meth) {
    ssl_error = gxSSL_PROTOCOL_ERROR;
    return 0;
  }
  if(!ctx) {
    ssl_error = gxSSL_CTX_INIT_ERROR;
    return 0;
  }

  int ssl_cert_type;
  switch(cert_type) {
    case gxSSL_PEM:
      ssl_cert_type = SSL_FILETYPE_PEM;
      break;
    case gxSSL_ASN1:
      ssl_cert_type = SSL_FILETYPE_ASN1;
      break;
    default:
      ssl_cert_type = SSL_FILETYPE_PEM;
      break;
  };

  if(!cert_file.is_null()) {
    if(SSL_CTX_use_certificate_file(ctx, 
				    cert_file.c_str(), ssl_cert_type) <= 0) {
      ssl_error = gxSSL_CERT_ERROR;
      DestroyCTX();
      return 0;
    }
  }
  if(!passwd.is_null()) {
    SSL_CTX_set_default_passwd_cb_userdata(ctx, (void *)this);
    SSL_CTX_set_default_passwd_cb(ctx, password_cb2);
  }
  if(!key_file.is_null()) {
    if(SSL_CTX_use_PrivateKey_file(ctx, key_file.c_str() , ssl_cert_type) <= 0) {
      ssl_error = gxSSL_KEY_ERROR;
      DestroyCTX();
      return 0;
    }
  }
  if(!ca_list.is_null()) {
    if(!(SSL_CTX_load_verify_locations(ctx, ca_list.c_str(), 0))) {
      ssl_error = gxSSL_CALIST_ERROR;
      DestroyCTX();
      return 0;
    }
  }

#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
  SSL_CTX_set_verify_depth(ctx, 1);
#endif
  
  ssl_error = gxSSL_NO_ERROR;
  return ctx;
}

void gxSSL::DestroyCTX()
// Destroys the context object.
{
  if(ctx) SSL_CTX_free(ctx);
  ctx = 0;
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
  // Global SSL library init
  if(!gxSSL::lib_init) {
    SSL_library_init();       // Initialize SSL library
    SSL_load_error_strings(); // Map SSL error numbers to strings
    SSLeay_add_all_algorithms ();
    SSLeay_add_ssl_algorithms ();
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
  ssl = 0;
  socket_bio = file_bio = memory_bio = 0;
  bytes_moved = bytes_read = 0;
  ssl_proto = gxSSL_TLS;
  cert_type = gxSSL_PEM;
  ssl_error = gxSSL_NO_ERROR;
  verify_cert = 0;
  is_client = is_server = 0;
  clear_strings();
  ResetAppError();
}

void gxSSL::clear_strings()
// Internal function used to clear all string members.
{
  random_file.Clear();
  key_file.Clear();
  ca_list.Clear();
  passwd.Clear();
  cert_file.Clear();
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

  char peer_common_name[256];
  memset(peer_common_name, 0, 256);
  X509_NAME_get_text_by_NID(X509_get_subject_name(cert), 
			    NID_commonName, peer_common_name, 256);

  X509_free(cert);
  if(CaseICmp(peer_common_name, hostname) != 0) {
    return ssl_error = gxSSL_PEERCN_ERROR;
  }

  return ssl_error = gxSSL_NO_ERROR;
}

int gxSSL::MakeDHParms(const char *dh_fname, 
		       unsigned char *dh1024_p, int dh1024_p_len,  
		       unsigned char *dh1024_g, int dh1024_g_len)
// Write Diffie-Hellman parameters to the dh_fname file. 
// Returns 0 if successful. 
// Returns a non-zero value if the function fails.
{
  unsigned char dh1024_p_default[128] = {
    0xD9,0x80,0x9D,0xF9,0x04,0x4E,0xE2,0x87,0x38,0x25,0x9A,0x13,
    0xE5,0xBE,0x17,0x79,0x0C,0xFD,0xEE,0xF3,0xE4,0x0F,0x21,0x65,
    0xBE,0xCE,0xAB,0x01,0x8C,0x42,0xB5,0x0E,0x68,0x11,0x00,0xAF,
    0x80,0x56,0x79,0x90,0x67,0x1B,0xFF,0x3B,0x86,0x1C,0x19,0x80,
    0xD3,0xFA,0xF7,0xF2,0x94,0x72,0x19,0xBB,0xD7,0x8A,0x9D,0xFC,
    0xFA,0x4E,0xB2,0x90,0x9A,0x50,0x9D,0xB4,0xC7,0xA3,0xDC,0x98,
    0xAB,0xA3,0xA4,0x08,0x1C,0x6B,0xF7,0xF5,0xF4,0x71,0x8C,0x06,
    0x34,0x5A,0xC1,0x03,0x24,0x47,0xDA,0x1A,0x07,0x06,0xB3,0xB9,
    0x04,0xF4,0x97,0x4A,0xD3,0x4C,0xA9,0xAF,0xA4,0xC1,0x1C,0xAD,
    0x04,0x08,0x71,0x55,0xA2,0x8B,0x70,0x99,0x23,0x9E,0x88,0xE7,
    0xAA,0x2A,0x17,0xC6,0xE2,0x18,0xF0,0xFB
  };
  unsigned char dh1024_g_default[1] = { 0x02 };
  
  DH *dh = DH_new();
  if(!dh) {
    return ssl_error = gxSSL_DHPARMS_GEN_ERROR;
  }
  if(dh1024_p) {
    if(dh1024_p_len <= 0) {
      DH_free(dh);
      return ssl_error = gxSSL_DHPARMS_GEN_ERROR;
    }
    dh->p = BN_bin2bn(dh1024_p, dh1024_p_len, 0);
  }
  else {
    dh->p = BN_bin2bn(dh1024_p_default, sizeof(dh1024_p_default), 0);
  }
  if(dh1024_g) {
    if(dh1024_g_len <= 0) {
      DH_free(dh);
      return ssl_error = gxSSL_DHPARMS_GEN_ERROR;
    }
    dh->g = BN_bin2bn(dh1024_g, dh1024_g_len, 0);
  }
  else {
    dh->g = BN_bin2bn(dh1024_g_default, sizeof(dh1024_g_default), 0);
  }
  
  if((dh->p == 0) || (dh->g == 0)) {
    DH_free(dh);
    return ssl_error = gxSSL_DHPARMS_GEN_ERROR;
  }
    
  FILE *fp = fopen(dh_fname, "w+b");
  if(!fp) {
    app_err_str << clear << "SSL exception: Can't write DH parms file " 
		<< dh_fname;
    DH_free(dh);
    return ssl_error = gxSSL_APPDEF_ERROR;
  }

  if(!PEM_write_DHparams(fp, dh)) {
    fclose(fp);
    DH_free(dh);
    return ssl_error = gxSSL_DHPARMS_GEN_ERROR;
  }

  fclose(fp);
  DH_free(dh);
  return ssl_error = gxSSL_NO_ERROR;
}

int gxSSL::LoadDHParms(const char *fname)
// Load Diffie-Hellman parameters store in fname. 
// Returns 0 if successful. 
// Returns a non-zero value if the function fails.
{
  DH *ret = 0;
  BIO *tmp_bio;
  
  if((tmp_bio = BIO_new_file(fname, "r")) == 0) { 
    return ssl_error = gxSSL_DHPARMS_ERROR;
  }
  ret = PEM_read_bio_DHparams(tmp_bio, 0, 0, 0);
  if(!ret) {
    return ssl_error = gxSSL_DHPARMS_ERROR;
  }
  BIO_free(tmp_bio);

  if(SSL_CTX_set_tmp_dh(ctx, ret) <0) {
    DH_free(ret);
    return ssl_error = gxSSL_DHPARMS_ERROR;
  }

  DH_free(ret);
  return ssl_error = gxSSL_NO_ERROR;
}

int gxSSL::MakeRSAEphKey(int key_len)
// Generate ephemeral RSA (Rivest, Shamir, Adleman) key. 
// Returns 0 if successful. 
// Returns a non-zero value if the function fails.
{
  if(InitPRNG() != gxSSL_NO_ERROR) return ssl_error;

  if(key_len < 512) key_len = 512;
  RSA *rsa = RSA_generate_key(key_len, RSA_F4, 0, 0);
  if(!rsa) {
    return ssl_error = gxSSL_RSAGEN_ERROR;
  }
  if(!SSL_CTX_set_tmp_rsa(ctx, rsa)) {
    return ssl_error = gxSSL_RSAGEN_ERROR;
  }
  RSA_free(rsa);

  return ssl_error = gxSSL_NO_ERROR;
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

  if(!random_file.is_null()) {
    RAND_load_file(random_file.c_str(), 16384);
    if(RAND_status()) { 
      return ssl_error = gxSSL_NO_ERROR;
    }
    else {
      return ssl_error = gxSSL_PRNG_ERROR;
    }
  }

  MemoryBuffer mbuf1;
  const unsigned max_rand = 256;
  unsigned i;

  gxRandom prng1(max_rand); 
  unsigned *rand_table = prng1.PRNGTable(max_rand);
  if(!rand_table) {
    return ssl_error = gxSSL_PRNG_ERROR;
  }

  for(i = 0; i < 16; i++) {
    mbuf1.Cat(rand_table, (max_rand*sizeof(unsigned)));
    if(!prng1.PRNGTable(rand_table, max_rand)) {
      delete rand_table;
      return ssl_error = gxSSL_PRNG_ERROR;
    }
  }

  unsigned int random_number;
  unsigned num_bytes = mbuf1.length()/4;
  unsigned a, b, c ,d;

  i = 0;
  while(num_bytes--) {
    a = b = c = d = 0;
    if(i < (mbuf1.length()-1)) a = (unsigned)mbuf1[i++];
    if(i < (mbuf1.length()-1)) b += (unsigned)mbuf1[i++];
    if(i < (mbuf1.length()-1)) c += (unsigned)mbuf1[i++];
    if(i < (mbuf1.length()-1)) d += (unsigned)mbuf1[i++];
    a = a & 0xFF;
    b = (b<<8) & 0xFF00;
    c = (c<<16) & 0xFF0000;
    d = (d<<24) & 0xFF000000;
    random_number = a + b + c + d;
    RAND_seed(&random_number, sizeof(random_number));
  }

  gxRandom prng2(max_rand);  
  if(!prng2.PRNGTable(rand_table, max_rand)) {
    delete rand_table;
    return ssl_error = gxSSL_PRNG_ERROR;
  }

  MemoryBuffer mbuf2;
  for(i = 0; i < 16; i++) {
    mbuf2.Cat(rand_table, (max_rand*sizeof(unsigned)));
    if(!prng2.PRNGTable(rand_table, max_rand)) {
      delete rand_table;
      return ssl_error = gxSSL_PRNG_ERROR;
    }
  }

  delete rand_table;
  a = b = c = d = 0;
  RAND_add(mbuf2.m_buf(), mbuf2.length(), 
	   (double)prng2.Seed(a, b, c, d));

  if(RAND_status() == 0) {
    return ssl_error = gxSSL_PRNG_ERROR;
  }

  return ssl_error = gxSSL_NO_ERROR;
}

int gxSSL::MakeRSAPrivateKey(const char *key_fname, int key_len,
			     KeyGenCallbackFunc cb_func, void *cb_arg)
// Function used to write a private RSA (Rivest, Shamir, Adleman) key to 
// the key_fname file. The length of the key is set by the key_len variable, 
// which must be a minimum of 512. An optional callback function can be used 
// with an optional callback argument if any additional processing must be 
// preformed during key generation.
// Returns 0 if successful. 
// Returns a non-zero value if the function fails
{
  if(key_len < 512) key_len = 512;
  FILE *fp = fopen(key_fname, "w+b");
  if(!fp) {
    app_err_str << clear << "SSL exception: Can't write RSA private key file " 
		<< key_fname;
    return ssl_error = gxSSL_APPDEF_ERROR;
  }

  EVP_PKEY *private_key = EVP_PKEY_new();
  if(!private_key) {
    return ssl_error = gxSSL_RSAGEN_ERROR;
  }
  
  if(InitPRNG(1) != gxSSL_NO_ERROR) return ssl_error;

  RSA *rsa = RSA_generate_key(key_len, RSA_F4, cb_func, cb_arg);
  if(!EVP_PKEY_assign_RSA(private_key, rsa)) {
    return ssl_error = gxSSL_RSAGEN_ERROR;
  }
  rsa=0;

  if(!PEM_write_PrivateKey(fp, private_key, 0, 0, 0, 0, 0)) {
    return ssl_error = gxSSL_RSAGEN_ERROR;
  }

  EVP_PKEY_free(private_key);
  fclose(fp);

  return ssl_error = gxSSL_NO_ERROR;
}

int gxSSL::add_cert_ext(X509 *cert, int nid, char *value)
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
    app_err_str << clear << "SSL exception: Can't read RSA private key file " 
		<< key_fname;
    return ssl_error = gxSSL_APPDEF_ERROR;
  }

  EVP_PKEY *private_key = PEM_read_PrivateKey(fp, 0, 0, 0);
  if(!private_key) {
    return ssl_error = gxSSL_KEY_ERROR;
  }
  fclose(fp);

  X509 *x = x=X509_new();
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
  if(!ci.C.is_null()) {
    if(!X509_NAME_add_entry_by_txt(name, "C", // Country Name = C
				   MBSTRING_ASC, 
				   (unsigned char *)ci.C.c_str(), 
				   -1, -1, 0)) {
      X509_free(x);
      EVP_PKEY_free(private_key);
      CRYPTO_cleanup_all_ex_data();
      app_err_str << clear << "SSL exception: Bad country name in cert req";
      return ssl_error = gxSSL_APPDEF_ERROR;
    }
  }
  if(!ci.ST.is_null()) {
    if(!X509_NAME_add_entry_by_txt(name, "ST", // State or Province Name = ST
				   MBSTRING_ASC, 
				   (unsigned char *)ci.ST.c_str(), 
				   -1, -1, 0)) {
      X509_free(x);
      EVP_PKEY_free(private_key);
      CRYPTO_cleanup_all_ex_data();
      app_err_str << clear 
		  << "SSL exception: Bad state or province in cert req";
      return ssl_error = gxSSL_APPDEF_ERROR;
    }
  }
  if(!ci.L.is_null()) {
    if(!X509_NAME_add_entry_by_txt(name, "L", // Locality Name = L
				   MBSTRING_ASC, 
				   (unsigned char *)ci.L.c_str(), 
				   -1, -1, 0)) {
      X509_free(x);
      EVP_PKEY_free(private_key);
      CRYPTO_cleanup_all_ex_data();
      app_err_str << clear << "SSL exception: Bad locality in cert req";
      return ssl_error = gxSSL_APPDEF_ERROR;
    }
  }
  if(!ci.O.is_null()) {
    if(!X509_NAME_add_entry_by_txt(name, "O", // Organization Name = O
				   MBSTRING_ASC, 
				   (unsigned char *)ci.O.c_str(), 
				   -1, -1, 0)) {
      X509_free(x);
      EVP_PKEY_free(private_key);
      CRYPTO_cleanup_all_ex_data();
      app_err_str << clear 
		  << "SSL exception: Bad organization name in cert req";
      return ssl_error = gxSSL_APPDEF_ERROR;
    }
  }
  if(!ci.OU.is_null()) {
    if(!X509_NAME_add_entry_by_txt(name, "OU", // Organizational Unit Name = OU
				   MBSTRING_ASC, 
				   (unsigned char *)ci.OU.c_str(), 
				   -1, -1, 0)) {
      X509_free(x);
      EVP_PKEY_free(private_key);
      CRYPTO_cleanup_all_ex_data();
      app_err_str << clear 
		  << "SSL exception: Bad organization unit in cert req";
      return ssl_error = gxSSL_APPDEF_ERROR;
    }
  }
  if(!ci.CN.is_null()) {
    if(!X509_NAME_add_entry_by_txt(name, "CN", // Common Name = CN
				   MBSTRING_ASC, 
				   (unsigned char *)ci.CN.c_str(), 
				   -1, -1, 0)) {
      X509_free(x);
      EVP_PKEY_free(private_key);
      CRYPTO_cleanup_all_ex_data();
      app_err_str << clear << "SSL exception: Bad common name in cert req";
      return ssl_error = gxSSL_APPDEF_ERROR;
    }
  }

  // Self sign by setting issuer name to same subject
  X509_set_issuer_name(x, name);
  
  // Standard extensions
  if(!ci.basic_constraints.is_null()) {
    if(add_cert_ext(x, NID_basic_constraints, ci.basic_constraints.c_str()) != 
       gxSSL_NO_ERROR) {
      X509_free(x);
      EVP_PKEY_free(private_key);
      CRYPTO_cleanup_all_ex_data();
      return ssl_error;
    }
  }
  if(!ci.key_usage.is_null()) {
    if(add_cert_ext(x, NID_key_usage, ci.key_usage.c_str()) != gxSSL_NO_ERROR) {
      X509_free(x);
      EVP_PKEY_free(private_key);
      CRYPTO_cleanup_all_ex_data();
      return ssl_error;
    }
  }
  if(!ci.subject_key_identifier.is_null()) {
    if(add_cert_ext(x, NID_subject_key_identifier, ci.subject_key_identifier.c_str()) !=
       gxSSL_NO_ERROR) {
      X509_free(x);
      EVP_PKEY_free(private_key);
      CRYPTO_cleanup_all_ex_data();
      return ssl_error;
    }
  }

  // Netscape specific extensions
  if(!ci.netscape_cert_type.is_null()) {
   if(add_cert_ext(x, NID_netscape_cert_type, ci.netscape_cert_type.c_str()) !=
      gxSSL_NO_ERROR) {
     X509_free(x);
     EVP_PKEY_free(private_key);
     CRYPTO_cleanup_all_ex_data();
     return ssl_error;
   }
  }
  if(!ci.netscape_comment.is_null()) {
    if(add_cert_ext(x, NID_netscape_comment, ci.netscape_comment.c_str()) !=
       gxSSL_NO_ERROR) {
      X509_free(x);
      EVP_PKEY_free(private_key);
      CRYPTO_cleanup_all_ex_data();
      return ssl_error;
    }
  }
  
  if(!X509_sign(x, private_key, EVP_md5())) {
    X509_free(x);
    EVP_PKEY_free(private_key);
    CRYPTO_cleanup_all_ex_data();
    app_err_str << clear << "SSL exception: Error self signing cert req";
    return ssl_error = gxSSL_APPDEF_ERROR;
  }

  fp = fopen(req_fname, "w+b");
  if(!fp) {
    app_err_str << clear << "SSL exception: Can't write cert request file " 
		<< req_fname;
    X509_free(x);
    EVP_PKEY_free(private_key);
    CRYPTO_cleanup_all_ex_data();
    return ssl_error = gxSSL_APPDEF_ERROR;
  }
  if(!PEM_write_X509(fp, x)) {
    X509_free(x);
    EVP_PKEY_free(private_key);
    CRYPTO_cleanup_all_ex_data();
    app_err_str << clear 
		<< "SSL exception: Error writing X509 cert req " 
		<< req_fname;
    return ssl_error = gxSSL_APPDEF_ERROR;
  }

  fclose(fp);
  X509_free(x);
  EVP_PKEY_free(private_key);
  CRYPTO_cleanup_all_ex_data();

  return ssl_error = gxSSL_NO_ERROR;
}

#endif // __USE_GX_SSL_EXTENSIONS__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //

