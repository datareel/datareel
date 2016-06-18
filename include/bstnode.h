// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: bstnode.h
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 01/23/1997 
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

Binary search tree node base class.
*/
// ----------------------------------------------------------- //   
#ifndef __GX_BSTREE_NODE_HPP__
#define __GX_BSTREE_NODE_HPP__

#include "gxdlcode.h"

// The binary search tree node base class is designed to be
// shared by multiple tree implementations regardless of the
// data type the tree is using.
struct GXDLCODE_API gxBStreeNode_t
{
  gxBStreeNode_t *left;  // Pointer to the left sub-tree
  gxBStreeNode_t *right; // Pointer to the right sub-tree
};

#endif // __GX_BSTREE_NODE_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
