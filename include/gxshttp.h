// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Header File Name: gxshttp.h
// C++ Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 01/25/2000
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

10/26/2026: DataReel 7 replaces gxsHTTPHeader, the cookie list and 
the method and status code arrays with gxsHTTPMessage, which holds 
a request or response line, its headers and its body as std::string 
values, and functions to read and write one over a TLS connection.
==============================================================
*/
// ----------------------------------------------------------- // 
#ifndef __GX_HTTP_HPP__
#define __GX_HTTP_HPP__

#include "gxdlcode.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class gxsTLSConnection;

// An HTTP/1.1 request or response
struct GXDLCODE_API gxsHTTPMessage
{
  gxsHTTPMessage() { Clear(); }
  void Clear();

  // Header access. Names are compared without regard to case.
  std::string Header(std::string_view name) const; // Empty if not set
  bool HasHeader(std::string_view name) const;
  void SetHeader(std::string_view name, std::string_view value);
  void AddHeader(std::string_view name, std::string_view value);
  void RemoveHeader(std::string_view name);

  // Request line fields
  std::string method;  // GET, POST, ...
  std::string target;  // /path?query
  // Response line fields
  int status;          // 200, 404, ...
  std::string reason;  // OK, Not Found, ...

  std::string version; // HTTP/1.1
  std::vector<std::pair<std::string, std::string>> headers;
  std::string body;
};

// Write a request or response. The Content-Length header is set from
// the body unless Transfer-Encoding is set, or the body is empty and
// Content-Length is already set (a response to HEAD). Returns 0 if
// successful.
GXDLCODE_API int gxsHTTPWriteRequest(gxsTLSConnection &conn, 
				     gxsHTTPMessage &msg);
GXDLCODE_API int gxsHTTPWriteResponse(gxsTLSConnection &conn, 
				      gxsHTTPMessage &msg);

// Read a request or response, including a Content-Length or chunked
// body. A response to a HEAD request has no body; pass head_request.
// Bodies larger than max_body fail. Returns 0 if successful.
GXDLCODE_API int gxsHTTPReadRequest(gxsTLSConnection &conn, 
				    gxsHTTPMessage &msg,
				    std::size_t max_body = 16*1024*1024);
GXDLCODE_API int gxsHTTPReadResponse(gxsTLSConnection &conn, 
				     gxsHTTPMessage &msg,
				     bool head_request = false,
				     std::size_t max_body = 1024*1024*1024);

// Standard reason phrase for a status code, such as "Not Found"
GXDLCODE_API const char *gxsHTTPStatusCodeMessage(int status);

#endif // __GX_HTTP_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
