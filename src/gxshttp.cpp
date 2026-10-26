// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxshttp.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 01/25/2000
// Date Last Modified: 10/26/2026
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

10/26/2026: DataReel 7 replaces gxsHTTPHeader, the cookie list and 
the method and status code arrays with gxsHTTPMessage, which holds 
a request or response line, its headers and its body as std::string 
values, and functions to read and write one over a TLS connection.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <cstdlib>
#include "gxshttp.h"
#include "gxstls.h"
#include "strutil.h"

namespace datareel {



void gxsHTTPMessage::Clear()
{
  method.clear(); target.clear(); reason.clear(); body.clear();
  version = "HTTP/1.1";
  status = 0;
  headers.clear();
}

std::string gxsHTTPMessage::Header(std::string_view name) const
{
  for(const auto &h : headers) {
    if(iequals(h.first, name)) return h.second;
  }
  return std::string();
}

bool gxsHTTPMessage::HasHeader(std::string_view name) const
{
  for(const auto &h : headers) {
    if(iequals(h.first, name)) return true;
  }
  return false;
}

void gxsHTTPMessage::SetHeader(std::string_view name, std::string_view value)
{
  RemoveHeader(name);
  AddHeader(name, value);
}

void gxsHTTPMessage::AddHeader(std::string_view name, std::string_view value)
{
  headers.emplace_back(std::string(name), std::string(value));
}

void gxsHTTPMessage::RemoveHeader(std::string_view name)
{
  for(std::size_t i = 0; i < headers.size(); ) {
    if(iequals(headers[i].first, name)) headers.erase(headers.begin() + i);
    else i++;
  }
}

static bool has_crlf(std::string_view s)
{
  return s.find('\r') != std::string_view::npos || 
    s.find('\n') != std::string_view::npos;
}

static int write_message(gxsTLSConnection &conn, gxsHTTPMessage &msg,
			 const std::string &start_line)
{
  // An empty body keeps a Content-Length set by the caller, which is
  // how a response to HEAD gives the size of the resource
  if(!msg.HasHeader("Transfer-Encoding") &&
     !(msg.body.empty() && msg.HasHeader("Content-Length"))) {
    msg.SetHeader("Content-Length", std::to_string(msg.body.size()));
  }
  std::string out = start_line + "\r\n";
  for(const auto &h : msg.headers) {
    // Refuse header injection through CR or LF in a name or value
    if(has_crlf(h.first) || has_crlf(h.second)) return -1;
    out += h.first + ": " + h.second + "\r\n";
  }
  out += "\r\n";
  out += msg.body;
  return conn.Send(out) < 0 ? -1 : 0;
}

GXDLCODE_API int gxsHTTPWriteRequest(gxsTLSConnection &conn, 
				     gxsHTTPMessage &msg)
{
  if(has_crlf(msg.method) || has_crlf(msg.target)) return -1;
  std::string target = msg.target.empty() ? std::string("/") : msg.target;
  return write_message(conn, msg, msg.method + " " + target + " " + 
		       msg.version);
}

GXDLCODE_API int gxsHTTPWriteResponse(gxsTLSConnection &conn, 
				      gxsHTTPMessage &msg)
{
  std::string reason = msg.reason.empty() ? 
    std::string(gxsHTTPStatusCodeMessage(msg.status)) : msg.reason;
  if(has_crlf(reason)) return -1;
  return write_message(conn, msg, msg.version + " " + 
		       std::to_string(msg.status) + " " + reason);
}

static int read_headers(gxsTLSConnection &conn, gxsHTTPMessage &msg)
{
  std::string line;
  std::size_t total = 0;
  for(;;) {
    if(!conn.ReadLine(line, 16384)) return -1;
    if(line.empty()) return 0;
    total += line.size();
    if(total > 256*1024 || msg.headers.size() > 200) return -1;
    if(line[0] == ' ' || line[0] == '\t') { // Obsolete folded header
      if(msg.headers.empty()) return -1;
      msg.headers.back().second += " " + trim(line);
      continue;
    }
    std::size_t colon = line.find(':');
    if(colon == std::string::npos || colon == 0) return -1;
    msg.headers.emplace_back(trim(line.substr(0, colon)), 
			     trim(line.substr(colon + 1)));
  }
}

static int read_body(gxsTLSConnection &conn, gxsHTTPMessage &msg,
		     std::size_t max_body, bool read_to_close)
{
  msg.body.clear();
  if(ifind(msg.Header("Transfer-Encoding"), "chunked") != std::string::npos) {
    std::string line, chunk;
    for(;;) {
      if(!conn.ReadLine(line, 1024)) return -1;
      std::size_t semi = line.find(';'); // Ignore chunk extensions
      std::string size_str = trim(line.substr(0, semi));
      if(size_str.empty()) return -1;
      char *end = 0;
      unsigned long long n = std::strtoull(size_str.c_str(), &end, 16);
      if(*end != '\0') return -1;
      if(n == 0) break;
      if(n > max_body || msg.body.size() + n > max_body) return -1;
      if(conn.Read(chunk, (std::size_t)n) != (int)n) return -1;
      msg.body += chunk;
      if(!conn.ReadLine(line, 2) || !line.empty()) return -1;
    }
    // Trailer headers end with an empty line
    for(;;) {
      if(!conn.ReadLine(line, 16384)) return -1;
      if(line.empty()) break;
    }
    return 0;
  }
  std::string len = msg.Header("Content-Length");
  if(!len.empty()) {
    char *end = 0;
    unsigned long long n = std::strtoull(len.c_str(), &end, 10);
    if(*end != '\0' || n > max_body) return -1;
    if(conn.Read(msg.body, (std::size_t)n) != (int)n) return -1;
    return 0;
  }
  if(read_to_close) {
    return conn.ReadAll(msg.body, max_body) ? 0 : -1;
  }
  return 0;
}

GXDLCODE_API int gxsHTTPReadRequest(gxsTLSConnection &conn, 
				    gxsHTTPMessage &msg, std::size_t max_body)
{
  msg.Clear();
  std::string line;
  do { // Ignore empty lines before the request line
    if(!conn.ReadLine(line, 16384)) return -1;
  } while(line.empty());
  std::vector<std::string> parts = split(line, ' ', true);
  if(parts.size() != 3 || !starts_with(parts[2], "HTTP/")) return -1;
  msg.method = parts[0];
  msg.target = parts[1];
  msg.version = parts[2];
  if(read_headers(conn, msg) != 0) return -1;
  return read_body(conn, msg, max_body, false);
}

GXDLCODE_API int gxsHTTPReadResponse(gxsTLSConnection &conn, 
				     gxsHTTPMessage &msg, bool head_request,
				     std::size_t max_body)
{
  for(;;) {
    msg.Clear();
    std::string line;
    if(!conn.ReadLine(line, 16384)) return -1;
    if(!starts_with(line, "HTTP/")) return -1;
    std::size_t sp1 = line.find(' ');
    if(sp1 == std::string::npos) return -1;
    msg.version = line.substr(0, sp1);
    msg.status = std::atoi(line.c_str() + sp1 + 1);
    std::size_t sp2 = line.find(' ', sp1 + 1);
    if(sp2 != std::string::npos) msg.reason = line.substr(sp2 + 1);
    if(msg.status < 100 || msg.status > 999) return -1;
    if(read_headers(conn, msg) != 0) return -1;
    if(msg.status >= 100 && msg.status < 200) continue; // 100 Continue
    if(head_request || msg.status == 204 || msg.status == 304) return 0;
    return read_body(conn, msg, max_body, true);
  }
}

GXDLCODE_API const char *gxsHTTPStatusCodeMessage(int status)
{
  switch(status) {
    case 100: return "Continue";
    case 101: return "Switching Protocols";
    case 200: return "OK";
    case 201: return "Created";
    case 202: return "Accepted";
    case 204: return "No Content";
    case 206: return "Partial Content";
    case 301: return "Moved Permanently";
    case 302: return "Found";
    case 303: return "See Other";
    case 304: return "Not Modified";
    case 307: return "Temporary Redirect";
    case 308: return "Permanent Redirect";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 408: return "Request Timeout";
    case 409: return "Conflict";
    case 411: return "Length Required";
    case 413: return "Content Too Large";
    case 414: return "URI Too Long";
    case 415: return "Unsupported Media Type";
    case 429: return "Too Many Requests";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 502: return "Bad Gateway";
    case 503: return "Service Unavailable";
    case 504: return "Gateway Timeout";
    case 505: return "HTTP Version Not Supported";
    default: break;
  }
  return "Unknown";
}
} // namespace datareel

// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
