// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxdfp64.h
// Compiler Used: MSVC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/04/1997 
// Date Last Modified: 05/25/2016
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

The gxDatabase 64-bit file helpers are a collection of classes 
and standalone functions used to support large files on multiple
target platforms. NOTE: None of the data structures and 
functions defined are intended to be used directly. They are
used by the gxDatabase class to call the correct native 
64-bit file API function for each supported platform. 

NOTE: DataReel supports large files on Windows NT/2000/XP, 
RedHat 6.2 and higher, HPUX 11.X, and Solaris 2.8 and higher.

Under Windows NT/2000/XP NTFS formatted files systems the file 
size is limited only by the size of the volume. NTFS volume sizes 
range from 10MB to 2TB and larger. FAT32 volumes range from 512MB 
to 2TB with a maximum file size of 4GB. FAT16 volumes range from 
floppy disk size to 4GB with a maximum file size of 2GB.

The 64-bit UNIX API is based on the non-POSIX standard UNIX API
interfaces to support large files:

::fopen64()
::fseeko64()
::ftello64()
::stat64()

NOTE: Since these functions are not part of the POSIX standard
they may be removed in future compiler versions.

Changes:
==============================================================
03/18/2002: Changed the return types for the gxdFPTR64Read() and
gxdFPTR64Write() functions to return the number of bytes read and
moved rather than the returning 0/1 to indicate a pass/fail 
condition. This change was made to support additional classes
throughout the library.
==============================================================
*/
// ----------------------------------------------------------- //  
#ifndef __GX_DATABASE_FILE_POINTER_64_HPP__
#define __GX_DATABASE_FILE_POINTER_64_HPP__

#include "gxdlcode.h"

#if defined (__64_BIT_DATABASE_ENGINE__) || defined (__ENABLE_64_BIT_INTEGERS__)

// Non-platform specific include files
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include "gxdtypes.h"
#include "gxheader.h"

// Platform specific include files
#if defined (__WIN32__)
#include <io.h>
#include <windows.h>
#elif defined (__UNIX__)
// NOTE: Place specific UNIX variants above this directive.
#include <unistd.h> // UNIX standard function definitions
#else
#error You must define a target platform: __WIN32__ or __UNIX__
#endif

// Platform specific file pointer type
struct GXDLCODE_API gxdFPTR64 {
#if defined (__WIN32__) && defined (__MSVC__)
  // WIN32 file pointer type for MSVC
  HANDLE fptr;
#elif defined (__WIN32__) && defined (__BCC32__)
  // WIN32 file pointer type for BCC32
  HANDLE fptr;
#elif defined (__UNIX__) && defined (__LINUX__)
  // NOTE: Large file support for Linux is supported by RedHat
  // Linux 6.2 and higher.
  FILE *fptr;
#elif defined (__UNIX__) && defined (__HPUX__)
  FILE *fptr;
#elif defined (__UNIX__) && defined (__SOLARIS__)
  FILE *fptr;
#else
#error You must define a target platform and/or compiler
#endif
};

// NOTE: Any underlying 64-bit file system used must provide the basic
// functionality defined here.
GXDLCODE_API gxdFPTR64 *gxdFPTR64Create(const char *fname);
GXDLCODE_API gxdFPTR64 *gxdFPTR64Open(const char *fname, 
				      gxDatabaseAccessMode mode);
GXDLCODE_API int gxdFPTR64Close(gxdFPTR64 *stream);
GXDLCODE_API int gxdFPTR64Flush(gxdFPTR64 *stream);
GXDLCODE_API __ULWORD__ gxdFPTR64Read(gxdFPTR64 *stream, void *buf, 
				      __ULWORD__ bytes);
GXDLCODE_API __ULWORD__ gxdFPTR64Write(gxdFPTR64 *stream, const void *buf, 
				       __ULWORD__ bytes);
GXDLCODE_API __LLWORD__ gxdFPTR64Seek(gxdFPTR64 *stream, __LLWORD__,
				      gxDatabaseSeekMode mode);
GXDLCODE_API __LLWORD__ gxdFPTR64Tell(gxdFPTR64 *stream);
GXDLCODE_API int gxdFPTR64Exists(const char *fname);
GXDLCODE_API __LLWORD__ gxdFPTR64FileSize(const char *fname);

#endif // __64_BIT_DATABASE_ENGINE__ || __ENABLE_64_BIT_INTEGERS__

#endif // __GX_DATABASE_FILE_POINTER_64_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
