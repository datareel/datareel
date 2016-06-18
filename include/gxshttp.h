// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Header File Name: gxshttp.h
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

HTTP client/server constants and data structures shared between
HTTP client and server applications.

Changes:
==============================================================
09/27/2001: Added the gxsHTTP_STATUS_DISK_ERROR and 
gxsHTTP_STATUS_NETWORK_ERROR constants to the HTTP status codes 
enumeration.

09/27/2001: Changed the gxsMAX_HTTP_STATUS_CODES integer constant
from 17 to 19 to match the number of enumerated constants defined
in the HTTP status codes enumeration.

09/28/2002: Changed the gxsMAX_HTTP_STATUS_CODES integer constant
from 19 to 20 to match the number of enumerated constants defined
in the HTTP status codes enumeration.

10/03/2002: Added the gxsHTTPHeader::Reset() function used to
reset HTTP header information.

10/03/2002: Moved the gxsHTTPHeader constructor and destructor
to the gxshttp.h include file.

10/03/2002: Added the __HTTP_RECV_TIMEOUT_SECS__ and
__HTTP_RECV_TIMEOUT_USECS__ constants used to set default
timeout values.
==============================================================
*/
// ----------------------------------------------------------- // 
#ifndef __GX_HTTP_HPP__
#define __GX_HTTP_HPP__

#include "gxdlcode.h"

// --------------------------------------------------------------
// Include files
// --------------------------------------------------------------
#include "gxstring.h"
#include "gxlist.h"
// --------------------------------------------------------------

#if defined (__MSVC__) && defined (__MSVC_DLL__)
#pragma warning(disable:4251) // Disable dll-interface template warning
#endif

// --------------------------------------------------------------
// Constants 
// --------------------------------------------------------------
// Total number of HTTP connection methods. This value must match
// the number constants defined in the "Methods" enumeration.
const int gxsMAX_HTTP_METHODS = 11;

// Total number of HTTP status codes. This value must match the number
// constants defined in the "Status Codes" enumeration.
const int gxsMAX_HTTP_STATUS_CODES = 45;

// Default timeout values
// 04/16/2007: Change default timeout from 7200 (1 hour 20 minutes) to
// 5 minutee. 
const int __HTTP_RECV_TIMEOUT_SECS__ = 300;
const int __HTTP_RECV_TIMEOUT_USECS__ = 0;
// --------------------------------------------------------------

// --------------------------------------------------------------
// Enumerations
// --------------------------------------------------------------
enum { // HTTP methods enumeration

  gxHTTP_UNKNOWN = 0, // Used for internal processing
  
  // HTTP 1.0 and 1.1 Client/Server Methods
  gxsHTTP_GET,     // Retrieve whatever data is identified by the URI
  gxsHTTP_HEAD,    // Returns only HTTP headers and no document body 
  gxsHTTP_PUT,     // Store the data under the supplied URL
  gxsHTTP_DELETE,  // Delete the information corresponding to the given URL
  gxsHTTP_POST,    // Creates a new object linked to the specified object 
  gxsHTTP_LINK,    // Links an existing object to the specified object 
  gxsHTTP_UNLINK,  // Removes link information from an object 
  // 07/24/2006: HTTP 1.1 Request methods
  gxsHTTP_TRACE,   // Echo back request to see what's sadded or changed.
  gxsHTTP_OPTIONS, // Returns the HTTP methods that the server supports.
  gxsHTTP_CONNECT  // Use with a proxy that can change to being an SSL tunnel.
};

enum { // HTTP status codes enumeration

  gxsHTTP_STATUS_UNKNOWN = 0, // Used for internal processing
  
  // 09/27/2001: Status codes added for internal client error
  gxsHTTP_STATUS_DISK_ERROR = 17,    // Client disk error
  gxsHTTP_STATUS_NETWORK_ERROR = 18, // Client network error  
  gxsHTTP_STATUS_CLIENT_ERROR = 19,  // Internal client error

  // HTTP 1.0 and 1.1 Status Codes obtained from:
  // http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html

  // 1xx: Informational - Not used, but reserved for future use.
  gxsHTTP_STATUS_CONTINUE            = 100, // Client should continue request
  gxsHTTP_STATUS_SWITCHING_PROTOCOLS = 101, // Server will comply with request 

