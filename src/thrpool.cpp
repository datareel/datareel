// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: thrpool.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 03/31/2000
// Date Last Modified: 06/17/2016
// Copyright (c) 2001-2016 DataReel Software Development
// ----------------------------------------------------------- // 
// ------------- Program Description and Details ------------- // 
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
#include "gxdlcode.h"

#include "thrpool.h"
#include "gthreadt.h"

thrPool::~thrPool()
{
  // PC-lint 09/08/2005: Function may throw exception in destructor
  ClearPool();
}

thrPoolNode *thrPool::AddThread(gxThread_t *thread)
// Add a thread to the thread pool. Returns a pointer to the
// newly allocated node or a null value if the node could
// be created.
{
  if(!thread) return 0; // The thread pointer is not initialized
  
  thrPoolNode *ptr = 0;
  thrPoolNode *tnode = 0;
  int inserted_node = 0;
  switch(thread->GetThreadPriority()) {
    case gxTHREAD_PRIORITY_LOW :
      // Add all low priorities to the back of the thread pool
      tnode = AddToBack(thread); 
      break;
    case gxTHREAD_PRIORITY_NORMAL :
      if(IsEmpty()) { // No threads are currently in the pool  
	tnode = AddToBack(thread);
	break;
      }
      else {
	ptr = tail;
	while(ptr) { // Start searching from the end of the pool
	  // Insert the node after the last normal priority node found or
	  // after the first high priority.
	  if(ptr->GetThreadPtr()->GetThreadPriority() != 
	     gxTHREAD_PRIORITY_LOW) {
	    tnode = AddAfter(ptr, thread);
	    inserted_node = 1;
	    break;
	  }
	  ptr = ptr->prev;
	} 
      }
      if(!inserted_node) { // The pool contains all low priorities
	tnode = AddToFront(thread);
      }
      break;
    case gxTHREAD_PRIORITY_HIGH :
      if(IsEmpty()) { // No threads are currently in the pool  
	tnode = AddToBack(thread);
	break;
      }
      else {
	ptr = head;
	while(ptr) { // Start searching from the end of the pool
	  if(ptr->GetThreadPtr()->GetThreadPriority() ==
	     gxTHREAD_PRIORITY_HIGH) {
	    // Insert the node after the last high priority
	    tnode = AddAfter(ptr, thread);
	    inserted_node = 1;
	    break;
	  }
	  ptr = ptr->next;
	} 
      }
      if(!inserted_node) { // This is the first high priority node in the pool
	tnode = AddToFront(thread);
      }
      break;
    default :
      // Invalid thread priority
      tnode = AddToBack(thread); 
      break;
  }

  // PC-lint 09/15/2005: tnode variable may not be initialized 
  if(!tnode) tnode = AddToBack(thread); 

  return tnode;
}

thrPoolNode *thrPool::AddToFront(gxThread_t *t)
// Add a thread to the front of the pool list.
// Returns a pointer to the newly created
// node or a null value if heap space could
// not be allocated for the node.
{
  thrPoolNode *n = AllocNode(t);
  if(!n) return 0; // Could not allocate memory for the node
  InsertAtHead(n);
  return n;
}

thrPoolNode *thrPool::AddToBack(gxThread_t *t)
// Add a thread to the back of the pool list.
// Returns a pointer to the newly created
// node or a null value if heap space could
// not be allocated for the node.
{
  thrPoolNode *n = AllocNode(t);
  if(!n) return 0; // Could not allocate memory for the node
  InsertAtTail(n);
  return n;
}

thrPoolNode *thrPool::AddAfter(thrPoolNode *pos, gxThread_t *t)
// Add a thread after the specified pool node.
// Returns a pointer to the newly created
// node or a null value if heap space could
// not be allocated for the node.
{
  thrPoolNode *n = AllocNode(t);
  if(!n) return 0; // Could not allocate memory for the node
  InsertAfter(pos, n);
  return n;
}

thrPoolNode *thrPool::AddBefore(thrPoolNode *pos, gxThread_t *t)
// Add a thread before the specified pool node.
// Returns a pointer to the newly created
// node or a null value if heap space could
// not be allocated for the node.
{
  thrPoolNode *n = AllocNode(t);
  if(!n) return 0; // Could not allocate memory for the node
  InsertBefore(pos, n);
  return n;
}

thrPoolNode *thrPool::AllocNode(gxThread_t *t)
// Allocate a new pool node. NOTE: This function assumes that
// memory has already been allocated to the thread pointer "t"
{
  thrPoolNode *n = new thrPoolNode(t);
  return n;
}

gxThread_t *thrPool::FreeNode(thrPoolNode *n)
// Free the memory location of the node and its data. NOTE: This
// function assumes that the node has already been detached from
// the pool. Returns a pointer to this node's thread.
{
  if(!n) return 0; // Prevents program crashes
  gxThread_t *thread = n->thread;
  delete n;
  
  // NOTE: This delete call will not release the memory allocated for  
  // the thread pointer back to the heap each time a node is deleted.
  // The deletion of the thread pointer is left to the application so
  // that the thread can be properly destroyed if it has not finished
  // executing or is in a suspended or wait state.
  return thread;
}

gxThread_t *thrPool::RemoveNode(thrPoolNode *n)
// Remove the specified node from the pool. Returns
// a pointer to the node's thread for deletion by
// the application.
{
  DetachNode(n);
  return FreeNode(n);
}

gxThread_t *thrPool::RemoveHead()
// Remove the first thread in the pool. Returns
// a pointer to the node's thread for deletion by
// the application.
{
  if(IsEmpty()) return 0;
  thrPoolNode *n = head;
  DetachNode(n);
  return FreeNode(n);
}

gxThread_t *thrPool::RemoveTail()
// Remove the last thread in the pool. Returns
// a pointer to the node's thread for deletion
// by the application.
{
  if(IsEmpty()) return 0;
  thrPoolNode *n = tail;
  DetachNode(n);
  return FreeNode(n);
}

void thrPool::ClearPool()
// Clear the pool without checking each thread's status.
{
  while(!IsEmpty()) {
    gxThread_t *t = RemoveTail();
    if(t) delete t; // Free any memory the thread is holding
  }
  head = tail = 0;
}

void thrPool::MoveToFront(thrPoolNode *n)
// Move the specified pool node to the front of the pool.
{
  if(n == head) return; // This node is the head of the pool 
  DetachNode(n);
  InsertAtHead(n);
}

void thrPool::MoveToBack(thrPoolNode *n)
// Move the specified pool node to the back of the pool.
{
  if(n == tail) return; // This node is the tail of the pool
  DetachNode(n);
  InsertAtTail(n);
}

void thrPool::MakeEmpty()
// Reset the "head" and "tail" pointers.
{
  head = tail = 0;
}

void thrPool::DetachNode(thrPoolNode *n)
// Detach the node from its current position.
{
  if((IsEmpty()) || (!n)) return;

  if(head == tail) { // There is only one node in the pool
    MakeEmpty();
  }
  else if(n == head) { // This is the head node
    if(n->next) {
      n->next->prev = 0;
      head = n->next;
    }
    else
      MakeEmpty(); // This is the last node starting from the head
  }
  else if(n == tail) { // This is the tail node
    if(n->prev) {
      n->prev->next = 0;
      tail = n->prev;  
    }
    else
      MakeEmpty(); // This is the last node starting at the tail
  }
  else if((n->next == 0) && (n->prev == 0)) {
    // This node is not linked anywhere in the pool
    return;
  }
  else { // We are somewhere in the middle of the pool
    n->prev->next = n->next;
    n->next->prev = n->prev;
  }
}

void thrPool::MoveBefore(thrPoolNode *pos, thrPoolNode *n)
// Move an existing node before the node residing at
// the specified position.
{
  DetachNode(n);
  InsertBefore(pos, n);
}

void thrPool::MoveAfter(thrPoolNode *pos, thrPoolNode *n)
// Move an existing node after the node residing at
// the specified position.
{
  DetachNode(n);
  InsertAfter(pos, n);
}

void thrPool::InsertBefore(thrPoolNode *pos, thrPoolNode *n)
// Insert a new or detached node before the node residing
// at the specified position.
{
  // The pool is empty so this will be the first node
  if(IsEmpty()) { 
    tail = head = n; // First node in the pool
    return;
  }
  
  if(pos == head) { // Inserting node at the head of the pool
    n->prev = 0; // This node if the new head
    n->next = head;
    // PC-lint 09/15/2005: Possible null pointer warning
    if(!head) {
      head = n;
      head->prev = n;
    }
    else {
      head->prev = n;
      head = n;
    }
  }
  else { // Inserting node somewhere in the middle of the pool
    n->prev = pos->prev;
    n->next = pos;
    pos->prev->next = n;
    pos->prev = n;
  }
}

void thrPool::InsertAfter(thrPoolNode *pos, thrPoolNode *n)
// Insert a new or detached node after the node residing
// at the specified position.
{
  // The pool is empty so this will be the first node
  if(IsEmpty()) {
    tail = head = n; // First node in the pool
    return;
  }

  if(pos == tail) { // Inserting node at the tail of the pool
    n->next = 0; // This node is the new tail;
    n->prev = tail;

    // PC-lint 09/15/2005: Possible null pointer warning
    if(!tail) {
      tail = n;
      tail->next = n;
    }
    else {
      tail->next = n;
      tail = n;
    }    
  }
  else { // Inserting node somewhere in the middle of the pool
    n->next = pos->next;
    n->prev = pos;
    pos->next->prev = n;
    pos->next = n;
  }
}

void thrPool::InsertAtHead(thrPoolNode *n)
// Insert a new or detached node at the head of the pool.
{
  InsertBefore(head, n);
}

void thrPool::InsertAtTail(thrPoolNode *n)
// Insert a new or detached node at the tail of the pool.
{
  InsertAfter(tail, n);
}

thrPoolNode *thrPool::FindThread(gxThread_t *t, thrPoolNode *ptr)
// Find the specified thread in the pool starting at a specified
// position or from the beginning of the pool if no position
// is specified. Returns a pointer to the pool node if the
// thread was found of a null value if the tread was not
// found.
{
  if(ptr == 0) ptr = GetHead();
  while(ptr) {
    if(ptr->thread == t) return ptr; // Match found
    ptr = ptr->GetNext();
  }
  return 0; // No match was found
}

gxThread_t *thrPool::RemoveThread(gxThread_t *t)
// Remove the specified thread from the pool. Returns a pointer to thread
// if the node was removed or a null value if the node was not found in
// the pool. NOTE: This remove call will not release the memory allocated
// for the thread pointer back to the heap when a node is removed. The
// deletion of the thread pointer is left to the application so that the
// thread can be properly destroyed if it has not finished executing or is
// in a suspended or wait state.
{
  thrPoolNode *ptr = FindThread(t);
  if(!ptr) return 0; // The node was not found in the pool
  return RemoveNode(ptr);
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
