// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: futils.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 01/25/2000
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

General-purpose platform independent file and directory functions
that need to be ported between WIN32 and UNIX platforms. 

Changes:
==============================================================
03/11/2002: Added large file support for the futils_filelength()
function. Also, the file length function now takes the string
file name instead of an int type representing an open file
handle.

08/12/2002: Added the "dirent *entry" parameter to the 
futils_readdir() function to support reentrant readdir calls
under UNIX. If the "entry" variable is not null the reentrant 
version of the readdir function will be called instead of the
non-thread safe version called by default. 

09/03/2002: Modified the futils_readdir() function to work
with POSIX and non-POSIX readdir_r() calls under Solaris.

09/04/2002: The __REENTRANT__ preprocessor directive must be 
defined during UNIX builds to enable the use of reentrant
readdir calls within the futils_readdir() function.

06/11/2003: Modified the futils_hasdriveletter() functions to 
test the string length of the directory name before checking 
for a driver letter.

06/11/2003: Modified the futils_rewinddir() function to check 
for null DIR* dirp pointers and to use the futils_exists() 
function to ensure that the directory still exists before 
attempting a rewind operation.

06/22/2003: Added Windows/UNIX compatibility macros to allow 
Windows programs to interpret UNIX file permissions.

12/04/2003: Modified the futils_GetDiskSpace() function to work
under Windows 95A. The previous WIN32 version would only work under
Windows 95B and higher.

09/18/2005: Modified the futils_isfile() function to check if the 
file exists before returning true. If the file is a directory of 
does no exist the function will return file. 
=============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "futils.h"

#ifdef __BCC32__
#pragma warn -8057
#endif

// -----------------------------------------------------------
// BEGIN: Windows and UNIX Compatibility macros
// -----------------------------------------------------------
#ifndef __S_ISTYPE
#define	__S_ISTYPE(mode, mask) (((mode) & 0170000) == (mask))
#endif
#ifndef S_ISDIR
#define	S_ISDIR(mode)  __S_ISTYPE((mode), 0040000)
#endif
#ifndef S_ISCHR
#define	S_ISCHR(mode)  __S_ISTYPE((mode), 0020000)
#endif
#ifndef S_ISBLK
#define	S_ISBLK(mode)  __S_ISTYPE((mode), 0060000)
#endif
#ifndef S_ISREG
#define	S_ISREG(mode)  __S_ISTYPE((mode), 0100000)
#endif
#ifndef S_ISFIFO
#define S_ISFIFO(mode) __S_ISTYPE((mode), 0010000)
#endif
#ifndef S_ISLNK
#define S_ISLNK(mode)  __S_ISTYPE((mode), 0120000)
#endif
#ifndef S_ISSOCK
#define S_ISSOCK(mode) __S_ISTYPE((mode), 0140000)
#endif
#ifndef S_ISUID
#define	S_ISUID 04000
#endif
#ifndef S_ISGID
#define	S_ISGID	02000
#endif
#ifndef S_ISVTX
#define S_ISVTX	01000
#endif
#ifndef S_IRUSR
#define	S_IRUSR	0400
#endif
#ifndef S_IWUSR
#define	S_IWUSR	0200
#endif
#ifndef S_IXUSR
#define	S_IXUSR	0100
#endif
#ifndef S_IRWXU
#define	S_IRWXU	(0400|0200|0100)
#endif
#ifndef S_IREAD
#define S_IREAD S_IRUSR
#endif
#ifndef S_IWRITE
#define S_IWRITE S_IWUSR
#endif
#ifndef S_IEXEC
#define S_IEXEC S_IXUSR
#endif
#ifndef S_IRGRP
#define	S_IRGRP (S_IRUSR >> 3)
#endif
#ifndef S_IWGRP
#define	S_IWGRP (S_IWUSR >> 3)
#endif
#ifndef S_IXGRP
#define	S_IXGRP (S_IXUSR >> 3)
#endif
#ifndef S_IRWXG
#define	S_IRWXG (S_IRWXU >> 3)
#endif
#ifndef S_IROTH
#define	S_IROTH (S_IRGRP >> 3)
#endif
#ifndef S_IWOTH
#define	S_IWOTH (S_IWGRP >> 3)
#endif
#ifndef S_IXOTH
#define	S_IXOTH (S_IXGRP >> 3)
#endif
#ifndef S_IRWXO
#define	S_IRWXO (S_IRWXG >> 3)
#endif
// -----------------------------------------------------------
// BEGIN: Windows and UNIX Compatibility macros
// -----------------------------------------------------------

// ----------------------------------------------------------- //
// Standalone platform independent file/directory functions    //
// ----------------------------------------------------------- //

GXDLCODE_API int futils_set_timestamp(const char *fname, 
				      time_t access_time, time_t mod_time)
// Function used to set file access time and modification time.
// Returns a non-zero value to indicate a fatal error condition 
// or zero if no fatal errors occur.
{
  // 09/07/2006: Added to set file access times and modification times.

  if(!fname) return -1; // Check for null pointer

#if defined (__WIN32__)
  // NOTE: The date can be changed for a file if the change date 
  // is after midnight, January 1, 1970, and before 19:14:07 January 18, 2038, 
  // UTC using _utime or _wutime or before 23:59:59, December 31, 3000, 
  // UTC using _utime64 or _wutime64.
  _utimbuf times;
  times.actime = access_time;
  times. modtime = mod_time;
  if(_utime(fname, &times) == -1) return -1;
 
#elif defined (__UNIX__) && defined (__LINUX__) 
  struct utimbuf utbuf;
  utbuf.actime = access_time;
  utbuf.modtime = mod_time;
  utime(fname, &utbuf);

#elif defined (__UNIX__)
  // TODO: Need to test under HPUX and Solaris
#else
#error You must define an OS __WIN32__ or__UNIX__
#endif
  return 0;
}

GXDLCODE_API int futils_is_unc_path(const char *path)
// Returns true if this is a UNC path.
// UNC path = \\hostname\resource\location
{
  // 09/07/2006: Added to handle UNC paths

  if(!path) return 0;
  if(strlen(path) < 2) return 0;
  
  if(path[0] == '\\') {
    if(path[1] == '\\') {
      return 1;
    }
  }
  return 0;
}

GXDLCODE_API int futils_exists(const char *name)
// Tests to see if the specified directory or file name 
// exists. Returns true if the file or directory exists. 
{
  struct stat buf;
  return stat(name, &buf) == 0;
}

GXDLCODE_API int futils_isreg(const char *name)
// Returns true if this is a regular file. Always
// returns true under Windows
{
#if defined (__WIN32__)
  if(futils_isfile(name)) return 1;
#endif

#if defined (__LINUX__)
  struct stat buf;
  if(stat(name, &buf) != 0) return 0;
#if defined (S_ISREG)
  return S_ISREG(buf.st_mode);
#else
  if((buf.st_mode & S_IFMT) == S_IFREG) return 1;
#endif 
#endif // __LINUX__
  
  // TODO: Need to test futils_isreg() under Solaris and HPUX

  return 0;
} 

GXDLCODE_API int futils_ischar(const char *name)
// Returns true if this is a character device. Always
// returns false under Windows
{
#if defined (__LINUX__)
  struct stat buf;
  if(stat(name, &buf) != 0) return 0;
#if defined (S_ISCHR)
  return S_ISCHR(buf.st_mode);
#else
  if((buf.st_mode & S_IFMT) == S_IFCHR) return 1;
#endif 
#endif // __LINUX__
  
  // TODO: Need to test futils_ischar() under Solaris and HPUX

  return 0;
} 

GXDLCODE_API int futils_isblock(const char *name)
// Returns true if this is a block device. Always
// returns false under Windows
{
#if defined (__LINUX__)
  struct stat buf;
  if(lstat(name, &buf) != 0) return 0;
#if defined (S_ISBLK)
  return S_ISBLK(buf.st_mode);
#else
  if((buf.st_mode & S_IFMT) == S_IFBLK) return 1;
#endif
#endif // __LINUX__
  
  // TODO: Need to test futils_isblock() under Solaris and HPUX

  return 0;
} 

GXDLCODE_API int futils_isfifo(const char *name)
// Returns true if this is a fifo pipe (mkfifo). Always
// returns false under Windows
{
#if defined (__LINUX__)
  struct stat buf;
  if(lstat(name, &buf) != 0) return 0;
#if defined (S_ISFIFO)
  return S_ISFIFO(buf.st_mode);
#else
  if((buf.st_mode & S_IFMT) == S_IFIFO) return 1;
#endif 
#endif // __LINUX__
  
  // TODO: Need to test futils_isfifo() under Solaris and HPUX

  return 0;
} 

GXDLCODE_API int futils_islink(const char *name)
// Returns true if this is a symbolic link. Always
// returns false under Windows
{
#if defined (__LINUX__)
  struct stat buf;
  if(lstat(name, &buf) != 0) return 0;
#if defined (S_ISLNK)
  return S_ISLNK(buf.st_mode);
#else
  if((buf.st_mode & S_IFMT) == S_IFLNK) return 1;
#endif // S_ISLNK
#endif // __LINUX__

  // TODO: Need to test futils_islink() under Solaris and HPUX

  return 0;
} 

GXDLCODE_API int futils_isdirectory(const char *dir_name) 
// Returns true if the specified name is a directory
{
  struct stat buf;

#if defined (__LINUX__)
  if(lstat(dir_name, &buf) != 0) return 0;
#if defined (S_ISDIR)
  return S_ISDIR(buf.st_mode);
#else
  if((buf.st_mode & S_IFMT) == S_IFDIR) return 1;
#endif
#endif // __LINUX__

  if(stat(dir_name, &buf) != 0) return 0;

#if defined (_S_IFMT) && defined(_S_IFDIR)
  if((buf.st_mode & _S_IFMT) == _S_IFDIR) return 1;
#else
  if((buf.st_mode & S_IFMT) == S_IFDIR) return 1;
#endif
  
 return 0;
}

GXDLCODE_API int futils_isfile(const char *fname)
// Returns true if the specified name is any kind of file 
// or false if this is a directory.
{
  return ((futils_isdirectory(fname) == 0) && (futils_exists(fname))); 
}

GXDLCODE_API int futils_stat(const char *name, 
			     int *isFile, int *isDir, int *isLink,
			     int *isChar, int *isBlock,
			     int *isFifo, int *isReg,
			     int *link_size)
// Recursive stat function for Linux and Windows. 
// Returns true if the specified file or directory exists. 
// Returns 0 if *name does not exist or -1 to indicate a 
// programming error.  
{
  if(!name) return -1;

  if(isFile) *isFile = 0;   // *name is a file
  if(isDir) *isDir = 0;     // *name is a directory   
  if(isLink) *isLink = 0;   // *name is a UNIX symbolic link
  if(isChar) *isChar = 0;   // *name is a UNIX character device 
  if(isBlock) *isBlock = 0; // *name is a UNIX block device
  if(isFifo) *isFifo = 0;   // *name is a UNIX named pipe
  if(isReg) *isReg = 0;     // *name is a regular file
  if(link_size) *link_size = 0; // Size of name
  // NOTE: The link_size should only be used to record the length of
  // NOTE: UNIX symbolic links, pipes, character or block devices. Use the 
  // NOTE: futils_filelength() function to size files. The file length function
  // NOTE: supports large files.

#if defined (__WIN32__)
  struct stat buf;

  if(stat(name, &buf) != 0) return 0;

  if((buf.st_mode & S_IFMT) == S_IFDIR) {
    if(isDir) *isDir = 1;
  }
  else {
    if(isFile) *isFile = 1;
    if(isReg) *isReg = 1;
  }

  if(link_size) *link_size = buf.st_size;

  return 1;

#elif defined (__LINUX__)
  struct stat buf;
  if(lstat(name, &buf) != 0) return 0;

  if((buf.st_mode & S_IFMT) == S_IFDIR) {
    if(isDir) *isDir = 1;
  }
  else {
    if(isFile) *isFile = 1;
  }

  if((buf.st_mode & S_IFMT) == S_IFLNK) {
    if(isLink) *isLink = 1;
  }
  if((buf.st_mode & S_IFMT) == S_IFIFO) {
    if(isFifo) *isFifo = 1;
  }
  if((buf.st_mode & S_IFMT) == S_IFBLK) {
    if(isBlock) *isBlock = 1;
  }
  if((buf.st_mode & S_IFMT) == S_IFCHR) {
    if(isChar) *isChar = 1;
  }
  if((buf.st_mode & S_IFMT) == S_IFREG) {
    if(isReg) *isReg = 1;
  }

  if(link_size) *link_size = buf.st_size;
  return 1;

#else
  // TODO: Need to test the futils_stat() call under Solaris and HPUX
  return -1;
#endif
}

GXDLCODE_API void futils_makeDOSpath(char *path)
// Make a DOS directory path by changing all
// forward slash path separators with back
// slashes.
{
  unsigned len = strlen(path);
  while(len--) {
    char c = *path;
    switch(c) {
      case '/' :
	*path = '\\';
	break;
      default:
	break;
    }
    path++;
  }
}

