// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxmac48.cpp
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

Changes:
==============================================================
03/10/2002: The gxMAC48 class no longer uses the C++ fstream 
class as the underlying file system. The gxDatabase file pointer 
routines are now used to define the underlying file system used by 
the gxMAC48 class. This change was made to support large files 
and NTFS file system enhancements. To enable large file support 
users must define the __64_BIT_DATABASE_ENGINE__ preprocessor 
directive.

06/02/2002: Corrected incorrect assignment in two if statements 
in all versions of the gxMAC48::SetMACAddress()function.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "gxmac48.h"
#include "dfileb.h"
#include "strutil.h"

#ifdef __BCC32__
#pragma warn -8080
#endif

gxMAC48::gxMAC48(const gxMAC48& ob)
{
  byte[0] = ob.byte[0];
  byte[1] = ob.byte[1];
  byte[2] = ob.byte[2];
  byte[3] = ob.byte[3];
  byte[4] = ob.byte[4];
  byte[5] = ob.byte[5];
}

gxMAC48& gxMAC48::operator=(const gxMAC48& ob)
{
  // PC-lint 04/26/2005: Check for self assignment
  if(this != &ob) {
    byte[0] = ob.byte[0];
    byte[1] = ob.byte[1];
    byte[2] = ob.byte[2];
    byte[3] = ob.byte[3];
    byte[4] = ob.byte[4];
    byte[5] = ob.byte[5];
  }
  return *this;
}

char *gxMAC48::c_str(char *sbuf, char delimiter, int tuple)
// Returns null terminated mac48 string
{
  if(!sbuf) return 0; // Prevent a program crash if this is a null pointer
  if(tuple == 4) {
    sprintf(sbuf, "%.2X%.2X%c%.2X%.2X%c%.2X%.2X",
	    (unsigned)byte[0],(unsigned)byte[1], delimiter, 
	    (unsigned)byte[2],(unsigned)byte[3], delimiter,
	    (unsigned)byte[4], (unsigned)byte[5]);
  }
  else {
    sprintf(sbuf, "%.2X%c%.2X%c%.2X%c%.2X%c%.2X%c%.2X",
	    (unsigned)byte[0], delimiter, (unsigned)byte[1], delimiter,
	    (unsigned)byte[2], delimiter, (unsigned)byte[3], delimiter, 
	    (unsigned)byte[4], delimiter, (unsigned)byte[5]);
  }
  return sbuf;
}

char *gxMAC48::c_str(char *sbuf, char delimiter, int tuple) const
// Returns null terminated mac48 string
{
  if(!sbuf) return 0; // Prevent a program crash if this is a null pointer
  if(tuple == 4) {
    sprintf(sbuf, "%.2X%.2X%c%.2X%.2X%c%.2X%.2X",
	    (unsigned)byte[0],(unsigned)byte[1], delimiter, 
	    (unsigned)byte[2],(unsigned)byte[3], delimiter,
	    (unsigned)byte[4], (unsigned)byte[5]);
  }
  else {
    sprintf(sbuf, "%.2X%c%.2X%c%.2X%c%.2X%c%.2X%c%.2X",
	    (unsigned)byte[0], delimiter, (unsigned)byte[1], delimiter,
	    (unsigned)byte[2], delimiter, (unsigned)byte[3], delimiter, 
	    (unsigned)byte[4], delimiter, (unsigned)byte[5]);
  }
  return sbuf;
}

GXDLCODE_API int operator==(const gxMAC48 &a, const gxMAC48 &b)
{
  if(a.byte[0] == b.byte[0] && a.byte[1] == b.byte[1] &&
     a.byte[2] == b.byte[2] && a.byte[3] == b.byte[3] &&
     a.byte[4] == b.byte[4] && a.byte[5] == b.byte[5])
    return 1;
  else
    return 0;
}

GXDLCODE_API int operator!=(const gxMAC48 &a, const gxMAC48 &b)
{
  if(a.byte[5] != b.byte[5]) return 1;
  if(a.byte[4] != b.byte[4]) return 1;
  if(a.byte[3] != b.byte[3]) return 1;
  if(a.byte[2] != b.byte[2]) return 1;
  if(a.byte[1] != b.byte[1]) return 1;
  if(a.byte[0] != b.byte[0]) return 1;
  return 0;
}

GXDLCODE_API int operator<(const gxMAC48 &a, const gxMAC48 &b)
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
    if(a.byte[3] == b.byte[3]) {
      if(a.byte[4] < b.byte[4]) return 1;
    }
    if(a.byte[5] == b.byte[5]) {
      if(a.byte[5] < b.byte[5]) return 1;
    }
  }
  return 0;
}

GXDLCODE_API int operator>(const gxMAC48 &a, const gxMAC48 &b)
{
  if(a == b) return 0;
  if(a < b) return 0;
  return 1;
}

int gxMAC48::SetMACAddress(const char *mac, char delimiter, int tuple)
// Sets the MAC address using a null terminated character string.
// Return false if the string is not formatted correctly using
// 48-bit notation: 00:60:B0:F9:B8:95, 00-60-B0-F9-B8-95,
// 0060.B0F9.B895, or 0060-B0F9-B895 
{
  // Reset the current value
  byte[0] = 0;
  byte[1] = 0;
  byte[2] = 0;
  byte[3] = 0;
  byte[4] = 0;
  byte[5] = 0;

  int i, j = 0;
  char buf[mac48MaxStrLen];
  for(j = 0; j < mac48MaxStrLen; j++) buf[j] = '\0';
  char *p = (char *)mac;
  int len = strlen(p);
  const int MaxBytes = 6;
  unsigned char mac_address[MaxBytes];
  for(i = 0; i < MaxBytes; i++) mac_address[i] = 0;
  int byte_count = 0;
  int val;

  for(i = 0, j = 0; i < len && i != mac48MaxStrLen; i++, p++) {
    if(*p == delimiter) {
      if(byte_count > MaxBytes) return 0;
      if(tuple == 2) {
	sscanf(buf, "%x", &val);
	// PC-lint 04/26/2005: Possible access of out-of-bounds pointer
	if(byte_count < (MaxBytes-1)) {
	  mac_address[byte_count++] = (unsigned char)(val & 0xFF);
	}
      }
      else if(tuple == 4) {
	sscanf(buf, "%x", &val);

	// PC-lint 04/26/2005: Possible access of out-of-bounds pointer
	if(byte_count < (MaxBytes-1)) {
	  mac_address[byte_count++] = (unsigned char)((val & 0xFF00)>>8);
	}
	// PC-lint 04/26/2005: Possible access of out-of-bounds pointer
	if(byte_count < (MaxBytes-1)) {
	  mac_address[byte_count++] = (unsigned char)(val & 0xFF);
	}
      }
      else {
	return 0;
      }
      // Reset the string buffer
      for(j = 0; j < (int)mac48MaxStrLen; j++) buf[j] = '\0';
      j = 0; 
    }
    if(*p != delimiter) { // Skip over the delimiter
      if(isalnum(*p)) {
	buf[j++] = *p;
      }
      else {
	return 0;
      }
    }
  }

  // String after the last delimiter
  if(tuple == 2) {
    sscanf(buf, "%x", &val);
    mac_address[byte_count] = (unsigned char)(val & 0xFF);
  }
  else if(tuple == 4) {
    sscanf(buf, "%x", &val);
    mac_address[byte_count++] = (unsigned char)((val & 0xFF00)>>8);
    mac_address[byte_count] = (unsigned char)(val & 0xFF);
  }
  
  byte[0] = mac_address[0];
  byte[1] = mac_address[1];
  byte[2] = mac_address[2];
  byte[3] = mac_address[3];
  byte[4] = mac_address[4];
  byte[5] = mac_address[5];

  return 1;
}

int gxMAC48::SetMACAddress(char *mac, char delimiter, int tuple)
// Sets the MAC address using a null terminated character string.
// Return false if the string is not formatted correctly using
// 48-bit notation: 00:60:B0:F9:B8:95, 00-60-B0-F9-B8-95,
// 0060.B0F9.B895, or 0060-B0F9-B895 
{
  // Reset the current values
  byte[0] = 0;
  byte[1] = 0;
  byte[2] = 0;
  byte[3] = 0;
  byte[4] = 0;
  byte[5] = 0;

  int i, j = 0;
  char buf[mac48MaxStrLen];
  for(j = 0; j < mac48MaxStrLen; j++) buf[j] = '\0';
  char *p = mac;
  int len = strlen(p);
  const int MaxBytes = 6;
  unsigned char mac_address[MaxBytes];
  for(i = 0; i < MaxBytes; i++) mac_address[i] = 0;
  int byte_count = 0;
  int val;

  for(i = 0, j = 0; i < len && i != mac48MaxStrLen; i++, p++) {
    if(*p == delimiter) {
      if(byte_count > MaxBytes) return 0;
      if(tuple == 2) {
	sscanf(buf, "%x", &val);
	// PC-lint 04/26/2005: Possible access of out-of-bounds pointer
	if(byte_count < (MaxBytes-1)) {
	  mac_address[byte_count++] = (unsigned char)(val & 0xFF);
	}
      }
      else if(tuple == 4) {
	sscanf(buf, "%x", &val);

	// PC-lint 04/26/2005: Possible access of out-of-bounds pointer
	if(byte_count < (MaxBytes-1)) {
	  mac_address[byte_count++] = (unsigned char)((val & 0xFF00)>>8);
	}
	// PC-lint 04/26/2005: Possible access of out-of-bounds pointer
	if(byte_count < (MaxBytes-1)) {
	  mac_address[byte_count++] = (unsigned char)(val & 0xFF);
	}
      }
      else {
	return 0;
      }
      // Reset the string buffer
      for(j = 0; j < mac48MaxStrLen; j++) buf[j] = '\0';
      j = 0; 
    }
    if(*p != delimiter) { // Skip over the delimiter
      if(isalnum(*p)) {
	buf[j++] = *p;
      }
      else {
	return 0;
      }
    }
  }

  // String after the last delimiter
  if(tuple == 2) {
    sscanf(buf, "%x", &val);
    mac_address[byte_count] = (unsigned char)(val & 0xFF);
  }
  else if(tuple == 4) {
    sscanf(buf, "%x", &val);
    mac_address[byte_count++] = (unsigned char)((val & 0xFF00)>>8);
    mac_address[byte_count] = (unsigned char)(val & 0xFF);
  }
  
  byte[0] = mac_address[0];
  byte[1] = mac_address[1];
  byte[2] = mac_address[2];
  byte[3] = mac_address[3];
  byte[4] = mac_address[4];
  byte[5] = mac_address[5];

  return 1;
}

int gxMAC48::GetVendorCode()
{
  int a, b, c;
  a = (int)byte[3];
  b = (int)byte[2];
  c = (int)byte[0];

  a = a & 0xFF;
  b = (b<<8) & 0xFF00;
  c = (c<<16) & 0xFF0000;

  return a + b + c;
}

char *gxMAC48::GetVendorCode(char *sbuf)
{
  if(!sbuf) return 0; // Prevent a program crash if this is a null pointer
  sprintf(sbuf, "%.2X%.2X%.2X",
	  (unsigned)byte[0], (unsigned)byte[1], (unsigned)byte[2]);
  return sbuf;
}

char *gxMAC48::GetVendorCode(char *sbuf) const
{
  if(!sbuf) return 0; // Prevent a program crash if this is a null pointer
  sprintf(sbuf, "%.2X%.2X%.2X",
	  (unsigned)byte[0], (unsigned)byte[1], (unsigned)byte[2]);
  return sbuf;
}

GXDLCODE_API int ParseMACVendorEntry(char *entry, char *code, char *name, char *comment)
// Function used to parse a single line of a MAC vendor code text file. 
// Returns true if no error were encountered or false if the line
// does not contain a valid vendor code and name.
//
// MAC vendor database format :
// <vendor code> <vendor name> [# comments]
// 
{
  int len, offset, i = 0;
  int byte_count = strlen(entry);

  // Reset all the values
  code[0] = 0;
  name[0] = 0;
  comment[0] = 0;

  // Trim leading tabs and spaces
  while((entry[i] == ' ') || (entry[i] == '\t')) i++;
  byte_count -= i;
  if(byte_count <= 0) return 0;
  memmove(entry, entry+i, byte_count);
  entry[byte_count] = 0;

  // Filter out comments and blank lines
  switch(entry[0]) {
    case '#': return 0;
    case '\n': return 0;
    case '\r': return 0;
    case '\0': return 0;
    default: break;
  }
  
  // Parse the vendor code
  for(i = 0; i < byte_count; i ++) {
    if((entry[i] == ' ') || (entry[i] == '\t')) break;
    code[i] = entry[i];
  }
  code[i] = 0; // Null terminate the string

  // Parse the vendor name
  while((entry[i] == ' ') || (entry[i] == '\t')) i++;
  byte_count -= i;
  if(byte_count <= 0) return 0;
  memmove(entry, entry+i, byte_count);
  for(i = 0; i < byte_count; i ++) {
    if(entry[i] == '#') break;
    name[i] = entry[i];
  }
  name[i] = 0; // Null terminate the string
  
  if(name[0] != 0)  { // Remove any trailing spaces
    offset = len = strlen(name);
    while(offset--) if(name[offset] != ' ') break;
    if(offset < len) name[offset+1] = 0;
  }
  
  // Parse any comments on this line
  while((entry[i] == ' ') || (entry[i] == '\t') || (entry[i] == '#')) i++;
  byte_count -= i;
  if(byte_count <= 0) return 1; // The comment is optional
  memmove(entry, entry+i, byte_count);

  for(i = 0; i < byte_count; i ++) {
    if((entry[i] == '\r') || (entry[i] == '\n')) break;
    comment[i] = entry[i];
  }
  comment[i] = 0; // Null terminate the string

  entry[0] = 0; // Reset the entry string
  return 1;     // No errors were encountered
}

GXDLCODE_API int FindVendor(const char *fname, const char *vendor_code, 
			    char *name, char *comment)
// Function used to find vendor information in a MAC code database file.
// Returns true if a match is found or false if a match could not be
// found or an error occurs. If successful the vendor name and any
// comments will be passed back in the "name" and "comment" variables.
{
  const int MaxLine = 255;
  char entry[MaxLine];
  char code[MaxLine];

  DiskFileB infile(fname, DiskFileB::df_READONLY, DiskFileB::df_NO_CREATE);
  if(!infile) return 0; // Cannot open the file
  
  while(!infile.df_EOF()) {
    // Clear the line buffer before each read
    for(int i = 0; i < MaxLine; i++) entry[i] = 0;
    
    // Read in the file line by line
    if(infile.df_GetLine(entry, MaxLine) != DiskFileB::df_NO_ERROR) {
      break; // Error reading file
    }

    if(ParseMACVendorEntry(entry, code, name, comment)) {
      if(code[0] != 0) { 
	if(CaseICmp(vendor_code, code) == 0) return 1;
      }
    }
  }

  infile.df_Close();
  return 0;
}

GXDLCODE_API int FindVendor(const char *fname, const gxMAC48 &mac, char *name, 
			    char *comment)
// Function used to find vendor information in a MAC code database file.
// Returns true if a match is found or false if a match could not be
// found or an error occurs. If successful the vendor name and any
// comments will be passed back in the "name" and "comment" variables.
{
  char sbuf[mac48MaxStrLen];
  return FindVendor(fname, mac.GetVendorCode(sbuf), name, comment);
}

#ifdef __BCC32__
#pragma warn .8080
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
