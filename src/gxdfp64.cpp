// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxdfp64.cpp
// Compiler Used: MSVC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/04/1997 
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

The gxDatabase 64-bit file helpers are a collection of classes 
and standalone functions used to support large files on multiple
target platforms. NOTE: None of the data structures and 
functions defined are intended to be used directly. They are
used by the gxDatabase class to call the correct native 
64-bit file API function for each supported platform.

Changes:
==============================================================
01/30/2002: Modified all WIN32 functions to use the WIN32 file 
I/O API, which will provide large file support for both MSVC 
and BCC32. WIN32 file I/O also adds enhanced NTFS compatibility  
to all WIN32 functions.

01/30/2002: Fixed potential memory leak in the gxdFPTR64Create()
and gxdFPTR64Open() functions.

02/02/2002: Modified all versions of the file pointer close functions 
to set the stream pointer to zero after releasing it. This ensures that 
the caller will not use the stream pointer after it has been deleted 
provided the calling function tests for a null stream pointer prior 
to use. 

03/18/2002: Changed the return types for the gxdFPTR64Read() and
gxdFPTR64Write() functions to return the number of bytes read and
moved rather than the returning 0/1 to indicate a pass/fail 
condition. This change was made to support additional classes
throughout the library.

12/02/2002: Modified the following functions to use string conversion 
macros under MSVC when the _UNICODE preprocessor directive is defined:
gxdFPTR64Create()
gxdFPTR64Open()
gxdFPTR64Exists()
gxdFPTR64FileSize()
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__64_BIT_DATABASE_ENGINE__) || defined (__ENABLE_64_BIT_INTEGERS__)

#include "gxdfp64.h"

#ifdef __BCC32__
#pragma warn -8080
#endif

GXDLCODE_API gxdFPTR64 *gxdFPTR64Create(const char *fname)
// Create a new file and truncate existing files.
// Returns a null value if the file cannot be
// created.
{
  gxdFPTR64 *stream = new gxdFPTR64;
  if(!stream) return 0;
#if defined (__WIN32__)

#ifdef _UNICODE
  USES_CONVERSION;
  stream->fptr = CreateFile(A2T((char*)fname), 
			    GENERIC_READ|GENERIC_WRITE, 
			    FILE_SHARE_READ|FILE_SHARE_WRITE, 
			    NULL, // No security
			    CREATE_ALWAYS, 
			    FILE_ATTRIBUTE_NORMAL|FILE_FLAG_RANDOM_ACCESS,
			    NULL // No attr. template 
			    );
#else
  stream->fptr = CreateFile(fname, 
			    GENERIC_READ|GENERIC_WRITE, 
			    FILE_SHARE_READ|FILE_SHARE_WRITE, 
			    NULL, // No security
			    CREATE_ALWAYS, 
			    FILE_ATTRIBUTE_NORMAL|FILE_FLAG_RANDOM_ACCESS,
			    NULL // No attr. template 
			    );
#endif // String Conversion Macros

  if(stream->fptr == INVALID_HANDLE_VALUE) {
    delete stream;
    return 0;
  }
#elif defined (__UNIX__)
  // Non-POSIX standard API interfaces to support large files.
  stream->fptr = fopen64(fname, "w+");
  if(stream->fptr == 0) {
    delete stream; // 01/30/2002: Prevent memory leak
    return 0;
  }
#else
#error You must define a target platform and/or compiler
#endif
  return stream;
}

GXDLCODE_API gxdFPTR64 *gxdFPTR64Open(const char *fname, 
				      gxDatabaseAccessMode mode)
