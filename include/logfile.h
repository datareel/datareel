// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: logfile.h
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
// Date Last Modified: 10/26/2026
// Copyright (c) 2001-2026 DataReel Software Development
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

The LogFile class is used to log program events. WriteLine() is
thread safe and writes one time-stamped line per call, so several
threads can share one log file.

Changes:
==============================================================
02/27/2003: Modified the LogFile::Open() and LogFile::OverWrite()
functions to correctly set the default translation mode to text
after opening or overwriting a log file.

10/26/2026: DataReel 7 rewrites LogFile on std::ofstream. It no
longer derives from DiskFileB. Open(), OverWrite() and Create()
return true on success. WriteLine() writes a time stamp, the
message and a new line under a mutex.
==============================================================
*/
// ----------------------------------------------------------- //
#ifndef __GX_LOGFILE_HPP__
#define __GX_LOGFILE_HPP__

#include "gxdlcode.h"

#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

namespace datareel {

class GXDLCODE_API LogFile
{
public:
  LogFile() { }
  explicit LogFile(const std::string &fname) { Open(fname); }
  ~LogFile() { Close(); }

  LogFile(const LogFile &) = delete;
  LogFile &operator=(const LogFile &) = delete;

public:
  // Open for append, creating the file if needed
  bool Open(const std::string &fname);

  // Create a new file or truncate an existing one
  bool OverWrite(const std::string &fname);
  bool Create(const std::string &fname) { return OverWrite(fname); }

  void Close();
  bool Flush();
  bool IsOpen() const { return stream.is_open(); }
  const std::string &FileName() const { return file_name; }

public:
  // Write "Oct 26, 2026 14:05:09 message\n" as one locked write
  bool WriteLine(std::string_view message);

  // Write text as-is, with no time stamp or new line
  bool Write(std::string_view text);

  // Write " Oct 26, 2026 14:05:09 " (kept for 6.x callers)
  bool WriteSysTime();

  // Stream insertion for building a line piece by piece. Not
  // thread safe across calls; use WriteLine() from several threads.
  template<class T>
  LogFile &operator<<(const T &value) {
    std::lock_guard<std::mutex> lock(mtx);
    if(stream.is_open()) stream << value;
    return *this;
  }

private:
  std::ofstream stream;
  std::string file_name;
  std::mutex mtx;
};

} // namespace datareel

#endif // __GX_LOGFILE_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
