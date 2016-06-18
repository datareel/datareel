// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxdtypes.h
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/05/1997  
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

This file contains typedefs and constants used to maintain
compatibility between the built-in data type sizes on various
compilers.
*/
// ----------------------------------------------------------- //   
#ifndef __GX_DATABASE__TYPES_HPP__
#define __GX_DATABASE__TYPES_HPP__

#include "gxdlcode.h"

// --------------------------------------------------------------
// Type definitions common to all platforms
// --------------------------------------------------------------
typedef char __SBYTE__;            // 8-bit signed value
typedef unsigned char __UBYTE__;   // 8-bit unsigned value
typedef int __WORD__;              // (W)ord size (4 bytes) 
typedef unsigned __UWORD__;        // (U)nsigned (W)ord size (4 bytes)
typedef long __LWORD__;            // (L)ong (W)ord size (4 bytes)
typedef unsigned long __ULWORD__;  // (U)nsigned (L)ong (W)ord size (4 Bytes)
typedef short __SWORD__;           // (S)hort (W)ord size (2 bytes)
typedef unsigned short __USWORD__; // (U)nsigned (S)hort (W)ord size (2 bytes) 
typedef double __DPFLOAT__; // (D)ouble (P)recision (F)loating point (8 bytes)
// --------------------------------------------------------------

// --------------------------------------------------------------
// Platform/compiler specific INT64 types
// --------------------------------------------------------------
#if defined (__64_BIT_DATABASE_ENGINE__) || defined (__ENABLE_64_BIT_INTEGERS__)
#include "gxdtyp64.h" 
#endif
// --------------------------------------------------------------

#endif // __GX_DATABASE__TYPES_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
