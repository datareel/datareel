// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Header File Name: gxshttpc.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 01/25/2000
// Date Last Modified: 06/17/2016
// Copyright (c) 2001-2016 DataReel Software Development
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

HTTP client class.

Changes:
==============================================================
09/27/2001: Added new gxsHTTPClient::Request() functions to 
replace the slower gxsHTTPClient::RequestFile() functions. The 
gxsHTTPClient::RequestFile() functions have been depreciated 
in version 4.11 and retained for backward compatibility.

03/10/2002: The gxsHTTPClient class no longer uses the C++ fstream 
class as the underlying file system. The gxDatabase file pointer 
routines are now used to define the underlying file system used by 
the gxsHTTPClient class. This change was made to support large files 
and NTFS file system enhancements. To enable large file support 
users must define the __64_BIT_DATABASE_ENGINE__ preprocessor 
directive.

10/03/2002: Added gxsHTTPClient::revc_loop, gxsHTTPClient::time_out_sec,
and gxsHTTPClient::time_out_usec data members used to break the receive 
loops and set timeout values.

10/03/2002: Added the following function used to break the receive  
loops and set timeout values: gxsHTTPClient::SetTimeOut(),
gxsHTTPClient::TerminateRevc(), gxsHTTPClient::ResetRecv() , 
gxsHTTPClient::ResetTimeOut(), and gxsHTTPClient::ResetRecv().

12/19/2005: Added SSL class and functions used to support HTTPS
connections.
==============================================================
*/
// ----------------------------------------------------------- // 
#ifndef __GX_SOCKET_HTTP_CLIENT__HPP__
#define __GX_SOCKET_HTTP_CLIENT__HPP__

#include "gxdlcode.h"

#include "gxdfptr.h"
#include "devcache.h"
#include "gxsocket.h"
#include "gxsurl.h"
#include "gxshttp.h"
#include "membuf.h"

#if defined (__USE_GX_SSL_EXTENSIONS__)
#include "gxssl.h"
#endif

// --------------------------------------------------------------
// Constants 
// --------------------------------------------------------------
const int __HTTP_DEFAULT_CACHE_SIZE__  = 10;   // Default cache size  
const int __HTTP_DEFAULT_PACKET_SIZE__ = 4096; // Max buffer length

// Adjust these values for maximum performance
const int __HTTP_CACHE_SIZE__  = 10;   // Number of cache buckets used
const int __HTTP_PACKET_SIZE__ = 4096; // Maximum HTTP buffer size
// --------------------------------------------------------------

// --------------------------------------------------------------
// Type Definitions
// --------------------------------------------------------------
typedef int (*HTTPClientDisplayFunc)(const gxString &mesg); 
// --------------------------------------------------------------

class GXDLCODE_API gxsHTTPClient : public gxSocket, public gxDeviceCache
{
public:
  gxsHTTPClient(int CacheSize = __HTTP_DEFAULT_CACHE_SIZE__,
		int seconds =  __HTTP_RECV_TIMEOUT_SECS__,
		int useconds =  __HTTP_RECV_TIMEOUT_USECS__);
  ~gxsHTTPClient();

public:
  void Flush() { cache.Flush(); } // Flush the cache buckets
  unsigned BucketsInUse() { return cache.BucketsInUse(); }
  int ConnectClient(const char *host, int port = gxSOCKET_HTTP_PORT);
  int RequestHeader(const gxsURLInfo &u, gxsHTTPHeader &hdr);
  int SimpleRequest(const gxsURLInfo &u, gxdFPTR *stream);
  int SimpleRequest(const gxsURLInfo &u, MemoryBuffer &mbuf);

  // 09/27/2001: Replacement request file functions optimized for speedy
  // downloads by requesting the document plus header in one operation.
  int Request(const gxsURLInfo &u, gxsHTTPHeader &hdr, gxdFPTR *stream, 
	      gxsHTTPHeader *compare_hdr = 0, 
	      HTTPClientDisplayFunc display = 0);
  int Request(const gxsURLInfo &u, gxsHTTPHeader &hdr, MemoryBuffer &mbuf);
  char *GetMethod(int method);
  char *GetProtocolString(int protocol);
  void BuildRequestString(const gxsURLInfo &u, int method, gxString &request);
  void BuildSimpleRequestString(const gxsURLInfo &u, int method,
				gxString &request);
  int ParseHTTPHeader(const gxsURLInfo &u, const gxString &header,
		      gxsHTTPHeader &hdr);

private: // Device cache base class interface
  void Read(void *buf, unsigned Bytes, gxDeviceTypes dev);
  void Write(const void *buf, unsigned Bytes, gxDeviceTypes dev);

public: // Client header information that can or must be modified
        // according to the application. These values will be used
        // when a request string is built.

  // HTTP 1.0 and 1.1 Header Field Definitions
  // http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html
  gxString HTTP_VERSION_STRING; // HTTP requested version string
  gxString HTTP_AGENT_STRING;   // Application name;
  gxString HTTP_ACCEPT_STRING;  // All accept strings 
  gxString HTTP_PRAGMA_STRING;  // Implementation-specific directives 

  // Used to provide information left earlier by some server.
  // Usage: "Cookie: persistent data\r\n"
  gxString HTTP_COOKIE_STRING; 
  
  // Default file name used if no file is specified. NOTE: defaults to " /"
  gxString HTTP_DEFAULT_FILE_REQUEST_STRING;

  // This value will be set to null by default. The application must set
  // this value in order to include it in the request string. NOTE: The
  // following syntax must be used when setting this value:
  // "Referrer: http://www.w3.org/hypertext/DataSources/Overview.html\r\n"
  gxString HTTP_REFERER_STRING; // The Referrer request-header field allows
                                // the client to specify, for the server's
                                // benefit, the address (URI) of the resource
                                // from which the Request-URI was obtained. 
  
  // This value will be set to null by default. The application must set
  // this value in order to include it in the request string. NOTE: The
  // following syntax must be used when setting this value:
  // "If-Modified-Since: Sat, 29 Oct 1999 19:43:31 GMT\r\n"
  // There are three accepted date formats: RFC 822, RFC 850, asctime()
  gxString HTTP_IF_MOD_STRING; // Used with the GET method to make it
                               // conditional: if the requested resource has
                               // not been modified since the time specified
                               // in this field, a copy of the resource will
                               // not be returned from the server; instead, a
                               // 304 (not modified) response will be returned
                               // without any Entity-Body. 


  // 07/25/2006: Addtional values used to support HTTP 1.1
  gxString HTTP_KEEP_ALIVE_STRING; // Keep alive value.
  gxString HTTP_CONNECTION_STRING; // All connections strings
  
  // Any additonal header content to appended to the request string
  gxString HTTP_CUSTOM_STRINGS;

  gxString HTTP_CUSTOM_CLIENT_HEADER; // User supplied client header

private:
  gxsURL url;              // Used to process URL information
  int write_error;         // Holds the last write error, if any
  int read_error;          // Holds the last read error, if any
  unsigned bytes_received; // Number of bytes read from an open socket

private: // I/O device members
  gxdFPTR *file_ptr;                 // Pointer to an open file
  char rxBuffer[__HTTP_PACKET_SIZE__]; // Receiver memory buffer

private: // Device cache
  gxDeviceBucketCache cache;

public: // Functions used to get the current device cache
  gxDeviceBucketCache *GetCache() { return &cache; }

public: // Depreciated functions
  int RequestFile(const gxsURLInfo &u, const gxsHTTPHeader &hdr,
		  gxdFPTR *stream);
  int RequestFile(const gxsURLInfo &u, const gxsHTTPHeader &hdr,
		  MemoryBuffer &mbuf);

public: // 10/30/2002 - Functions used to set timeout values
  void SetTimeOut(int seconds = __HTTP_RECV_TIMEOUT_SECS__, 
		  int useconds =  __HTTP_RECV_TIMEOUT_USECS__) {
    time_out_sec = seconds;
    time_out_usec = useconds;
  }
  void TerminateRecv() { recv_loop = 0; }
  void ResetRecv() {  recv_loop = 1; }
  void ResetTimeOut() {
    time_out_sec = __HTTP_RECV_TIMEOUT_SECS__;
    time_out_usec = __HTTP_RECV_TIMEOUT_SECS__;
  }

public: // 10/03/2002 - Timing variables to prevent infinite receive loops
  int recv_loop;     // Used to break receive loops 
  int time_out_sec;  // Number of seconds before a blocking timeout
  int time_out_usec; // Number of microseconds before a blocking timeout

public: // 07/23/2006: Added to record http header exchanges
  int record_proto;      // True if user want to record the session
  gxString proto_string; // Recorded protocol strings
};

#if defined (__USE_GX_SSL_EXTENSIONS__)

class GXDLCODE_API gxsHTTPSClient : public gxSocket, public gxDeviceCache
{
public:
  gxsHTTPSClient(int CacheSize = __HTTP_DEFAULT_CACHE_SIZE__,
		int seconds =  __HTTP_RECV_TIMEOUT_SECS__,
		int useconds =  __HTTP_RECV_TIMEOUT_USECS__);
  ~gxsHTTPSClient();

public:
  void Flush() { cache.Flush(); } // Flush the cache buckets
  unsigned BucketsInUse() { return cache.BucketsInUse(); }
  int ConnectClient(const char *host, int port = gxSOCKET_HTTPS_PORT);
  int RequestHeader(const gxsURLInfo &u, gxsHTTPHeader &hdr);
  int SimpleRequest(const gxsURLInfo &u, gxdFPTR *stream);
  int SimpleRequest(const gxsURLInfo &u, MemoryBuffer &mbuf);
  int Request(const gxsURLInfo &u, gxsHTTPHeader &hdr, gxdFPTR *stream, 
	      gxsHTTPHeader *compare_hdr = 0, 
	      HTTPClientDisplayFunc display = 0);
  int Request(const gxsURLInfo &u, gxsHTTPHeader &hdr, MemoryBuffer &mbuf);
  char *GetMethod(int method);
  char *GetProtocolString(int protocol);
  void BuildRequestString(const gxsURLInfo &u, int method, gxString &request);
  void BuildSimpleRequestString(const gxsURLInfo &u, int method,
				gxString &request);
  int ParseHTTPHeader(const gxsURLInfo &u, const gxString &header,
		      gxsHTTPHeader &hdr);

private: // Device cache base class interface
  void Read(void *buf, unsigned Bytes, gxDeviceTypes dev);
  void Write(const void *buf, unsigned Bytes, gxDeviceTypes dev);

public: // Client header information that can or must be modified
        // according to the application. These values will be used
        // when a request string is built.

  // HTTP 1.0 and 1.1 Header Field Definitions
  // http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html
  gxString HTTP_VERSION_STRING; // HTTP requested version string
  gxString HTTP_AGENT_STRING;   // Application name;
  gxString HTTP_ACCEPT_STRING;  // All accept strings 
  gxString HTTP_PRAGMA_STRING;  // Implementation-specific directives 

  // Used to provide information left earlier by some server.
  // Usage: "Cookie: persistent data\r\n"
  gxString HTTP_COOKIE_STRING; 
  
  // Default file name used if no file is specified. NOTE: defaults to " /"
  gxString HTTP_DEFAULT_FILE_REQUEST_STRING;

  // This value will be set to null by default. The application must set
  // this value in order to include it in the request string. NOTE: The
  // following syntax must be used when setting this value:
  // "Referrer: http://www.w3.org/hypertext/DataSources/Overview.html\r\n"
  gxString HTTP_REFERER_STRING; // The Referrer request-header field allows
                                // the client to specify, for the server's
                                // benefit, the address (URI) of the resource
                                // from which the Request-URI was obtained. 
  
  // This value will be set to null by default. The application must set
  // this value in order to include it in the request string. NOTE: The
  // following syntax must be used when setting this value:
  // "If-Modified-Since: Sat, 29 Oct 1999 19:43:31 GMT\r\n"
  // There are three accepted date formats: RFC 822, RFC 850, asctime()
  gxString HTTP_IF_MOD_STRING; // Used with the GET method to make it
                               // conditional: if the requested resource has
                               // not been modified since the time specified
                               // in this field, a copy of the resource will
                               // not be returned from the server; instead, a
                               // 304 (not modified) response will be returned
                               // without any Entity-Body. 
  
  // 07/25/2006: Addtional values used to support HTTP 1.1
  gxString HTTP_KEEP_ALIVE_STRING; // Keep alive value.
  gxString HTTP_CONNECTION_STRING; // All connections strings
  
  // Any additonal header content to appended to the request string
  gxString HTTP_CUSTOM_STRINGS;
  
  gxString HTTP_CUSTOM_CLIENT_HEADER; // User supplied client header

private:
  gxsURL url;              // Used to process URL information
  int write_error;         // Holds the last write error, if any
  int read_error;          // Holds the last read error, if any
  unsigned bytes_received; // Number of bytes read from an open socket

private: // I/O device members
  gxdFPTR *file_ptr;                 // Pointer to an open file
  char rxBuffer[__HTTP_PACKET_SIZE__]; // Receiver memory buffer

private: // Device cache
  gxDeviceBucketCache cache;

public: // Functions used to get the current device cache
  gxDeviceBucketCache *GetCache() { return &cache; }

public: // Functions used to set timeout values
  void SetTimeOut(int seconds = __HTTP_RECV_TIMEOUT_SECS__, 
		  int useconds =  __HTTP_RECV_TIMEOUT_USECS__) {
    time_out_sec = seconds;
    time_out_usec = useconds;
  }
  void TerminateRecv() { recv_loop = 0; }
  void ResetRecv() {  recv_loop = 1; }
  void ResetTimeOut() {
    time_out_sec = __HTTP_RECV_TIMEOUT_SECS__;
    time_out_usec = __HTTP_RECV_TIMEOUT_SECS__;
  }

public:
  int recv_loop;     // Used to break receive loops 
  int time_out_sec;  // Number of seconds before a blocking timeout
  int time_out_usec; // Number of microseconds before a blocking timeout

public: // OpenSSL data memebers
  gxSSL openssl;

public: // 07/23/2006: Added to record http header exchanges
  int record_proto;      // True if user want to record the session
  gxString proto_string; // Recorded protocol strings
};

#endif // __USE_GX_SSL_EXTENSIONS__

#endif // __GX_SOCKET_HTTP_CLIENT__HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
