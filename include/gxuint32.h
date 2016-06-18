// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxuint32.h
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

The gxUINT32 class is used to represent 32 bit unsigned integers
independently of the operating system or hardware platform used.
It works by separating a 32-bit value into four separate byte
values and reordering the bytes lowest-order to highest-order.
An gxUINT32 type has a base 10 limit of 4,294,967,295.
*/
// ----------------------------------------------------------- //   
#ifndef __GX_UINT32_HPP__
#define __GX_UINT32_HPP__

#include "gxdlcode.h"

#include "gxdtypes.h"

#if defined (__USE_NATIVE_INT_TYPES__)
typedef unsigned long gxUINT32;
#else
// Data structure for unsigned 32 bit integer values.
class GXDLCODE_API gxUINT32
{
public:
  gxUINT32(__ULWORD__ val = 0);
  gxUINT32(const gxUINT32& ob);
  gxUINT32& operator=(const gxUINT32& ob);
  gxUINT32& operator=(const __ULWORD__ ob);

public:
  void UnPackBits(__ULWORD__ val);
  __ULWORD__ PackBits() const;

public:
  operator __ULWORD__() const;
  
public: // Arithmetic operators that modify their operand
  gxUINT32 operator++(int);  // Postfix
  gxUINT32 operator--(int);  // Postfix
  gxUINT32 &operator++() { operator=(*this + 1); return *this; } // Prefix
  gxUINT32 &operator--() { operator=(*this - 1); return *this; } // Prefix
  void operator+=(const gxUINT32 &i) { operator=(*this + i); }
  void operator-=(const gxUINT32 &i) { operator=(*this - i); }
  void operator*=(const gxUINT32 &i) { operator=(*this * i); }
  void operator/=(const gxUINT32 &i);

  void operator+=(const __LWORD__ &i) { operator=(*this + (__ULWORD__)i); }
  void operator-=(const __LWORD__ &i) { operator=(*this - (__ULWORD__)i); }
  void operator*=(const __LWORD__ &i) { operator=(*this * (__ULWORD__)i); }
  void operator/=(const __LWORD__ &i);

  void operator+=(const __ULWORD__ &i) { operator=(*this + (__ULWORD__)i); }
  void operator-=(const __ULWORD__ &i) { operator=(*this - (__ULWORD__)i); }
  void operator*=(const __ULWORD__ &i) { operator=(*this * (__ULWORD__)i); }
  void operator/=(const __ULWORD__ &i);

  void operator+=(const __WORD__ &i) { operator=(*this + (__ULWORD__)i); }
  void operator-=(const __WORD__ &i) { operator=(*this - (__ULWORD__)i); }
  void operator*=(const __WORD__ &i) { operator=(*this * (__ULWORD__)i); }
  void operator/=(const __WORD__ &i);

  void operator+=(const __SWORD__ &i) { operator=(*this + (__ULWORD__)i); }
  void operator-=(const __SWORD__ &i) { operator=(*this - (__ULWORD__)i); }
  void operator*=(const __SWORD__ &i) { operator=(*this * (__ULWORD__)i); }
  void operator/=(const __SWORD__ &i);

  void operator+=(const __UWORD__ &i) { operator=(*this + (__ULWORD__)i); }
  void operator-=(const __UWORD__ &i) { operator=(*this - (__ULWORD__)i); }
  void operator*=(const __UWORD__ &i) { operator=(*this * (__ULWORD__)i); }
  void operator/=(const __UWORD__ &i);

  void operator+=(const __USWORD__ &i) { operator=(*this + (__ULWORD__)i); }
  void operator-=(const __USWORD__ &i) { operator=(*this - (__ULWORD__)i); }
  void operator*=(const __USWORD__ &i) { operator=(*this * (__ULWORD__)i); }
  void operator/=(const __USWORD__ &i);

  void operator+=(const __SBYTE__ &i) { operator=(*this + (__ULWORD__)i); }
  void operator-=(const __SBYTE__ &i) { operator=(*this - (__ULWORD__)i); }
  void operator*=(const __SBYTE__ &i) { operator=(*this * (__ULWORD__)i); }
  void operator/=(const __SBYTE__ &i);

