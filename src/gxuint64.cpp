// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxuint64.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/05/1997
// Date Last Modified: 10/24/2026
// Copyright (c) 2001-2026 DataReel Software Development
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

The gxUINT64 class is used to represent 64 bit unsigned integers
independently of the operating system or hardware platform used.
It works by separating a 64-bit value into eight separate byte
values and reordering the bytes lowest-order to highest-order.
A gxUINT64 type has a base 10 limit of 18446744073709551615.

Changes:
==============================================================
02/21/2002: Modified the PackBits() and UnPackBits() functions
to work with the GNU MINGW32 compiler.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"


#include "gxuint64.h"

#ifndef __USE_NATIVE_INT_TYPES__ 
#include <string.h>
#include <memory.h>

#include <cstdlib>
#include <string>

gxUINT64::gxUINT64(__ULLWORD__ val)
{
  UnPackBits(val);
}

gxUINT64::gxUINT64(const gxUINT64& ob)
{
  memmove((void *)byte, (const void *)ob.byte, 8);
}

gxUINT64& gxUINT64::operator=(const gxUINT64& ob)
{
  if(this != &ob) { // PC-lint 05/25/2005: Prevent self assignment 
    memmove((void *)byte, (const void *)ob.byte, 8);
  }
  return *this;
}

gxUINT64& gxUINT64::operator=(const __ULLWORD__ val)
{
  UnPackBits(val);
  return *this;
}

gxUINT64::operator __ULLWORD__() const
{
  return PackBits();
}

__ULLWORD__ gxUINT64::PackBits() const
{
  __ULLWORD__ a, b, c, d, e ,f, g, h;
  
  a = (__ULLWORD__)byte[0];
  b = (__ULLWORD__)byte[1];
  c = (__ULLWORD__)byte[2];
  d = (__ULLWORD__)byte[3];
  e = (__ULLWORD__)byte[4];
  f = (__ULLWORD__)byte[5];
  g = (__ULLWORD__)byte[6];
  h = (__ULLWORD__)byte[7];

  a = a & 0xFF;
  b = (b<<8) & 0xFF00;
  c = (c<<16) & 0xFF0000;
  d = (d<<24) & 0xFF000000;
#if defined(__LINUX__)
  e = (e<<32) & 0xFF00000000LL;
  f = (f<<40) & 0xFF0000000000LL;
  g = (g<<48) & 0xFF000000000000LL;
  h = (h<<56) & 0xFF00000000000000LL;
#else
  e = (e<<32) & 0xFF00000000;
  f = (f<<40) & 0xFF0000000000;
  g = (g<<48) & 0xFF000000000000;
  h = (h<<56) & 0xFF00000000000000;
#endif
  return a + b + c + d + e + f + g + h;
}

void gxUINT64::UnPackBits(__ULLWORD__ val)
{
  byte[0] = (__SBYTE__)(val & 0xFF);
  byte[1] = (__SBYTE__)((val & 0xFF00)>>8);
  byte[2] = (__SBYTE__)((val & 0xFF0000)>>16);
  byte[3] = (__SBYTE__)((val & 0xFF000000)>>24);
#if defined(__LINUX__)
  byte[4] = (__SBYTE__)((val & 0xFF00000000LL)>>32);
  byte[5] = (__SBYTE__)((val & 0xFF0000000000LL)>>40);
  byte[6] = (__SBYTE__)((val & 0xFF000000000000LL)>>48);
  byte[7] = (__SBYTE__)((val & 0xFF00000000000000LL)>>56);
#else
  byte[4] = (__SBYTE__)((val & 0xFF00000000)>>32);
  byte[5] = (__SBYTE__)((val & 0xFF0000000000)>>40);
  byte[6] = (__SBYTE__)((val & 0xFF000000000000)>>48);
  byte[7] = (__SBYTE__)((val & 0xFF00000000000000)>>56);
#endif
}

gxUINT64 gxUINT64::operator++(int) // Postfix
{
  gxUINT64 val_before(*this); 
  operator=(*this + 1);
  return val_before;
}
 
gxUINT64 gxUINT64::operator--(int) // Postfix
{
  gxUINT64 val_before(*this); 
  operator=(*this - 1);
  return val_before;
}

void gxUINT64::operator/=(const gxUINT64 &i)
{
  if(i == (gxUINT64)0) return; // Divide by zero error
  operator=(*this / i);
}

void gxUINT64::operator/=(const __LLWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__ULLWORD__)i);
}

void gxUINT64::operator/=(const __ULLWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / i);
}

void gxUINT64::operator/=(const __LWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__ULLWORD__)i); // PC-lint 09/15/2005: Ignore cast warning
}

void gxUINT64::operator/=(const __ULWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__ULLWORD__)i);
}

void gxUINT64::operator/=(const __WORD__ &i)
{
  if(i == 0) return; // Divide by zero error
 operator=(*this / (__ULLWORD__)i); // PC-lint 09/15/2005: Ignore cast warning
}

void gxUINT64::operator/=(const __SWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__ULLWORD__)i); // PC-lint 09/15/2005: Ignore cast warning
}

void gxUINT64::operator/=(const __UWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__ULLWORD__)i);
}

void gxUINT64::operator/=(const __USWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__ULLWORD__)i);
}

void gxUINT64::operator/=(const __SBYTE__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__ULLWORD__)i); // PC-lint 09/15/2005: Ignore cast warning
}

void gxUINT64::operator/=(const __UBYTE__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__ULLWORD__)i);
}

GXDLCODE_API int operator==(const gxUINT64 &a, const gxUINT64 &b)
{
  return a.PackBits() == b.PackBits();
}

GXDLCODE_API int operator==(const gxUINT64 &a, const __LLWORD__ &bl)
{
  return a.PackBits() == (__ULLWORD__)bl;
}

GXDLCODE_API int operator==(const __LLWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxUINT64 &a, const __ULLWORD__ &bl)
{
  return a.PackBits() == bl;
}

GXDLCODE_API int operator==(const __ULLWORD__ &al, const gxUINT64 &b)
{
  return al == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxUINT64 &a, const __LWORD__ &bl)
{
  return a.PackBits() == (__ULLWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator==(const __LWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxUINT64 &a, const __ULWORD__ &bl)
{
  return a.PackBits() == (__ULLWORD__)bl;
}

GXDLCODE_API int operator==(const __ULWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxUINT64 &a, const __WORD__ &bl)
{
  return a.PackBits() == (__ULLWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator==(const __WORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al == b.PackBits();
}

GXDLCODE_API int operator==(const gxUINT64 &a, const __SWORD__ &bl)
{
  return a.PackBits() == (__ULLWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator==(const __SWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al == b.PackBits(); // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator==(const gxUINT64 &a, const __UWORD__ &bl)
{
  return a.PackBits() == (__ULLWORD__)bl;
}

GXDLCODE_API int operator==(const __UWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxUINT64 &a, const __USWORD__ &bl)
{
  return  a.PackBits() == (__ULLWORD__)bl;
}

GXDLCODE_API int operator==(const __USWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxUINT64 &a, const __SBYTE__ &bl)
{
  return a.PackBits() == (__ULLWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator==(const __SBYTE__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al == b.PackBits(); // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator==(const gxUINT64 &a, const __UBYTE__ &bl)
{
  return a.PackBits() == (__ULLWORD__)bl;
}

GXDLCODE_API int operator==(const __UBYTE__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al == b.PackBits(); 
}

GXDLCODE_API int operator!=(const gxUINT64 &a, const gxUINT64 &b)
{
  return a.PackBits() != b.PackBits();
}

GXDLCODE_API int operator!=(const gxUINT64 &a, const __LLWORD__ &bl)
{
  return a.PackBits() != (__ULLWORD__)bl;
}

GXDLCODE_API int operator!=(const __LLWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxUINT64 &a, const __ULLWORD__ &bl)
{
  return a.PackBits() != bl;
}

GXDLCODE_API int operator!=(const __ULLWORD__ &al, const gxUINT64 &b)
{
  return al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxUINT64 &a, const __LWORD__ &bl)
{
  return a.PackBits() != (__ULLWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator!=(const __LWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al != b.PackBits(); // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator!=(const gxUINT64 &a, const __ULWORD__ &bl)
{
  return a.PackBits() != (__ULLWORD__)bl;
}

GXDLCODE_API int operator!=(const __ULWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxUINT64 &a, const __WORD__ &bl)
{
  return a.PackBits() != (__ULLWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator!=(const __WORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al != b.PackBits(); // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator!=(const gxUINT64 &a, const __SWORD__ &bl)
{
  return a.PackBits() != (__ULLWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator!=(const __SWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al != b.PackBits(); // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator!=(const gxUINT64 &a, const __UWORD__ &bl)
{
  return a.PackBits() != (__ULLWORD__)bl;
}

GXDLCODE_API int operator!=(const __UWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxUINT64 &a, const __USWORD__ &bl)
{
  return a.PackBits() != (__ULLWORD__)bl;
}

GXDLCODE_API int operator!=(const __USWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxUINT64 &a, const __SBYTE__ &bl)
{
  return a.PackBits() != (__ULLWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator!=(const __SBYTE__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al != b.PackBits(); // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator!=(const gxUINT64 &a, const __UBYTE__ &bl)
{
  return a.PackBits() != (__ULLWORD__)bl;
}

GXDLCODE_API int operator!=(const __UBYTE__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al != b.PackBits();
}

GXDLCODE_API int operator<(const gxUINT64 &a, const gxUINT64 &b)
{
  return a.PackBits() < b.PackBits();
}

GXDLCODE_API int operator<(const gxUINT64 &a, const __LWORD__ &bl)
{
  return a.PackBits() < (__ULLWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator<(const gxUINT64 &a, const __LLWORD__ &bl)
{
  return a.PackBits() < (__ULLWORD__)bl;
}

GXDLCODE_API int operator<(const __LLWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<(const gxUINT64 &a, const __ULLWORD__ &bl)
{
  return a.PackBits() < bl;
}

GXDLCODE_API int operator<(const __ULLWORD__ &al, const gxUINT64 &b)
{
  return al < b.PackBits();
}

GXDLCODE_API int operator<(const __LWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al < b.PackBits(); // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator<(const gxUINT64 &a, const __ULWORD__ &bl)
{
  return a.PackBits() < (__ULLWORD__)bl;
}

GXDLCODE_API int operator<(const __ULWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<(const gxUINT64 &a, const __WORD__ &bl)
{
  return a.PackBits() < (__ULLWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator<(const __WORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<(const gxUINT64 &a, const __SWORD__ &bl)
{
  return a.PackBits() < (__ULLWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator<(const __SWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al < b.PackBits(); // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator<(const gxUINT64 &a, const __UWORD__ &bl)
{
  return a.PackBits() < (__ULLWORD__)bl;
}

GXDLCODE_API int operator<(const __UWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<(const gxUINT64 &a, const __USWORD__ &bl)
{
  return a.PackBits() < (__ULLWORD__)bl;
}

GXDLCODE_API int operator<(const __USWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<(const gxUINT64 &a, const __SBYTE__ &bl)
{
  return a.PackBits() < (__ULLWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator<(const __SBYTE__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al < b.PackBits(); // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator<(const gxUINT64 &a, const __UBYTE__ &bl)
{
  return a.PackBits() < (__ULLWORD__)bl;
}

GXDLCODE_API int operator<(const __UBYTE__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al < b.PackBits();
}

GXDLCODE_API int operator>(const gxUINT64 &a, const gxUINT64 &b)
{
  return a.PackBits() > b.PackBits();
}

GXDLCODE_API int operator>(const gxUINT64 &a, const __LLWORD__ &bl)
{
  return a.PackBits() > (__ULLWORD__)bl;
}

GXDLCODE_API int operator>(const __LLWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>(const gxUINT64 &a, const __ULLWORD__ &bl)
{
  return a.PackBits() > bl;
}

GXDLCODE_API int operator>(const __ULLWORD__ &al, const gxUINT64 &b)
{
  return al > b.PackBits();
}

GXDLCODE_API int operator>(const gxUINT64 &a, const __LWORD__ &bl)
{
  return a.PackBits() > (__ULLWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator>(const __LWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>(const gxUINT64 &a, const __ULWORD__ &bl)
{
  return a.PackBits() > (__ULLWORD__)bl;
}

GXDLCODE_API int operator>(const __ULWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>(const gxUINT64 &a, const __WORD__ &bl)
{
  return a.PackBits() > (__ULLWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator>(const __WORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al > b.PackBits(); // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator>(const gxUINT64 &a, const __SWORD__ &bl)
{
  return a.PackBits() > (__ULLWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator>(const __SWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al > b.PackBits(); // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator>(const gxUINT64 &a, const __UWORD__ &bl)
{
  return a.PackBits() > (__ULLWORD__)bl;
}

GXDLCODE_API int operator>(const __UWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>(const gxUINT64 &a, const __USWORD__ &bl)
{
  return a.PackBits() > (__ULLWORD__)bl;
}

GXDLCODE_API int operator>(const __USWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>(const gxUINT64 &a, const __SBYTE__ &bl)
{
  return a.PackBits() > (__ULLWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator>(const __SBYTE__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al > b.PackBits(); // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator>(const gxUINT64 &a, const __UBYTE__ &bl)
{
  return a.PackBits() > (__ULLWORD__)bl;
}

GXDLCODE_API int operator>(const __UBYTE__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al > b.PackBits();
}

GXDLCODE_API int operator<=(const gxUINT64 &a, const gxUINT64 &b)
{
  return a.PackBits() == b.PackBits() || a.PackBits() < b.PackBits();
}

GXDLCODE_API int operator<=(const gxUINT64 &a, const __LLWORD__ &bl)
{
  return a.PackBits() == (__ULLWORD__)bl || a.PackBits() < (__ULLWORD__)bl;
}

GXDLCODE_API int operator<=(const __LLWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al == b.PackBits() || (__ULLWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxUINT64 &a, const __ULLWORD__ &bl)
{
  return a.PackBits() == bl || a.PackBits() < bl;
}

GXDLCODE_API int operator<=(const __ULLWORD__ &al, const gxUINT64 &b)
{
  return al == b.PackBits() || al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxUINT64 &a, const __LWORD__ &bl)
{
  return a.PackBits() == (__ULLWORD__)bl || a.PackBits() < (__ULLWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator<=(const __LWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al == b.PackBits() || (__ULLWORD__)al < b.PackBits(); // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator<=(const gxUINT64 &a, const __ULWORD__ &bl)
{
  return a.PackBits() == (__ULLWORD__)bl || a.PackBits() < (__ULLWORD__)bl;
}

GXDLCODE_API int operator<=(const __ULWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al == b.PackBits() || (__ULLWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxUINT64 &a, const __WORD__ &bl)
{
  return a.PackBits() == (__ULLWORD__)bl || a.PackBits() < (__ULLWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator<=(const __WORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al == b.PackBits() || (__ULLWORD__)al < b.PackBits(); // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator<=(const gxUINT64 &a, const __SWORD__ &bl)
{
  return a.PackBits() == (__ULLWORD__)bl || a.PackBits() < (__ULLWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator<=(const __SWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al == b.PackBits() || (__ULLWORD__)al < b.PackBits(); // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator<=(const gxUINT64 &a, const __UWORD__ &bl)
{
  return a.PackBits() == (__ULLWORD__)bl || a.PackBits() < (__ULLWORD__)bl;
}

GXDLCODE_API int operator<=(const __UWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al == b.PackBits() || (__ULLWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxUINT64 &a, const __USWORD__ &bl)
{
  return a.PackBits() == (__ULLWORD__)bl || a.PackBits() < (__ULLWORD__)bl;
}

GXDLCODE_API int operator<=(const __USWORD__ &al, const gxUINT64 &b)
{
  return al == b.PackBits() || al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxUINT64 &a, const __SBYTE__ &bl)
{
  return a.PackBits() == (__ULLWORD__)bl ||  a.PackBits() < (__ULLWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator<=(const __SBYTE__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al == b.PackBits() || (__ULLWORD__)al < b.PackBits(); // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator<=(const gxUINT64 &a, const __UBYTE__ &bl)
{
  return a.PackBits() == (__ULLWORD__)bl ||  a.PackBits() < (__ULLWORD__)bl;
}

GXDLCODE_API int operator<=(const __UBYTE__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al == b.PackBits() || (__ULLWORD__)al < b.PackBits();
}

GXDLCODE_API int operator>=(const gxUINT64 &a, const gxUINT64 &b)
{
  return a.PackBits() == b.PackBits() || a.PackBits() > b.PackBits();
}

GXDLCODE_API int operator>=(const gxUINT64 &a, const __LWORD__ &bl)
{
  return a.PackBits() == (__ULLWORD__)bl || a.PackBits() > (__ULLWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator>=(const gxUINT64 &a, const __LLWORD__ &bl)
{
  return a.PackBits() == (__ULLWORD__)bl || a.PackBits() > (__ULLWORD__)bl;
}

GXDLCODE_API int operator>=(const __LLWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al == b.PackBits() || (__ULLWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxUINT64 &a, const __ULLWORD__ &bl)
{
  return a.PackBits() == bl || a.PackBits() > bl;
}

GXDLCODE_API int operator>=(const __ULLWORD__ &al, const gxUINT64 &b)
{
  return al == b.PackBits() || al > b.PackBits();
}

GXDLCODE_API int operator>=(const __LWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al == b.PackBits() || (__ULLWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxUINT64 &a, const __ULWORD__ &bl)
{
  return a.PackBits() == (__ULLWORD__)bl || a.PackBits() > (__ULLWORD__)bl;
}

GXDLCODE_API int operator>=(const __ULWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al == b.PackBits() || (__ULLWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxUINT64 &a, const __WORD__ &bl)
{
  return a.PackBits() == (__ULLWORD__)bl || a.PackBits() > (__ULLWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator>=(const __WORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al == b.PackBits() || (__ULLWORD__)al > b.PackBits(); // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator>=(const gxUINT64 &a, const __SWORD__ &bl)
{
  return a.PackBits() == (__ULLWORD__)bl || a.PackBits() > (__ULLWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator>=(const __SWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al == b.PackBits() || (__ULLWORD__)al > b.PackBits(); // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator>=(const gxUINT64 &a, const __UWORD__ &bl)
{
  return a.PackBits() == (__ULLWORD__)bl || a.PackBits() > (__ULLWORD__)bl;
}

GXDLCODE_API int operator>=(const __UWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al == b.PackBits() || (__ULLWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxUINT64 &a, const __USWORD__ &bl)
{
  return a.PackBits() == (__ULLWORD__)bl || a.PackBits() > (__ULLWORD__)bl;
}

GXDLCODE_API int operator>=(const __USWORD__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al == b.PackBits() || (__ULLWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxUINT64 &a, const __SBYTE__ &bl)
{
  return a.PackBits() == (__ULLWORD__)bl || a.PackBits() > (__ULLWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator>=(const __SBYTE__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al == b.PackBits() || (__ULLWORD__)al > b.PackBits(); // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator>=(const gxUINT64 &a, const __UBYTE__ &bl)
{
  return a.PackBits() == (__ULLWORD__)bl || a.PackBits() > (__ULLWORD__)bl;
}

GXDLCODE_API int operator>=(const __UBYTE__ &al, const gxUINT64 &b)
{
  return (__ULLWORD__)al == b.PackBits() || (__ULLWORD__)al > b.PackBits();
}

std::string gxUINT64::str() const
// Returns this object's 64-bit value as a decimal string
{
  return std::to_string((unsigned long long)PackBits());
}

gxUINT64 &gxUINT64::FromString(const std::string &s)
{
  *this = (__ULLWORD__)std::strtoull(s.c_str(), nullptr, 10);
  return *this;
}

GXDLCODE_API std::ostream &operator<<(std::ostream &os, 
					const gxUINT64 &val)
{
  return os << val.str();
}

GXDLCODE_API std::istream &operator>>(std::istream &os, 
					gxUINT64 &val) 
{
  std::string sbuf;
  os >> sbuf;
  val.FromString(sbuf);
  return os;
}

#endif // __USE_NATIVE_INT_TYPES__ 

// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
