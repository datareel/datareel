// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxuint64.h
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/05/1997
// Date Last Modified: 06/17/2016
// Copyright (c) 2001-2024 DataReel Software Development
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

The gxUINT64 class is used to represent 64 bit unsigned integers
independently of the operating system or hardware platform used.
It works by separating a 64-bit value into eight separate byte
values and reordering the bytes lowest-order to highest-order.
A gxUINT64 type has a base 10 limit of 18446744073709551615.

Changes:
==============================================================
03/11/2002: By default C++ iostreams are no longer used. In order 
to use the iostream functions built into this class users must 
define __USE_CPP_IOSTREAM__ preprocessor directive. When using C++ 
iostreams users have the option to use the ANSI Standard C++ library 
by defining the __USE_ANSI_CPP__ preprocessor directive. By default 
the old iostream library will be used when the  __USE_CPP_IOSTREAM__ 
directive is defined.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_UINT64_HPP__
#define __GX_UINT64_HPP__

#include "gxdlcode.h"

#if defined (__64_BIT_DATABASE_ENGINE__) || defined (__ENABLE_64_BIT_INTEGERS__)

#include "gxdtypes.h"
#include "gxdtyp64.h"

#if defined (__USE_NATIVE_INT_TYPES__)
typedef __ULLWORD__ gxUINT64;
#else

#if defined (__USE_CPP_IOSTREAM__)
#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
#include <iomanip>
#else // Use the old iostream library by default
#include <iostream.h>
#include <iomanip.h>
#endif // __USE_ANSI_CPP__
#endif // __USE_CPP_IOSTREAM__

// Data structure for platform independent signed 32 bit integer values.
class GXDLCODE_API gxUINT64
{
public:
  gxUINT64(__ULLWORD__ val = 0);
  gxUINT64(const gxUINT64& ob);
  gxUINT64& operator=(const gxUINT64& ob);
  gxUINT64& operator=(const __ULLWORD__ ob);

public:
  void UnPackBits(__ULLWORD__ val);
  __ULLWORD__ PackBits() const;

public: // Conversion functions
  operator __ULLWORD__() const;

public: // String functions added to print 64-bit types
  char *c_str(char *sbuf, int radix = 10);
  char *c_str(char *sbuf, int radix = 10) const;
  gxUINT64 &StringToUINT64(const char *str);
  gxUINT64 &StringToUINT64(char *str);

#if defined (__USE_CPP_IOSTREAM__)
public: // Class type to ostream type conversions
  GXDLCODE_API friend GXSTD::ostream &operator<<(GXSTD::ostream &os, 
						 const gxUINT64 &val);
  GXDLCODE_API friend GXSTD::istream &operator>>(GXSTD::istream &os, 
						 gxUINT64 &val);
#endif // __USE_CPP_IOSTREAM__

public: // Arithmetic operators that modify their operand
  gxUINT64 operator++(int);  // Postfix
  gxUINT64 operator--(int);  // Postfix
  gxUINT64 &operator++() { operator=(*this + 1); return *this; } // Prefix
  gxUINT64 &operator--() { operator=(*this - 1); return *this; } // Prefix
  void operator+=(const gxUINT64 &i) { operator=(*this + i); }
  void operator-=(const gxUINT64 &i) { operator=(*this - i); }
  void operator*=(const gxUINT64 &i) { operator=(*this * i); }
  void operator/=(const gxUINT64 &i);

  void operator+=(const __LLWORD__ &i) { operator=(*this + i); }
  void operator-=(const __LLWORD__ &i) { operator=(*this - i); }
  void operator*=(const __LLWORD__ &i) { operator=(*this * i); }
  void operator/=(const __LLWORD__ &i);

  void operator+=(const __ULLWORD__ &i) { operator=(*this + (__LLWORD__)i); }
  void operator-=(const __ULLWORD__ &i) { operator=(*this - (__LLWORD__)i); }
  void operator*=(const __ULLWORD__ &i) { operator=(*this * (__LLWORD__)i); }
  void operator/=(const __ULLWORD__ &i);

  void operator+=(const __LWORD__ &i) { operator=(*this + i); }
  void operator-=(const __LWORD__ &i) { operator=(*this - i); }
  void operator*=(const __LWORD__ &i) { operator=(*this * i); }
  void operator/=(const __LWORD__ &i);

  void operator+=(const __ULWORD__ &i) { operator=(*this + (__LLWORD__)i); }
  void operator-=(const __ULWORD__ &i) { operator=(*this - (__LLWORD__)i); }
  void operator*=(const __ULWORD__ &i) { operator=(*this * (__LLWORD__)i); }
  void operator/=(const __ULWORD__ &i);

  void operator+=(const __WORD__ &i) { operator=(*this + (__LLWORD__)i); }
  void operator-=(const __WORD__ &i) { operator=(*this - (__LLWORD__)i); }
  void operator*=(const __WORD__ &i) { operator=(*this * (__LLWORD__)i); }
  void operator/=(const __WORD__ &i);

  void operator+=(const __SWORD__ &i) { operator=(*this + (__LLWORD__)i); }
  void operator-=(const __SWORD__ &i) { operator=(*this - (__LLWORD__)i); }
  void operator*=(const __SWORD__ &i) { operator=(*this * (__LLWORD__)i); }
  void operator/=(const __SWORD__ &i);

  void operator+=(const __UWORD__ &i) { operator=(*this + (__LLWORD__)i); }
  void operator-=(const __UWORD__ &i) { operator=(*this - (__LLWORD__)i); }
  void operator*=(const __UWORD__ &i) { operator=(*this * (__LLWORD__)i); }
  void operator/=(const __UWORD__ &i);

  void operator+=(const __USWORD__ &i) { operator=(*this + (__LLWORD__)i); }
  void operator-=(const __USWORD__ &i) { operator=(*this - (__LLWORD__)i); }
  void operator*=(const __USWORD__ &i) { operator=(*this * (__LLWORD__)i); }
  void operator/=(const __USWORD__ &i);

  void operator+=(const __SBYTE__ &i) { operator=(*this + (__LLWORD__)i); }
  void operator-=(const __SBYTE__ &i) { operator=(*this - (__LLWORD__)i); }
  void operator*=(const __SBYTE__ &i) { operator=(*this * (__LLWORD__)i); }
  void operator/=(const __SBYTE__ &i);

  void operator+=(const __UBYTE__ &i) { operator=(*this + (__LLWORD__)i); }
  void operator-=(const __UBYTE__ &i) { operator=(*this - (__LLWORD__)i); }
  void operator*=(const __UBYTE__ &i) { operator=(*this * (__LLWORD__)i); }
  void operator/=(const __UBYTE__ &i);

public: // Comparison operators
  GXDLCODE_API friend int operator==(const gxUINT64 &a, const gxUINT64 &b);
  GXDLCODE_API friend int operator==(const gxUINT64 &a, const __LLWORD__ &bl);
  GXDLCODE_API friend int operator==(const __LLWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator==(const gxUINT64 &a, const __ULLWORD__ &bl);
  GXDLCODE_API friend int operator==(const __ULLWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator==(const gxUINT64 &a, const __LWORD__ &bl);
  GXDLCODE_API friend int operator==(const __LWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator==(const gxUINT64 &a, const __ULWORD__ &bl);
  GXDLCODE_API friend int operator==(const __ULWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator==(const gxUINT64 &a, const __WORD__ &bl);
  GXDLCODE_API friend int operator==(const __WORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator==(const gxUINT64 &a, const __SWORD__ &bl);
  GXDLCODE_API friend int operator==(const __SWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator==(const gxUINT64 &a, const __UWORD__ &bl);
  GXDLCODE_API friend int operator==(const __UWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator==(const gxUINT64 &a, const __USWORD__ &bl);
  GXDLCODE_API friend int operator==(const __USWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator==(const gxUINT64 &a, const __SBYTE__ &bl);
  GXDLCODE_API friend int operator==(const __SBYTE__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator==(const gxUINT64 &a, const __UBYTE__ &bl);
  GXDLCODE_API friend int operator==(const __UBYTE__ &al, const gxUINT64 &b);

  GXDLCODE_API friend int operator!=(const gxUINT64 &a, const __LLWORD__ &bl);
  GXDLCODE_API friend int operator!=(const __LLWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator!=(const gxUINT64 &a, const __ULLWORD__ &bl);
  GXDLCODE_API friend int operator!=(const __ULLWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator!=(const gxUINT64 &a, const gxUINT64 &b);
  GXDLCODE_API friend int operator!=(const gxUINT64 &a, const __LWORD__ &bl);
  GXDLCODE_API friend int operator!=(const __LWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator!=(const gxUINT64 &a, const __ULWORD__ &bl);
  GXDLCODE_API friend int operator!=(const __ULWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator!=(const gxUINT64 &a, const __WORD__ &bl);
  GXDLCODE_API friend int operator!=(const __WORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator!=(const gxUINT64 &a, const __SWORD__ &bl);
  GXDLCODE_API friend int operator!=(const __SWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator!=(const gxUINT64 &a, const __UWORD__ &bl);
  GXDLCODE_API friend int operator!=(const __UWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator!=(const gxUINT64 &a, const __USWORD__ &bl);
  GXDLCODE_API friend int operator!=(const __USWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator!=(const gxUINT64 &a, const __SBYTE__ &bl);
  GXDLCODE_API friend int operator!=(const __SBYTE__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator!=(const gxUINT64 &a, const __UBYTE__ &bl);
  GXDLCODE_API friend int operator!=(const __UBYTE__ &al, const gxUINT64 &b);

  GXDLCODE_API friend int operator<(const gxUINT64 &a, const __LLWORD__ &bl);
  GXDLCODE_API friend int operator<(const __LLWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator<(const gxUINT64 &a, const __ULLWORD__ &bl);
  GXDLCODE_API friend int operator<(const __ULLWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator<(const gxUINT64 &a, const gxUINT64 &b);
  GXDLCODE_API friend int operator<(const gxUINT64 &a, const __LWORD__ &bl);
  GXDLCODE_API friend int operator<(const __LWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator<(const gxUINT64 &a, const __ULWORD__ &bl);
  GXDLCODE_API friend int operator<(const __ULWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator<(const gxUINT64 &a, const __WORD__ &bl);
  GXDLCODE_API friend int operator<(const __WORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator<(const gxUINT64 &a, const __SWORD__ &bl);
  GXDLCODE_API friend int operator<(const __SWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator<(const gxUINT64 &a, const __UWORD__ &bl);
  GXDLCODE_API friend int operator<(const __UWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator<(const gxUINT64 &a, const __USWORD__ &bl);
  GXDLCODE_API friend int operator<(const __USWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator<(const gxUINT64 &a, const __SBYTE__ &bl);
  GXDLCODE_API friend int operator<(const __SBYTE__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator<(const gxUINT64 &a, const __UBYTE__ &bl);
  GXDLCODE_API friend int operator<(const __UBYTE__ &al, const gxUINT64 &b);

  GXDLCODE_API friend int operator>(const gxUINT64 &a, const __LLWORD__ &bl);
  GXDLCODE_API friend int operator>(const __LLWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator>(const gxUINT64 &a, const __ULLWORD__ &bl);
  GXDLCODE_API friend int operator>(const __ULLWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator>(const gxUINT64 &a, const gxUINT64 &b);
  GXDLCODE_API friend int operator>(const gxUINT64 &a, const __LWORD__ &bl);
  GXDLCODE_API friend int operator>(const __LWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator>(const gxUINT64 &a, const __ULWORD__ &bl);
  GXDLCODE_API friend int operator>(const __ULWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator>(const gxUINT64 &a, const __WORD__ &bl);
  GXDLCODE_API friend int operator>(const __WORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator>(const gxUINT64 &a, const __SWORD__ &bl);
  GXDLCODE_API friend int operator>(const __SWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator>(const gxUINT64 &a, const __UWORD__ &bl);
  GXDLCODE_API friend int operator>(const __UWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator>(const gxUINT64 &a, const __USWORD__ &bl);
  GXDLCODE_API friend int operator>(const __USWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator>(const gxUINT64 &a, const __SBYTE__ &bl);
  GXDLCODE_API friend int operator>(const __SBYTE__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator>(const gxUINT64 &a, const __UBYTE__ &bl);
  GXDLCODE_API friend int operator>(const __UBYTE__ &al, const gxUINT64 &b);

  GXDLCODE_API friend int operator<=(const gxUINT64 &a, const __LLWORD__ &bl);
  GXDLCODE_API friend int operator<=(const __LLWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator<=(const gxUINT64 &a, const __ULLWORD__ &bl);
  GXDLCODE_API friend int operator<=(const __ULLWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator<=(const gxUINT64 &a, const gxUINT64 &b);
  GXDLCODE_API friend int operator<=(const gxUINT64 &a, const __LWORD__ &bl);
  GXDLCODE_API friend int operator<=(const __LWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator<=(const gxUINT64 &a, const __ULWORD__ &bl);
  GXDLCODE_API friend int operator<=(const __ULWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator<=(const gxUINT64 &a, const __WORD__ &bl);
  GXDLCODE_API friend int operator<=(const __WORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator<=(const gxUINT64 &a, const __SWORD__ &bl);
  GXDLCODE_API friend int operator<=(const __SWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator<=(const gxUINT64 &a, const __UWORD__ &bl);
  GXDLCODE_API friend int operator<=(const __UWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator<=(const gxUINT64 &a, const __USWORD__ &bl);
  GXDLCODE_API friend int operator<=(const __USWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator<=(const gxUINT64 &a, const __SBYTE__ &bl);
  GXDLCODE_API friend int operator<=(const __SBYTE__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator<=(const gxUINT64 &a, const __UBYTE__ &bl);
  GXDLCODE_API friend int operator<=(const __UBYTE__ &al, const gxUINT64 &b);

  GXDLCODE_API friend int operator>=(const gxUINT64 &a, const __LLWORD__ &bl);
  GXDLCODE_API friend int operator>=(const __LLWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator>=(const gxUINT64 &a, const __ULLWORD__ &bl);
  GXDLCODE_API friend int operator>=(const __ULLWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator>=(const gxUINT64 &a, const gxUINT64 &b);
  GXDLCODE_API friend int operator>=(const gxUINT64 &a, const __LWORD__ &bl);
  GXDLCODE_API friend int operator>=(const __LWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator>=(const gxUINT64 &a, const __ULWORD__ &bl);
  GXDLCODE_API friend int operator>=(const __ULWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator>=(const gxUINT64 &a, const __WORD__ &bl);
  GXDLCODE_API friend int operator>=(const __WORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator>=(const gxUINT64 &a, const __SWORD__ &bl);
  GXDLCODE_API friend int operator>=(const __SWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator>=(const gxUINT64 &a, const __UWORD__ &bl);
  GXDLCODE_API friend int operator>=(const __UWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator>=(const gxUINT64 &a, const __USWORD__ &bl);
  GXDLCODE_API friend int operator>=(const __USWORD__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator>=(const gxUINT64 &a, const __SBYTE__ &bl);
  GXDLCODE_API friend int operator>=(const __SBYTE__ &al, const gxUINT64 &b);
  GXDLCODE_API friend int operator>=(const gxUINT64 &a, const __UBYTE__ &bl);
  GXDLCODE_API friend int operator>=(const __UBYTE__ &al, const gxUINT64 &b);
  
private:
  __UBYTE__ byte[8];
};
#endif // __USE_NATIVE_INT_TYPES__

#endif // __64_BIT_DATABASE_ENGINE__ || __ENABLE_64_BIT_INTEGERS__

#endif // __GX_UINT64_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
