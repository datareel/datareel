// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Header File Name: asprint.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/18/1997  
// Date Last Modified: 06/17/2016
// Copyright (c) 2001-2016 DataReel Software Development
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
03/11/2002: Users now have the option to use the ANSI Standard 
C++ library by defining the __USE_ANSI_CPP__ preprocessor directive. 
By default the old iostream library will be used for all iostream 
operations.
==============================================================
*/
// ----------------------------------------------------------- // 
#ifndef __GX_ASPRINT_HPP__
#define __GX_ASPRINT_HPP__

#include "gxdlcode.h"

// This directive must be defined to use the ASCII print driver
#if defined (__USE_TEXT_PRINTING__)

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <fstream> 
#else // Use the old iostream library by default
#include <fstream.h>
#endif // __USE_ANSI_CPP__

// Constants for print functions
const int asPrintCols = 80;      // Columns for portrait printouts
const int asPrintColsLong = 132; // Columns for landscape printouts
const int asPrintRows = 25;      // Rows for portrait printouts
const int asPrintRowsLong = 25;  // Rows for landscape printouts

// Control characters
const char asLineFeed = '\n';

// Function prototypes for standalone print functions
GXDLCODE_API void ASPrint(const char *s, GXSTD::ostream &stream, int offset, 
			  int pos = 0, const char pad = ' ');
GXDLCODE_API void ASPrint(const char *s, const char filter, 
			  GXSTD::ostream &stream, int offset,
			  int pos = 0, const char pad = ' ');
GXDLCODE_API void ASPrint(const char c, GXSTD::ostream &stream, int offset, 
			  int pos = 0,
			  const char pad = ' ');
GXDLCODE_API void ASPrint(int val, GXSTD::ostream &stream, int offset, 
			  int pos = 0, const char pad = ' ');
GXDLCODE_API void ASPrint(long val, GXSTD::ostream &stream, int offset, 
			  int pos = 0,
			  const char pad = ' ');
GXDLCODE_API void ASPrint(double val, GXSTD::ostream &stream, 
			  int offset, int pos = 0,
			  const char pad = ' ');

#endif // __USE_TEXT_PRINTING__

#endif // __GX_ASPRINT_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
