// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: btstack.h
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

Stack class used by the Btree class in place of the processor
stack to store node addresses during non-recursive insertions
and deletions.

Changes:
==============================================================
02/08/2002: All B-tree stack functions using non-persistent file 
address units have been modified to use FAU_t data types instead 
of FAU types. FAU_t types are native or built-in integer types 
and require no additional overhead to process. All persistent 
file address units still use the FAU integer type, which is a 
platform independent type allowing database files to be shared 
across multiple platforms.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_BTREE_STACK_HPP__
#define __GX_BTREE_STACK_HPP__

#include "gxdlcode.h"

#include "keytypes.h"

struct GXDLCODE_API BtreeStack_t
{
  BtreeStack_t() { next = prev = 0; node_address = (FAU_t)0; }
  BtreeStack_t(FAU_t na) { next = prev = 0; node_address = na; }
  ~BtreeStack_t() { }

  FAU_t node_address;   // 32-bit or 64-bit btree node address
  BtreeStack_t *next; // Pointer to the next node in the list
  BtreeStack_t *prev; // Pointer to the previous node in the list
};

// Btree stack class
class GXDLCODE_API BtreeStack
{
public:
  BtreeStack() { head = tail = 0; }
  ~BtreeStack();

public: 
  void Clear(); 
  int IsEmpty() { return head == 0; }
  int IsEmpty() const { return (const BtreeStack_t *)head == 0; }
  BtreeStack_t *GetHead() { return head; }
  BtreeStack_t *GetTail() { return tail; }
  
  // Stack functions
  BtreeStack_t *Push(FAU_t node_address);
  FAU_t Pop();

  // Queue functions
  BtreeStack_t *Insert(FAU_t node_address);
  FAU_t Extract();

private: // Internal processing functions
  FAU_t FreeNode(BtreeStack_t *n);
  BtreeStack_t *AllocNode(FAU_t node_address);
  void MakeEmpty();
  FAU_t RemoveHead();
  FAU_t RemoveTail();
  void DetachNode(BtreeStack_t *);
  void InsertBefore(BtreeStack_t *pos, BtreeStack_t *n);
  void InsertAfter(BtreeStack_t *pos, BtreeStack_t *n);
  
private:
  BtreeStack_t *head; // First node in the list
  BtreeStack_t *tail; // Last node in the list
};

#endif // __GX_BTREE_STACK_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
