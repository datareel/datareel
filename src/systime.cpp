// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: systime.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 12/11/1996 
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
  
The SysTime class is used to convert the system time to various
character string and integer formats.

Changes:
==============================================================
05/03/2002: The SysTime::FormatTime() will output use 4-digit 
dates under Linux gcc. This change was made to eliminate 2-
digit year warnings.

08/12/2002: The SysTime destructor will free heap space for the
SysTime::TimeBuffer variable for all objects using the reentrant
localtime_r() and gmtime_r() functions.

08/12/2002: Modified the SysTime::InitTimeBuffers() function to
call the reentrant localtime_r() function instead of localtime()
under UNIX.

08/12/2002: Removed the duplicate declaration of "time_t STime"
and "struct tm *TimeBuffer" from the SysTime::GetSystemDateTime()
function.

08/12/2002: Removed the duplicate declaration of "time_t STime"
and "struct tm *TimeBuffer" from the SysTime::GetSystemTime()
function.

08/12/2002: Removed the duplicate declaration of "time_t STime"
and "struct tm *TimeBuffer" from the SysTime::GetSystemDate()
function.

08/12/2002: Modified the SysTime::DateSpan() function to call 
the reentrant localtime_r() function instead of localtime()
under UNIX.

08/12/2002: Modified the SysTime::DaysOld() function to call 
the reentrant localtime_r() function instead of localtime()
under UNIX.

08/12/2002: Removed the duplicate declaration of "time_t STime"
and "struct tm *TimeBuffer" from the SysTime::GetGMTDateTime()
function.

08/12/2002: Modified the SysTime::GetGMTDateTime() function to 
call the reentrant gmtime_r() function instead of gmtime()
under UNIX.

09/03/2002: Added the SysTime::GetSMTPTime() function used
to make SMTP time stamps.

09/03/2002: Modified the SysTime::GetSystemDateTime function to 
call the reentrant localtime_r() function instead of localtime()
under UNIX.

09/03/2002: Modified the SysTime::GetSystemTime function to call
the reentrant localtime_r() function instead of localtime()
under UNIX.

09/03/2002: Modified the SysTime::GetSystemDate function to call
the reentrant localtime_r() function instead of localtime()
under UNIX.

09/04/2002: The __REENTRANT__ preprocessor directive must be 
defined during UNIX builds to enable the use of reentrant
localtime_r and gmtime_r calls within all SysTime functions
that initialize the internal time buffers.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "systime.h"

#ifdef __BCC32__
#pragma warn -8080
#endif

char *SysTimeUnit::c_str_Hours()
{
  const int MaxStrLen = 25;
  char str[MaxStrLen];
  sprintf(str, "%u", hours);
  int len = strlen(str);
  char *buf = new char[len];
  buf[len] = '\0';
  memmove(buf, str, len);
  return buf;
}

char *SysTimeUnit::c_str_Minutes()
{
  const int MaxStrLen = 25;
  char str[MaxStrLen];
  sprintf(str, "%u", minutes);
  int len = strlen(str);
  char *buf = new char[len];
  buf[len] = '\0';
  memmove(buf, str, len);
  return buf;
}

char *SysTimeUnit::c_str_Seconds()
{
  const int MaxStrLen = 25;
  char str[MaxStrLen];
  sprintf(str, "%u", seconds);
  int len = strlen(str);
  char *buf = new char[len];
  buf[len] = '\0';
  memmove(buf, str, len);
  return buf;
}

char *SysTimeUnit::c_str_Milliseconds()
{
  const int MaxStrLen = 25;
  char str[MaxStrLen];
  sprintf(str, "%u", milliseconds);
  int len = strlen(str);
  char *buf = new char[len];
  buf[len] = '\0';
  memmove(buf, str, len);
  return buf;
}

SysTime::SysTime()
{
  InitTimeBuffers();
}

SysTime::~SysTime()
{
#if defined (__UNIX__) && defined (__REENTRANT__)
  if(TimeBuffer) {
    delete TimeBuffer;
    TimeBuffer = 0;
  }
#else
  ; // TimeBuffer is not using heap space
#endif
}

void SysTime::InitTimeBuffers()
{
  time(&STime); // PC-lint 09/15/2005: time() function is considered dangerous
#if defined (__WIN32__)
  TimeBuffer = localtime(&STime);
#elif defined (__UNIX__) && defined (__REENTRANT__)
  // Using the UNIX localtime_r() function instead of localtime()
  TimeBuffer = new tm;
  memset(TimeBuffer, 0, sizeof(struct tm));
  localtime_r(&STime, TimeBuffer);
#else // Use localtime() by default
  TimeBuffer = localtime(&STime);
#endif
}

void SysTime::FormatTime(int Format)
{
  InitTimeBuffers();

  char TimeFormat[81];
  switch(Format) {
    case AMPM:             // AM or PM
      // PC-lint 09/15/2005: strftime function is considered dangerous
      strftime(TimeFormat, 80, "%p", TimeBuffer); 
      IntTimeFormat = 0;
      break;
      
    case Date:             // System date
      // 05/30/2002: Modified to use 4 digit year to avoid
      // 2 digit year warning under Linux gcc 
      strftime(TimeFormat, 80, "%X", TimeBuffer);
      IntTimeFormat = 0;
      break;
      
    case DayOfTheMonth:    // Day of the month (01..31)
      strftime(TimeFormat, 80, "%d", TimeBuffer);
      // PC-lint 09/15/2005: atoi function is considered dangerous
      IntTimeFormat = atoi(TimeFormat); 
      break;
      
    case DayOfTheWeek:     // Display the day of the week (0..6, Sunday=0)
      strftime(TimeFormat, 80, "%w", TimeBuffer);
      IntTimeFormat = atoi(TimeFormat);
      break;
      
    case Hour:             // Hour (24 hour clock - 00..23)
      strftime(TimeFormat, 80, "%H", TimeBuffer);
      IntTimeFormat = atoi(TimeFormat);
      break;
      
    case JDay:             // Julian day
      strftime(TimeFormat, 80, "%j", TimeBuffer);
      IntTimeFormat = atoi(TimeFormat);
      break;
      
    case Minutes:          // Minutes (00..59)
      strftime(TimeFormat, 80, "%M", TimeBuffer);
      IntTimeFormat = atoi(TimeFormat);
      break;
      
    case Month:            // Month (01..12)
      strftime(TimeFormat, 80, "%m", TimeBuffer);
      IntTimeFormat = atoi(TimeFormat);
      break;
      
    case FullMonthName:    // Full month name 
      strftime(TimeFormat, 80, "%B", TimeBuffer);
      IntTimeFormat = 0;
      break;
      
    case MonthName:        // Abbreviated month name
      strftime(TimeFormat, 80, "%b", TimeBuffer);
      IntTimeFormat = 0;
      break;
      
    case Seconds:          // Seconds (00..59)
      strftime(TimeFormat, 80, "%S", TimeBuffer);
      IntTimeFormat = atoi(TimeFormat);
      break;
      
    case SystemTime:       // System time
      strftime(TimeFormat, 80, "%X", TimeBuffer);
      IntTimeFormat = 0;
      break;
      
    case TimeZoneName:     // Timezone name (null string if no timezone)
      strftime(TimeFormat, 80, "%Z", TimeBuffer);
      IntTimeFormat = 0;
      break;
      
    case WeekDayName:      // Abbreviated weekday name
      strftime(TimeFormat, 80, "%a", TimeBuffer);
      IntTimeFormat = 0;
      break;
      
    case FullWeekDayName:  // Full weekday name
      strftime(TimeFormat, 80, "%A", TimeBuffer);
      IntTimeFormat = 0;
      break;
      
    case WeekOfYearSF:     // Week of the year (00..52, Sunday first)
      strftime(TimeFormat, 80, "%U", TimeBuffer);
      IntTimeFormat = atoi(TimeFormat);
      break;
      
    case WeekOfYearMF:     // Week of the year (00..52, Monday first)
      strftime(TimeFormat, 80, "%W", TimeBuffer);
      IntTimeFormat = atoi(TimeFormat);
      break;
      
    case Year:             // Year (0000..9999)
      strftime(TimeFormat, 80, "%Y", TimeBuffer);
      IntTimeFormat = atoi(TimeFormat);
      break;
      
    case YearXX:           // Year (00..99)
      // 05/30/2002: Modified to use 4 digit year to avoid
      // 2 digit year warning under Linux gcc. This change
      // means the use of two digit years will not be allowed
      // under Linux 
#if defined(__LINUX__)
      strftime(TimeFormat, 80, "%Y", TimeBuffer);
#else
      strftime(TimeFormat, 80, "%y", TimeBuffer);
#endif
      IntTimeFormat = atoi(TimeFormat);
      break;
      
    default: {               
	char *mesg = "Invalid time format";
	SBuffer = mesg;
	IntTimeFormat = InvalidTimeFormat;
	return;
      }
  }
  
  SBuffer = TimeFormat;
  return;
}

char *SysTime::GetSystemDateTime(int full_month_name)
// Function used to copy the current system time and date 
// into a null-terminated string. 
{
  char s[255];
  char month[25]; char day[25]; char year[25];
  char hour[25]; char minutes[25]; char seconds[25];
  time(&STime);

#if defined (__WIN32__)
  TimeBuffer = localtime(&STime);
#elif defined (__UNIX__) && defined (__REENTRANT__)
  // Using the UNIX localtime_r() function instead of localtime()
  localtime_r(&STime, TimeBuffer);
#else // Use localtime() by default
  TimeBuffer = localtime(&STime);
#endif

  if(full_month_name)
    strftime(month, 25, "%B", TimeBuffer);
  else
    strftime(month, 25, "%b", TimeBuffer);
  strftime(day, 25, "%d", TimeBuffer);
  strftime(year, 25, "%Y", TimeBuffer);
  strftime(hour, 25, "%H", TimeBuffer);
  strftime(minutes, 25, "%M", TimeBuffer);
  strftime(seconds, 25, "%S", TimeBuffer);

  // Weekday Name Month Day, Year HH:MM:SS
  sprintf(s, "%s %s, %s %s:%s:%s", month, day, year, hour, minutes, seconds);

  SBuffer = s;
  return SBuffer.c_str();
}

char *SysTime::GetSystemTime()
// Function used to copy the current system time into a
// null-terminated string. 
{
  char s[255];
  char hour[25]; char minutes[25]; char seconds[25];
  time(&STime);

#if defined (__WIN32__)
  TimeBuffer = localtime(&STime);
#elif defined (__UNIX__) && defined (__REENTRANT__)
  // Using the UNIX localtime_r() function instead of localtime()
  localtime_r(&STime, TimeBuffer);
#else // Use localtime() by default
  TimeBuffer = localtime(&STime);
#endif

  strftime(hour, 25, "%H", TimeBuffer);
  strftime(minutes, 25, "%M", TimeBuffer);
  strftime(seconds, 25, "%S", TimeBuffer);

  // HH:MM:SS
  sprintf(s, "%s:%s:%s", hour, minutes, seconds);

  SBuffer = s;
  return SBuffer.c_str();
}

char *SysTime::GetSystemDate(int full_month_name)
// Function used to copy the current system date into a 
// null-terminated string. 
{
  char s[255];
  char month[25]; char day[25]; char year[25];
  time(&STime);

#if defined (__WIN32__)
  TimeBuffer = localtime(&STime);
#elif defined (__UNIX__) && defined (__REENTRANT__)
  // Using the UNIX localtime_r() function instead of localtime()
  localtime_r(&STime, TimeBuffer);
#else // Use localtime() by default
  TimeBuffer = localtime(&STime);
#endif

  if(full_month_name)
    strftime(month, 25, "%B", TimeBuffer);
  else
    strftime(month, 25, "%b", TimeBuffer);
  strftime(day, 25, "%d", TimeBuffer);
  strftime(year, 25, "%Y", TimeBuffer);

  // Weekday Name Month Day, Year 
  sprintf(s, "%s %s, %s", month, day, year);

  SBuffer = s;
  return SBuffer.c_str();
}

int SysTime::DateSpan(char *s, unsigned days, int full_month_name)
// Function used to calculate a date span from a specified day.
// Returns false is an error occurs. This function assumes that
// memory has already been allocated for the "s" pointer.

{
  struct tm when;
  time_t now, result; 
  char month[25]; char day[25]; char year[25];
  
  time(&now);

#if defined (__WIN32__)
  when = *localtime(&now);
#elif defined (__UNIX__) && defined (__REENTRANT__)
  // Using the UNIX localtime_r() function instead of localtime()
  localtime_r(&now, &when);
#else // Use localtime() by default
  when = *localtime(&now);
#endif

  when.tm_mday = when.tm_mday + days;
  // 06/03/2002: Now using result variable to eliminate BCC32 warning
  result = mktime(&when);
  if(result != (time_t)-1) {
    if(full_month_name)
      strftime(month, 25, "%B", &when);
    else
      strftime(month, 25, "%b", &when);
    strftime(day, 25, "%d", &when);
    strftime(year, 25, "%Y", &when);
    
    // Weekday Name Month Day, Year 
    sprintf(s, "%s %s, %s", month, day, year);
  }
  else
    return 0;

  return 1; // Return true if no errors occurred
}

int SysTime::IsLeapYear(int year)
// Calculate leap years using the Gregorian leap
// year formula: 365 + 1/4 - 1/100 + 1/400
// Returns true if the specified year is a leap year.
{
  int is_leap_year;
  
  if(year % 4 != 0)
    is_leap_year = 0;
  else if(year % 400 == 0)
    is_leap_year = 1;
  else if(year % 100 == 0)
    // If the year is divisible by 100, it's not a 
    // leap year UNLESS it is also divisible by 400.
    is_leap_year = 0;
  else
    is_leap_year = 1;
    
  return is_leap_year;

}
  
int SysTime::DaysOld(int jday, int year)
// Calculate the number of days that have passed since the
// specified Julian day and four digit year.
{
  int age;
  time_t system_time;
  char julian_day_cur[80];
  char julian_year_cur[80];
  int cur_day_value;
  int cur_year_value;
  int diff_year;
  int diff_day;
  time(&system_time);

#if defined (__WIN32__)
  struct tm *time_now;
  time_now = localtime(&system_time);
  strftime(julian_day_cur, 80, "%j", time_now );
  strftime(julian_year_cur, 80, "%Y", time_now );
#elif defined (__UNIX__) && defined (__REENTRANT__)
  // Using the UNIX localtime_r() function instead of localtime()
  struct tm time_now;
  localtime_r(&system_time, &time_now);
  strftime(julian_day_cur, 80, "%j", &time_now );
  strftime(julian_year_cur, 80, "%Y", &time_now );
#else // Use localtime() by default
  struct tm *time_now;
  time_now = localtime(&system_time);
  strftime(julian_day_cur, 80, "%j", time_now );
  strftime(julian_year_cur, 80, "%Y", time_now );
#endif

  cur_day_value = atoi(julian_day_cur);
  cur_year_value = atoi(julian_year_cur);
  diff_day = (cur_day_value - jday);
  diff_year = (cur_year_value - year);

  // Compensate for year change 
  if((diff_year != 0) && (cur_day_value < jday)) {
    diff_day = (systimeYEAR_CONSTANT - jday) + cur_day_value;
    diff_year = (diff_year - 1);
  } 

  age = ((diff_year * systimeYEAR_CONSTANT) + diff_day);
  return(age);
} 

char *SysTime::GetSyslogTime()
// Function used to make a Syslog time string. Returns a
// null terminated time string in the following format: 
// Month day HH:MM:SS
{
  char month[25]; char day[25];
  char hour[25]; char minutes[25]; char seconds[25];
  time(&STime);

#if defined (__WIN32__)
  TimeBuffer = localtime(&STime);
#elif defined (__UNIX__) && defined (__REENTRANT__)
  // Using the UNIX localtime_r() function instead of localtime()
  localtime_r(&STime, TimeBuffer);
#else // Use localtime() by default
  TimeBuffer = localtime(&STime);
#endif

  strftime(month, 25, "%b", TimeBuffer);
  strftime(day, 25, "%d", TimeBuffer);
  strftime(hour, 25, "%H", TimeBuffer);
  strftime(minutes, 25, "%M", TimeBuffer);
  strftime(seconds, 25, "%S", TimeBuffer);

  SBuffer << clear << month << " " << day << " " << hour << ":"
	  << minutes << ":" << seconds;
  return SBuffer.c_str();
}

char *SysTime::GetSMTPTime()
// Function used to make a SMTP time string. Returns a
// null terminated time string in the following format: 
// Weekday, Day Month Year HH:MM:SS - Timezone
{
  char s[255];
  char weekday[25]; char timezone[25];
  char month[25]; char day[25]; char year[25];
  char hour[25]; char minutes[25]; char seconds[25];
  time(&STime);

#if defined (__WIN32__)
  TimeBuffer = localtime(&STime);
#elif defined (__UNIX__) && defined (__REENTRANT__)
  // Using the UNIX localtime_r() function instead of localtime()
  localtime_r(&STime, TimeBuffer);
#else // Use localtime() by default
  TimeBuffer = localtime(&STime);
#endif

  strftime(weekday, 25, "%a", TimeBuffer);
  strftime(timezone, 25, "%Z", TimeBuffer);
  strftime(month, 25, "%b", TimeBuffer);
  strftime(day, 25, "%d", TimeBuffer);
  strftime(year, 25, "%Y", TimeBuffer);
  strftime(hour, 25, "%H", TimeBuffer);
  strftime(minutes, 25, "%M", TimeBuffer);
  strftime(seconds, 25, "%S", TimeBuffer);

  // Format the SMTP date string
  sprintf(s, "%s, %s %s %s %s:%s:%s - %s", 
	  weekday, day, month, year, hour, minutes, seconds,
	  timezone);

  SBuffer = s;
  return SBuffer.c_str();
}

char *SysTime::GetGMTDateTime()
// Function used to make a date string in GMT time
// rather then local time. Returns a null terminated
// date string: Tue, 15 Nov 1994 08:12:31 GMT
{
  char s[255];
  char month[25]; char day[25]; char year[25]; char wday_name[25];
  char hour[25]; char minutes[25]; char seconds[25];
  time(&STime);

#if defined (__WIN32__)
  TimeBuffer = gmtime(&STime);
#elif defined (__UNIX__) && defined (__REENTRANT__)
  // Using the UNIX gmtime_r() function instead of gmtime()
  gmtime_r(&STime, TimeBuffer);
#else // Use gmtime() by default
  TimeBuffer = gmtime(&STime);
#endif

  strftime(month, 25, "%b", TimeBuffer);
  strftime(day, 25, "%d", TimeBuffer);
  strftime(year, 25, "%Y", TimeBuffer);
  strftime(hour, 25, "%H", TimeBuffer);
  strftime(minutes, 25, "%M", TimeBuffer);
  strftime(seconds, 25, "%S", TimeBuffer);
  strftime(wday_name, 25, "%a", TimeBuffer);

  // Weekday Name Month Day, Year HH:MM:SS
  //  Tue, 15 Nov 1994 08:12:31 GMT
  sprintf(s, "%s, %s %s %s %s:%s:%s GMT",
	  wday_name, day, month, year, hour, minutes, seconds);

  SBuffer = s;
  return SBuffer.c_str();
}

char *SysTime::MakeFileModTime(time_t mod_time)
// Function used to make a file modification time use
// with directory functions.
{
  char s[255];
  char month[25]; char day[25]; char year[25];
  char hour[25]; char minutes[25];
  char ampm[25];

#if defined (__WIN32__)
  TimeBuffer = localtime(&mod_time);
#elif defined (__UNIX__) && defined (__REENTRANT__)
  // Using the UNIX localtime_r() function instead of localtime()
  localtime_r(&mod_time, TimeBuffer);
#else // Use localtime() by default
  TimeBuffer = localtime(&mod_time);
#endif

  strftime(month, 25, "%m", TimeBuffer);
  strftime(day, 25, "%d", TimeBuffer);
  strftime(year, 25, "%Y", TimeBuffer);
  strftime(hour, 25, "%I", TimeBuffer);
  strftime(minutes, 25, "%M", TimeBuffer);
  strftime(ampm, 25, "%p", TimeBuffer);
  
  // Weekday Name Month Day, Year HH:MM:SS
  sprintf(s, "%s/%s/%s  %s:%s %s", month, day, year, hour, minutes, ampm);

  SBuffer = s;
  return SBuffer.c_str();
}

char *SysTime::MakeGMTDateTime(time_t mod_time)
// Function used to make a date string in GMT time
// rather then local time. Returns a null terminated
// date string: Tue, 15 Nov 1994 08:12:31 GMT
{
  char s[255];
  char month[25]; char day[25]; char year[25]; char wday_name[25];
  char hour[25]; char minutes[25]; char seconds[25];

#if defined (__WIN32__)
  TimeBuffer = gmtime(&mod_time);
#elif defined (__UNIX__) && defined (__REENTRANT__)
  // Using the UNIX gmtime_r() function instead of gmtime()
  gmtime_r(&mod_time, TimeBuffer);
#else // Use gmtime() by default
  TimeBuffer = gmtime(&mod_time);
#endif

  strftime(month, 25, "%b", TimeBuffer);
  strftime(day, 25, "%d", TimeBuffer);
  strftime(year, 25, "%Y", TimeBuffer);
  strftime(hour, 25, "%H", TimeBuffer);
  strftime(minutes, 25, "%M", TimeBuffer);
  strftime(seconds, 25, "%S", TimeBuffer);
  strftime(wday_name, 25, "%a", TimeBuffer);

  // Weekday Name Month Day, Year HH:MM:SS
  //  Tue, 15 Nov 1994 08:12:31 GMT
  sprintf(s, "%s, %s %s %s %s:%s:%s GMT",
	  wday_name, day, month, year, hour, minutes, seconds);

  SBuffer = s;
  return SBuffer.c_str();
}

#ifdef __BCC32__
#pragma warn .8080
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
