// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxmac48.h
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

The MAC48 class is used turn a 48-bit MAC address into concrete
a data type. A MAC (Media Access Control) address is a hardware 
address that uniquely identifies each node of a network. Ethernet 
hardware addresses are 48 bits, expressed as 12 hexadecimal digits. 
The first 24 bits of the MAC address consist of a unique vendor code
which should match the vendor of the network interface card.
*/
// ----------------------------------------------------------- //   
#ifndef __GX_MAC48_HPP__
#define __GX_MAC48_HPP__

#include "gxdlcode.h"

// --------------------------------------------------------------
// Constants 
// --------------------------------------------------------------
const int mac48MaxStrLen = 255;
// --------------------------------------------------------------

// MAC 48 class
class GXDLCODE_API gxMAC48
{
public:
  gxMAC48(unsigned char b1 = 0, unsigned char b2 = 0, unsigned char b3 = 0,
	  unsigned char b4 = 0, unsigned char b5 = 0, unsigned char b6 = 0) {
    byte[0] = b1; byte[1] = b2; byte[2] = b3; 
    byte[3] = b4; byte[4] = b5; byte[5] = b6; 
  }
  gxMAC48(char *mac, char delimiter = ':', int tuple = 2) { 
    SetMACAddress(mac, delimiter, tuple); 
  }
  gxMAC48(const char *mac, char delimiter = ':', int tuple = 2) { 
    SetMACAddress(mac, delimiter, tuple); 
  }
  gxMAC48(const gxMAC48& ob);
  gxMAC48& operator=(const gxMAC48& ob);
  
public:
  void SetByte1(unsigned char b = 0) { byte[0] = b; }
  void SetByte2(unsigned char b = 0) { byte[1] = b; }
  void SetByte3(unsigned char b = 0) { byte[2] = b; }
  void SetByte4(unsigned char b = 0) { byte[3] = b; }
  void SetByte5(unsigned char b = 0) { byte[4] = b; }
  void SetByte6(unsigned char b = 0) { byte[5] = b; }
  int SetMACAddress(const char *mac, char delimiter = ':', int tuple = 2);
  int SetMACAddress(char *mac, char delimiter = ':', int tuple = 2);
  void SetMACAddress(unsigned char b1 = 0, unsigned char b2 = 0, 
		     unsigned char b3 = 0, unsigned char b4 = 0, 
		     unsigned char b5 = 0, unsigned char b6 = 0) {
    byte[0] = b1; byte[1] = b2; byte[2] = b3; 
    byte[3] = b4; byte[4] = b5; byte[5] = b6;
  }

  unsigned char GetByte1() { return byte[0]; }
  unsigned char GetByte2() { return byte[1]; }
  unsigned char GetByte3() { return byte[2]; }
  unsigned char GetByte4() { return byte[3]; }
  unsigned char GetByte5() { return byte[4]; }
  unsigned char GetByte6() { return byte[5]; }
  int GetVendorCode();
  char *GetVendorCode(char *sbuf);
  char *GetVendorCode(char *sbuf) const;
  char *c_str(char *sbuf, char delimiter = ':', int tuple = 2);
  char *c_str(char *sbuf, char delimiter = ':', int tuple = 2) const;
  unsigned SizeOf() { return sizeof(byte);  }
  
public: // Overloaded operators
  GXDLCODE_API friend int operator==(const gxMAC48 &a, const gxMAC48 &b);
  GXDLCODE_API friend int operator!=(const gxMAC48 &a, const gxMAC48 &b);
  GXDLCODE_API friend int operator<(const gxMAC48 &a, const gxMAC48 &b);
  GXDLCODE_API friend int operator>(const gxMAC48 &a, const gxMAC48 &b);

private:
  unsigned char byte[6]; // 48-bit MAC address
};


// --------------------------------------------------------------
// Standalone functions
// --------------------------------------------------------------
GXDLCODE_API int ParseMACVendorEntry(char *entry, char *code, 
				     char *name, char *comment);
GXDLCODE_API int FindVendor(const char *fname, const char *code, 
			    char *name, char *comment);
GXDLCODE_API int FindVendor(const char *fname, const gxMAC48 &mac, 
			    char *name, char *comment);
// --------------------------------------------------------------

#endif  // __GX_MAC48_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
