// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: gxfloat.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/12/1997  
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

The gxFLOAT64 class is used to represent 64 bit signed double
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

Changes:
==============================================================
02/22/2002: The PackBits() function no longer uses heap space 
for temporary floating pointer buffer.

02/22/2002: The UnPackBits() function no longer uses heap space 
for temporary floating pointer buffer.
==============================================================
*/
// ----------------------------------------------------------- //   
#include "gxdlcode.h"

#include "gxfloat.h"

#ifndef __USE_NATIVE_FLOAT_TYPES__

#include <string.h>
#include <memory.h>

// Enable this macro for Intel x86 family (Little-Endian)
// ===============================================================
// #ifndef __X86__
// #define __X86__
// #endif  

gxFLOAT64::gxFLOAT64(__DPFLOAT__ val)
{
  UnPackBits(val);
}

gxFLOAT64::gxFLOAT64(const gxFLOAT64& ob)
{
  memmove((void *)byte, (const void *)ob.byte, sizeof(__DPFLOAT__));
}

gxFLOAT64& gxFLOAT64::operator=(const gxFLOAT64& ob)
{
  if(this != &ob) { // PC-lint 05/25/2005: Prevent self assignment 
    memmove((void *)byte, (const void *)ob.byte, sizeof(__DPFLOAT__));
  }
  return *this;
}

gxFLOAT64& gxFLOAT64::operator=(const __DPFLOAT__ val)
{
  UnPackBits(val);
  return *this;
}

gxFLOAT64::operator __DPFLOAT__() const
{
  return PackBits();
}

__DPFLOAT__ gxFLOAT64::PackBits() const
{
  double buf;
  double *nval = &buf;

#ifdef __X86__ // Big to little endian conversion for X86 processors
  gxFLOAT64 ob;  
  ob.byte[7] = byte[0]; 
  ob.byte[6] = byte[1];
  ob.byte[5] = byte[2];
  ob.byte[4] = byte[3];
  ob.byte[3] = byte[4];
  ob.byte[2] = byte[5];
  ob.byte[1] = byte[6];
  ob.byte[0] = byte[7];
  memmove((void *)nval, (const void *)ob.byte, sizeof(__DPFLOAT__));
#else
  memmove((void *)nval, (const void *)byte, sizeof(__DPFLOAT__));
#endif
  
  return buf;
}

void gxFLOAT64::UnPackBits(__DPFLOAT__ val)
{
  __DPFLOAT__ *dptr = &val;
  const unsigned len = sizeof(__DPFLOAT__);
  char ptr[len];

  memmove((void *)ptr, (const void *)dptr, sizeof(__DPFLOAT__));

#ifdef __X86__ // Little to big endian conversion for X86 processors
  byte[7] = ptr[0];
  byte[6] = ptr[1];
  byte[5] = ptr[2];
  byte[4] = ptr[3];
  byte[3] = ptr[4];
  byte[2] = ptr[5];
  byte[1] = ptr[6];
  byte[0] = ptr[7];
#else                
  byte[0] = ptr[0];
  byte[1] = ptr[1];
  byte[2] = ptr[2];
  byte[3] = ptr[3];
  byte[4] = ptr[4];
  byte[5] = ptr[5];
  byte[6] = ptr[6];
  byte[7] = ptr[7];
#endif
}

gxFLOAT64 gxFLOAT64::operator++(int) // Postfix
{
  gxFLOAT64 val_before(*this); 
  operator=(*this + 1);
  return val_before;
}
 
gxFLOAT64 gxFLOAT64::operator--(int) // Postfix
{
  gxFLOAT64 val_before(*this); 
  operator=(*this - 1);
  return val_before;
}

void gxFLOAT64::operator/=(const gxFLOAT64 &fp)
{
  if(fp == (gxFLOAT64)0) return; // Divide by zero error
  operator=(*this / fp);
}

void gxFLOAT64::operator/=(const __DPFLOAT__ &fp)
{
  if(fp == 0) return; // Divide by zero error
  operator=(*this / fp);
}

void gxFLOAT64::operator/=(const __LWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / i);
}

void gxFLOAT64::operator/=(const __ULWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / i);
}

void gxFLOAT64::operator/=(const __WORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / i);
}

void gxFLOAT64::operator/=(const __SWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / i);
}

void gxFLOAT64::operator/=(const __UWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / i);
}

void gxFLOAT64::operator/=(const __USWORD__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / i);
}

void gxFLOAT64::operator/=(const __SBYTE__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__DPFLOAT__)i);
}

void gxFLOAT64::operator/=(const __UBYTE__ &i)
{
  if(i == 0) return; // Divide by zero error
  operator=(*this / (__DPFLOAT__)i);
}

GXDLCODE_API int operator==(const gxFLOAT64 &a, const gxFLOAT64 &b)
{
  return a.PackBits() == b.PackBits();
}

GXDLCODE_API int operator==(const gxFLOAT64 &a, const __DPFLOAT__ &bf)
{
  return a.PackBits() == bf;
}

GXDLCODE_API int operator==(const __DPFLOAT__ &af, const gxFLOAT64 &b)
{
  return af == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxFLOAT64 &a, const __LWORD__ &bl)
{
  return a.PackBits() == bl;
}

GXDLCODE_API int operator==(const __LWORD__ &al, const gxFLOAT64 &b)
{
  return al == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxFLOAT64 &a, const __ULWORD__ &bl)
{
  return a.PackBits() == bl;
}

GXDLCODE_API int operator==(const __ULWORD__ &al, const gxFLOAT64 &b)
{
  return al == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxFLOAT64 &a, const __WORD__ &bl)
{
  return a.PackBits() == bl;
}

GXDLCODE_API int operator==(const __WORD__ &al, const gxFLOAT64 &b)
{
  return al == b.PackBits();
}

GXDLCODE_API int operator==(const gxFLOAT64 &a, const __SWORD__ &bl)
{
  return a.PackBits() == bl;
}

GXDLCODE_API int operator==(const __SWORD__ &al, const gxFLOAT64 &b)
{
  return al == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxFLOAT64 &a, const __UWORD__ &bl)
{
  return a.PackBits() == bl;
}

GXDLCODE_API int operator==(const __UWORD__ &al, const gxFLOAT64 &b)
{
  return al == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxFLOAT64 &a, const __USWORD__ &bl)
{
  return  a.PackBits() == bl;
}

GXDLCODE_API int operator==(const __USWORD__ &al, const gxFLOAT64 &b)
{
  return al == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxFLOAT64 &a, const __SBYTE__ &bl)
{
  return a.PackBits() == (__DPFLOAT__)bl;
}

GXDLCODE_API int operator==(const __SBYTE__ &al, const gxFLOAT64 &b)
{
  return (__DPFLOAT__)al == b.PackBits(); 
}

GXDLCODE_API int operator==(const gxFLOAT64 &a, const __UBYTE__ &bl)
{
  return a.PackBits() == (__DPFLOAT__)bl;
}

GXDLCODE_API int operator==(const __UBYTE__ &al, const gxFLOAT64 &b)
{
  return (__DPFLOAT__)al == b.PackBits(); 
}

GXDLCODE_API int operator!=(const gxFLOAT64 &a, const gxFLOAT64 &b)
{
  return a.PackBits() != b.PackBits();
}

GXDLCODE_API int operator!=(const gxFLOAT64 &a, const __DPFLOAT__ &bf)
{
  return a.PackBits() != bf;
}

GXDLCODE_API int operator!=(const __DPFLOAT__ &af, const gxFLOAT64 &b)
{
  return af != b.PackBits();
}

GXDLCODE_API int operator!=(const gxFLOAT64 &a, const __LWORD__ &bl)
{
  return a.PackBits() != bl;
}

GXDLCODE_API int operator!=(const __LWORD__ &al, const gxFLOAT64 &b)
{
  return al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxFLOAT64 &a, const __ULWORD__ &bl)
{
  return a.PackBits() != bl;
}

GXDLCODE_API int operator!=(const __ULWORD__ &al, const gxFLOAT64 &b)
{
  return al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxFLOAT64 &a, const __WORD__ &bl)
{
  return a.PackBits() != bl;
}

GXDLCODE_API int operator!=(const __WORD__ &al, const gxFLOAT64 &b)
{
  return al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxFLOAT64 &a, const __SWORD__ &bl)
{
  return a.PackBits() != bl;
}

GXDLCODE_API int operator!=(const __SWORD__ &al, const gxFLOAT64 &b)
{
  return al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxFLOAT64 &a, const __UWORD__ &bl)
{
  return a.PackBits() != bl;
}

