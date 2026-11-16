// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: btnode.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 08/22/2000 
// Date Last Modified: 10/26/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- // 
// ------------- Program Description and Details ------------- // 
// ----------------------------------------------------------- // 
/*
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.
 
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library. If not, see
<https://www.gnu.org/licenses/>.

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
#include <vector>
#include <stdint.h>

namespace datareel {



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
  memmove((void *)&right_child, (const void *)ptr, sizeof(FAU));
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
    memmove((void *)&compare_key.right_child, (const void *)ptr, sizeof(FAU));
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
  memmove((void *)&key.right_child, (const void *)(void *)ptr, sizeof(FAU));
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

// 11/15/2026: B-tree nodes in their file's layout (see btnode.h)
static bool node_raw(const gxDatabase *f)
{
  return (size_t)f->DiskLongSize() == sizeof(BtreeKeyCount) && (size_t)f->DiskFAUSize() == sizeof(FAU);
}

size_t gxBtreeDiskNodeSize(const gxDatabase *f, size_t mem_entry, BtreeNodeOrder_t node_order)
{
  size_t disk_entry = mem_entry - sizeof(FAU) + (size_t)f->DiskFAUSize();
  return disk_entry * (size_t)node_order + (size_t)f->DiskLongSize() + (size_t)f->DiskFAUSize();
}

gxDatabaseError gxBtreeReadNodeDisk(gxDatabase *f, FAU_t node_address, BtreeKeyCount &key_count, FAU &left_child,
				    char *entries, size_t mem_entry, BtreeNodeOrder_t node_order)
{
  if(node_raw(f)) {                       // the bytes as they are (10/31/2026: count and left child in one read)
    char hdr[sizeof(key_count) + sizeof(left_child)];
    if(f->Read(hdr, sizeof(hdr), node_address) != gxDBASE_NO_ERROR) return f->GetDatabaseError();
    memcpy(&key_count, hdr, sizeof(key_count));
    memcpy(&left_child, hdr + sizeof(key_count), sizeof(left_child));
    if((key_count < (BtreeKeyCount_t)0) || (key_count > (BtreeKeyCount_t)node_order)) {
      key_count = (BtreeKeyCount_t)0;
      return f->SetDatabaseError(gxDBASE_BADBLOCK_ERROR);
    }
    if(key_count > (BtreeKeyCount_t)0 &&
       f->Read(entries, (__ULWORD__)(mem_entry * (size_t)key_count)) != gxDBASE_NO_ERROR)
      return f->GetDatabaseError();
    return gxDBASE_NO_ERROR;
  }
  const size_t L = (size_t)f->DiskLongSize(), F = (size_t)f->DiskFAUSize();
  unsigned char hdr[16];
  if(f->Read(hdr, (__ULWORD__)(L + F), node_address) != gxDBASE_NO_ERROR) return f->GetDatabaseError();
  long long kc = (long long)(int64_t)f->UnpackLong(hdr);
  if(L == 4) kc = (long long)(int32_t)(uint32_t)f->UnpackLong(hdr);
  if(kc < 0 || kc > (long long)node_order) {
    key_count = (BtreeKeyCount_t)0;
    return f->SetDatabaseError(gxDBASE_BADBLOCK_ERROR);
  }
  key_count = (BtreeKeyCount_t)kc;
  left_child = (FAU)f->UnpackFAU(hdr + L);
  if(kc == 0) return gxDBASE_NO_ERROR;
  const size_t key_len = mem_entry - sizeof(FAU), disk_entry = key_len + F;
  std::vector<unsigned char> buf(disk_entry * (size_t)kc);
  if(f->Read(buf.data(), (__ULWORD__)buf.size()) != gxDBASE_NO_ERROR) return f->GetDatabaseError();
  for(long long k = 0; k < kc; k++) {
    const unsigned char *src = buf.data() + disk_entry * (size_t)k;
    char *dst = entries + mem_entry * (size_t)k;
    FAU rc = (FAU)f->UnpackFAU(src);
    memcpy(dst, &rc, sizeof(FAU));
    memcpy(dst + sizeof(FAU), src + F, key_len);
  }
  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxBtreeWriteNodeDisk(gxDatabase *f, FAU_t node_address, const BtreeKeyCount &key_count,
				     const FAU &left_child, const char *entries, size_t mem_entry)
{
  static thread_local std::vector<char> wbuf;
  size_t n = key_count > (BtreeKeyCount_t)0 ? (size_t)key_count : 0;
  if(node_raw(f)) {                       // 10/31/2026: count, left child and keys with one write
    size_t hdr_len = sizeof(key_count) + sizeof(left_child), keys_len = mem_entry * n;
    wbuf.resize(hdr_len + keys_len);
    memcpy(wbuf.data(), &key_count, sizeof(key_count));
    memcpy(wbuf.data() + sizeof(key_count), &left_child, sizeof(left_child));
    if(keys_len) memcpy(wbuf.data() + hdr_len, entries, keys_len);
  }
  else {
    const size_t L = (size_t)f->DiskLongSize(), F = (size_t)f->DiskFAUSize();
    const size_t key_len = mem_entry - sizeof(FAU), disk_entry = key_len + F;
    wbuf.assign(L + F + disk_entry * n, 0);
    f->PackLong((unsigned char *)wbuf.data(), (__ULWORD__)(long long)(BtreeKeyCount_t)key_count);
    f->PackFAU((unsigned char *)wbuf.data() + L, (FAU_t)left_child);
    for(size_t k = 0; k < n; k++) {
      const char *src = entries + mem_entry * k;
      unsigned char *dst = (unsigned char *)wbuf.data() + L + F + disk_entry * k;
      FAU rc;
      memcpy(&rc, src, sizeof(FAU));
      f->PackFAU(dst, (FAU_t)rc);
      memcpy(dst + F, src + sizeof(FAU), key_len);
    }
  }
  if(f->Write(wbuf.data(), (__ULWORD__)wbuf.size(), node_address, 0, 0) != gxDBASE_NO_ERROR)
    return f->GetDatabaseError();
  return gxDBASE_NO_ERROR;
}

} // namespace datareel

// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
