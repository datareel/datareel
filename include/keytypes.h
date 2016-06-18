// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: keytypes.h
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 08/22/2000 
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

This file contains typedefs and constants used by the DatabaseKeyB
base class, Btree class, and Btree node class.

Changes:
==============================================================
02/22/2002: Changed the BtreeNodeOrder_t, BtreeSize_t, 
BtreeKeyCount_t, BtreeKeyLocation_t to native integer types.

02/22/2002: Added the BtreeNodeOrder, BtreeSize, BtreeKeyCount, 
and the BtreeKeyLocation platform independent types.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_DATABASE_KEY_TYPES_HPP__
#define __GX_DATABASE_KEY_TYPES_HPP__

#include "gxdlcode.h"

#include "gxdbase.h"

#if defined (__64_BIT_DATABASE_ENGINE__)
typedef gxINT64 gxClassID;       // Persistent class ID type
typedef __LLWORD__ gxClassID_t;  // Native class ID type 
typedef gxINT64 gxObjectID;      // Persistent object ID type
typedef __LLWORD__ gxObjectID_t; // Native object ID type

#else // Use the 32-bit version by default
typedef gxINT32 gxClassID;      // Persistent class ID type
typedef __LWORD__ gxClassID_t;  // Native class ID type 
typedef gxINT32 gxObjectID;     // Persistent object ID type
typedef __LWORD__ gxObjectID_t; // Native object ID type
#endif

// Native types used in both the 32-bit and 64-bit Btree
typedef __LWORD__ BtreeNodeOrder_t;   // Type used for the Btree node order
typedef __LWORD__ BtreeSize_t;        // Type used to store entry key sizes
typedef __LWORD__ BtreeKeyCount_t;    // Type used for the Btree node key count
typedef __LWORD__ BtreeKeyLocation_t; // Type used for Btree node key locations

// Platform independent types used in both the 32-bit and 64-bit Btree  
typedef gxINT32 BtreeNodeOrder;   // Type used for the Btree node order
typedef gxINT32 BtreeSize;        // Type used to Btree size values
typedef gxINT32 BtreeKeyCount;    // Type used for the Btree node key count
typedef gxINT32 BtreeKeyLocation; // Type used for Btree node key locations

#endif // __GX_DATABASE_KEY_TYPES_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
