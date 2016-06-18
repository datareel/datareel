// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: cdate.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/21/1997 
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

The CDate class is used to turn a date string into a concrete
data type.

Changes:
==============================================================
05/19/2003: Change the name of the MaxStrLen to cdateMaxStrLen 
to avoid any possible name conflicts.

09/19/2005: Using simplified version of the == and != overloads. 
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "cdate.h"

#ifdef __BCC32__
#pragma warn -8071
#pragma warn -8080
#endif

// String length for date strings
const int cdateMaxStrLen = 25;

CDate::CDate(const CDate& ob)
{
  Month = ob.Month;
  Day = ob.Day;
  Year = ob.Year;
}

CDate& CDate::operator=(const CDate& ob)
{
  if(this != &ob) {
    Month = ob.Month;
    Day = ob.Day;
    Year = ob.Year;
  }
  return *this;
}

char *CDate::c_str()
// Returns null terminated date string
{
  char date_str[cdateMaxStrLen];
  sprintf(date_str, "%u/%u/%u", (unsigned)Month,(unsigned)Day,(unsigned)Year);
  int len = strlen(date_str);
  char *buf = new char[len+1];
  buf[len] = '\0';
  memmove(buf, date_str, len);
  return buf;
}

char *CDate::month_c_str()
// Returns null terminated date string
{
  char date_str[cdateMaxStrLen];
  sprintf(date_str, "%u", (unsigned)Month);
  int len = strlen(date_str);
  char *buf = new char[len+1];
  buf[len] = '\0';
  memmove(buf, date_str, len);
  return buf;
}

char *CDate::day_c_str()
// Returns null terminated date string
{
  char date_str[cdateMaxStrLen];
  sprintf(date_str, "%u", (unsigned)Day);
  int len = strlen(date_str);
  char *buf = new char[len+1];
  buf[len] = '\0';
  memmove(buf, date_str, len);
  return buf;
}

char *CDate::year_c_str()
// Returns null terminated date string
{
  char date_str[cdateMaxStrLen];
  sprintf(date_str, "%u", (unsigned)Year);
  int len = strlen(date_str);
  char *buf = new char[len+1];
  buf[len] = '\0';
  memmove(buf, date_str, len);
  return buf;
}

#ifdef __USE_CPP_IOSTREAM__
GXDLCODE_API GXSTD::ostream &operator<<(GXSTD::ostream &os, const CDate &ob)
{
  os << (unsigned)ob.Month << "/"
     << (unsigned)ob.Day << "/"
     << ob.Year;
  return os;
}

GXDLCODE_API void operator>>(GXSTD::istream &is, CDate &ob)
{
  unsigned  buf;

  GXSTD::cout << "Month (XX):  ";
  is >> buf;
  ob.SetMonth((__UBYTE__)buf);

  GXSTD::cout << "Day (XX):    ";
  is >> buf;
  ob.SetDay((__UBYTE__)buf);

  GXSTD::cout << "Year (XXXX): ";
  is >> buf;
  // Ignore BCC32 conversion warning
  ob.SetYear(buf);
}
#endif // __USE_CPP_IOSTREAM__

GXDLCODE_API int operator==(const CDate &a, const CDate &b)
{
  return !(a != b); 
}

GXDLCODE_API int operator!=(const CDate &a, const CDate &b)
{
  if(a.Year != b.Year) return 1;
  if(a.Month != b.Month) return 1;
  if(a.Day != b.Day) return 1;
  return 0; 
}

GXDLCODE_API int operator<(const CDate &a, const CDate &b)
{
  if(a.Year < b.Year) return 1;
  if(a.Year == b.Year) {
    if(a.Month < b.Month) {
      return 1;
    }
    if(a.Month == b.Month) {
      if(a.Day < b.Day) return 1;
    }
  }
  
  return 0;
}

GXDLCODE_API int operator>(const CDate &a, const CDate &b)
{
  // PC-lint 05/25/2002: Calling overloaded == and < operators in 
  // overloaded > function generates bad type error.
  if(a == b) return 0;
  if(a < b) return 0;
  return 1;
}

int CDate::SetDate(const char *date, char delimiter)
// Sets the date using a null terminated character string.
// Return false if the string is not formatted correctly using
// The MM/DD/YYYY notation.
{
  // Reset the current value
  Month = 0;
  Day = 0;
  Year = (gxUINT16)0;

  int i, j = 0;
  char buf[cdateMaxStrLen];
  memset(buf, 0, cdateMaxStrLen);
  char *p = (char *)date;
  int len = strlen(p);
  const int MaxBytes = 4;
  unsigned char date_str[MaxBytes];
  memset(date_str, 0, MaxBytes);
  int byte_count = 0;
  int val;
  int num_delimiter = 0;

  for(i = 0, j = 0; i < len && i != cdateMaxStrLen; i++, p++) {
    if(*p == delimiter) {
      num_delimiter++;
      if(byte_count > MaxBytes) return 0;
      sscanf(buf, "%d", &val);
      // PC-lint 04/21/2004: Possible access of out-of-bounds pointer
      if(byte_count < (MaxBytes-1)) {
	date_str[byte_count++] = (unsigned char)(val & 0xFF);
      }
      memset(buf, 0, cdateMaxStrLen); // Reset the string buffer
      j = 0; 
    }
    if(num_delimiter > 2) return 0;
    if(*p != delimiter) { // Skip over the delimiter
      if(isdigit(*p)) {
	buf[j++] = *p;
      }
      else {
	return 0;
      }
    }
  }

  // String after the last delimiter
  sscanf(buf, "%d", &val);

  // Set the date
  Month = date_str[0];
  Day = date_str[1];
  Year = (gxUINT16)val;

  return 1;
}

#ifdef __BCC32__
#pragma warn .8071
#pragma warn .8080
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //

