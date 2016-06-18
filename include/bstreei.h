// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: bstreei.h
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

Binary search tree iterator class used for sort order tree
iterations.
*/
// ----------------------------------------------------------- //   
#ifndef __GX_BSTREE_ITERATOR_HPP__
#define __GX_BSTREE_ITERATOR_HPP__

#include "gxdlcode.h"

#include "gxlist.h"
#include "bstnode.h"

#if defined (__MSVC__) && defined (__MSVC_DLL__)
#pragma warning(disable:4251) // Disable dll-interface template warning
#endif

class GXDLCODE_API gxBStreeIterator
{
public:
  gxBStreeIterator(gxBStreeNode_t *node) { Reset(node); }
  ~gxBStreeIterator() { }

public:
  void Reset() { Reset(root); }
  void Reset(gxBStreeNode_t *node);
  gxBStreeNode_t *GetNext();
    
private:
  gxList<gxBStreeNode_t *> path; // Current path of parent nodes 
  gxBStreeNode_t *root;          // Start of the tree being iterated
  gxBStreeNode_t *curr;          // Current node being traversed
};

#if defined (__MSVC__) && defined (__MSVC_DLL__)
#pragma warning(default:4251) // Restore previous warning
#endif

#endif // __GX_BSTREE_ITERATOR_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
