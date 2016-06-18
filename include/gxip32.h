// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxip32.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/21/1997
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

The gxIP32 class is used turn a 32-bit IP address into concrete a
data type.
*/
// ----------------------------------------------------------- //   
#ifndef __GX_IP32_HPP__
#define __GX_IP32_HPP__

#include "gxdlcode.h"

// --------------------------------------------------------------
// Constants 
// --------------------------------------------------------------
const int ip32MaxStrLen = 255;
// --------------------------------------------------------------

// IP 32 class
class GXDLCODE_API gxIP32
{
public:
  gxIP32(unsigned char b1 = 0, unsigned char b2 = 0, 
	 unsigned char b3 = 0, unsigned char b4 = 0) {
    byte[0] = b1; byte[1] = b2; byte[2] = b3; byte[3] = b4; 
  }
  gxIP32(char *ip) { SetIPAddress(ip); }
  gxIP32(const char *ip) { SetIPAddress(ip); }
  gxIP32(const gxIP32& ob);
  gxIP32& operator=(const gxIP32& ob);
  
public:
  void SetByte1(unsigned char b = 0) { byte[0] = b; }
  void SetByte2(unsigned char b = 0) { byte[1] = b; }
  void SetByte3(unsigned char b = 0) { byte[2] = b; }
  void SetByte4(unsigned char b = 0) { byte[3] = b; }

  int SetIPAddress(char *ip);
  int SetIPAddress(const char *ip);
  void SetIPAddress(unsigned char b1 = 0, unsigned char b2 = 0, 
		    unsigned char b3 = 0, unsigned char b4 = 0) {
    byte[0] = b1; byte[1] = b2; byte[2] = b3; byte[3] = b4; 
  }

  unsigned char GetByte1() { return byte[0]; }
  unsigned char GetByte2() { return byte[1]; }
  unsigned char GetByte3() { return byte[2]; }
  unsigned char GetByte4() { return byte[3]; }

  char *c_str(char *sbuf);
  char *c_str(char *sbuf) const;
  unsigned SizeOf() { return sizeof(byte); }
  
public: // Overloaded operators
  GXDLCODE_API friend int operator==(const gxIP32 &a, const gxIP32 &b);
  GXDLCODE_API friend int operator!=(const gxIP32 &a, const gxIP32 &b);
  GXDLCODE_API friend int operator<(const gxIP32 &a, const gxIP32 &b);
  GXDLCODE_API friend int operator>(const gxIP32 &a, const gxIP32 &b);

private:
  unsigned char byte[4]; // 32-bit IP address
};

#endif  // __GX_IP32_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
