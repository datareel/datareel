// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: btnode.cpp
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

The BtreeNode class is used by the gxBtree class to insert, delete, 
and search for database keys in a node. Database keys are derived 
from the DatabaseKeyB class and are kept in sorted order within 
each B-tree node.

Changes:
==============================================================
09/10/2001: Added binary search code to the BtreeNode::FindKeyLocation() 
function. To use the binary search method instead of a linear search 
define the __USE_BINARY_SEARCH__ preprocessor directive when compiling.

11/05/2001: Added single comparison method to BtreeNode::FindKeyLocation()
function. To use a single compare instead of the greater then and equal to 
operators define the __USE_SINGLE_COMPARE__ preprocessor directive when 
compiling. NOTE: Using the single comparison method will also affect the 
classes derived from the DatabaseKeyB. If a dual comparison is used the 
derived class must override the pure virtual greater than and equal to 
operators declared in the DatabaseKeyB base class. If the a single 
comparison is used the derived class must override the pure virtual 
DatabaseKeyB::CompareKey() function with return values of: -1 less than, 
0 equal to, 1 greater than.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <string.h>
#include "btnode.h"

#ifdef __BCC32__
#pragma warn -8080
#endif

BtreeNode::BtreeNode(BtreeSize_t dbkey_size, BtreeNodeOrder_t order)
{
  // Each node must contain 5 or more keys.
  // Ensure comparison is between same data types
  // the correct type may be arbitrarily selected.
  if(order < (BtreeNodeOrder_t)5) order = (BtreeNodeOrder_t)5;
  
  key_size = dbkey_size;  
  node_order = order-1;

  // Allocate memory to hold the keys
  key_entries = new char[(key_size * node_order)];

  key_count = (BtreeKeyCount_t)0;
  left_child = (FAU_t)0;
  node_address = (FAU_t)-1;
}

BtreeNode::~BtreeNode()
{
  if(key_entries) delete[] key_entries;
}

FAU BtreeNode::GetBranch(BtreeKeyLocation_t key_location)
// Returns the node's left or right child based on the specified
// key location.
{
  // The -1th branch represents the nodes left child. 
  if(key_location < (BtreeKeyLocation_t)0) return left_child;


  // The right branch of this key will be returned starting
  // from the 0th branch on.
  char *ptr = key_entries + (key_size * key_location);
  FAU right_child;
  memmove(&right_child, ptr, sizeof(FAU));
  return right_child;
}

int BtreeNode::FindKeyLocation(const DatabaseKeyB &key,
			       DatabaseKeyB &compare_key,
			       BtreeKeyLocation_t &key_location)
// Find the key location. Returns 0 if an entry for the key
// already exists, 1 if the specified key is greater than
// all the entries in this node, or -1 if the specified key
// is less than all the entries in this node. The key class
// derived from the abstract DatabaseKeyB base class defines
// the key methods used by this function to compare the
// keys. The key location or the branch to keep searching
// down the tree is passed back in the "key_location"
// variable.
{
#if defined (__USE_BINARY_SEARCH__)
  // Binary search code.
  // Same as the old FindKeyLocation except it uses a
  // binary search instead of a linear search.
  BtreeKeyCount_t nLeft = (BtreeKeyCount_t)0;
  BtreeKeyCount_t nRight = key_count - (BtreeKeyCount_t)1;
  char *ptr;

  while(nRight >= nLeft) {
    key_location = ((nLeft + nRight) / (BtreeKeyCount_t)2);
    ptr = key_entries + (key_size * key_location);
    memmove(&compare_key.right_child, ptr, sizeof(FAU));
    memmove(compare_key.db_key, ptr+sizeof(FAU), compare_key.KeySize());
#ifndef __USE_SINGLE_COMPARE__ // Default to dual compare
    if(key == compare_key) 
      return 0;
    if(key > compare_key)
      nLeft = key_location + (BtreeKeyLocation_t)1;
    else
      nRight = key_location - (BtreeKeyLocation_t)1;
#else // Use single compare method
    int rv = key.CompareKey(compare_key);
    if(rv == 0) 
      return 0;
    if(rv > 0) 
      nLeft = key_location + (BtreeKeyLocation_t)1;
    else
      nRight = key_location - (BtreeKeyLocation_t)1;
#endif // __USE_SINGLE_COMPARE__
  }
  
  key_location = nRight;

  if(nRight < (BtreeKeyLocation_t)0) {
    return -1;
  }
  else {
    return 1; 
  }
#endif // __USE_BINARY_SEARCH__

  key_location = (BtreeKeyLocation_t)key_count-1;

  while(key_location >= (BtreeKeyLocation_t)0) {
    char *ptr = key_entries + (key_size * key_location);
    memmove(&compare_key.right_child, ptr, sizeof(FAU));
    memmove(compare_key.db_key, ptr+sizeof(FAU), compare_key.KeySize());
#ifndef __USE_SINGLE_COMPARE__ // Use dual compare by default
    if(key > compare_key) return 1;
    if(key == compare_key) return 0;
#else
    int rv = key.CompareKey(compare_key);
    if(rv > 0) return 1;
    if(rv == 0) return 0;
#endif // __USE_SINGLE_COMPARE__
    key_location--;
  }
  return -1;
}

void BtreeNode::LoadKey(DatabaseKeyB &key, BtreeKeyLocation_t key_location)
// Copies the key at the specified location into the "key" variable.
{
  char *ptr = key_entries + (key_size * key_location);
  memmove(&key.right_child, (void *)ptr, sizeof(FAU));
  memmove(key.db_key, ptr+sizeof(FAU), key.KeySize());
}

void BtreeNode::SplitNode(BtreeNode &node, BtreeKeyLocation_t split_location)
// Splits this node by moving the keys, starting at the "split_location",
// into specified node.
{
  node.key_count = key_count - (BtreeKeyCount_t)split_location; 
  char *ptr = key_entries + (split_location * key_size);
  memmove(node.key_entries, ptr, (node.key_count * key_size));
  key_count = split_location;
}

void BtreeNode::InsertDatabaseKey(DatabaseKeyB &key,
				  BtreeKeyLocation_t key_location)
// Insert the database key into the node at the specified location.
{
  memmove((key_entries + ((key_location+1) * key_size)),
	  (key_entries + (key_location * key_size)),
  	  (key_size * (key_count - (BtreeKeyCount_t)key_location)));
	  char *ptr = key_entries + (key_size * key_location);
  memmove(ptr, &key.right_child, sizeof(FAU));
  memmove(ptr+sizeof(FAU), key.db_key, key.KeySize());
  key_count++;
}

void BtreeNode::OverWriteDatabaseKey(DatabaseKeyB &key,
				     BtreeKeyLocation_t key_location)
// Overwrite the key at the specified location.
{
  char *ptr = key_entries + (key_size * key_location);
  memmove(ptr, &key.right_child, sizeof(FAU));
  memmove(ptr+sizeof(FAU), key.db_key, key.KeySize());
}

void BtreeNode::MergeNode(BtreeNode &node)
// Merge this node with the specified node.
{
  memmove((key_entries + (key_count * key_size)), node.key_entries,
	  (node.key_count * key_size));
  key_count += node.key_count;
}

void BtreeNode::DeleteDatabaseKey(BtreeKeyLocation_t key_location)
// Delete the database key at specified location.
{
  key_count--;
  memmove((key_entries + (key_location * key_size)),
	  (key_entries + ((key_location+1) * key_size)),
	  ((key_count - key_location) * key_size));
}

size_t BtreeNode::SizeOfBtreeNode()
// Calculates the total size of this Btree node.
{
  size_t node_size = (key_size * node_order);
  node_size += sizeof(key_count);
  node_size += sizeof(left_child);
  return node_size;
}

BtreeNode::BtreeNode()
// Initialize data members when the default 
// constructor is called.
{
  key_size = (BtreeSize_t)0;
  node_order = (BtreeNodeOrder_t)4;
  key_entries = 0;
  key_count = (BtreeKeyCount_t)0;
  left_child = (FAU_t)0;
  node_address = (FAU_t)-1;
}

BtreeNode& BtreeNode::operator=(const BtreeNode &ob)
{
  if(this != &ob) { // Prevent self assignment 
    Copy(ob);
  }
  return *this;
}

void BtreeNode::Copy(const BtreeNode &ob)
// Function used to copy a B-tree node. This version of the 
// copy function ensures that each copy is safe by making
// a unique copy of the object being copied.
{
  BtreeSize_t org_size = (key_size * node_order);
  key_size = ob.key_size;
  node_order = ob.node_order;
  key_count = ob.key_count;
  left_child = ob.left_child;
  node_address = ob.node_address;
  
  // Calculate the new size are copying size and order variables.
  BtreeSize_t new_size = (key_size * node_order);

  // Free any memory this object is holding
  if((key_entries) && (ob.key_entries)) {
    if(org_size >= new_size) { // We have enough memory to hold the keys
      // Make a unique copy and return
      memmove(key_entries, ob.key_entries, (key_size * node_order));
      return;
    }
  }

  // Need to realloc enough memory to hold the keys
  if(key_entries) {
    delete[] key_entries;
    key_entries = 0;
  }
  
  if(ob.key_entries) {
    key_entries = new char[new_size];
    if(!key_entries) return; // Allocation error
    // Make a unique copy and return
    memmove(key_entries, ob.key_entries, new_size);
  }
}

void BtreeNode::Release()
// A Release() call ensures that this object's destructor 
// will not delete the entry key array, which is currently 
// bound to one or more objects. NOTE: This function
// should be called only if other entities are bound to 
// this object's entry key array.  
{
  // PC-lint 04/20/2004: Creation of memory leak warning (ignore)
  key_entries = 0;
}

#ifdef __BCC32__
#pragma warn .8080
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
