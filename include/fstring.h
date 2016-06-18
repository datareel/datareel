// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Header File Name: fstring.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/21/1997
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

Fixed length string class used by database key types or members 
that must be fixed in length.

Changes:
==============================================================
07/24/2001: Changed include path for the strutil.h include file
from <strutil.h> to "strutil.h"

03/11/2002: By default C++ iostreams are no longer used. In order 
to use the iostream functions built into this class users must 
define __USE_CPP_IOSTREAM__ preprocessor directive. When using C++ 
iostreams users have the option to use the ANSI Standard C++ library 
by defining the __USE_ANSI_CPP__ preprocessor directive. By default 
the old iostream library will be used when the  __USE_CPP_IOSTREAM__ 
directive is defined.

05/05/2002: Modified the FString default constructor to clear all 
the bytes in a string when a FString object is constructed.

10/13/2002: Removed the following conversion function from the FString
class due to excessive compiler warning under Linux: 
operator const int () const { return sptr[0] != 0; }

10/13/2002: Removed the following conversion function to avoid having
FString objects being automatically converted to int types: 
operator int () { return ((sptr != 0) && (s_length != 0)); }

10/13/2002: Added the FString::is_not_null() function to replace the
int conversion function. NOTE: The FString class no longer has any
conversion functions.
==============================================================
*/
// ----------------------------------------------------------- // 
#ifndef __GX_FIXED_STRING_CLASS_HPP__
#define __GX_FIXED_STRING_CLASS_HPP__

#include "gxdlcode.h"

#if defined (__USE_CPP_IOSTREAM__)
#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
#include <iomanip>
#else // Use the old iostream library by default
#include <iostream.h>
#include <iomanip.h>
#endif // __USE_ANSI_CPP__
#endif // __USE_CPP_IOSTREAM__

#include <string.h>
#include "strutil.h"

// This constant sets the string length for all FString objects.
// The total length for all FString objects is equal to the length
// specified in this constant plus one byte for a null terminator.
const unsigned FStringLength = 255; // Length not including null terminator

// Fixed length string class
class GXDLCODE_API FString 
{ 
public:
  FString() { Clear(); }
  FString(const char *s, unsigned bytes = 0) {
    SetString(s, bytes);
  }
  ~FString() { }    

  FString(const FString &s) { Copy(s); }
  FString &operator=(const char *s) { SetString(s); return *this; }
  FString &operator=(const FString &s) {
    if(this != &s) Copy(s); // Prevent self assignment 
    return *this;
  }

public: // Append, Insert, delete, and remove functions
  int SetString(const char *s, unsigned bytes = 0);
  void Clear();
  void Copy(const FString &s);

public: // C String, pointer, and length functions
  unsigned length() { return strlen(sptr); }
  unsigned length() const { return strlen(sptr); }
  char *c_str() { return (char *)sptr; }
  const char *c_str() const { return (const char *)sptr; }
  int is_null() { return sptr[0] == 0; } 
  int is_null() const { return sptr[0] == 0; }
  int is_not_null() { return sptr[0] != 0; }
  int is_not_null() const { return sptr[0] != 0; }
  FString *strdup();
  FString *strdup() const;

public: // Database functions
  size_t SizeOf() { return sizeof(sptr); }

public: // Overloaded operators
  int operator!() { return sptr[0] == 0; } 
  int operator!() const { return sptr[0] == 0; }
  GXDLCODE_API friend int operator==(const FString &a, const FString &b);
  GXDLCODE_API friend int operator<(const FString &a, const FString &b);
  GXDLCODE_API friend int operator>(const FString &a, const FString &b);
  GXDLCODE_API friend int operator!=(const FString &a, const FString &b);

#if defined (__USE_CPP_IOSTREAM__)
public: // Class type to ostream type conversions
  GXDLCODE_API friend GXSTD::ostream &operator<<(GXSTD::ostream &os, 
						 const FString &s) {
    return os.write(s.sptr, strlen(s.sptr));
  }
  GXDLCODE_API friend GXSTD::istream &operator>>(GXSTD::istream &os, 
						 FString &s) {
    os >> GXSTD::setw(FStringLength+1) >> s.sptr;
    return os;
  }
#endif // __USE_CPP_IOSTREAM__
       
public: // The FString structure can have only one data member
  char sptr[FStringLength+1]; // Fixed string length plus null terminator
};

// General-purpose string routines that need to be ported from UNIX to DOS
GXDLCODE_API int CaseICmp(const FString &s1, const FString &s2);
GXDLCODE_API int CaseICmp(FString &s1, FString &s2);
GXDLCODE_API int CaseICmp(const FString &s1, const char *s);
GXDLCODE_API int CaseICmp(const char *s, const FString &s2);

// Global variable used to toggle FString case comparisons
extern GXDLCODE_API int FStringCaseCompare;

#endif // __GX_FIXED_STRING_CLASS_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
