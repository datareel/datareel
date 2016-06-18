// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxdstats.h 
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/03/1997  
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

The database file statistics functions are used to display
detailed file information about open database files. 
This information is used to analyze and troubleshoot 
database files. 
*/
// ----------------------------------------------------------- //
#ifndef __GX_DATABASE_STATS_HPP__
#define __GX_DATABASE_STATS_HPP__

#include "gxdlcode.h"

#include "gxdbase.h"

GXDLCODE_API void DatabaseStats(gxDatabase *f);
GXDLCODE_API void BlockStats(gxDatabase *f, FAU oa);
GXDLCODE_API int CheckError(gxDatabase *f);
GXDLCODE_API int CheckError(gxDatabaseError err);
GXDLCODE_API void EchoDatabaseVersion();
GXDLCODE_API void EchoDatabaseVersion(gxDatabase *f);

#endif // __GX_DATABASE_STATS_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
