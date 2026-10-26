// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxs_b64.h
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

Standalone functions used for base 64 password encoding and
decoding.

NOTE: Data encoding is a very weak form of asymmetric encryption 
and can be cracked more readily than stronger forms of asymmetric 
encryption. The data encoding routines provided with this library 
should not be implemented in any measure to stop malicious hackers 
on the Internet from breaking into communication systems or from 
compromising encoded data files. The data encoding routines are an 
alternative to sending plain text messages or writing plain text to 
database or configuration files. These routines are simple, fast, 
and integrated into the library so you do not need to include any 
other third party libraries when building code using this library. 
If you need a comprehensive strong encryption toolkit please visit 
the http://www.openssl.org Website to download the latest version 
of OpenSSL.

Changes:
==============================================================
10/26/2026: DataReel 7 replaces the char buffer functions with 
std::string functions. Decoding rejects characters outside the 
base 64 alphabet instead of reading past the input.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "gxs_b64.h"

static const char gxsBase64Chars[] = 
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

GXDLCODE_API std::string gxsBase64Encode(const void *data, std::size_t len)
{
  const unsigned char *p = (const unsigned char *)data;
  std::string out;
  out.reserve(((len + 2) / 3) * 4);
  std::size_t i = 0;
  for(; i + 2 < len; i += 3) {
    unsigned v = (unsigned(p[i]) << 16) | (unsigned(p[i+1]) << 8) | p[i+2];
    out += gxsBase64Chars[(v >> 18) & 0x3F];
    out += gxsBase64Chars[(v >> 12) & 0x3F];
    out += gxsBase64Chars[(v >> 6) & 0x3F];
    out += gxsBase64Chars[v & 0x3F];
  }
  if(i < len) {
    unsigned v = unsigned(p[i]) << 16;
    if(i + 1 < len) v |= unsigned(p[i+1]) << 8;
    out += gxsBase64Chars[(v >> 18) & 0x3F];
    out += gxsBase64Chars[(v >> 12) & 0x3F];
    out += (i + 1 < len) ? gxsBase64Chars[(v >> 6) & 0x3F] : '=';
    out += '=';
  }
  return out;
}

static int gxsBase64Value(char c)
{
  if(c >= 'A' && c <= 'Z') return c - 'A';
  if(c >= 'a' && c <= 'z') return c - 'a' + 26;
  if(c >= '0' && c <= '9') return c - '0' + 52;
  if(c == '+') return 62;
  if(c == '/') return 63;
  return -1;
}

GXDLCODE_API bool gxsBase64Decode(std::string_view s, std::string &out)
{
  out.clear();
  unsigned v = 0;
  int bits = 0;
  int pad = 0;
  for(char c : s) {
    if(c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
    if(c == '=') { pad++; continue; }
    if(pad) return false; // Data after padding
    int d = gxsBase64Value(c);
    if(d < 0) return false;
    v = (v << 6) | unsigned(d);
    bits += 6;
    if(bits >= 8) {
      bits -= 8;
      out += char((v >> bits) & 0xFF);
    }
  }
  return pad <= 2;
}

GXDLCODE_API std::string gxsBasicAuthenticationEncode(std::string_view user,
						      std::string_view passwd)
{
  std::string creds(user);
  creds += ':';
  creds += passwd;
  return "Basic " + gxsBase64Encode(creds);
}

GXDLCODE_API bool gxsBasicAuthenticationDecode(std::string_view value, 
					       std::string &user,
					       std::string &passwd)
{
  user.clear(); passwd.clear();
  while(!value.empty() && (value[0] == ' ' || value[0] == '\t')) {
    value.remove_prefix(1);
  }
  if(value.size() < 6) return false;
  for(int i = 0; i < 5; i++) {
    if((value[i] | 0x20) != "basic"[i]) return false;
  }
  if(value[5] != ' ') return false;
  std::string creds;
  if(!gxsBase64Decode(value.substr(6), creds)) return false;
  std::size_t colon = creds.find(':');
  if(colon == std::string::npos) return false;
  user = creds.substr(0, colon);
  passwd = creds.substr(colon + 1);
  return true;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
