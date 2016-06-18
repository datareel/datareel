// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: hxcrc.h 
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

The CRC Class (Cyclic Redundancy Check) is used to calculate a
sophisticated checksum based on the algebra of polynomials over
the integers (mod 2). The Cyclic Redundancy Check, is a way to
detect bit errors that occur during data storage or transmission.
The CRC algorithm operates on a block of data as a single large
numerical value. The algorithm divides this large value by the
CRC polynomial or generator polynomial, leaving the remainder,
which is the CRC result. 
*/
// ----------------------------------------------------------- //   
#ifndef __HXCRC_HPP__
#define __HXCRC_HPP__

#include "gxdlcode.h"

#include "dfileb.h"

// (C)yclic (R)edundancy (C)heck Class
class hxCRC
{
public:
  hxCRC();
  ~hxCRC() { }
  
public:
  void crc32init();
  void crc16init();
  void crc16CCITTinit();
  
  unsigned long calcCRC32(DiskFileB &infile);
  
  // mode 1 = XModem/Zmodem/Arc/Hpack/LZH    
  // mode 0 = CCITT CRC-16, AX.25 Europan    
  unsigned short calcCRC16(DiskFileB &infile, int mode = 1);
  
public:
  unsigned long crc32tab[256];    // CRC-32 table
  unsigned short crc16tab[256];   // CRC-16 table 
  unsigned short crc16CCITT[256]; // CCITT CRC-16 table
};

#endif  // __HXCRC_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