GXDLCODE_API int operator!=(const __UWORD__ &al, const gxFLOAT64 &b)
{
  return al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxFLOAT64 &a, const __USWORD__ &bl)
{
  return a.PackBits() != bl;
}

GXDLCODE_API int operator!=(const __USWORD__ &al, const gxFLOAT64 &b)
{
  return al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxFLOAT64 &a, const __SBYTE__ &bl)
{
  return a.PackBits() != (__DPFLOAT__)bl;
}

GXDLCODE_API int operator!=(const __SBYTE__ &al, const gxFLOAT64 &b)
{
  return (__DPFLOAT__)al != b.PackBits();
}

GXDLCODE_API int operator!=(const gxFLOAT64 &a, const __UBYTE__ &bl)
{
  return a.PackBits() != (__DPFLOAT__)bl;
}

GXDLCODE_API int operator!=(const __UBYTE__ &al, const gxFLOAT64 &b)
{
  return (__DPFLOAT__)al != b.PackBits();
}

GXDLCODE_API int operator<(const gxFLOAT64 &a, const gxFLOAT64 &b)
{
  return a.PackBits() < b.PackBits();
}

GXDLCODE_API int operator<(const gxFLOAT64 &a, const __DPFLOAT__ &bf)
{
  return a.PackBits() < bf;
}

GXDLCODE_API int operator<(const __DPFLOAT__ &af, const gxFLOAT64 &b)
{
  return af < b.PackBits();
}

GXDLCODE_API int operator<(const gxFLOAT64 &a, const __LWORD__ &bl)
{
  return a.PackBits() < bl;
}

GXDLCODE_API int operator<(const __LWORD__ &al, const gxFLOAT64 &b)
{
  return al < b.PackBits();
}

GXDLCODE_API int operator<(const gxFLOAT64 &a, const __ULWORD__ &bl)
{
  return a.PackBits() < bl;
}

GXDLCODE_API int operator<(const __ULWORD__ &al, const gxFLOAT64 &b)
{
  return al < b.PackBits();
}

GXDLCODE_API int operator<(const gxFLOAT64 &a, const __WORD__ &bl)
{
  return a.PackBits() < bl;
}

GXDLCODE_API int operator<(const __WORD__ &al, const gxFLOAT64 &b)
{
  return al < b.PackBits();
}

GXDLCODE_API int operator<(const gxFLOAT64 &a, const __SWORD__ &bl)
{
  return a.PackBits() < bl;
}

GXDLCODE_API int operator<(const __SWORD__ &al, const gxFLOAT64 &b)
{
  return al < b.PackBits();
}

GXDLCODE_API int operator<(const gxFLOAT64 &a, const __UWORD__ &bl)
{
  return a.PackBits() < bl;
}

GXDLCODE_API int operator<(const __UWORD__ &al, const gxFLOAT64 &b)
{
  return al < b.PackBits();
}

GXDLCODE_API int operator<(const gxFLOAT64 &a, const __USWORD__ &bl)
{
  return a.PackBits() < bl;
}

GXDLCODE_API int operator<(const __USWORD__ &al, const gxFLOAT64 &b)
{
  return al < b.PackBits();
}

GXDLCODE_API int operator<(const gxFLOAT64 &a, const __SBYTE__ &bl)
{
  return a.PackBits() < (__DPFLOAT__)bl;
}

GXDLCODE_API int operator<(const __SBYTE__ &al, const gxFLOAT64 &b)
{
  return (__DPFLOAT__)al < b.PackBits();
}

GXDLCODE_API int operator<(const gxFLOAT64 &a, const __UBYTE__ &bl)
{
  return a.PackBits() < (__DPFLOAT__)bl;
}

GXDLCODE_API int operator<(const __UBYTE__ &al, const gxFLOAT64 &b)
{
  return (__DPFLOAT__)al < b.PackBits();
}

GXDLCODE_API int operator>(const gxFLOAT64 &a, const gxFLOAT64 &b)
{
  return a.PackBits() > b.PackBits();
}

GXDLCODE_API int operator>(const gxFLOAT64 &a, const __DPFLOAT__ &bf)
{
  return a.PackBits() > bf;
}

GXDLCODE_API int operator>(const __DPFLOAT__ &af, const gxFLOAT64 &b)
{
  return af > b.PackBits();
}

GXDLCODE_API int operator>(const gxFLOAT64 &a, const __LWORD__ &bl)
{
  return a.PackBits() > bl;
}

GXDLCODE_API int operator>(const __LWORD__ &al, const gxFLOAT64 &b)
{
  return al > b.PackBits();
}

GXDLCODE_API int operator>(const gxFLOAT64 &a, const __ULWORD__ &bl)
{
  return a.PackBits() > bl;
}

GXDLCODE_API int operator>(const __ULWORD__ &al, const gxFLOAT64 &b)
{
  return al > b.PackBits();
}

GXDLCODE_API int operator>(const gxFLOAT64 &a, const __WORD__ &bl)
{
  return a.PackBits() > bl;
}

GXDLCODE_API int operator>(const __WORD__ &al, const gxFLOAT64 &b)
{
  return al > b.PackBits();
}

GXDLCODE_API int operator>(const gxFLOAT64 &a, const __SWORD__ &bl)
{
  return a.PackBits() > bl;
}

GXDLCODE_API int operator>(const __SWORD__ &al, const gxFLOAT64 &b)
{
  return al > b.PackBits();
}

GXDLCODE_API int operator>(const gxFLOAT64 &a, const __UWORD__ &bl)
{
  return a.PackBits() > bl;
}

GXDLCODE_API int operator>(const __UWORD__ &al, const gxFLOAT64 &b)
{
  return al > b.PackBits();
}

GXDLCODE_API int operator>(const gxFLOAT64 &a, const __USWORD__ &bl)
{
  return a.PackBits() > bl;
}

GXDLCODE_API int operator>(const __USWORD__ &al, const gxFLOAT64 &b)
{
  return al > b.PackBits();
}

GXDLCODE_API int operator>(const gxFLOAT64 &a, const __SBYTE__ &bl)
{
  return a.PackBits() > (__DPFLOAT__)bl;
}

GXDLCODE_API int operator>(const __SBYTE__ &al, const gxFLOAT64 &b)
{
  return (__DPFLOAT__)al > b.PackBits();
}

GXDLCODE_API int operator>(const gxFLOAT64 &a, const __UBYTE__ &bl)
{
  return a.PackBits() > (__DPFLOAT__)bl;
}

GXDLCODE_API int operator>(const __UBYTE__ &al, const gxFLOAT64 &b)
{
  return (__DPFLOAT__)al > b.PackBits();
}

GXDLCODE_API int operator<=(const gxFLOAT64 &a, const gxFLOAT64 &b)
{
  return a.PackBits() == b.PackBits() || a.PackBits() < b.PackBits();
}

GXDLCODE_API int operator<=(const gxFLOAT64 &a, const __DPFLOAT__ &bf)
{
  return a.PackBits() == bf || a.PackBits() < bf;
}

GXDLCODE_API int operator<=(const __DPFLOAT__ &af, const gxFLOAT64 &b)
{
  return af == b.PackBits() || af < b.PackBits();
}

GXDLCODE_API int operator<=(const gxFLOAT64 &a, const __LWORD__ &bl)
{
  return a.PackBits() == bl || a.PackBits() < bl;
}

GXDLCODE_API int operator<=(const __LWORD__ &al, const gxFLOAT64 &b)
{
  return al == b.PackBits() || al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxFLOAT64 &a, const __ULWORD__ &bl)
{
  return a.PackBits() == bl || a.PackBits() < bl;
}

GXDLCODE_API int operator<=(const __ULWORD__ &al, const gxFLOAT64 &b)
{
  return al == b.PackBits() || al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxFLOAT64 &a, const __WORD__ &bl)
{
  return a.PackBits() == bl || a.PackBits() < bl;
}

GXDLCODE_API int operator<=(const __WORD__ &al, const gxFLOAT64 &b)
{
  return al == b.PackBits() || al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxFLOAT64 &a, const __SWORD__ &bl)
{
  return a.PackBits() == bl || a.PackBits() < bl;
}

GXDLCODE_API int operator<=(const __SWORD__ &al, const gxFLOAT64 &b)
{
  return al == b.PackBits() || al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxFLOAT64 &a, const __UWORD__ &bl)
{
  return a.PackBits() == bl || a.PackBits() < bl;
}

GXDLCODE_API int operator<=(const __UWORD__ &al, const gxFLOAT64 &b)
{
  return al == b.PackBits() || al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxFLOAT64 &a, const __USWORD__ &bl)
{
  return a.PackBits() == bl || a.PackBits() < bl;
}

