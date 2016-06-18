// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Header File Name: stdafx.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 05/25/2001  
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

Include file for standard system include files, or project specific 
include files that are used frequently, but are changed infrequently.

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

#if !defined(AFX_STDAFX_H__70213157_826C_11D5_8735_0002B328670A__INCLUDED_)
#define AFX_STDAFX_H__70213157_826C_11D5_8735_0002B328670A__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

// Insert your headers here
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers

#include <windows.h>

// TODO: reference additional headers your program requires here
#include "gxdlincs.h"

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__70213157_826C_11D5_8735_0002B328670A__INCLUDED_)

#endif // __MSVC_DLL__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
