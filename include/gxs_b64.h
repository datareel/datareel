// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Header File Name: gxs_b64.h
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
*/
// ----------------------------------------------------------- // 
#ifndef __GX_BASE64_HPP__
#define __GX_BASE64_HPP__

#include "gxdlcode.h"

#include "gxstring.h"

GXDLCODE_API int gxsCalculateBase64Length(int len);
GXDLCODE_API void gxsBase64Encode(const char *s, char *strbuf, int length);
GXDLCODE_API int gxsBase64ConvertToNumber(unsigned char c);
GXDLCODE_API void gxsBase64Decode(const char *s, char *strbuf);
GXDLCODE_API int gxsBasicAuthenticationEncode(const char *user, 
					      const char *passwd,
					      const char *header, 
					      gxString &wwwauth);
GXDLCODE_API void gxsBasicAuthenticationDecode(const gxString &wwwauth, 
					       gxString &user,
					       gxString &passwd);

#endif // __GX_BASE64_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
