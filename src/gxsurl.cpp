// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxsurl.cpp
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
#include "gxdlcode.h"

#include "gxsurl.h"
#include "strutil.h"

namespace datareel {



void gxsURLInfo::Clear()
{
  scheme.clear(); user.clear(); passwd.clear(); host.clear();
  path.clear(); query.clear(); fragment.clear();
  port = 0;
  port_given = false;
}

std::string gxsURLInfo::PathAndQuery() const
{
  std::string s = path.empty() ? std::string("/") : path;
  if(!query.empty()) s += "?" + query;
  return s;
}

std::string gxsURLInfo::HostAndPort() const
{
  std::string h = (host.find(':') != std::string::npos) ? 
    "[" + host + "]" : host;
  if(port_given && port != gxsURL::DefaultPort(scheme)) {
    h += ":" + std::to_string(port);
  }
  return h;
}

std::string gxsURLInfo::ToString() const
{
  std::string s = scheme + "://";
  if(!user.empty()) {
    s += gxsURL::Encode(user);
    if(!passwd.empty()) s += ":" + gxsURL::Encode(passwd);
    s += "@";
  }
  s += HostAndPort() + PathAndQuery();
  return s;
}

bool gxsURLInfo::IsSecure() const
{
  return scheme == "https" || scheme == "ftps" || scheme == "wss" ||
    scheme == "smtps" || scheme == "pop3s" || scheme == "imaps";
}

int gxsURL::DefaultPort(std::string_view scheme)
{
  std::string s = to_lower(scheme);
  if(s == "https" || s == "wss") return 443;
  if(s == "http" || s == "ws") return 80;
  if(s == "ftps") return 990;
  if(s == "ftp") return 21;
  if(s == "smtps") return 465;
  if(s == "smtp") return 25;
  if(s == "pop3s") return 995;
  if(s == "pop3") return 110;
  if(s == "imaps") return 993;
  if(s == "telnet") return 23;
  return 0;
}

bool gxsURL::Parse(std::string_view url, gxsURLInfo &u)
{
  u.Clear();
  url = trim_right_view(trim_left_view(url));

  std::size_t p = url.find("://");
  if(p == std::string_view::npos || p == 0) return false;
  u.scheme = to_lower(url.substr(0, p));
  for(char c : u.scheme) {
    if(!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || 
	 c == '+' || c == '-' || c == '.')) return false;
  }
  std::string_view rest = url.substr(p + 3);

  // The fragment and query end the URL in that order
  std::size_t h = rest.find('#');
  if(h != std::string_view::npos) {
    u.fragment = std::string(rest.substr(h + 1));
    rest = rest.substr(0, h);
  }
  std::size_t q = rest.find('?');
  std::string_view before_query = rest;
  if(q != std::string_view::npos) {
    u.query = std::string(rest.substr(q + 1));
    before_query = rest.substr(0, q);
  }

  // Authority runs to the first '/'
  std::size_t slash = before_query.find('/');
  std::string_view authority = before_query.substr(0, slash);
  if(slash != std::string_view::npos) {
    u.path = std::string(before_query.substr(slash));
  }

  std::size_t at = authority.rfind('@');
  if(at != std::string_view::npos) {
    std::string_view userinfo = authority.substr(0, at);
    authority = authority.substr(at + 1);
    std::size_t colon = userinfo.find(':');
    u.user = Decode(userinfo.substr(0, colon));
    if(colon != std::string_view::npos) {
      u.passwd = Decode(userinfo.substr(colon + 1));
    }
  }

  std::string_view port_part;
  if(!authority.empty() && authority[0] == '[') { // IPv6 literal
    std::size_t close = authority.find(']');
    if(close == std::string_view::npos) return false;
    u.host = std::string(authority.substr(1, close - 1));
    std::string_view after = authority.substr(close + 1);
    if(!after.empty()) {
      if(after[0] != ':') return false;
      port_part = after.substr(1);
    }
  }
  else {
    std::size_t colon = authority.rfind(':');
    u.host = std::string(authority.substr(0, colon));
    if(colon != std::string_view::npos) port_part = authority.substr(colon + 1);
  }
  if(u.host.empty()) return false;
  u.host = to_lower(u.host);

  if(!port_part.empty()) {
    int port = 0;
    for(char c : port_part) {
      if(c < '0' || c > '9') return false;
      port = port * 10 + (c - '0');
      if(port > 65535) return false;
    }
    if(port == 0) return false;
    u.port = port;
    u.port_given = true;
  }
  else {
    u.port = DefaultPort(u.scheme);
  }
  return true;
}

std::string gxsURL::Resolve(const gxsURLInfo &base, std::string_view location)
{
  std::string loc = trim(location);
  if(loc.find("://") != std::string::npos) return loc;
  std::string prefix = base.scheme + "://" + base.HostAndPort();
  if(starts_with(loc, "//")) return base.scheme + ":" + loc;
  if(starts_with(loc, "/")) return prefix + loc;
  std::string dir = base.path.empty() ? std::string("/") : base.path;
  std::size_t last = dir.rfind('/');
  dir = dir.substr(0, last + 1);
  return prefix + dir + loc;
}

std::string gxsURL::Encode(std::string_view s)
{
  static const char hex[] = "0123456789ABCDEF";
  std::string out;
  for(unsigned char c : s) {
    if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || 
       (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~') {
      out += char(c);
    }
    else {
      out += '%';
      out += hex[c >> 4];
      out += hex[c & 0x0F];
    }
  }
  return out;
}

std::string gxsURL::Decode(std::string_view s, bool plus_is_space)
{
  auto hexval = [](char c) -> int {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };
  std::string out;
  for(std::size_t i = 0; i < s.size(); i++) {
    if(s[i] == '%' && i + 2 < s.size()) {
      int a = hexval(s[i+1]), b = hexval(s[i+2]);
      if(a >= 0 && b >= 0) {
	out += char(a * 16 + b);
	i += 2;
	continue;
      }
    }
    if(plus_is_space && s[i] == '+') { out += ' '; continue; }
    out += s[i];
  }
  return out;
}
} // namespace datareel

// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
