// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: thrpool.h
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 03/31/2000
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

The thrPool class is used to pool threads of a single or multiple
processes together. Each thread pool consists of a doubly linked
list of thrPoolNode nodes. A thrPoolNode node holds a pointer to
a gxThread_t object plus a pointer to previous and next node in
the list.
*/
// ----------------------------------------------------------- //   
#ifndef __GX_THREAD_POOL_HPP__
#define __GX_THREAD_POOL_HPP__

#include "gxdlcode.h"

// Foward class calling conventions
class gxThread_t;
class thrPool;

// Thread Pool Node Class
class GXDLCODE_API thrPoolNode
{
private:
  friend class thrPool;

public:
  thrPoolNode() { next = prev = 0; thread = 0; }
  thrPoolNode(gxThread_t *t) { next = prev = 0; thread = t; }
  ~thrPoolNode() { }
  
private: // Disallow copying and assignmnet
  thrPoolNode(const thrPoolNode &ob) { }
  void operator=(const thrPoolNode &ob) { }
  
public:
  thrPoolNode *GetNext() { return next; }  
  thrPoolNode *GetPrev() { return prev; } 
  gxThread_t *GetThreadPtr() { return thread; } 
  gxThread_t *GetThreadPtr() const { return thread; } 
  void SetThreadPtr(gxThread_t *t) { thread = t; }
  void SetThreadPtr(const gxThread_t *t) { thread = (gxThread_t *)t; }
  void SetNext(thrPoolNode *n) { next = n; }  
  void SetNext(const thrPoolNode *n) { next = (thrPoolNode *)n; }  
  void SetPrev(thrPoolNode *p) { prev = p; }
  void SetPrev(const thrPoolNode *p) { prev = (thrPoolNode *)p; }
  
private:
  gxThread_t *thread; // Pointer to the this node's thread
  thrPoolNode *next;  // Pointer to the next node in the list
  thrPoolNode *prev;  // Pointer to the previous node in the list
};

// Thread Pool Class
class GXDLCODE_API thrPool
{
public:
  thrPool() { head = tail = 0; }
  ~thrPool();

private: // Disallow copying and assignment
  thrPool(const thrPool &ob) { }
  void operator=(const thrPool &ob) { }

public: // Pointer manipulation operations
  void DetachNode(thrPoolNode *);
  void MoveToFront(thrPoolNode *n);
  void MoveToBack(thrPoolNode *n);
  void MoveBefore(thrPoolNode *pos, thrPoolNode *n);
  void MoveAfter(thrPoolNode *pos, thrPoolNode *n);
  void InsertBefore(thrPoolNode *pos, thrPoolNode *n);
  void InsertAfter(thrPoolNode *pos, thrPoolNode *n);
  void InsertAtHead(thrPoolNode *n);
  void InsertAtTail(thrPoolNode *n);

public: // Add and remove functions
  thrPoolNode *AddThread(gxThread_t *thread);
  thrPoolNode *AddToFront(gxThread_t *t);
  thrPoolNode *AddToBack(gxThread_t *t);
  thrPoolNode *AddAfter(thrPoolNode *pos, gxThread_t *t);
  thrPoolNode *AddBefore(thrPoolNode *pos, gxThread_t *t);
  void ClearPool(); // Empty the pool without checking the thread status
  void MakeEmpty(); // Called only if all the nodes have been removed 
  gxThread_t *RemoveThread(gxThread_t *t);
  gxThread_t *RemoveNode(thrPoolNode *n);
  gxThread_t *RemoveHead();
  gxThread_t *RemoveTail();

public: // Search functions
  thrPoolNode *FindThread(gxThread_t *t, thrPoolNode *ptr = 0);

private: // Allocation/de-allocation functions
  gxThread_t *FreeNode(thrPoolNode *n);
  thrPoolNode *AllocNode(gxThread_t *t);
  
public: // Node position functions
  int IsEmpty() { return head == 0; }
  int IsEmpty() const { return (const thrPoolNode *)head == 0; }
  thrPoolNode *GetHead() { return head; }
  thrPoolNode *GetTail() { return tail; }
  thrPoolNode *GetHead() const { return head; }
  thrPoolNode *GetTail() const { return tail; }
  void SetHead(thrPoolNode *h) { head = h; }
  void SetHead(const thrPoolNode *h) { head = (thrPoolNode *)h; }
  void SetTail(thrPoolNode *t) { tail = t; }
  void SetTail(const thrPoolNode *t) { tail = (thrPoolNode *)t; }
  
private:
  thrPoolNode *head; // First node in the list
  thrPoolNode *tail; // Last node in the list
};

#endif // __GX_THREAD_POOL_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
