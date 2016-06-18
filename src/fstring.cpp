// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: fstring.cpp
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

Fixed length string class used by database key types or members 
that must be fixed in length.

Changes:
==============================================================
10/05/2001: Modified the FString::SetString() function to clear 
the remaining bytes after the string's null terminator to ensure 
that the memory buffer is equal to other buffers with a copy of
this string.

05/05/2002: Modified the FString::Copy() function to use the 
FString::SetString() function when copying or assigning FString 
objects. This change was made to ensure that all bytes following 
string's null terminator are clear.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "fstring.h"

// Initialize FString Global Variables
GXDLCODE_API int FStringCaseCompare = 1;

void FString::Clear()
{
  for(unsigned i = 0; i < FStringLength; i++) sptr[i] = 0;
}

int FString::SetString(const char *s, unsigned bytes)
// Set the string value for this object. This function will truncate 
// the number of bytes requested if the number of bytes exceeds maximum
// fixed string length. Returns true if successful or false if the 
// string was truncated. NOTE: The FString class guarantees that each
// string object is unique by storing a unique copy of the string with
// each object. This ensures that FString objects can be safely copy
// constructed, assigned, resized, and deleted by multiple threads.
// Multiple threads accessing shared memory segments must be handled
// by the application.
{
  // Calculate the length of this string if no bytes size is specified
  if(bytes == 0) bytes = strlen(s);

  int truncate = 0;  
  if(bytes > FStringLength) {
    bytes = FStringLength;
    truncate = 1;
  }
  memmove(sptr, s, bytes);
  sptr[bytes] = 0; // Null terminate the string

  // 10/05/2001: Clear the bytes after the string's null terminator.
  unsigned i;
  unsigned num = FStringLength - bytes;
  char *ptr = sptr+bytes;
  for(i = 0; i < num; i++) {
    *ptr++ = 0;
  }

  return truncate == 0;
}

void FString::Copy(const FString &s)
// Function used to copy FString objects.
{
  SetString(s.sptr, s.length());
}

FString *FString::strdup()
// Returns a duplicate string object or a null value if an error
// occurs.
{
  return (FString *)new FString(sptr);
}

FString *FString::strdup() const
// Returns a duplicate string object or a null value if an error
// occurs.
{
  return (FString *)new FString(sptr);
}

GXDLCODE_API int operator==(const FString &a, const FString &b) 
{
  if(FStringCaseCompare)
    return strcmp(a.sptr, b.sptr) == 0;
  else
    return CaseICmp(a, b) == 0;
}

GXDLCODE_API int operator<(const FString &a, const FString &b) 
{
  if(FStringCaseCompare)
    return strcmp(a.sptr, b.sptr) < 0;
  else
    return CaseICmp(a, b) < 0;
}

GXDLCODE_API int operator>(const FString &a, const FString &b) 
{
  if(FStringCaseCompare)
    return strcmp(a.sptr, b.sptr) > 0;
  else
    return CaseICmp(a, b) > 0;
}

GXDLCODE_API int operator!=(const FString &a, const FString &b) 
{
  if(FStringCaseCompare)
    return strcmp(a.sptr, b.sptr) != 0;
  else
    return CaseICmp(a, b) != 0;
}

GXDLCODE_API int CaseICmp(const FString &s1, const FString &s2)
{
  return CaseICmp(s1.c_str(), s2.c_str()); 
}

GXDLCODE_API int CaseICmp(FString &s1, FString &s2)
{
  return CaseICmp(s1.c_str(), s2.c_str()); 
}

GXDLCODE_API int CaseICmp(const FString &s1, const char *s)
{
  return CaseICmp(s1.c_str(), s); 
}

GXDLCODE_API int CaseICmp(const char *s, const FString &s2)
{
  return CaseICmp(s, s2.c_str()); 
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
