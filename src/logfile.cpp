// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: logfile.cpp
// C++ Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/17/1995 
// Date Last Modified: 11/01/2026
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

10/26/2026: DataReel 7 rewrites LogFile on std::ofstream.
==============================================================
*/
// ----------------------------------------------------------- //   
#include "gxdlcode.h"

#include "logfile.h"
#include "systime.h"

#if !defined (_WIN32)
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace datareel {

// 11/01/2026: Open the log file safely before the stream opens it.
// POSIX: create the file with mode 0640 (less the umask) instead of the
// stream's 0666, refuse a symbolic link (O_NOFOLLOW) and anything that
// is not a regular file, then check that the stream opened the same
// file. An existing file keeps its permissions. Windows uses the
// stream's defaults.
static bool logfile_open(std::ofstream &stream, const std::string &fname,
			 bool truncate)
{
  std::ios::openmode mode = std::ios::out | 
    (truncate ? std::ios::trunc : std::ios::app);
#if defined (_WIN32)
  stream.open(fname, mode);
  return stream.is_open();
#else
  int flags = O_WRONLY | O_CREAT | (truncate ? O_TRUNC : O_APPEND);
#if defined (O_NOFOLLOW)
  flags |= O_NOFOLLOW;
#endif
#if defined (O_CLOEXEC)
  flags |= O_CLOEXEC;
#endif
#if defined (O_NONBLOCK)
  flags |= O_NONBLOCK; // Do not hang on a FIFO; it is rejected below
#endif
  int fd = ::open(fname.c_str(), flags, 0640);
  if(fd < 0) return false;
  struct stat fst;
  if(fstat(fd, &fst) != 0 || !S_ISREG(fst.st_mode)) {
    ::close(fd);
    return false;
  }
  stream.open(fname, mode);
  // The name must still be the regular file opened above
  struct stat lst;
  bool same = stream.is_open() && lstat(fname.c_str(), &lst) == 0 &&
    S_ISREG(lst.st_mode) && lst.st_dev == fst.st_dev && 
    lst.st_ino == fst.st_ino;
  ::close(fd);
  if(!same) {
    if(stream.is_open()) stream.close();
    return false;
  }
  return true;
#endif
}

// 11/01/2026: WriteLine() writes one entry per call. A CR or LF inside
// the message (for example from a user name or network data) could 
// start what looks like a second, forged entry, so both are written as
// the two character escapes \r and \n. A backslash itself is not
// escaped. Use Write() for text that is meant to span lines.
static void logfile_append_escaped(std::string &line, std::string_view message)
{
  for(char c : message) {
    if(c == '\n') line += "\\n";
    else if(c == '\r') line += "\\r";
    else line += c;
  }
}

bool LogFile::Open(const std::string &fname)
// Open an existing log file without overwriting it, or create it.
{
  std::lock_guard<std::mutex> lock(mtx);
  if(stream.is_open()) stream.close();
  stream.clear();
  file_name = fname;
  return logfile_open(stream, fname, false); // 11/01/2026
}

bool LogFile::OverWrite(const std::string &fname)
// Create a log file, truncating it if it exists.
{
  std::lock_guard<std::mutex> lock(mtx);
  if(stream.is_open()) stream.close();
  stream.clear();
  file_name = fname;
  return logfile_open(stream, fname, true); // 11/01/2026
}

void LogFile::Close() 
{ 
  std::lock_guard<std::mutex> lock(mtx);
  if(stream.is_open()) stream.close();
}

bool LogFile::Flush() 
{ 
  std::lock_guard<std::mutex> lock(mtx);
  if(!stream.is_open()) return false;
  stream.flush();
  return bool(stream);
}

bool LogFile::WriteLine(std::string_view message)
{
  std::string line = date_time_string();
  line += ' ';
  logfile_append_escaped(line, message); // 11/01/2026: Escape CR/LF
  line += '\n';
  std::lock_guard<std::mutex> lock(mtx);
  if(!stream.is_open()) return false;
  stream.write(line.data(), std::streamsize(line.size()));
  stream.flush();
  return bool(stream);
}

bool LogFile::Write(std::string_view text)
{
  std::lock_guard<std::mutex> lock(mtx);
  if(!stream.is_open()) return false;
  stream.write(text.data(), std::streamsize(text.size()));
  return bool(stream);
}

bool LogFile::WriteSysTime()
{
  std::string ts = " " + date_time_string() + " ";
  return Write(ts);
}

} // namespace datareel

// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