GXDLCODE_API void futils_makeUNIXpath(char *path)
// Make a UNIX directory path by changing all
// back slash path separators with forward
// slashes.
{
  unsigned len = strlen(path);
  while(len--) {
    char c = *path;
    switch(c) {
      case '\\' :
	*path = '/';
	break;
      default:
	break;
    }
    path++;
  }
}

GXDLCODE_API int futils_chdir(const char *dir_name)
// Change directory. Returns zero if successful.
{
  char *path = (char *)dir_name;
  
  if(!futils_exists(dir_name)) {
    return 1;
  }

#if defined (__WIN32__)
  futils_makeDOSpath(path);
#elif defined (__UNIX__)
  futils_makeUNIXpath(path);
#else
#error You must define a file system: __WIN32__ or __UNIX__
#endif

  if(chdir(path) != 0) {
    return 1;
  }
  return 0;
}

GXDLCODE_API int futils_rmdir(const char *dir_name)
// Remove the specified directory. Returns zero if successful.
// The directory must be empty and not be the current working
// directory or the root directory.
{
  char *path = (char *)dir_name;
  
  if(!futils_exists(dir_name)) {
    return 1;
  }

#if defined (__WIN32__)
  futils_makeDOSpath(path);
#elif defined (__UNIX__)
  futils_makeUNIXpath(path);
#else
#error You must define a file system: __WIN32__ or __UNIX__
#endif

  if(rmdir(path) != 0) {
    return 1;
  }
  return 0;
}

GXDLCODE_API int futils_access(const char *fname, char &exists, 
			       char &write, char &read) 
// Function used to determine read/write access to a file before 
// reading or writing to a file. Returns a non-zero value to 
// indicate an error condition or zero if no errors occur.
{
  char execute;
  return futils_access(fname, exists, write, read, execute);
}

GXDLCODE_API int futils_access(const char *fname, char &exists, 
			       char &write, char &read, char &execute)
// Function used to determine read/write access to a file before 
// reading or writing to a file. Returns a non-zero value to 
// indicate a fatal error condition or zero if no fatal errors occur.
{
  exists = write = read = execute = 0;
  if(!futils_exists(fname)) {
    exists = 0;
    return 0; // Do not return a fatal error
  }
  exists = 1; // The file or DIR exists

#if defined (__WIN32__)
  if(futils_mode(fname, 1, read, write, execute) != 0) {
    write = read = execute = 0;
    return -1;
  }
#elif defined (__UNIX__)
  int mode, uid, gid;
  if(futils_getmode(fname, mode, uid, gid) != 0) return -1;
  if(uid == (int)getuid()) { // Check user permissions
    if((mode & S_IRUSR) == S_IRUSR) read = 1;
    if((mode & S_IWUSR) == S_IWUSR) write = 1;
    if((mode & S_IXUSR) == S_IXUSR) execute = 1;
    if((mode & S_IRWXU) == S_IRWXU) read = write = execute = 1;
  }
  else if(gid == (int)getgid()) { // Check group permissions
    if((mode & S_IRGRP) == S_IRGRP) read = 1;
    if((mode & S_IWGRP) == S_IWGRP) write = 1;
    if((mode & S_IXGRP) == S_IXGRP) execute = 1;
    if((mode & S_IRWXG) == S_IRWXG) read = write = execute = 1;
  }
  else { // Check all user permissions
    if((mode & S_IROTH) == S_IROTH) read = 1;
    if((mode & S_IWOTH) == S_IWOTH) write = 1;
    if((mode & S_IXOTH) == S_IXOTH) execute = 1;
    if((mode & S_IRWXO) == S_IRWXO) read = write = execute = 1;
  }
#else
#error You must define a file system: __WIN32__ or __UNIX__
#endif

  if(futils_isfile(fname)) {
    FILE *f;
    if(read) { // Test read access to the file
      f = fopen(fname, "rb");
      if(!f) read = 0;
      if(f) fclose(f);
    }
    if(write) { // Test write access to the file
      f = fopen(fname, "r+b");
      if(!f) write = 0;
      if(f) fclose(f);
    }
  }

  return 0;
}

GXDLCODE_API int futils_strmode(int mode, char *sbuf, int max_len)
// Function used to convert UNIX file's permissions into a string.
// Returns a non-zero value to indicate an error condition or zero 
// if no errors occur.
{
  if(max_len < 11) return -1;
  memset(sbuf, 0, max_len);
  strcpy(sbuf, "----------");

  if(S_ISDIR(mode)) {
    sbuf[0] = 'd';
  } 
  else if(S_ISCHR(mode)) {
    sbuf[0] = 'c';
  } 
  else if(S_ISBLK(mode)) {
    sbuf[0] = 'b';
  } 
  else if(S_ISFIFO(mode)) {
    sbuf[0] = 'f';
  } 
  else if(S_ISREG(mode)) {
    sbuf[0] = '-';
  } 
  else if(S_ISLNK(mode)) {
    sbuf[0] = 'l';
  } 
  else if(S_ISSOCK(mode)) {
    sbuf[0] = 's';
  }
  if(mode & S_IRUSR) {
    sbuf[1] = 'r';
  } 
  if(mode & S_IWUSR) {
    sbuf[2] = 'w';
  }
  if((mode & S_IXUSR) && (mode & S_ISUID)) {
    sbuf[3] = 's';
  } 
  else if(mode & S_ISUID) {
    sbuf[3] = 'S';
  } 
  else if(mode & S_IXUSR) {
    sbuf[3] = 'x';
  }
  if(mode & S_IRGRP) {
    sbuf[4] = 'r';
  } 
  if(mode & S_IWGRP) {
    sbuf[5] = 'w';
  }
  if((mode & S_IXGRP) && (mode & S_ISGID)) {
    sbuf[6] = 's';
  } 
  else if(mode & S_ISGID) {
    sbuf[6] = 'S';
  } 
  else if(mode & S_IXGRP) {
    sbuf[6] = 'x';
  }
  if(mode & S_IROTH) {
    sbuf[7] = 'r';
  } 
  if(mode & S_IWOTH) {
    sbuf[8] = 'w';
  }
  if((mode & S_IXOTH) && (mode & S_ISVTX)) {
    sbuf[9] = 't';
  } 
  else if(mode & S_ISVTX) {
    sbuf[9] = 'T';
  } 
  else if(mode & S_IXOTH) {
    sbuf[9] = 'x';
  }
  return 0;
}

GXDLCODE_API int futils_getmode(const char *fname, int &mode, 
				int &uid, int &gid)
// Function used to get a file's permission mode, user ID 
// and group ID. Returns a non-zero value to indicate an 
// error condition or zero if no errors occur.
{
  // Check for null pointers
  if(!fname) return -1;

  struct stat buf;
  mode = 0; uid = 0; gid = 0;
  int result = stat(fname, &buf);
  if(result != 0) return -1;
  mode = (int)buf.st_mode;
  uid = (int)buf.st_uid;
  gid = (int)buf.st_gid;
  return 0;
}

GXDLCODE_API int futils_mode(const char *fname, int ugo, 
			     char &read, char &write, char &execute)
// Function used to get UNIX style file permissions for a specified file. 
// If the "ugo" variable equals 1 the user permissions are passed back in 
// the read/write/execute variables. If the "ugo" variable equals 2 the group 
// permissions are passed back. If the "ugo" variable is greater then 2 the 
// permissions for all users will be passed back. NOTE: Under Windows the 
// "ugo" variable has no affect. Returns a non-zero value to indicate an error 
// condition or zero if no errors occur.
{
  // Check for null pointers
  if(!fname) return -1;
  
  // Reset the file attributes
  read = write = execute = 0;

#if defined (__WIN32__)
  char ro, system, hidden, arch;
  char dot_ext[4];
  int len, offset, i, j;
  memset(dot_ext, 0, 4);
  if(futils_attrib(fname, ro, system, hidden, arch) != 0) {
    return -1;
  }
  if(ro) {
    read = 1;
    write = 0;
  }
  else {
    read = write = 1;
  }
  if(strlen(fname) >= 4) {
    if(!futils_isdirectory(fname)) {
      len = strlen(fname);
      offset = len - 4;
      for(i = offset, j = 0; i < len; i++, j++) {
	dot_ext[j] = fname[i];
	tolower(dot_ext[j]);
      } 
      if(strcmp(dot_ext, ".exe") == 0) execute = 1;
      if(strcmp(dot_ext, ".com") == 0) execute = 1;
      if(strcmp(dot_ext, ".bat") == 0) execute = 1;
      if(strcmp(dot_ext, ".cmd") == 0) execute = 1;
    }
  }

#elif defined (__UNIX__)
  int mode, uid, gid;
  if(futils_getmode(fname, mode, uid, gid) != 0) return -1;
  if(ugo == 1) { // Check user permissions
    if((mode & S_IRUSR) == S_IRUSR) read = 1;
    if((mode & S_IWUSR) == S_IWUSR) write = 1;
    if((mode & S_IXUSR) == S_IXUSR) execute = 1;
    if((mode & S_IRWXU) == S_IRWXU) read = write = execute = 1;
  }
  else if(ugo == 2) { // Check group permissions
    if((mode & S_IRGRP) == S_IRGRP) read = 1;
    if((mode & S_IWGRP) == S_IWGRP) write = 1;
    if((mode & S_IXGRP) == S_IXGRP) execute = 1;
    if((mode & S_IRWXG) == S_IRWXG) read = write = execute = 1;
  }
  else { // Check all user permissions
    if((mode & S_IROTH) == S_IROTH) read = 1;
    if((mode & S_IWOTH) == S_IWOTH) write = 1;
    if((mode & S_IXOTH) == S_IXOTH) execute = 1;
    if((mode & S_IRWXO) == S_IRWXO) read = write = execute = 1;
  }
#else
#error You must define a file system: __WIN32__ or __UNIX__
#endif
  return 0;
}

GXDLCODE_API int futils_attrib(const char *fname, char &ro, char &system,
			       char &hidden, char &arch)
// Function used to get DOS style file attributes for a specified file. 
// NOTE: Under UNIX the archive flag will always equal zero. Returns a 
// non-zero value to indicate an error condition or zero if no errors 
// occur.
{
  // Check for null pointers
  if(!fname) return -1;

  // Reset the file attributes
  ro = system = hidden = arch = 0;

#if defined (__WIN32__)
  _finddata_t c_file;
  long hFile;
  if((hFile = _findfirst((char *)fname, &c_file)) == -1L) return -1;
  if(c_file.attrib & _A_RDONLY) ro = 1;
  if(c_file.attrib & _A_SYSTEM) system = 1;
  if(c_file.attrib & _A_HIDDEN) hidden = 1;
  if(c_file.attrib & _A_ARCH ) arch = 1;
  _findclose(hFile);
#elif defined (__UNIX__)
  int mode, uid, gid;
  char write = 0; char read = 0;
  if(futils_getmode(fname, mode, uid, gid) != 0) return -1;
  if(uid == (int)getuid()) { // Check user permissions
    if((mode & S_IRUSR) == S_IRUSR) read = 1;
    if((mode & S_IWUSR) == S_IWUSR) write = 1;
    if((mode & S_IRWXU) == S_IRWXU) read = write = 1;
  }
  else if(gid == (int)getgid()) { // Check group permissions
    if((mode & S_IRGRP) == S_IRGRP) read = 1;
    if((mode & S_IWGRP) == S_IWGRP) write = 1;
    if((mode & S_IRWXG) == S_IRWXG) read = write = 1;
  }
  else { // Check all user permissions
    if((mode & S_IROTH) == S_IROTH) read = 1;
    if((mode & S_IWOTH) == S_IWOTH) write = 1;
    if((mode & S_IRWXO) == S_IRWXO) read = write = 1;
  }

  if(read) {
    if(!write) ro = 1;
    if(write) ro = 0;
  }

  if(!read) {
    if(!write) ro = 1;
    if(write) ro = 0;
  }
  if(!S_ISREG(mode)) {
    if(!futils_isdirectory(fname)) system = 1; // Not a regular file
  }

  // Check for dot files
  if(strlen(fname) >= 2) {
    if(fname[0] == '.') {
      if(fname[1] != '.') {
	hidden = 1;
      }
    }
  }

#else
#error You must define a file system: __WIN32__ or __UNIX__
#endif
  return 0;
} 

GXDLCODE_API int futils_timestamp(const char *fname, time_t &access_time, 
				  time_t &mod_time, time_t &create_time)
// Function used to get a file's creation, access, and modification time. 
// Returns a non-zero value to indicate an error condition or zero if no 
// errors occur.
{
  // Check for null pointers
  if(!fname) return -1;

  // Reset the time values
  access_time = (time_t)0;
  mod_time = (time_t)0;
  create_time = (time_t)0;

#if defined (__WIN32__)
  _finddata_t c_file;
  long hFile;
  
  if((hFile = _findfirst((char *)fname, &c_file )) == -1L) return -1;

  mod_time = c_file.time_write;
#ifdef __NTFS__
  // Cannot set these values on FAT file systems
  access_time = c_file.time_access;
  create_time = c_file.time_create;
#endif // __NTFS__

  _findclose(hFile);
  return 0;
    
#else // Default to standard LIBC routine
  struct stat buf;
  int result = stat(fname, &buf);
  if(result != 0) return -1;
  access_time = buf.st_atime;
  mod_time = buf.st_mtime;
  create_time = buf.st_ctime;
  return 0;
#endif
}

GXDLCODE_API int futils_hasdriveletter(const char *dir_name, char &letter)
// WIN32 function used to parse the drive letter from the
// specified directory. Returns false if the path does not
// contain a drive letter. If a drive letter is found it will
// be passed back in the "letter" variable.
{
  // Check for null pointers
  if(!dir_name) return 0; 

  // Reset the drive letter variable
  letter = 0;

  // Test the string for the minimum number of characters
  if(strlen(dir_name) < 2) return 0;

  if(dir_name[1] != ':') return 0; // No drive letter found

  // Get the drive letter following the first colon
  letter = dir_name[0];

  return 1;
}

GXDLCODE_API int futils_hasdriveletter(const char *dir_name, char *letter)
// WIN32 function used to parse the drive letter from the
// specified directory. Returns false if the path does not
// contain a drive letter. If a drive letter is found it will
// be passed back in the "letter" variable.
{
  // Check for null pointers
  if(!dir_name) return 0; 
  if(!letter) return 0;

  // Reset the drive letter variable
  *letter = 0;

  // Test the string for the minimum number of characters
  if(strlen(dir_name) < 2) return 0;

  if(dir_name[1] != ':') return 0; // No drive letter found

  // Get the drive letter following the first colon
  *letter = dir_name[0];

  return 1;
}

