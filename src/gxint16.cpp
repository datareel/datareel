// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxint16.cpp
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

The gxINT16 class is used to represent 16 bit signed integers
independently of the operating system or hardware platform used.
It works by separating 16-bit values into two separate byte
values and reordering the bytes lowest-order to highest-order.
An gxINT16 type has a base 10 positive limit of 32,767 and a
negative limit of 32,768.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "gxint16.h"

#ifndef __USE_NATIVE_INT_TYPES__

#ifdef __BCC32__
#pragma warn -8071
#endif

#include <string.h>
#include <memory.h>

gxINT16::gxINT16(__SWORD__ val)
{
  UnPackBits(val);
}

gxINT16::gxINT16(const gxINT16& ob)
{
  memmove((void *)byte, (const void *)ob.byte, 2);
}

gxINT16& gxINT16::operator=(const gxINT16& ob)
{
  if(this != &ob) { // PC-lint 05/25/2005: Prevent self assignment 
    memmove((void *)byte, (const void *)ob.byte, 2);
  }
  return *this;
}

gxINT16& gxINT16::operator=(const __SWORD__ val)
{
  UnPackBits(val);
  return *this;
}

gxINT16::operator __SWORD__() const
{
  return PackBits();
}

__SWORD__ gxINT16::PackBits() const
{
  __SWORD__ a, b;
  
  a = (__SWORD__)byte[0];
  b = (__SWORD__)byte[1];

  a = a & 0xFF;
  b = (b<<8) & 0xFF00;

  return a + b;
}

void gxINT16::UnPackBits(__SWORD__ val)
{
  byte[0] = (__SBYTE__)(val & 0xFF);
  byte[1] = (__SBYTE__)((val & 0xFF00)>>8);
}

gxINT16 gxINT16::operator++(int) // Postfix
{
  gxINT16 val_before(*this); 
  operator=(*this + 1);
  return val_before;
}
 
gxINT16 gxINT16::operator--(int) // Postfix
{
  gxINT16 val_before(*this); 
  operator=(*this - 1);
  return val_before;
}

void gxINT16::operator/=(const gxINT16 &i)
{
  if(i == (gxINT16)0) return; // Divide by zero error
  operator=(*this / i);
}

void gxINT16::operator/=(const __LWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__SWORD__)i);
}

void gxINT16::operator/=(const __ULWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__SWORD__)i);
}

void gxINT16::operator/=(const __WORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__SWORD__)i);
}

void gxINT16::operator/=(const __SWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / i);
}

void gxINT16::operator/=(const __UWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__SWORD__)i);
}

void gxINT16::operator/=(const __USWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__SWORD__)i);
}

void gxINT16::operator/=(const __SBYTE__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__SWORD__)i);
}

void gxINT16::operator/=(const __UBYTE__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__SWORD__)i);
}

GXDLCODE_API int operator==(const gxINT16 &a, const gxINT16 &b)
{
  return a.PackBits() == b.PackBits();
}

GXDLCODE_API int operator==(const gxINT16 &a, const __LWORD__ &bs)
{
  return a.PackBits() == (__SWORD__)bs;
}

GXDLCODE_API int operator==(const __LWORD__ &as, const gxINT16 &b)
{
  return (__SWORD__)as == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxINT16 &a, const __ULWORD__ &bs)
{
  return a.PackBits() == (__SWORD__)bs;
}

GXDLCODE_API int operator==(const __ULWORD__ &as, const gxINT16 &b)
{
  return (__SWORD__)as == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxINT16 &a, const __WORD__ &bs)
{
  return a.PackBits() == (__SWORD__)bs;
}

GXDLCODE_API int operator==(const __WORD__ &as, const gxINT16 &b)
{
  return (__SWORD__)as == b.PackBits();
}

GXDLCODE_API int operator==(const gxINT16 &a, const __SWORD__ &bs)
{
  return a.PackBits() == bs;
}

GXDLCODE_API int operator==(const __SWORD__ &as, const gxINT16 &b)
{
  return as == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxINT16 &a, const __UWORD__ &bs)
{
  return a.PackBits() == (__SWORD__)bs;
}

GXDLCODE_API int operator==(const __UWORD__ &as, const gxINT16 &b)
{
  return (__SWORD__)as == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxINT16 &a, const __USWORD__ &bs)
{
  return  a.PackBits() == (__SWORD__)bs;
}

GXDLCODE_API int operator==(const __USWORD__ &as, const gxINT16 &b)
{
  return (__SWORD__)as == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxINT16 &a, const __SBYTE__ &bs)
{
  return a.PackBits() == (__SWORD__)bs;
}

GXDLCODE_API int operator==(const __SBYTE__ &as, const gxINT16 &b)
{
  return (__SWORD__)as == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxINT16 &a, const __UBYTE__ &bs)
{
  return a.PackBits() == (__SWORD__)bs;
}

GXDLCODE_API int operator==(const __UBYTE__ &as, const gxINT16 &b)
{
  return (__SWORD__)as == b.PackBits(); 
}

GXDLCODE_API int operator!=(const gxINT16 &a, const gxINT16 &b)
{
  return a.PackBits() != b.PackBits();
}

GXDLCODE_API int operator!=(const gxINT16 &a, const __LWORD__ &bs)
{
  return a.PackBits() != (__SWORD__)bs;
}

GXDLCODE_API int operator!=(const __LWORD__ &as, const gxINT16 &b)
{
  return (__SWORD__)as != b.PackBits();
}

GXDLCODE_API int operator!=(const gxINT16 &a, const __ULWORD__ &bs)
{
  return a.PackBits() != (__SWORD__)bs;
}

GXDLCODE_API int operator!=(const __ULWORD__ &as, const gxINT16 &b)
{
  return (__SWORD__)as != b.PackBits();
}

GXDLCODE_API int operator!=(const gxINT16 &a, const __WORD__ &bs)
{
  return a.PackBits() != (__SWORD__)bs;
}

GXDLCODE_API int operator!=(const __WORD__ &as, const gxINT16 &b)
{
  return (__SWORD__)as != b.PackBits();
}

GXDLCODE_API int operator!=(const gxINT16 &a, const __SWORD__ &bs)
{
  return a.PackBits() != bs;
}

GXDLCODE_API int operator!=(const __SWORD__ &as, const gxINT16 &b)
{
  return as != b.PackBits();
}

GXDLCODE_API int operator!=(const gxINT16 &a, const __UWORD__ &bs)
{
  return a.PackBits() != (__SWORD__)bs;
}

GXDLCODE_API int operator!=(const __UWORD__ &as, const gxINT16 &b)
{
  return (__SWORD__)as != b.PackBits();
}

GXDLCODE_API int operator!=(const gxINT16 &a, const __USWORD__ &bs)
{
  return a.PackBits() != (__SWORD__)bs;
}

GXDLCODE_API int operator!=(const __USWORD__ &as, const gxINT16 &b)
{
  return (__SWORD__)as != b.PackBits();
}

GXDLCODE_API int operator!=(const gxINT16 &a, const __SBYTE__ &bs)
{
  return a.PackBits() != (__SWORD__)bs;
}

GXDLCODE_API int operator!=(const __SBYTE__ &as, const gxINT16 &b)
{
  return (__SWORD__)as != b.PackBits();
}

GXDLCODE_API int operator!=(const gxINT16 &a, const __UBYTE__ &bs)
{
  return a.PackBits() != (__SWORD__)bs;
}

GXDLCODE_API int operator!=(const __UBYTE__ &as, const gxINT16 &b)
{
  return (__SWORD__)as != b.PackBits();
}

GXDLCODE_API int operator<(const gxINT16 &a, const gxINT16 &b)
{
  return a.PackBits() < b.PackBits();
}

GXDLCODE_API int operator<(const gxINT16 &a, const __LWORD__ &bs)
{
  return a.PackBits() < (__SWORD__)bs;
}

GXDLCODE_API int operator<(const __LWORD__ &as, const gxINT16 &b)
{
  return (__SWORD__)as < b.PackBits();
}

GXDLCODE_API int operator<(const gxINT16 &a, const __ULWORD__ &bs)
{
  return a.PackBits() < (__SWORD__)bs;
}

GXDLCODE_API int operator<(const __ULWORD__ &as, const gxINT16 &b)
{
  return (__SWORD__)as < b.PackBits();
}

GXDLCODE_API int operator<(const gxINT16 &a, const __WORD__ &bs)
{
  return a.PackBits() < (__SWORD__)bs;
}

GXDLCODE_API int operator<(const __WORD__ &as, const gxINT16 &b)
{
  return (__SWORD__)as < b.PackBits();
}

GXDLCODE_API int operator<(const gxINT16 &a, const __SWORD__ &bs)
{
  return a.PackBits() < bs;
}

GXDLCODE_API int operator<(const __SWORD__ &as, const gxINT16 &b)
{
  return as < b.PackBits();
}

GXDLCODE_API int operator<(const gxINT16 &a, const __UWORD__ &bs)
{
  return a.PackBits() < (__SWORD__)bs;
}

GXDLCODE_API int operator<(const __UWORD__ &as, const gxINT16 &b)
{
  return (__SWORD__)as < b.PackBits();
}

GXDLCODE_API int operator<(const gxINT16 &a, const __USWORD__ &bs)
{
  return a.PackBits() < (__SWORD__)bs;
}

GXDLCODE_API int operator<(const __USWORD__ &as, const gxINT16 &b)
{
  return (__SWORD__)as < b.PackBits();
}

GXDLCODE_API int operator<(const gxINT16 &a, const __SBYTE__ &bs)
{
  return a.PackBits() < (__SWORD__)bs;
}

GXDLCODE_API int operator<(const __SBYTE__ &as, const gxINT16 &b)
{
  return (__SWORD__)as < b.PackBits();
}

GXDLCODE_API int operator<(const gxINT16 &a, const __UBYTE__ &bs)
{
  return a.PackBits() < (__SWORD__)bs;
}

GXDLCODE_API int operator<(const __UBYTE__ &as, const gxINT16 &b)
{
  return (__SWORD__)as < b.PackBits();
}

GXDLCODE_API int operator>(const gxINT16 &a, const gxINT16 &b)
{
  return a.PackBits() > b.PackBits();
}

GXDLCODE_API int operator>(const gxINT16 &a, const __LWORD__ &bs)
{
  return a.PackBits() > (__SWORD__)bs;
}

GXDLCODE_API int operator>(const __LWORD__ &as, const gxINT16 &b)
{
  return (__SWORD__)as > b.PackBits();
}

GXDLCODE_API int operator>(const gxINT16 &a, const __ULWORD__ &bs)
{
  return a.PackBits() > (__SWORD__)bs;
}

GXDLCODE_API int operator>(const __ULWORD__ &as, const gxINT16 &b)
{
  return (__SWORD__)as > b.PackBits();
}

GXDLCODE_API int operator>(const gxINT16 &a, const __WORD__ &bs)
{
  return a.PackBits() > (__SWORD__)bs;
}

GXDLCODE_API int operator>(const __WORD__ &as, const gxINT16 &b)
{
  return (__SWORD__)as > b.PackBits();
}

GXDLCODE_API int operator>(const gxINT16 &a, const __SWORD__ &bs)
{
  return a.PackBits() > bs;
}

GXDLCODE_API int operator>(const __SWORD__ &as, const gxINT16 &b)
{
  return as > b.PackBits();
}

GXDLCODE_API int operator>(const gxINT16 &a, const __UWORD__ &bs)
{
  return a.PackBits() > (__SWORD__)bs;
}

GXDLCODE_API int operator>(const __UWORD__ &as, const gxINT16 &b)
{
  return (__SWORD__)as > b.PackBits();
}

GXDLCODE_API int operator>(const gxINT16 &a, const __USWORD__ &bs)
{
  return a.PackBits() > (__SWORD__)bs;
}

GXDLCODE_API int operator>(const __USWORD__ &as, const gxINT16 &b)
{
  return (__SWORD__)as > b.PackBits();
}

GXDLCODE_API int operator>(const gxINT16 &a, const __SBYTE__ &bs)
{
  return a.PackBits() > (__SWORD__)bs;
}

GXDLCODE_API int operator>(const __SBYTE__ &as, const gxINT16 &b)
{
  return (__SWORD__)as > b.PackBits();
}

GXDLCODE_API int operator>(const gxINT16 &a, const __UBYTE__ &bs)
{
  return a.PackBits() > (__SWORD__)bs;
}

GXDLCODE_API int operator>(const __UBYTE__ &as, const gxINT16 &b)
{
  return (__SWORD__)as > b.PackBits();
}

GXDLCODE_API int operator<=(const gxINT16 &a, const gxINT16 &b)
{
  return a.PackBits() == b.PackBits() || a.PackBits() < b.PackBits();
}

GXDLCODE_API int operator<=(const gxINT16 &a, const __LWORD__ &bs)
{
  return a.PackBits() == (__SWORD__)bs || a.PackBits() < (__SWORD__)bs;
}

GXDLCODE_API int operator<=(const __LWORD__ &as, const gxINT16 &b)
{
  return (__SWORD__)as == b.PackBits() || (__SWORD__)as < b.PackBits();
}

GXDLCODE_API int operator<=(const gxINT16 &a, const __ULWORD__ &bs)
{
  return a.PackBits() == (__SWORD__)bs || a.PackBits() < (__SWORD__)bs;
}

GXDLCODE_API int operator<=(const __ULWORD__ &as, const gxINT16 &b)
{
  return (__SWORD__)as == b.PackBits() || (__SWORD__)as < b.PackBits();
}

GXDLCODE_API int operator<=(const gxINT16 &a, const __WORD__ &bs)
{
  return a.PackBits() == (__SWORD__)bs || a.PackBits() < (__SWORD__)bs;
}

GXDLCODE_API int operator<=(const __WORD__ &as, const gxINT16 &b)
{
  return (__SWORD__)as == b.PackBits() || (__SWORD__)as < b.PackBits();
}

GXDLCODE_API int operator<=(const gxINT16 &a, const __SWORD__ &bs)
{
  return a.PackBits() == bs || a.PackBits() < bs;
}

GXDLCODE_API int operator<=(const __SWORD__ &as, const gxINT16 &b)
{
  return as == b.PackBits() || as < b.PackBits();
}

GXDLCODE_API int operator<=(const gxINT16 &a, const __UWORD__ &bs)
{
  return a.PackBits() == (__SWORD__)bs || a.PackBits() < (__SWORD__)bs;
}

GXDLCODE_API int operator<=(const __UWORD__ &as, const gxINT16 &b)
{
  return (__SWORD__)as == b.PackBits() || (__SWORD__)as < b.PackBits();
}

GXDLCODE_API int operator<=(const gxINT16 &a, const __USWORD__ &bs)
{
  return a.PackBits() == (__SWORD__)bs || a.PackBits() < (__SWORD__)bs;
}

GXDLCODE_API int operator<=(const __USWORD__ &as, const gxINT16 &b)
{
  return (__SWORD__)as == b.PackBits() || (__SWORD__)as < b.PackBits();
}

GXDLCODE_API int operator<=(const gxINT16 &a, const __SBYTE__ &bs)
{
  return a.PackBits() == (__SWORD__)bs ||  a.PackBits() < (__SWORD__)bs;
}

GXDLCODE_API int operator<=(const __SBYTE__ &as, const gxINT16 &b)
{
  return (__SWORD__)as == b.PackBits() || (__SWORD__)as < b.PackBits();
}

GXDLCODE_API int operator<=(const gxINT16 &a, const __UBYTE__ &bs)
{
  return a.PackBits() == (__SWORD__)bs ||  a.PackBits() < (__SWORD__)bs;
}

GXDLCODE_API int operator<=(const __UBYTE__ &as, const gxINT16 &b)
{
  return (__SWORD__)as == b.PackBits() || (__SWORD__)as < b.PackBits();
}

GXDLCODE_API int operator>=(const gxINT16 &a, const gxINT16 &b)
{
  return a.PackBits() == b.PackBits() || a.PackBits() > b.PackBits();
}

GXDLCODE_API int operator>=(const gxINT16 &a, const __LWORD__ &bs)
{
  return a.PackBits() == (__SWORD__)bs || a.PackBits() > (__SWORD__)bs;
}

GXDLCODE_API int operator>=(const __LWORD__ &as, const gxINT16 &b)
{
  return (__SWORD__)as == b.PackBits() || (__SWORD__)as > b.PackBits();
}

GXDLCODE_API int operator>=(const gxINT16 &a, const __ULWORD__ &bs)
{
  return a.PackBits() == (__SWORD__)bs || a.PackBits() > (__SWORD__)bs;
}

GXDLCODE_API int operator>=(const __ULWORD__ &as, const gxINT16 &b)
{
  return (__SWORD__)as == b.PackBits() || (__SWORD__)as > b.PackBits();
}

GXDLCODE_API int operator>=(const gxINT16 &a, const __WORD__ &bs)
{
  return a.PackBits() == (__SWORD__)bs || a.PackBits() > (__SWORD__)bs;
}

GXDLCODE_API int operator>=(const __WORD__ &as, const gxINT16 &b)
{
  return (__SWORD__)as == b.PackBits() || (__SWORD__)as > b.PackBits();
}

GXDLCODE_API int operator>=(const gxINT16 &a, const __SWORD__ &bs)
{
  return a.PackBits() == bs || a.PackBits() > bs;
}

GXDLCODE_API int operator>=(const __SWORD__ &as, const gxINT16 &b)
{
  return as == b.PackBits() || as > b.PackBits();
}

GXDLCODE_API int operator>=(const gxINT16 &a, const __UWORD__ &bs)
{
  return a.PackBits() == (__SWORD__)bs || a.PackBits() > (__SWORD__)bs;
}

GXDLCODE_API int operator>=(const __UWORD__ &as, const gxINT16 &b)
{
  return (__SWORD__)as == b.PackBits() || (__SWORD__)as > b.PackBits();
}

GXDLCODE_API int operator>=(const gxINT16 &a, const __USWORD__ &bs)
{
  return a.PackBits() == (__SWORD__)bs || a.PackBits() > (__SWORD__)bs;
}

GXDLCODE_API int operator>=(const __USWORD__ &as, const gxINT16 &b)
{
  return (__SWORD__)as == b.PackBits() || (__SWORD__)as > b.PackBits();
}

GXDLCODE_API int operator>=(const gxINT16 &a, const __SBYTE__ &bs)
{
  return a.PackBits() == (__SWORD__)bs || a.PackBits() > (__SWORD__)bs;
}

GXDLCODE_API int operator>=(const __SBYTE__ &as, const gxINT16 &b)
{
  return (__SWORD__)as == b.PackBits() || (__SWORD__)as > b.PackBits();
}

GXDLCODE_API int operator>=(const gxINT16 &a, const __UBYTE__ &bs)
{
  return a.PackBits() == (__SWORD__)bs || a.PackBits() > (__SWORD__)bs;
}

GXDLCODE_API int operator>=(const __UBYTE__ &as, const gxINT16 &b)
{
  return (__SWORD__)as == b.PackBits() || (__SWORD__)as > b.PackBits();
}

#ifdef __BCC32__
#pragma warn .8071
#endif

#endif // __USE_NATIVE_INT_TYPES__ 
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
