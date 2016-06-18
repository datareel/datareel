// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxint32.cpp
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

The gxINT32 class is used to represent 32 bit signed integers
independently of the operating system or hardware platform used.
It works by separating a 32-bit value into four separate byte
values and reordering the bytes lowest-order to highest-order.
An gxINT32 type has a base 10 positive limit of 2,147,483,647 and
a negative limit of 2,147,483,648.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "gxint32.h"

#ifndef __USE_NATIVE_INT_TYPES__ 

#include <string.h>
#include <memory.h>

gxINT32::gxINT32(__LWORD__ val)
{
  UnPackBits(val);
}

gxINT32::gxINT32(const gxINT32& ob)
{
  memmove((void *)byte, (const void *)ob.byte, 4);
}

gxINT32& gxINT32::operator=(const gxINT32& ob)
{
  if(this != &ob) { // PC-lint 05/25/2005: Prevent self assignment 
    memmove((void *)byte, (const void *)ob.byte, 4);
  }
  return *this;
}

gxINT32& gxINT32::operator=(const __LWORD__ val)
{
  UnPackBits(val);
  return *this;
}

gxINT32::operator __LWORD__() const
{
  return PackBits();
}

__LWORD__ gxINT32::PackBits() const
{
  __LWORD__ a, b, c, d;
  
  a = (__LWORD__)byte[0];
  b = (__LWORD__)byte[1];
  c = (__LWORD__)byte[2];
  d = (__LWORD__)byte[3];

  a = a & 0xFF;
  b = (b<<8) & 0xFF00;
  c = (c<<16) & 0xFF0000;
  d = (d<<24) & 0xFF000000;

  return a + b + c + d;
}

void gxINT32::UnPackBits(__LWORD__ val)
{
  byte[0] = (__SBYTE__)(val & 0xFF);
  byte[1] = (__SBYTE__)((val & 0xFF00)>>8);
  byte[2] = (__SBYTE__)((val & 0xFF0000)>>16);
  byte[3] = (__SBYTE__)((val & 0xFF000000)>>24);
}

gxINT32 gxINT32::operator++(int) // Postfix
{
  gxINT32 val_before(*this); 
  operator=(*this + 1);
  return val_before;
}
 
gxINT32 gxINT32::operator--(int) // Postfix
{
  gxINT32 val_before(*this); 
  operator=(*this - 1);
  return val_before;
}

void gxINT32::operator/=(const gxINT32 &i)
{
  if(i == (gxINT32)0) return; // Divide by zero error
  operator=(*this / i);
}

void gxINT32::operator/=(const __LWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / i);
}

void gxINT32::operator/=(const __ULWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__LWORD__)i);
}

void gxINT32::operator/=(const __WORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__LWORD__)i);
}

void gxINT32::operator/=(const __SWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__LWORD__)i);
}

void gxINT32::operator/=(const __UWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__LWORD__)i);
}

void gxINT32::operator/=(const __USWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__LWORD__)i);
}

void gxINT32::operator/=(const __SBYTE__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__LWORD__)i);
}

void gxINT32::operator/=(const __UBYTE__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__LWORD__)i);
}

GXDLCODE_API int operator==(const gxINT32 &a, const gxINT32 &b)
{
  return a.PackBits() == b.PackBits();
}

GXDLCODE_API int operator==(const gxINT32 &a, const __LWORD__ &bl)
{
  return a.PackBits() == bl;
}

GXDLCODE_API int operator==(const __LWORD__ &al, const gxINT32 &b)
{
  return al == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxINT32 &a, const __ULWORD__ &bl)
{
  return a.PackBits() == (__LWORD__)bl;
}

GXDLCODE_API int operator==(const __ULWORD__ &al, const gxINT32 &b)
{
  return (__LWORD__)al == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxINT32 &a, const __WORD__ &bl)
{
  return a.PackBits() == (__LWORD__)bl;
}

GXDLCODE_API int operator==(const __WORD__ &al, const gxINT32 &b)
{
  return (__LWORD__)al == b.PackBits();
}

GXDLCODE_API int operator==(const gxINT32 &a, const __SWORD__ &bl)
{
  return a.PackBits() == (__LWORD__)bl;
}

GXDLCODE_API int operator==(const __SWORD__ &al, const gxINT32 &b)
{
  return (__LWORD__)al == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxINT32 &a, const __UWORD__ &bl)
{
  return a.PackBits() == (__LWORD__)bl;
}

