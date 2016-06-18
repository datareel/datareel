// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: strutil.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 12/16/1997 
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

The string utilities are a collection of data structures and 
standalone functions used to manipulate and parse null terminated 
ASCII and Unicode character strings.

Changes:
==============================================================
09/28/2001: Added a string length parameters to the FindMatch()
and IFindMatch() functions used to set the length of the string
variable when the string is not null-terminated.

03/10/2002: Added the ULLWORDToString(), LLWORDToString(), 
StringToULLWORD(), and StringToLLWORD() functions used to convert 
64-bit signed and unsigned integer values to and from string types. 
These functions are now used throughout the library by all classes 
that require string support for 64-bit integer types.

05/27/2002: Changed all char *s function arguments to const char *s 
in all functions. This change was made to make the conversion of 
string literals const safe

05/27/2002: Changed the return types from unsigned to int in all 
Find() functions. This change was made to eliminate signed/unsigned 
warnings and errors.

06/05/2002: Added type defines, conditional includes, and the
gxStringType enumeration needed to support Unicode strings.

06/05/2002: Added standalone functions needed to work with both
ASCII and Unicode strings
 
06/05/2002: Added conversion functions used with ASCII and
Unicode string literals.

06/05/2002: Added the gxwchar class and gxwchar_t typedef used to 
operate on platform interoperable Unicode strings.

09/04/2002: Added Unicode versions of all strutil functions
included with release 4.1.

09/04/2002: All versions of the StringCat() functions now use 
null pointers as the default arguments and will return a null
string if no arguments are passed to the function from the 
caller.

04/22/2003: Added the test_for_null() function used to test
strings for null terminators.

05/04/2003: Added the StringANCompare() fucntion used to compare 
strings with alphanumeric content.

05/15/2003: Added the StringToULWORD() function used to convert
strings to unsigned long integer values.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_STRUTIL_HPP__
#define __GX_STRUTIL_HPP__

#include "gxdlcode.h"
#include <string.h> 
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

#if defined (__64_BIT_DATABASE_ENGINE__) || defined (__ENABLE_64_BIT_INTEGERS__)
#include "gxdtyp64.h"
#endif

// Platform dependent Unicode include files used 
// with compilers that have support for Unicode.
#if defined (__HAS_UNICODE__)
#if defined (__WINCE__)
#include <stddef.h>
#include <tchar.h> 
#else
#include <wchar.h>
#include <stddef.h>
#include <wctype.h>
#endif // Compiler dependent includes
#endif // __HAS_UNICODE__

// Define the wchar_t type for compilers that do have any 
// Unicode support.
#if defined (__HAS_NO_WCHAR_T__)
#if defined (__WIN32__)  // Set the WIN32 wide char type
typedef unsigned short wchar_t;
#elif defined (__UNIX__) // Set the UNIX wide char type
typedef unsigned long wchar_t; 
#else // Use 16-bit int as the default wide char type
typedef unsigned short wchar_t; 
#endif // __WIN32__ __UNIX__
#endif // __HAS_NO_WCHAR_T__

// String type enumeration used to identify string
// types in the UString base class as ASCII or Unicode.
enum gxStringType {
  gxASCII_STRING   = 0,
  gxUNICODE_STRING = 1
};

// Platform interoperable type used with 16-bit and 32-bit 
// Unicode encoding. This type is designed to be used as a 
// persistent type used in cross-platform database files and 
// shared across network connections.
class GXDLCODE_API gxwchar 
{
public:
  gxwchar(unsigned long wc = 0);
  gxwchar(const gxwchar& wc);
  gxwchar& operator=(const gxwchar& wc);
  gxwchar& operator=(const unsigned long wc);

public: // Byte ordering functions
  void UnPackBits(unsigned long wc);
  unsigned long PackBits() const;

public:
  operator unsigned long() const;

private:
  unsigned char byte[4]; // 32-bit wide character type
};

// Type define for the gxwchar_t type
#if defined (__USE_NATIVE_INT_TYPES__)
typedef unsigned long gxwchar_t; // Native type used for debugging
#else
typedef gxwchar gxwchar_t;
#endif

// Standalone functions used to work with ASCII string types 
inline char *gxstrcpy(char *to, const char *from) {
  return strcpy(to, from);
}
inline unsigned gxstrlen(const char *s) { return strlen(s); }
inline char *gxstrcat(char *des, const char *src) { return strcat(des, src); }
inline int gxstrcmp(const char *a, const char *b) { return strcmp(a, b); }

// Standalone functions used to work with Unicode string types 
GXDLCODE_API wchar_t *gxstrcpy(wchar_t *to, const wchar_t *from);
GXDLCODE_API unsigned gxstrlen(const wchar_t *s);
GXDLCODE_API wchar_t *gxstrcat(wchar_t *des, const wchar_t *src);
GXDLCODE_API int gxstrcmp(const wchar_t *a, const wchar_t *b);

// Standalone functions used to work with gxwchar_t string types 
GXDLCODE_API gxwchar_t *gxstrcpy(gxwchar_t *to, const gxwchar_t *from);
GXDLCODE_API unsigned gxstrlen(const gxwchar_t *s);
GXDLCODE_API gxwchar_t *gxstrcat(gxwchar_t *des, const gxwchar_t *src);
GXDLCODE_API int gxstrcmp(const gxwchar_t *a, const gxwchar_t *b);

// Standalone byte comparison functions used to work with memory buffers.
GXDLCODE_API int gxbyte_compare(const void *a, unsigned a_bytes, 
				const void *b, unsigned b_bytes);
GXDLCODE_API int gxbyte_icompare(const void *a, unsigned a_bytes, 
				 const void *b, unsigned b_bytes,
				 gxStringType st);

// Standalone pattern matching functions used to work with memory buffers.
GXDLCODE_API int gxfind_pattern(const void *src, unsigned s_bytes, 
				const void *p, unsigned p_bytes, 
				unsigned offset, gxStringType st, unsigned cs);
GXDLCODE_API int gxifind_pattern(const void *src, unsigned s_bytes, 
				 const void *p, unsigned p_bytes, 
				 unsigned offset, gxStringType st,
				 unsigned cs);

// Conversion functions used with string literals of different types
GXDLCODE_API char *Unicode2ASCII(const wchar_t *s, unsigned nchars = 0, 
				 char *nsptr = 0);
GXDLCODE_API wchar_t *ASCII2Unicode(const char *s, unsigned nchars = 0,
				    wchar_t *nsptr = 0);
GXDLCODE_API wchar_t *gxwchar_t2Unicode(const gxwchar_t *s, 
					unsigned nchars = 0, 
					wchar_t *nsptr = 0);
GXDLCODE_API char *gxwchar_t2ASCII(const gxwchar_t *s, unsigned nchars = 0,
				   char *nsptr = 0);
GXDLCODE_API gxwchar_t *ASCII2gxwchar_t(const char *s, unsigned nchars = 0,
					gxwchar_t *nsptr = 0);
GXDLCODE_API gxwchar_t *Unicode2gxwchar_t(const wchar_t *s, 
					  unsigned nchars = 0, 
					  gxwchar_t *nsptr = 0);

// Inline conversion functions used with string literals of different types
inline char *gxLTCHAR(const wchar_t *s, char *nsptr) {
  return Unicode2ASCII(s, 0, nsptr);
}

inline char gxLTCHAR(const wchar_t c, char *nsptr) {
  return (char)c;
}

inline wchar_t *gxLTCHAR(const char *s, wchar_t *nsptr) {
  return ASCII2Unicode(s, 0, nsptr);
}

inline wchar_t gxLTCHAR(const char c, wchar_t *nsptr) {
  return (wchar_t)c;
}

inline wchar_t *gxLTCHAR(const gxwchar_t *s, wchar_t *nsptr) {
  return gxwchar_t2Unicode(s, 0, nsptr);
}

inline wchar_t gxLTCHAR(const gxwchar_t c, wchar_t *nsptr) {
  return wchar_t((long)c);
}

inline char *gxLTCHAR(const gxwchar_t *s, char *nsptr) {
  return gxwchar_t2ASCII(s, 0, nsptr);
}

inline char gxLTCHAR(const gxwchar_t c, char *nsptr) {
  return char((long)c);
}

inline gxwchar_t *gxLTCHAR(const char *s, gxwchar_t *nsptr) {
  return ASCII2gxwchar_t(s, 0, nsptr);
}

inline gxwchar_t gxLTCHAR(const char c, gxwchar_t *nsptr) {
  return gxwchar((long)c);
}