// Open an existing file. The "mode" variable determines if the file
// is opened for read only or read/write access. Returns a null value
// if the specified file cannot be opened. NOTE: This version of the
// open functions will only accept: gxDBASE_READONLY and gxDBASE_READWRITE
// access modes.
{
  gxdFPTR64 *stream = new gxdFPTR64;
  if(!stream) return 0;
#if defined (__WIN32__) && defined (_UNICODE)
  USES_CONVERSION;
#endif

  if(mode == gxDBASE_READONLY) { // Open for read only access
#if defined (__WIN32__)
#ifdef _UNICODE
    stream->fptr = CreateFile(A2T((char*)fname), 
			      GENERIC_READ,
			      FILE_SHARE_READ|FILE_SHARE_WRITE, 
			      NULL, // No security
			      OPEN_EXISTING, 
			      FILE_ATTRIBUTE_NORMAL|FILE_FLAG_RANDOM_ACCESS, 
			      NULL // No attr. template 
			      );
#else
    stream->fptr = CreateFile(fname, 
			      GENERIC_READ,
			      FILE_SHARE_READ|FILE_SHARE_WRITE, 
			      NULL, // No security
			      OPEN_EXISTING, 
			      FILE_ATTRIBUTE_NORMAL|FILE_FLAG_RANDOM_ACCESS, 
			      NULL // No attr. template 
			      );
#endif // String Conversion Macros

    if(stream->fptr == INVALID_HANDLE_VALUE) {
      delete stream;
      return 0;
    }
    
#elif defined (__UNIX__)
    // Non-POSIX standard API interfaces to support large files.
    stream->fptr = fopen64(fname, "r");
    if(stream->fptr == 0) {
      delete stream; // 01/30/2002: Prevent memory leak
      return 0;
    }
#else
#error You must define a target platform and/or compiler
#endif
  }
  else { // Open for read/write access
#if defined (__WIN32__)
#ifdef _UNICODE
    stream->fptr = CreateFile(A2T((char*)fname), 
			      GENERIC_READ|GENERIC_WRITE, 
			      FILE_SHARE_READ|FILE_SHARE_WRITE, 
			      NULL, // No security 
			      OPEN_EXISTING, 
			      FILE_ATTRIBUTE_NORMAL|FILE_FLAG_RANDOM_ACCESS, 
			      NULL // No attr. template 
			      );
#else
    stream->fptr = CreateFile(fname, 
			      GENERIC_READ|GENERIC_WRITE, 
			      FILE_SHARE_READ|FILE_SHARE_WRITE, 
			      NULL, // No security 
			      OPEN_EXISTING, 
			      FILE_ATTRIBUTE_NORMAL|FILE_FLAG_RANDOM_ACCESS, 
			      NULL // No attr. template 
			      );
#endif // String Conversion Macros

    if(stream->fptr == INVALID_HANDLE_VALUE) {
      delete stream;
      return 0;
    }

#elif defined (__UNIX__)
    // Non-POSIX standard API interfaces to support large files.
    stream->fptr = fopen64(fname, "r+");
    if(stream->fptr == 0) {
      delete stream; // 01/30/2002: Prevent memory leak
      return 0;
    }
#else
#error You must define a target platform and/or compiler
#endif
  }
  return stream;
}

GXDLCODE_API int gxdFPTR64Close(gxdFPTR64 *stream)
// Close an open file. Returns a non-zero value to
// indicate an error condition or zero if successful.
{
  if(!stream) return 0; // The file stream is not open
#if defined (__WIN32__)
  if(!CloseHandle(stream->fptr)) return 1;
#elif defined (__UNIX__)
  if(fclose(stream->fptr) != 0) return 1;
#else
#error You must define a target platform and/or compiler
#endif
  delete stream;
  stream = 0; // 02/02/2002: Set pointer to zero after releasing it
  return 0;
}

GXDLCODE_API int gxdFPTR64Flush(gxdFPTR64 *stream)
// Flush the any open disk buffers. Returns
// a non-zero value to indicate an error
// condition or zero if successful. 
{
  if(!stream) return 0; // The file stream is not open
#if defined (__WIN32__)
  if(!FlushFileBuffers(stream->fptr)) {
    return 1;
  }
#elif defined (__UNIX__)
  if(fflush(stream->fptr) != 0) return 1;
#else
#error You must define a target platform and/or compiler
#endif
  return 0;
}

GXDLCODE_API __ULWORD__ gxdFPTR64Read(gxdFPTR64 *stream, void *buf, 
				      __ULWORD__ bytes)
// Read a specified number of bytes from the current file position
// into a memory buffer. Returns the total number if bytes read from the
// file.
{
  if(!stream) return (__ULWORD__)0; // The file stream is not open
  
#if defined (__WIN32__)
  DWORD bytes_read; 
  if(!ReadFile(stream->fptr, (LPVOID)buf, (DWORD)bytes, &bytes_read, NULL)) {
    return (__ULWORD__)0;
  }
  return (__ULWORD__)bytes_read;

#elif defined (__UNIX__)
  return (__ULWORD__)fread((unsigned char *)buf, 1, bytes, stream->fptr);
#else
#error You must define a target platform and/or compiler
#endif
}

GXDLCODE_API __ULWORD__ gxdFPTR64Write(gxdFPTR64 *stream, const void *buf, 
				       __ULWORD__ bytes) 
// Write a specific number of bytes from a memory buffer to the
// current file position. Returns the total number if bytes written
// to the file.
{
  if(!stream) return (__ULWORD__)0; // The file stream is not open
  
#if defined (__WIN32__)
  DWORD bytes_moved; 
  if(!WriteFile(stream->fptr, (LPVOID)buf, (DWORD)bytes, &bytes_moved, NULL)) {
    return (__ULWORD__)0;
  }
  return (__ULWORD__)bytes_moved;

#elif defined (__UNIX__)
 return (__ULWORD__)fwrite((const unsigned char *)buf, 1, bytes, stream->fptr);
#else
#error You must define a target platform and/or compiler
#endif
 // 06/03/2002: Removed unneeded return 0 to eliminate BCC32 warning
}

GXDLCODE_API __LLWORD__ gxdFPTR64Seek(gxdFPTR64 *stream, __LLWORD__ offset,
				      gxDatabaseSeekMode mode)
// Seek to the specified offset starting at the beginning (gxDBASE_SEEK_SET),
// end (gxDBASE_SEEK_END) or current offset (gxDBASE_SEEK_CUR). Returns a 
// negative 1 to indicate an error condition or the current stream position 
// if successful.
{
  if(!stream) return (__LLWORD__)-1; // The file stream is not open

#if defined (__WIN32__)
  LARGE_INTEGER li;
  li.QuadPart = offset;

  if(mode == gxDBASE_SEEK_BEG) {
    li.LowPart = SetFilePointer(stream->fptr, li.LowPart, 
				&li.HighPart, 
				FILE_BEGIN);
  }
  else if(mode == gxDBASE_SEEK_CUR) {
    li.LowPart = SetFilePointer(stream->fptr, li.LowPart, 
				&li.HighPart, 
				FILE_CURRENT);
  }
  else if(mode == gxDBASE_SEEK_END) {
    li.LowPart = SetFilePointer(stream->fptr, li.LowPart, 
				&li.HighPart, 
				FILE_END);
  }
  else { // An invalid seek mode was specified
    return (__LLWORD__)-1;
  }

  // Check for seek errors
  if(li.LowPart == 0xFFFFFFFF && GetLastError() != NO_ERROR) {
    li.QuadPart = (__LLWORD__)-1;
  }
  return (__LLWORD__)li.QuadPart;

#elif defined (__UNIX__)
  if(mode == gxDBASE_SEEK_BEG) {
    return fseeko64(stream->fptr, offset, SEEK_SET);
  }
  else if(mode == gxDBASE_SEEK_CUR) {
    return fseeko64(stream->fptr, offset, SEEK_CUR);
  }
  else if(mode == gxDBASE_SEEK_END) {
    return fseeko64(stream->fptr, offset, SEEK_CUR);
  }
  else { // An invalid seek mode was specified
    return (__LLWORD__)-1;
  }
#else
#error You must define a target platform and/or compiler
#endif
}

GXDLCODE_API __LLWORD__ gxdFPTR64Tell(gxdFPTR64 *stream)
{
  if(!stream) return (__LLWORD__)-1; // The file stream is not open
  
#if defined (__WIN32__)
  LARGE_INTEGER li;
  li.QuadPart = (__LLWORD__)0;
  li.LowPart = SetFilePointer(stream->fptr, li.LowPart, 
			      &li.HighPart, 
			      FILE_CURRENT);
  // Check for errors
  if(li.LowPart == 0xFFFFFFFF && GetLastError() != NO_ERROR) {
    li.QuadPart = (__LLWORD__)-1;
  }
  
  return (__LLWORD__)li.QuadPart;

#elif defined (__UNIX__)
  // Non-POSIX standard API interfaces to support large files.
  return ftello64(stream->fptr);
#else
#error You must define a target platform and/or compiler
#endif
}

GXDLCODE_API int gxdFPTR64Exists(const char *fname)
// Returns true if the specified file exists.
{
#if defined (__WIN32__)
#ifdef _UNICODE
  USES_CONVERSION;
  HANDLE hFile = CreateFile(A2T((char*)fname), 
			    GENERIC_READ,
			    FILE_SHARE_READ|FILE_SHARE_WRITE, 
			    NULL, // No security
			    OPEN_EXISTING, 
			    FILE_ATTRIBUTE_NORMAL, 
			    NULL // No attr. template
			    );
#else
  HANDLE hFile = CreateFile(fname, 
			    GENERIC_READ,
			    FILE_SHARE_READ|FILE_SHARE_WRITE, 
			    NULL, // No security
			    OPEN_EXISTING, 
			    FILE_ATTRIBUTE_NORMAL, 
			    NULL // No attr. template
			    );
#endif // String Conversion Macros

  if(hFile == INVALID_HANDLE_VALUE) {
    return 0;
  }

  CloseHandle(hFile);
  return 1;

#elif defined (__UNIX__)
  // Non-POSIX standard API interfaces to support large files.
  struct stat64 buf;
  return stat64(fname, &buf) == (__LLWORD__)0;
#else
#error You must a target platform and/or compiler
#endif
}

GXDLCODE_API __LLWORD__ gxdFPTR64FileSize(const char *fname)
// Returns the length of the specified file or negative 1
// to indicate an error condition.
{
#if defined (__WIN32__)
#ifdef _UNICODE
  USES_CONVERSION;
  HANDLE hFile = CreateFile(A2T((char*)fname), 
			    GENERIC_READ,
			    FILE_SHARE_READ|FILE_SHARE_WRITE, 
			    NULL, // No security
			    OPEN_EXISTING, 
			    FILE_ATTRIBUTE_NORMAL, 
			    NULL // No attr. template 
			    );
#else
  HANDLE hFile = CreateFile(fname, 
			    GENERIC_READ,
			    FILE_SHARE_READ|FILE_SHARE_WRITE, 
			    NULL, // No security
			    OPEN_EXISTING, 
			    FILE_ATTRIBUTE_NORMAL, 
			    NULL // No attr. template 
			    );
#endif // String Conversion Macros

  // Could not open the specified file
  if(hFile == INVALID_HANDLE_VALUE) {
    return (__LLWORD__)-1;
  }

  BY_HANDLE_FILE_INFORMATION info;

  if(GetFileInformationByHandle(hFile, &info) == 0) {
    return (__LLWORD__)-1;
  }

  LARGE_INTEGER li;
  li.HighPart = info.nFileSizeHigh;
  li.LowPart = info.nFileSizeLow;

  CloseHandle(hFile);
  return (__LLWORD__)li.QuadPart;

#elif defined (__UNIX__)
    // Non-POSIX standard API interfaces to support large files.
    struct stat64 buf;
    if(stat64(fname, &buf) != (__LLWORD__)0) return (__LLWORD__)-1;
    return buf.st_size;
#else
#error You must a target platform and/or compiler
#endif
}

#ifdef __BCC32__
#pragma warn .8080
#endif

#endif // __64_BIT_DATABASE_ENGINE__ || __ENABLE_64_BIT_INTEGERS__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
