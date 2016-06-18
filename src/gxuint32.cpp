// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxuint32.cpp
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

The gxUINT32 class is used to represent 32 bit unsigned integers
independently of the operating system or hardware platform used.
It works by separating a 32-bit value into four separate byte
values and reordering the bytes lowest-order to highest-order.
An gxUINT32 type has a base 10 limit of 4,294,967,295.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "gxuint32.h"

#ifndef __USE_NATIVE_INT_TYPES__

#include <string.h>
#include <memory.h>

gxUINT32::gxUINT32(__ULWORD__ val)
{
  UnPackBits(val);
}

gxUINT32::gxUINT32(const gxUINT32& ob)
{
  memmove((void *)byte, (const void *)ob.byte, 4);
}

gxUINT32& gxUINT32::operator=(const gxUINT32& ob)
{
  if(this != &ob) { // PC-lint 05/25/2005: Prevent self assignment 
    memmove((void *)byte, (const void *)ob.byte, 4);
  }
  return *this;
}

gxUINT32& gxUINT32::operator=(const __ULWORD__ val)
{
  UnPackBits(val);
  return *this;
}

gxUINT32::operator __ULWORD__() const
{
  return PackBits();
}

__ULWORD__ gxUINT32::PackBits() const
{
  __ULWORD__ a, b, c, d;
  
  a = (__ULWORD__)byte[0];
  b = (__ULWORD__)byte[1];
  c = (__ULWORD__)byte[2];
  d = (__ULWORD__)byte[3];

  a = a & 0xFF;
  b = (b<<8) & 0xFF00;
  c = (c<<16) & 0xFF0000;
  d = (d<<24) & 0xFF000000;

  return a + b + c + d;
}

void gxUINT32::UnPackBits(__ULWORD__ val)
{
  byte[0] = (__UBYTE__)(val & 0xFF);
  byte[1] = (__UBYTE__)((val & 0xFF00)>>8);
  byte[2] = (__UBYTE__)((val & 0xFF0000)>>16);
  byte[3] = (__UBYTE__)((val & 0xFF000000)>>24);
}

gxUINT32 gxUINT32::operator++(int) // Postfix
{
  gxUINT32 val_before(*this); 
  operator=(*this + 1);
  return val_before;
}
 
gxUINT32 gxUINT32::operator--(int) // Postfix
{
  gxUINT32 val_before(*this); 
  operator=(*this - 1);
  return val_before;
}

void gxUINT32::operator/=(const gxUINT32 &i)
{
  if(i == (gxUINT32)0) return; // Divide by zero error
  operator=(*this / i);
}

void gxUINT32::operator/=(const __LWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__ULWORD__)i);
}

void gxUINT32::operator/=(const __ULWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / i);
}

void gxUINT32::operator/=(const __WORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__ULWORD__)i);
}

void gxUINT32::operator/=(const __SWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__ULWORD__)i); // PC-lint 09/15/2005: Ignore cast warning
}

void gxUINT32::operator/=(const __UWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__ULWORD__)i);
}

void gxUINT32::operator/=(const __USWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__ULWORD__)i);
}

void gxUINT32::operator/=(const __SBYTE__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__ULWORD__)i); // PC-lint 09/15/2005: Ignore cast warning
}

void gxUINT32::operator/=(const __UBYTE__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__ULWORD__)i);
}

GXDLCODE_API int operator==(const gxUINT32 &a, const gxUINT32 &b)
{
  return a.PackBits() == b.PackBits();
}

GXDLCODE_API int operator==(const gxUINT32 &a, const __LWORD__ &bl)
{
  return a.PackBits() == (__ULWORD__)bl;
}

GXDLCODE_API int operator==(const __LWORD__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxUINT32 &a, const __ULWORD__ &bl)
{
  return a.PackBits() == bl;
}

GXDLCODE_API int operator==(const __ULWORD__ &al, const gxUINT32 &b)
{
  return al == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxUINT32 &a, const __WORD__ &bl)
{
  return a.PackBits() == (__ULWORD__)bl;
}

GXDLCODE_API int operator==(const __WORD__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al == b.PackBits();
}

GXDLCODE_API int operator==(const gxUINT32 &a, const __SWORD__ &bl)
{
  return a.PackBits() == (__ULWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator==(const __SWORD__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al == b.PackBits(); // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator==(const gxUINT32 &a, const __UWORD__ &bl)
{
  return a.PackBits() == (__ULWORD__)bl;
}

GXDLCODE_API int operator==(const __UWORD__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxUINT32 &a, const __USWORD__ &bl)
{
  return  a.PackBits() == (__ULWORD__)bl;
}

GXDLCODE_API int operator==(const __USWORD__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxUINT32 &a, const __SBYTE__ &bl)
{
  return a.PackBits() == (__ULWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator==(const __SBYTE__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al == b.PackBits(); // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator==(const gxUINT32 &a, const __UBYTE__ &bl)
{
  return a.PackBits() == (__ULWORD__)bl;
}

GXDLCODE_API int operator==(const __UBYTE__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al == b.PackBits(); 
}

GXDLCODE_API int operator!=(const gxUINT32 &a, const gxUINT32 &b)
{
  return a.PackBits() != b.PackBits();
}

GXDLCODE_API int operator!=(const gxUINT32 &a, const __LWORD__ &bl)
{
  return a.PackBits() != (__ULWORD__)bl;
}

GXDLCODE_API int operator!=(const __LWORD__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxUINT32 &a, const __ULWORD__ &bl)
{
  return a.PackBits() != bl;
}

GXDLCODE_API int operator!=(const __ULWORD__ &al, const gxUINT32 &b)
{
  return al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxUINT32 &a, const __WORD__ &bl)
{
  return a.PackBits() != (__ULWORD__)bl;
}

GXDLCODE_API int operator!=(const __WORD__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxUINT32 &a, const __SWORD__ &bl)
{
  return a.PackBits() != (__ULWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator!=(const __SWORD__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al != b.PackBits();// PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator!=(const gxUINT32 &a, const __UWORD__ &bl)
{
  return a.PackBits() != (__ULWORD__)bl;
}

GXDLCODE_API int operator!=(const __UWORD__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxUINT32 &a, const __USWORD__ &bl)
{
  return a.PackBits() != (__ULWORD__)bl;
}

GXDLCODE_API int operator!=(const __USWORD__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxUINT32 &a, const __SBYTE__ &bl)
{
  return a.PackBits() != (__ULWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator!=(const __SBYTE__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al != b.PackBits(); // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator!=(const gxUINT32 &a, const __UBYTE__ &bl)
{
  return a.PackBits() != (__ULWORD__)bl;
}

GXDLCODE_API int operator!=(const __UBYTE__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al != b.PackBits();
}

GXDLCODE_API int operator<(const gxUINT32 &a, const gxUINT32 &b)
{
  return a.PackBits() < b.PackBits();
}

GXDLCODE_API int operator<(const gxUINT32 &a, const __LWORD__ &bl)
{
  return a.PackBits() < (__ULWORD__)bl;
}

GXDLCODE_API int operator<(const __LWORD__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<(const gxUINT32 &a, const __ULWORD__ &bl)
{
  return a.PackBits() < bl;
}

GXDLCODE_API int operator<(const __ULWORD__ &al, const gxUINT32 &b)
{
  return al < b.PackBits();
}

GXDLCODE_API int operator<(const gxUINT32 &a, const __WORD__ &bl)
{
  return a.PackBits() < (__ULWORD__)bl;
}

GXDLCODE_API int operator<(const __WORD__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<(const gxUINT32 &a, const __SWORD__ &bl)
{
  return a.PackBits() < (__ULWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator<(const __SWORD__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al < b.PackBits(); // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator<(const gxUINT32 &a, const __UWORD__ &bl)
{
  return a.PackBits() < (__ULWORD__)bl;
}

GXDLCODE_API int operator<(const __UWORD__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<(const gxUINT32 &a, const __USWORD__ &bl)
{
  return a.PackBits() < (__ULWORD__)bl;
}

GXDLCODE_API int operator<(const __USWORD__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<(const gxUINT32 &a, const __SBYTE__ &bl)
{
  return a.PackBits() < (__ULWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator<(const __SBYTE__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al < b.PackBits(); // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator<(const gxUINT32 &a, const __UBYTE__ &bl)
{
  return a.PackBits() < (__ULWORD__)bl;
}

GXDLCODE_API int operator<(const __UBYTE__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al < b.PackBits();
}

GXDLCODE_API int operator>(const gxUINT32 &a, const gxUINT32 &b)
{
  return a.PackBits() > b.PackBits();
}

GXDLCODE_API int operator>(const gxUINT32 &a, const __LWORD__ &bl)
{
  return a.PackBits() > (__ULWORD__)bl;
}

GXDLCODE_API int operator>(const __LWORD__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>(const gxUINT32 &a, const __ULWORD__ &bl)
{
  return a.PackBits() > bl;
}

GXDLCODE_API int operator>(const __ULWORD__ &al, const gxUINT32 &b)
{
  return al > b.PackBits();
}

GXDLCODE_API int operator>(const gxUINT32 &a, const __WORD__ &bl)
{
  return a.PackBits() > (__ULWORD__)bl;
}

GXDLCODE_API int operator>(const __WORD__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>(const gxUINT32 &a, const __SWORD__ &bl)
{
  return a.PackBits() > (__ULWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator>(const __SWORD__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al > b.PackBits(); // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator>(const gxUINT32 &a, const __UWORD__ &bl)
{
  return a.PackBits() > (__ULWORD__)bl;
}

GXDLCODE_API int operator>(const __UWORD__ &al, const gxUINT32 &b)
{
  return al > b.PackBits();
}

GXDLCODE_API int operator>(const gxUINT32 &a, const __USWORD__ &bl)
{
  return a.PackBits() > (__ULWORD__)bl;
}

GXDLCODE_API int operator>(const __USWORD__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>(const gxUINT32 &a, const __SBYTE__ &bl)
{
  return a.PackBits() > (__ULWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator>(const __SBYTE__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al > b.PackBits(); // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator>(const gxUINT32 &a, const __UBYTE__ &bl)
{
  return a.PackBits() > (__ULWORD__)bl;
}

GXDLCODE_API int operator>(const __UBYTE__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al > b.PackBits();
}

GXDLCODE_API int operator<=(const gxUINT32 &a, const gxUINT32 &b)
{
  return a.PackBits() == b.PackBits() || a.PackBits() < b.PackBits();
}

GXDLCODE_API int operator<=(const gxUINT32 &a, const __LWORD__ &bl)
{
  return a.PackBits() == (__ULWORD__)bl || a.PackBits() < (__ULWORD__)bl;
}

GXDLCODE_API int operator<=(const __LWORD__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al == b.PackBits() || (__ULWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxUINT32 &a, const __ULWORD__ &bl)
{
  return a.PackBits() == bl || a.PackBits() < bl;
}

GXDLCODE_API int operator<=(const __ULWORD__ &al, const gxUINT32 &b)
{
  return al == b.PackBits() || al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxUINT32 &a, const __WORD__ &bl)
{
  return a.PackBits() == (__ULWORD__)bl || a.PackBits() < (__ULWORD__)bl;
}

GXDLCODE_API int operator<=(const __WORD__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al == b.PackBits() || (__ULWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxUINT32 &a, const __SWORD__ &bl)
{
  return a.PackBits() == (__ULWORD__)bl || a.PackBits() < (__ULWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator<=(const __SWORD__ &al, const gxUINT32 &b) 
{
  return (__ULWORD__)al == b.PackBits() || (__ULWORD__)al < b.PackBits(); // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator<=(const gxUINT32 &a, const __UWORD__ &bl) 
{
  return a.PackBits() == (__ULWORD__)bl || a.PackBits() < (__ULWORD__)bl;
}

GXDLCODE_API int operator<=(const __UWORD__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al == b.PackBits() || (__ULWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxUINT32 &a, const __USWORD__ &bl)
{
  return a.PackBits() == (__ULWORD__)bl || a.PackBits() < (__ULWORD__)bl;
}

GXDLCODE_API int operator<=(const __USWORD__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al == b.PackBits() || (__ULWORD__)al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxUINT32 &a, const __SBYTE__ &bl)
{
  return a.PackBits() == (__ULWORD__)bl ||  a.PackBits() < (__ULWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator<=(const __SBYTE__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al == b.PackBits() || (__ULWORD__)al < b.PackBits(); // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator<=(const gxUINT32 &a, const __UBYTE__ &bl)
{
  return a.PackBits() == (__ULWORD__)bl ||  a.PackBits() < (__ULWORD__)bl;
}

GXDLCODE_API int operator<=(const __UBYTE__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al == b.PackBits() || (__ULWORD__)al < b.PackBits();
}

GXDLCODE_API int operator>=(const gxUINT32 &a, const gxUINT32 &b)
{
  return a.PackBits() == b.PackBits() || a.PackBits() > b.PackBits();
}

GXDLCODE_API int operator>=(const gxUINT32 &a, const __LWORD__ &bl)
{
  return a.PackBits() == (__ULWORD__)bl || a.PackBits() > (__ULWORD__)bl;
}

GXDLCODE_API int operator>=(const __LWORD__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al == b.PackBits() || (__ULWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxUINT32 &a, const __ULWORD__ &bl)
{
  return a.PackBits() == bl || a.PackBits() > bl;
}

GXDLCODE_API int operator>=(const __ULWORD__ &al, const gxUINT32 &b)
{
  return al == b.PackBits() || al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxUINT32 &a, const __WORD__ &bl)
{
  return a.PackBits() == (__ULWORD__)bl || a.PackBits() > (__ULWORD__)bl;
}

GXDLCODE_API int operator>=(const __WORD__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al == b.PackBits() || (__ULWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxUINT32 &a, const __SWORD__ &bl)
{
  return a.PackBits() == (__ULWORD__)bl || a.PackBits() > (__ULWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator>=(const __SWORD__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al == b.PackBits() || (__ULWORD__)al > b.PackBits(); // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator>=(const gxUINT32 &a, const __UWORD__ &bl)
{
  return a.PackBits() == (__ULWORD__)bl || a.PackBits() > (__ULWORD__)bl;
}

GXDLCODE_API int operator>=(const __UWORD__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al == b.PackBits() || (__ULWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxUINT32 &a, const __USWORD__ &bl)
{
  return a.PackBits() == (__ULWORD__)bl || a.PackBits() > (__ULWORD__)bl;
}

GXDLCODE_API int operator>=(const __USWORD__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al == b.PackBits() || (__ULWORD__)al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxUINT32 &a, const __SBYTE__ &bl)
{
  return a.PackBits() == (__ULWORD__)bl || a.PackBits() > (__ULWORD__)bl; // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator>=(const __SBYTE__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al == b.PackBits() || (__ULWORD__)al > b.PackBits(); // PC-lint 09/15/2005: Ignore cast warning
}

GXDLCODE_API int operator>=(const gxUINT32 &a, const __UBYTE__ &bl)
{
  return a.PackBits() == (__ULWORD__)bl || a.PackBits() > (__ULWORD__)bl;
}

GXDLCODE_API int operator>=(const __UBYTE__ &al, const gxUINT32 &b)
{
  return (__ULWORD__)al == b.PackBits() || (__ULWORD__)al > b.PackBits();
}

#endif // __USE_NATIVE_INT_TYPES__ 
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
