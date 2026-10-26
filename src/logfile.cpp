// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: logfile.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/17/1995 
// Date Last Modified: 10/26/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- // 
// ------------- Program description and details ------------- // 
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

namespace datareel {

bool LogFile::Open(const std::string &fname)
// Open an existing log file without overwriting it, or create it.
{
  std::lock_guard<std::mutex> lock(mtx);
  if(stream.is_open()) stream.close();
  stream.clear();
  stream.open(fname, std::ios::out | std::ios::app);
  file_name = fname;
  return stream.is_open();
}

bool LogFile::OverWrite(const std::string &fname)
// Create a log file, truncating it if it exists.
{
  std::lock_guard<std::mutex> lock(mtx);
  if(stream.is_open()) stream.close();
  stream.clear();
  stream.open(fname, std::ios::out | std::ios::trunc);
  file_name = fname;
  return stream.is_open();
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
  line.append(message.data(), message.size());
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
