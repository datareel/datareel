// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Header File Name: gxs_b64.h
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
#ifndef __GX_BASE64_HPP__
#define __GX_BASE64_HPP__

#include "gxdlcode.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace datareel {


// Standard base 64 (RFC 4648) with = padding
GXDLCODE_API std::string gxsBase64Encode(const void *data, std::size_t len);
inline std::string gxsBase64Encode(std::string_view s) {
  return gxsBase64Encode(s.data(), s.size());
}

// Decode into out (which may hold binary data). White space is
// ignored. Returns false if the input is not valid base 64.
GXDLCODE_API bool gxsBase64Decode(std::string_view s, std::string &out);

// HTTP basic authentication: "Basic " followed by base64(user:passwd)
GXDLCODE_API std::string gxsBasicAuthenticationEncode(std::string_view user,
						      std::string_view passwd);

// Decode an Authorization header value. Returns false if it is not
// basic authentication.
GXDLCODE_API bool gxsBasicAuthenticationDecode(std::string_view value, 
					       std::string &user,
					       std::string &passwd);

} // namespace datareel

#endif // __GX_BASE64_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
