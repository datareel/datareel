
// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: stdafx.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 05/25/2001  
// Date Last Modified: 06/17/2016
// Copyright (c) 2001-2024 DataReel Software Development
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

Source file that includes just the standard includes. gxdlcode.pch 
will be the pre-compiled header stdafx.obj will contain the 
pre-compiled type information.

Changes:
==============================================================
01/23/2002: Added the __MSVC_DLL__ preprocessor directive, which 
is now required to compile the DLL library. This directive was added 
to allow the DLL code base to be combined with the UNIX library and 
static library code bases. 
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#ifdef __MSVC_DLL__

#include "stdafx.h"

// TODO: reference any additional headers you need in STDAFX.H
// and not in this file

#endif // __MSVC_DLL__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
