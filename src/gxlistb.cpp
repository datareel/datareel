// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxlistb.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 04/05/1996
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

Doubly linked list base class. The gxListNodeB and gxListB classes
can be used as base classes to create a family of link-list class
specific to a particular data type. These classes can also be used
as non-base classes provided that each data type is cast to a void
pointer.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "gxlistb.h"

gxListNodeB::~gxListNodeB()
{
  // Virtual destructor cannot be in-lined
  // PC-lint 09/08/2005: Ingnore PC lint warnings about next, prev,
  // and data not being destroyed. Memory is freed in the derived class.
}

gxListB::~gxListB()
{
  // PC-lint 09/08/2005: Function may throw exception in destructor
  ClearList();
}

gxListNodeB *gxListB::Add(void *v)
// General purpose add function the inserts
// a new list node to the back of the list.
// Returns a pointer to the newly created
// node or a null value if heap space could
// not be allocated for the node.
{
  gxListNodeB *node = AllocNode(v);
  if(!node) return 0; // Could not allocate memory for the node
  InsertAtTail(node);
  return node;
}

gxListNodeB *gxListB::AddToFront(void *v)
// Add a list node to the front of the list.
// Returns a pointer to the newly created
// node or a null value if heap space could
// not be allocated for the node.
{
  gxListNodeB *node = AllocNode(v);
  if(!node) return 0; // Could not allocate memory for the node
  InsertAtHead(node);
  return node;
}

gxListNodeB *gxListB::AddToBack(void *v)
// Add a list node to the back of the list.
// Returns a pointer to the newly created
// node or a null value if heap space could
// not be allocated for the node.
{
  gxListNodeB *node = AllocNode(v);
  if(!node) return 0; // Could not allocate memory for the node
  InsertAtTail(node);
  return node;
}

gxListNodeB *gxListB::AddAfter(gxListNodeB *pos, void *v)
// Add a list node after the specified node.
// Returns a pointer to the newly created
// node or a null value if heap space could
// not be allocated for the node.
{
  gxListNodeB *node = AllocNode(v);
  if(!node) return 0; // Could not allocate memory for the node
  InsertAfter(pos, node);
  return node;
}

gxListNodeB *gxListB::AddBefore(gxListNodeB *pos, void *v)
// Add a list node before the specified node.
// Returns a pointer to the newly created
// node or a null value if heap space could
// not be allocated for the node.
{
  gxListNodeB *node = AllocNode(v);
  if(!node) return 0; // Could not allocate memory for the node
  InsertBefore(pos, node);
  return node;
}

gxListNodeB *gxListB::AllocNode(void *v)
// Allocate a new list node. NOTE: This function assumes that
// memory has already been allocated for the list node pointer "v"
{
  gxListNodeB *node = new gxListNodeB;
  if(node) node->data = v;
  return node;
}

void *gxListB::FreeNode(gxListNodeB *n)
// Free the memory location of the node without releasing the nodes
// data pointer. NOTE: This function assumes that the node has already
// been detached from the list. Returns a pointer to the list node
// data pointer.
{
  if(!n) return 0; // Prevents program crashes
  void *data = n->data;

  // NOTE: This delete call will not release the memory allocated for  
  // the node data pointer back to the heap each time a node is deleted.
  // The deletion of the data pointer is left to the application to allow
  // the application to properly destroyed data.
  delete n;
  
  return data; // Return a pointer to this nodes data
}

void *gxListB::RemoveHead()
// Remove the first node in the list. Returns
// a pointer to the list node data pointer for 
// deletion by the application.
{
  if(IsEmpty()) return 0;
  gxListNodeB *n = head;
  DetachNode(n);
  return FreeNode(n);
}

void *gxListB::Remove(gxListNodeB *n)
// Remove the specified node from the list. Returns
// a pointer to the list node data pointer for 
// deletion by the application.
{
  if(IsEmpty()) return 0;
  DetachNode(n);
  return FreeNode(n);
}

void *gxListB::RemoveTail()
// Remove the last node in the list. Returns
// a pointer to the list node data pointer for 
// deletion by the application.
{
  if(IsEmpty()) return 0;
  gxListNodeB *n = tail;
  DetachNode(n);
  return FreeNode(n);
}

void gxListB::ClearList()
// Clear the list
{
  while(!IsEmpty()) RemoveTail();
  head = tail = 0;
}

void gxListB::DestroyList()
// Clear the list and destroy the node data.
{
  while(!IsEmpty()) {
    void *v = RemoveTail();

    // 05/29/2002: Casting to char * to avoid undefined delete
    // warnings under Linux g++
    if(v) delete (char *)v;
  }
  head = tail = 0;
}

void gxListB::MoveToFront(gxListNodeB *n)
// Move the specified node to the front of the list.
{
  if(n == head) return; // This node is the head of the list
  DetachNode(n);
  InsertAtHead(n);
}

void gxListB::MoveToBack(gxListNodeB *n)
// Move the specified node to the back of the list.
{
  if(n == tail) return; // This node is the tail of the list
  DetachNode(n);
  InsertAtTail(n);
}

void gxListB::MakeEmpty()
// Reset the "head" and "tail" pointers.
{
  head = tail = 0;
}

void gxListB::DetachNode(gxListNodeB *n)
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

void gxListB::MoveBefore(gxListNodeB *pos, gxListNodeB *n)
// Move an existing node before the node residing at
// the specified location.
{
  DetachNode(n);
  InsertBefore(pos, n);
}

void gxListB::MoveAfter(gxListNodeB *pos, gxListNodeB *n)
// Move an existing node after the node residing at
// the specified location.
{
  DetachNode(n);
  InsertAfter(pos, n);
}

void gxListB::InsertBefore(gxListNodeB *pos, gxListNodeB *n)
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
    // PC-lint 05/25/2005: Possible null pointer warning
    if(!head) {
      head = n;
      head->prev = n;
    }
    else {
      head->prev = n;
      head = n;
    }
  }
  else { // Inserting node somewhere in the middle of the list
    n->prev = pos->prev;
    n->next = pos;
    pos->prev->next = n;
    pos->prev = n;
  }
}

void gxListB::InsertAfter(gxListNodeB *pos, gxListNodeB *n)
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
    // PC-lint 05/25/2005: Possible null pointer warning
    if(!tail) {
      tail = n;
      tail->next = n;
    }
    else {
      tail->next = n;
      tail = n;
    }
  }
  else { // Inserting node somewhere in the middle of the list
    n->next = pos->next;
    n->prev = pos;
    pos->next->prev = n;
    pos->next = n;
  }
}

void gxListB::InsertAtHead(gxListNodeB *n)
// Insert a new or detached node at the head of the list.
{
  InsertBefore(head, n);
}

void gxListB::InsertAtTail(gxListNodeB *n)
// Insert a new or detached node at the tail of the list.
{
  InsertAfter(tail, n);
}

gxListNodeB *gxListB::Push(void *v)
// Stack operation used to push a list node into the list.
// Returns a pointer to the node or a null value if
// memory for the node could not be allocated. NOTE:
// This function assumes that memory for the list node
// pointer "v" has already been allocated.
{
  gxListNodeB *node = AllocNode(v);
  if(!node) return 0; // Could not allocate memory for the node
  InsertAtHead(node);
  return node;  
}

void *gxListB::Pop()
// Stack operation used to pop a list node from the list.
// Returns a pointer to the list node or a null value if
// the list is empty.
{
  if(IsEmpty()) return 0;
  gxListNodeB *node = head;
  DetachNode(node);
  return FreeNode(node);
}

gxListNodeB *gxListB::Insert(void *v)
// Queue operation used to insert a list node into the list.
// Returns a pointer to the node or a null value if
// memory for the node could not be allocated. NOTE:
// This function assumes that memory for the list node
// pointer "v" has already been allocated.
{
  gxListNodeB *node = AllocNode(v);
  if(!node) return 0; // Could not allocate memory for the node
  InsertAtTail(node);
  return node;  
}

void *gxListB::Extract()
// Queue operation used to extract a list node from the list.
// Returns a pointer to the list node or a null value if the
// list is empty.
{
  if(IsEmpty()) return 0;
  gxListNodeB *node = head;
  DetachNode(node);
  return FreeNode(node);
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
