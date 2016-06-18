// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: btstack.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 08/22/2000 
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

Stack class used by the Btree class to store node addresses
during non-recursive insertions and deletions.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "btstack.h"

#ifdef __BCC32__
#pragma warn -8080
#endif

BtreeStack::~BtreeStack()
{
  Clear();
}

BtreeStack_t *BtreeStack::AllocNode(FAU_t node_address)
// Allocate a new stack node for the Btree node address.
// Returns null if memory could not be allocated or a pointer
// to the stack node if no errors occur.
{
  BtreeStack_t *stack_node = new BtreeStack_t(node_address);
  return stack_node;
}

FAU_t BtreeStack::FreeNode(BtreeStack_t *n)
// Free the memory location of the stack node.
// Returns a file pointer to the Btree node address.
{
  if(!n) return 0; // Prevents program crashes
  FAU_t node_address = n->node_address;
  delete n;
  return node_address;
}

FAU_t BtreeStack::RemoveHead()
// Remove the first stack node in the list.
// Returns a file pointer to the Btree node address.
{
  if(IsEmpty()) return 0;
  BtreeStack_t *n = head;
  DetachNode(n);
  return FreeNode(n);
}

FAU_t BtreeStack::RemoveTail()
// Remove the last stack node in the list.
// Returns a file pointer to the Btree node address.
{
  if(IsEmpty()) return 0;
  BtreeStack_t *n = tail;
  DetachNode(n);
  return FreeNode(n);
}

void BtreeStack::Clear()
// Clear the stack.
{
  while(!IsEmpty()) RemoveTail();
  head = tail = 0;
}

void BtreeStack::MakeEmpty()
// Reset the "head" and "tail" pointers.
{
  head = tail = 0;
}

void BtreeStack::DetachNode(BtreeStack_t *n)
// Detach the stack node from its current location.
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

void BtreeStack::InsertBefore(BtreeStack_t *pos, BtreeStack_t *n)
// Insert a new or detached stack node before the node residing
// at the specified location.
{
  // The list is empty so this will be the first node
  if(IsEmpty()) { 
    tail = head = n; // First node in the list
    return;
  }
  
  // PC-lint 05/25/2002: Warning about possible use of null pointer
  // can be ignored because this condition is checked for by the
  // IsEmpty() call prior to using the head or tail variables.
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

void BtreeStack::InsertAfter(BtreeStack_t *pos, BtreeStack_t *n)
// Insert a new or detached stack node after the node residing
// at the specified location.
{
  // The list is empty so this will be the first node
  if(IsEmpty()) {
    tail = head = n; // First node in the list
    return;
  }

  // PC-lint 05/25/2002: Warning about possible use of null pointer
  // can be ignored because this condition is checked for by the
  // IsEmpty() call prior to using the head or tail variables.
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

BtreeStack_t *BtreeStack::Push(FAU_t node_address)
// Stack operation used to push a Btree node address into the list.
// Returns a pointer to the stack node or a null value if memory
// for the node could not be allocated.
{
  BtreeStack_t *stack_node = AllocNode(node_address);
  if(!stack_node) return 0; // Could not allocate memory for the node
  InsertBefore(head, stack_node);
  return stack_node;  
}

FAU_t BtreeStack::Pop()
// Stack operation used to pop a Btree node address from the list.
// Returns a file pointer to the Btree node address.
{
  if(IsEmpty()) return 0;
  BtreeStack_t *stack_node = head;
  DetachNode(stack_node);
  return FreeNode(stack_node);
}

BtreeStack_t *BtreeStack::Insert(FAU_t node_address)
// Queue operation used to insert a Btree node address into the list.
// Returns a pointer to the stack node or a null value if memory
// for the node could not be allocated.
{
  BtreeStack_t *stack_node = AllocNode(node_address);
  if(!stack_node) return 0; // Could not allocate memory for the node
  InsertAfter(tail, stack_node);
  return stack_node;  
}

FAU_t BtreeStack::Extract()
// Queue operation used to extract a Btree node address from the list.
// Returns a file pointer to the Btree node address.
{
  if(IsEmpty()) return 0;
  BtreeStack_t *stack_node = head;
  DetachNode(stack_node);
  return FreeNode(stack_node);
}

#ifdef __BCC32__
#pragma warn .8080
#endif
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