inline gxwchar_t *gxLTCHAR(const wchar_t *s, gxwchar_t *nsptr) {
  return Unicode2gxwchar_t(s, 0, nsptr);
}

inline gxwchar_t gxLTCHAR(const wchar_t c, gxwchar_t *nsptr) {
  return gxwchar((long)c);
}

// Inline conversion functions used with string literals of the same type
inline char *gxLTCHAR(const char *s, char *nsptr) {
  return gxstrcpy(nsptr, s);
}

inline char gxLTCHAR(const char c, char *nsptr) {
  return (char)c;
}

inline wchar_t *gxLTCHAR(const wchar_t *s, wchar_t *nsptr) {
  return gxstrcpy(nsptr, s);
}

inline wchar_t gxLTCHAR(const wchar_t c, wchar_t *nsptr) {
  return (wchar_t)c;
}

inline gxwchar_t *gxLTCHAR(const gxwchar_t *s, gxwchar_t *nsptr) {
  return gxstrcpy(nsptr, s);
}

inline gxwchar_t gxLTCHAR(const gxwchar_t c, gxwchar_t *nsptr) {
  return (gxwchar_t)c;
}

// String concatenation routines
GXDLCODE_API char *StringCat(const char *s1 = 0, const char *s2 = 0, 
			     const char *s3 = 0);
GXDLCODE_API wchar_t *StringCat(const wchar_t *s1 = 0, const wchar_t *s2 = 0, 
				const wchar_t *s3 = 0);

// General-purpose string parser 
const int MAXWORDLENGTH = 255;
const int MAXWORDS = 255;
GXDLCODE_API int parse(const char *str, char words[MAXWORDS][MAXWORDLENGTH],
		       int *numwords, const char sepchar);
GXDLCODE_API int parse(const wchar_t *str, 
		       wchar_t words[MAXWORDS][MAXWORDLENGTH],
		       int *numwords, const wchar_t sepchar);

// General-purpose functions used to test strings
GXDLCODE_API int test_for_null(const char *s, unsigned nchars);
GXDLCODE_API int test_for_null(const gxwchar_t *s, unsigned nchars);

// Case insensitive string compare that need to be ported from UNIX to DOS  
GXDLCODE_API int CaseICmp(const char *a, const char *b);
GXDLCODE_API int CaseICmp(const wchar_t *a, const wchar_t *b);

// String compare that compare strings with alphanumeric content
GXDLCODE_API int StringANCompare(const char *a, const char *b, 
				 int check_case = 1);
// String searching functions
GXDLCODE_API int FindMatch(const char *src, const char *p, 
			   unsigned offset = 0, unsigned src_len = 0,
			   unsigned p_len = 0);
GXDLCODE_API int FindMatch(const wchar_t *src, const wchar_t *p, 
			   unsigned offset = 0, unsigned src_len = 0,
			   unsigned p_len = 0);
GXDLCODE_API int IFindMatch(const char *src, const char *p, 
			    unsigned offset = 0, unsigned src_len = 0,
			    unsigned p_len = 0);
GXDLCODE_API int IFindMatch(const wchar_t *src, const wchar_t *p, 
			    unsigned offset = 0, unsigned src_len = 0,
			    unsigned p_len = 0);
GXDLCODE_API unsigned long StringToULWORD(const char *str);
GXDLCODE_API unsigned long StringToULWORD(const wchar_t *str);

#if defined (__64_BIT_DATABASE_ENGINE__) || defined (__ENABLE_64_BIT_INTEGERS__)
GXDLCODE_API char *ULLWORDToString(char *sbuf, __ULLWORD__ val, 
				   int radix = 10);
GXDLCODE_API wchar_t *ULLWORDToString(wchar_t *sbuf, __ULLWORD__ val, 
				      int radix = 10);
GXDLCODE_API char *LLWORDToString(char *sbuf, __ULLWORD__ val, 
				  int radix = 10);
GXDLCODE_API wchar_t *LLWORDToString(wchar_t *sbuf, __ULLWORD__ val, 
				     int radix = 10);
GXDLCODE_API __ULLWORD__ StringToULLWORD(const char *str);
GXDLCODE_API __ULLWORD__ StringToULLWORD(const wchar_t *str);
GXDLCODE_API __LLWORD__ StringToLLWORD(const char *str);
GXDLCODE_API __LLWORD__ StringToLLWORD(const wchar_t *str);
#endif

#endif  // __GX_STRUTIL_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