GXDLCODE_API int operator<=(const __USWORD__ &al, const gxFLOAT64 &b)
{
  return al == b.PackBits() || al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxFLOAT64 &a, const __SBYTE__ &bl)
{
  return a.PackBits() == (__DPFLOAT__)bl ||  a.PackBits() < (__DPFLOAT__)bl;
}

GXDLCODE_API int operator<=(const __SBYTE__ &al, const gxFLOAT64 &b)
{
  return (__DPFLOAT__)al == b.PackBits() || (__DPFLOAT__)al < b.PackBits();
}

GXDLCODE_API int operator<=(const gxFLOAT64 &a, const __UBYTE__ &bl)
{
  return a.PackBits() == (__DPFLOAT__)bl ||  a.PackBits() < (__DPFLOAT__)bl;
}

GXDLCODE_API int operator<=(const __UBYTE__ &al, const gxFLOAT64 &b)
{
  return (__DPFLOAT__)al == b.PackBits() || (__DPFLOAT__)al < b.PackBits();
}

GXDLCODE_API int operator>=(const gxFLOAT64 &a, const gxFLOAT64 &b)
{
  return a.PackBits() == b.PackBits() || a.PackBits() > b.PackBits();
}

GXDLCODE_API int operator>=(const gxFLOAT64 &a, const __DPFLOAT__ &bf)
{
  return a.PackBits() == bf || a.PackBits() > bf;
}

GXDLCODE_API int operator>=(const __DPFLOAT__ &af, const gxFLOAT64 &b)
{
  return af == b.PackBits() || af > b.PackBits();
}

GXDLCODE_API int operator>=(const gxFLOAT64 &a, const __LWORD__ &bl)
{
  return a.PackBits() == bl || a.PackBits() > bl;
}

GXDLCODE_API int operator>=(const __LWORD__ &al, const gxFLOAT64 &b)
{
  return al == b.PackBits() || al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxFLOAT64 &a, const __ULWORD__ &bl)
{
  return a.PackBits() == bl || a.PackBits() > bl;
}

GXDLCODE_API int operator>=(const __ULWORD__ &al, const gxFLOAT64 &b)
{
  return al == b.PackBits() || al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxFLOAT64 &a, const __WORD__ &bl)
{
  return a.PackBits() == bl || a.PackBits() > bl;
}

GXDLCODE_API int operator>=(const __WORD__ &al, const gxFLOAT64 &b)
{
  return al == b.PackBits() || al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxFLOAT64 &a, const __SWORD__ &bl)
{
  return a.PackBits() == bl || a.PackBits() > bl;
}

GXDLCODE_API int operator>=(const __SWORD__ &al, const gxFLOAT64 &b)
{
  return al == b.PackBits() || al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxFLOAT64 &a, const __UWORD__ &bl)
{
  return a.PackBits() == bl || a.PackBits() > bl;
}

GXDLCODE_API int operator>=(const __UWORD__ &al, const gxFLOAT64 &b)
{
  return al == b.PackBits() || al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxFLOAT64 &a, const __USWORD__ &bl)
{
  return a.PackBits() == bl || a.PackBits() > bl;
}

GXDLCODE_API int operator>=(const __USWORD__ &al, const gxFLOAT64 &b)
{
  return al == b.PackBits() || al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxFLOAT64 &a, const __SBYTE__ &bl)
{
  return a.PackBits() == (__DPFLOAT__)bl || a.PackBits() > (__DPFLOAT__)bl;
}

GXDLCODE_API int operator>=(const __SBYTE__ &al, const gxFLOAT64 &b)
{
  return (__DPFLOAT__)al == b.PackBits() || (__DPFLOAT__)al > b.PackBits();
}

GXDLCODE_API int operator>=(const gxFLOAT64 &a, const __UBYTE__ &bl)
{
  return a.PackBits() == (__DPFLOAT__)bl || a.PackBits() > (__DPFLOAT__)bl;
}

GXDLCODE_API int operator>=(const __UBYTE__ &al, const gxFLOAT64 &b)
{
  return (__DPFLOAT__)al == b.PackBits() || (__DPFLOAT__)al > b.PackBits();
}

#endif // __USE_NATIVE_FLOAT_TYPES__ 
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
