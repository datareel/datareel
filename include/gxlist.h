// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxlist.h
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

Generic doubly linked list class.

Changes:
==============================================================
03/24/2002: Added the Release() synchronization function. The 
release function is used to signal to the class destructor not  
to delete the list when an object is deleted or no longer 
visible.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_LIST_HPP__
#define __GX_LIST_HPP__

#include "gxdlcode.h"

// Linked list node class
template<class TYPE>
class gxListNode
{
public:
  gxListNode() { next = prev = 0; }
  gxListNode(TYPE &X) : data(X) { next = prev = 0; }
  ~gxListNode() { }

public: 
  TYPE data; // Node data
  gxListNode<TYPE> *next; // Pointer to the next node in the list
  gxListNode<TYPE> *prev; // Pointer to the previous node in the list
};

// Linked list class
template<class TYPE>
class gxList
{
public:
  gxList() { head = tail = 0; }
  ~gxList() { ClearList(); }

private: // Disallow copying and assignment
  gxList(const gxList<TYPE> &ob) { }
  void operator=(const gxList<TYPE> &ob) { }
  
public: // Pointer manipulation operations
  gxListNode<TYPE> *GetHead() { return head; }
  gxListNode<TYPE> *GetHead() const { return head; }
  gxListNode<TYPE> *GetTail() { return tail; }
  gxListNode<TYPE> *GetTail() const { return tail; }
  void DetachNode(gxListNode<TYPE> *n);
  void MoveToFront(gxListNode<TYPE> *n);
  void MoveToBack(gxListNode<TYPE> *n);
  void MoveBefore(gxListNode<TYPE> *pos, gxListNode<TYPE> *n);
  void MoveAfter(gxListNode<TYPE> *pos, gxListNode<TYPE> *n);
  void InsertBefore(gxListNode<TYPE> *pos, gxListNode<TYPE> *n);
  void InsertAfter(gxListNode<TYPE> *pos, gxListNode<TYPE> *n);
  void InsertAtHead(gxListNode<TYPE> *n) { InsertBefore(head, n); }
  void InsertAtTail(gxListNode<TYPE> *n) { InsertAfter(tail, n); }

public: // Add and remove functions
  gxListNode<TYPE> *Add(TYPE &X);
  gxListNode<TYPE> *AddToFront(TYPE &X);
  gxListNode<TYPE> *AddToBack(TYPE &X);
  gxListNode<TYPE> *AddAfter(gxListNode<TYPE> *pos, TYPE &X);
  gxListNode<TYPE> *AddBefore(gxListNode<TYPE> *pos, TYPE &X);
  void ClearList();
  void Clear() { ClearList(); }
  void DestroyList();
  void MakeEmpty(); // Called only if all the nodes have been removed 
  int RemoveHead(TYPE &X);
  int RemoveTail(TYPE &X);
  int Remove(gxListNode<TYPE> *n, TYPE &X);
  int Remove(gxListNode<TYPE> *n);
  
public: // Stack functions
  gxListNode<TYPE> *Push(TYPE &X);
  int Pop(TYPE &X);

public: // Queue functions
  gxListNode<TYPE> *Insert(TYPE &X);
  int Extract(TYPE &X);

private: // Allocation/deallocation functions
  int FreeNode(gxListNode<TYPE> *n, TYPE &X);
  gxListNode<TYPE> *AllocNode(TYPE &X);
  
public:
  int IsEmpty() { return head == 0; }
  int IsEmpty() const { return (const gxListNode<TYPE> *)head == 0; }
  
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

private:
  gxListNode<TYPE> *head; // First node in the list
  gxListNode<TYPE> *tail; // Last node in the list
};

template<class TYPE>
gxListNode<TYPE> *gxList<TYPE>::Add(TYPE &X)
// General purpose add function the inserts
// a new list node to the back of the list.
// Returns a pointer to the newly created
// node or a null value if heap space could
// not be allocated for the node.
{
  gxListNode<TYPE> *node = AllocNode(X);
  if(!node) return 0; // Could not allocate memory for the node
  InsertAtTail(node);
  return node;
}

template<class TYPE>
gxListNode<TYPE> *gxList<TYPE>::AddToFront(TYPE &X)
// Add a list node to the front of the list.
// Returns a pointer to the newly created
// node or a null value if heap space could
// not be allocated for the node.
{
  gxListNode<TYPE> *node = AllocNode(X);
  if(!node) return 0; // Could not allocate memory for the node
  InsertAtHead(node);
  return node;
}

template<class TYPE>
gxListNode<TYPE> *gxList<TYPE>::AddToBack(TYPE &X)
// Add a list node to the back of the list.
// Returns a pointer to the newly created
// node or a null value if heap space could
// not be allocated for the node.
{
  gxListNode<TYPE> *node = AllocNode(X);
  if(!node) return 0; // Could not allocate memory for the node
  InsertAtTail(node);
  return node;
}

template<class TYPE>
gxListNode<TYPE> *gxList<TYPE>::AddAfter(gxListNode<TYPE> *pos, TYPE &X)
// Add a list node after the specified node.
// Returns a pointer to the newly created
// node or a null value if heap space could
// not be allocated for the node.
{
  gxListNode<TYPE> *node = AllocNode(X);
  if(!node) return 0; // Could not allocate memory for the node
  InsertAfter(pos, node);
  return node;
}

template<class TYPE>
gxListNode<TYPE> *gxList<TYPE>::AddBefore(gxListNode<TYPE> *pos, TYPE &X)
// Add a list node before the specified node.
// Returns a pointer to the newly created
// node or a null value if heap space could
// not be allocated for the node.
{
  gxListNode<TYPE> *node = AllocNode(X);
  if(!node) return 0; // Could not allocate memory for the node
  InsertBefore(pos, node);
  return node;
}

template<class TYPE>
gxListNode<TYPE> *gxList<TYPE>::AllocNode(TYPE &X)
// Allocate a new list node.
{
  gxListNode<TYPE> *node = new gxListNode<TYPE>(X);
  return node;
}

template<class TYPE>
int gxList<TYPE>::FreeNode(gxListNode<TYPE> *n, TYPE &X)
// Free the memory location of the node without releasing the nodes
// data pointer. NOTE: This function assumes that the node has already
// been detached from the list. Returns a true if the node was deleted.
{
  if(!n) return 0; // Prevents program crashes
  X = n->data;

  // NOTE: This delete call will not release the memory allocated for  
  // the node data back to the heap each time a node is deleted.
  // The deletion of the data is left to the application to allow
  // the application to properly destroy its data.
  delete n;
  
  return 1;
}

template<class TYPE>
int gxList<TYPE>::RemoveHead(TYPE &X)
// Remove the first node in the list. Returns
// true if the node was removed.
{
  if(IsEmpty()) return 0;
  gxListNode<TYPE> *n = head;
  DetachNode(n);
  return FreeNode(n, X);
}

template<class TYPE>
int gxList<TYPE>::Remove(gxListNode<TYPE> *n, TYPE &X)
// Remove the specified node from the list. Returns
// true if the node was removed.
{
  if(IsEmpty()) return 0;
  DetachNode(n);
  return FreeNode(n, X);
}

template<class TYPE>
int gxList<TYPE>::Remove(gxListNode<TYPE> *n)
// Remove the specified node from the list. Returns
// true if the node was removed.
{
  if(IsEmpty()) return 0;
  TYPE X;
  DetachNode(n);
  return FreeNode(n, X);
}

template<class TYPE>
int gxList<TYPE>::RemoveTail(TYPE &X)
// Remove the last node in the list. Returns
// true if the node was removed.
{
  if(IsEmpty()) return 0;
  gxListNode<TYPE> *n = tail;
  DetachNode(n);
  return FreeNode(n, X);
}

template<class TYPE>
void gxList<TYPE>::ClearList()
// Clear the list
{
  TYPE X;
  while(!IsEmpty()) RemoveTail(X);
  head = tail = 0;
}

template<class TYPE>
void gxList<TYPE>::DestroyList()
// Clear the list and destroy the node data.
{
  TYPE X;
  while(!IsEmpty()) {
    if(RemoveTail(X)) delete X;
  }
  head = tail = 0;
}

template<class TYPE>
void gxList<TYPE>::MoveToFront(gxListNode<TYPE> *n)
// Move the specified node to the front of the list.
{
  if(n == head) return; // This node is the head of the list
  DetachNode(n);
  InsertAtHead(n);
}

template<class TYPE>
void gxList<TYPE>::MoveToBack(gxListNode<TYPE> *n)
// Move the specified node to the back of the list.
{
  if(n == tail) return; // This node is the tail of the list
  DetachNode(n);
  InsertAtTail(n);
}

template<class TYPE>
void gxList<TYPE>::MakeEmpty()
// Reset the "head" and "tail" pointers.
{
  head = tail = 0;
}

template<class TYPE>
void gxList<TYPE>::DetachNode(gxListNode<TYPE> *n)
// Detach the node from its current location.
{
  if((IsEmpty()) || (!n)) return;

  if(head == tail) { // There is only one node in the list
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
    // This node is not linked anywhere in the list
    return;
  }
  else { // We are somewhere in the middle of the list
    n->prev->next = n->next;
    n->next->prev = n->prev;
  }
}

template<class TYPE>
void gxList<TYPE>::MoveBefore(gxListNode<TYPE> *pos, gxListNode<TYPE> *n)
// Move an existing node before the node residing at
// the specified location.
{
  DetachNode(n);
  InsertBefore(pos, n);
}

template<class TYPE>
void gxList<TYPE>::MoveAfter(gxListNode<TYPE> *pos, gxListNode<TYPE> *n)
// Move an existing node after the node residing at
// the specified location.
{
  DetachNode(n);
  InsertAfter(pos, n);
}

template<class TYPE>
void gxList<TYPE>::InsertBefore(gxListNode<TYPE> *pos, gxListNode<TYPE> *n)
// Insert a new or detached node before the node residing
// at the specified location.
{
  // The list is empty so this will be the first node
  if(IsEmpty()) { 
    tail = head = n; // First node in the list
    return;
  }
  
  if(pos == head) { // Inserting node at the head of the list
    n->prev = 0; // This node if the new head
    n->next = head;
    head->prev = n;
    head = n;
  }
  else { // Inserting node somewhere in the middle of the list
    n->prev = pos->prev;
    n->next = pos;
    pos->prev->next = n;
    pos->prev = n;
  }
}

template<class TYPE>
void gxList<TYPE>::InsertAfter(gxListNode<TYPE> *pos, gxListNode<TYPE> *n)
// Insert a new or detached node after the node residing
// at the specified location.
{
  // The list is empty so this will be the first node
  if(IsEmpty()) {
    tail = head = n; // First node in the list
    return;
  }

  if(pos == tail) { // Inserting node at the tail of the list
    n->next = 0; // This node is the new tail;
    n->prev = tail;
    tail->next = n;
    tail = n;
  }
  else { // Inserting node somewhere in the middle of the list
    n->next = pos->next;
    n->prev = pos;
    pos->next->prev = n;
    pos->next = n;
  }
}

template<class TYPE>
gxListNode<TYPE> *gxList<TYPE>::Push(TYPE &X)
// Stack operation used to push a list node into the list.
// Returns a pointer to the node or a null value if
// memory for the node could not be allocated. 
{
  gxListNode<TYPE> *node = AllocNode(X);
  if(!node) return 0; // Could not allocate memory for the node
  InsertAtHead(node);
  return node;  
}

template<class TYPE>
int gxList<TYPE>::Pop(TYPE &X)
// Stack operation used to pop a list node from the list.
// Returns a true if the node was popped.
{
  if(IsEmpty()) return 0;
  gxListNode<TYPE> *node = head;
  DetachNode(node);
  return FreeNode(node, X);
}

template<class TYPE>
gxListNode<TYPE> *gxList<TYPE>::Insert(TYPE &X)
// Queue operation used to insert a list node into the list.
// Returns a pointer to the node or a null value if
// memory for the node could not be allocated. 
{
  gxListNode<TYPE> *node = AllocNode(X);
  if(!node) return 0; // Could not allocate memory for the node
  InsertAtTail(node);
  return node;  
}

template<class TYPE>
int gxList<TYPE>::Extract(TYPE &X)
// Queue operation used to extract a list node from the list.
// Returns true if the node was extracted.
{
  if(IsEmpty()) return 0;
  gxListNode<TYPE> *node = head;
  DetachNode(node);
  return FreeNode(node, X);
}

#endif // __GX_LIST_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
