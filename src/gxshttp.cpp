// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxshttp.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 01/25/2000
// Date Last Modified: 06/17/2016
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

HTTP client/server constants and data structures shared between
HTTP client and server applications.

Changes:
==============================================================
09/27/2001: Added 2 new strings to the gxsHTTP_STATUS_CODES 
array for internal client errors.

09/27/2001: Modified the gxsHTTPStatusCodeMessage() function to
return strings for internal client errors.

09/28/2002: Modified the gxsHTTPStatusCodeMessage() function to
return additional strings for internal client errors.

10/03/2002: Added the gxsHTTPHeader::Reset() function used to
reset HTTP header information.

10/03/2002: Moved the gxsHTTPHeader constructor and destructor
to the gxshttp.h include file.

05/05/2003: Added server side members to the gxsHTTPHeader
data structure.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "gxshttp.h"

// HTTP connection methods
const char *gxsHTTP_METHOD[gxsMAX_HTTP_METHODS] = {
  "UNKNOWN",    // String used for internal processing
  "GET",        // Retrieve whatever data is identified by the URI.
  "HEAD",       // Returns only HTTP headers and no document body. 
  "PUT",        // Store the data under the supplied URL.
  "DELETE",     // Delete the information corresponding to the given URL.
  "POST",       // Creates a new object linked to the specified object. 
  "LINK",       // Links an existing object to the specified object. 
  "UNLINK",     // Removes link (or other meta-) information from an object. 
  // 07/24/2006: HTTP 1.1 Request methods
  "TRACE",      // Echo back request to see what's sadded or changed.
  "OPTIONS",    // Returns the HTTP methods that the server supports.
  "CONNECT"     // Use with a proxy that can change to being an SSL tunnel.
};

// HTTP status messages
const char *gxsHTTP_STATUS_CODES[gxsMAX_HTTP_STATUS_CODES] = {
  "HTTP status: Encountered an HTTP error condition",   // Default message
  "100 HTTP status: Continue request",                  // CONTINUE 
  "101 HTTP status: Switching protocols",               // SWITCHING_PROTOCOLS
  "200 HTTP status: Document OK",                       // OK
  "201 HTTP status: Document created",                  // CREATED
  "202 HTTP status: Document accepted",                 // ACCEPTED
  "203 HTTP status: Non-authoritative information",     // NON_AUTH_INFO
  "204 HTTP status: Document has no content",           // NO_CONTENT
  "205 HTTP status: Reset document content",            // RESET_CONTENT
  "206 HTTP status: Partial document content",          // PARTIAL_CONTENT
  "300 HTTP status: Multiple choices",                  // MULTIPLE_CHOICES
  "301 HTTP status: Document has moved permanently",    // MOVED_PERMANENTLY
  "302 HTTP status: Document has moved temporarily",    // MOVED_TEMPORARILY
  "303 HTTP status: Document using other URL",          // SEE_OTHER 
  "304 HTTP status: Document has not been modified",    // NOT_MODIFIED
  "305 HTTP status: Document available via proxy",      // USE_PROXY
  "306 HTTP status: Unused status code",                // 306_UNUSED
  "307 HTTP status: Document temporarily redirected",   // TEMPORARY_REDIRECT
  "400 HTTP client error: Bad client request",          // BAD_REQUEST
  "401 HTTP client error: Unauthorized client request", // UNAUTHORIZED
  "402 HTTP client error: Unused status code",          // PAYMENT_REQUIRED
  "403 HTTP client error: Forbidden client request",    // FORBIDDEN
  "404 HTTP client error: Document not found",          // NOT_FOUND
  "405 HTTP client error: Method not allowed",          // METHOD_NOT_ALLOWED
  "406 HTTP client error: Method not acceptable",       // NOT_ACCEPTABLE
  "407 HTTP client error: Proxy authentication required", // PROXY_AUTH_REQUIRED
  "408 HTTP client error: Request timeout",             // REQUEST_TIMEOUT 
  "409 HTTP client error: Resource conflict",           // CONFLICT
  "410 HTTP client error: Resource no longer available", // GONE
  "411 HTTP client error: Content length required",     // LENGTH_REQUIRED
  "412 HTTP client error: Precondition failed",         // PRECONDITION_FAILED 
  "413 HTTP client error: Request entity too large",    // REQUEST_ENTITY_TOO_LARGE
  "414 HTTP client error: Request uri too long",        // REQUEST_URI_TOO_LONG
  "415 HTTP client error: Unsupported media type",      // UNSUPPORTED_MEDIA_TYPE
  "416 HTTP client error: Requested range not satisfiable", // REQUESTED_RANGE_NOT_SATISFIABLE
  "417 HTTP client error: Expectation_failed",          // EXPECTATION_FAILED
  "500 HTTP server error: Internal server error",       // INTERNAL
  "501 HTTP server error: Service not implemented",     // NOT_IMPLEMENTED
  "502 HTTP server error: Bad gateway or proxy",        // BAD_GATEWAY
  "503 HTTP server error: Status unavailable",          // UNAVAILABLE
  "504 HTTP server error: Gateway timeout",             // GATEWAY_TIMEOUT
  "505 HTTP server error: HTTP version not supported",  // HTTP_VERSION_NOT_SUPPORTED

  // 09/27/2001: Status codes added for internal client error
  "HTTP status: Encountered a client disk error",       // Disk Error
  "HTTP status: Encountered a client network error",    // Network Error
  "HTTP status: Encountered an internal client error"   // Internal Error
};

GXDLCODE_API const char *gxsHTTPMethodMessage(int method)
{
  if(method > (gxsMAX_HTTP_METHODS-1)) method = gxHTTP_UNKNOWN;
  return gxsHTTP_METHOD[method];
}

GXDLCODE_API const char *gxsHTTPStatusCodeMessage(int status)
{
  switch(status) {
    // 09/27/2001: Added default error message
    case gxsHTTP_STATUS_UNKNOWN:
      return gxsHTTP_STATUS_CODES[0]; // Default message

    case gxsHTTP_STATUS_CONTINUE: // 100
      return gxsHTTP_STATUS_CODES[1];

    case gxsHTTP_STATUS_SWITCHING_PROTOCOLS: //101
      return gxsHTTP_STATUS_CODES[2];

    case gxsHTTP_STATUS_OK: // 200
      return gxsHTTP_STATUS_CODES[3];

    case gxsHTTP_STATUS_CREATED: // 201
      return gxsHTTP_STATUS_CODES[4];

    case gxsHTTP_STATUS_ACCEPTED: // 202
      return gxsHTTP_STATUS_CODES[5];

    case gxsHTTP_STATUS_NON_AUTH_INFO: // 203
      return gxsHTTP_STATUS_CODES[6];

    case gxsHTTP_STATUS_NO_CONTENT: // 204
      return gxsHTTP_STATUS_CODES[7];

    case gxsHTTP_STATUS_RESET_CONTENT: // 205
      return gxsHTTP_STATUS_CODES[8];    

    case gxsHTTP_STATUS_PARTIAL_CONTENT: // 206
      return gxsHTTP_STATUS_CODES[9];

    case gxsHTTP_STATUS_MULTIPLE_CHOICES: // 300
      return gxsHTTP_STATUS_CODES[10];

    case gxsHTTP_STATUS_MOVED_PERMANENTLY: // 301
      return gxsHTTP_STATUS_CODES[11];

    case gxsHTTP_STATUS_MOVED_TEMPORARILY: // 302
      return gxsHTTP_STATUS_CODES[12];

    case gxsHTTP_STATUS_SEE_OTHER: // 303
      return gxsHTTP_STATUS_CODES[13];

    case gxsHTTP_STATUS_NOT_MODIFIED: // 304
      return gxsHTTP_STATUS_CODES[14];

    case gxsHTTP_STATUS_USE_PROXY: // 305
      return gxsHTTP_STATUS_CODES[15];

    case gxsHTTP_STATUS_306_UNUSED: // 306
      return gxsHTTP_STATUS_CODES[16];

    case gxsHTTP_STATUS_TEMPORARY_REDIRECT: // 307
      return gxsHTTP_STATUS_CODES[17];

    case gxsHTTP_STATUS_BAD_REQUEST: // 400
      return gxsHTTP_STATUS_CODES[18];

    case gxsHTTP_STATUS_UNAUTHORIZED: // 401
      return gxsHTTP_STATUS_CODES[19];

    case gxsHTTP_STATUS_PAYMENT_REQUIRED: // 402
      return gxsHTTP_STATUS_CODES[20];

    case gxsHTTP_STATUS_FORBIDDEN: // 403
      return gxsHTTP_STATUS_CODES[21];

    case gxsHTTP_STATUS_NOT_FOUND: // 404
      return gxsHTTP_STATUS_CODES[22];

    case gxsHTTP_STATUS_METHOD_NOT_ALLOWED: // 405
      return gxsHTTP_STATUS_CODES[23];

    case gxsHTTP_STATUS_NOT_ACCEPTABLE: // 406
      return gxsHTTP_STATUS_CODES[24];

    case gxsHTTP_STATUS_PROXY_AUTH_REQUIRED: // 407
      return gxsHTTP_STATUS_CODES[25];

    case gxsHTTP_STATUS_REQUEST_TIMEOUT: // 408
      return gxsHTTP_STATUS_CODES[26];

    case gxsHTTP_STATUS_CONFLICT: // 409
      return gxsHTTP_STATUS_CODES[27];

    case gxsHTTP_STATUS_GONE: // 410
      return gxsHTTP_STATUS_CODES[28];

    case gxsHTTP_STATUS_LENGTH_REQUIRED: // 411
      return gxsHTTP_STATUS_CODES[29];

    case gxsHTTP_STATUS_PRECONDITION_FAILED: // 412
      return gxsHTTP_STATUS_CODES[30];

    case gxsHTTP_STATUS_REQUEST_ENTITY_TOO_LARGE: // 413
      return gxsHTTP_STATUS_CODES[31];

    case gxsHTTP_STATUS_REQUEST_URI_TOO_LONG: // 414
      return gxsHTTP_STATUS_CODES[32];

    case gxsHTTP_STATUS_UNSUPPORTED_MEDIA_TYPE: // 415
      return gxsHTTP_STATUS_CODES[33];

    case gxsHTTP_STATUS_REQUESTED_RANGE_NOT_SATISFIABLE: // 416
      return gxsHTTP_STATUS_CODES[34];

    case gxsHTTP_STATUS_EXPECTATION_FAILED: // 417
      return gxsHTTP_STATUS_CODES[35];

    case gxsHTTP_STATUS_INTERNAL: // 500
      return gxsHTTP_STATUS_CODES[36];

    case gxsHTTP_STATUS_NOT_IMPLEMENTED: // 501
      return gxsHTTP_STATUS_CODES[37];

    case gxsHTTP_STATUS_BAD_GATEWAY: // 502
      return gxsHTTP_STATUS_CODES[38];

    case gxsHTTP_STATUS_UNAVAILABLE: // 503
      return gxsHTTP_STATUS_CODES[39];

    case gxsHTTP_STATUS_GATEWAY_TIMEOUT: // 504
      return gxsHTTP_STATUS_CODES[40];

    case gxsHTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED: // 505
      return gxsHTTP_STATUS_CODES[41];

    // 09/27/2001: Status codes added for internal client error
    case gxsHTTP_STATUS_DISK_ERROR: // Client disk error
      return gxsHTTP_STATUS_CODES[42]; 

    case gxsHTTP_STATUS_NETWORK_ERROR: // Client network error
      return gxsHTTP_STATUS_CODES[43]; 

    case gxsHTTP_STATUS_CLIENT_ERROR: // Internal client error
      return gxsHTTP_STATUS_CODES[44]; 

    default:
      break;
  }
  return gxsHTTP_STATUS_CODES[0];
}

gxsNetscapeCookie::gxsNetscapeCookie()
{
  host = name = value = expires = domain = path = "\0";
  secure = 0;
}

gxsNetscapeCookie::~gxsNetscapeCookie()
{

}

void gxsNetscapeCookie::Copy(const gxsNetscapeCookie &ob)
{
  // 01/14/2009: Avoid self copying
  if(this == &ob) return;

  expires = ob.expires;
  domain = ob.domain;
  path = ob.path;
  host = ob.host;
  name = ob.name;
  value = ob.value;
  secure = ob.secure;
}

GXDLCODE_API int operator==(const gxsNetscapeCookie &a, 
			    const gxsNetscapeCookie &b)
// Overloaded == operator added to work with gxs linked list class.
{
  if(a.expires != b.expires) return 0;
  if(a.domain != b.domain) return 0;
  if(a.path != b.path) return 0;
  if(a.host != b.host) return 0;
  if(a.name != b.name) return 0;
  if(a.value != b.value) return 0;
  if(a.secure != b.secure) return 0;
  return 1;
}

void gxsHTTPHeader::Reset() 
{
  http_header.Clear(); 
  authentication_scheme.Clear(); 
  realm.Clear(); auth_cookie.Clear();
  current_server.Clear(); 
  location.Clear(); 
  http_last_modified.Clear();  
  date.Clear(); 
  http_expires.Clear(); 
  file_extension.Clear(); 
  content_type.Clear(); 
  mime_type.Clear(); 
  etag.Clear(); 
  content_encoding.Clear(); 
  pragma.Clear(); 
  cache_control.Clear();
  method.Clear(); 
  fname.Clear(); 
  post_data.Clear();

  // Default settings
  http_version = 1.0F;
  http_status = gxsHTTP_STATUS_OK;
  authentication_needed = 0;
  not_found = 0;
  keep_alive = 0;
  timeout = -1;   // -1 indicates that this value has not been set
  max_conns = -1; // -1 indicates that this value has not been set
  length = -1;    // -1 indicates that this value has not been set
  no_cache = 0;
  accept_ranges = 0;
  use_cookies = 1;

  // 01/14/2009: Added members below
  current_host.Clear();
  host_port = 80;
  user_agent.Clear();
  accept.Clear();
  accept_language.Clear();
  accept_encodeing.Clear();
  accept_charset.Clear();
  proxy_connection.Clear();
  proxy_auth.Clear();
  referer.Clear();
  connection.Clear();
}

void gxsHTTPHeader::Copy(const gxsHTTPHeader &ob)
{
  // 01/14/2009: Avoid self copying
  if(this == &ob) return;

  http_header = ob.http_header;
  authentication_scheme = ob.authentication_scheme;
  realm = ob.realm;
  current_server = ob.current_server;
  location = ob.location;
  http_last_modified = ob.http_last_modified;
  date = ob.date;
  http_expires = ob.http_expires;
  file_extension = ob.file_extension;
  content_type = ob.content_type;
  mime_type = ob.mime_type;
  etag = ob.etag;
  pragma = ob.pragma;
  cache_control = ob.cache_control;
  content_encoding = ob.content_encoding;
  http_version = ob.http_version;
  http_status = ob.http_status;
  authentication_needed = ob.authentication_needed;
  not_found = ob.not_found;
  keep_alive = ob.keep_alive;
  timeout = ob.timeout;
  max_conns = ob.max_conns;
  length = ob.length;
  no_cache = ob.no_cache;
  accept_ranges = ob.accept_ranges;
  auth_cookie = ob.auth_cookie;
  use_cookies = ob.use_cookies;
  method = ob.method;
  fname = ob.fname;
  post_data = ob.post_data;

  // Copy the list
  gxListNode<gxsNetscapeCookie> *ptr = ob.netscape_cookies.GetHead();
  netscape_cookies.ClearList();
  while(ptr) {
    netscape_cookies.Add(ptr->data);
    ptr = ptr->next;
  }

  // 01/14/2009: Add members below
  current_host = ob.current_host;
  host_port = ob.host_port;
  user_agent = ob.user_agent;
  accept = ob.accept;
  accept_language = ob.accept_language;
  accept_encodeing = ob.accept_encodeing;
  accept_charset = ob.accept_charset;
  proxy_connection = ob.proxy_connection;
  proxy_connection = ob.proxy_connection;
  proxy_auth = ob.proxy_auth;
  referer = ob.referer;
  connection = ob.connection;
}

GXDLCODE_API int operator==(const gxsHTTPHeader &a, const gxsHTTPHeader &b)
{
  if(a.http_header != b.http_header) return 0;
  return 1;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
