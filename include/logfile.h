// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: logfile.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/17/1995 
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

The LogFile class is used to log program events.

Changes:
==============================================================
03/10/2002: The LogFile class no longer uses the C++ fstream 
class as the underlying file system. The gxDatabase file pointer 
routines are now used to define the underlying file system used by 
the LogFile class. This change was made to support large files 
and NTFS file system enhancements. To enable large file support 
users must define the __64_BIT_DATABASE_ENGINE__ preprocessor 
directive.

09/30/2002: Added the LogFile::LogFile(const char *) constructor.
Used to open a log file for appending when a LogFile object is
constructed.

11/14/2002: Added the LogFile::Create() function used to create
a logfile overwriting any existing logfile with the same name. 

02/27/2003: Modified the LogFile constructor to set the default
translation mode to text. This changes allows integer and floating 
point values to be stored in the log file as plain text.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_LOGFILE_HPP__
#define __GX_LOGFILE_HPP__

#include "gxdlcode.h"

#include "dfileb.h"

// Class used to log program events. 
class GXDLCODE_API LogFile : public DiskFileB
{
public:
  LogFile() { df_Text(); }
  LogFile(const char *fname) { Open(fname); df_Text(); }
  ~LogFile() { }

public:
  int Open(const char *fname);
  int OverWrite(const char *fname);
  int Create(const char *fname) { return OverWrite(fname); }
  int Close();
  int Flush();
  
public:
  int WriteSysTime();
  char *GetSystemTime(char *s, int full_month_name = 0);
};

#endif // __GX_LOGFILE_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
