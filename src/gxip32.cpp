// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxip32.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/21/1997
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

The gxIP32 class is used turn a 32-bit IP address into concrete a
data type.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include "gxip32.h"

#ifdef __BCC32__
#pragma warn -8080
#endif

gxIP32::gxIP32(const gxIP32& ob)
{
  byte[0] = ob.byte[0];
  byte[1] = ob.byte[1];
  byte[2] = ob.byte[2];
  byte[3] = ob.byte[3];
}

gxIP32& gxIP32::operator=(const gxIP32& ob)
{
  // PC-lint 04/26/2005: Check for self assignment
  if(this != &ob) {
    byte[0] = ob.byte[0];
    byte[1] = ob.byte[1];
    byte[2] = ob.byte[2];
    byte[3] = ob.byte[3];
  }
  return *this;
}

char *gxIP32::c_str(char *sbuf)
// Returns null terminated ip32 string
{
  if(!sbuf) return 0; // Prevent a program crash if this is a null pointer
  sprintf(sbuf, "%u.%u.%u.%u",
	  (unsigned)byte[0],(unsigned)byte[1],(unsigned)byte[2],(unsigned)byte[3]);
  return sbuf;
}

char *gxIP32::c_str(char *sbuf) const
// Returns null terminated ip32 string
{
  if(!sbuf) return 0; // Prevent a program crash if this is a null pointer
  sprintf(sbuf, "%u.%u.%u.%u",
	  (unsigned)byte[0],(unsigned)byte[1],(unsigned)byte[2],(unsigned)byte[3]);
  return sbuf;
}

GXDLCODE_API int operator==(const gxIP32 &a, const gxIP32 &b)
{
  if(a.byte[0] == b.byte[0] && a.byte[1] == b.byte[1] &&
     a.byte[2] == b.byte[2] && a.byte[3] == b.byte[3])
    return 1;
  else
    return 0;
}

GXDLCODE_API int operator!=(const gxIP32 &a, const gxIP32 &b)
{
  if(a.byte[3] != b.byte[3]) return 1;
  if(a.byte[2] != b.byte[2]) return 1;
  if(a.byte[1] != b.byte[1]) return 1;
  if(a.byte[0] != b.byte[0]) return 1;
  return 0;
}

GXDLCODE_API int operator<(const gxIP32 &a, const gxIP32 &b)
{
  if(a.byte[0] == b.byte[0]) {
    if(a. byte[1] < b.byte[1]) {
      return 1;
    }
    if(a.byte[1] == b.byte[1]) {
      if(a.byte[2] < b.byte[2]) return 1;
    }
    if(a.byte[2] == b.byte[2]) {
      if(a.byte[3] < b.byte[3]) return 1;
    }
  }
  return 0;
}

GXDLCODE_API int operator>(const gxIP32 &a, const gxIP32 &b)
{
  if(a == b) return 0;
  if(a < b) return 0;
  return 1;
}

int gxIP32::SetIPAddress(char *ip)
// Sets the IP address using a null terminated character string.
// Return false if the string is not formatted correctly using
// 32-bit dotted notation: 127.0.0.1 
{
  // Reset the current value
  byte[0] = 0;
  byte[1] = 0;
  byte[2] = 0;
  byte[3] = 0; 

  int i, j = 0;
  char buf[ip32MaxStrLen];
  for(j = 0; j < ip32MaxStrLen; j++) buf[j] = '\0';
  char *p = (char *)ip;
  int len = strlen(p);
  const int MaxBytes = 4;
  unsigned char ip_address[MaxBytes];
  for(i = 0; i < MaxBytes; i++) ip_address[i] = 0;
  int byte_count = 0;
  
  for(i = 0, j = 0; i < len && i != ip32MaxStrLen; i++, p++) {
    if(*p == '.') {
      if(byte_count > MaxBytes) return 0;
      if(atoi(buf) > 255) return 0;

      // PC-lint 04/26/2005: Possible access of out-of-bounds pointer
      if(byte_count < (MaxBytes-1)) {
	ip_address[byte_count++] = (unsigned char)atoi(buf);      
      }

      // Reset the string buffer
      for(j = 0; j < ip32MaxStrLen; j++) buf[j] = '\0';
      j = 0; 
    }
    if(*p != '.') { // Skip over the dot
      if(isdigit(*p)) {
	buf[j++] = *p;
      }
      else {
	return 0;
      }
    }
  }

  // String after the last dot
  if(atoi(buf) > 255) return 0;
  ip_address[byte_count++] = (unsigned char)atoi(buf);      
  
  byte[0] = ip_address[0];
  byte[1] = ip_address[1];
  byte[2] = ip_address[2];
  byte[3] = ip_address[3];

  return 1;
}

int gxIP32::SetIPAddress(const char *ip)
// Sets the IP address using a null terminated character string.
// Return false if the string is not formatted correctly using
// 32-bit dotted notation: 127.0.0.1 
{
  // Reset the current value
  byte[0] = 0;
  byte[1] = 0;
  byte[2] = 0;
  byte[3] = 0; 

  int i, j = 0;
  char buf[ip32MaxStrLen];
  for(j = 0; j < ip32MaxStrLen; j++) buf[j] = '\0';
  char *p = (char *)ip;
  int len = strlen(p);
  const int MaxBytes = 4;
  unsigned char ip_address[MaxBytes];
  for(i = 0; i < MaxBytes; i++) ip_address[i] = 0;
  int byte_count = 0;
  
  for(i = 0, j = 0; i < len && i != ip32MaxStrLen; i++, p++) {
    if(*p == '.') {
      if(byte_count > MaxBytes) return 0;
      if(atoi(buf) > 255) return 0;

      // PC-lint 04/26/2005: Possible access of out-of-bounds pointer
      if(byte_count < (MaxBytes-1)) {
	ip_address[byte_count++] = (unsigned char)atoi(buf);
      }

      // Reset the string buffer
      for(j = 0; j < ip32MaxStrLen; j++) buf[j] = '\0';
      j = 0; 
    }
    if(*p != '.') { // Skip over the dot
      if(isdigit(*p)) {
	buf[j++] = *p;
      }
      else {
	return 0;
      }
    }
  }

  // String after the last dot
  if(atoi(buf) > 255) return 0;
  ip_address[byte_count++] = (unsigned char)atoi(buf);      
  
  byte[0] = ip_address[0];
  byte[1] = ip_address[1];
  byte[2] = ip_address[2];
  byte[3] = ip_address[3];

  return 1;
}

#ifdef __BCC32__
#pragma warn .8080
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //

