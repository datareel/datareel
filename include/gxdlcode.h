// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Header File Name: gxdlcode.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 05/25/2001  
// Date Last Modified: 06/29/2016
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

This include file is used by all platforms to define macros required 
to build static and dynamic libraries. The gxdlcode.h file is included 
in every include file extending the visibility of the macros defined 
here to the every source code file. 

Changes:
==============================================================
01/23/2002: Added the __MSVC_DLL__ preprocessor directive, which 
is now required to compile the DLL library. This directive was added 
to allow the DLL code base to be combined with the UNIX library and 
static library code bases. 

02/28/2002: Added the __USE_ANSI_CPP__ preprocessor directive
used to force compatibility with the new ANSI Standard C++ 
library. 
==============================================================
*/
// ----------------------------------------------------------- // 
#ifndef __GX_DLL_CODE_HPP__
#define __GX_DLL_CODE_HPP__

#if defined (__MSVC_DLL__) // Defines the entry point for the DLL application
// The following ifdef block is the standard way of creating macros which 
// make exporting from a DLL simpler. All files within this DLL are compiled 
// with the __GXDLCODE_EXPORTS__  symbol defined on the command line. This 
// symbol should not be defined on any project that uses this DLL. This way 
// any other project whose source files include this file see GXDLCODE_API 
// functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef GXDLCODE_EXPORTS
#define GXDLCODE_API __declspec(dllexport)
#else
#define GXDLCODE_API __declspec(dllimport)
#endif
#else // Macro used for non DLL code
#define GXDLCODE_API
#endif // __MSVC_DLL__

// Force ANSI compatibility for Visual C++ .NET
#if defined (__MSDEVNET__) && !defined (__USE_ANSI_CPP__)
#define __USE_ANSI_CPP__
#endif // __MSDEVNET__

// Force ANSI compatibility for BCC32
#if defined (__BCC32__) && !defined (__USE_ANSI_CPP__)
#define __USE_ANSI_CPP__
#endif // __BCC32__

#if defined (__USE_ANSI_CPP__)
// Macro used to define the std name space used by the ANSI 
// Standard C++ library.
#define GXSTD std
#else // Use the old Standard C++ library by default  
#define GXSTD
#endif // __USE_ANSI_CPP__

// Windows CE macros
#if defined(_WIN32_WCE)
#define __WINCE__
#endif // Windows CE macros

#endif // __GX_DLL_CODE_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
