// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: cdate.h
// C++ Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/21/1997 
// Date Last Modified: 10/26/2026
// Copyright (c) 2001-2026 DataReel Software Development
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

10/26/2026: DataReel 7: CDate is the 4-byte date type stored in 
gxRDBMS date fields, so its layout is unchanged. The c_str() 
functions, which returned a new heap buffer on every call, are 
replaced by str() functions returning std::string. The 
interactive operator>> is removed. SetDate() returns false for a 
month above 12 or a day above 31.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_CDATE_HPP__
#define __GX_CDATE_HPP__

#include "gxdlcode.h"

#include <ostream>
#include <string>
#include <string_view>

#include "gxdtypes.h"
#include "gxuint16.h"

// Date type class. The 4-byte layout (month, day, 16-bit year) is
// part of the gxRDBMS file format and must not change.
class GXDLCODE_API CDate
{
public:
  CDate() : Month(0), Day(0), Year((__UWORD__)0) { }
  CDate(__UBYTE__ month, __UBYTE__ day, gxUINT16 year) {
    Month = month, Day = day, Year = year;
  }
  explicit CDate(std::string_view date, char delimiter = '/') {
    SetDate(date, delimiter);
  }
  CDate& operator=(std::string_view date) {
    SetDate(date);
    return *this;
  }
  CDate& operator=(const char *date) {
    SetDate(date ? std::string_view(date) : std::string_view());
    return *this;
  }

public:
  void SetDay(__UBYTE__ byte) { Day = byte; }
  void SetMonth(__UBYTE__ byte) { Month = byte; }
  void SetYear(gxUINT16 uint16) { Year = uint16; }
  void SetDate(__UBYTE__ month, __UBYTE__ day, gxUINT16 year) {
    Month = month, Day = day, Year = year;
  }

  // Parse M/D/YYYY. Returns false if the string is not a valid date.
  bool SetDate(std::string_view date, char delimiter = '/');
  __UBYTE__ GetDay() const { return Day; }
  __UBYTE__ GetMonth() const { return Month; }
  gxUINT16 GetYear() const { return Year; }
  std::string str() const;      // M/D/YYYY
  std::string month_str() const;
  std::string day_str() const;
  std::string year_str() const;
  unsigned SizeOf() const { return sizeof(Month)+sizeof(Day)+sizeof(Year); }
  
public: // Overloaded operators
  GXDLCODE_API friend bool operator==(const CDate &a, const CDate &b);
  GXDLCODE_API friend bool operator!=(const CDate &a, const CDate &b);
  GXDLCODE_API friend bool operator<(const CDate &a, const CDate &b);
  GXDLCODE_API friend bool operator>(const CDate &a, const CDate &b);
  GXDLCODE_API friend std::ostream &operator<<(std::ostream &os, 
					       const CDate &ob);
  
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
