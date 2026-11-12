// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxdfp64.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/04/1997 
// Date Last Modified: 10/26/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- // 
// ------------- Program Description and Details ------------- // 
// ----------------------------------------------------------- // 
/*
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.
 
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library. If not, see
<https://www.gnu.org/licenses/>.

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


#include "gxdfp64.h"

#if defined (__UNIX__)
#include <errno.h>
#include <sys/file.h>
#include <sys/mman.h>
#endif

namespace datareel {

#if defined (__WIN32__)
// 10/31/2026: Positional I/O helpers for WIN32. The offset is passed in
// an OVERLAPPED structure on the synchronous file handle.
static __ULWORD__ gxdFPTR64PRead(HANDLE h, void *buf, __ULWORD__ bytes,
				 __LLWORD__ offset)
{
  unsigned char *p = (unsigned char *)buf;
  __ULWORD__ done = 0;
  while(done < bytes) {
    OVERLAPPED ov;
    memset(&ov, 0, sizeof(ov));
    LARGE_INTEGER li;
    li.QuadPart = offset + done;
    ov.Offset = li.LowPart;
    ov.OffsetHigh = (DWORD)li.HighPart;
    DWORD n = 0;
    if(!ReadFile(h, (LPVOID)(p + done), (DWORD)(bytes - done), &n, &ov)) {
      break; // ERROR_HANDLE_EOF or a read error
    }
    if(n == 0) break; // End of file
    done += (__ULWORD__)n;
  }
  return done;
}

static __ULWORD__ gxdFPTR64PWrite(HANDLE h, const void *buf, __ULWORD__ bytes,
				  __LLWORD__ offset)
{
  const unsigned char *p = (const unsigned char *)buf;
  __ULWORD__ done = 0;
  while(done < bytes) {
    OVERLAPPED ov;
    memset(&ov, 0, sizeof(ov));
    LARGE_INTEGER li;
    li.QuadPart = offset + done;
    ov.Offset = li.LowPart;
    ov.OffsetHigh = (DWORD)li.HighPart;
    DWORD n = 0;
    if(!WriteFile(h, (LPCVOID)(p + done), (DWORD)(bytes - done), &n, &ov)) {
      break;
    }
    if(n == 0) break;
    done += (__ULWORD__)n;
  }
  return done;
}
#endif

#if defined (__UNIX__)
// 10/31/2026: Positional I/O helpers. The stream keeps its own file
// position and every read and write is one pread()/pwrite() at that
// position, retried on short transfers and EINTR.
static gxdFPTR64 *gxdFPTR64Attach(int fd, const char *mode)
{
  if(fd < 0) return 0;
  gxdFPTR64 *stream = new gxdFPTR64;
  stream->fd = fd;
  stream->pos = (__LLWORD__)0;
  stream->fptr = fdopen(fd, mode);
  if(stream->fptr == 0) {
    close(fd);
    delete stream;
    return 0;
  }
  return stream;
}

static __ULWORD__ gxdFPTR64PRead(int fd, void *buf, __ULWORD__ bytes,
				 __LLWORD__ offset)
{
  unsigned char *p = (unsigned char *)buf;
  __ULWORD__ done = 0;
  while(done < bytes) {
    ssize_t n = pread(fd, p + done, bytes - done, (off_t)(offset + done));
    if(n < 0) {
      if(errno == EINTR) continue;
      break;
    }
    if(n == 0) break; // End of file
    done += (__ULWORD__)n;
  }
  return done;
}

static __ULWORD__ gxdFPTR64PWrite(int fd, const void *buf, __ULWORD__ bytes,
				  __LLWORD__ offset)
{
  const unsigned char *p = (const unsigned char *)buf;
  __ULWORD__ done = 0;
  while(done < bytes) {
    ssize_t n = pwrite(fd, p + done, bytes - done, (off_t)(offset + done));
    if(n < 0) {
      if(errno == EINTR) continue;
      break;
    }
    if(n == 0) break;
    done += (__ULWORD__)n;
  }
  return done;
}
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

  const char* narrowString = fname;
  wchar_t wideBuffer[1024];
  MultiByteToWideChar(CP_ACP, 0, narrowString, -1, wideBuffer, 1024);

  stream->fptr = CreateFile(wideBuffer, 
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
  stream->pos = (__LLWORD__)0;
#elif defined (__UNIX__)
  delete stream;
  stream = gxdFPTR64Attach(open(fname, O_RDWR|O_CREAT|O_TRUNC|O_CLOEXEC, 0666),
			   "w+");
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
#endif

  if(mode == gxDBASE_READONLY) { // Open for read only access
#if defined (__WIN32__)
#ifdef _UNICODE

    const char* narrowString = fname;
    wchar_t wideBuffer[1024];
    MultiByteToWideChar(CP_ACP, 0, narrowString, -1, wideBuffer, 1024);

    stream->fptr = CreateFile(wideBuffer,
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
    stream->pos = (__LLWORD__)0;
    
#elif defined (__UNIX__)
    delete stream;
    return gxdFPTR64Attach(open(fname, O_RDONLY|O_CLOEXEC), "r");
#else
#error You must define a target platform and/or compiler
#endif
  }
  else { // Open for read/write access
#if defined (__WIN32__)
#ifdef _UNICODE
    const char* narrowString = fname;
    wchar_t wideBuffer[1024];
    MultiByteToWideChar(CP_ACP, 0, narrowString, -1, wideBuffer, 1024);

    stream->fptr = CreateFile(wideBuffer,
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
    stream->pos = (__LLWORD__)0;

#elif defined (__UNIX__)
    delete stream;
    return gxdFPTR64Attach(open(fname, O_RDWR|O_CLOEXEC), "r+");
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
  // 10/31/2026: Writes go straight to the kernel with pwrite(), so there
  // are no user space buffers to flush. gxdFPTR64Sync() forces the data
  // to disk.
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
  __ULWORD__ n = gxdFPTR64PRead(stream->fptr, buf, bytes, stream->pos);
  stream->pos += n;
  return n;

#elif defined (__UNIX__)
  __ULWORD__ n = gxdFPTR64PRead(stream->fd, buf, bytes, stream->pos);
  stream->pos += n;
  return n;
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
  __ULWORD__ n = gxdFPTR64PWrite(stream->fptr, buf, bytes, stream->pos);
  stream->pos += n;
  return n;

#elif defined (__UNIX__)
  __ULWORD__ n = gxdFPTR64PWrite(stream->fd, buf, bytes, stream->pos);
  stream->pos += n;
  return n;
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

#if defined (__WIN32__) || defined (__UNIX__)
  // 10/31/2026: A seek only moves the stream's own position. The END
  // mode used to seek from the current position.
  __LLWORD__ newpos;
  if(mode == gxDBASE_SEEK_BEG) {
    newpos = offset;
  }
  else if(mode == gxDBASE_SEEK_CUR) {
    newpos = stream->pos + offset;
  }
  else if(mode == gxDBASE_SEEK_END) {
#if defined (__WIN32__)
    LARGE_INTEGER size;
    if(!GetFileSizeEx(stream->fptr, &size)) return (__LLWORD__)-1;
    newpos = (__LLWORD__)size.QuadPart + offset;
#else
    struct stat64 buf;
    if(fstat64(stream->fd, &buf) != 0) return (__LLWORD__)-1;
    newpos = (__LLWORD__)buf.st_size + offset;
#endif
  }
  else { // An invalid seek mode was specified
    return (__LLWORD__)-1;
  }
  if(newpos < (__LLWORD__)0) return (__LLWORD__)-1;
  stream->pos = newpos;
  return newpos;
#else
#error You must define a target platform and/or compiler
#endif
}

GXDLCODE_API __LLWORD__ gxdFPTR64Tell(gxdFPTR64 *stream)
{
  if(!stream) return (__LLWORD__)-1; // The file stream is not open
  
#if defined (__WIN32__) || defined (__UNIX__)
  return stream->pos;
#else
#error You must define a target platform and/or compiler
#endif
}

GXDLCODE_API int gxdFPTR64Sync(gxdFPTR64 *stream, int full)
// 10/31/2026: Force the file's data to stable storage. With "full" set
// the file's metadata (size, times) is written too. Returns a non-zero
// value to indicate an error condition or zero if successful.
{
  if(!stream) return 0; // The file stream is not open
#if defined (__WIN32__)
  (void)full;
  if(!FlushFileBuffers(stream->fptr)) return 1;
#elif defined (__UNIX__)
  int rv = full ? fsync(stream->fd) : fdatasync(stream->fd);
  if(rv != 0) return 1;
#else
#error You must define a target platform and/or compiler
#endif
  return 0;
}

GXDLCODE_API __ULWORD__ gxdFPTR64ReadAt(gxdFPTR64 *stream, void *buf, 
					__ULWORD__ bytes, __LLWORD__ offset)
// 10/31/2026: Read from a file offset without moving the file position.
// Returns the number of bytes read, which is less than "bytes" at the end
// of the file.
{
  if(!stream) return (__ULWORD__)0; // The file stream is not open
#if defined (__WIN32__)
  return gxdFPTR64PRead(stream->fptr, buf, bytes, offset);
#elif defined (__UNIX__)
  return gxdFPTR64PRead(stream->fd, buf, bytes, offset);
#else
#error You must define a target platform and/or compiler
#endif
}

GXDLCODE_API __ULWORD__ gxdFPTR64WriteAt(gxdFPTR64 *stream, const void *buf,
					 __ULWORD__ bytes, __LLWORD__ offset)
// 10/31/2026: Write at a file offset without moving the file position.
// Returns the number of bytes written.
{
  if(!stream) return (__ULWORD__)0; // The file stream is not open
#if defined (__WIN32__)
  return gxdFPTR64PWrite(stream->fptr, buf, bytes, offset);
#elif defined (__UNIX__)
  return gxdFPTR64PWrite(stream->fd, buf, bytes, offset);
#else
#error You must define a target platform and/or compiler
#endif
}

GXDLCODE_API int gxdFPTR64Truncate(gxdFPTR64 *stream, __LLWORD__ size)
// 10/31/2026: Set the file size. Returns a non-zero value to indicate an
// error condition or zero if successful.
{
  if(!stream) return 1; // The file stream is not open
#if defined (__WIN32__)
  FILE_END_OF_FILE_INFO eof;
  eof.EndOfFile.QuadPart = size;
  return SetFileInformationByHandle(stream->fptr, FileEndOfFileInfo, 
				    &eof, sizeof(eof)) ? 0 : 1;
#elif defined (__UNIX__)
  return ftruncate64(stream->fd, (off64_t)size) == 0 ? 0 : 1;
#else
#error You must define a target platform and/or compiler
#endif
}

GXDLCODE_API int gxdFPTR64Allocate(gxdFPTR64 *stream, __LLWORD__ size)
// 10/31/2026: Reserve disk space so the file is at least "size" bytes
// long. Writes inside the reserved space do not change the file size,
// so a data sync after them does not have to write the file's metadata.
// Returns a non-zero value to indicate an error condition or zero if
// successful.
{
  if(!stream) return 1; // The file stream is not open
#if defined (__WIN32__)
  LARGE_INTEGER cur;
  if(!GetFileSizeEx(stream->fptr, &cur)) return 1;
  if(cur.QuadPart >= size) return 0;
  return gxdFPTR64Truncate(stream, size);
#elif defined (__UNIX__)
  struct stat64 buf;
  if(fstat64(stream->fd, &buf) != 0) return 1;
  if((__LLWORD__)buf.st_size >= size) return 0;
  // Write zeros rather than posix_fallocate(): on XFS and ext4 the
  // reserved extents stay "unwritten" until written, and converting
  // them makes every later data sync write metadata too.
  static const unsigned char zeros[65536] = { 0 };
  __LLWORD__ off = (__LLWORD__)buf.st_size;
  while(off < size) {
    __ULWORD__ n = (size - off) > (__LLWORD__)sizeof(zeros) ? 
      (__ULWORD__)sizeof(zeros) : (__ULWORD__)(size - off);
    if(gxdFPTR64PWrite(stream->fd, zeros, n, off) != n) return 1;
    off += n;
  }
  return 0;
#else
#error You must define a target platform and/or compiler
#endif
}

#if defined (__WIN32__)
// The lock is on one byte far past the end of any file, so it never
// blocks reads or writes of the file's data on Windows.
static void gxdFPTR64LockRange(OVERLAPPED &ov)
{
  memset(&ov, 0, sizeof(ov));
  ov.Offset = 0;
  ov.OffsetHigh = 0x40000000;
}
#endif

GXDLCODE_API int gxdFPTR64Lock(gxdFPTR64 *stream, int exclusive)
// 10/31/2026: Advisory whole-file lock between processes, shared or
// exclusive, waiting until it is granted (flock() on Linux, LockFileEx()
// on Windows). It does not stop reads or writes by processes that do
// not lock. Returns a non-zero value to indicate an error condition or
// zero if successful.
{
  if(!stream) return 1; // The file stream is not open
#if defined (__WIN32__)
  OVERLAPPED ov;
  gxdFPTR64LockRange(ov);
  return LockFileEx(stream->fptr, exclusive ? LOCKFILE_EXCLUSIVE_LOCK : 0,
		    0, 1, 0, &ov) ? 0 : 1;
#elif defined (__UNIX__)
  int rv;
  do {
    rv = flock(stream->fd, exclusive ? LOCK_EX : LOCK_SH);
  } while(rv != 0 && errno == EINTR);
  return rv == 0 ? 0 : 1;
#else
#error You must define a target platform and/or compiler
#endif
}

GXDLCODE_API int gxdFPTR64Unlock(gxdFPTR64 *stream)
{
  if(!stream) return 1; // The file stream is not open
#if defined (__WIN32__)
  OVERLAPPED ov;
  gxdFPTR64LockRange(ov);
  return UnlockFileEx(stream->fptr, 0, 1, 0, &ov) ? 0 : 1;
#elif defined (__UNIX__)
  return flock(stream->fd, LOCK_UN) == 0 ? 0 : 1;
#else
#error You must define a target platform and/or compiler
#endif
}

GXDLCODE_API void *gxdFPTR64Map(gxdFPTR64 *stream, __LLWORD__ len,
				void **map_handle)
// 10/31/2026: Map the first "len" bytes of the file read-only (mmap() on
// Linux, CreateFileMapping()/MapViewOfFile() on Windows). "len" must not
// be past the end of the file. The mapping sees later writes made with
// gxdFPTR64Write() and gxdFPTR64WriteAt(). Returns the address or a null
// pointer if the file cannot be mapped.
{
  *map_handle = 0;
  if(!stream || len <= (__LLWORD__)0) return 0;
  if((sizeof(void *) < 8) && (len > (__LLWORD__)0x40000000)) return 0;
#if defined (__WIN32__)
  LARGE_INTEGER li;
  li.QuadPart = len;
  HANDLE h = CreateFileMapping(stream->fptr, NULL, PAGE_READONLY,
			       (DWORD)li.HighPart, li.LowPart, NULL);
  if(!h) return 0;
  void *addr = MapViewOfFile(h, FILE_MAP_READ, 0, 0, (SIZE_T)len);
  if(!addr) {
    CloseHandle(h);
    return 0;
  }
  *map_handle = (void *)h;
  return addr;
#elif defined (__UNIX__)
  void *addr = mmap(0, (size_t)len, PROT_READ, MAP_SHARED, stream->fd, 0);
  if(addr == MAP_FAILED) return 0;
  return addr;
#else
#error You must define a target platform and/or compiler
#endif
}

GXDLCODE_API void gxdFPTR64Unmap(void *addr, __LLWORD__ len, void *map_handle)
{
  if(!addr) return;
#if defined (__WIN32__)
  (void)len;
  UnmapViewOfFile(addr);
  if(map_handle) CloseHandle((HANDLE)map_handle);
#elif defined (__UNIX__)
  (void)map_handle;
  munmap(addr, (size_t)len);
#else
#error You must define a target platform and/or compiler
#endif
}

GXDLCODE_API int gxdFPTR64Exists(const char *fname)
// Returns true if the specified file exists.
{
#if defined (__WIN32__)
#ifdef _UNICODE
  const char* narrowString = fname;
  wchar_t wideBuffer[1024];
  MultiByteToWideChar(CP_ACP, 0, narrowString, -1, wideBuffer, 1024);

  HANDLE hFile = CreateFile(wideBuffer, 
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
  const char* narrowString = fname;
  wchar_t wideBuffer[1024];
  MultiByteToWideChar(CP_ACP, 0, narrowString, -1, wideBuffer, 1024);

  HANDLE hFile = CreateFile(wideBuffer, 
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
    struct stat64 buf;
    if(stat64(fname, &buf) != (__LLWORD__)0) return (__LLWORD__)-1;
    return buf.st_size;
#else
#error You must a target platform and/or compiler
#endif
}


} // namespace datareel

// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
