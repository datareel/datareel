// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: crc32.h
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 08/17/1998 
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

The CRC32 functions (Cyclic Redundancy Check) are used to
calculate a sophisticated checksum based on the algebra of
polynomials. The Cyclic Redundancy Check, is a way to detect
bit errors that occur during data storage or transmission.
The CRC-32 algorithm operates on a block of data as a single
large numerical value. The algorithm divides this large value
by the CRC-32 polynomial or generator polynomial, leaving the
remainder 32-bit, which is the checksum. 
*/
// ----------------------------------------------------------- //   
#ifndef __GX_CRC32_HPP__
#define __GX_CRC32_HPP__

#include "gxdlcode.h"

GXDLCODE_API unsigned long calcCRC32(char *buf, unsigned len);
GXDLCODE_API unsigned long calcCRC32(const char *buf, unsigned len);
GXDLCODE_API unsigned long calcCRC32(unsigned char c, unsigned long CRC);

// This code is used for test purposes only
#ifdef __USE_CRC32_TABLE_FUNCTIONS__
#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
#include <fstream>
#include <iomanip>
#else // Use the old iostream library by default
#include <iostream.h>
#include <iomanip.h>
#endif // __USE_ANSI_CPP__

#include "dfileb.h"

GXDLCODE_API int makeCRC32(GXSTD::ostream &stream);
GXDLCODE_API unsigned long calcCRC32(DiskFileB &infile);
#endif

#endif  // __GX_CRC32_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
