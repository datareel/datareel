// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: cdate.cpp
// C++ Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/21/1997 
// Date Last Modified: 10/26/2026
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

The CDate class is used to turn a date string into a concrete
data type.

Changes:
==============================================================
05/19/2003: Change the name of the MaxStrLen to cdateMaxStrLen 
to avoid any possible name conflicts.

09/19/2005: Using simplified version of the == and != overloads. 

10/26/2026: DataReel 7: CDate is the 4-byte date type stored in 
gxRDBMS date fields, so its layout is unchanged. The c_str() 
functions, which returned a new heap buffer on every call, are 
replaced by str() functions returning std::string. The 
interactive operator>> is removed. SetDate() returns false for a 
month above 12 or a day above 31.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "cdate.h"

namespace datareel {


std::string CDate::str() const
{
  return std::to_string((unsigned)Month) + "/" + 
    std::to_string((unsigned)Day) + "/" + std::to_string((unsigned long)Year);
}

std::string CDate::month_str() const
{
  return std::to_string((unsigned)Month);
}

std::string CDate::day_str() const
{
  return std::to_string((unsigned)Day);
}

std::string CDate::year_str() const
{
  return std::to_string((unsigned long)Year);
}

GXDLCODE_API std::ostream &operator<<(std::ostream &os, const CDate &ob)
{
  return os << ob.str();
}

GXDLCODE_API bool operator==(const CDate &a, const CDate &b)
{
  return a.Year == b.Year && a.Month == b.Month && a.Day == b.Day;
}

GXDLCODE_API bool operator!=(const CDate &a, const CDate &b)
{
  return !(a == b);
}

GXDLCODE_API bool operator<(const CDate &a, const CDate &b)
{
  if(a.Year != b.Year) return a.Year < b.Year;
  if(a.Month != b.Month) return a.Month < b.Month;
  return a.Day < b.Day;
}

GXDLCODE_API bool operator>(const CDate &a, const CDate &b)
{
  return b < a;
}

bool CDate::SetDate(std::string_view date, char delimiter)
// Sets the date from a string in M/D/YYYY notation. Returns false
// and leaves the date at 0/0/0 if the string is not formatted
// correctly.
{
  Month = 0;
  Day = 0;
  Year = (__UWORD__)0;

  unsigned long fields[3] = { 0, 0, 0 };
  int field = 0;
  int digits = 0;
  for(char c : date) {
    if(c == delimiter) {
      if(digits == 0 || ++field > 2) return false;
      digits = 0;
      continue;
    }
    if(c < '0' || c > '9') return false;
    if(++digits > 4) return false;
    fields[field] = fields[field] * 10 + (unsigned long)(c - '0');
  }
  if(field != 2 || digits == 0) return false;
  if(fields[0] > 12 || fields[1] > 31) return false;

  Month = (__UBYTE__)fields[0];
  Day = (__UBYTE__)fields[1];
  Year = (__UWORD__)fields[2];
  return true;
}
} // namespace datareel

// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
