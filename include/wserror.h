// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: wserror.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
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

Standalone functions that can optionally be used with the
gxSocket class to display extended WinSock error information
following a socket error.
*/
// ----------------------------------------------------------- //   
#ifndef __GX_WINSOCK_ERROR_HPP__
#define __GX_WINSOCK_ERROR_HPP__

#include "gxdlcode.h"

GXDLCODE_API int WinSockExceptionNumber();
GXDLCODE_API void WinSockSetException(int err);
GXDLCODE_API char *WinSockExceptionMessage();
GXDLCODE_API char *WinSockExceptionMessage(int wserror);

#endif // __GX_WINSOCK_ERROR_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
