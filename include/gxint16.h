// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxint16.h
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/05/1997  
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

The gxINT16 class is used to represent 16 bit signed integers
independently of the operating system or hardware platform used.
It works by separating 16-bit values into two separate byte
values and reordering the bytes lowest-order to highest-order.
An gxINT16 type has a base 10 positive limit of 32,767 and a
negative limit of 32,768.
*/
// ----------------------------------------------------------- //   
#ifndef __GX_INT16_HPP__
#define __GX_INT16_HPP__

#include "gxdlcode.h"

#include "gxdtypes.h"

#if defined (__USE_NATIVE_INT_TYPES__)
typedef short gxINT16;
#else

#ifdef __BCC32__
#pragma warn -8071
#endif

// Data structure for signed 16 bit integer values.
class GXDLCODE_API gxINT16
{
public:
  gxINT16(__SWORD__ val = 0);
  gxINT16(const gxINT16& ob);
  gxINT16& operator=(const gxINT16& ob);
  gxINT16& operator=(const __SWORD__ ob);

public:
  void UnPackBits(__SWORD__ val);
  __SWORD__ PackBits() const;

public:
  operator __SWORD__() const;
  
public: // Arithmetic operators that modify their operand
  gxINT16 operator++(int);  // Postfix
  gxINT16 operator--(int);  // Postfix

  // Ignore all BCC32 conversion warnings
  gxINT16 &operator++() { operator=(*this + (__SWORD__)1); return *this; }
  gxINT16 &operator--() { operator=(*this - (__SWORD__)1); return *this; }

  void operator+=(const gxINT16 &i) { operator=(*this + i); }
  void operator-=(const gxINT16 &i) { operator=(*this - i); }
  void operator*=(const gxINT16 &i) { operator=(*this * i); }
  void operator/=(const gxINT16 &i);

  void operator+=(const __LWORD__ &i) { operator=(*this + (__SWORD__)i); }
  void operator-=(const __LWORD__ &i) { operator=(*this - (__SWORD__)i); }
  void operator*=(const __LWORD__ &i) { operator=(*this * (__SWORD__)i); }
  void operator/=(const __LWORD__ &i);

  void operator+=(const __ULWORD__ &i) { operator=(*this + (__SWORD__)i); }
  void operator-=(const __ULWORD__ &i) { operator=(*this - (__SWORD__)i); }
  void operator*=(const __ULWORD__ &i) { operator=(*this * (__SWORD__)i); }
  void operator/=(const __ULWORD__ &i);

  void operator+=(const __WORD__ &i) { operator=(*this + (__SWORD__)i); }
  void operator-=(const __WORD__ &i) { operator=(*this - (__SWORD__)i); }
  void operator*=(const __WORD__ &i) { operator=(*this * (__SWORD__)i); }
  void operator/=(const __WORD__ &i);

  void operator+=(const __SWORD__ &i) { operator=(*this + (__SWORD__)i); }
  void operator-=(const __SWORD__ &i) { operator=(*this - (__SWORD__)i); }
  void operator*=(const __SWORD__ &i) { operator=(*this * (__SWORD__)i); }
  void operator/=(const __SWORD__ &i);

  void operator+=(const __UWORD__ &i) { operator=(*this + (__SWORD__)i); }
  void operator-=(const __UWORD__ &i) { operator=(*this - (__SWORD__)i); }
  void operator*=(const __UWORD__ &i) { operator=(*this * (__SWORD__)i); }
  void operator/=(const __UWORD__ &i);

  void operator+=(const __USWORD__ &i) { operator=(*this + i); }
  void operator-=(const __USWORD__ &i) { operator=(*this - i); }
  void operator*=(const __USWORD__ &i) { operator=(*this * i); }
  void operator/=(const __USWORD__ &i);

  void operator+=(const __SBYTE__ &i) { operator=(*this + (__SWORD__)i); }
  void operator-=(const __SBYTE__ &i) { operator=(*this - (__SWORD__)i); }
  void operator*=(const __SBYTE__ &i) { operator=(*this * (__SWORD__)i); }
  void operator/=(const __SBYTE__ &i);

