// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: asprint.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/18/1997  
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

These classes and functions are used to print data to ASCII
text files and format data for various print functions.

Changes:
==============================================================
05/28/2002: Fixed out of bounds pointer in the ASPrint(const char c)
function.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

// This directive must be defined to use the ASCII print driver
#if defined (__USE_TEXT_PRINTING__)

#include "asprint.h"
#include <stdio.h>
#include <string.h>

GXDLCODE_API void ASPrint(const char *s, GXSTD::ostream &stream, 
			  int offset, int pos, const char pad)
// Prints a null terminated string to the stream starting at the
// specified position or the current position if the pos variable
// equals zero. The offset variable specifies the number of character
// to pad after the text is printed.
{
  int i = 0;
  int len = strlen(s);
  char *buf = new char[len+1];
  buf[len] = '\0';
  strcpy(buf, s);
  if(len > offset) len = offset-1; // Subtract 1 to avoid overlaps
  if(pos) for(i = 0; i < pos; i++) stream << pad; // Position the text
  for(i = 0; i < len; i++) stream << buf[i];
  if(len < offset) {
    offset = offset - len;
    for(i = 0; i < offset; i++) stream << pad;
  }
  delete[] buf;
}

GXDLCODE_API void ASPrint(const char *s, const char filter, 
			  GXSTD::ostream &stream, int offset,
			  int pos, const char pad)
// Prints a null terminated string to the stream starting and filters
// any unwanted characters specified by the filter variable.
{
  int i = 0;
  int len = strlen(s);
  char *buf = new char[len+1];
  buf[len] = '\0';
  strcpy(buf, s);
  if(len > offset) len = offset-1; // Subtract 1 to avoid overlaps
  if(pos) for(i = 0; i < pos; i++) stream << pad; // Position the text
  for(i = 0; i < len; i++) {
    // Filter all unwanted characters
    if(buf[i] != filter) stream << buf[i];
  }
  if(len < offset) {
    offset = offset - len;
    for(i = 0; i < offset; i++) stream << pad;
  }
  delete[] buf;
}

GXDLCODE_API void ASPrint(const char c, GXSTD::ostream &stream, 
			  int offset, int pos, const char pad)
// Prints a single character to the stream.
{
  char s[2];
  s[0] = c;    // Copy char into a string buffer
  s[1] = '\0'; // Null terminate the string
  ASPrint(s, stream, offset, pos, pad);
}
  
GXDLCODE_API void ASPrint(int val, GXSTD::ostream &stream, int offset, 
			  int pos, const char pad)
{
  char *buf = new char[offset+1];
  buf[offset] = '\0';
  sprintf(buf, "%d", val);
  ASPrint(buf, stream, offset, pos, pad);
  delete[] buf; 
}

GXDLCODE_API void ASPrint(long val, GXSTD::ostream &stream, int offset, 
			  int pos, const char pad)
{
  char *buf = new char[offset+1];
  buf[offset] = '\0';
  // 05/30/2002: Using type cast to int type to eliminate
  // warning: int format, long int arg (arg 3) when compiled
  // under Linux gcc.
  sprintf(buf, "%d", (int)val);
  ASPrint(buf, stream, offset, pos, pad);
  delete[] buf; 
}

GXDLCODE_API void ASPrint(double val, GXSTD::ostream &stream, int offset, 
			  int pos, const char pad)
{
  char *buf = new char[offset+1];
  buf[offset] = '\0';
  sprintf(buf, "%g", val);
  ASPrint(buf, stream, offset, pos, pad);
  delete[] buf; 
}

#endif // __USE_TEXT_PRINTING__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
