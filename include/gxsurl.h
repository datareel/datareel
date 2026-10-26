// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Header File Name: gxsurl.h
// Compiler Used: MSVC, GCC
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

The gxsURL class is used to parse uniform resource locators.

Changes:
==============================================================
10/26/2026: DataReel 7 rewrites the URL parser on std::string. 
Parse() handles IPv6 literal hosts ([2001:db8::1]:8443), user 
information, query strings and fragments. Resolve() turns a 
relative redirect location into an absolute URL.
==============================================================
*/
// ----------------------------------------------------------- // 
#ifndef __GX_URL_HPP__
#define __GX_URL_HPP__

#include "gxdlcode.h"

#include <string>
#include <string_view>

namespace datareel {


// Parts of a URL: scheme://user:passwd@host:port/path?query#fragment
struct GXDLCODE_API gxsURLInfo
{
  gxsURLInfo() { Clear(); }
  void Clear();

  std::string PathAndQuery() const; // "/path?query", at least "/"
  std::string HostAndPort() const;  // Host header value
  std::string ToString() const;     // The URL without the fragment
  bool IsSecure() const;            // https, ftps, wss or smtps style

  std::string scheme;   // Lower case, such as "https"
  std::string user;     // Decoded user name
  std::string passwd;   // Decoded password
  std::string host;     // Host name or address, IPv6 without brackets
  int port;             // Explicit port or the scheme's default
  bool port_given;      // True if the URL had an explicit port
  std::string path;     // Path, still percent-encoded
  std::string query;    // Query string without the '?'
  std::string fragment; // Fragment without the '#'
};

class GXDLCODE_API gxsURL
{
public:
  // Parse an absolute URL. Returns false if it has no scheme or host.
  static bool Parse(std::string_view url, gxsURLInfo &u);

  // Default port for a scheme, or 0 if it is not known
  static int DefaultPort(std::string_view scheme);

  // Turn a redirect Location (absolute, /path, or relative) into an
  // absolute URL based on the request URL
  static std::string Resolve(const gxsURLInfo &base, std::string_view location);

  // Percent-encode everything except unreserved characters (RFC 3986)
  static std::string Encode(std::string_view s);

  // Decode %XX sequences, and '+' as a space when plus_is_space is true
  static std::string Decode(std::string_view s, bool plus_is_space = false);
};

} // namespace datareel

#endif // __GX_URL_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
