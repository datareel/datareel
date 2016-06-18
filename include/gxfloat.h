// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxfloat.h
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/12/1997  
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

The gxgxFLOAT64 class is used to represent 64 bit signed double
precision floating point values independently of the operating
system or hardware platform used. It works by separating a 64-bit
value into eight separate byte values and reordering the bytes
highest-order to lowest-order. A gxFLOAT64 type has a positive
limit of 1.7E+308 and a negative limit of 1.7E-308 with 15-digit
precision. NOTE: In order to maintain the correct byte ordering
on PC based, Intel x86 family, systems the __X86__ macro must be
defined at compile time.

In a double precision floating point number, the binary info is
stored within a total of 64 bits. The first bit contains the sign
of the mantissa (0 for positive and 1 for negative). The next 11
bits store the exponent, and the remaining 52 bits provide the
mantissa, giving an approximate decimal precision of 15 digits.
*/
// ----------------------------------------------------------- //   
#ifndef __GX_FLOAT64_HPP__
#define __GX_FLOAT64_HPP__

#include "gxdlcode.h"

#include "gxdtypes.h"

#if defined (__USE_NATIVE_FLOAT_TYPES__)
typedef double gxFLOAT64;
#else
// Data structure for signed double precision floating point values.
class GXDLCODE_API gxFLOAT64
{
public:
  gxFLOAT64(__DPFLOAT__ val = 0);
  gxFLOAT64(const gxFLOAT64& ob);
  gxFLOAT64& operator=(const gxFLOAT64& ob);
  gxFLOAT64& operator=(const __DPFLOAT__ ob);

public:
  void UnPackBits(__DPFLOAT__ val);
  __DPFLOAT__ PackBits() const;
  
public:
  operator __DPFLOAT__() const;

public: // Arithmetic operators that modify their operand
  gxFLOAT64 operator++(int);  // Postfix
  gxFLOAT64 operator--(int);  // Postfix
  gxFLOAT64 &operator++() { operator=(*this + 1); return *this; } // Prefix
  gxFLOAT64 &operator--() { operator=(*this - 1); return *this; } // Prefix
  void operator+=(const gxFLOAT64 &fp) { operator=(*this + fp); }
  void operator-=(const gxFLOAT64 &fp) { operator=(*this - fp); }
  void operator*=(const gxFLOAT64 &fp) { operator=(*this * fp); }
  void operator/=(const gxFLOAT64 &fp);

  void operator+=(const __DPFLOAT__ &fp) { operator=(*this + fp); }
  void operator-=(const __DPFLOAT__ &fp) { operator=(*this - fp); }
  void operator*=(const __DPFLOAT__ &fp) { operator=(*this * fp); }
  void operator/=(const __DPFLOAT__ &fp);
  
  void operator+=(const __LWORD__ &i) { operator=(*this + i); }
  void operator-=(const __LWORD__ &i) { operator=(*this - i); }
  void operator*=(const __LWORD__ &i) { operator=(*this * i); }
  void operator/=(const __LWORD__ &i);

  void operator+=(const __ULWORD__ &i) { operator=(*this + i); }
  void operator-=(const __ULWORD__ &i) { operator=(*this - i); }
  void operator*=(const __ULWORD__ &i) { operator=(*this * i); }
  void operator/=(const __ULWORD__ &i);

  void operator+=(const __WORD__ &i) { operator=(*this + i); }
  void operator-=(const __WORD__ &i) { operator=(*this - i); }
  void operator*=(const __WORD__ &i) { operator=(*this * i); }
  void operator/=(const __WORD__ &i);

  void operator+=(const __SWORD__ &i) { operator=(*this + i); }
  void operator-=(const __SWORD__ &i) { operator=(*this - i); }
  void operator*=(const __SWORD__ &i) { operator=(*this * i); }
  void operator/=(const __SWORD__ &i);

  void operator+=(const __UWORD__ &i) { operator=(*this + i); }
  void operator-=(const __UWORD__ &i) { operator=(*this - i); }
  void operator*=(const __UWORD__ &i) { operator=(*this * i); }
  void operator/=(const __UWORD__ &i);

  void operator+=(const __USWORD__ &i) { operator=(*this + i); }
  void operator-=(const __USWORD__ &i) { operator=(*this - i); }
  void operator*=(const __USWORD__ &i) { operator=(*this * i); }
  void operator/=(const __USWORD__ &i);

  void operator+=(const __SBYTE__ &i) { operator=(*this + (__DPFLOAT__)i); }
  void operator-=(const __SBYTE__ &i) { operator=(*this - (__DPFLOAT__)i); }
  void operator*=(const __SBYTE__ &i) { operator=(*this * (__DPFLOAT__)i); }
  void operator/=(const __SBYTE__ &i);