GXDLCODE_API int futils_mkdir(const char *dir_name)
// Make the specified directory (with sub-directories) 
// if the directory if it does not exist. Returns a 
// non-zero value if an error occurs. UNIX file systems 
// will use 755 permissions when new directories are 
// created.
{
  char *path = (char *)dir_name;
  char path_sep;
  char sbuf[futils_MAX_PATH_LENGTH];
  unsigned i = 0; int rv;  
  unsigned len;
  char *s;
  int num_paths = 0;
  
#if defined (__WIN32__)
  char drive_letter = 0;
  int has_drive_letter = 0;
  int unc_path = 0;
#endif
  
  if(futils_exists(dir_name)) {
    // Directory exists, return with no error reported
    return 0;
  }

#if defined (__WIN32__)
  has_drive_letter = futils_hasdriveletter(dir_name, &drive_letter);
  unc_path = futils_is_unc_path(dir_name);
  futils_makeDOSpath(path);
  path_sep = '\\';
#elif defined (__UNIX__)
  futils_makeUNIXpath(path);
  path_sep = '/';
#else
#error You must define a file system: __WIN32__ or __UNIX__
#endif

  // The mkdir function can only create one new directory per call
  // so the complete directory path can only be created one sub-
  // directory at a time.
  for(i = 0; i < futils_MAX_PATH_LENGTH; i++) sbuf[i] = '\0';

  len = strlen(path); s = path;
  if(len > futils_MAX_PATH_LENGTH) len = futils_MAX_PATH_LENGTH;

  for(i = 0; i < len; i++, s++) {

    // Get the whole directory path
    if(i == (len-1)) { memmove(sbuf, path, len); break; }

#if defined (__WIN32__)
    if((has_drive_letter) && (i == 1)) { // Skip over the drive letter
      i++; s++; 
      if(*s == path_sep) { i++; s++; }
    }
    if((unc_path) && (i == 0)) {
      i++; s++; 
      if(*s == path_sep) { i++; s++; }
    }
    if((unc_path) && (i == 1)) {
      i++; s++; 
      if(*s == path_sep) { i++; s++; }
    }
#endif

    if((*s == path_sep) && (i > 1)) { // Step past the root directory
      memmove(sbuf, path, i);
      num_paths++;

#if defined (__WIN32__)
      // 09/07/2006: Modified to handle UNC paths
      // UNC path = \\hostname\resource\location
      if(unc_path) {
	rv = 0;
	if(num_paths > 2) {
	  if(!futils_exists(sbuf)) {
	    if(mkdir(sbuf) != 0) return 1;
	  }
	}
      }
      else {
	if(!futils_exists(sbuf)) {
	  if(mkdir(sbuf) != 0) return 1;
	}
      }
#elif defined (__UNIX__)
      if(!futils_exists(sbuf)) {
	if(mkdir(sbuf,
		 S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) != 0) return 1;
      }
#else
#error You must define a file system: __WIN32__ or __UNIX__
#endif
    }
  }

  // Get rid of any terminating path separators
  if(sbuf[len-1] == path_sep) sbuf[len-1] = '\0';

  // Make the complete directory path if needed
  if(!futils_exists(sbuf)) {
#if defined (__WIN32__)
    rv = mkdir(sbuf);
#elif defined(__UNIX__)
    rv = mkdir(sbuf,
	       S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
#else
#error You must define a file system: __WIN32__ or __UNIX__
#endif
    if(rv != 0) {
      return 1;
    }
  }      

  return 0;
}

GXDLCODE_API int futils_remove(const char *fname)
// Remove the specified file
{
  if(!futils_exists(fname)) {
    return 1;
  }
  if(remove(fname) != 0) { 
   return 1;
  }
  return 0;
}

GXDLCODE_API int futils_rename(const char *oldname, const char *newname)
// Rename the specified file
{
  if(!futils_exists(oldname)) {
    return 1;
  }
  if(rename(oldname, newname) != 0) {
    return 1;
  }
  return 0;
}

GXDLCODE_API int futils_chmod(const char *fname, const char *mode_str)
// Change the file-permission settings under UNIX. The "mode"
// variable should be equal to a valid UNIX file permission. Returns 
// a non-zero value if an error occurs.
{
#if defined (__WIN32__)
  return 1;
#elif defined (__UNIX__)
  if(!mode_str) return 1; // This is a null pointer
  int mode = 0;
  char obuf[81];
  memset(obuf, 0, sizeof(obuf));
  strcpy(obuf, mode_str);
  char *p = obuf;

  // Mode string should be preceded by a 0
  if(obuf[0] != '0') {
    memmove((p+1), obuf, strlen(obuf));
    obuf[0] = '0';
  }
  sscanf(obuf, "%o", &mode);

  if(chmod(fname, mode) != 0) {
    return 1;
  }
  return 0;
#else
#error You must define a file system: __WIN32__ or __UNIX__
#endif
}

GXDLCODE_API int futils_chmod(const char *fname, futilPermissionMode pmode)
// Change the file-permission settings. The "pmode"
// variable should be equal to futils_READONLY, futils_WRITEONLY,
// or futils_READWRITE. Returns a non-zero value if an 
// error occurs.
{
  if(!futils_exists(fname)) return 1;

  switch(pmode) {
    case futils_READONLY :
#if defined (_S_IREAD)
      if(chmod(fname, _S_IREAD) != 0) {
	return 1;
      }
#else
      if(chmod(fname, S_IREAD) != 0) {
	return 1;
      }
#endif
      break;

    case futils_WRITEONLY :
#if defined (_S_IWRITE)
      if(chmod(fname, _S_IWRITE) != 0) {
	return 1;
      }
#else
      if(chmod(fname, S_IWRITE) != 0) {
	return 1;
      }
#endif
      break;

    case futils_READWRITE :
#if defined (_S_IREAD) && defined (_S_IWRITE)
      if(chmod(fname, _S_IREAD | _S_IWRITE) != 0) {
	return 1;
      }
#else
      if(chmod(fname, S_IREAD | S_IWRITE) != 0) {
	return 1;
      }
#endif
      break;

    default:
      return 1;
  }

  return 0;
}

GXDLCODE_API void futils_pathsimplify(const char *path, char *new_path, 
				      char path_sep)
// Function used to canonicalize a path and return a new path.
// The path separator should either be a forward slash for UNIX
// file systems or a backslash for DOS/WIN32 file systems. Multiple
// path separators will be collapsed to a single path separator.
// Leading `./' paths and trailing `/.' paths will be removed.
// Trailing path separators will be removed. All non-leading `../'
// paths and trailing `..' paths are handled by removing portions of
// the path. NOTE: This function assumes that the necessary memory has
// already been allocated for the "new_path" variable by the calling
// function.
{
  unsigned len ;
  char stub_char;
  int i, start;
  int ddot; // Directory immediately above the current directory position
  
  if(!*path) return;

  // Copy the complete path into the new_path variable
  len = strlen(path);
  memmove(new_path, path, len);
  new_path[len] = '\0'; // Ensure null termination
  stub_char = (*new_path == path_sep) ? path_sep : '.';

  // Remove all `./' paths preceding the string.  If `../' paths
  // precede, put `/' in front and remove them as well
  i = ddot = start = 0;
  while (1) { // PC-lint 05/27/2002: while(1) loop
    if(new_path[i] == '.' && new_path[i + 1] == path_sep)
      i += 2;
    else if (new_path[i] == '.' && new_path[i + 1] == '.' && \
	     new_path[i + 2] == path_sep) { 
      i += 3;
      ddot = 1;
    }
    else
      break;
  }
  if(i) strcpy(new_path, new_path + i - ddot);

  // Replace single `.' or `..' with `/'.
  if((new_path[0] == '.' && new_path[1] == '\0') 
     || (new_path[0] == '.' && new_path[1] == '.' && new_path[2] == '\0')) {
    new_path[0] = stub_char;
    new_path[1] = '\0';
    return;
  }

  // Walk along the path looking for paths to compact
  i = 0;
  while(1) { // PC-lint 05/27/2002: while(1) loop
    if(!new_path[i]) break;

    while(new_path[i] && new_path[i] != path_sep) {
      i++;
      // PC-lint 05/02/2005: Possible access of out-of-bounds pointer
      if(i > (int)strlen(new_path)) break;
    }

    start = i++;

    // If we didn't find any slashes, then there is nothing left to do
    if(!new_path[start]) break;

    // Handle multiple path separators in a row
    while(new_path[i] == path_sep) {
      i++;
      // PC-lint 05/02/2005: Possible access of out-of-bounds pointer
      if(i > (int)strlen(new_path)) break;
    }

    if((start + 1) != i) {
      strcpy(new_path + start + 1, new_path + i);
      i = start + 1;
    }

    // Check for trailing path separator
    if(start && !new_path[i]) {
      new_path[--i] = '\0';
      break;
    }

    // Check for `../' paths, `./' paths or a trailing `.' by itself
    if(new_path[i] == '.') {
      // Handle trailing a `.' path
      if(!new_path[i + 1]) {
	new_path[--i] = '\0';
	break;
      }

      // Handle `./' paths
      if(new_path[i + 1] == path_sep) {
	strcpy(new_path + i, new_path + i + 1);
	i =(start < 0) ? 0 : start;
	continue;
      }

      // Handle `../' paths or a trailing `..' path by itself
      if(new_path[i + 1] == '.' &&
	 (new_path[i + 2] == path_sep || !new_path[i + 2])) {
	while(--start > -1 && new_path[start] != path_sep);
	strcpy(new_path + start + 1, new_path + i + 2);
	i = (start < 0) ? 0 : start;
	continue;
      }
    }
  }

  if(!*new_path) { // Nothing was left after the path was simplified
     *new_path = stub_char;
    new_path[1] = '\0';
  }

  // Ensure null termination
  new_path[strlen(new_path)] = '\0';
}

GXDLCODE_API int futils_genoutputfilename(const char *current_file, 
					  char *out_file, char *extension)
// Generate a name for the output file using the "current_file" name with 
// the specified dot extension. NOTE: This function assumes that the 
// necessary memory has already been allocated for the "out_file"
// variable by the calling function. Returns a non-zero value if any
// errors occur.
{
  unsigned i = 0;
  unsigned len;
  char *p;
  
  if(!out_file) return 1;

  for(i = 0; i < futils_MAX_NAME_LENGTH; i++) out_file[i] = '\0';
  p = (char *)current_file;
  len = strlen(p);
  for(i = 0; i < len && i != futils_MAX_NAME_LENGTH; i++, p++) {
    if(*p == '.') break;
    out_file[i] = *p;
  }
  if((strlen(out_file) + strlen(extension)) > (futils_MAX_NAME_LENGTH - 1)) 
    return 1;
  strcat(out_file, extension); // Add the file extension (.xxx)
  return 0;
}

GXDLCODE_API int futils_getcwd(char *dir, unsigned max_len)
// Passes back the present working directory in the "dir"
// variable. Returns 0 if no errors occur. NOTE: This
// function assumes that the required amount of memory
// has already been allocated for the "dir" pointer. The
// "max_len" value must be at least one byte greater than
// the length of the pathname to be returned.
{
  unsigned i = 0;
  
  // Clear the path name buffer
  for(i = 0; i < max_len; i++) dir[i] = '\0';

  if(getcwd(dir, max_len) == 0) {
    return 1;
  }

  // Ensure null termination
  dir[strlen(dir)] = '\0';
  return 0;
}

#if defined (__64_BIT_DATABASE_ENGINE__) || defined (__ENABLE_64_BIT_INTEGERS__)
GXDLCODE_API __LLWORD__ futils_filelength(const char *fname)
#else
GXDLCODE_API long futils_filelength(const char *fname)
#endif
// Returns the file size, in bytes, for the specified file.
// Return a value of -1 to indicate an error.
{
#if defined (__64_BIT_DATABASE_ENGINE__) && defined (__WIN32__)
  struct stat buf;
  int result = stat(fname, &buf);

  if((result != 0) || (buf.st_size == -1)) {
    // 09/07/2006: Modified to use stat() call first.
    // If the file is larger than 2.1 GB use the CreateFile() call.
    // NOTE: The CreateFile() call can be extremly slow in recursive
    // functions so the stat() call is used first.
    HANDLE hFile = CreateFile(TEXT(fname), 
			      GENERIC_READ,
			      FILE_SHARE_READ|FILE_SHARE_WRITE,
			      NULL, // No security
			      OPEN_EXISTING, 
			      FILE_ATTRIBUTE_NORMAL, 
			      NULL // No attr. template 
			      );
    
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
  }
  else {
    return (__LLWORD__)buf.st_size;
  }
#elif defined (__ENABLE_64_BIT_INTEGERS__) && defined (__WIN32__)
  struct stat buf;
  int result = stat(fname, &buf);

  if((result != 0) || (buf.st_size == -1)) {
    // If the file is larger than 2.1 GB use the CreateFile() call.
    // NOTE: The CreateFile() call can be extremly slow in recursive
    // functions so the stat() call is used first.
    HANDLE hFile = CreateFile(TEXT(fname), 
			      GENERIC_READ,
			      FILE_SHARE_READ|FILE_SHARE_WRITE,
			      NULL, // No security
			      OPEN_EXISTING, 
			      FILE_ATTRIBUTE_NORMAL, 
			      NULL // No attr. template 
			      );
    
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
  }
  else {
    return (__LLWORD__)buf.st_size;
  }
#elif defined (__64_BIT_DATABASE_ENGINE__) && defined (__UNIX__)
    // Non-POSIX standard API interfaces to support large files.
    struct stat64 buf;
    if(stat64(fname, &buf) != (__LLWORD__)0) return (__LLWORD__)-1;
    return buf.st_size;

#elif defined (__WIN32__) && defined (__NTFS__)
  struct stat buf;
  int result = stat(fname, &buf);

  if((result != 0) || (buf.st_size == -1)) {
    // NOTE: The CreateFile() call can be extremly slow in recursive
    // functions so the stat() call is used first.
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
      return (long)-1;
    }
    
    BY_HANDLE_FILE_INFORMATION info;
    
    if(GetFileInformationByHandle(hFile, &info) == 0) {
      return (long)-1;
    }
    CloseHandle(hFile);
    return (long)info.nFileSizeLow;;
  }
  else {
    return (long)buf.st_size;
  }
#else // Use the 32-bit stdio version by default
  struct stat buf;
  int result = stat(fname, &buf);
  if(result != 0) return -1;
  return buf.st_size;
#endif
}
// ----------------------------------------------------------- //

// ----------------------------------------------------------- //
// Standalone platform independent directory stream functions  //
// ----------------------------------------------------------- //
GXDLCODE_API DIR *futils_opendir(const char *dirname)  
// Opens a directory stream corresponding to the directory named by 
// the dirname argument. The directory stream is positioned at the 
// first entry.
{
#if defined (__WIN32__)
  DIR *dirp;

  // Ensure that the directory exists
  if(futils_chdir(dirname) != 0) return NULL;

  // Allocate a memory for the DIR structure
  dirp = (DIR *)malloc(sizeof(DIR));
  if(dirp == NULL) return NULL;

  // Position the directory stream to the first entry
  dirp->dd_buf.d_ino = FindFirstFile("*.*", &dirp->dd_buf.file);
  if(dirp->dd_buf.d_ino == INVALID_HANDLE_VALUE) {
    free(dirp);
    return NULL;
  }
  
  // Initialize the DIR variables
  strcpy(dirp->dd_buf.d_name, dirp->dd_buf.file.cFileName);
  dirp->dd_buf.d_reclen = dirp->dd_buf.file.nFileSizeLow;
  dirp->dd_buf.d_namlen = strlen(dirp->dd_buf.file.cFileName);
  dirp->dd_loc = 0;
  dirp->dd_fd = -1;
  dirp->dd_size = dirp->dd_buf.file.nFileSizeLow;
  strcpy(dirp->dd_dirname, dirname);
  
  return dirp;
#elif defined (__UNIX__)
  return opendir(dirname);
#else
#error You must define a file system: __WIN32__ or __UNIX__
#endif
}

GXDLCODE_API int futils_closedir(DIR *dirp)
// Closes the directory stream referred to by the argument dirp.
// Returns zero upon successful completion or -1  if an error
// occurs.
{
#if defined (__WIN32__)
  if(!FindClose(dirp->dd_buf.d_ino)) return -1;
  free(dirp);
  return 0;
#elif defined (__UNIX__)
  return closedir(dirp);
#else
#error You must define a file system: __WIN32__ or __UNIX__
#endif
}

GXDLCODE_API struct dirent *futils_readdir(DIR *dirp, dirent *entry)
// Returns a pointer to a structure representing the directory entry 
// at the current position in the directory stream specified by the 
// argument dirp, and positions the directory stream at the next entry. 
// It returns a null pointer upon reaching the end of the directory 
// stream. If the "entry" variable is not null the thread safe version 
// of the readdir function will be called instead of the non-thread safe 
// version called by default.
{
#if defined (__WIN32__)
  if(dirp->dd_loc == 0) { // We are at the first entry
    dirp->dd_loc++;
    return &dirp->dd_buf;
  }

  if(FindNextFile(dirp->dd_buf.d_ino, &dirp->dd_buf.file)) {
    dirp->dd_loc++;
    strcpy(dirp->dd_buf.d_name, dirp->dd_buf.file.cFileName);
    dirp->dd_buf.d_reclen = dirp->dd_buf.file.nFileSizeLow;
    dirp->dd_buf.d_namlen = strlen(dirp->dd_buf.file.cFileName);
    dirp->dd_fd = -1;
    dirp->dd_size = dirp->dd_buf.file.nFileSizeLow;
    return &dirp->dd_buf;
  }
  else {
    return NULL;
  }
#elif defined (__UNIX__)
  // NOTE: This note applies to the C language version of the file 
  // utility functions.
  // 07/30/2000: The readdir() function will chop off the first
  // two characters of the file name under Solaris if not compiled
  // with the default native C compiler. Make sure you use the native
  // compiler (default /opt/SUNWspro/bin/cc, which may not be in your 
  // PATH), and not /usr/ucb/cc. See the following URL for more details: 
  // http://sunse.jinr.ru/local/solaris/solaris2.html#q6.17

  if(!entry) { 
    // Using non-reentrant readdir function for UNIX builds that 
    // do require the use of the reentrant readdir call. 
    return readdir(dirp);
  }
  else { // Using reentrant readdir function
#if defined (__REENTRANT__)
#if defined (__SOLARIS__) && !defined(_POSIX_PTHREAD_SEMANTICS)
    return readdir_r(dirp, entry); 
#elif defined (__HPUX10__)
    if(readdir_r(dirp, entry) != 0) return 0;
    return entry;
#else
    if(readdir_r(dirp, entry, &entry) != 0) return 0; 
    return entry;
#endif 
#else // Using non-reentrant readdir() function for UNIX platforms that
      // do not define the readdir_r() function.
    return readdir(dirp);
#endif // __REENTRANT__
  }
#else
#error You must define a file system: __WIN32__ or __UNIX__
#endif
}

GXDLCODE_API void futils_rewinddir(DIR *dirp)
// Resets the position of the directory stream to which dirp refers
// to the beginning of the directory.
{
  // Check for null pointers
  if(!dirp) return; 

#if defined (__WIN32__)
  if(!dirp->dd_dirname) return;

  // Ensure that the directory still exists
  if(!futils_exists(dirp->dd_dirname)) return;

  // Position the directory stream to the first entry
  dirp->dd_buf.d_ino = FindFirstFile("*.*", &dirp->dd_buf.file);
  if(dirp->dd_buf.d_ino == INVALID_HANDLE_VALUE) return;
  
  // Reset the DIR varaibles
  strcpy(dirp->dd_buf.d_name, dirp->dd_buf.file.cFileName);
  dirp->dd_buf.d_reclen = dirp->dd_buf.file.nFileSizeLow;
  dirp->dd_buf.d_namlen = strlen(dirp->dd_buf.file.cFileName);
  dirp->dd_loc = 0;
  dirp->dd_fd = -1;
  dirp->dd_size = dirp->dd_buf.file.nFileSizeLow;
#elif defined (__UNIX__)
  rewinddir(dirp);
#else
#error You must define a file system: __WIN32__ or __UNIX__
#endif
}

#if defined (__64_BIT_DATABASE_ENGINE__) || defined (__ENABLE_64_BIT_INTEGERS__)
int futils_GetDiskSpace(char *partition, __ULLWORD__ &free_space,
			__ULLWORD__ &total_bytes, __ULLWORD__ &bytes_used)
// Determine the free space in a partition. Returns a
// non-zero value if an error occurs or zero to indicate
// success. All UNIX variants will return the total number
// of bytes available to a non-superuser
{
#if defined (__WIN32__)
  typedef BOOL (WINAPI *P_GDFSE)(LPCTSTR, PULARGE_INTEGER, 
				 PULARGE_INTEGER, PULARGE_INTEGER);
  BOOL  fResult;
  P_GDFSE pGetDiskFreeSpaceEx = NULL;

  // NOTE: Under Windows 95A you will get the following runtime error if the 
  // GetDiskFreeSpaceEx() function is used: missing export in Kernel32.DLL. 
  // (KERNEL32.DLL:GetDiskFreeSpaceExA). The GetDiskFreeSpaceEx() function is 
  // only supported under Windows 95 OSR2 (also called Windows 95b) and higher.
  // Since the GetDiskFreeSpaceEx() function is not available in Windows 95A 
  // we can dynamically link to it and only call it if it is present in the 
  // current version of Windows the application is running under. We don't 
  // need to call LoadLibrary on KERNEL32.DLL because it is already loaded 
  // into every Win32 process's address space.
  pGetDiskFreeSpaceEx = \
 (P_GDFSE)GetProcAddress(GetModuleHandle("kernel32.dll"),
			 "GetDiskFreeSpaceExA");
  if(pGetDiskFreeSpaceEx) {
    PULARGE_INTEGER avail_bytes = new ULARGE_INTEGER;
    PULARGE_INTEGER bytes_total = new ULARGE_INTEGER;
    PULARGE_INTEGER free_bytes  = new ULARGE_INTEGER;
    fResult = pGetDiskFreeSpaceEx(partition, avail_bytes, bytes_total, 
				  free_bytes);
    if(fResult) {
      total_bytes = ((((__ULLWORD__)bytes_total->HighPart) << 32) | 
		     ((__ULLWORD__)bytes_total->LowPart));
      free_space = ((((__ULLWORD__)free_bytes->HighPart) << 32) | 
		    ((__ULLWORD__)free_bytes->LowPart));
      bytes_used = total_bytes - free_space;
      delete avail_bytes;
      delete bytes_total;
      delete free_bytes;
    }
    else {
      delete avail_bytes;
      delete bytes_total;
      delete free_bytes;
      return -1;
    }
  }
  else { // Use the Windows 95A code
    unsigned long bytesPerSector, sectorsPerCluster;
    unsigned long freeClusters, totalClusters;
    if(GetDiskFreeSpace(partition, &sectorsPerCluster, &bytesPerSector,
			&freeClusters, &totalClusters)) {
      free_space = \
	sectorsPerCluster * bytesPerSector * (__LLWORD__)freeClusters;
      total_bytes = \
	sectorsPerCluster * bytesPerSector * (__LLWORD__)totalClusters;
      bytes_used = total_bytes - free_space;
    }
    else {
      return -1;

    }
  }

#elif defined (__UNIX__) && defined (__LINUX__)
  struct statfs vfsinfo;

  if(statfs(partition, &vfsinfo)!= 0) return -1;
  __ULLWORD__ block_size = (__ULLWORD__)vfsinfo.f_bsize;
  free_space = (__ULLWORD__)vfsinfo.f_bavail * block_size;
  total_bytes = (__ULLWORD__)vfsinfo.f_blocks * block_size;
  bytes_used  = ((__ULLWORD__)vfsinfo.f_blocks - \
		 (__ULLWORD__)vfsinfo.f_bfree) * block_size;
  
#elif defined (__UNIX__)
  struct statvfs vfsinfo;
  if(statvfs(partition, &vfsinfo)!= 0) return -1;
  __ULLWORD__ block_size = vfsinfo.f_frsize;
  free_space = (__ULLWORD__)vfsinfo.f_bavail * block_size;
  total_bytes = (__ULLWORD__)vfsinfo.f_blocks * block_size;
  bytes_used = ((__ULLWORD__)vfsinfo.f_blocks - \
		(__ULLWORD__)vfsinfo.f_bfree) * block_size;
#else
#error You must define a target platform: __WIN32__ or __UNIX__
#endif

  return 0;
} 
#endif // __64_BIT_DATABASE_ENGINE__ || __ENABLE_64_BIT_INTEGERS__
// ----------------------------------------------------------- //

#ifdef __BCC32__
#pragma warn .8057
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //

