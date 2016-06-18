// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: logfile.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/17/1995 
// Date Last Modified: 06/17/2016
// Copyright (c) 2001-2016 DataReel Software Development
// ----------------------------------------------------------- // 
// ------------- Program description and details ------------- // 
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

The LogFile class is used to log program events.

Changes:
==============================================================
02/27/2003: Modified the LogFile::Open() and LogFile::OverWrite() 
functions to correctly set the default translation mode to text 
after opening or overwriting a log file.
==============================================================
*/
// ----------------------------------------------------------- //   
#include "gxdlcode.h"

#include <string.h>
#include "logfile.h"
#include "systime.h"

#ifdef __BCC32__
#pragma warn -8080
#endif

int LogFile::Open(const char *fname)
// Public member function use to open an existing logfile without 
// overwriting it. Returns zero to indicate success or a non-zero 
// value to indicate a failure.
{
  int rv;
  if(df_Exists(fname)) {
    rv = df_Open(fname, DiskFileB::df_READWRITE, DiskFileB::df_APPEND);
  }
  else {
    rv =  df_Create(fname);
  }
  df_Text(); // Turn on text translation for all logfiles
  return rv;
}

int LogFile::OverWrite(const char *fname)
// Public member function use to open an existing logfile and 
// overwrite it. Returns zero to indicate success or a non-zero 
// value to indicate a failure.
{
  int rv = df_Create(fname);
  df_Text(); // Turn on text translation for all logfiles
  return rv;
}

int LogFile::Close() 
{ 
  return df_Close(); 
}

int LogFile::Flush() 
{ 
  return df_Flush(); 
}

int LogFile::WriteSysTime()
{
  if(df_is_open) {
    SysTime date;
    *this << ' ' << date.GetSystemDateTime(0) << ' ';
    return df_last_error;
  }
  else {
    return df_last_error = DiskFileB::df_OPEN_ERROR;
  }
}

char *LogFile::GetSystemTime(char *s, int full_month_name)
// Function used to create a custom time string. This function
// assumes that enough memory to hold the date/time string has
// been allocated for the "s" buffer. If "s" is a null pointer
// heap memory will be allocated for the string.
{
  if(!s) { // This is a null pointer
    s = new char[255]; // Allocate memory for the string buffer
    if(!s) return 0;
  } 
  SysTime date;
  strcpy(s, date.GetSystemDateTime(full_month_name));
  return s;
}

#ifdef __BCC32__
#pragma warn .8080
#endif
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