  void operator+=(const __UBYTE__ &i) { operator=(*this + (__DPFLOAT__)i); }
  void operator-=(const __UBYTE__ &i) { operator=(*this - (__DPFLOAT__)i); }
  void operator*=(const __UBYTE__ &i) { operator=(*this * (__DPFLOAT__)i); }
  void operator/=(const __UBYTE__ &i);

public: // Comparison operators
  GXDLCODE_API friend int operator==(const gxFLOAT64 &a, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator==(const gxFLOAT64 &a, const __DPFLOAT__ &bf);
  GXDLCODE_API friend int operator==(const __DPFLOAT__ &af, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator==(const gxFLOAT64 &a, const __LWORD__ &bl);
  GXDLCODE_API friend int operator==(const __LWORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator==(const gxFLOAT64 &a, const __ULWORD__ &bl);
  GXDLCODE_API friend int operator==(const __ULWORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator==(const gxFLOAT64 &a, const __WORD__ &bl);
  GXDLCODE_API friend int operator==(const __WORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator==(const gxFLOAT64 &a, const __SWORD__ &bl);
  GXDLCODE_API friend int operator==(const __SWORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator==(const gxFLOAT64 &a, const __UWORD__ &bl);
  GXDLCODE_API friend int operator==(const __UWORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator==(const gxFLOAT64 &a, const __USWORD__ &bl);
  GXDLCODE_API friend int operator==(const __USWORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator==(const gxFLOAT64 &a, const __SBYTE__ &bl);
  GXDLCODE_API friend int operator==(const __SBYTE__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator==(const gxFLOAT64 &a, const __UBYTE__ &bl);
  GXDLCODE_API friend int operator==(const __UBYTE__ &al, const gxFLOAT64 &b);

  GXDLCODE_API friend int operator!=(const gxFLOAT64 &a, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator!=(const gxFLOAT64 &a, 
				     const __DPFLOAT__ &bf);
  GXDLCODE_API friend int operator!=(const __DPFLOAT__ &af, 
				     const gxFLOAT64 &b);
  GXDLCODE_API friend int operator!=(const gxFLOAT64 &a, const __LWORD__ &bl);
  GXDLCODE_API friend int operator!=(const __LWORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator!=(const gxFLOAT64 &a, const __ULWORD__ &bl);
  GXDLCODE_API friend int operator!=(const __ULWORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator!=(const gxFLOAT64 &a, const __WORD__ &bl);
  GXDLCODE_API friend int operator!=(const __WORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator!=(const gxFLOAT64 &a, const __SWORD__ &bl);
  GXDLCODE_API friend int operator!=(const __SWORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator!=(const gxFLOAT64 &a, const __UWORD__ &bl);
  GXDLCODE_API friend int operator!=(const __UWORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator!=(const gxFLOAT64 &a, const __USWORD__ &bl);
  GXDLCODE_API friend int operator!=(const __USWORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator!=(const gxFLOAT64 &a, const __SBYTE__ &bl);
  GXDLCODE_API friend int operator!=(const __SBYTE__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator!=(const gxFLOAT64 &a, const __UBYTE__ &bl);
  GXDLCODE_API friend int operator!=(const __UBYTE__ &al, const gxFLOAT64 &b);

  GXDLCODE_API friend int operator<(const gxFLOAT64 &a, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator<(const gxFLOAT64 &a, const __DPFLOAT__ &bf);
  GXDLCODE_API friend int operator<(const __DPFLOAT__ &af, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator<(const gxFLOAT64 &a, const __LWORD__ &bl);
  GXDLCODE_API friend int operator<(const __LWORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator<(const gxFLOAT64 &a, const __ULWORD__ &bl);
  GXDLCODE_API friend int operator<(const __ULWORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator<(const gxFLOAT64 &a, const __WORD__ &bl);
  GXDLCODE_API friend int operator<(const __WORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator<(const gxFLOAT64 &a, const __SWORD__ &bl);
  GXDLCODE_API friend int operator<(const __SWORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator<(const gxFLOAT64 &a, const __UWORD__ &bl);
  GXDLCODE_API friend int operator<(const __UWORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator<(const gxFLOAT64 &a, const __USWORD__ &bl);
  GXDLCODE_API friend int operator<(const __USWORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator<(const gxFLOAT64 &a, const __SBYTE__ &bl);
  GXDLCODE_API friend int operator<(const __SBYTE__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator<(const gxFLOAT64 &a, const __UBYTE__ &bl);
  GXDLCODE_API friend int operator<(const __UBYTE__ &al, const gxFLOAT64 &b);

  GXDLCODE_API friend int operator>(const gxFLOAT64 &a, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator>(const gxFLOAT64 &a, const __DPFLOAT__ &bf);
  GXDLCODE_API friend int operator>(const __DPFLOAT__ &af, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator>(const gxFLOAT64 &a, const __LWORD__ &bl);
  GXDLCODE_API friend int operator>(const __LWORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator>(const gxFLOAT64 &a, const __ULWORD__ &bl);
  GXDLCODE_API friend int operator>(const __ULWORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator>(const gxFLOAT64 &a, const __WORD__ &bl);
  GXDLCODE_API friend int operator>(const __WORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator>(const gxFLOAT64 &a, const __SWORD__ &bl);
  GXDLCODE_API friend int operator>(const __SWORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator>(const gxFLOAT64 &a, const __UWORD__ &bl);
  GXDLCODE_API friend int operator>(const __UWORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator>(const gxFLOAT64 &a, const __USWORD__ &bl);
  GXDLCODE_API friend int operator>(const __USWORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator>(const gxFLOAT64 &a, const __SBYTE__ &bl);
  GXDLCODE_API friend int operator>(const __SBYTE__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator>(const gxFLOAT64 &a, const __UBYTE__ &bl);
  GXDLCODE_API friend int operator>(const __UBYTE__ &al, const gxFLOAT64 &b);

  GXDLCODE_API friend int operator<=(const gxFLOAT64 &a, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator<=(const gxFLOAT64 &a, const __DPFLOAT__ &bf);
  GXDLCODE_API friend int operator<=(const __DPFLOAT__ &af, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator<=(const gxFLOAT64 &a, const __LWORD__ &bl);
  GXDLCODE_API friend int operator<=(const __LWORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator<=(const gxFLOAT64 &a, const __ULWORD__ &bl);
  GXDLCODE_API friend int operator<=(const __ULWORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator<=(const gxFLOAT64 &a, const __WORD__ &bl);
  GXDLCODE_API friend int operator<=(const __WORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator<=(const gxFLOAT64 &a, const __SWORD__ &bl);
  GXDLCODE_API friend int operator<=(const __SWORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator<=(const gxFLOAT64 &a, const __UWORD__ &bl);
  GXDLCODE_API friend int operator<=(const __UWORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator<=(const gxFLOAT64 &a, const __USWORD__ &bl);
  GXDLCODE_API friend int operator<=(const __USWORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator<=(const gxFLOAT64 &a, const __SBYTE__ &bl);
  GXDLCODE_API friend int operator<=(const __SBYTE__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator<=(const gxFLOAT64 &a, const __UBYTE__ &bl);
  GXDLCODE_API friend int operator<=(const __UBYTE__ &al, const gxFLOAT64 &b);

  GXDLCODE_API friend int operator>=(const gxFLOAT64 &a, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator>=(const gxFLOAT64 &a, 
				     const __DPFLOAT__ &bf);
  GXDLCODE_API friend int operator>=(const __DPFLOAT__ &af, 
				     const gxFLOAT64 &b);
  GXDLCODE_API friend int operator>=(const gxFLOAT64 &a, const __LWORD__ &bl);
  GXDLCODE_API friend int operator>=(const __LWORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator>=(const gxFLOAT64 &a, const __ULWORD__ &bl);
  GXDLCODE_API friend int operator>=(const __ULWORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator>=(const gxFLOAT64 &a, const __WORD__ &bl);
  GXDLCODE_API friend int operator>=(const __WORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator>=(const gxFLOAT64 &a, const __SWORD__ &bl);
  GXDLCODE_API friend int operator>=(const __SWORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator>=(const gxFLOAT64 &a, const __UWORD__ &bl);
  GXDLCODE_API friend int operator>=(const __UWORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator>=(const gxFLOAT64 &a, const __USWORD__ &bl);
  GXDLCODE_API friend int operator>=(const __USWORD__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator>=(const gxFLOAT64 &a, const __SBYTE__ &bl);
  GXDLCODE_API friend int operator>=(const __SBYTE__ &al, const gxFLOAT64 &b);
  GXDLCODE_API friend int operator>=(const gxFLOAT64 &a, const __UBYTE__ &bl);
  GXDLCODE_API friend int operator>=(const __UBYTE__ &al, const gxFLOAT64 &b);
  
private:
  __SBYTE__ byte[8];
};
#endif // __USE_NATIVE_FLOAT_TYPES__

#endif // __GX_FLOAT64_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