  // 2xx: Success - The action was successfully received, understood,
  // and accepted. 
  gxsHTTP_STATUS_OK                = 200, // The request has succeeded.
  gxsHTTP_STATUS_CREATED           = 201, // The request has been fulfilled
                                          // and resulted in a new resource
                                          // being created (Used with POST).
  gxsHTTP_STATUS_ACCEPTED          = 202, // The request has been accepted for
                                          // processing, but the processing has
                                          // not been completed. 
  gxsHTTP_STATUS_NON_AUTH_INFO     = 203, // Non-Authoritative Information.
                                          // Returned metainformation in the 
                                          // entity-header is not definitive 
                                          // set available from origin server, 
                                          // but is gathered from a local or a 
                                          // third-party copy. Use of this 
                                          // response code is not required 
                                          // and is only appropriate when the 
                                          // response would otherwise be 200. 
  gxsHTTP_STATUS_NO_CONTENT        = 204, // The server has fulfilled the
                                          // request but there is no new
                                          // information to send back.
  gxsHTTP_STATUS_RESET_CONTENT     = 205, // Server fulfilled the request and 
                                          // the user agent should reset the 
                                          // document view which caused the 
                                          // request to be sent. Intended to 
                                          // allow input for actions to take 
                                          // place via user input, followed by 
                                          // a clearing of the form in which 
                                          // the input is given so that the 
                                          // user can initiate another input 
                                          // action.
  gxsHTTP_STATUS_PARTIAL_CONTENT   = 206, // Server fulfilled partial GET 
                                          // request. Request must include 
                                          // a Range header field, and may 
                                          // have included an If-Range header 
                                          // field to make the request 
                                          // conditional. 

  // 3xx: Redirection - Further action must be taken in order to complete
  // the request.
  gxsHTTP_STATUS_MULTIPLE_CHOICES  = 300, // The requested resource is
                                          // available at one or more
                                          // locations. 
  gxsHTTP_STATUS_MOVED_PERMANENTLY = 301, // The requested resource has been
                                          // assigned a new permanent URL and
                                          // any future references to this
                                          // resource should be done using
                                          // that URL.
  gxsHTTP_STATUS_MOVED_TEMPORARILY = 302, // The requested resource resides
                                          // temporarily under a different URL.
  gxsHTTP_STATUS_SEE_OTHER         = 303, // Response can be found under a 
                                          // different URI and should be 
                                          // retrieved using a GET method on 
                                          // that resource. This method exists
                                          // primarily to allow the output of 
                                          // a POST-activated script to 
                                          // redirect the user agent to a 
                                          // selected resource. The new URI is
                                          // not a substitute reference for 
                                          // the originally requested resource.
  gxsHTTP_STATUS_NOT_MODIFIED      = 304, // Returned if the client has
                                          // performed a conditional GET
                                          // request and access is allowed,
                                          // but the document has not been
                                          // modified since the date and time
                                          // specified in the If-Modified-Since
                                          // field.
  gxsHTTP_STATUS_USE_PROXY         = 305, // Requested resource must be 
                                          // accessed through the proxy given 
                                          // by the Location field.
  gxsHTTP_STATUS_306_UNUSED        = 306, // The 306 status code was used in a
                                          // previous version of the spec, is 
                                          // no longer used, and the code is 
                                          // reserved.
  gxsHTTP_STATUS_TEMPORARY_REDIRECT = 307, // The requested resource resides 
                                           // temporarily under different URI.
                                           // The temporary URI should be given
                                           // by the Location field in the 
                                           // response. 