  void operator+=(const __UBYTE__ &i) { operator=(*this + (__ULWORD__)i); }
  void operator-=(const __UBYTE__ &i) { operator=(*this - (__ULWORD__)i); }
  void operator*=(const __UBYTE__ &i) { operator=(*this * (__ULWORD__)i); }
  void operator/=(const __UBYTE__ &i);

public: // Comparison operators
  GXDLCODE_API friend int operator==(const gxUINT32 &a, const gxUINT32 &b);
  GXDLCODE_API friend int operator==(const gxUINT32 &a, const __LWORD__ &bl);
  GXDLCODE_API friend int operator==(const __LWORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator==(const gxUINT32 &a, const __ULWORD__ &bl);
  GXDLCODE_API friend int operator==(const __ULWORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator==(const gxUINT32 &a, const __WORD__ &bl);
  GXDLCODE_API friend int operator==(const __WORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator==(const gxUINT32 &a, const __SWORD__ &bl);
  GXDLCODE_API friend int operator==(const __SWORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator==(const gxUINT32 &a, const __UWORD__ &bl);
  GXDLCODE_API friend int operator==(const __UWORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator==(const gxUINT32 &a, const __USWORD__ &bl);
  GXDLCODE_API friend int operator==(const __USWORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator==(const gxUINT32 &a, const __SBYTE__ &bl);
  GXDLCODE_API friend int operator==(const __SBYTE__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator==(const gxUINT32 &a, const __UBYTE__ &bl);
  GXDLCODE_API friend int operator==(const __UBYTE__ &al, const gxUINT32 &b);

  GXDLCODE_API friend int operator!=(const gxUINT32 &a, const gxUINT32 &b);
  GXDLCODE_API friend int operator!=(const gxUINT32 &a, const __LWORD__ &bl);
  GXDLCODE_API friend int operator!=(const __LWORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator!=(const gxUINT32 &a, const __ULWORD__ &bl);
  GXDLCODE_API friend int operator!=(const __ULWORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator!=(const gxUINT32 &a, const __WORD__ &bl);
  GXDLCODE_API friend int operator!=(const __WORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator!=(const gxUINT32 &a, const __SWORD__ &bl);
  GXDLCODE_API friend int operator!=(const __SWORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator!=(const gxUINT32 &a, const __UWORD__ &bl);
  GXDLCODE_API friend int operator!=(const __UWORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator!=(const gxUINT32 &a, const __USWORD__ &bl);
  GXDLCODE_API friend int operator!=(const __USWORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator!=(const gxUINT32 &a, const __SBYTE__ &bl);
  GXDLCODE_API friend int operator!=(const __SBYTE__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator!=(const gxUINT32 &a, const __UBYTE__ &bl);
  GXDLCODE_API friend int operator!=(const __UBYTE__ &al, const gxUINT32 &b);

  GXDLCODE_API friend int operator<(const gxUINT32 &a, const gxUINT32 &b);
  GXDLCODE_API friend int operator<(const gxUINT32 &a, const __LWORD__ &bl);
  GXDLCODE_API friend int operator<(const __LWORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator<(const gxUINT32 &a, const __ULWORD__ &bl);
  GXDLCODE_API friend int operator<(const __ULWORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator<(const gxUINT32 &a, const __WORD__ &bl);
  GXDLCODE_API friend int operator<(const __WORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator<(const gxUINT32 &a, const __SWORD__ &bl);
  GXDLCODE_API friend int operator<(const __SWORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator<(const gxUINT32 &a, const __UWORD__ &bl);
  GXDLCODE_API friend int operator<(const __UWORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator<(const gxUINT32 &a, const __USWORD__ &bl);
  GXDLCODE_API friend int operator<(const __USWORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator<(const gxUINT32 &a, const __SBYTE__ &bl);
  GXDLCODE_API friend int operator<(const __SBYTE__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator<(const gxUINT32 &a, const __UBYTE__ &bl);
  GXDLCODE_API friend int operator<(const __UBYTE__ &al, const gxUINT32 &b);

  GXDLCODE_API friend int operator>(const gxUINT32 &a, const gxUINT32 &b);
  GXDLCODE_API friend int operator>(const gxUINT32 &a, const __LWORD__ &bl);
  GXDLCODE_API friend int operator>(const __LWORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator>(const gxUINT32 &a, const __ULWORD__ &bl);
  GXDLCODE_API friend int operator>(const __ULWORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator>(const gxUINT32 &a, const __WORD__ &bl);
  GXDLCODE_API friend int operator>(const __WORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator>(const gxUINT32 &a, const __SWORD__ &bl);
  GXDLCODE_API friend int operator>(const __SWORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator>(const gxUINT32 &a, const __UWORD__ &bl);
  GXDLCODE_API friend int operator>(const __UWORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator>(const gxUINT32 &a, const __USWORD__ &bl);
  GXDLCODE_API friend int operator>(const __USWORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator>(const gxUINT32 &a, const __SBYTE__ &bl);
  GXDLCODE_API friend int operator>(const __SBYTE__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator>(const gxUINT32 &a, const __UBYTE__ &bl);
  GXDLCODE_API friend int operator>(const __UBYTE__ &al, const gxUINT32 &b);

  GXDLCODE_API friend int operator<=(const gxUINT32 &a, const gxUINT32 &b);
  GXDLCODE_API friend int operator<=(const gxUINT32 &a, const __LWORD__ &bl);
  GXDLCODE_API friend int operator<=(const __LWORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator<=(const gxUINT32 &a, const __ULWORD__ &bl);
  GXDLCODE_API friend int operator<=(const __ULWORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator<=(const gxUINT32 &a, const __WORD__ &bl);
  GXDLCODE_API friend int operator<=(const __WORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator<=(const gxUINT32 &a, const __SWORD__ &bl);
  GXDLCODE_API friend int operator<=(const __SWORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator<=(const gxUINT32 &a, const __UWORD__ &bl);
  GXDLCODE_API friend int operator<=(const __UWORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator<=(const gxUINT32 &a, const __USWORD__ &bl);
  GXDLCODE_API friend int operator<=(const __USWORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator<=(const gxUINT32 &a, const __SBYTE__ &bl);
  GXDLCODE_API friend int operator<=(const __SBYTE__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator<=(const gxUINT32 &a, const __UBYTE__ &bl);
  GXDLCODE_API friend int operator<=(const __UBYTE__ &al, const gxUINT32 &b);

  GXDLCODE_API friend int operator>=(const gxUINT32 &a, const gxUINT32 &b);
  GXDLCODE_API friend int operator>=(const gxUINT32 &a, const __LWORD__ &bl);
  GXDLCODE_API friend int operator>=(const __LWORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator>=(const gxUINT32 &a, const __ULWORD__ &bl);
  GXDLCODE_API friend int operator>=(const __ULWORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator>=(const gxUINT32 &a, const __WORD__ &bl);
  GXDLCODE_API friend int operator>=(const __WORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator>=(const gxUINT32 &a, const __SWORD__ &bl);
  GXDLCODE_API friend int operator>=(const __SWORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator>=(const gxUINT32 &a, const __UWORD__ &bl);
  GXDLCODE_API friend int operator>=(const __UWORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator>=(const gxUINT32 &a, const __USWORD__ &bl);
  GXDLCODE_API friend int operator>=(const __USWORD__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator>=(const gxUINT32 &a, const __SBYTE__ &bl);
  GXDLCODE_API friend int operator>=(const __SBYTE__ &al, const gxUINT32 &b);
  GXDLCODE_API friend int operator>=(const gxUINT32 &a, const __UBYTE__ &bl);
  GXDLCODE_API friend int operator>=(const __UBYTE__ &al, const gxUINT32 &b);
  
private:
  __UBYTE__ byte[4];
};
#endif // __USE_NATIVE_INT_TYPES__

#endif // __GX_UINT32_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
