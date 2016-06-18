// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxint64.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/05/1997
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

The gxINT64 class is used to represent 64 bit signed integers
independently of the operating system or hardware platform used.
It works by separating a 64-bit value into eight separate byte
values and reordering the bytes lowest-order to highest-order.
A gxINT64 type has a base 10 positive limit of
9,223,372,036,854,775,807 and a negative limit of
9,223,372,036,854,775,808

Changes:
==============================================================
02/21/2002: Modified the PackBits() and UnPackBits() functions
to work with the GNU MINGW32 compiler.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__64_BIT_DATABASE_ENGINE__) || defined (__ENABLE_64_BIT_INTEGERS__)

#include "gxint64.h"

#ifndef __USE_NATIVE_INT_TYPES__
#include <string.h>
#include <memory.h>
#include "strutil.h"

gxINT64::gxINT64(__LLWORD__ val)
{
  UnPackBits(val);
}

gxINT64::gxINT64(const gxINT64& ob)
{
  memmove((void *)byte, (const void *)ob.byte, 8);
}

gxINT64& gxINT64::operator=(const gxINT64& ob)
{
  if(this != &ob) { // PC-lint 04/19/2004: Check assignment
    memmove((void *)byte, (const void *)ob.byte, 8);
  }
  return *this;
}

gxINT64& gxINT64::operator=(const __LLWORD__ val)
{
  UnPackBits(val);
  return *this;
}

gxINT64::operator __LLWORD__() const
{
  return PackBits();
}

__LLWORD__ gxINT64::PackBits() const
{
  __LLWORD__ a, b, c, d, e ,f, g, h;
  
  a = (__LLWORD__)byte[0];
  b = (__LLWORD__)byte[1];
  c = (__LLWORD__)byte[2];
  d = (__LLWORD__)byte[3];
  e = (__LLWORD__)byte[4];
  f = (__LLWORD__)byte[5];
  g = (__LLWORD__)byte[6];
  h = (__LLWORD__)byte[7];

  a = a & 0xFF;
  b = (b<<8) & 0xFF00;
  c = (c<<16) & 0xFF0000;
  d = (d<<24) & 0xFF000000;
#if defined (__MINGW32__)
  e = (e<<32) & 0xFF00000000LL;
  f = (f<<40) & 0xFF0000000000LL;
  g = (g<<48) & 0xFF000000000000LL;
  h = (h<<56) & 0xFF00000000000000LL;
#elif defined (__LINUX__)
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

void gxINT64::UnPackBits(__LLWORD__ val)
{
  byte[0] = (__SBYTE__)(val & 0xFF);
  byte[1] = (__SBYTE__)((val & 0xFF00)>>8);
  byte[2] = (__SBYTE__)((val & 0xFF0000)>>16);
  byte[3] = (__SBYTE__)((val & 0xFF000000)>>24);
#if defined (__MINGW32__)
  byte[4] = (__SBYTE__)((val & 0xFF00000000LL)>>32);
  byte[5] = (__SBYTE__)((val & 0xFF0000000000LL)>>40);
  byte[6] = (__SBYTE__)((val & 0xFF000000000000LL)>>48);
  byte[7] = (__SBYTE__)((val & 0xFF00000000000000LL)>>56);
#elif defined (__LINUX__)
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

gxINT64 gxINT64::operator++(int) // Postfix
{
  gxINT64 val_before(*this); 
  operator=(*this + 1);
  return val_before;
}
 
gxINT64 gxINT64::operator--(int) // Postfix
{
  gxINT64 val_before(*this); 
  operator=(*this - 1);
  return val_before;
}

void gxINT64::operator/=(const gxINT64 &i)
{
  if(i == (gxINT64)0) return; // Divide by zero error
  operator=(*this / i);
}

void gxINT64::operator/=(const __LLWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / i);
}

void gxINT64::operator/=(const __ULLWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__LLWORD__)i);
}

void gxINT64::operator/=(const __LWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / i);
}

void gxINT64::operator/=(const __ULWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__LLWORD__)i);
}

void gxINT64::operator/=(const __WORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__LLWORD__)i);
}

void gxINT64::operator/=(const __SWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__LLWORD__)i);
}

void gxINT64::operator/=(const __UWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__LLWORD__)i);
}

void gxINT64::operator/=(const __USWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__LLWORD__)i);
}

void gxINT64::operator/=(const __SBYTE__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__LLWORD__)i);
}

void gxINT64::operator/=(const __UBYTE__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__LLWORD__)i);
}

GXDLCODE_API int operator==(const gxINT64 &a, const gxINT64 &b)
{
  return a.PackBits() == b.PackBits();
}

GXDLCODE_API int operator==(const gxINT64 &a, const __LLWORD__ &bl)
{
  return a.PackBits() == bl;
}

GXDLCODE_API int operator==(const __LLWORD__ &al, const gxINT64 &b)
{
  return al == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxINT64 &a, const __ULLWORD__ &bl)
{
  return a.PackBits() == (__LLWORD__)bl;
}

GXDLCODE_API int operator==(const __ULLWORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxINT64 &a, const __LWORD__ &bl)
{
  return a.PackBits() == bl;
}

GXDLCODE_API int operator==(const __LWORD__ &al, const gxINT64 &b)
{
  return al == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxINT64 &a, const __ULWORD__ &bl)
{
  return a.PackBits() == (__LLWORD__)bl;
}

GXDLCODE_API int operator==(const __ULWORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxINT64 &a, const __WORD__ &bl)
{
  return a.PackBits() == (__LLWORD__)bl;
}

GXDLCODE_API int operator==(const __WORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al == b.PackBits();
}

GXDLCODE_API int operator==(const gxINT64 &a, const __SWORD__ &bl)
{
  return a.PackBits() == (__LLWORD__)bl;
}

GXDLCODE_API int operator==(const __SWORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxINT64 &a, const __UWORD__ &bl)
{
  return a.PackBits() == (__LLWORD__)bl;
}

GXDLCODE_API int operator==(const __UWORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxINT64 &a, const __USWORD__ &bl)
{
  return  a.PackBits() == (__LLWORD__)bl;
}

GXDLCODE_API int operator==(const __USWORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxINT64 &a, const __SBYTE__ &bl)
{
  return a.PackBits() == (__LLWORD__)bl;
}

GXDLCODE_API int operator==(const __SBYTE__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxINT64 &a, const __UBYTE__ &bl)
{
  return a.PackBits() == (__LLWORD__)bl;
}

GXDLCODE_API int operator==(const __UBYTE__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al == b.PackBits(); 
}

GXDLCODE_API int operator!=(const gxINT64 &a, const gxINT64 &b)
{
  return a.PackBits() != b.PackBits();
}

GXDLCODE_API int operator!=(const gxINT64 &a, const __LLWORD__ &bl)
{
  return a.PackBits() != bl;
}

GXDLCODE_API int operator!=(const __LLWORD__ &al, const gxINT64 &b)
{
  return al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxINT64 &a, const __ULLWORD__ &bl)
{
  return a.PackBits() != (__LLWORD__)bl;
}

GXDLCODE_API int operator!=(const __ULLWORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxINT64 &a, const __LWORD__ &bl)
{
  return a.PackBits() != bl;
}

GXDLCODE_API int operator!=(const __LWORD__ &al, const gxINT64 &b)
{
  return al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxINT64 &a, const __ULWORD__ &bl)
{
  return a.PackBits() != (__LLWORD__)bl;
}

GXDLCODE_API int operator!=(const __ULWORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxINT64 &a, const __WORD__ &bl)
{
  return a.PackBits() != (__LLWORD__)bl;
}

GXDLCODE_API int operator!=(const __WORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxINT64 &a, const __SWORD__ &bl)
{
  return a.PackBits() != (__LLWORD__)bl;
}

GXDLCODE_API int operator!=(const __SWORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxINT64 &a, const __UWORD__ &bl)
{
  return a.PackBits() != (__LLWORD__)bl;
}

GXDLCODE_API int operator!=(const __UWORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxINT64 &a, const __USWORD__ &bl)
{
  return a.PackBits() != (__LLWORD__)bl;
}

GXDLCODE_API int operator!=(const __USWORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxINT64 &a, const __SBYTE__ &bl)
{
  return a.PackBits() != (__LLWORD__)bl;
}

GXDLCODE_API int operator!=(const __SBYTE__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxINT64 &a, const __UBYTE__ &bl)
{
  return a.PackBits() != (__LLWORD__)bl;
}

GXDLCODE_API int operator!=(const __UBYTE__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al != b.PackBits();
}

GXDLCODE_API int operator<(const gxINT64 &a, const gxINT64 &b)
{
  return a.PackBits() < b.PackBits();
}

GXDLCODE_API int operator<(const gxINT64 &a, const __LWORD__ &bl)
{
  return a.PackBits() < bl;
}

GXDLCODE_API int operator<(const gxINT64 &a, const __LLWORD__ &bl)
{
  return a.PackBits() < bl;
}

GXDLCODE_API int operator<(const __LLWORD__ &al, const gxINT64 &b)
{
  return al < b.PackBits();
}

GXDLCODE_API int operator<(const gxINT64 &a, const __ULLWORD__ &bl)
{
  return a.PackBits() < (__LLWORD__)bl;
}

GXDLCODE_API int operator<(const __ULLWORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<(const __LWORD__ &al, const gxINT64 &b)
{
  return al < b.PackBits();
}

GXDLCODE_API int operator<(const gxINT64 &a, const __ULWORD__ &bl)
{
  return a.PackBits() < (__LLWORD__)bl;
}

GXDLCODE_API int operator<(const __ULWORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<(const gxINT64 &a, const __WORD__ &bl)
{
  return a.PackBits() < (__LLWORD__)bl;
}

GXDLCODE_API int operator<(const __WORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<(const gxINT64 &a, const __SWORD__ &bl)
{
  return a.PackBits() < (__LLWORD__)bl;
}

GXDLCODE_API int operator<(const __SWORD__ &al, const gxINT64 &b)
{
  return al < b.PackBits();
}

GXDLCODE_API int operator<(const gxINT64 &a, const __UWORD__ &bl)
{
  return a.PackBits() < (__LLWORD__)bl;
}

GXDLCODE_API int operator<(const __UWORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<(const gxINT64 &a, const __USWORD__ &bl)
{
  return a.PackBits() < (__LLWORD__)bl;
}

GXDLCODE_API int operator<(const __USWORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<(const gxINT64 &a, const __SBYTE__ &bl)
{
  return a.PackBits() < (__LLWORD__)bl;
}

GXDLCODE_API int operator<(const __SBYTE__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<(const gxINT64 &a, const __UBYTE__ &bl)
{
  return a.PackBits() < (__LLWORD__)bl;
}

GXDLCODE_API int operator<(const __UBYTE__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al < b.PackBits();
}

GXDLCODE_API int operator>(const gxINT64 &a, const gxINT64 &b)
{
  return a.PackBits() > b.PackBits();
}

GXDLCODE_API int operator>(const gxINT64 &a, const __LLWORD__ &bl)
{
  return a.PackBits() > bl;
}

GXDLCODE_API int operator>(const __LLWORD__ &al, const gxINT64 &b)
{
  return al > b.PackBits();
}

GXDLCODE_API int operator>(const gxINT64 &a, const __ULLWORD__ &bl)
{
  return a.PackBits() > (__LLWORD__)bl;
}

GXDLCODE_API int operator>(const __ULLWORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>(const gxINT64 &a, const __LWORD__ &bl)
{
  return a.PackBits() > bl;
}

GXDLCODE_API int operator>(const __LWORD__ &al, const gxINT64 &b)
{
  return al > b.PackBits();
}

GXDLCODE_API int operator>(const gxINT64 &a, const __ULWORD__ &bl)
{
  return a.PackBits() > (__LLWORD__)bl;
}

GXDLCODE_API int operator>(const __ULWORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>(const gxINT64 &a, const __WORD__ &bl)
{
  return a.PackBits() > (__LLWORD__)bl;
}

GXDLCODE_API int operator>(const __WORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>(const gxINT64 &a, const __SWORD__ &bl)
{
  return a.PackBits() > (__LLWORD__)bl;
}

GXDLCODE_API int operator>(const __SWORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>(const gxINT64 &a, const __UWORD__ &bl)
{
  return a.PackBits() > (__LLWORD__)bl;
}

GXDLCODE_API int operator>(const __UWORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>(const gxINT64 &a, const __USWORD__ &bl)
{
  return a.PackBits() > (__LLWORD__)bl;
}

GXDLCODE_API int operator>(const __USWORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>(const gxINT64 &a, const __SBYTE__ &bl)
{
  return a.PackBits() > (__LLWORD__)bl;
}

GXDLCODE_API int operator>(const __SBYTE__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>(const gxINT64 &a, const __UBYTE__ &bl)
{
  return a.PackBits() > (__LLWORD__)bl;
}

GXDLCODE_API int operator>(const __UBYTE__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al > b.PackBits();
}

GXDLCODE_API int operator<=(const gxINT64 &a, const gxINT64 &b)
{
  return a.PackBits() == b.PackBits() || a.PackBits() < b.PackBits();
}

GXDLCODE_API int operator<=(const gxINT64 &a, const __LLWORD__ &bl)
{
  return a.PackBits() == bl || a.PackBits() < bl;
}

GXDLCODE_API int operator<=(const __LLWORD__ &al, const gxINT64 &b)
{
  return al == b.PackBits() || al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxINT64 &a, const __ULLWORD__ &bl)
{
  return a.PackBits() == (__LLWORD__)bl || a.PackBits() < (__LLWORD__)bl;
}

GXDLCODE_API int operator<=(const __ULLWORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al == b.PackBits() || (__LLWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxINT64 &a, const __LWORD__ &bl)
{
  return a.PackBits() == bl || a.PackBits() < bl;
}

GXDLCODE_API int operator<=(const __LWORD__ &al, const gxINT64 &b)
{
  return al == b.PackBits() || al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxINT64 &a, const __ULWORD__ &bl)
{
  return a.PackBits() == (__LLWORD__)bl || a.PackBits() < (__LLWORD__)bl;
}

GXDLCODE_API int operator<=(const __ULWORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al == b.PackBits() || (__LLWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxINT64 &a, const __WORD__ &bl)
{
  return a.PackBits() == (__LLWORD__)bl || a.PackBits() < (__LLWORD__)bl;
}

GXDLCODE_API int operator<=(const __WORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al == b.PackBits() || (__LLWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxINT64 &a, const __SWORD__ &bl)
{
  return a.PackBits() == (__LLWORD__)bl || a.PackBits() < (__LLWORD__)bl;
}

GXDLCODE_API int operator<=(const __SWORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al == b.PackBits() || (__LLWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxINT64 &a, const __UWORD__ &bl)
{
  return a.PackBits() == (__LLWORD__)bl || a.PackBits() < (__LLWORD__)bl;
}

GXDLCODE_API int operator<=(const __UWORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al == b.PackBits() || (__LLWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxINT64 &a, const __USWORD__ &bl)
{
  return a.PackBits() == (__LLWORD__)bl || a.PackBits() < (__LLWORD__)bl;
}

GXDLCODE_API int operator<=(const __USWORD__ &al, const gxINT64 &b)
{
  return al == b.PackBits() || al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxINT64 &a, const __SBYTE__ &bl)
{
  return a.PackBits() == (__LLWORD__)bl ||  a.PackBits() < (__LLWORD__)bl;
}

GXDLCODE_API int operator<=(const __SBYTE__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al == b.PackBits() || (__LLWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxINT64 &a, const __UBYTE__ &bl)
{
  return a.PackBits() == (__LLWORD__)bl ||  a.PackBits() < (__LLWORD__)bl;
}

GXDLCODE_API int operator<=(const __UBYTE__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al == b.PackBits() || (__LLWORD__)al < b.PackBits();
}

GXDLCODE_API int operator>=(const gxINT64 &a, const gxINT64 &b)
{
  return a.PackBits() == b.PackBits() || a.PackBits() > b.PackBits();
}

GXDLCODE_API int operator>=(const gxINT64 &a, const __LWORD__ &bl)
{
  return a.PackBits() == bl || a.PackBits() > bl;
}

GXDLCODE_API int operator>=(const gxINT64 &a, const __LLWORD__ &bl)
{
  return a.PackBits() == bl || a.PackBits() > bl;
}

GXDLCODE_API int operator>=(const __LLWORD__ &al, const gxINT64 &b)
{
  return al == b.PackBits() || al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxINT64 &a, const __ULLWORD__ &bl)
{
  return a.PackBits() == (__LLWORD__)bl || a.PackBits() > (__LLWORD__)bl;
}

GXDLCODE_API int operator>=(const __ULLWORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al == b.PackBits() || (__LLWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>=(const __LWORD__ &al, const gxINT64 &b)
{
  return al == b.PackBits() || al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxINT64 &a, const __ULWORD__ &bl)
{
  return a.PackBits() == (__LLWORD__)bl || a.PackBits() > (__LLWORD__)bl;
}

GXDLCODE_API int operator>=(const __ULWORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al == b.PackBits() || (__LLWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxINT64 &a, const __WORD__ &bl)
{
  return a.PackBits() == (__LLWORD__)bl || a.PackBits() > (__LLWORD__)bl;
}

GXDLCODE_API int operator>=(const __WORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al == b.PackBits() || (__LLWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxINT64 &a, const __SWORD__ &bl)
{
  return a.PackBits() == (__LLWORD__)bl || a.PackBits() > (__LLWORD__)bl;
}

GXDLCODE_API int operator>=(const __SWORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al == b.PackBits() || (__LLWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxINT64 &a, const __UWORD__ &bl)
{
  return a.PackBits() == (__LLWORD__)bl || a.PackBits() > (__LLWORD__)bl;
}

GXDLCODE_API int operator>=(const __UWORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al == b.PackBits() || (__LLWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxINT64 &a, const __USWORD__ &bl)
{
  return a.PackBits() == (__LLWORD__)bl || a.PackBits() > (__LLWORD__)bl;
}

GXDLCODE_API int operator>=(const __USWORD__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al == b.PackBits() || (__LLWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxINT64 &a, const __SBYTE__ &bl)
{
  return a.PackBits() == (__LLWORD__)bl || a.PackBits() > (__LLWORD__)bl;
}

GXDLCODE_API int operator>=(const __SBYTE__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al == b.PackBits() || (__LLWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxINT64 &a, const __UBYTE__ &bl)
{
  return a.PackBits() == (__LLWORD__)bl || a.PackBits() > (__LLWORD__)bl;
}

GXDLCODE_API int operator>=(const __UBYTE__ &al, const gxINT64 &b)
{
  return (__LLWORD__)al == b.PackBits() || (__LLWORD__)al > b.PackBits();
}

char *gxINT64::c_str(char *sbuf, int radix)
// Returns a null terminated string representing this object's
// 64-bit value. NOTE: This function assumes that the proper
// amount of memory has been allocated for the "sbuf" string
// buffer. The string buffer "sbuf" is redundantly returned by
// this function.
{
  __ULLWORD__ val = (__ULLWORD__)PackBits();
  return LLWORDToString(sbuf, val, radix);
}

char *gxINT64::c_str(char *sbuf, int radix) const
// Returns a null terminated string representing this object's
// 64-bit value. NOTE: This function assumes that the proper
// amount of memory has been allocated for the "sbuf" string
// buffer. The string buffer "sbuf" is redundantly returned by
// this function.
{
  __ULLWORD__ val = (__ULLWORD__)PackBits();
  return LLWORDToString(sbuf, val, radix);
}

gxINT64 &gxINT64::StringToINT64(const char *str)
{
  *this = StringToLLWORD(str);
  return *this;
}

gxINT64 &gxINT64::StringToINT64(char *str)
{
  *this = StringToLLWORD((const char *)str);
  return *this;
}

#if defined (__USE_CPP_IOSTREAM__)
GXDLCODE_API GXSTD::ostream &operator<<(GXSTD::ostream &os, 
					const gxINT64 &val)
{
  char sbuf[64];
  return os << val.c_str(sbuf);
}

GXDLCODE_API GXSTD::istream &operator>>(GXSTD::istream &os, gxINT64 &val) 
{
  char sbuf[25];
  for(unsigned i = 0; i < 25; i++) sbuf[i] = 0;
  os >> GXSTD::setw(25) >> sbuf;
  val.StringToINT64(sbuf);
  return os;
}
#endif // __USE_CPP_IOSTREAM__

#endif // __USE_NATIVE_INT_TYPES__ 

#endif // __64_BIT_DATABASE_ENGINE__ || __ENABLE_64_BIT_INTEGERS__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