  void operator+=(const __UBYTE__ &i) { operator=(*this + (__SWORD__)i); }
  void operator-=(const __UBYTE__ &i) { operator=(*this - (__SWORD__)i); }
  void operator*=(const __UBYTE__ &i) { operator=(*this * (__SWORD__)i); }
  void operator/=(const __UBYTE__ &i);

public: // Comparison operators
  GXDLCODE_API friend int operator==(const gxINT16 &a, const gxINT16 &b);
  GXDLCODE_API friend int operator==(const gxINT16 &a, const __LWORD__ &bs);
  GXDLCODE_API friend int operator==(const __LWORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator==(const gxINT16 &a, const __ULWORD__ &bs);
  GXDLCODE_API friend int operator==(const __ULWORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator==(const gxINT16 &a, const __WORD__ &bs);
  GXDLCODE_API friend int operator==(const __WORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator==(const gxINT16 &a, const __SWORD__ &bs);
  GXDLCODE_API friend int operator==(const __SWORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator==(const gxINT16 &a, const __UWORD__ &bs);
  GXDLCODE_API friend int operator==(const __UWORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator==(const gxINT16 &a, const __USWORD__ &bs);
  GXDLCODE_API friend int operator==(const __USWORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator==(const gxINT16 &a, const __SBYTE__ &bs);
  GXDLCODE_API friend int operator==(const __SBYTE__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator==(const gxINT16 &a, const __UBYTE__ &bs);
  GXDLCODE_API friend int operator==(const __UBYTE__ &as, const gxINT16 &b);

  GXDLCODE_API friend int operator!=(const gxINT16 &a, const gxINT16 &b);
  GXDLCODE_API friend int operator!=(const gxINT16 &a, const __LWORD__ &bs);
  GXDLCODE_API friend int operator!=(const __LWORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator!=(const gxINT16 &a, const __ULWORD__ &bs);
  GXDLCODE_API friend int operator!=(const __ULWORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator!=(const gxINT16 &a, const __WORD__ &bs);
  GXDLCODE_API friend int operator!=(const __WORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator!=(const gxINT16 &a, const __SWORD__ &bs);
  GXDLCODE_API friend int operator!=(const __SWORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator!=(const gxINT16 &a, const __UWORD__ &bs);
  GXDLCODE_API friend int operator!=(const __UWORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator!=(const gxINT16 &a, const __USWORD__ &bs);
  GXDLCODE_API friend int operator!=(const __USWORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator!=(const gxINT16 &a, const __SBYTE__ &bs);
  GXDLCODE_API friend int operator!=(const __SBYTE__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator!=(const gxINT16 &a, const __UBYTE__ &bs);
  GXDLCODE_API friend int operator!=(const __UBYTE__ &as, const gxINT16 &b);

  GXDLCODE_API friend int operator<(const gxINT16 &a, const gxINT16 &b);
  GXDLCODE_API friend int operator<(const gxINT16 &a, const __LWORD__ &bs);
  GXDLCODE_API friend int operator<(const __LWORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator<(const gxINT16 &a, const __ULWORD__ &bs);
  GXDLCODE_API friend int operator<(const __ULWORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator<(const gxINT16 &a, const __WORD__ &bs);
  GXDLCODE_API friend int operator<(const __WORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator<(const gxINT16 &a, const __SWORD__ &bs);
  GXDLCODE_API friend int operator<(const __SWORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator<(const gxINT16 &a, const __UWORD__ &bs);
  GXDLCODE_API friend int operator<(const __UWORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator<(const gxINT16 &a, const __USWORD__ &bs);
  GXDLCODE_API friend int operator<(const __USWORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator<(const gxINT16 &a, const __SBYTE__ &bs);
  GXDLCODE_API friend int operator<(const __SBYTE__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator<(const gxINT16 &a, const __UBYTE__ &bs);
  GXDLCODE_API friend int operator<(const __UBYTE__ &as, const gxINT16 &b);

  GXDLCODE_API friend int operator>(const gxINT16 &a, const gxINT16 &b);
  GXDLCODE_API friend int operator>(const gxINT16 &a, const __LWORD__ &bs);
  GXDLCODE_API friend int operator>(const __LWORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator>(const gxINT16 &a, const __ULWORD__ &bs);
  GXDLCODE_API friend int operator>(const __ULWORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator>(const gxINT16 &a, const __WORD__ &bs);
  GXDLCODE_API friend int operator>(const __WORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator>(const gxINT16 &a, const __SWORD__ &bs);
  GXDLCODE_API friend int operator>(const __SWORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator>(const gxINT16 &a, const __UWORD__ &bs);
  GXDLCODE_API friend int operator>(const __UWORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator>(const gxINT16 &a, const __USWORD__ &bs);
  GXDLCODE_API friend int operator>(const __USWORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator>(const gxINT16 &a, const __SBYTE__ &bs);
  GXDLCODE_API friend int operator>(const __SBYTE__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator>(const gxINT16 &a, const __UBYTE__ &bs);
  GXDLCODE_API friend int operator>(const __UBYTE__ &as, const gxINT16 &b);

  GXDLCODE_API friend int operator<=(const gxINT16 &a, const gxINT16 &b);
  GXDLCODE_API friend int operator<=(const gxINT16 &a, const __LWORD__ &bs);
  GXDLCODE_API friend int operator<=(const __LWORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator<=(const gxINT16 &a, const __ULWORD__ &bs);
  GXDLCODE_API friend int operator<=(const __ULWORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator<=(const gxINT16 &a, const __WORD__ &bs);
  GXDLCODE_API friend int operator<=(const __WORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator<=(const gxINT16 &a, const __SWORD__ &bs);
  GXDLCODE_API friend int operator<=(const __SWORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator<=(const gxINT16 &a, const __UWORD__ &bs);
  GXDLCODE_API friend int operator<=(const __UWORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator<=(const gxINT16 &a, const __USWORD__ &bs);
  GXDLCODE_API friend int operator<=(const __USWORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator<=(const gxINT16 &a, const __SBYTE__ &bs);
  GXDLCODE_API friend int operator<=(const __SBYTE__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator<=(const gxINT16 &a, const __UBYTE__ &bs);
  GXDLCODE_API friend int operator<=(const __UBYTE__ &as, const gxINT16 &b);

  GXDLCODE_API friend int operator>=(const gxINT16 &a, const gxINT16 &b);
  GXDLCODE_API friend int operator>=(const gxINT16 &a, const __LWORD__ &bs);
  GXDLCODE_API friend int operator>=(const __LWORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator>=(const gxINT16 &a, const __ULWORD__ &bs);
  GXDLCODE_API friend int operator>=(const __ULWORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator>=(const gxINT16 &a, const __WORD__ &bs);
  GXDLCODE_API friend int operator>=(const __WORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator>=(const gxINT16 &a, const __SWORD__ &bs);
  GXDLCODE_API friend int operator>=(const __SWORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator>=(const gxINT16 &a, const __UWORD__ &bs);
  GXDLCODE_API friend int operator>=(const __UWORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator>=(const gxINT16 &a, const __USWORD__ &bs);
  GXDLCODE_API friend int operator>=(const __USWORD__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator>=(const gxINT16 &a, const __SBYTE__ &bs);
  GXDLCODE_API friend int operator>=(const __SBYTE__ &as, const gxINT16 &b);
  GXDLCODE_API friend int operator>=(const gxINT16 &a, const __UBYTE__ &bs);
  GXDLCODE_API friend int operator>=(const __UBYTE__ &as, const gxINT16 &b);
  
private:
  __SBYTE__ byte[2];
};

#ifdef __BCC32__
#pragma warn .8071
#endif

#endif // __USE_NATIVE_INT_TYPES__

#endif // __GX_INT16_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