GXDLCODE_API int operator==(const __UWORD__ &al, const gxINT32 &b)
{
  return (__LWORD__)al == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxINT32 &a, const __USWORD__ &bl)
{
  return  a.PackBits() == (__LWORD__)bl;
}

GXDLCODE_API int operator==(const __USWORD__ &al, const gxINT32 &b)
{
  return (__LWORD__)al == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxINT32 &a, const __SBYTE__ &bl)
{
  return a.PackBits() == (__LWORD__)bl;
}

GXDLCODE_API int operator==(const __SBYTE__ &al, const gxINT32 &b)
{
  return (__LWORD__)al == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxINT32 &a, const __UBYTE__ &bl)
{
  return a.PackBits() == (__LWORD__)bl;
}

GXDLCODE_API int operator==(const __UBYTE__ &al, const gxINT32 &b)
{
  return (__LWORD__)al == b.PackBits(); 
}

GXDLCODE_API int operator!=(const gxINT32 &a, const gxINT32 &b)
{
  return a.PackBits() != b.PackBits();
}

GXDLCODE_API int operator!=(const gxINT32 &a, const __LWORD__ &bl)
{
  return a.PackBits() != bl;
}

GXDLCODE_API int operator!=(const __LWORD__ &al, const gxINT32 &b)
{
  return al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxINT32 &a, const __ULWORD__ &bl)
{
  return a.PackBits() != (__LWORD__)bl;
}

GXDLCODE_API int operator!=(const __ULWORD__ &al, const gxINT32 &b)
{
  return (__LWORD__)al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxINT32 &a, const __WORD__ &bl)
{
  return a.PackBits() != (__LWORD__)bl;
}

GXDLCODE_API int operator!=(const __WORD__ &al, const gxINT32 &b)
{
  return (__LWORD__)al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxINT32 &a, const __SWORD__ &bl)
{
  return a.PackBits() != (__LWORD__)bl;
}

GXDLCODE_API int operator!=(const __SWORD__ &al, const gxINT32 &b)
{
  return (__LWORD__)al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxINT32 &a, const __UWORD__ &bl)
{
  return a.PackBits() != (__LWORD__)bl;
}

GXDLCODE_API int operator!=(const __UWORD__ &al, const gxINT32 &b)
{
  return (__LWORD__)al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxINT32 &a, const __USWORD__ &bl)
{
  return a.PackBits() != (__LWORD__)bl;
}

GXDLCODE_API int operator!=(const __USWORD__ &al, const gxINT32 &b)
{
  return (__LWORD__)al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxINT32 &a, const __SBYTE__ &bl)
{
  return a.PackBits() != (__LWORD__)bl;
}

GXDLCODE_API int operator!=(const __SBYTE__ &al, const gxINT32 &b)
{
  return (__LWORD__)al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxINT32 &a, const __UBYTE__ &bl)
{
  return a.PackBits() != (__LWORD__)bl;
}

GXDLCODE_API int operator!=(const __UBYTE__ &al, const gxINT32 &b)
{
  return (__LWORD__)al != b.PackBits();
}

GXDLCODE_API int operator<(const gxINT32 &a, const gxINT32 &b)
{
  return a.PackBits() < b.PackBits();
}

GXDLCODE_API int operator<(const gxINT32 &a, const __LWORD__ &bl)
{
  return a.PackBits() < bl;
}

GXDLCODE_API int operator<(const __LWORD__ &al, const gxINT32 &b)
{
  return al < b.PackBits();
}

GXDLCODE_API int operator<(const gxINT32 &a, const __ULWORD__ &bl)
{
  return a.PackBits() < (__LWORD__)bl;
}

GXDLCODE_API int operator<(const __ULWORD__ &al, const gxINT32 &b)
{
  return (__LWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<(const gxINT32 &a, const __WORD__ &bl)
{
  return a.PackBits() < (__LWORD__)bl;
}

GXDLCODE_API int operator<(const __WORD__ &al, const gxINT32 &b)
{
  return (__LWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<(const gxINT32 &a, const __SWORD__ &bl)
{
  return a.PackBits() < (__LWORD__)bl;
}

GXDLCODE_API int operator<(const __SWORD__ &al, const gxINT32 &b)
{
  return al < b.PackBits();
}

GXDLCODE_API int operator<(const gxINT32 &a, const __UWORD__ &bl)
{
  return a.PackBits() < (__LWORD__)bl;
}

GXDLCODE_API int operator<(const __UWORD__ &al, const gxINT32 &b)
{
  return (__LWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<(const gxINT32 &a, const __USWORD__ &bl)
{
  return a.PackBits() < (__LWORD__)bl;
}

GXDLCODE_API int operator<(const __USWORD__ &al, const gxINT32 &b)
{
  return (__LWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<(const gxINT32 &a, const __SBYTE__ &bl)
{
  return a.PackBits() < (__LWORD__)bl;
}

GXDLCODE_API int operator<(const __SBYTE__ &al, const gxINT32 &b)
{
  return (__LWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<(const gxINT32 &a, const __UBYTE__ &bl)
{
  return a.PackBits() < (__LWORD__)bl;
}

GXDLCODE_API int operator<(const __UBYTE__ &al, const gxINT32 &b)
{
  return (__LWORD__)al < b.PackBits();
}

GXDLCODE_API int operator>(const gxINT32 &a, const gxINT32 &b)
{
  return a.PackBits() > b.PackBits();
}

GXDLCODE_API int operator>(const gxINT32 &a, const __LWORD__ &bl)
{
  return a.PackBits() > bl;
}

GXDLCODE_API int operator>(const __LWORD__ &al, const gxINT32 &b)
{
  return al > b.PackBits();
}

GXDLCODE_API int operator>(const gxINT32 &a, const __ULWORD__ &bl)
{
  return a.PackBits() > (__LWORD__)bl;
}

GXDLCODE_API int operator>(const __ULWORD__ &al, const gxINT32 &b)
{
  return (__LWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>(const gxINT32 &a, const __WORD__ &bl)
{
  return a.PackBits() > (__LWORD__)bl;
}

GXDLCODE_API int operator>(const __WORD__ &al, const gxINT32 &b)
{
  return (__LWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>(const gxINT32 &a, const __SWORD__ &bl)
{
  return a.PackBits() > (__LWORD__)bl;
}

GXDLCODE_API int operator>(const __SWORD__ &al, const gxINT32 &b)
{
  return (__LWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>(const gxINT32 &a, const __UWORD__ &bl)
{
  return a.PackBits() > (__LWORD__)bl;
}

GXDLCODE_API int operator>(const __UWORD__ &al, const gxINT32 &b)
{
  return (__LWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>(const gxINT32 &a, const __USWORD__ &bl)
{
  return a.PackBits() > (__LWORD__)bl;
}

GXDLCODE_API int operator>(const __USWORD__ &al, const gxINT32 &b)
{
  return (__LWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>(const gxINT32 &a, const __SBYTE__ &bl)
{
  return a.PackBits() > (__LWORD__)bl;
}

GXDLCODE_API int operator>(const __SBYTE__ &al, const gxINT32 &b)
{
  return (__LWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>(const gxINT32 &a, const __UBYTE__ &bl)
{
  return a.PackBits() > (__LWORD__)bl;
}

GXDLCODE_API int operator>(const __UBYTE__ &al, const gxINT32 &b)
{
  return (__LWORD__)al > b.PackBits();
}

GXDLCODE_API int operator<=(const gxINT32 &a, const gxINT32 &b)
{
  return a.PackBits() == b.PackBits() || a.PackBits() < b.PackBits();
}

GXDLCODE_API int operator<=(const gxINT32 &a, const __LWORD__ &bl)
{
  return a.PackBits() == bl || a.PackBits() < bl;
}

GXDLCODE_API int operator<=(const __LWORD__ &al, const gxINT32 &b)
{
  return al == b.PackBits() || al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxINT32 &a, const __ULWORD__ &bl)
{
  return a.PackBits() == (__LWORD__)bl || a.PackBits() < (__LWORD__)bl;
}

GXDLCODE_API int operator<=(const __ULWORD__ &al, const gxINT32 &b)
{
  return (__LWORD__)al == b.PackBits() || (__LWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxINT32 &a, const __WORD__ &bl)
{
  return a.PackBits() == (__LWORD__)bl || a.PackBits() < (__LWORD__)bl;
}

GXDLCODE_API int operator<=(const __WORD__ &al, const gxINT32 &b)
{
  return (__LWORD__)al == b.PackBits() || (__LWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxINT32 &a, const __SWORD__ &bl)
{
  return a.PackBits() == (__LWORD__)bl || a.PackBits() < (__LWORD__)bl;
}

GXDLCODE_API int operator<=(const __SWORD__ &al, const gxINT32 &b)
{
  return (__LWORD__)al == b.PackBits() || (__LWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxINT32 &a, const __UWORD__ &bl)
{
  return a.PackBits() == (__LWORD__)bl || a.PackBits() < (__LWORD__)bl;
}

GXDLCODE_API int operator<=(const __UWORD__ &al, const gxINT32 &b)
{
  return (__LWORD__)al == b.PackBits() || (__LWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxINT32 &a, const __USWORD__ &bl)
{
  return a.PackBits() == (__LWORD__)bl || a.PackBits() < (__LWORD__)bl;
}

GXDLCODE_API int operator<=(const __USWORD__ &al, const gxINT32 &b)
{
  return al == b.PackBits() || al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxINT32 &a, const __SBYTE__ &bl)
{
  return a.PackBits() == (__LWORD__)bl ||  a.PackBits() < (__LWORD__)bl;
}

GXDLCODE_API int operator<=(const __SBYTE__ &al, const gxINT32 &b)
{
  return (__LWORD__)al == b.PackBits() || (__LWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxINT32 &a, const __UBYTE__ &bl)
{
  return a.PackBits() == (__LWORD__)bl ||  a.PackBits() < (__LWORD__)bl;
}

GXDLCODE_API int operator<=(const __UBYTE__ &al, const gxINT32 &b)
{
  return (__LWORD__)al == b.PackBits() || (__LWORD__)al < b.PackBits();
}

GXDLCODE_API int operator>=(const gxINT32 &a, const gxINT32 &b)
{
  return a.PackBits() == b.PackBits() || a.PackBits() > b.PackBits();
}

GXDLCODE_API int operator>=(const gxINT32 &a, const __LWORD__ &bl)
{
  return a.PackBits() == bl || a.PackBits() > bl;
}

GXDLCODE_API int operator>=(const __LWORD__ &al, const gxINT32 &b)
{
  return al == b.PackBits() || al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxINT32 &a, const __ULWORD__ &bl)
{
  return a.PackBits() == (__LWORD__)bl || a.PackBits() > (__LWORD__)bl;
}

GXDLCODE_API int operator>=(const __ULWORD__ &al, const gxINT32 &b)
{
  return (__LWORD__)al == b.PackBits() || (__LWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxINT32 &a, const __WORD__ &bl)
{
  return a.PackBits() == (__LWORD__)bl || a.PackBits() > (__LWORD__)bl;
}

GXDLCODE_API int operator>=(const __WORD__ &al, const gxINT32 &b)
{
  return (__LWORD__)al == b.PackBits() || (__LWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxINT32 &a, const __SWORD__ &bl)
{
  return a.PackBits() == (__LWORD__)bl || a.PackBits() > (__LWORD__)bl;
}

GXDLCODE_API int operator>=(const __SWORD__ &al, const gxINT32 &b)
{
  return (__LWORD__)al == b.PackBits() || (__LWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxINT32 &a, const __UWORD__ &bl)
{
  return a.PackBits() == (__LWORD__)bl || a.PackBits() > (__LWORD__)bl;
}

GXDLCODE_API int operator>=(const __UWORD__ &al, const gxINT32 &b)
{
  return (__LWORD__)al == b.PackBits() || (__LWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxINT32 &a, const __USWORD__ &bl)
{
  return a.PackBits() == (__LWORD__)bl || a.PackBits() > (__LWORD__)bl;
}

GXDLCODE_API int operator>=(const __USWORD__ &al, const gxINT32 &b)
{
  return (__LWORD__)al == b.PackBits() || (__LWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxINT32 &a, const __SBYTE__ &bl)
{
  return a.PackBits() == (__LWORD__)bl || a.PackBits() > (__LWORD__)bl;
}

GXDLCODE_API int operator>=(const __SBYTE__ &al, const gxINT32 &b)
{
  return (__LWORD__)al == b.PackBits() || (__LWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxINT32 &a, const __UBYTE__ &bl)
{
  return a.PackBits() == (__LWORD__)bl || a.PackBits() > (__LWORD__)bl;
}

GXDLCODE_API int operator>=(const __UBYTE__ &al, const gxINT32 &b)
{
  return (__LWORD__)al == b.PackBits() || (__LWORD__)al > b.PackBits();
}

#endif // __USE_NATIVE_INT_TYPES__ 
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
