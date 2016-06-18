// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Header File Name: futils.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 01/25/2000
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

General-purpose platform independent file and directory functions
that need to be ported between WIN32 and UNIX platforms. 

Changes:
==============================================================
03/11/2002: Added large file support for the futils_filelength()
function. Also, the file length function now takes the string
file name instead of an int type representing an open file
handle.

08/12/2002: Added the "dirent *entry = 0" parameter to the 
futils_readdir() function to support thread safe readdir calls
under UNIX.

10/17/2002: Added the futils_chmod(const char *fname, const char *mode_str)
function used to set file permission under UNIX.

06/11/2003: Added the futils_hasdriveletter(const char *, char &)
function to replace the futils_hasdriveletter(const char *, char *)
function.

06/13/2003: Added the futils_timestamp() function used to get a file's 
last access time, modification time, creation time.

06/17/2003: Added the futils_GetDiskSpace() function used with 64-bit
database engine to calculate disk statistics.

06/19/2003: Added the futils_attrib() function used to retrieve DOS 
style attributes for Windows and UNIX files.

06/20/2003: Added the futils_access() used to determine read, write, 
or execute access to a file before reading or writing to a file.

06/21/2003: Added the futils_mode() function used to get UNIX style 
permissions for Windows and UNIX files. 

06/21/2003: Added the futils_getmode() function used to get a file's 
permissions, user and group ID for UNIX files.
==============================================================
*/
// ----------------------------------------------------------- // 
#ifndef __GX_FILE_UTILITIES_HPP__
#define __GX_FILE_UTILITIES_HPP__

#include "gxdlcode.h"

// Non-platform specific include files
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

// Platform specific include files 
#if defined (__WIN32__)
// WIN32 include files for file/directory functions 
#include <windows.h>
#include <direct.h>
#include <io.h>
#include <share.h>

#elif defined (__UNIX__)
// UNIX include files for file/directory functions for all
// UNIX variants. NOTE: Place specific UNIX variants above
// this directive.
#include <unistd.h> // UNIX standard function definitions
#include <dirent.h> // POSIX directory stream functions 
#else
#error You must define a target platform: __WIN32__ or __UNIX__
#endif

// 09/07/2006: Includes for utime calls
#if defined (__WIN32__)
#include <sys/utime.h>
#endif

#if defined (__LINUX__)
#include <utime.h>
#endif

// TODO: Have to add utime includes for HPUX and Solaris
// after testing set timestamp function.

#if defined (__64_BIT_DATABASE_ENGINE__) || defined (__ENABLE_64_BIT_INTEGERS__)
#include "gxdtyp64.h"

// Platform specific UNIX include files required for 
// 64-bit disk space calculations
#if defined (__UNIX__) && defined (__LINUX__)
#include <sys/statfs.h>
#elif defined (__UNIX__)
#include <sys/statvfs.h>
#endif
#endif

// Platform independent path and directory length definitions 
const unsigned futils_MAX_LINE_LENGTH = 1024; // Max line length
const unsigned futils_MAX_NAME_LENGTH = 255;  // Max length of a file name
const unsigned futils_MAX_DIR_LENGTH  = 1024; // Max length of a directory name
const unsigned futils_MAX_PATH_LENGTH = 1024; // Max absolute or relative path 

// Enumerations 
typedef enum { 
  // Permission mode enumeration needed to set file permissions
  // when new files are created. 
  futils_READONLY,
  futils_WRITEONLY,
  futils_READWRITE
} futilPermissionMode;

// ---------------------------------------------------------- //
// POSIX Directory stream and directory entry data structures //
// ---------------------------------------------------------- //
#if defined (__WIN32__)
// POSIX style directory entry for WIN32 platforms
typedef struct dirent {
  HANDLE d_ino;   // File number of entry 
  DWORD d_reclen; // Length of this record 
  DWORD d_namlen; // Length of string in d_name 
  char  d_name[futils_MAX_NAME_LENGTH]; // Name of this file 

  // WIN32 Specifics 
  WIN32_FIND_DATA file; // Describes a file found by a WIN32 find function 
} WIN32FileData;

// POSIX style directory structure for WIN32 platforms 
typedef struct {
  int dd_fd;            // optional file descriptor 
  DWORD dd_loc;         // file location in the directory stream 
  DWORD dd_size;        // file size  
  WIN32FileData dd_buf; // WIN32 file information 
  char dd_dirname[futils_MAX_NAME_LENGTH]; // Directory name 
} DIR;
#endif

// ----------------------------------------------------------- //
// Standalone platform indepentent file/directory functions    //
// ----------------------------------------------------------- //
GXDLCODE_API int futils_exists(const char *name);
GXDLCODE_API int futils_isdirectory(const char *dir_name);
GXDLCODE_API int futils_isfile(const char *fname);
GXDLCODE_API void futils_makeDOSpath(char *path);
GXDLCODE_API void futils_makeUNIXpath(char *path);
GXDLCODE_API int futils_chdir(const char *dir_name);
GXDLCODE_API int futils_rmdir(const char *dir_name);
GXDLCODE_API int futils_hasdriveletter(const char *dir_name, char &letter);
GXDLCODE_API int futils_hasdriveletter(const char *dir_name, char *letter);
GXDLCODE_API int futils_mkdir(const char *dir_name);
GXDLCODE_API int futils_remove(const char *fname);
GXDLCODE_API int futils_rename(const char *oldname, const char *newname);
GXDLCODE_API int futils_chmod(const char *fname, futilPermissionMode pmode);
GXDLCODE_API int futils_chmod(const char *fname, const char *mode_str);
GXDLCODE_API void futils_pathsimplify(const char *path, char *new_path, 
				      char path_sep);
GXDLCODE_API int futils_genoutputfilename(const char *current_file, 
					  char *out_file,
					  char *extension);
GXDLCODE_API int futils_getcwd(char *dir, unsigned max_len);
GXDLCODE_API int futils_timestamp(const char *fname, time_t &access_time, 
				  time_t &mod_time, time_t &create_time);
GXDLCODE_API int futils_attrib(const char *fname, char &ro, char &system,
			       char &hidden, char &arch);
GXDLCODE_API int futils_mode(const char *fname, int ugo, 
			     char &read, char &write, char &execute);
GXDLCODE_API int futils_getmode(const char *fname, int &mode, 
				int &uid, int &gid);
GXDLCODE_API int futils_strmode(int mode, char *sbuf, int max_len);
GXDLCODE_API int futils_access(const char *fname, char &exists, 
			       char &write, char &read);
GXDLCODE_API int futils_access(const char *fname, char &exists, 
			       char &write, char &read, char &execute);
GXDLCODE_API int futils_set_timestamp(const char *fname, 
				      time_t access_time, time_t mod_time);
GXDLCODE_API int futils_is_unc_path(const char *path);

// 09/13/2006: Extended stat functions for Windows and Linux
GXDLCODE_API int futils_islink(const char *name);
GXDLCODE_API int futils_isreg(const char *name);
GXDLCODE_API int futils_ischar(const char *name);
GXDLCODE_API int futils_isblock(const char *name);
GXDLCODE_API int futils_isfifo(const char *name);
GXDLCODE_API int futils_stat(const char *name, 
			     int *isFile, int *isDir, int *isLink = 0,
			     int *isChar = 0, int *isBlock = 0,
			     int *isFifo = 0, int *isReg = 0,
			     int *link_size = 0);

// 32-bit and 64-bit code
#if defined (__64_BIT_DATABASE_ENGINE__) || defined (__ENABLE_64_BIT_INTEGERS__)
GXDLCODE_API __LLWORD__ futils_filelength(const char *fname);
#else
GXDLCODE_API long futils_filelength(const char *fname);
#endif

// 64-bit only code
#if defined (__64_BIT_DATABASE_ENGINE__) || defined (__ENABLE_64_BIT_INTEGERS__)
int futils_GetDiskSpace(char *partition, __ULLWORD__ &free_space,
			__ULLWORD__ &total_bytes, __ULLWORD__ &bytes_used);
#endif
// ----------------------------------------------------------- //

// ----------------------------------------------------------- //
// Standalone platform indepentent directory stream functions  //
// ----------------------------------------------------------- //
GXDLCODE_API DIR *futils_opendir(const char *dirname);
GXDLCODE_API int futils_closedir(DIR *dirp);
GXDLCODE_API struct dirent *futils_readdir(DIR *dirp, dirent *entry = 0);
GXDLCODE_API void futils_rewinddir(DIR *dirp);
// ----------------------------------------------------------- //

#endif // __GX_FILE_UTILITIES_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //


