// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxdtyp64.h
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
compatibility between the built-in 64-bit data type sizes on
various compilers.

Changes:
==============================================================
02/21/2002: Add preprocessor directive for the GNU MINGW32
compiler.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_DATABASE__64BIT_TYPES_HPP__
#define __GX_DATABASE__64BIT_TYPES_HPP__

#include "gxdlcode.h"

// --------------------------------------------------------------
// Platform/compiler specific INT64 types
// --------------------------------------------------------------
#if defined (__WIN32__) && defined (__MSVC__)
// MSVC version 4.2 and higher
#ifdef _lint
// PC-lint 04/19/2004: Using MS compiler implementations to avoid 
// PC-lint errors. The MS double underscore convention should not 
// be used as identifier names.
typedef __int64 __LLWORD__, *__LLWORD_PTR__; 
typedef unsigned __int64 __ULLWORD__, *ULLWORD_PTR__; 
#else
typedef _int64 __LLWORD__;           
typedef unsigned _int64 __ULLWORD__; 
#endif // _lint

#elif defined (__WIN32__) && defined (__BCC32__)
// BCC32 version 5.5 and higher
typedef __int64 __LLWORD__, *__LLWORD_PTR__; 
typedef unsigned __int64 __ULLWORD__, *ULLWORD_PTR__; 

#elif defined (__MINGW32__)
// MINGW GCC Compiler
typedef long long __LLWORD__;
typedef unsigned long long __ULLWORD__;

#elif defined (__UNIX__)
// Default for all UNIX variants. NOTE: Place specific UNIX variants 
// above this directive.
typedef long long __LLWORD__;
typedef unsigned long long __ULLWORD__;	

#else
#error You must define a target platform and/or compiler
#endif // INT64 types 
// --------------------------------------------------------------

#endif // __GX_DATABASE__64BIT_TYPES_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
