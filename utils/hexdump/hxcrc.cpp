// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: hxcrc.cpp 
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 08/17/1998 
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
#include "gxdlcode.h"

#include "hxcrc.h"

hxCRC::hxCRC()
{
  // Initialize all the CRC tables
  crc32init();
  crc16init();
  crc16CCITTinit();
}

void hxCRC::crc32init()
// Write a CRC-32 table for a byte-wise 32-bit CRC calculation
// based on the Autodin/Ethernet/ADCCP polynomial of 0x4C11DB7:
// 0000 0100 1100 0001 0001 1101 1011 0111 
// In this representation the coefficient of x^0 is stored in the 
// MSB of the 32-bit word and the coefficient of x^31 is stored in 
// the LSB. Thus 0x4C11DB7 becomes 0xEDB88320:
// 1110 1101 1011 1000 1000 0011 0010 0000
{
  int i,n;
  unsigned long CRC32;
  for (i = 0; i < 256; i++) {
    CRC32=i;
    for (n = 1; n < 9; n++) {
      if (CRC32 & 1)
	CRC32 = (CRC32 >> 1) ^ 0xedb88320;
      else
	CRC32 = CRC32 >> 1;
    }
    crc32tab[i] = CRC32;
  }
}

void hxCRC::crc16init()
// Write a CRC-16 table for a byte-wise 16-bit CRC calculation
// based on the XModem/Zmodem/Arc/Hpack/LZH polynomial of 0x8005:
// 1000 0000 0000 0101
// In this representation the coefficient of x^0 is stored in the
// MSB of the 16-bit word and the coefficient of x^15 is stored in
// the LSB. Thus 0x8005 becomes 0xA001:
// 1010 0000 0000 0001
{
  int i,n;
  unsigned short CRC16;

  for (i = 0; i < 256; i++) {
    CRC16=i;
    for (n = 1; n < 9; n++) {
      if (CRC16 & 1)
	CRC16=(CRC16 >> 1) ^ 0xA001;
      else
	CRC16=CRC16 >> 1;
    }
    crc16tab[i]=CRC16;
  }
}

void hxCRC::crc16CCITTinit()
// Write a CRC-16 table for a byte-wise 16-bit CRC calculation
// based on the CCITT CRC-16/AX.25 polynomial of 0x1021:
// 0001 0000 0010 0001
// In this representation the coefficient of x^0 is stored in the
// MSB of the 16-bit word and the coefficient of x^15 is stored in
// the LSB. Thus 0x8005 becomes 0x8408:
// 1000 01000 0000 1000
{
  int i,n;
  unsigned short CRC16;
  for (i = 0; i < 256; i++) {
    CRC16=i;
    for (n = 1; n < 9; n++) {
      if (CRC16 & 1)
	CRC16=(CRC16 >> 1) ^ 0x8408;
      else
	CRC16=CRC16 >> 1;
    }
    crc16CCITT[i]=CRC16;
  }
}

unsigned long hxCRC::calcCRC32(DiskFileB &infile)
// Calculate the CRC-32 of a file. Returns
// the checksum value.
{
 unsigned long CRC = 0xffffffffL;
 char c;
 unsigned int i;

 // Rewind to the start of the stream
 if(infile.df_Rewind() != DiskFileB::df_NO_ERROR) return 0;

 while(!infile.df_EOF()) {
   if(infile.df_Get(c) != DiskFileB::df_NO_ERROR) return 0;
   i = (unsigned int)c;
   i &= 0xFF; // Reset all the bits
   CRC = crc32tab[(CRC ^ i) & 0xFF] ^ ((CRC>>8) & 0x00ffffffL);
 }
 return CRC ^ 0xffffffffL;
}

unsigned short hxCRC::calcCRC16(DiskFileB &infile, int mode)
// Calculate a 16-bit CRC for a file. If the mode
// variable is set to zero, a checksum based on the
// CCITT CRC-16/AX.25 polynomial is returned. If the
// mode variable is set to one, a checksum based on the
// XModem/Zmodem/Arc/Hpack/LZH polynomial is returned.
{
 unsigned short CRC = 0xffff;
 if(mode == 1) CRC = 0; 
 char c;
 unsigned short i;

  // Rewind to the start of the stream
 if(infile.df_Rewind() != DiskFileB::df_NO_ERROR) return 0;

 while(!infile.df_EOF()) {
   if(infile.df_Get(c) != DiskFileB::df_NO_ERROR) return 0;
   i = (unsigned short)c;
   i &= 0xFF; // Reset all the bits
   if (mode==1)
    CRC = ((CRC >> 8) & 0x00ff) ^ crc16tab[(CRC ^ i) & 0x00ff];
   else
     CRC = ((CRC >> 8) & 0x00ff) ^ crc16CCITT[(CRC ^ i) & 0x00ff];
 }
 if (mode == 1) return CRC;
 return CRC ^ 0xffff;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