  // 4xx: Client Error - The request contains bad syntax or cannot be
  // fulfilled.
  gxsHTTP_STATUS_BAD_REQUEST       = 400, // The request could not be
                                          // understood by the server due to
                                          // malformed syntax.
  gxsHTTP_STATUS_UNAUTHORIZED      = 401, // The request requires user
                                          // authentication. The response must
                                          // include a WWW-Authenticate header
                                          // field containing a challenge
                                          // applicable to the requested
                                          // resource. 
  gxsHTTP_STATUS_PAYMENT_REQUIRED  = 402, // Code reserved for future use. 
  gxsHTTP_STATUS_FORBIDDEN         = 403, // The server understood the request,
                                          // but is refusing to fulfill it.
                                          // Authorization will not help and
                                          // the request should not be
                                          // repeated.
  gxsHTTP_STATUS_NOT_FOUND         = 404, // The server has not found anything
                                          // matching the Request-URI.
  gxsHTTP_STATUS_METHOD_NOT_ALLOWED = 405, // Method specified in the 
                                           // Request-Line is not allowed for 
                                           // the resource identified by the 
                                           // Request-URI. 
  gxsHTTP_STATUS_NOT_ACCEPTABLE    = 406, // Resource identified by request is
                                          // only capable of generating 
                                          // response entities which have 
                                          // content characteristics not 
                                          // acceptable according to the 
                                          // accept headers sent in request.
  gxsHTTP_STATUS_PROXY_AUTH_REQUIRED = 407, // Indicates that the client must 
                                            // first authenticate itself with 
                                            // the proxy.
  gxsHTTP_STATUS_REQUEST_TIMEOUT    = 408, // Client did not produce a request 
                                           // within the time that the server 
                                           // was prepared to wait.
  gxsHTTP_STATUS_CONFLICT           = 409, // Request could not be completed 
                                           // due to a conflict with the 
                                           // current state of the resource.
  gxsHTTP_STATUS_GONE               = 410, // Requested resource is no longer 
                                           // available at the server and no 
                                           // forwarding address is known.
  gxsHTTP_STATUS_LENGTH_REQUIRED    = 411, // Server refuses to accept the 
                                           // request without a defined 
                                           // Content-Length. Client may 
                                           // repeat the request if it adds 
                                           // a valid Content-Length header 
                                           // field containing the length of 
                                           // the message-body in the request 
                                           // message.
  gxsHTTP_STATUS_PRECONDITION_FAILED = 412, // Precondition given in one or 
                                            // more of the request-header 
                                            // fields evaluated to false when 
                                            // it was tested on the server.
  gxsHTTP_STATUS_REQUEST_ENTITY_TOO_LARGE = 413, // Server refusing to process
                                                 // request because the 
                                                 // request entity is larger 
                                                 // than the server is willing
                                                 // or able to process.
  gxsHTTP_STATUS_REQUEST_URI_TOO_LONG = 414, // Server refusing to service 
                                             // request because the 
                                             // Request-URI is longer than the
                                             // server is willing to interpret.
  gxsHTTP_STATUS_UNSUPPORTED_MEDIA_TYPE = 415, // Server refusing to service 
                                               // request because the entity 
                                               // of the request is in a 
                                               // format not supported by the 
                                               // requested resource for the 
                                               // requested method.
  gxsHTTP_STATUS_REQUESTED_RANGE_NOT_SATISFIABLE = 416, // Bad range 
                                                        // requested. 
  gxsHTTP_STATUS_EXPECTATION_FAILED    = 417, // Expectation given in an 
                                              // Expect request-header field
                                              // could not be met by server.

  // 5xx: Server Error - The server failed to fulfill an apparently valid
  // request.
  gxsHTTP_STATUS_INTERNAL          = 500, // The server encountered an
                                          // unexpected condition which
                                          // prevented it from fulfilling
                                          // the request. 
  gxsHTTP_STATUS_NOT_IMPLEMENTED   = 501, // The server does not support the
                                          // functionality required to fulfill
                                          // the request. 
  gxsHTTP_STATUS_BAD_GATEWAY       = 502, // The server, while acting as a
                                          // gateway or proxy, received an
                                          // invalid response from the upstream
                                          // server it accessed in attempting
                                          // to fulfill the request. 
  gxsHTTP_STATUS_UNAVAILABLE       = 503, // The server is currently unable to
                                          // handle the request due to a
                                          // temporary overloading or
                                          // maintenance of the server.
  gxsHTTP_STATUS_GATEWAY_TIMEOUT   = 504, // Server, while acting as a 
                                          // gateway or proxy, did not 
                                          // receive a timely response from 
                                          // the upstream server specified by 
                                          // the URI or some other auxiliary 
                                          // server it needed to access to 
                                          // complete the request.
  gxsHTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED = 505 // Server does not support
                                                  // or refuses to support the
                                                  // HTTP protocol version 
                                                  // used in the request.
};
// --------------------------------------------------------------

// --------------------------------------------------------------
// Data structures
// --------------------------------------------------------------
struct GXDLCODE_API gxsNetscapeCookie
{
  gxsNetscapeCookie();
  ~gxsNetscapeCookie();
  gxsNetscapeCookie(const gxsNetscapeCookie &ob) { Copy(ob); }
  gxsNetscapeCookie& operator=(const gxsNetscapeCookie &ob) { 
    Copy(ob); return *this; 
  }
  void Copy(const gxsNetscapeCookie &ob);
  GXDLCODE_API friend int operator==(const gxsNetscapeCookie &a,
				     const gxsNetscapeCookie &b);

  // Cookie information
  gxString expires;
  gxString domain;
  gxString path;
  gxString host;
  gxString name;
  gxString value;
  int secure;
};

// Header used by client and server fucntions
struct GXDLCODE_API gxsHTTPHeader {
  gxsHTTPHeader() { Reset(); }
  ~gxsHTTPHeader() { }
  gxsHTTPHeader(const gxsHTTPHeader &ob) { Copy(ob); }
  gxsHTTPHeader& operator=(const gxsHTTPHeader &ob) { Copy(ob); return *this; }
  void Copy(const gxsHTTPHeader &ob);
  void Reset();
  GXDLCODE_API friend int operator==(const gxsHTTPHeader &a, 
				     const gxsHTTPHeader &b);
  
  gxString http_header;            // Unchanged or complete HTTP header
  gxString authentication_scheme;  // Authentication scheme used (WWW-Authenticate:)
                                   // Authorization from client  (Authorization:)  
  gxString realm;                  // Authentication realm 
  gxString current_server;         // Server type (Server:)
  gxString location;               // Document location (Location:)
  gxString http_last_modified;     // Date that the document was last modified (Last-Modified:)
  gxString date;                   // Today's date (Date:)
  gxString http_expires;           // Date this document expires (Expires:)
  gxString file_extension;         // Document's file extension (Content-Type:)
  gxString content_type;           // Content type (Content-Type:)
  gxString mime_type;              // MIME type (Content-Type:)
  gxString etag;                   // Entity tag for the associated entity (ETag:)
  gxString pragma;                 // Implementation specific directives (Pragma:)
  gxString cache_control;          // Cache control parameters (Cache-Control:)
  gxString content_encoding;       // Content encoding used (Content-Encoding:)
  float http_version;              // HTTP version
  int http_status;                 // Document/Client/Server status
  int authentication_needed;       // True if authentication is needed
  int not_found;                   // True if the document is not found
  int keep_alive;                  // True if this connection is persistent (Keep-Alive:)
  int timeout;                     // Time out value
  int max_conns;                   // Maximum number of connections
  int length;                      // Document length (Content-Length:)
  int no_cache;                    // True if not using cached copies
  int accept_ranges;               // True if ranges are accepted (Accept-Ranges:)

  // Server side header members
  gxString method;    // Client request method
  gxString fname;     // Requested file name or Web object the client is requesting
  gxString post_data; // Client form data 

  // Netscape cookie infomation
  gxString auth_cookie; // Authentication cookie
  int use_cookies;      // True if using cookies
  gxList<gxsNetscapeCookie> netscape_cookies; // List of parsed cookies (Set-Cookie:)

  // 01/14/2009: Added server side client fields below
  gxString current_host;     // Host name of the client (Host:)
  int host_port;             // Port number client is connecting to
  gxString user_agent;       // Web browser (User-Agent:) 
  gxString accept;           // Accept:
  gxString accept_language;  // Accept-Language:
  gxString accept_encodeing; // Accept-Encoding:
  gxString accept_charset;   // Accept-Charset:
  gxString referer;          // Referer: 
  gxString connection;       // Connection: 

  // 01/14/2009: Added proxy fields below
  gxString proxy_connection; // Proxy-Connection: 
  gxString proxy_auth; // Server - (Proxy-Authenticate:) 
                       // Client - (Proxy-Authorization:)
};
// --------------------------------------------------------------

// --------------------------------------------------------------
// Standalone functions
// --------------------------------------------------------------
GXDLCODE_API const char *gxsHTTPMethodMessage(int method);
GXDLCODE_API const char *gxsHTTPStatusCodeMessage(int status);
// --------------------------------------------------------------

#if defined (__MSVC__) && defined (__MSVC_DLL__)
#pragma warning(default:4251) // Restore previous warning
#endif

#endif // __GX_HTTP_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
