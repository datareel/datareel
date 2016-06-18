// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxlistb.h
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 04/05/1996
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

Doubly linked list base class. The gxListNodeB and gxListB classes
can be used as base classes to create a family of link-list class
specific to a particular data type. These classes can also be used
as non-base classes provided that each data type is cast to a void
pointer.

Changes:
==============================================================
03/24/2002: Added the Release() synchronization function. The 
release function is used to signal to the class destructor not  
to delete the list when an object is deleted or no longer 
visible.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_LIST_BASE_CLASS_HPP__
#define __GX_LIST_BASE_CLASS_HPP__

#include "gxdlcode.h"

// Linked list node class
class GXDLCODE_API gxListNodeB
{
public:
  gxListNodeB() { next = prev = 0; data = 0; }
  virtual ~gxListNodeB();

public:
  void *data;        // Pointer to this node data
  gxListNodeB *next; // Pointer to the next node in the list
  gxListNodeB *prev; // Pointer to the previous node in the list
};

// Linked list class
class GXDLCODE_API gxListB
{
public:
  gxListB() { head = tail = 0; }
  virtual ~gxListB();

private: // Disallow copying and assignment
  gxListB(const gxListB &ob) { }
  void operator=(const gxListB &ob) { }
  
public: // Pointer manipulation operations
  gxListNodeB *GetHead() { return head; }
  gxListNodeB *GetHead() const { return head; }
  gxListNodeB *GetTail() { return tail; }
  gxListNodeB *GetTail() const { return tail; }
  void DetachNode(gxListNodeB *n);
  void MoveToFront(gxListNodeB *n);
  void MoveToBack(gxListNodeB *n);
  void MoveBefore(gxListNodeB *pos, gxListNodeB *n);
  void MoveAfter(gxListNodeB *pos, gxListNodeB *n);
  void InsertBefore(gxListNodeB *pos, gxListNodeB *n);
  void InsertAfter(gxListNodeB *pos, gxListNodeB *n);
  void InsertAtHead(gxListNodeB *n);
  void InsertAtTail(gxListNodeB *n);

public: // Add and remove functions
  virtual gxListNodeB *Add(void *v);
  gxListNodeB *AddToFront(void *v);
  gxListNodeB *AddToBack(void *v);
  gxListNodeB *AddAfter(gxListNodeB *pos, void *v);
  gxListNodeB *AddBefore(gxListNodeB *pos, void *v);
  void ClearList(); 
  void Clear() { ClearList(); }
  void DestroyList();
  void MakeEmpty(); // Called only if all the nodes have been removed 
  void *RemoveHead();
  void *RemoveTail();
  void *Remove(gxListNodeB *n);
  
public: // Stack functions
  gxListNodeB *Push(void *v);
  void *Pop();

public: // Queue functions
  gxListNodeB *Insert(void *v);
  void *Extract();

protected: // Allocation/de-allocation functions
  virtual void *FreeNode(gxListNodeB *n);
  virtual gxListNodeB *AllocNode(void *v);
  
public:
  int IsEmpty() { return head == 0; }
  int IsEmpty() const { return (const gxListNodeB *)head == 0; }
  
public: // Synchronization functions
  void Release() {
    // The release function is used to release this object's list  
    // without clearing the list or performing any other action. 
    // NOTE: The release function is used to release this list when  
    // more then one object is referencing this list. After calling   
    // the release function the object previously bound to this list 
    // can safely be deleted without affecting any other objects   
    // bound to this list. This allows the list data to remain in  
    // memory until all threads accessing this list have exited or 
    // released it.
    head = tail = 0;
  }

protected:
  gxListNodeB *head; // First node in the list
  gxListNodeB *tail; // Last node in the list
};

#endif // __GX_LIST_BASE_CLASS_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
