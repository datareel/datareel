// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxstring.h
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999  
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

The gxString and gxStringw classes are supplied with the library 
to be used in place of null-terminated ASCII and Unicode character 
strings.

Changes:
==============================================================
06/05/2002: Added the gxStringw type definition for the Unicode
string class.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_STRING_HPP__
#define __GX_STRING_HPP__

#include "gxdlcode.h"

#include "ustring.h"

// This type definition allows you to substitute another string
// class for the UString class if needed.
typedef UString gxString;   // ASCII string class
typedef UStringw gxStringw; // Unicode string class

#endif  // __GX_STRING_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //

