// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Header File Name: systime.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 12/11/1996 
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

The SysTime class is used to convert the system time to various
character string and integer formats.

Changes:
==============================================================
08/12/2002: The SysTime destructor is no longer inlined and is 
now defined in the systime.cpp file.

09/03/2002: Added the SysTime::GetSMTPTime() function used
to make SMTP time stamps.

04/01/2003: Added the SysTime::GetSyslogTime() function used
to make Syslog time stamps.

06/05/2003: Added the SysTime::MakeFileModTime() and 
SysTime::MakeGMTDateTime() functions used to make custom
time stamps for file and directory functions.
==============================================================
*/
// ----------------------------------------------------------- // 
#ifndef __GX_SYSTIME_HPP__
#define __GX_SYSTIME_HPP__

#include "gxdlcode.h"

#include <time.h>
#include "ustring.h"

// Year constants
const int systimeYEAR_CONSTANT      = 365;
const int systimeLEAP_YEAR_CONSTANT = 366;

// SysTime constants
const int systimeSTART_YEAR = 1900; // Starting point for time calculations 
const int systimeEND_YEAR = 2038;   // Last valid year for time calculations 

// Calender day constants
const int systimeJANUARY   = 31;
const int systimeFEBUARY   = 28;
const int systimeFEB_LEAP  = 29;
const int systimeMARCH     = 31;
const int systimeAPRIL     = 30;
const int systimeMAY       = 31;
const int systimeJUNE      = 30;
const int systimeJULY      = 31;
const int systimeAUGUST    = 31;
const int systimeSEPTEMBER = 30;
const int systimeOCTOBER   = 31;
const int systimeNOVEMBER  = 30;
const int systimeDECEMBER  = 31;

// Time unit data structure used to store time values
struct GXDLCODE_API SysTimeUnit
{
  SysTimeUnit() { hours = minutes = seconds = milliseconds = 0; }
  SysTimeUnit(const SysTimeUnit &ob) {
    hours = ob.hours; minutes = ob.minutes; seconds = ob.seconds;
    milliseconds = ob.milliseconds;
  }
  SysTimeUnit& operator=(const SysTimeUnit &ob) {
    hours = ob.hours; minutes = ob.minutes; seconds = ob.seconds;
    milliseconds = ob.milliseconds;
    return *this;
  }
  ~SysTimeUnit() { }
  
  char *c_str_Hours();
  char *c_str_Minutes();
  char *c_str_Seconds();
  char *c_str_Milliseconds();
  
  unsigned hours;
  unsigned minutes;
  unsigned seconds;
  unsigned milliseconds;
};

// System time class 
class GXDLCODE_API SysTime
{
public:
  SysTime();
  ~SysTime();
  
  enum {              // Time formats
    AMPM,             // AM or PM
    Date,             // System date
    DayOfTheMonth,    // Day of the month (01..31)
    DayOfTheWeek,     // Display the day of the week (0..6, Sunday=0)
    Hour,             // Hour (24 hour clock - 00..23)
    JDay,             // Julian day
    Minutes,          // Minutes (00..59)
    Month,            // Month (01..12)
    FullMonthName,    // Full month name 
    MonthName,        // Abbreviated month name
    Seconds,          // Seconds (00..59)
    SystemTime,       // System time
    TimeZoneName,     // Time zone name (null string if no time zone)
    WeekDayName,      // Abbreviated weekday name
    FullWeekDayName,  // Full weekday name
    WeekOfYearSF,     // Week of the year (00..52, Sunday first)
    WeekOfYearMF,     // Week of the year (00..52, Monday first)
    Year,             // Year (0000..9999)
    YearXX,           // Year (00..99)
    InvalidTimeFormat = 0xffff // Error code for invalid format
  };

private:
  SysTime(const SysTime &ob) { }        // Disallow copying
  void operator=(const SysTime &ob) { } // Disallow assignment

public:
  void FormatTime(int Format);
  void InitTimeBuffers();
  char *GetStrTime(int Format) { FormatTime(Format); return SBuffer.c_str(); }
  int GetIntTime(int Format) { FormatTime(Format); return IntTimeFormat; }

public: // Date/Time formating functions
  char *GetSystemTime();
  char *GetSystemDate(int full_month_name = 0);
  char *GetSystemDateTime(int full_month_name = 0);
  char *GetGMTDateTime();
  char *GetSMTPTime();
  char *GetSyslogTime();
  char *MakeFileModTime(time_t mod_time);
  char *MakeGMTDateTime(time_t mod_time);
  int DateSpan(char *s, unsigned days, int full_month_name = 0);
  int IsLeapYear(int year);
  int DaysOld(int jday, int year);
  
private:
  UString SBuffer;
  int IntTimeFormat;
  
public:
  time_t STime;
  struct tm *TimeBuffer;
};

// Standalone timer functions
// --------------------------------------------------------------
inline void systimeSleep(clock_t seconds)
// Sleep for specified number of seconds.
{
  // Convert milliseconds to seconds
  clock_t wait = ( seconds * CLOCKS_PER_SEC );
  clock_t goal = wait + clock();
  while( goal > clock() )
    ;
}

inline void systimePauseFor(clock_t wait)
// Pauses for a specified number of milliseconds. 
{
  clock_t goal;
  goal = wait + clock();
  while( goal > clock() )
    ;
}

inline clock_t systimeStart()
// Mark the starting time for the routine.
{
  return clock();
}

inline clock_t systimeStop()
// Mark the stop time for the routine. 
{
  return clock();
}

inline clock_t systimeElapsedSeconds(clock_t begin, clock_t end)
// Calculate the elapsed time in seconds.
{
  return (end - begin) / CLOCKS_PER_SEC;
}

inline double systimeElapsedTime(clock_t begin, clock_t end)
// Calculate the elapsed time in milliseconds. 
{
  return (double)(end - begin) / CLOCKS_PER_SEC;
}

#endif // __GX_SYSTIME_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //

