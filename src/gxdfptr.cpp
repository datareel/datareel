// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxdfptr.cpp
// Compiler Used: MSVC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/04/1997 
// Date Last Modified: 05/25/2016
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

The gxDatabase file pointer routines are a collection of classes 
and standalone functions used to define the underlying file system
used by the gxDatabase class. NOTE: None of the data structures
and functions defined here are intended to be used directly. They
are used by the gxDatabase class to call the correct native file
API function for each supported platform.

Changes:
==============================================================
01/30/2002: Fixed potential memory leak in the gxdFPTRCreate()
and gxdFPTROpen() functions.

02/01/2002: Add enhanced NTFS compatibility to all WIN32 functions 
by adding WIN32 file I/O. Define the __WIN32__ and __NTFS__ 
preprocessor directives to use the WIN32 file I/O API instead of 
the stdio file I/O.  

02/02/2002: Modified all versions of the file pointer close functions 
to set the stream pointer to zero after releasing it. This ensures that 
the caller will not use the stream pointer after it has been deleted 
provided the calling function tests for a null stream pointer prior 
to use. 

03/18/2002: Changed the return types for the gxdFPTRRead() and
gxdFPTRWrite() functions to return the number of bytes read and
moved rather than the returning 0/1 to indicate a pass/fail 
condition. This change was made to support additional classes
throughout the library.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (_USE_DIRECT_IO_) && defined (__LINUX__)
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#if defined USE_IOBUF_MACROS
#include <iobuf.h>
#endif
#endif

#include "gxdfptr.h"

#ifdef __BCC32__
#pragma warn -8080
#endif

GXDLCODE_API gxdFPTR *gxdFPTRCreate(const char *fname)
// Create a new file and truncate existing files.
// Returns a null value if the file cannot be
// created.
{
#if defined (__64_BIT_DATABASE_ENGINE__)
  return gxdFPTR64Create(fname);

#elif defined (__WIN32__) && defined (__NTFS__)
  gxdFPTR *stream = new gxdFPTR;
  if(!stream) return 0;
  stream->fptr = CreateFile(fname, 
			    GENERIC_READ|GENERIC_WRITE, 
			    FILE_SHARE_READ|FILE_SHARE_WRITE, 
			    NULL, // No security
			    CREATE_ALWAYS, 
			    FILE_ATTRIBUTE_NORMAL|FILE_FLAG_RANDOM_ACCESS,
			    NULL // No attr. template 
			    );

  if(stream->fptr == INVALID_HANDLE_VALUE) {
    delete stream;
    return 0;
  }
  return stream;

#elif defined (_USE_DIRECT_IO_) && defined (__LINUX__)
  gxdFPTR *stream = new gxdFPTR;
  if(!stream) return 0;
  stream->fptr = dio_fcreate(fname);
  if(stream->fptr == 0) {
    delete stream;
    return 0;
  }
  return stream;

#else // Use the 32-bit stdio version by default
  gxdFPTR *stream = new gxdFPTR;
  if(!stream) return 0;
  stream->fptr = fopen(fname, "w+b");
  if(stream->fptr == 0) {
    delete stream; // 01/30/2002: Prevent memory leak
    return 0;
  }
  return stream;
#endif
}

GXDLCODE_API gxdFPTR *gxdFPTROpen(const char *fname, gxDatabaseAccessMode mode)
// Open an existing file. The "mode" variable determines if the file
// is opened for read only or read/write access. Returns a null value
// if the specified file cannot be opened. NOTE: This version of the
// open functions will only accept: gxDBASE_READONLY and gxDBASE_READWRITE
// access modes.
{
#if defined (__64_BIT_DATABASE_ENGINE__)
  return gxdFPTR64Open(fname, mode);

#elif defined (__WIN32__) && defined (__NTFS__)
  gxdFPTR *stream = new gxdFPTR;
  if(!stream) return 0;
  if(mode == gxDBASE_READONLY) { // Open for read only access
    stream->fptr = CreateFile(fname, 
			      GENERIC_READ,
			      FILE_SHARE_READ|FILE_SHARE_WRITE, 
			      NULL, // No security
			      OPEN_EXISTING, 
			      FILE_ATTRIBUTE_NORMAL|FILE_FLAG_RANDOM_ACCESS, 
			      NULL // No attr. template 
			      );
    if(stream->fptr == INVALID_HANDLE_VALUE) {
      delete stream;
      return 0;
    }
  }
  else { // Open for read/write access
    stream->fptr = CreateFile(fname, 
			      GENERIC_READ|GENERIC_WRITE, 
			      FILE_SHARE_READ|FILE_SHARE_WRITE, 
			      NULL, // No security 
			      OPEN_EXISTING, 
			      FILE_ATTRIBUTE_NORMAL|FILE_FLAG_RANDOM_ACCESS, 
			      NULL // No attr. template 
			      );

    if(stream->fptr == INVALID_HANDLE_VALUE) {
      delete stream;
      return 0;
    }
  }
  return stream;

#elif defined (_USE_DIRECT_IO_) && defined (__LINUX__)
  gxdFPTR *stream = new gxdFPTR;
  if(!stream) return 0;

  if(mode == gxDBASE_READONLY) { // Open for read only access
    stream->fptr = dio_fopen(fname);
    if(stream->fptr == 0) {
      delete stream;
      return 0;
    }
  }
  else { // Open for read/write access
    stream->fptr = dio_fopenrw(fname);
    if(stream->fptr == 0) {
      delete stream; // 01/30/2002: Prevent memory leak
      return 0;
    }
  }
  return stream;

#else // Use the 32-bit stdio version by default
  gxdFPTR *stream = new gxdFPTR;
  if(!stream) return 0;

  if(mode == gxDBASE_READONLY) { // Open for read only access
    stream->fptr = fopen(fname, "rb");
    if(stream->fptr == 0) {
      delete stream; // 01/30/2002: Prevent memory leak
      return 0;
    }
  }
  else { // Open for read/write access
    stream->fptr = fopen(fname, "r+b");
    if(stream->fptr == 0) {
      delete stream; // 01/30/2002: Prevent memory leak
      return 0;
    }
  }
  return stream;
#endif
}

GXDLCODE_API int gxdFPTRClose(gxdFPTR *stream)
// Close an open file. Returns a non-zero value to
// indicate an error condition or zero if successful.
{
  if(!stream) return 0; // The file stream is not open

#if defined (__64_BIT_DATABASE_ENGINE__)
  return gxdFPTR64Close(stream);

#elif defined (__WIN32__) && defined (__NTFS__)
  if(!CloseHandle(stream->fptr)) return 1;
  delete stream;
  stream = 0;
  return 0;

#elif defined (_USE_DIRECT_IO_) && defined (__LINUX__)
  if(dio_close(stream->fptr) != 0) return 1;
  delete stream;
  stream = 0; // 02/02/2002: Set pointer to zero after releasing it
  return 0;

#else // Use the 32-bit stdio version by default
  if(fclose(stream->fptr) != 0) return 1;
  delete stream;
  stream = 0; // 02/02/2002: Set pointer to zero after releasing it
  return 0;
#endif
}

GXDLCODE_API int gxdFPTRFlush(gxdFPTR *stream)
// Flush the any open disk buffers. Returns
// a non-zero value to indicate an error
// condition or zero if successful. 
{
  if(!stream) return 0; // The file stream is not open

#if defined (__64_BIT_DATABASE_ENGINE__)
  return gxdFPTR64Flush(stream);

#elif defined (__WIN32__) && defined (__NTFS__)
  if(!FlushFileBuffers(stream->fptr)) return 1;
  return 0;

#elif defined (_USE_DIRECT_IO_) && defined (__LINUX__)
  if(dio_flush(stream->fptr) != 0) return 1;
  return 0;

#else // Use the 32-bit stdio version by default
  if(fflush(stream->fptr) != 0) return 1;
  return 0;
#endif
}

GXDLCODE_API __ULWORD__ gxdFPTRRead(gxdFPTR *stream, void *buf, 
				    __ULWORD__ bytes)
// Read a specified number of bytes from the current file position
// into a memory buffer. Returns the total number if bytes read from the
// file.
{
  if(!stream) return (__ULWORD__)0; // The file stream is not open

#if defined (__64_BIT_DATABASE_ENGINE__)
  return gxdFPTR64Read(stream, buf, bytes);

#elif defined (__WIN32__) && defined (__NTFS__)
  DWORD bytes_read; 
  if(!ReadFile(stream->fptr, (LPVOID)buf, (DWORD)bytes, &bytes_read, NULL)) {
    return (__ULWORD__)0;
  }
  return (__ULWORD__)bytes_read;

#elif defined (_USE_DIRECT_IO_) && defined (__LINUX__)
  return (__ULWORD__)dio_read(stream->fptr, (unsigned char *)buf, bytes);

#else // Use the 32-bit stdio version by default
  return (__ULWORD__)fread((unsigned char *)buf, 1, bytes, stream->fptr);
#endif
}

GXDLCODE_API __ULWORD__ gxdFPTRWrite(gxdFPTR *stream, const void *buf, 
				    __ULWORD__ bytes) 
// Write a specific number of bytes from a memory buffer to the
// current file position. Returns the total number if bytes written
// to the file.
{
  if(!stream) return (__ULWORD__)0; // The file stream is not open

#if defined (__64_BIT_DATABASE_ENGINE__)
  return gxdFPTR64Write(stream, buf, bytes);

#elif defined (__WIN32__) && defined (__NTFS__)
  DWORD bytes_moved; 
  if(!WriteFile(stream->fptr, (LPVOID)buf, (DWORD)bytes, &bytes_moved, NULL)) {
    return (__ULWORD__)0;
  }
  return (__ULWORD__)bytes_moved;

#elif defined (_USE_DIRECT_IO_) && defined (__LINUX__)
  return (__ULWORD__)dio_write(stream->fptr, (const unsigned char *)buf, bytes);

#else // Use the 32-bit stdio version by default
 return (__ULWORD__)fwrite((const unsigned char *)buf, 1, bytes, stream->fptr);
#endif
}

GXDLCODE_API FAU_t gxdFPTRSeek(gxdFPTR *stream, FAU_t offset,
			       gxDatabaseSeekMode mode)
// Seek to the specified offset starting at the beginning (gxDBASE_SEEK_SET),
// end (gxDBASE_SEEK_END) or current offset (gxDBASE_SEEK_CUR). Returns a 
// negative 1 to indicate an error condition or the current stream position 
// if successful.
{
  if(!stream) return (FAU_t)-1; // The file stream is not open

#if defined (__64_BIT_DATABASE_ENGINE__)
  return gxdFPTR64Seek(stream, offset, mode);

#elif defined (__WIN32__) && defined (__NTFS__)
  if(mode == gxDBASE_SEEK_BEG) {
    return (__LWORD__)SetFilePointer(stream->fptr, offset, NULL, FILE_BEGIN);
  }
  else if(mode == gxDBASE_SEEK_CUR) {
    return (__LWORD__)SetFilePointer(stream->fptr, offset, NULL, FILE_CURRENT);
  }
  else if(mode == gxDBASE_SEEK_END) {
    return (__LWORD__)SetFilePointer(stream->fptr, offset, NULL, FILE_END);
  }
  else { // An invalid seek mode was specified
    return (__LWORD__)-1;
  }

#elif defined (_USE_DIRECT_IO_) && defined (__LINUX__)
  if(mode == gxDBASE_SEEK_BEG) {
    return dio_seek(stream->fptr, offset, SEEK_SET);
  }
  else if(mode == gxDBASE_SEEK_CUR) {
    return dio_seek(stream->fptr, offset, SEEK_CUR);
  }
  else if(mode == gxDBASE_SEEK_END) {
    return dio_seek(stream->fptr, offset, SEEK_CUR);
  }
  else { // An invalid seek mode was specified
    return (FAU_t)-1;
  }

#else // Use the 32-bit stdio version by default
  if(mode == gxDBASE_SEEK_BEG) {
    return fseek(stream->fptr, offset, SEEK_SET);
  }
  else if(mode == gxDBASE_SEEK_CUR) {
    return fseek(stream->fptr, offset, SEEK_CUR);
  }
  else if(mode == gxDBASE_SEEK_END) {
    return fseek(stream->fptr, offset, SEEK_CUR);
  }
  else { // An invalid seek mode was specified
    return (FAU_t)-1;
  }
#endif
}

GXDLCODE_API FAU_t gxdFPTRTell(gxdFPTR *stream)
{
  if(!stream) return (FAU_t)-1; // The file stream is not open

#if defined (__64_BIT_DATABASE_ENGINE__)
  return gxdFPTR64Tell(stream);

#elif defined (__WIN32__) && defined (__NTFS__)
  return (__LWORD__)SetFilePointer(stream->fptr, 0, NULL, FILE_CURRENT);

#elif defined (_USE_DIRECT_IO_) && defined (__LINUX__)
  return dio_tell(stream->fptr);

#else // Use the 32-bit stdio version by default
  return ftell(stream->fptr);
#endif
}

GXDLCODE_API int gxdFPTRExists(const char *fname)
// Returns true if the specified file exists.
{
#if defined (__64_BIT_DATABASE_ENGINE__)
  return gxdFPTR64Exists(fname);

#elif defined (__WIN32__) && defined (__NTFS__)
  HANDLE hFile = CreateFile(fname, 
			    GENERIC_READ,
			    FILE_SHARE_READ|FILE_SHARE_WRITE, 
			    NULL, // No security
			    OPEN_EXISTING, 
			    FILE_ATTRIBUTE_NORMAL, 
			    NULL // No attr. template
			    );

  if(hFile == INVALID_HANDLE_VALUE) {
    return 0;
  }

  CloseHandle(hFile);
  return 1;

#else // Use the 32-bit stdio version by default
  struct stat buf;
  return stat(fname, &buf) == (FAU_t)0;
#endif
}

GXDLCODE_API FAU_t gxdFPTRFileSize(const char *fname)
// Returns the length of the specified file or negative 1
// to indicate an error condition.
{
#if defined (__64_BIT_DATABASE_ENGINE__)
  return gxdFPTR64FileSize(fname);

#elif defined (__WIN32__) && defined (__NTFS__)
  HANDLE hFile = CreateFile(fname, 
			    GENERIC_READ,
			    FILE_SHARE_READ|FILE_SHARE_WRITE, 
			    NULL, // No security
			    OPEN_EXISTING, 
			    FILE_ATTRIBUTE_NORMAL, 
			    NULL // No attr. template 
			    );

  // Could not open the specified file
  if(hFile == INVALID_HANDLE_VALUE) {
    return (__LWORD__)-1;
  }

  BY_HANDLE_FILE_INFORMATION info;

  if(GetFileInformationByHandle(hFile, &info) == 0) {
    return (__LWORD__)-1;
  }
  CloseHandle(hFile);
  return (__LWORD__)info.nFileSizeLow;;

#else // Use the 32-bit stdio version by default
  struct stat buf;
  int result = stat(fname, &buf);
  if(result != 0) return -1;
  return buf.st_size;
#endif
}

#if defined (_USE_DIRECT_IO_) && defined (__LINUX__)
DIOFILE * dio_open(const char  *filename,  int  access,  int  permission)
{
  DIOFILE *fp = new DIOFILE;
  int fd = POSIX_OPEN(filename, access, permission);
  if(fd < 0) return 0;
  fp->fd = fd;
  fp->access = access;
  fp->perm = permission;
  fp->fname = new char[(strlen(filename)+1)];
  memset(fp->fname,0,strlen(filename)+1);
  strcpy(fp->fname, filename); 
  fp->read_eof = -1;
  fp->seek_past_eof = -1;
  fp->fill_holes = 0;
  fp->error_flag = 0;
  return fp;
}

int dio_close(DIOFILE *fp)
{
  int rv;
  if(!fp) return -1;
  rv = POSIX_CLOSE(fp->fd);
  if(rv < 0) return rv;
  if(fp->fname) delete fp->fname;
  delete fp;
  fp = 0;
  return 0;
}

DIOFILE * dio_fopen(const char  *filename)
{
  return dio_open(filename, O_RDONLY, 0444);
}

DIOFILE * dio_fcreate(const char  *filename)
{
  return dio_open(filename, O_RDWR|O_CREAT|O_TRUNC, 0644);
}

DIOFILE * dio_fopenrw(const char  *filename)
{
  return dio_open(filename, O_RDWR, 0644);
}

int dio_seterrno(DIOFILE *fp)
{
  if(fp == 0) return -1;
  fp->error_flag = errno;
  return fp->error_flag;
}

int dio_read(DIOFILE *fp, void *buf, int nbytes)
{
  if(fp == 0) return -1;
  if(buf == 0) return -1;
  fp->read_eof = -1;
  fp->error_flag = 0;
  int rv = POSIX_READ(fp->fd, buf, nbytes);
  if(rv == 0) fp->read_eof = 0;
  if(rv < 0) fp->error_flag = -1;
  return rv;
}

int dio_write(DIOFILE *fp, const void *buf, int nbytes)
{
  if(fp == 0) return -1;
  if(buf == 0) return -1;
  int rv = POSIX_WRITE(fp->fd, buf, nbytes);
  if(rv < 0) fp->error_flag = -1;
  return rv;
}

FILEOFF_t dio_seek(DIOFILE *fp, FILEOFF_t offset, int whence)
{
  int nbytes;
  char *buf;
  if(fp == 0) return -1;
  fp->seek_past_eof = -1;
  if(offset > dio_eof(fp)) {
    fp->seek_past_eof = 0;
    if(fp->fill_holes == 1) {
      if((fp->access && O_RDWR) || (fp->access && O_WRONLY)) {
	nbytes = offset - dio_eof(fp);
	buf = new char[nbytes];
	memset(buf, 0, nbytes);
	dio_write(fp, (void *)buf, nbytes);
	delete buf;
      }
    }
  }
  FILEOFF_t rv = POSIX_LSEEK(fp->fd, offset, whence);
  if(rv < 0) fp->error_flag = -1;
  return rv;
}

FILEOFF_t dio_tell(DIOFILE *fp)
{
  if(fp == 0) return -1;
  /* Seek 0 btyes from current postion to get our position */
  return dio_seek(fp, 0, SEEK_CUR );
}

/* Flush file buffers and return 0 on success, -1 on fail */
int dio_flush(DIOFILE *fp)
{
  if(fp == 0) return -1;
  return POSIX_FSYNC(fp->fd);
}

FILEOFF_t dio_eof(DIOFILE *fp)
{
#if defined (__64_BIT_DATABASE_ENGINE__) && defined (__UNIX__)
  struct stat64 buf;
  if(fp == 0) return -1;
  memset(&buf, 0, sizeof(buf));
  if(fstat64(fp->fd, &buf) != 0) return -1;
  return buf.st_size;
#else
  struct stat buf;
  if(fp == 0) return -1;
  memset(&buf, 0, sizeof(buf));
  if(fstat(fp->fd, &buf) != 0) return -1;
  return buf.st_size;
#endif
}

FILEOFF_t dio_rewind(DIOFILE *fp)
{
  if(fp == 0) return -1;
  return dio_seek(fp, 0, SEEK_SET); 
}


int dio_fputc(DIOFILE *fp, int c)
{
  int rv;
  unsigned char ch;
  ch = c;
  rv = dio_write(fp, (void *)&ch, 1);
  if(rv < 0) {
    fp->error_flag = -1;
    return -1;
  }
  return(c);
}

int dio_fgetc(DIOFILE *fp)
{
  int cnt;
  unsigned char c;
  cnt = dio_read(fp, (void *)&c, 1);
  if (cnt <= 0) {
    fp->error_flag = -1;
    return -1;
  }
  return c;
}

ssize_t dio_getdelim(DIOFILE *fp, char **buf, size_t *bufsiz, int delimiter)
{
   char *ptr, *eptr;
   int c;
   char *nbuf;
   size_t nbufsiz;
   ssize_t d;

   if (*buf == NULL || *bufsiz == 0) {
     *bufsiz = BUFSIZ;
     *buf = new char[*bufsiz];
     if(*buf == 0) return -1;
   }
   for (ptr = *buf, eptr = *buf + *bufsiz;;) {
     c = dio_fgetc(fp);
     if(fp->read_eof == 0) {
       return ptr == *buf ? -1 : ptr - *buf;
     }
     if(fp->error_flag != 0) {
       return -1;
     }
     *ptr++ = c;
     if (c == delimiter) {
       *ptr = '\0';
       return ptr - *buf;
     }
     if (ptr + 2 >= eptr) {
       nbufsiz = *bufsiz * 2;
       d = ptr - *buf;
       nbuf = new char[nbufsiz];
       if(nbuf == 0) return -1;
       memcpy(nbuf, *buf, *bufsiz);
       delete *buf;
       *buf = nbuf;
       *bufsiz = nbufsiz;
       eptr = nbuf + nbufsiz;
       ptr = nbuf + d;
     }
   }
   return -1;
}

ssize_t dio_getline(DIOFILE *fp, char **buf, size_t *bufsiz)
{
  return dio_getdelim(fp, buf, bufsiz, '\n');
}
#endif

#ifdef __BCC32__
#pragma warn .8080
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
