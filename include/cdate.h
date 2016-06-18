// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: cdate.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/21/1997 
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

The CDate class is used to turn a date string into a concrete
data type.

Changes:
==============================================================
03/11/2002: By default C++ iostreams are no longer used. In order 
to use the iostream functions built into this class users must 
define __USE_CPP_IOSTREAM__ preprocessor directive. When using C++ 
iostreams users have the option to use the ANSI Standard C++ library 
by defining the __USE_ANSI_CPP__ preprocessor directive. By default 
the old iostream library will be used when the  __USE_CPP_IOSTREAM__ 
directive is defined.

05/19/2003: Added the CDate(const char *date) constructor, 
operator=(const char *date) assignment operator, and 
CDate::SetDate(const char *date) function to set a date value 
using a null-terminate C string.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_CDATE_HPP__
#define __GX_CDATE_HPP__

#include "gxdlcode.h"

#if defined (__USE_CPP_IOSTREAM__)
#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__
#endif // __USE_CPP_IOSTREAM__

#include "gxdtypes.h"
#include "gxuint16.h"

// Date type class
class GXDLCODE_API CDate
{
public:
  CDate() { }
  CDate(__UBYTE__ month, __UBYTE__ day, gxUINT16 year) {
    Month = month, Day = day, Year = year;
  }
  CDate(const char *date, char delimiter = '/') {
    SetDate(date, delimiter);
  }
  CDate(const CDate& ob);
  CDate& operator=(const CDate& ob);
  CDate& operator=(const char *date) {
    SetDate(date);
    return *this;
  }

public:
  void SetDay(__UBYTE__ byte) { Day = byte; }
  void SetMonth(__UBYTE__ byte) { Month = byte; }
  void SetYear(gxUINT16 uint16) { Year = uint16; }
  void SetDate(__UBYTE__ month, __UBYTE__ day, gxUINT16 year) {
    Month = month, Day = day, Year = year;
  }
  int SetDate(const char *date, char delimiter = '/');
  __UBYTE__ GetDay() { return Day; }
  __UBYTE__ GetMonth() { return Month; }
  gxUINT16 GetYear() { return Year; }
  char *c_str();
  char *month_c_str();
  char *day_c_str();
  char *year_c_str();
  unsigned SizeOf() { return sizeof(Month)+sizeof(Day)+sizeof(Year); }
  
public: // Overloaded operators
  GXDLCODE_API friend int operator==(const CDate &a, const CDate &b);
  GXDLCODE_API friend int operator!=(const CDate &a, const CDate &b);
  GXDLCODE_API friend int operator<(const CDate &a, const CDate &b);
  GXDLCODE_API friend int operator>(const CDate &a, const CDate &b);

#ifdef __USE_CPP_IOSTREAM__
public:
  GXDLCODE_API friend GXSTD::ostream &operator<<(GXSTD::ostream &os, 
						 const CDate &ob);
  GXDLCODE_API friend void operator>>(GXSTD::istream &is, CDate &ob);
#endif
  
private:
  __UBYTE__ Month;
  __UBYTE__ Day;
  gxUINT16 Year;
};

#endif  // __GX_CDATE_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
