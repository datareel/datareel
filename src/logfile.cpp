// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: logfile.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/17/1995 
// Date Last Modified: 10/31/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- // 
// ------------- Program description and details ------------- // 
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

The LogFile class is used to log program events.

Changes:
==============================================================
02/27/2003: Modified the LogFile::Open() and LogFile::OverWrite() 
functions to correctly set the default translation mode to text 
after opening or overwriting a log file.
==============================================================
*/
// ----------------------------------------------------------- //   
#include "gxdlcode.h"

#include <string.h>
#include "logfile.h"
#include "systime.h"
#include "gxdfptr.h" // 10/31/2026: gxdFPTRCreatePrivate()

#if defined (__UNIX__)
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

static int logfile_unix_open(const char *fname, int truncate, struct stat *st)
// 10/31/2026: Open the log file name before the DiskFileB functions
// open it, without following a symbolic link (O_NOFOLLOW). A link
// planted at the log file name (in /tmp, or a directory other users can
// write) used to send the log, and OverWrite()'s truncation, to the
// file it pointed at; a dangling link created that file. A new file is
// created owner-only (mode 0600) in the same call, so it never exists
// with the umask permissions. Only regular files and character devices
// (/dev/null, a tty) are accepted; OverWrite() truncates through this
// descriptor. Returns the descriptor, or -1 if the name is refused. The
// caller checks that the name still refers to "st" after opening it.
{
  if(!fname) return -1;
  int flags = O_WRONLY | O_CREAT;
#if defined (O_NOFOLLOW)
  flags |= O_NOFOLLOW;
#endif
#if defined (O_CLOEXEC)
  flags |= O_CLOEXEC;
#endif
#if defined (O_NONBLOCK)
  flags |= O_NONBLOCK; // Do not hang on a FIFO; it is refused below
#endif
  int fd = open(fname, flags, S_IRUSR|S_IWUSR);
  if(fd < 0) return -1;
  if((fstat(fd, st) != 0) || 
     (!S_ISREG(st->st_mode) && !S_ISCHR(st->st_mode))) {
    close(fd);
    return -1;
  }
  if(truncate && S_ISREG(st->st_mode) && (ftruncate(fd, 0) != 0)) {
    close(fd);
    return -1;
  }
  return fd;
}

static int logfile_unix_same(const char *fname, const struct stat *st)
// 10/31/2026: True if "fname" is still the file checked by
// logfile_unix_open() and not a symbolic link
{
  struct stat lst;
  if(lstat(fname, &lst) != 0) return 0;
  return !S_ISLNK(lst.st_mode) && (lst.st_dev == st->st_dev) && 
    (lst.st_ino == st->st_ino);
}
#else

static void logfile_create_private(const char *fname)
// 10/31/2026: A new log file is created owner-only (mode 0600 on UNIX,
// a protected DACL for the current user on Windows). It used to get the
// umask permissions (found by the DataReel 7.3 security sweep). The
// access of an existing log file is not changed.
{
  if(!fname || gxdFPTRExists(fname)) return;
  gxdFPTR *f = gxdFPTRCreatePrivate(fname);
  if(f) gxdFPTRClose(f);
}
#endif // __UNIX__

#ifdef __BCC32__
#pragma warn -8080
#endif

int LogFile::Open(const char *fname)
// Public member function use to open an existing logfile without 
// overwriting it. Returns zero to indicate success or a non-zero 
// value to indicate a failure.
{
  int rv;
#if defined (__UNIX__)
  // 10/31/2026: Refuse a symbolic link, create new files 0600
  struct stat st;
  int fd = logfile_unix_open(fname, 0, &st);
  if(fd < 0) {
    df_Close();
    return df_last_error = DiskFileB::df_OPEN_ERROR;
  }
  rv = df_Open(fname, DiskFileB::df_READWRITE, DiskFileB::df_APPEND);
  close(fd);
  if((rv == DiskFileB::df_NO_ERROR) && !logfile_unix_same(fname, &st)) {
    df_Close(); // The name was changed while it was opened
    rv = df_last_error = DiskFileB::df_OPEN_ERROR;
  }
#else
  logfile_create_private(fname); // 10/31/2026
  if(df_Exists(fname)) {
    rv = df_Open(fname, DiskFileB::df_READWRITE, DiskFileB::df_APPEND);
  }
  else {
    rv =  df_Create(fname);
  }
#endif
  df_Text(); // Turn on text translation for all logfiles
  return rv;
}

int LogFile::OverWrite(const char *fname)
// Public member function use to open an existing logfile and 
// overwrite it. Returns zero to indicate success or a non-zero 
// value to indicate a failure.
{
#if defined (__UNIX__)
  // 10/31/2026: Refuse a symbolic link, create new files 0600 and
  // truncate through the checked descriptor, then open without
  // truncating by name
  struct stat st;
  int fd = logfile_unix_open(fname, 1, &st);
  if(fd < 0) {
    df_Close();
    return df_last_error = DiskFileB::df_CREATE_ERROR;
  }
  int rv = df_Open(fname, DiskFileB::df_READWRITE, DiskFileB::df_NO_APPEND);
  close(fd);
  if((rv == DiskFileB::df_NO_ERROR) && !logfile_unix_same(fname, &st)) {
    df_Close(); // The name was changed while it was opened
    rv = df_last_error = DiskFileB::df_CREATE_ERROR;
  }
#else
  logfile_create_private(fname); // 10/31/2026
  int rv = df_Create(fname);
#endif
  df_Text(); // Turn on text translation for all logfiles
  return rv;
}

int LogFile::Close() 
{ 
  return df_Close(); 
}

int LogFile::Flush() 
{ 
  return df_Flush(); 
}

int LogFile::WriteSysTime()
{
  if(df_is_open) {
    SysTime date;
    *this << ' ' << date.GetSystemDateTime(0) << ' ';
    return df_last_error;
  }
  else {
    return df_last_error = DiskFileB::df_OPEN_ERROR;
  }
}

char *LogFile::GetSystemTime(char *s, int full_month_name)
// Function used to create a custom time string. This function
// assumes that enough memory to hold the date/time string has
// been allocated for the "s" buffer. If "s" is a null pointer
// heap memory will be allocated for the string.
{
  if(!s) { // This is a null pointer
    s = new char[255]; // Allocate memory for the string buffer
    if(!s) return 0;
  } 
  SysTime date;
  strcpy(s, date.GetSystemDateTime(full_month_name));
  return s;
}

#ifdef __BCC32__
#pragma warn .8080
#endif
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
