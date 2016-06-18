// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: bstreei.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 01/23/1997 
// Date Last Modified: 06/17/2016
// Copyright (c) 2001-2016 DataReel Software Development
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

Binary search tree iterator class used for sort order tree
iterations.
*/
// ----------------------------------------------------------- //   
#include "gxdlcode.h"

#include "bstreei.h"

void gxBStreeIterator::Reset(gxBStreeNode_t *node)
// Reset the tree iterator object.  
{
  root = node;
  curr = root;
  path.ClearList();
}

gxBStreeNode_t *gxBStreeIterator::GetNext()
// Returns the next node in the tree in sort order.
{
  while(1) { // PC-lint 05/23/2002: while(1) loop (ignore)
    if(curr) {
      path.Push(curr); // PC-lint 04/20/2004: Ignoring return value (ignore)
      curr = curr->left;
    }
    else {
      if(path.Pop(curr) == 0) {
	return curr = 0;
      }
      else {
	gxBStreeNode_t *p = curr;
	curr = curr->right;
	return p;
      }
    }
  }
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
