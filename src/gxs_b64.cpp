// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxs_b64.h
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
#include "gxdlcode.h"

#include <stdio.h>
#include "gxs_b64.h"

// Base 64 conversion table
const char gxsBase64table[64] = {
  'A','B','C','D','E','F','G','H',
  'I','J','K','L','M','N','O','P',
  'Q','R','S','T','U','V','W','X',
  'Y','Z','a','b','c','d','e','f',
  'g','h','i','j','k','l','m','n',
  'o','p','q','r','s','t','u','v',
  'w','x','y','z','0','1','2','3',
  '4','5','6','7','8','9','+','/'
};

GXDLCODE_API int gxsCalculateBase64Length(int len)
// Calculate the number of bytes it will take to store a
// base 64 encode string.
{
  return 4 * (((len) + 2) / 3);
}

GXDLCODE_API int gxsBase64ConvertToNumber(unsigned char c)
// Convert the encoded base 64 byte to a number.
{
  if(c >= 'A' && c <= 'Z') return (c - 'A');
  if(c >= 'a' && c <= 'z') return (c - 'a' + 26);
  if(c >= '0' && c <= '9') return (c - '0' + 52);
  if(c == '+') return 62;
  if(c == '/') return 63;
  return -1;
}

GXDLCODE_API void gxsBase64Decode(const char *s, char *strbuf)
// Decode the encoded string "s" from a base64 format and pass
// back the decoded string in the "strbuf" variable.
{
  int i, ii;
  unsigned char sbuf[255]; 
  unsigned char *p = (unsigned char *)strbuf;
  int sbufSize = 0;
  int length = strlen(s);
  unsigned char out;

  for(i = 0; i < 255; i++) sbuf[i] = 0;

  for(ii = 0; ii < length; ii++) {
    i = gxsBase64ConvertToNumber(*s);

    if(i >= 0)
      sbuf[sbufSize++]=(unsigned char) i;
    if(sbufSize == 4) {
      out = (unsigned char)((sbuf[0] << 2) | (sbuf[1] >> 4));
      *p++ = out;
      
      out = (unsigned char)((sbuf[1] << 4) | (sbuf[2] >> 2));
      if(sbufSize > 2) *p++ = out;
      
      out = (unsigned char)((sbuf[2] << 6) | sbuf[3]);
      if(sbufSize > 3) *p++ = out;
      
      sbufSize=0;
    }
    s++;
  }

  // Decode the remaining bytes
  if(sbufSize) {
    out = (unsigned char)((sbuf[0] << 2) | (sbuf[1] >> 4));
    *p++ = out;
    
    out = (unsigned char)((sbuf[1] << 4) | (sbuf[2] >> 2));
    if(sbufSize > 2) *p++ = out;
    
    out = (unsigned char)((sbuf[2] << 6) | sbuf[3]);
    if(sbufSize > 3) *p++ = out;
  }
  
  *p = '\0'; // Ensure null termination
}

GXDLCODE_API void gxsBase64Encode(const char *s, char *strbuf, int length)
// Encode the specified string "s" to a base64 format and pass
// back the encoded string in the "strbuf" variable.
{
  int i;
  unsigned char *p = (unsigned char *)strbuf;

  // Transform the 3x8 bits to 4x6 bits, as required by base64 encoding
  for(i = 0; i < length; i += 3) {
    *p++ = gxsBase64table[s[0] >> 2];
    *p++ = gxsBase64table[((s[0] & 3) << 4) + (s[1] >> 4)];
    *p++ = gxsBase64table[((s[1] & 0xf) << 2) + (s[2] >> 6)];
    *p++ = gxsBase64table[s[2] & 0x3f];
    s += 3;
  }

  // Pad the result if necessary
  if(i == length + 1)
    *(p - 1) = '=';
  else if(i == length + 2)
    *(p - 1) = *(p - 2) = '=';

  *p = '\0'; // Null terminate the string
}

GXDLCODE_API int gxsBasicAuthenticationEncode(const char *user, const char *passwd,
					      const char *header, gxString &wwwauth)
// Write an authentication header for the basic authentication scheme using
// base 64 encoding. The "basic" authentication scheme is based on the model
// that the user agent must authenticate itself with a user-ID and a password
// for each realm. The realm value should be considered an opaque string which
// can only be compared for equality with other realms on that server. The
// server will authorize the request only if it can validate the user-ID and
// password for the protection space of the Request-URI. Returns false if
// an error occurs.
{
  char *t1, *t2, *res;
  int len1 = strlen(user) + 1 + strlen(passwd);
  int len2 = gxsCalculateBase64Length(len1);
  t1 = new char[len1 + 1];
  if(!t1) return 0;
  sprintf(t1, "%s:%s", user, passwd);
  t2 = new char[1 + len2];
  if(!t2) {
    delete[] t1;
    return 0;
  }
  gxsBase64Encode(t1, t2, len1);
  res = new char[len2 + 11 + strlen(header)];
  if(!res) {
    delete[] t1;
    delete[] t2;
    return 0;
  }
  sprintf(res, "%s: Basic %s\r\n", header, t2);
  wwwauth = res;

  // Free the memory allocated for the strings
  delete[] t1;
  delete[] t2;
  delete[] res;

  return 1;
}

GXDLCODE_API void gxsBasicAuthenticationDecode(const gxString &wwwauth, gxString &user,
					       gxString &passwd)
// Decode a basic authentication line and pass back the username and
// password in the "user" and "passwd" variables.
{
  gxString sbuf(wwwauth);

  // Remove the header information
  sbuf.TrimTrailingSpaces();
  sbuf.DeleteBeforeLastIncluding(" ");

  char str[1024];
  for(int i = 0; i< 1024; i++) str[i] = 0;
  gxsBase64Decode(sbuf.c_str(), str);
  user = str;
  user.DeleteAfterIncluding(":");
  passwd = str;
  passwd.DeleteBeforeIncluding(":");
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
