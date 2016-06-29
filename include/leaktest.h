// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: leaktest.h
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/09/2000 
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

Code used to monitor memory leaks within an MSVC application.
This program was taken from the Visual C++ Debug Routines
example program. The leak test functions will print an error
message to the console if the end of the application attempts to
overwrite the end of an allocated buffer or if any memory leaks
occur. To use this code the InitLeakTest() function must be called
in the program's main thread of execution. Additionally the
program must be compiled with the /MDd compiler flag the /MTd
compiler flag for multi-threaded applications.

Changes:
==============================================================
03/11/2002: By default C++ iostreams are no longer used. In order 
to use the iostream functions with leak test functions users must 
define __USE_CPP_IOSTREAM__ preprocessor directive. When using C++ 
iostreams users have the option to use the ANSI Standard C++ library 
by defining the __USE_ANSI_CPP__ preprocessor directive. By default 
the old iostream library will be used when the  __USE_CPP_IOSTREAM__ 
directive is defined.
==============================================================
*/
// ----------------------------------------------------------- //  
#ifndef __GX_LEAK_TEST_HPP__
#define __GX_LEAK_TEST_HPP__

#include "gxdlcode.h"

// NOTE: This macro must defined in the MSVC project makefile 
#if defined(__MSVC_DEBUG__)

#if defined (__USE_CPP_IOSTREAM__) // Using I/O stream debugging
#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__
#endif // __USE_CPP_IOSTREAM__

#include <crtdbg.h>

#ifndef _CRTDBG_MAP_ALLOC 
#define _CRTDBG_MAP_ALLOC 
#endif

GXDLCODE_API void OutputHeading(const char *explanation);
GXDLCODE_API void InitLeakTest();

#endif // __MSVC_DEBUG__

#endif // __GX_LEAK_TEST_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //




