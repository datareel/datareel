// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxshttp.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 01/25/2000
// Date Last Modified: 11/01/2026
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

11/01/2026: Security fixes. The readers run on an http_source so a
buffer can be parsed (gxsHTTPParseRequest/Response). Interim 1xx
responses are capped at 10 and 101 is an error; status codes must
be 3 digits; control characters and non-token header names are
refused on read and write; chunked plus Content-Length and
conflicting Content-Length values are refused; max_body is clamped
to INT_MAX.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <climits>
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

// 11/01/2026: RFC 7230 field checks. has_crlf() only caught CR and
// LF; NUL and the other control characters are refused too, and
// header names must be tokens ("Host : x" or "X\0Y" are smuggling
// vectors when a proxy reads them differently).
GXDLCODE_API bool gxsHTTPIsToken(std::string_view s)
{
  if(s.empty()) return false;
  for(unsigned char c : s) {
    if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || 
       (c >= '0' && c <= '9')) continue;
    switch(c) {
      case '!': case '#': case '$': case '%': case '&': case '\'': 
      case '*': case '+': case '-': case '.': case '^': case '_': 
      case '`': case '|': case '~':
	continue;
      default:
	return false;
    }
  }
  return true;
}

GXDLCODE_API bool gxsHTTPHasControlChars(std::string_view s, bool allow_tab)
{
  for(unsigned char c : s) {
    if(c == '\t' && allow_tab) continue;
    if(c < 0x20 || c == 0x7F) return true;
  }
  return false;
}

GXDLCODE_API bool gxsHTTPValidHeader(std::string_view name, 
				     std::string_view value)
{
  return gxsHTTPIsToken(name) && !gxsHTTPHasControlChars(value, true);
}

// 11/01/2026: A request target is printable ASCII or obs-text with
// no spaces
static bool valid_target(std::string_view s)
{
  if(s.empty()) return false;
  for(unsigned char c : s) {
    if(c <= 0x20 || c == 0x7F) return false;
  }
  return true;
}

static bool valid_version(std::string_view s)
{
  return starts_with(s, "HTTP/") && s.size() <= 16 && valid_target(s);
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
    // 11/01/2026: and any other control character or a name that is
    // not a token
    if(!gxsHTTPValidHeader(h.first, h.second)) return -1;
    out += h.first + ": " + h.second + "\r\n";
  }
  out += "\r\n";
  out += msg.body;
  return conn.Send(out) < 0 ? -1 : 0;
}

GXDLCODE_API int gxsHTTPWriteRequest(gxsTLSConnection &conn, 
				     gxsHTTPMessage &msg)
{
  std::string target = msg.target.empty() ? std::string("/") : msg.target;
  // 11/01/2026: Method must be a token and the target and version
  // must not hold spaces or control characters
  if(!gxsHTTPIsToken(msg.method) || !valid_target(target) ||
     !valid_version(msg.version)) return -1;
  return write_message(conn, msg, msg.method + " " + target + " " + 
		       msg.version);
}

GXDLCODE_API int gxsHTTPWriteResponse(gxsTLSConnection &conn, 
				      gxsHTTPMessage &msg)
{
  std::string reason = msg.reason.empty() ? 
    std::string(gxsHTTPStatusCodeMessage(msg.status)) : msg.reason;
  // 11/01/2026: Refuse every control character, not only CR LF, and
  // status codes that are not 3 digits
  if(gxsHTTPHasControlChars(reason, true) || !valid_version(msg.version) ||
     msg.status < 100 || msg.status > 999) return -1;
  return write_message(conn, msg, msg.version + " " + 
		       std::to_string(msg.status) + " " + reason);
}

// 11/01/2026: The parsers read from an http_source, so the same code
// reads a TLS connection or a buffer (gxsHTTPParseRequest(), 
// gxsHTTPParseResponse(), the tests and the fuzz target).
class http_source
{
public:
  virtual ~http_source() { }
  virtual bool ReadLine(std::string &line, std::size_t max_len) = 0;
  virtual int Read(std::string &s, std::size_t bytes) = 0;
  virtual bool ReadAll(std::string &s, std::size_t max_len) = 0;
};

class tls_source : public http_source
{
public:
  tls_source(gxsTLSConnection &c) : conn(c) { }
  bool ReadLine(std::string &line, std::size_t max_len) override {
    return conn.ReadLine(line, max_len);
  }
  int Read(std::string &s, std::size_t bytes) override {
    return conn.Read(s, bytes);
  }
  bool ReadAll(std::string &s, std::size_t max_len) override {
    return conn.ReadAll(s, max_len);
  }

private:
  gxsTLSConnection &conn;
};

class buffer_source : public http_source
{
public:
  buffer_source(std::string_view b) : buf(b), pos(0) { }
  // Same rules as gxsTLSConnection::ReadLine(): the line ends at LF,
  // a CR before it is removed and a line over max_len fails
  bool ReadLine(std::string &line, std::size_t max_len) override {
    line.clear();
    std::size_t nl = buf.find('\n', pos);
    if(nl == std::string_view::npos || nl - pos > max_len) return false;
    line.assign(buf.data() + pos, nl - pos);
    pos = nl + 1;
    if(!line.empty() && line.back() == '\r') line.pop_back();
    return true;
  }
  int Read(std::string &s, std::size_t bytes) override {
    std::size_t n = buf.size() - pos;
    if(n > bytes) n = bytes;
    s.assign(buf.data() + pos, n);
    pos += n;
    return (int)n;
  }
  bool ReadAll(std::string &s, std::size_t max_len) override {
    if(buf.size() - pos > max_len) return false;
    s.assign(buf.data() + pos, buf.size() - pos);
    pos = buf.size();
    return true;
  }
  std::size_t Used() const { return pos; }

private:
  std::string_view buf;
  std::size_t pos;
};

static int read_headers(http_source &conn, gxsHTTPMessage &msg)
{
  std::string line;
  std::size_t total = 0;
  for(;;) {
    if(!conn.ReadLine(line, 16384)) return -1;
    if(line.empty()) return 0;
    total += line.size();
    if(total > 256*1024 || msg.headers.size() > 200) return -1;
    // 11/01/2026: No control characters other than HTAB anywhere in
    // a header line (NUL, bare CR, ...)
    if(gxsHTTPHasControlChars(line, true)) return -1;
    if(line[0] == ' ' || line[0] == '\t') { // Obsolete folded header
      if(msg.headers.empty()) return -1;
      msg.headers.back().second += " " + trim(line);
      continue;
    }
    std::size_t colon = line.find(':');
    if(colon == std::string::npos || colon == 0) return -1;
    // 11/01/2026: The name must be a token with no white space before
    // the colon (RFC 7230 3.2.4)
    std::string name = line.substr(0, colon);
    if(!gxsHTTPIsToken(name)) return -1;
    msg.headers.emplace_back(name, trim(line.substr(colon + 1)));
  }
}

static bool all_digits(std::string_view s, bool hex = false)
{
  if(s.empty()) return false;
  for(char c : s) {
    if(c >= '0' && c <= '9') continue;
    if(hex && ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) continue;
    return false;
  }
  return true;
}

// 11/01/2026: Work out how the body is framed. Fails on both
// Transfer-Encoding and Content-Length, conflicting Content-Length
// values, a Content-Length that is not a decimal number, or chunked
// that is not the last transfer coding. te_other is set when there
// is a Transfer-Encoding that does not end in chunked.
static int message_framing(const gxsHTTPMessage &msg, bool &chunked, 
			   bool &te_other, bool &has_length, 
			   unsigned long long &length)
{
  chunked = te_other = has_length = false;
  length = 0;
  bool has_te = false;
  std::string te;
  for(const auto &h : msg.headers) {
    if(iequals(h.first, "Transfer-Encoding")) {
      if(has_te) te += ",";
      te += h.second;
      has_te = true;
    }
    else if(iequals(h.first, "Content-Length")) {
      // A list of the same value ("5, 5") is allowed (RFC 7230 3.3.2)
      for(const auto &item : split(h.second, ',')) {
	std::string v = trim(item);
	if(!all_digits(v) || v.size() > 18) return -1;
	unsigned long long n = std::strtoull(v.c_str(), 0, 10);
	if(has_length && n != length) return -1;
	has_length = true;
	length = n;
      }
    }
  }
  if(!has_te) return 0;
  if(has_length) return -1; // Request smuggling (RFC 7230 3.3.3)
  std::vector<std::string> codings = split(te, ',');
  for(std::size_t i = 0; i < codings.size(); i++) {
    std::string c = trim(codings[i]);
    if(c.empty()) continue;
    if(iequals(c, "chunked")) {
      if(i + 1 != codings.size()) return -1; // chunked must be last
      chunked = true;
    }
  }
  te_other = !chunked;
  return 0;
}

static int read_body(http_source &conn, gxsHTTPMessage &msg,
		     std::size_t max_body, bool read_to_close)
{
  msg.body.clear();
  // 11/01/2026: Lengths are compared with the int returned by Read()
  if(max_body > (std::size_t)INT_MAX) max_body = (std::size_t)INT_MAX;
  bool chunked, te_other, has_length;
  unsigned long long length;
  if(message_framing(msg, chunked, te_other, has_length, length) != 0) {
    return -1;
  }
  if(chunked) {
    std::string line, chunk;
    for(;;) {
      if(!conn.ReadLine(line, 1024)) return -1;
      std::size_t semi = line.find(';'); // Ignore chunk extensions
      std::string size_str = trim(line.substr(0, semi));
      // 11/01/2026: Hex digits only; strtoull() also took "0x", a 
      // sign and leading white space
      if(!all_digits(size_str, true) || size_str.size() > 15) return -1;
      unsigned long long n = std::strtoull(size_str.c_str(), 0, 16);
      if(n == 0) break;
      if(n > max_body || msg.body.size() + n > max_body) return -1;
      if(conn.Read(chunk, (std::size_t)n) != (int)n) return -1;
      msg.body += chunk;
      if(!conn.ReadLine(line, 2) || !line.empty()) return -1;
    }
    // Trailer headers end with an empty line
    // 11/01/2026: at most 200 of them
    for(int lines = 0; ; lines++) {
      if(lines > 200 || !conn.ReadLine(line, 16384)) return -1;
      if(line.empty()) break;
    }
    return 0;
  }
  if(te_other) {
    // 11/01/2026: A request with an unknown transfer coding has no
    // known length (RFC 7230 3.3.3); a response runs to the close
    if(!read_to_close) return -1;
    return conn.ReadAll(msg.body, max_body) ? 0 : -1;
  }
  if(has_length) {
    if(length > max_body) return -1;
    if(conn.Read(msg.body, (std::size_t)length) != (int)length) return -1;
    return 0;
  }
  if(read_to_close) {
    return conn.ReadAll(msg.body, max_body) ? 0 : -1;
  }
  return 0;
}

static int read_request(http_source &conn, gxsHTTPMessage &msg, 
			std::size_t max_body)
{
  msg.Clear();
  std::string line;
  int empty_lines = 0;
  do { // Ignore empty lines before the request line
    // 11/01/2026: but not an endless stream of them
    if(empty_lines++ > 16 || !conn.ReadLine(line, 16384)) return -1;
  } while(line.empty());
  // 11/01/2026: Exactly "method SP target SP version", where the
  // method is a token and the target has no control characters
  std::vector<std::string> parts = split(line, ' ');
  if(parts.size() != 3 || !gxsHTTPIsToken(parts[0]) || 
     !valid_target(parts[1]) || !valid_version(parts[2])) return -1;
  msg.method = parts[0];
  msg.target = parts[1];
  msg.version = parts[2];
  if(read_headers(conn, msg) != 0) return -1;
  return read_body(conn, msg, max_body, false);
}

// 11/01/2026: "HTTP/1.1 200 OK". The status is exactly 3 digits,
// where atoi() took "200abc" or " +200".
static int parse_status_line(const std::string &line, gxsHTTPMessage &msg)
{
  if(gxsHTTPHasControlChars(line, true)) return -1;
  std::size_t sp1 = line.find(' ');
  if(sp1 == std::string::npos) return -1;
  msg.version = line.substr(0, sp1);
  if(!valid_version(msg.version)) return -1;
  std::string_view code = std::string_view(line).substr(sp1 + 1, 3);
  if(code.size() != 3 || !all_digits(code)) return -1;
  std::size_t after = sp1 + 4;
  if(after < line.size() && line[after] != ' ') return -1;
  msg.status = (code[0] - '0') * 100 + (code[1] - '0') * 10 + (code[2] - '0');
  if(msg.status < 100) return -1;
  if(after < line.size()) msg.reason = line.substr(after + 1);
  return 0;
}

static int read_response(http_source &conn, gxsHTTPMessage &msg, 
			 bool head_request, std::size_t max_body)
{
  // 11/01/2026: At most 10 interim (1xx) responses. A server that
  // sent them forever kept this loop running.
  for(int interim = 0; ; interim++) {
    msg.Clear();
    if(interim > 10) return -1;
    std::string line;
    if(!conn.ReadLine(line, 16384)) return -1;
    if(!starts_with(line, "HTTP/")) return -1;
    if(parse_status_line(line, msg) != 0) return -1;
    if(read_headers(conn, msg) != 0) return -1;
    // 11/01/2026: This client never asks to switch protocols
    if(msg.status == 101) return -1;
    if(msg.status >= 100 && msg.status < 200) continue; // 100 Continue
    if(head_request || msg.status == 204 || msg.status == 304) return 0;
    return read_body(conn, msg, max_body, true);
  }
}

GXDLCODE_API int gxsHTTPReadRequest(gxsTLSConnection &conn, 
				    gxsHTTPMessage &msg, std::size_t max_body)
{
  tls_source src(conn);
  return read_request(src, msg, max_body);
}

GXDLCODE_API int gxsHTTPReadResponse(gxsTLSConnection &conn, 
				     gxsHTTPMessage &msg, bool head_request,
				     std::size_t max_body)
{
  tls_source src(conn);
  return read_response(src, msg, head_request, max_body);
}

GXDLCODE_API int gxsHTTPParseRequest(std::string_view buf, 
				     gxsHTTPMessage &msg,
				     std::size_t max_body, std::size_t *consumed)
{
  buffer_source src(buf);
  int rv = read_request(src, msg, max_body);
  if(consumed) *consumed = src.Used();
  return rv;
}

GXDLCODE_API int gxsHTTPParseResponse(std::string_view buf, 
				      gxsHTTPMessage &msg, bool head_request,
				      std::size_t max_body, 
				      std::size_t *consumed)
{
  buffer_source src(buf);
  int rv = read_response(src, msg, head_request, max_body);
  if(consumed) *consumed = src.Used();
  return rv;
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
