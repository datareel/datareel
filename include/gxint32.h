// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxint32.h
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

The gxINT32 class is used to represent 32 bit signed integers
independently of the operating system or hardware platform used.
It works by separating a 32-bit value into four separate byte
values and reordering the bytes lowest-order to highest-order.
An gxINT32 type has a base 10 positive limit of 2,147,483,647 and
a negative limit of 2,147,483,648.
*/
// ----------------------------------------------------------- //   
#ifndef __GX_INT32_HPP__
#define __GX_INT32_HPP__

#include "gxdlcode.h"

#include "gxdtypes.h"

#if defined (__USE_NATIVE_INT_TYPES__)
typedef __LWORD__ gxINT32;
#else

// Data structure for platform independent signed 32 bit integer values.
class GXDLCODE_API gxINT32
{
public:
  gxINT32(__LWORD__ val = 0);
  gxINT32(const gxINT32& ob);
  gxINT32& operator=(const gxINT32& ob);
  gxINT32& operator=(const __LWORD__ ob);

public:
  void UnPackBits(__LWORD__ val);
  __LWORD__ PackBits() const;

public:
  operator __LWORD__() const;
  
public: // Arithmetic operators that modify their operand
  gxINT32 operator++(int);  // Postfix
  gxINT32 operator--(int);  // Postfix
  gxINT32 &operator++() { operator=(*this + 1); return *this; } // Prefix
  gxINT32 &operator--() { operator=(*this - 1); return *this; } // Prefix
  void operator+=(const gxINT32 &i) { operator=(*this + i); }
  void operator-=(const gxINT32 &i) { operator=(*this - i); }
  void operator*=(const gxINT32 &i) { operator=(*this * i); }
  void operator/=(const gxINT32 &i);

  void operator+=(const __LWORD__ &i) { operator=(*this + i); }
  void operator-=(const __LWORD__ &i) { operator=(*this - i); }
  void operator*=(const __LWORD__ &i) { operator=(*this * i); }
  void operator/=(const __LWORD__ &i);

  void operator+=(const __ULWORD__ &i) { operator=(*this + (__LWORD__)i); }
  void operator-=(const __ULWORD__ &i) { operator=(*this - (__LWORD__)i); }
  void operator*=(const __ULWORD__ &i) { operator=(*this * (__LWORD__)i); }
  void operator/=(const __ULWORD__ &i);

  void operator+=(const __WORD__ &i) { operator=(*this + (__LWORD__)i); }
  void operator-=(const __WORD__ &i) { operator=(*this - (__LWORD__)i); }
  void operator*=(const __WORD__ &i) { operator=(*this * (__LWORD__)i); }
  void operator/=(const __WORD__ &i);

  void operator+=(const __SWORD__ &i) { operator=(*this + (__LWORD__)i); }
  void operator-=(const __SWORD__ &i) { operator=(*this - (__LWORD__)i); }
  void operator*=(const __SWORD__ &i) { operator=(*this * (__LWORD__)i); }
  void operator/=(const __SWORD__ &i);

  void operator+=(const __UWORD__ &i) { operator=(*this + (__LWORD__)i); }
  void operator-=(const __UWORD__ &i) { operator=(*this - (__LWORD__)i); }
  void operator*=(const __UWORD__ &i) { operator=(*this * (__LWORD__)i); }
  void operator/=(const __UWORD__ &i);

  void operator+=(const __USWORD__ &i) { operator=(*this + (__LWORD__)i); }
  void operator-=(const __USWORD__ &i) { operator=(*this - (__LWORD__)i); }
  void operator*=(const __USWORD__ &i) { operator=(*this * (__LWORD__)i); }
  void operator/=(const __USWORD__ &i);

  void operator+=(const __SBYTE__ &i) { operator=(*this + (__LWORD__)i); }
  void operator-=(const __SBYTE__ &i) { operator=(*this - (__LWORD__)i); }
  void operator*=(const __SBYTE__ &i) { operator=(*this * (__LWORD__)i); }
  void operator/=(const __SBYTE__ &i);

  void operator+=(const __UBYTE__ &i) { operator=(*this + (__LWORD__)i); }
  void operator-=(const __UBYTE__ &i) { operator=(*this - (__LWORD__)i); }
  void operator*=(const __UBYTE__ &i) { operator=(*this * (__LWORD__)i); }
  void operator/=(const __UBYTE__ &i);

public: // Comparison operators
  GXDLCODE_API friend int operator==(const gxINT32 &a, const gxINT32 &b);
  GXDLCODE_API friend int operator==(const gxINT32 &a, const __LWORD__ &bl);
  GXDLCODE_API friend int operator==(const __LWORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator==(const gxINT32 &a, const __ULWORD__ &bl);
  GXDLCODE_API friend int operator==(const __ULWORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator==(const gxINT32 &a, const __WORD__ &bl);
  GXDLCODE_API friend int operator==(const __WORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator==(const gxINT32 &a, const __SWORD__ &bl);
  GXDLCODE_API friend int operator==(const __SWORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator==(const gxINT32 &a, const __UWORD__ &bl);
  GXDLCODE_API friend int operator==(const __UWORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator==(const gxINT32 &a, const __USWORD__ &bl);
  GXDLCODE_API friend int operator==(const __USWORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator==(const gxINT32 &a, const __SBYTE__ &bl);
  GXDLCODE_API friend int operator==(const __SBYTE__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator==(const gxINT32 &a, const __UBYTE__ &bl);
  GXDLCODE_API friend int operator==(const __UBYTE__ &al, const gxINT32 &b);

  GXDLCODE_API friend int operator!=(const gxINT32 &a, const gxINT32 &b);
  GXDLCODE_API friend int operator!=(const gxINT32 &a, const __LWORD__ &bl);
  GXDLCODE_API friend int operator!=(const __LWORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator!=(const gxINT32 &a, const __ULWORD__ &bl);
  GXDLCODE_API friend int operator!=(const __ULWORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator!=(const gxINT32 &a, const __WORD__ &bl);
  GXDLCODE_API friend int operator!=(const __WORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator!=(const gxINT32 &a, const __SWORD__ &bl);
  GXDLCODE_API friend int operator!=(const __SWORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator!=(const gxINT32 &a, const __UWORD__ &bl);
  GXDLCODE_API friend int operator!=(const __UWORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator!=(const gxINT32 &a, const __USWORD__ &bl);
  GXDLCODE_API friend int operator!=(const __USWORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator!=(const gxINT32 &a, const __SBYTE__ &bl);
  GXDLCODE_API friend int operator!=(const __SBYTE__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator!=(const gxINT32 &a, const __UBYTE__ &bl);
  GXDLCODE_API friend int operator!=(const __UBYTE__ &al, const gxINT32 &b);

  GXDLCODE_API friend int operator<(const gxINT32 &a, const gxINT32 &b);
  GXDLCODE_API friend int operator<(const gxINT32 &a, const __LWORD__ &bl);
  GXDLCODE_API friend int operator<(const __LWORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator<(const gxINT32 &a, const __ULWORD__ &bl);
  GXDLCODE_API friend int operator<(const __ULWORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator<(const gxINT32 &a, const __WORD__ &bl);
  GXDLCODE_API friend int operator<(const __WORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator<(const gxINT32 &a, const __SWORD__ &bl);
  GXDLCODE_API friend int operator<(const __SWORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator<(const gxINT32 &a, const __UWORD__ &bl);
  GXDLCODE_API friend int operator<(const __UWORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator<(const gxINT32 &a, const __USWORD__ &bl);
  GXDLCODE_API friend int operator<(const __USWORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator<(const gxINT32 &a, const __SBYTE__ &bl);
  GXDLCODE_API friend int operator<(const __SBYTE__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator<(const gxINT32 &a, const __UBYTE__ &bl);
  GXDLCODE_API friend int operator<(const __UBYTE__ &al, const gxINT32 &b);

  GXDLCODE_API friend int operator>(const gxINT32 &a, const gxINT32 &b);
  GXDLCODE_API friend int operator>(const gxINT32 &a, const __LWORD__ &bl);
  GXDLCODE_API friend int operator>(const __LWORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator>(const gxINT32 &a, const __ULWORD__ &bl);
  GXDLCODE_API friend int operator>(const __ULWORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator>(const gxINT32 &a, const __WORD__ &bl);
  GXDLCODE_API friend int operator>(const __WORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator>(const gxINT32 &a, const __SWORD__ &bl);
  GXDLCODE_API friend int operator>(const __SWORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator>(const gxINT32 &a, const __UWORD__ &bl);
  GXDLCODE_API friend int operator>(const __UWORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator>(const gxINT32 &a, const __USWORD__ &bl);
  GXDLCODE_API friend int operator>(const __USWORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator>(const gxINT32 &a, const __SBYTE__ &bl);
  GXDLCODE_API friend int operator>(const __SBYTE__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator>(const gxINT32 &a, const __UBYTE__ &bl);
  GXDLCODE_API friend int operator>(const __UBYTE__ &al, const gxINT32 &b);

  GXDLCODE_API friend int operator<=(const gxINT32 &a, const gxINT32 &b);
  GXDLCODE_API friend int operator<=(const gxINT32 &a, const __LWORD__ &bl);
  GXDLCODE_API friend int operator<=(const __LWORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator<=(const gxINT32 &a, const __ULWORD__ &bl);
  GXDLCODE_API friend int operator<=(const __ULWORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator<=(const gxINT32 &a, const __WORD__ &bl);
  GXDLCODE_API friend int operator<=(const __WORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator<=(const gxINT32 &a, const __SWORD__ &bl);
  GXDLCODE_API friend int operator<=(const __SWORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator<=(const gxINT32 &a, const __UWORD__ &bl);
  GXDLCODE_API friend int operator<=(const __UWORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator<=(const gxINT32 &a, const __USWORD__ &bl);
  GXDLCODE_API friend int operator<=(const __USWORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator<=(const gxINT32 &a, const __SBYTE__ &bl);
  GXDLCODE_API friend int operator<=(const __SBYTE__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator<=(const gxINT32 &a, const __UBYTE__ &bl);
  GXDLCODE_API friend int operator<=(const __UBYTE__ &al, const gxINT32 &b);

  GXDLCODE_API friend int operator>=(const gxINT32 &a, const gxINT32 &b);
  GXDLCODE_API friend int operator>=(const gxINT32 &a, const __LWORD__ &bl);
  GXDLCODE_API friend int operator>=(const __LWORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator>=(const gxINT32 &a, const __ULWORD__ &bl);
  GXDLCODE_API friend int operator>=(const __ULWORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator>=(const gxINT32 &a, const __WORD__ &bl);
  GXDLCODE_API friend int operator>=(const __WORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator>=(const gxINT32 &a, const __SWORD__ &bl);
  GXDLCODE_API friend int operator>=(const __SWORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator>=(const gxINT32 &a, const __UWORD__ &bl);
  GXDLCODE_API friend int operator>=(const __UWORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator>=(const gxINT32 &a, const __USWORD__ &bl);
  GXDLCODE_API friend int operator>=(const __USWORD__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator>=(const gxINT32 &a, const __SBYTE__ &bl);
  GXDLCODE_API friend int operator>=(const __SBYTE__ &al, const gxINT32 &b);
  GXDLCODE_API friend int operator>=(const gxINT32 &a, const __UBYTE__ &bl);
  GXDLCODE_API friend int operator>=(const __UBYTE__ &al, const gxINT32 &b);
  
private:
  __SBYTE__ byte[4];
};
#endif // __USE_NATIVE_INT_TYPES__

#endif // __GX_INT32_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
