// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxd_ver.h
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/05/1997  
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

This include file is used to define the current version number
and revision letter of the GX thread library. 

Changes:
==============================================================
02/03/2002: Changed GX database default revision letter to 
rev E.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_DATABASE__VERSION_HPP__
#define __GX_DATABASE__VERSION_HPP__

#include "gxdlcode.h"

// Current GX Database version number.
const long gxDatabaseVersionNumber = 4630;

// Version specific changes
const double gxDatabaseRevisionNumber = .101;

// GXD library distribution version number
const double gxDatabaseVersion = gxDatabaseVersionNumber + \
gxDatabaseRevisionNumber;

// ----------------------------------------- //
// Set the GX Database revision letter here.
// ----------------------------------------- //
const char gxDatabaseRevisionLetter = 'E';

// The version letter will determine the amount of overhead per
// database block. Any of these revision letters can be used to
// create files with the gxDatabase file manager, version 2000.101
// and higher. When an existing file is opened the version/revision
// stored in the file header will be used.

// GXD revision letter zero (denoted by any of the following: 0, '\0', 
// or ' ') excludes the persistent checksum value, the persistent file
// lock header, and the persistent record lock header. The total revision
// zero overhead per block equals 16 bytes.

// GXD revision 'A' reserves space at the end of each block for
// an optional persistent checksum value and excludes the persistent
// file lock header, and the persistent record. The total revision 'A'
// overhead per block equals 20 bytes.

// GXD revision 'B' includes the persistent checksum value with addition
// of a persistent file lock header. The total revision 'B' overhead per
// block equals 20 bytes.

// GXD revision 'C' includes all the features of revision 'A' and 'B' with
// the addition of persistent lock headers. The total revision 'C' overhead
// per block equals 32 bytes. 

// GXD revision 'D' includes all the features of revision 'A', 'B', and 'C'
// with the addition of persistent file statistics. Persistent file stats
// are use to count the number of normal, removed, and deleted blocks. The 
// total revision 'D' overhead per block equals 32 bytes. 

// GXD revision 'E' excludes the persistent checksum value and the persistent 
// record lock header. The total revision zero overhead per block equals 16 
// bytes. Revision 'E' includes persistent file lock header and persistent 
// file statistics offering file locking with the least amount of overhead
// per block.
// ----------------------------------------- //

#endif // __GX_DATABASE__VERSION_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
