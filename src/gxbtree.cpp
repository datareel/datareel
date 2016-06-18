// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxbtree.cpp
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

The gxBtree class is a disk-base B-tree used by various database 
applications for searching and sorting structured data and indexing 
associated objects or records stored in a data file. The gxBtree 
uses the 32/64-bit gxDatabase engine to perform all disk operations, 
supporting large files and proprietary file system interfaces. 

Changes:
==============================================================
05/28/2002: Fixed possible use of null pointer in the gxBtree::Create() 
function. Use of null pointer could have occurred if memory allocation 
for the "f" variable failed and the "f" variable was used.

3/12/2003: Added cached versions of the B-tree insert, delete, 
find, and change functions. NOTE: The __USE_BTREE_CACHE__ 
preprocessor directive must be defined at compile time in order 
to use the B-tree cache.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <string.h>
#include "gxbtree.h"

#ifdef __BCC32__
#pragma warn -8066
#pragma warn -8080
#endif

gxBtree::gxBtree(DatabaseKeyB &key_type, BtreeNodeOrder_t order) 
{
  btree_header.node_order = order;
  btree_header.key_size = key_type.SizeOfDatabaseKey();
  header_in_sync = 0;
  f = 0;
  ResetStats();
}

gxBtree::~gxBtree()
{
  // PC-lint 09/08/2005: Function may throw exception in destructor
  Close(); 
}

gxDatabaseError gxBtree::InitBtree(gxDatabase *fptr, int create, 
				   FAU_t header_address, int num_trees,
				   int flushdb)
// Connect the B-tree to the database engine and write a new B-tree header if
// the "create" variable is true. Returns zero if successful or a non-zero 
// value to indicate a failure.
{
  f = fptr; 
  return InitBtree(create, header_address, num_trees, flushdb);
}

gxDatabaseError gxBtree::InitBtree(int create, FAU_t header_address, 
				   int num_trees, int flushdb)
// Connect the B-tree to a header in the open database engine or write a
// new B-tree header if the "create" variable is true. Returns zero if
// successful or a non-zero value to indicate a failure.
{
  btree_header_address = header_address;

  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  if(create) {
    if(num_trees <= 0) num_trees = 1;
    BtreeNode root_node(btree_header.key_size, btree_header.node_order); 
    FAU_t root_address = f->Alloc(root_node.SizeOfBtreeNode());

    // Check for block allocation errors
    if(root_address == (FAU_t)0) return f->GetDatabaseError();
    btree_header.root = root_address;
    btree_header.btree_height = (BtreeSize_t)1;
    btree_header.n_keys = (BtreeSize_t)0;
    btree_header.n_nodes = (BtreeSize_t)1;
    btree_header.num_trees = (BtreeSize_t)num_trees;
    if(WriteBtreeHeader() != gxDBASE_NO_ERROR) return f->GetDatabaseError();
    if(WriteNode(root_node, root_address) != gxDBASE_NO_ERROR) 
      return f->GetDatabaseError();
    if(flushdb) Flush(0); // Flush all open disk buffers after creating the tree
  }
  else {
    if(ReadBtreeHeader() != gxDBASE_NO_ERROR) return f->GetDatabaseError();
    if(f->GetVersion() == (FAU_t)4000) {
      btree_header.num_trees = 1; // This value was not recorded in ver 4.0
    }
  }

  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxBtree::Close()
// Close the B-tree. Returns zero if successful or a non-zero 
// value to indicate a failure.
{
  if(f) {
    if(WriteBtreeHeader() != gxDBASE_NO_ERROR) return f->GetDatabaseError();
    delete f; // Database engine closed by destructor call
    f = 0;              
  }
  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxBtree::Open(const char *fname, gxDatabaseAccessMode mode)
// Open an existing B-tree file. Returns zero if successful or a non-zero 
// value to indicate a failure. NOTE: This function assumes that there is
// only one B-tree in this file.
{
  if(Close() != gxDBASE_NO_ERROR) {
    // PC-lint 05/02/2005: Possible use of null pointer
    if(f) {
      return f->GetDatabaseError();
    }
    else {
      return gxDBASE_NULL_PTR;
    }
  }
  f = new gxDatabase;
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_MEM_ALLOC_ERROR;
#endif
  }

  if(f->Open(fname, mode) != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }

  return InitBtree(0, (FAU_t)f->FileHeaderSize());
}

gxDatabaseError gxBtree::Create(const char *fname, int num_trees,
				__SBYTE__ RevisionLetter)
// Create a new B-tree file, specifying the number of trees this
// index will contain and the gxDatabase revision letter. NOTE: This 
// create function only initialize the first tree header and will 
// always connect the tree to the first tree header. Returns zero 
// if successful or a non-zero value to indicate a failure.
{
  if(Close() != gxDBASE_NO_ERROR) {
    // PC-lint 05/02/2005: Possible use of null pointer
    if(f) {
      return f->GetDatabaseError();
    }
    else {
      return gxDBASE_NULL_PTR;
    }
  }

  // PC-lint 09/08/2005: PC-lint warns of possible memory leak but any
  // memory allocated to the f pointer will be freeed by the Close() call.
  if(f) delete f;
  f = new gxDatabase;
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_MEM_ALLOC_ERROR;
#endif
  }
  
  if(num_trees <= 0) num_trees = 1;
  // PC-lint 09/08/2005: Suspicious truncation warning for FAU_t(sizeof(gxBtreeHeader) * num_trees)
  FAU_t static_size = (FAU_t)sizeof(gxBtreeHeader) * num_trees;
  if(f->Create(fname, static_size, RevisionLetter) != gxDBASE_NO_ERROR)
    return f->GetDatabaseError();
  int i;
  FAU_t offset = (FAU_t)f->FileHeaderSize();
  for(i = 0; i < num_trees; i++) {
    if(InitBtree(1, offset, num_trees, 0) != gxDBASE_NO_ERROR)
      return f->GetDatabaseError();
    offset += FAU_t(sizeof(gxBtreeHeader));
  }

  Flush(0); // Flush all open disk buffers after creating the tree

  // Connect to the first tree in the index file
  if(num_trees > 1) 
    return InitBtree(0, f->FileHeaderSize());
  else
    return f->GetDatabaseError();
}

gxDatabaseError gxBtree::Create(const char *fname, __SBYTE__ RevisionLetter,
				int num_trees)
// Create a new B-tree file, specifying the gxDatabase revision letter
// and the number of trees this index will contain. NOTE: This 
// create function only initialize the first tree header and will 
// always connect the tree to the first tree header. Returns zero 
// if successful or a non-zero value to indicate a failure.
{
  return Create(fname, num_trees, RevisionLetter);
}

gxDatabaseError gxBtree::Open(const char *fname, FAU_t header_address,
			      gxDatabaseAccessMode mode)
// Open an existing B-tree file, specifying the file address of the B-tree
// header. Returns zero if successful or a non-zero value to indicate a 
// failure.
{
  if(Close() != gxDBASE_NO_ERROR) {
    // PC-lint 05/02/2005: Possible use of null pointer
    if(f) {
      return f->GetDatabaseError();
    }
    else {
      return gxDBASE_NULL_PTR;
    }
  }

  // PC-lint 09/08/2005: PC-lint warns of possible memory leak but any
  // memory allocated to the f pointer will be freeed by the Close() call.
  if(f) delete f;
  f = new gxDatabase;
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_MEM_ALLOC_ERROR;
#endif
  }

  if(f->Open(fname, mode) != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }

  return InitBtree(0, header_address);
}

gxDatabaseError gxBtree::WriteBtreeHeader()
// Write the B-tree header. Returns zero if successful or 
// a non-zero value to indicate a failure.
{
  header_writes++;

  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  if(f->ReadyForWriting()) {
    if(f->Write(&btree_header, sizeof(gxBtreeHeader), btree_header_address) !=
       gxDBASE_NO_ERROR)
      return f->GetDatabaseError();
    header_in_sync = 1;
  }

  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxBtree::WriteBtreeHeader(gxBtreeHeader &hdr, 
					  FAU_t header_address)
// Write the specified B-tree header and update the current 
// current header. Returns zero if successful or a non-zero 
// value to indicate a failure.
{
  btree_header_address = header_address;
  btree_header = hdr;
  return WriteBtreeHeader();
}

gxDatabaseError gxBtree::ReadBtreeHeader()
// Read the B-tree header. Returns zero if successful or 
// a non-zero value to indicate a failure.
{
  header_reads++;

  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  if(f->Read(&btree_header, sizeof(gxBtreeHeader), btree_header_address) !=
     gxDBASE_NO_ERROR)
    return f->GetDatabaseError();

  header_in_sync = 1;
  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxBtree::ReadBtreeHeader(gxBtreeHeader &hdr, 
					 FAU_t header_address)
// Read the current B-tree header info the specified header. 
// Returns zero if successful or a non-zero value to indicate 
// a failure.
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  if(f->Read(&hdr, sizeof(gxBtreeHeader), header_address) !=
     gxDBASE_NO_ERROR)
    return f->GetDatabaseError();

  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxBtree::Flush(int write_header)
// Flush any open disk buffers and write the B-tree header by default. 
// Returns zero if successful or a non-zero value to indicate a failure.

{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  if(write_header) {
    if(WriteBtreeHeader() != gxDBASE_NO_ERROR) return f->GetDatabaseError();
  }
  return f->Flush();
}

size_t gxBtree::TotalNodeSize()
// Returns the total size of a single B-tree node, including overhead.
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  BtreeNode node(btree_header.key_size, btree_header.node_order);
  size_t node_size = node.SizeOfBtreeNode();
  node_size += f->BlockHeaderSize();
  
  // Adjust the node size according to the GXD revision letter.
  if((f->GetRevLetter() == '\0') || (f->GetRevLetter()  == ' ')) {
    return node_size;
  }
  else {
    node_size += sizeof(gxChecksum); // Rev 'A' and higher
    return node_size;
  }
}

gxDatabaseError gxBtree::WriteNode(const BtreeNode &node, FAU_t node_address)
// Write a B-tree node to the specified file address. Returns zero if 
// successful or a non-zero value to indicate a failure.
{
  node_writes++;

  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  if(f->Write(&node.key_count, sizeof(node.key_count), node_address, 0, 0) 
     != gxDBASE_NO_ERROR)
    return f->GetDatabaseError();
  if(f->Write(&node.left_child, sizeof(node.left_child), gxCurrAddress, 0, 0) 
     != gxDBASE_NO_ERROR)
    return f->GetDatabaseError();
  
  // Optimize the write by only writing the exact number of keys
  if(node.key_count > (BtreeKeyCount_t)0) { // Ensure node has entries
    if(f->Write(node.key_entries, (node.key_size * node.key_count),
		gxCurrAddress, 0, 0) != gxDBASE_NO_ERROR)
     return f->GetDatabaseError();
  }

  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxBtree::ReadNode(BtreeNode &node, FAU_t node_address)
// Read a B-tree node from the specified file address. Returns zero if 
// successful or a non-zero value to indicate a failure.
{
  // 08/09/2001: Set the node address for operations that store this node
  node.node_address = node_address;

  node_reads++;

  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  if(f->Read(&node.key_count, sizeof(node.key_count), node_address) !=
     gxDBASE_NO_ERROR)
    return f->GetDatabaseError();
  if(f->Read(&node.left_child, sizeof(node.left_child)) != gxDBASE_NO_ERROR)
    return f->GetDatabaseError();

  // Optimize the read by only reading the exact number of keys
  if(node.key_count > (BtreeKeyCount_t)0) { // Ensure node has entries
    if(f->Read(node.key_entries, (node.key_size * node.key_count)) != 
       gxDBASE_NO_ERROR)
      return f->GetDatabaseError();
  }

  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxBtree::GrowNewRoot(DatabaseKeyB &key)
// Grow new root node. Returns zero if successful or a 
// non-zero value to indicate a failure.
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  FAU_t old_root = btree_header.root;
  BtreeNode new_root_node(btree_header.key_size, btree_header.node_order);
  FAU_t root_address = f->Alloc(new_root_node.SizeOfBtreeNode());
  if(root_address == (FAU_t)0) f->GetDatabaseError();
  btree_header.n_nodes++; 
  btree_header.btree_height++;
  btree_header.root = root_address;
  header_in_sync = 0;
  new_root_node.left_child = old_root;
  new_root_node.InsertDatabaseKey(key, (BtreeKeyLocation_t)0);
  return WriteNode(new_root_node, root_address);
}

gxDatabaseError gxBtree::CollapseRoot()
// Collapse the current root node and shorten the tree. Returns zero 
// if successful or a non-zero value to indicate a failure.
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  BtreeNode root_node(btree_header.key_size, btree_header.node_order);
  if(ReadNode(root_node, btree_header.root) != gxDBASE_NO_ERROR)
    return f->GetDatabaseError(); 

  // Ensure that the root node is empty and the height is greater then one
  if((root_node.IsEmpty()) && (btree_header.btree_height > (BtreeSize_t)1)) {
    FAU_t old_root = btree_header.root;
    btree_header.root = root_node.left_child;
    btree_header.n_nodes--; 
    btree_header.btree_height--;
    header_in_sync = 0;
    if(!f->Remove(old_root)) return f->GetDatabaseError(); 
  }
 
  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxBtree::CollapseRoot(BtreeNode &root_node)
// Collapse the specified root node after a read operation 
// and shorten the tree. Returns zero if successful or a 
// non-zero value to indicate a failure.
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  // Ensure that the root node is empty and the height is greater then one
  if((root_node.IsEmpty()) && (btree_header.btree_height > (BtreeSize_t)1))  {
    FAU_t old_root = btree_header.root;
    btree_header.root = root_node.left_child;
    btree_header.n_nodes--; 
    btree_header.btree_height--;
    header_in_sync = 0;
    if(!f->Remove(old_root)) return f->GetDatabaseError(); 
  }
 
  return gxDBASE_NO_ERROR;
}

int gxBtree::Insert(DatabaseKeyB &key, DatabaseKeyB &compare_key,
		   int flushdb)
// Insert the specified key. Returns true if successful or false 
// if the key could not be inserted.
{
  if(InsertDatabaseKey(key, compare_key) != gxDBASE_NO_ERROR) return 0;

  // Ensure that the in memory buffers and the file data
  // stay in sync during multiple file access.
  if(flushdb) Flush();

  return 1;
}

int gxBtree::Find(DatabaseKeyB &key, DatabaseKeyB &compare_key, int test_tree) 
// Find the specified key in the Btree. Returns true if successful
// or false if the key could not be found.
{
  BtreeNode curr_node(btree_header.key_size, btree_header.node_order);
  return Find(key, compare_key, curr_node, test_tree);
}

int gxBtree::FindFirst(DatabaseKeyB &key, int test_tree)
// Find the first key in the Btree. Returns true if successful
// or false if the key could not be found.
{
  BtreeNode curr_node(btree_header.key_size, btree_header.node_order);
  return FindFirst(key, curr_node, test_tree);
}

int gxBtree::FindNext(DatabaseKeyB &key, DatabaseKeyB &compare_key,
		     int test_tree)
// Find the next key after the specified key, passing back the key in the
// "key" variable. Returns true if successful or false if the key could not 
// be found.
{
  BtreeNode curr_node(btree_header.key_size, btree_header.node_order);
  return FindNext(key, compare_key, curr_node, test_tree);
}

int gxBtree::FindPrev(DatabaseKeyB &key, DatabaseKeyB &compare_key,
		     int test_tree)
// Find the previous key before the specified key, passing back the key in the
// "key" variable. Returns true if successful or false if the key could not 
// be found.
{
  BtreeNode curr_node(btree_header.key_size, btree_header.node_order);
  return FindPrev(key, compare_key, curr_node, test_tree);
}

int gxBtree::FindLast(DatabaseKeyB &key, int test_tree)
// Find the last key in the Btree. Returns true if successful
// or false if the key could not be found.
{
  BtreeNode curr_node(btree_header.key_size, btree_header.node_order);
  return FindLast(key, curr_node, test_tree);
}

int gxBtree::Delete(DatabaseKeyB &key, DatabaseKeyB &compare_key, int flushdb)
// Delete the specified key and balance the B-tree following the 
// deletion. Returns true if successful or false if the key could not 
// be deleted.

{
  if(DeleteDatabaseKey(key, compare_key) != gxDBASE_NO_ERROR) return 0;

  // Ensure that the B-tree file header stays in sync
  // during multiple file access.
  if(flushdb) Flush();

  return 1;
}

int gxBtree::LazyDelete(DatabaseKeyB &key, DatabaseKeyB &compare_key,
			int flushdb)
// Delete the specified key without balancing the tree following the 
// deletion. Returns true if successful or false if the key could not 
// be deleted. 08/17/2001: This function is left in the code to remain
// backward compatible with earlier versions and is the same as a 
// FastDelete() call.
{
  if(FastDeleteDatabaseKey(key, compare_key) != gxDBASE_NO_ERROR) return 0;

  // Ensure that the B-tree file header stays in sync
  // during multiple file access.
  if(flushdb) Flush();

  return 1;
}

int gxBtree::FastDelete(DatabaseKeyB &key, DatabaseKeyB &compare_key,
			int flushdb)
// Delete the specified key without balancing the tree following the 
// deletion. Returns true if successful or false if the key could not 
// be deleted.
{
  if(FastDeleteDatabaseKey(key, compare_key) != gxDBASE_NO_ERROR) return 0;

  // Ensure that the B-tree file header stays in sync
  // during multiple file access.
  if(flushdb) Flush();

  return 1;
}

gxDatabaseError gxBtree::InsertDatabaseKey(DatabaseKeyB &key,
					   DatabaseKeyB &compare_key)
// Non-recursive insertion function used to insert a key into the Btree.
// Returns zero if successful or a non-zero value to indicate a failure.
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  BtreeKeyLocation_t key_location;
  FAU_t curr_address = (FAU_t)0;
  key.right_child = (FAU_t)0;
  curr_address = btree_header.root;
  BtreeStack node_stack;
  BtreeNode curr_node(btree_header.key_size, btree_header.node_order);
  int num_splits = 0;

  // Walk to the leaf where the key will be inserted and look duplicate keys
  while(curr_address != (FAU_t)0) {
    if(ReadNode(curr_node, curr_address) != gxDBASE_NO_ERROR)
      return f->GetDatabaseError();
    if(!node_stack.Push(curr_address)) {
      f->SetDatabaseError(gxDBASE_MEM_ALLOC_ERROR);
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return f->GetDatabaseError();
#endif
    }

    if(curr_node.FindKeyLocation(key, compare_key, key_location) == 0) {
      // Found a duplicate key
      node_stack.Clear();
      f->SetDatabaseError(gxDBASE_DUPLICATE_ENTRY);
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return f->GetDatabaseError();
#endif
    }
    
    // Continue the search until a node with no children is found
    curr_address = curr_node.GetBranch(key_location);
  }

  while(!node_stack.IsEmpty()) {
    curr_address = node_stack.Pop();
    if(num_splits > 0) {
      if(ReadNode(curr_node, curr_address) != gxDBASE_NO_ERROR)
	return f->GetDatabaseError();
      curr_node.FindKeyLocation(key, compare_key, key_location);
    }
    key_location++; // Move to the insertion key_location
    
    if(!curr_node.IsFull()) { // Add the key if the node is not full
      curr_node.InsertDatabaseKey(key, key_location);
      node_stack.Clear();
      if(WriteNode(curr_node, curr_address) != gxDBASE_NO_ERROR)
	return f->GetDatabaseError();
      btree_header.n_keys++;
      header_in_sync = 0;
      return gxDBASE_NO_ERROR;
    }
    else { // Split the node
      num_splits++;
      BtreeNode right_node(btree_header.key_size, btree_header.node_order);
      FAU_t right_address = f->Alloc(right_node.SizeOfBtreeNode());

      // Check for block allocation errors
      if(right_address == (FAU_t)0) {
	node_stack.Clear();
	return f->GetDatabaseError();
      }
      btree_header.n_nodes++;
      header_in_sync = 0;
      
      BtreeKeyLocation_t split_location = btree_header.node_order/2;
      if(key_location < split_location) split_location--;
      curr_node.SplitNode(right_node, split_location);
      
      if(key_location > split_location) { // Insert key in the right node
	right_node.LoadKey(compare_key, (BtreeKeyLocation_t)0);
	right_node.DeleteDatabaseKey((BtreeKeyLocation_t)0);
        right_node.InsertDatabaseKey(key, key_location-split_location-1);
	
	// Pass the middle key up the tree and insert into its parent node
	memmove(key.db_key, compare_key.db_key, key.KeySize()); 
	key.right_child = compare_key.right_child;
      }
      else if(key_location < split_location) { // Insert key in the left node
        curr_node.InsertDatabaseKey(key, key_location);
	right_node.LoadKey(compare_key, (BtreeKeyLocation_t)0);
	
	// Pass the middle key up the tree and insert into its parent node
	memmove(key.db_key, compare_key.db_key, key.KeySize()); 
	key.right_child = compare_key.right_child;
	right_node.DeleteDatabaseKey((BtreeKeyLocation_t)0);
      }
      
      right_node.left_child = key.right_child;
      key.right_child = right_address;

      if(WriteNode(right_node) != gxDBASE_NO_ERROR) {
	node_stack.Clear();
	return f->GetDatabaseError();
      }
      if(WriteNode(curr_node, curr_address) != gxDBASE_NO_ERROR) {
	node_stack.Clear();
	return f->GetDatabaseError();
      }
      
      if(curr_address == btree_header.root) {
	node_stack.Clear();
	if(GrowNewRoot(key) != gxDBASE_NO_ERROR)
	  return f->GetDatabaseError();
	btree_header.n_keys++;
	header_in_sync = 0;
	return gxDBASE_NO_ERROR;
      }
    }
  }   

  // The insertion was not completed
  node_stack.Clear();
  f->SetDatabaseError(gxDBASE_INSERTION_ERROR);
#ifdef __CPP_EXCEPTIONS__
  throw gxCDatabaseException();
#endif
  return f->GetDatabaseError();
}

gxDatabaseError gxBtree::FastDeleteDatabaseKey(DatabaseKeyB &key,
					       DatabaseKeyB &compare_key)
// Fast unbalanced deletion method implemented specifically to support 
// concurrent database operations that are effected by the excessive node 
// splitting and merging that occurs during tree rotations. Returns zero 
// if successful or a non-zero value to indicate a failure. 
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  BtreeKeyLocation_t key_location;
  FAU_t parent_address = (FAU_t)0;
  FAU_t sibling_address = (FAU_t)0;
  BtreeStack node_stack;
  BtreeNode parent_node(btree_header.key_size, btree_header.node_order);
  BtreeNode sibling_node(btree_header.key_size, btree_header.node_order);
  
  int found_key = 0;
  int num_siblings = 0;
  key.right_child = (FAU_t)0;
  parent_address = btree_header.root;

  while(parent_address != (FAU_t)0) {
    if(ReadNode(parent_node, parent_address) != gxDBASE_NO_ERROR)
      return f->GetDatabaseError();

    if(parent_node.FindKeyLocation(key, compare_key, key_location) == 0) {
      sibling_address = parent_node.GetBranch(key_location);  
      found_key = 1;
      break; // Found the specified key
    }
    
    // Continue the searching for the spcified key
    parent_address = parent_node.GetBranch(key_location);  
  }

  if(!found_key) {
    f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return f->GetDatabaseError();
#endif    
  }
  
  if(sibling_address == (FAU_t)0) { // This is leaf node 
    parent_node.DeleteDatabaseKey(key_location);
    if(WriteNode(parent_node, parent_address) != gxDBASE_NO_ERROR)
      return f->GetDatabaseError();
    btree_header.n_keys--;
    header_in_sync = 0;

    // Check the root node and collapse if empty
    if(CollapseRoot() != gxDBASE_NO_ERROR) return f->GetDatabaseError();
    return gxDBASE_NO_ERROR;
  }
  else { // This is a parent node
    num_siblings++;
    if(!node_stack.Push(sibling_address)) {
      f->SetDatabaseError(gxDBASE_MEM_ALLOC_ERROR);
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return f->GetDatabaseError();
#endif
    }
    
    if(ReadNode(sibling_node, sibling_address) != gxDBASE_NO_ERROR) {
      node_stack.Clear();
      return f->GetDatabaseError();
    }
    
    // Follow the siblings to the parent node's left most child
    while(sibling_node.left_child != (FAU_t)0) {
      sibling_address = sibling_node.left_child;
      if(ReadNode(sibling_node, sibling_address) != gxDBASE_NO_ERROR) {
	node_stack.Clear();
	return f->GetDatabaseError();
      }
      if(!node_stack.Push(sibling_address)) {
	node_stack.Clear();
	f->SetDatabaseError(gxDBASE_MEM_ALLOC_ERROR);
#ifdef __CPP_EXCEPTIONS__
	throw gxCDatabaseException();
#else
	return f->GetDatabaseError();
#endif
      }
      num_siblings++;
    }
    
    // Skip over the first entry
    sibling_address = node_stack.Pop();

    // This sibling is empty due to a prior deletion
    if(sibling_node.IsEmpty()) {

      // Remove the empty node
      if(!f->Remove(sibling_address)) {
	node_stack.Clear();
	return f->GetDatabaseError();
      }

      btree_header.n_nodes--;
      header_in_sync = 0;
      num_siblings--;

      // Continue deleting the empty siblings
      while(!node_stack.IsEmpty()) {
	sibling_address = node_stack.Pop();
	if(ReadNode(sibling_node, sibling_address) != gxDBASE_NO_ERROR) {
	  node_stack.Clear();
	  return f->GetDatabaseError(); 
	}
	if(!sibling_node.IsEmpty()) break;
	
	if(!f->Remove(sibling_address)) {
	  node_stack.Clear();
	  return f->GetDatabaseError();
	}
	btree_header.n_nodes--;
	header_in_sync = 0;
	num_siblings--;
      }
    } 

    if(num_siblings == 0) { // Did not find any siblings containing keys
      parent_node.DeleteDatabaseKey(key_location);
      if(WriteNode(parent_node, parent_address) != gxDBASE_NO_ERROR) {
	node_stack.Clear();
	return f->GetDatabaseError();
      }
      btree_header.n_keys--;
      header_in_sync = 0;

      // The root node is node empty
      if((parent_node.IsEmpty()) && (parent_address == btree_header.root)) {
	if(CollapseRoot(parent_node) != gxDBASE_NO_ERROR) {
	  node_stack.Clear();
	  return f->GetDatabaseError();
	}
      }
      return gxDBASE_NO_ERROR;
    }
  }

  // Take a the first key from the sibling and overwrite the key
  // marked for deletion in parent node
  sibling_node.LoadKey(key, (BtreeKeyLocation_t)0);
  sibling_node.left_child = key.right_child;
  sibling_node.DeleteDatabaseKey((BtreeKeyLocation_t)0);
  key.right_child = compare_key.right_child; 
  parent_node.OverWriteDatabaseKey(key, key_location);
  if(WriteNode(parent_node, parent_address) != gxDBASE_NO_ERROR) {
    node_stack.Clear();
    return f->GetDatabaseError();
  }
  if(WriteNode(sibling_node, sibling_address) != gxDBASE_NO_ERROR) {
    node_stack.Clear();
    return f->GetDatabaseError();
  }
  btree_header.n_keys--;
  header_in_sync = 0;
  node_stack.Clear();
  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxBtree::DeleteDatabaseKey(DatabaseKeyB &key,
					   DatabaseKeyB &compare_key)
// Non-recursive balanced B-tree delete function used to ensure that all
// nodes, with the exception of the root node, are at least half full 
// following a deletion. Returns zero if successful or a non-zero value 
// to indicate a failure.
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  BtreeKeyLocation_t key_location;
  FAU_t curr_address = (FAU_t)0;
  FAU_t sibling_address = (FAU_t)0;
  BtreeStack node_stack;  
  BtreeNode curr_node(btree_header.key_size, btree_header.node_order);
  BtreeNode *leaf_node;
  BtreeNode parent_node(btree_header.key_size, btree_header.node_order);
  BtreeNode sibling_node(btree_header.key_size, btree_header.node_order);
  int found_key = 0;
  key.right_child = (FAU_t)0;
  curr_address = btree_header.root;

  if(!node_stack.Push(btree_header.root)) {
    node_stack.Clear();
    f->SetDatabaseError(gxDBASE_MEM_ALLOC_ERROR);
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return f->GetDatabaseError();
#endif
  }
  
  // Search the B-tree for the specified key
  while(curr_address != (FAU_t)0) {
    if(ReadNode(curr_node, curr_address) != gxDBASE_NO_ERROR) {
      node_stack.Clear();
      return f->GetDatabaseError();
    }

    if(curr_node.FindKeyLocation(key, compare_key, key_location) == 0) {
      sibling_address = curr_node.GetBranch(key_location);  
      found_key = 1;
      break; // Found the specified key
    }
    
    // Push parent and grandparent addresses for rotations
    // that must be propagated up the tree
    if(!node_stack.Push(curr_address)) {
      node_stack.Clear();
      f->SetDatabaseError(gxDBASE_MEM_ALLOC_ERROR);
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return f->GetDatabaseError();
#endif
    }
    
    // Continue the searching for the key
    curr_address = curr_node.GetBranch(key_location);  
  }

  if(!found_key) { // The key was not found
    f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return f->GetDatabaseError();
#endif    
  }

  if(sibling_address != (FAU_t)0) { // This is a parent node
    // Push the siblings parent node
    if(!node_stack.Push(curr_address)) {
      node_stack.Clear();
      f->SetDatabaseError(gxDBASE_MEM_ALLOC_ERROR);
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return f->GetDatabaseError();
#endif
    }
    if(ReadNode(sibling_node, sibling_address) != gxDBASE_NO_ERROR) {
      node_stack.Clear();
      return f->GetDatabaseError();
    }
    
    // Follow the siblings to the parent node's left most child
    while(sibling_node.left_child != (FAU_t)0) {

      // Push all parent and grandparent nodes
      if(!node_stack.Push(sibling_address)) {
	node_stack.Clear();
	f->SetDatabaseError(gxDBASE_MEM_ALLOC_ERROR);
#ifdef __CPP_EXCEPTIONS__
	throw gxCDatabaseException();
#else
	return f->GetDatabaseError();
#endif
      }
      sibling_address = sibling_node.left_child;
      if(ReadNode(sibling_node, sibling_address) != gxDBASE_NO_ERROR) {
	node_stack.Clear();
	return f->GetDatabaseError();
      }
    }
    
    // Take a the first key from the sibling and overwrite the key
    // marked for deletion in parent node
    sibling_node.LoadKey(key, (BtreeKeyLocation_t)0); // Promote this key
    sibling_node.DeleteDatabaseKey((BtreeKeyLocation_t)0);
    key.right_child = compare_key.right_child; 
    curr_node.OverWriteDatabaseKey(key, key_location);
    if(WriteNode(curr_node, curr_address) != gxDBASE_NO_ERROR) {
      node_stack.Clear();
      return f->GetDatabaseError();
    }
    if(WriteNode(sibling_node, sibling_address) != gxDBASE_NO_ERROR) {
      node_stack.Clear();
      return f->GetDatabaseError();
    }
    btree_header.n_keys--;
    header_in_sync = 0;
    parent_node.node_address = node_stack.Pop();
    leaf_node = &sibling_node;
    leaf_node->node_address = sibling_address;
  }
  else { // This is a leaf (node with no children)
    curr_node.DeleteDatabaseKey(key_location);
    if(WriteNode(curr_node, curr_address) != gxDBASE_NO_ERROR) {
      node_stack.Clear();
      return f->GetDatabaseError();
    }
    btree_header.n_keys--;
    header_in_sync = 0;
    parent_node.node_address = node_stack.Pop();
    leaf_node = &curr_node;
    leaf_node->node_address = curr_address;
  }

  // Insert balance if the node does not contain the minimum number of entries
  if((leaf_node->HasFew()) && (leaf_node->node_address != btree_header.root)) {
    if(ReadNode(parent_node, parent_node.node_address) != gxDBASE_NO_ERROR) {
      node_stack.Clear();
      return f->GetDatabaseError();
    }
    parent_node.FindKeyLocation(key, compare_key, key_location);
    if(InsertBalance(parent_node, key_location, compare_key) !=
       gxDBASE_NO_ERROR) {
      node_stack.Clear();
      return f->GetDatabaseError();
    }

    // Ensure that the node's parent contains the minimum number of entries
    // NOTE: The root node is allowed to have less then the minimum number
    // of entries and may be empty.
    if((parent_node.HasFew()) && 
       (parent_node.node_address != btree_header.root)) {
      while(!node_stack.IsEmpty()) {
	// Loop until all the grandparent nodes are balanced
	BtreeNode grandparent_node(btree_header.key_size, 
				   btree_header.node_order);
	grandparent_node.node_address = node_stack.Pop();
	if(ReadNode(grandparent_node, grandparent_node.node_address) !=
	   gxDBASE_NO_ERROR) {
	  node_stack.Clear();
	  return f->GetDatabaseError();
	}
        grandparent_node.FindKeyLocation(key, compare_key, key_location);
        if(InsertBalance(grandparent_node, key_location, compare_key) !=
	   gxDBASE_NO_ERROR) {
	  node_stack.Clear();
	  return f->GetDatabaseError();
	}
	if((grandparent_node.HasFew()) && 
	   (grandparent_node.node_address != btree_header.root)) {
	  continue;
	}
	else {
	  break;
	}
      }
    }
  }
    
  // Clear the node stack
  node_stack.Clear();
  
  // Check the root node and collapse if empty
  if(CollapseRoot() != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }

  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxBtree::InsertBalance(BtreeNode &parent_node, 
				       BtreeKeyLocation_t key_location,
				       DatabaseKeyB &compare_key)
// Internal function used to balance this parent's sibling node after 
// performing a deletion. Returns zero if successful or a non-zero value 
// to indicate a failure.
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  FAU_t left_child = (FAU_t)0;
  FAU_t right_child = (FAU_t)0;
  BtreeNode left_node(btree_header.key_size, btree_header.node_order);
  BtreeNode right_node(btree_header.key_size, btree_header.node_order);
  
  if(key_location == (BtreeKeyLocation_t)-1) {
    // The parent node does not have a left child
    right_child = parent_node.GetBranch((BtreeKeyLocation_t)0);
    if(ReadNode(right_node, right_child) != gxDBASE_NO_ERROR) {
      return f->GetDatabaseError();
    }
    if(right_node.HasMany()) { // Take a key from the right child
      if(InsertLeftRotation(parent_node, (BtreeKeyLocation_t)0,
			    compare_key) != gxDBASE_NO_ERROR) {
	return f->GetDatabaseError();
      } 
    }
    else { 
      if(Merge(parent_node, (BtreeKeyLocation_t)0, compare_key) !=
	 gxDBASE_NO_ERROR) {
	return f->GetDatabaseError();
      }
    }
  }
  else if(key_location == parent_node.HighestKey()) {
    // The parent node does not have a right child
    left_child = parent_node.GetBranch(key_location-1);
    if(ReadNode(left_node, left_child) != gxDBASE_NO_ERROR) {
      return f->GetDatabaseError();
    }
    if(left_node.HasMany()) { // Take a key from the left child
      if(InsertRightRotation(parent_node, key_location, compare_key) !=
	 gxDBASE_NO_ERROR) {
	return f->GetDatabaseError();
      }
    }
    else {
      if(Merge(parent_node, key_location, compare_key) != gxDBASE_NO_ERROR) {
	return f->GetDatabaseError();
      }
    }
  }
  else {
    // The parent node has both left and right siblings
    left_child = parent_node.GetBranch(key_location-1);
    if(ReadNode(left_node, left_child) != gxDBASE_NO_ERROR) {
      return f->GetDatabaseError();
    }
    if(left_node.HasMany()) { // Take a key from the left child
      if(InsertRightRotation(parent_node, key_location, compare_key) !=
	 gxDBASE_NO_ERROR) {
	return f->GetDatabaseError();
      }
    }
    else {
      right_child = parent_node.GetBranch(key_location+1);
      if(ReadNode(right_node, right_child) != gxDBASE_NO_ERROR) {
	return f->GetDatabaseError();
      }
      if(right_node.HasMany()) { // Take a key from the right child
        if(InsertLeftRotation(parent_node, key_location+1, compare_key) !=
	   gxDBASE_NO_ERROR) {
	  return f->GetDatabaseError();
	}
      }
      else {
        if(Merge(parent_node, key_location, compare_key) != gxDBASE_NO_ERROR) {
	  return f->GetDatabaseError();
	}
      }
    }
  }
  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxBtree::InsertRightRotation(BtreeNode &parent_node, 
					     BtreeKeyLocation_t key_location,
					     DatabaseKeyB &compare_key)
// Internal function used by the gxBtree::InsertBalance() function to perform 
// a right rotation. Returns zero if successful or a non-zero value to
// indicate a failure.
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  FAU_t right_child = parent_node.GetBranch(key_location);
  FAU_t left_child = parent_node.GetBranch(key_location-1);
  BtreeNode right_node(btree_header.key_size, btree_header.node_order);
  BtreeNode left_node(btree_header.key_size, btree_header.node_order);

  if(ReadNode(right_node, right_child) != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }

  if(ReadNode(left_node, left_child) != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }
  
  // Move the key from the parent node to the right most location of the
  // right child. 
  parent_node.LoadKey(compare_key, key_location);
  compare_key.right_child = right_node.left_child;
  right_node.InsertDatabaseKey(compare_key, (BtreeKeyLocation_t)0);


  // Move the right most key of the left child into the parent node.
  BtreeKeyLocation_t highest_key = left_node.HighestKey();
  right_node.left_child = left_node.GetBranch(highest_key);
  left_node.LoadKey(compare_key, highest_key);
  compare_key.right_child = right_child;
  parent_node.OverWriteDatabaseKey(compare_key, key_location);
  left_node.DeleteDatabaseKey(highest_key);

  if(WriteNode(parent_node, parent_node.node_address) != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }
  if(WriteNode(right_node, right_child) != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }
  if(WriteNode(left_node, left_child) != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }

  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxBtree::InsertLeftRotation(BtreeNode &parent_node,
					    BtreeKeyLocation_t key_location,
					    DatabaseKeyB &compare_key)
// Internal function used by the gxBtree::InsertBalance() function to perform a 
// left rotation. Returns zero if successful or a non-zero value to indicate a 
// failure.
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  FAU_t right_child = parent_node.GetBranch(key_location);
  FAU_t left_child = parent_node.GetBranch(key_location-1);
  BtreeNode right_node(btree_header.key_size, btree_header.node_order);
  BtreeNode left_node(btree_header.key_size, btree_header.node_order);
  if(ReadNode(right_node, right_child) != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }
  if(ReadNode(left_node, left_child) != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }
  
  // Move the key from the parent node to the left most location of the 
  // left child
  parent_node.LoadKey(compare_key, key_location);
  compare_key.right_child = right_node.left_child;
  left_node.AppendDatabaseKey(compare_key);

  // Move the move the right most key of the right child into the
  // parent node.
  right_node.left_child = right_node.GetBranch((BtreeKeyLocation_t)0);  
  right_node.LoadKey(compare_key, (BtreeKeyLocation_t)0);
  compare_key.right_child = right_child;
  parent_node.OverWriteDatabaseKey(compare_key, key_location);
  right_node.DeleteDatabaseKey((BtreeKeyLocation_t)0);

  if(WriteNode(parent_node, parent_node.node_address) != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }
  if(WriteNode(right_node, right_child) != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }
  if(WriteNode(left_node, left_child) != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }

  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxBtree::Merge(BtreeNode &parent_node,
			       BtreeKeyLocation_t key_location,
			       DatabaseKeyB &compare_key)
// Internal function used by the gxBtree::InsertBalance() function to merge 
// the parents left and right siblings and remove the right sibling. Returns 
// zero if successful or a non-zero value to indicate a failure.
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  FAU_t right_child = parent_node.GetBranch(key_location);
  FAU_t left_child = parent_node.GetBranch(key_location-1);
  BtreeNode right_node(btree_header.key_size, btree_header.node_order);
  BtreeNode left_node(btree_header.key_size, btree_header.node_order);
  if(ReadNode(right_node, right_child) != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }
  if(ReadNode(left_node, left_child) != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }

  // Move the key from the parent node to the left most location of the 
  // left child.
  parent_node.LoadKey(compare_key, key_location);
  compare_key.right_child = right_node.left_child;
  left_node.AppendDatabaseKey(compare_key);
  parent_node.DeleteDatabaseKey(key_location);

  // Merge the left and right siblings
  left_node.MergeNode(right_node);
  if(!f->Remove(right_child)) { // Remove the right child
    return f->GetDatabaseError();
  }
  btree_header.n_nodes--;
  header_in_sync = 0;
  
  if(WriteNode(parent_node, parent_node.node_address) != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }
  if(WriteNode(left_node, left_child) != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }

  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxBtree::ReConnect(int tree_number)
// Reconnect the B-tree header to the another tree in the same 
// index file. Returns zero if successful or a non-zero value to 
// indicate a failure.
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  // Write the current B-tree header and flush all disk buffers
  if(Flush() != gxDBASE_NO_ERROR) return f->GetDatabaseError(); 

  // Calculate the current B-tree header address
  btree_header_address = (FAU_t)f->FileHeaderSize();
  if(tree_number > 0) tree_number--;
  btree_header_address += (FAU_t)tree_number * sizeof(gxBtreeHeader);

  return ReadBtreeHeader();          
}

gxDatabaseError gxBtree::ReInit(FAU_t header_address, int flushdb)
// Reconnect the B-tree header to ensure that the file data stays
// in sync during multiple file access and multiple instances of the 
// same application. If the flush variable is true, the file buffers
// will be flushed to disk before reconnecting the Btree. Returns 
// zero if successful or a non-zero value to indicate a failure.
{
  if(flushdb) Flush();
  btree_header_address = header_address;
  return ReadBtreeHeader();          
}

int gxBtree::TestTree(FAU_t header_address, int reinit)
// This function is used to ensure that the file data stays in 
// sync during multiple file access. Returns zero if no errors
// were encountered or an integer value corresponding to one of 
// the following values:
//
// 1 = Node order error 
// 2 = Key size error 
// 3 = Number of keys error
// 4 = Number of nodes error
// 5 = B-tree height error
// 6 = B-tree root address error
// 7 = B-tree number of trees error
// 8 = Error reading the B-tree header
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  int error_number = 0;
  gxBtreeHeader curr_btree_header;
  btree_header_address = header_address;

  if(f->Read(&curr_btree_header, sizeof(gxBtreeHeader), 
	     btree_header_address) !=  gxDBASE_NO_ERROR) {
    return error_number = 8; // Error reading the btree header
  }

  if(curr_btree_header.node_order != btree_header.node_order) {
    error_number = 1; // 1 = Node order error 
  }

  if(curr_btree_header.key_size != btree_header.key_size) {
    error_number = 2; // 2 = Key size error 
  }

  if(curr_btree_header.n_keys != btree_header.n_keys) {
    error_number = 3; // 3 = Number of keys error
  }

  if(curr_btree_header.n_nodes != btree_header.n_nodes) {
    error_number = 4 ; // 4 = Number of nodes error
  }

  if(curr_btree_header.btree_height != btree_header.btree_height) {
    error_number = 5; // 5 = B-tree height error
  }

  if(curr_btree_header.root != btree_header.root) {
    error_number = 6; // 6 = B-tree root address error
  }

  if(curr_btree_header.num_trees != btree_header.num_trees) {
    error_number = 7; // 7 = B-tree number of trees error
  }

  if(error_number > 0) {
    if(reinit) ReInit();
  }

  return error_number;
}

const char *gxBtree::DatabaseExceptionMessage()
// Returns a null terminated string that can
// be used to log or print a database exception.
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) return gxDatabaseExceptionMessage(gxDBASE_NULL_PTR);

  return gxDatabaseExceptionMessage(f->GetDatabaseError());
}

int gxBtree::PartialFind(DatabaseKeyB &key, DatabaseKeyB &compare_key, 
			 int test_tree)
// Partial lookup function used to find a key equal to or greater then the 
// specified key even if the specified key does not exist in the B-tree. 
// Loads the exact or partial match and returns true if successful. 
// Returns false if the tree is empty or a partial match greater then 
// the specified key cannot be found.
{
  BtreeNode curr_node(btree_header.key_size, btree_header.node_order);
  return PartialFind(key, compare_key, curr_node, test_tree);
}

int gxBtree::Find(DatabaseKeyB &key, DatabaseKeyB &compare_key, 
		  BtreeNode &curr_node, int test_tree) 
// Find the specified key in the Btree. Returns true if successful
// or false if the key could not be found. This version of the find 
// function is optimized for sort order searches. The "curr_node" 
// variable is used to store the node where the specified key was
// found. If the specified key is found in the node this function 
// returns true. If the key is not less then or greater then all the
// keys in the "curr_node" variable and a child pointer is found 
// the child pointer is used as the starting point for this search. 
// If the key is less then or greater then all the key in the 
// "curr_node" variable or there is no child pointer the root node
// is used as the starting point for this search. NOTE: To 
// disable the optimized search set the "curr_node" key count
// to zero before calling this function.
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  // Ensure that the in memory buffers and the file data
  // stay in sync during multiple file access.
  if(test_tree) TestTree();

  // Search the current node for the specified key
  // PC-lint 09/08/2005: key_location may not be initialized on line 1403
  BtreeKeyLocation_t key_location = (BtreeKeyLocation_t)0;
  FAU_t curr_address = (FAU_t)0;

  if(curr_node.key_count > (BtreeKeyLocation_t)0) {
    if(curr_node.FindKeyLocation(key, compare_key, key_location) == 0) {
      // Load the key data in the event that this is a partial look up
      curr_node.LoadKey(key, key_location); 
      return 1; // Found a matching node
    }

    // 09/10/2001: Set the current address if this node has keys.
    // The key was not found so check this node for child pointer
    curr_address =  curr_node.GetBranch(key_location);
  }

  if(curr_address != (FAU_t)0) { // This node has a child
    if(key_location < (BtreeKeyLocation_t)0) { 
      // This key is less then all the entries in this node so do not
      // follow this child link.
      curr_address = btree_header.root;
    }
    curr_node.LoadKey(compare_key, key_location);
#ifndef __USE_SINGLE_COMPARE__
    if(key > compare_key) {
      // This key is greater then all the entries in this node so do not
      // follow this child link.
      curr_address = btree_header.root;
    }
#else
    if(key.CompareKey(compare_key) > 0) {
      // This key is greater then all the entries in this node so do not
      // follow this child link.
      curr_address = btree_header.root;
    }
#endif // __USE_SINGLE_COMPARE__
  }

  // Did not find the key in the current node. There are no links that 
  // point up the B-tree so we have to start our search at the root node.
  curr_address = btree_header.root; 

  while(curr_address != (FAU_t)0) {
    if(ReadNode(curr_node, curr_address) != gxDBASE_NO_ERROR) return 0;
    if(curr_node.FindKeyLocation(key, compare_key, key_location) == 0) {
      // Load the key data in the event that this is a partial look up
      curr_node.LoadKey(key, key_location); 
      return 1; // Found a matching node
    }
    curr_address = curr_node.GetBranch(key_location);
  }
  
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);

  return 0;
}

int gxBtree::FindFirst(DatabaseKeyB &key, BtreeNode &curr_node, int test_tree)
// Find the first key in the Btree. Returns true if successful
// or false if the key could not be found. This version of the 
// find first function is used with the optimized sort order
// search function. When the first key in the B-tree is
// found the node containing the key is passed back to the
// caller in the "curr_node" variable.
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  // Ensure that the in memory buffers and the file data
  // stay in sync during multiple file access.
  if(test_tree) TestTree();

  if(IsEmpty()) return 0;

  FAU_t curr_address = btree_header.root;
  if(ReadNode(curr_node, curr_address) != gxDBASE_NO_ERROR) return 0;
  
  while(curr_node.left_child != (FAU_t)0) {
    curr_address = curr_node.left_child;
    if(ReadNode(curr_node, curr_address) != gxDBASE_NO_ERROR) return 0;
  }
  if(curr_node.key_count <= (BtreeKeyCount)0) {
    f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);
    return 0;
  }
  curr_node.LoadKey(key, (BtreeKeyLocation_t)0);
  return 1;
}

int gxBtree::FindLast(DatabaseKeyB &key, BtreeNode &curr_node, int test_tree)
// Find the last key in the Btree. Returns true if successful
// or false if the key could not be found. This version of the 
// find last function is used with the optimized sort order
// search functions. When the last key in the B-tree is
// found the node containing the key is passed back to the
// caller in the "curr_node" variable.
{
  // Ensure that the in memory buffers and the file data
  // stay in sync during multiple file access.
  if(test_tree) TestTree();
  
  if (IsEmpty()) return 0;
  
  FAU_t curr_address = btree_header.root;
  if(ReadNode(curr_node, curr_address) != gxDBASE_NO_ERROR) return 0;

  while(curr_node.GetBranch((BtreeKeyLocation_t)curr_node.key_count-1)) {
    curr_address = \
      curr_node.GetBranch((BtreeKeyLocation_t)curr_node.key_count-1);
    if(ReadNode(curr_node, curr_address)!= gxDBASE_NO_ERROR) return 0;
  }
  if(curr_node.key_count <= (BtreeKeyCount_t)0) return 0;
  curr_node.LoadKey(key, curr_node.key_count-1);
  return 1;
}

int gxBtree::FindNext(DatabaseKeyB &key, DatabaseKeyB &compare_key,
		      BtreeNode &curr_node, int test_tree)
// Find the next key after the specified key, passing back the key in the
// "key" variable. Returns true if successful or false if the key could not 
// be found. This version of the find next function is optimized for sort 
// order searches. The "curr_node" variable is used to store the node where 
// the specified key was found. If the specified key is found this function 
// will search for the next node following the current node child pointers.
// If the next sort order node is found this function returns true. If the 
// next sort order node is not found the root node is used as the starting 
// point for this search. NOTE: To disable the optimized search set the 
// "curr_node" key count to zero before calling this function.
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  // Ensure that the in memory buffers and the file data
  // stay in sync during multiple file access.
  if(test_tree) TestTree();
  
  // Search the current node for the specified key
  FAU_t curr_address = (FAU_t)0;
  BtreeKeyLocation_t key_location;
  if(curr_node.key_count > (BtreeKeyLocation_t)0) {
    if(curr_node.FindKeyLocation(key, compare_key, key_location) == 0) {
      if(curr_node.GetBranch(key_location) != (FAU_t)0) {
        curr_address = curr_node.GetBranch(key_location);
        if(ReadNode(curr_node, curr_address) != gxDBASE_NO_ERROR) return 0;
	while(curr_node.left_child != (FAU_t)0) { // Get left most node
          curr_address = curr_node.left_child;
          if(ReadNode(curr_node, curr_address)!= gxDBASE_NO_ERROR) return 0;
	}
        if(curr_node.key_count <= (BtreeKeyCount_t)0) {
	  f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);
	  return 0;
	}
        curr_node.LoadKey(key, (BtreeKeyLocation_t)0);
	return 1;
      }
      else {
        if(key_location < curr_node.key_count-1) {
          curr_node.LoadKey(key, key_location+1);
	  return 1;
	}
      }
    }
  }

  // Did not find the key or next key in the current node so start searching
  // from the root node.
  curr_address = btree_header.root; 
  FAU_t parent_address = (FAU_t)0;
  BtreeNode parent_node(btree_header.key_size, btree_header.node_order);
  int rv;

  // 06/02/2002: Initialize to eliminate W8013 warning under BCC32. 
  // Possible use of variable before definition in function.
  BtreeKeyLocation_t sav_location = (BtreeKeyLocation_t)0;
 
  while(curr_address != (FAU_t)0) { 
    if(ReadNode(curr_node, curr_address) != gxDBASE_NO_ERROR) return 0;
    rv = curr_node.FindKeyLocation(key, compare_key, key_location);
    if(rv == 0) { // Found matching key, search for the next key in sort order
      if(curr_node.GetBranch(key_location) != (FAU_t)0) {
        curr_address = curr_node.GetBranch(key_location);
        if(ReadNode(curr_node, curr_address) != gxDBASE_NO_ERROR) return 0;
	while(curr_node.left_child != (FAU_t)0) { // Get left most node
          curr_address = curr_node.left_child;
          if(ReadNode(curr_node, curr_address)!= gxDBASE_NO_ERROR) return 0;
	}
        if(curr_node.key_count <= (BtreeKeyCount_t)0) {
	  f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);
	  return 0;
	}
        curr_node.LoadKey(key, (BtreeKeyLocation_t)0);
	return 1;
      }
      else {
        if(key_location < curr_node.key_count-1) {
          curr_node.LoadKey(key, key_location+1);
	  return 1;
	}
	if(parent_address != (FAU_t)0) { // Go back to the parent node
          if(ReadNode(parent_node, parent_address)!= gxDBASE_NO_ERROR) 
	    return 0;
          if(parent_node.key_count <= (BtreeKeyCount_t)0) {
	    f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);
	    return 0;
	  }
          if(++sav_location >= parent_node.key_count) {
	    f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);
	    return 0;
	  }
          parent_node.LoadKey(key, sav_location);
	  return 1;
	}
	return 0;
      }
    }
    if(key_location < curr_node.key_count-1) {
      parent_address = curr_address;
      sav_location = key_location;
    }
    curr_address = curr_node.GetBranch(key_location); // No match, keep walking
  }

  f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);
  return 0;
}

int gxBtree::FindPrev(DatabaseKeyB &key, DatabaseKeyB &compare_key,
		      BtreeNode &curr_node, int test_tree)
// Find the previous key before the specified key, passing back the key in the
// "key" variable. Returns true if successful or false if the key could not 
// be found. This version of the find previous function is optimized for sort 
// order searches. The "curr_node" variable is used to store the node where 
// the specified key was found. If the specified key is found this function 
// will search for the previous node following the current node child pointers.
// If the previous sort order node is found this function returns true. If the 
// next sort order node is not found the root node is used as the starting 
// point for this search. NOTE: To disable the optimized search set the 
// "curr_node" key count to zero before calling this function.
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  // Ensure that the in memory buffers and the file data
  // stay in sync during multiple file access.
  if(test_tree) TestTree();
  
  // Search the current node for the specified key
  FAU_t curr_address;
  BtreeKeyLocation_t key_location;
  if(curr_node.key_count > (BtreeKeyLocation_t)0) {
    if(curr_node.FindKeyLocation(key, compare_key, key_location) == 0) {
      if(--key_location >= (BtreeKeyLocation_t)0) {
        while(curr_node.GetBranch(key_location) != (FAU_t)0) {
          curr_address = curr_node.GetBranch(key_location);
          if(ReadNode(curr_node, curr_address)!= gxDBASE_NO_ERROR) return 0;
          key_location = curr_node.key_count-1;
	}
        curr_node.LoadKey(key, key_location);
	return 1;
      }
      if(curr_node.left_child != (FAU_t)0) {
        curr_address = curr_node.left_child;
        if(ReadNode(curr_node, curr_address) != gxDBASE_NO_ERROR) return 0;
        key_location = curr_node.key_count-1;
        while(curr_node.GetBranch(key_location) != (FAU_t)0) {
          curr_address = curr_node.GetBranch(key_location);
          if(ReadNode(curr_node, curr_address) != gxDBASE_NO_ERROR) return 0;
          key_location = curr_node.key_count-1;
	}
        curr_node.LoadKey(key, key_location);
	return 1;
      }
    }
  }
  
  // Did not find the key or previous key in the current node so start 
  // searching from the root node.
  curr_address = btree_header.root;
  FAU_t parent_address = (FAU_t)0;

  BtreeNode parent_node(btree_header.key_size, btree_header.node_order);
  int rv;
  BtreeKeyLocation_t sav_location = -1;

  while(curr_address != (FAU_t)0) {
    if(ReadNode(curr_node, curr_address)!= gxDBASE_NO_ERROR) return 0;
    rv = curr_node.FindKeyLocation(key, compare_key, key_location);
    if(rv == 0) {
      // Found matching key, search for the previous key in sort order 
      if(--key_location >= (BtreeKeyLocation_t)0) {
        while(curr_node.GetBranch(key_location) != (FAU_t)0) {
          curr_address = curr_node.GetBranch(key_location);
          if(ReadNode(curr_node, curr_address)!= gxDBASE_NO_ERROR) return 0;
          key_location = curr_node.key_count-1;
	}
        curr_node.LoadKey(key, key_location);
	return 1;
      }
      if(curr_node.left_child != (FAU_t)0) {
        curr_address = curr_node.left_child;
        if(ReadNode(curr_node, curr_address) != gxDBASE_NO_ERROR) return 0;
        key_location = curr_node.key_count-1;
        while(curr_node.GetBranch(key_location) != (FAU_t)0) {
          curr_address = curr_node.GetBranch(key_location);
          if(ReadNode(curr_node, curr_address) != gxDBASE_NO_ERROR) return 0;
          key_location = curr_node.key_count-1;
	}
        curr_node.LoadKey(key, key_location);
	return 1;
      }
      if(parent_address != (FAU_t)0) { // Go back to the parent node
        parent_node.LoadKey(key, sav_location);
	return 1;
      }
      f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);
      return 0;
    }
    else if (rv > 0) { // Save the parent node only if it branches to the right
      parent_address = curr_address;
      if(ReadNode(parent_node, parent_address)!= gxDBASE_NO_ERROR) return 0;
      sav_location = key_location;
    }
    curr_address = curr_node.GetBranch(key_location); // Keep walking 
  }
  f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);
  return 0;
}

int gxBtree::PartialFind(DatabaseKeyB &key, DatabaseKeyB &compare_key, 
			 BtreeNode &curr_node, int test_tree)
// Partial lookup function used to find a key equal to or greater then the 
// specified key even if the specified key does not exist in the B-tree. 
// Loads the exact or partial match and returns true if successful. 
// Returns false if the tree is empty or a partial match greater then 
// the specified key cannot be found. This version of the partial function 
// is used with the optimized sort order search function. When the exact or
// partial match is found the node containing the key is passed back to the
// caller in the "curr_node" variable.
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  // Ensure that the in memory buffers and the file data
  // stay in sync during multiple file access.
  if(test_tree) TestTree();
  
  FAU_t curr_address = btree_header.root;
  FAU_t parent_address = (FAU_t)0;
  BtreeKeyLocation_t key_location = (BtreeKeyLocation_t)0;
  BtreeKeyLocation_t sav_location = (BtreeKeyLocation_t)0;
  BtreeNode parent_node(btree_header.key_size, btree_header.node_order);

  while(curr_address != (FAU_t)0) {
    if(ReadNode(curr_node, curr_address) != gxDBASE_NO_ERROR) return 0;
    if(curr_node.FindKeyLocation(key, compare_key, key_location) == 0) {
      // Found an exact match, so load the key and return 
      curr_node.LoadKey(key, key_location); 
      return 1; 
    }

    // Record this node's parent address to allow us to point to the  
    // node in order if the search fails at a leaf node.
    if(key_location < curr_node.key_count-1) {
      parent_address = curr_address;
      sav_location = key_location;
    }
    curr_address = curr_node.GetBranch(key_location);
  }

  // Search for the next key where the search for an exact match failed.
  if(curr_node.GetBranch(key_location) != (FAU_t)0) {
    curr_address = curr_node.GetBranch(key_location);
    if(ReadNode(curr_node, curr_address) != gxDBASE_NO_ERROR) return 0;
    while(curr_node.left_child != (FAU_t)0) { // Get left most node
      curr_address = curr_node.left_child;
      if(ReadNode(curr_node, curr_address)!= gxDBASE_NO_ERROR) return 0;
    }
    if(curr_node.key_count <= (BtreeKeyCount_t)0) {
      f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);
      return 0;
    }
    curr_node.LoadKey(key, (BtreeKeyLocation_t)0);
    return 1;
  }
  else {
    if(key_location < curr_node.key_count-1) {
      curr_node.LoadKey(key, key_location+1);
      return 1;
    }
    if(parent_address != (FAU_t)0) { // Go back to the parent node
      if(ReadNode(parent_node, parent_address)!= gxDBASE_NO_ERROR) 
	return 0;
      if(parent_node.key_count <= (BtreeKeyCount_t)0) {
	f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);
	return 0;
      }
      if(++sav_location >= parent_node.key_count) {
	f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);
	return 0;
      }
      parent_node.LoadKey(key, sav_location);
      return 1;
    }
    return 0;
  }

  // 06/02/2002: Eliminate unreachable code in function warning under BCC32
  // PC-lint 05/02/2005: Ingore PC-lint unreachable code warning
  // Some compilers require a return value even if code is not reached
#ifndef __BCC32__
  // Could not make a partial match
  f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);
  return 0;
#endif
}

int gxBtree::ChangeKey(DatabaseKeyB &key, DatabaseKeyB &new_key_vals,
		       DatabaseKeyB &compare_key, int test_tree)
// Function used to change a fixed length key in the index file.
// Returns true if the change was changed or false if the key
// does not exist in the database or a file error occurred.
{
 BtreeNode curr_node(btree_header.key_size, btree_header.node_order);
 return ChangeKey(key, new_key_vals, compare_key, curr_node, 
		  test_tree);
}

int gxBtree::ChangeKey(DatabaseKeyB &key, DatabaseKeyB &new_key_vals,
		       DatabaseKeyB &compare_key, BtreeNode &curr_node, 
		       int test_tree)
// Function used to change a fixed length key in the index file.
// Returns true if the change was changed or false if the key
// does not exist in the database or a file error occurred.
{
  if(!Find(key, compare_key, curr_node, test_tree)) return 0;
  BtreeKeyLocation_t key_location;
  if(curr_node.FindKeyLocation(key, compare_key, key_location) == 0) {
    curr_node.OverWriteDatabaseKey(new_key_vals, key_location);
  }
  else {
    return 0;
  }
  if(WriteNode(curr_node, curr_node.node_address) != gxDBASE_NO_ERROR) {
    return 0;
  }
  return 1;
}

int gxBtree::ChangeOrAddKey(DatabaseKeyB &key, DatabaseKeyB &new_key_vals,
			    DatabaseKeyB &compare_key, int test_tree)
{
 BtreeNode curr_node(btree_header.key_size, btree_header.node_order);
 return ChangeOrAddKey(key, new_key_vals, compare_key, 
		       curr_node, test_tree);
}

int gxBtree::ChangeOrAddKey(DatabaseKeyB &key, DatabaseKeyB &new_key_vals,
			    DatabaseKeyB &compare_key, BtreeNode &curr_node, 
			    int test_tree)
{
  if(!Find(key, compare_key, curr_node, test_tree)) {
    return Insert(key, compare_key, test_tree);
  }
  BtreeKeyLocation_t key_location;
  if(curr_node.FindKeyLocation(key, compare_key, key_location) == 0) {
    curr_node.OverWriteDatabaseKey(new_key_vals, key_location);
  }
  else {
    return 0;
  }
  if(WriteNode(curr_node, curr_node.node_address) != gxDBASE_NO_ERROR) {
    return 0;
  }
  return 1;
}

// =========================================================== //
// Begin - B-tree cache additions
// =========================================================== //
#ifdef __USE_BTREE_CACHE__
int gxBtree::Insert(DatabaseKeyB &key, DatabaseKeyB &compare_key, 
		    BtreeCache *btree_cache)
// B-tree cached insertion. Returns true if successful or false 
// if the key could not be inserted.
{
  if(InsertDatabaseKey(key, compare_key, btree_cache) != gxDBASE_NO_ERROR) 
    return 0;

  return 1;
}

gxDatabaseError gxBtree::InsertDatabaseKey(DatabaseKeyB &key,
					   DatabaseKeyB &compare_key, 
					   BtreeCache *btree_cache)
// Cached non-recursive cached insertion function used to insert a key 
// into the Btree. The "btree_cache" must be a previously allocated and
// connected B-tree cache object. Returns zero if successful or a non-zero 
// value to indicate a failure. Cached insertions are used for to increase
// insertion times for batch inserts. NOTE: You must flush or disconnect
// the cache before calling any non-cached B-tree functions. This will
// ensure that in the memory copy and the disk copy stay in sync. 
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  BtreeKeyLocation_t key_location;
  BtreeCacheNode curr_node(btree_cache);
  key.right_child = (FAU_t)0;
  curr_node = btree_header.root;
  if((FAU_t)curr_node == (FAU_t)0) { 
    // Could not acquire a cache bucket
    f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return f->GetDatabaseError();
#endif
  }

  BtreeStack node_stack;
  int num_splits = 0;

  // Walk to the leaf where the key will be inserted and look duplicate keys
  while((FAU_t)curr_node != (FAU_t)0) {
    if(!node_stack.Push((FAU_t)curr_node)) {
      f->SetDatabaseError(gxDBASE_MEM_ALLOC_ERROR);
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return f->GetDatabaseError();
#endif
    }

    if(curr_node->FindKeyLocation(key, compare_key, key_location) == 0) {
      // Found a duplicate key
      node_stack.Clear();
      f->SetDatabaseError(gxDBASE_DUPLICATE_ENTRY);
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return f->GetDatabaseError();
#endif
    }
    
    // Continue the search until a node with no children is found.
    // Bucket assignment errors will be checked at the top of the loop.
    curr_node = curr_node->GetBranch(key_location);
  }

  while(!node_stack.IsEmpty()) {
    curr_node = node_stack.Pop();
    if((FAU_t)curr_node == (FAU_t)0) { 
      // Could not acquire a cache bucket
      f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return f->GetDatabaseError();
#endif
    }
    
    if(num_splits > 0) {
      curr_node->FindKeyLocation(key, compare_key, key_location);
    }
    key_location++; // Move to the insertion key_location
    
    if(!curr_node->IsFull()) { // Add the key if the node is not full
      curr_node->InsertDatabaseKey(key, key_location);
      // Signal to the cache that this node has been modified
      curr_node->SetDirty(); 

      node_stack.Clear();
      btree_header.n_keys++;
      header_in_sync = 0;
      return gxDBASE_NO_ERROR;
    }
    else { // Split the node
      num_splits++;
      BtreeCacheNode right_node(btree_cache);
      
      // Check for block allocation errors
      if(!right_node.Alloc(curr_node->SizeOfBtreeNode())) {
	node_stack.Clear();
	return f->GetDatabaseError();
      }
      btree_header.n_nodes++;
      header_in_sync = 0;
      
      BtreeKeyLocation_t split_location = btree_header.node_order/2;
      if(key_location < split_location) split_location--;

      curr_node->SplitNode(*right_node->GetNode(), split_location);

      // Signal to the cache that this node has been modified
      curr_node->SetDirty(); 
      
      if(key_location > split_location) { // Insert key in the right node
	right_node->LoadKey(compare_key, (BtreeKeyLocation_t)0);
	right_node->DeleteDatabaseKey((BtreeKeyLocation_t)0);
        right_node->InsertDatabaseKey(key, key_location-split_location-1);
	
	// Pass the middle key up the tree and insert into its parent node
	memmove(key.db_key, compare_key.db_key, key.KeySize()); 
	key.right_child = compare_key.right_child;
      }
      else if(key_location < split_location) { // Insert key in the left node
        curr_node->InsertDatabaseKey(key, key_location);
	right_node->LoadKey(compare_key, (BtreeKeyLocation_t)0);
	
	// Pass the middle key up the tree and insert into its parent node
	memmove(key.db_key, compare_key.db_key, key.KeySize()); 
	key.right_child = compare_key.right_child;
	right_node->DeleteDatabaseKey((BtreeKeyLocation_t)0);
      }
      
      right_node->left_child = key.right_child;
      key.right_child = (FAU_t)right_node;
      // Signal to the cache that this node has been modified
      right_node->SetDirty(); 

      if((FAU_t)curr_node == btree_header.root) { // Grow a new root
	node_stack.Clear();

	FAU_t old_root = btree_header.root;
	BtreeCacheNode new_root_node(btree_cache);

	// Check for block allocation errors
	if(!new_root_node.Alloc(curr_node->SizeOfBtreeNode())) {
	  return f->GetDatabaseError();
	}

	btree_header.n_nodes++; 
	btree_header.btree_height++;
	btree_header.root = (FAU_t)new_root_node;
	header_in_sync = 0;
	new_root_node->left_child = old_root;
	new_root_node->InsertDatabaseKey(key, (BtreeKeyLocation_t)0);
	// Signal to the cache that this node has been modified
	new_root_node->SetDirty(); 
	btree_header.n_keys++;
	header_in_sync = 0;
	return gxDBASE_NO_ERROR;
      }
    }
  }   

  // The insertion was not completed
  node_stack.Clear();
  f->SetDatabaseError(gxDBASE_INSERTION_ERROR);
#ifdef __CPP_EXCEPTIONS__
  throw gxCDatabaseException();
#endif
  return f->GetDatabaseError();
}

int gxBtree::Find(DatabaseKeyB &key, DatabaseKeyB &compare_key, 
		  BtreeCache *btree_cache) 
// Cached find function used to find the specified key. Returns 
// true if successful or false if the key could not be found.
{
  BtreeCacheNode curr_node(btree_cache);
  return Find(key, compare_key, curr_node);
}

int gxBtree::FindFirst(DatabaseKeyB &key, BtreeCache *btree_cache)
// Cached find first function used to find the first key in the Btree. 
// Returns true if successful or false if the key could not be found.
{
  BtreeCacheNode curr_node(btree_cache);
  return FindFirst(key, curr_node);
}

int gxBtree::FindNext(DatabaseKeyB &key, DatabaseKeyB &compare_key,
		      BtreeCache *btree_cache)
// Cached find next function used to find the next key after the 
// specified key, passing back the key in the "key" variable. 
// Returns true if successful or false if the key could not be 
// found.
{
  BtreeCacheNode curr_node(btree_cache);
  return FindNext(key, compare_key, curr_node);
}

int gxBtree::FindPrev(DatabaseKeyB &key, DatabaseKeyB &compare_key,
		      BtreeCache *btree_cache)
// Cached find previous function used to find the previous key before 
// the specified key, passing back the key in the "key" variable. 
// Returns true if successful or false if the key could not be 
// found.
{
  BtreeCacheNode curr_node(btree_cache);
  return FindPrev(key, compare_key, curr_node);
}

int gxBtree::FindLast(DatabaseKeyB &key, BtreeCache *btree_cache)
// Cached find last function used to find the last key in the Btree. 
// Returns true if successful or false if the key could not be found.
{
  BtreeCacheNode curr_node(btree_cache);
  return FindLast(key, curr_node);
}

int gxBtree::PartialFind(DatabaseKeyB &key, DatabaseKeyB &compare_key, 
			 BtreeCache *btree_cache)
// Cache partial lookup function used to find a key equal to or greater then 
// the specified key even if the specified key does not exist in the B-tree. 
// Loads the exact or partial match and returns true if successful. 
// Returns false if the tree is empty or a partial match greater then 
// the specified key cannot be found. This version of the partial function 
// is used with the optimized sort order search function. When the exact or
// partial match is found the node containing the key is passed back to the
// caller in the "curr_node" variable.
{
  BtreeCacheNode curr_node(btree_cache);
  return PartialFind(key, compare_key, curr_node);
}

int gxBtree::Find(DatabaseKeyB &key, DatabaseKeyB &compare_key, 
		  BtreeCacheNode &curr_node) 
// Cached find function used to find the specified key. Returns 
// true if successful or false if the key could not be found.
// This version of the find function is optimized for sort order 
// searches. The "curr_node" variable is used to store the node 
// where the specified key was found. If the specified key is 
// found in the node this function returns true. If the key is 
// not less then or greater then all the keys in the "curr_node" 
// variable and a child pointer is found the child pointer is 
// used as the starting point for this search. If the key is 
// less then or greater then all the key in the "curr_node" 
// variable or there is no child pointer the root node is used 
// as the starting point for this search. NOTE: To disable the 
// optimized search set the "curr_node" key count to zero before 
// calling this function.
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return 0;
#endif
  }

  // Test this cached node to before using it.
  if((FAU_t)curr_node == (FAU_t)0) { 
    // This cached node is not bound to a bucket so
    // start the search at the root node.
    curr_node = btree_header.root;
    if((FAU_t)curr_node == (FAU_t)0) { 
      // Could not acquire a cache bucket
      f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return 0;
#endif
    }
  }

  // Search the current node for the specified key
  BtreeKeyLocation_t key_location;

  if(curr_node->key_count > (BtreeKeyLocation_t)0) {
    if(curr_node->FindKeyLocation(key, compare_key, key_location) == 0) {
      // Load the key data in the event that this is a partial look up
      curr_node->LoadKey(key, key_location); 
      return 1; // Found a matching node
    }
    else {
      // The key was not found so check this node for child pointer
      FAU_t curr_address = (FAU_t)0;
      curr_address = curr_node->GetBranch(key_location);
      if(curr_address != (FAU_t)0) { // This is leaf node
	if(key_location < (BtreeKeyLocation_t)0) { 
	  // This key is less then all the entries in this node so do not
	  // follow this child link.
	  curr_node = btree_header.root;
	}
	curr_node->LoadKey(compare_key, key_location);
#ifndef __USE_SINGLE_COMPARE__
	if(key > compare_key) {
	  // This key is greater then all the entries in this node so do not
	  // follow this child link.
	  curr_node = btree_header.root;
	}
#else
	if(key.CompareKey(compare_key) > 0) {
	  // This key is greater then all the entries in this node so do not
	  // follow this child link.
	  curr_node = btree_header.root;
	}
#endif // __USE_SINGLE_COMPARE__
      }
      else {
	curr_node = btree_header.root;	
      }
    }
  }
  else {
    curr_node = btree_header.root;
  }

  if((FAU_t)curr_node == (FAU_t)0) { 
    // Could not acquire a cache bucket
    f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return 0;
#endif
  }
  
  while((FAU_t)curr_node != (FAU_t)0) {
    if(curr_node->FindKeyLocation(key, compare_key, key_location) == 0) {
      // Load the key data in the event that this is a partial look up
      curr_node->LoadKey(key, key_location); 
      return 1; // Found a matching node
    }

    // Continue the search until a node with no children is found.
    // Bucket assignment errors will be checked at the top of the loop.
    curr_node = curr_node->GetBranch(key_location);
  }
  
  f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);
  return 0;
}

int gxBtree::FindFirst(DatabaseKeyB &key, BtreeCacheNode &curr_node)
// Cached find first function used to find the first key in the Btree. 
// Returns true if successful or false if the key could not be found.
{
  if(IsEmpty()) return 0;

  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return 0;
#endif
  }

  curr_node = btree_header.root;
  if((FAU_t)curr_node == (FAU_t)0) { 
    // Could not acquire a cache bucket
    f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return 0;
#endif
  }

  while(curr_node->left_child != (FAU_t)0) {
    curr_node = curr_node->left_child;
    if((FAU_t)curr_node == (FAU_t)0) { 
      // Could not acquire a cache bucket
      f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return 0;
#endif
    }
  }
  if(curr_node->key_count <= (BtreeKeyCount)0) {
    f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);
    return 0;
  }
  curr_node->LoadKey(key, (BtreeKeyLocation_t)0);
  return 1;
}

int gxBtree::FindNext(DatabaseKeyB &key, DatabaseKeyB &compare_key,
		      BtreeCacheNode &curr_node)
// Cached find next function used to find the next key after the 
// specified key, passing back the key in the "key" variable. 
// Returns true if successful or false if the key could not be 
// found. This version of the find next function is optimized  
// for sort order searches. The "curr_node" variable is used to 
// store the node where the specified key was found. If the 
// specified key is found this function will search for the next 
// node following the current node child pointers. If the next sort 
// order node is found this function returns true. If the next sort 
// order node is not found the root node is used as the starting 
// point for this search. NOTE: To disable the optimized search set
// the  "curr_node" key count to zero before calling this function.
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return 0;
#endif
  }

  // Test this cached node to before using it.
  if((FAU_t)curr_node == (FAU_t)0) { 
    // This cached node is not bound to a bucket so
    // start the search at the root node.
    curr_node = btree_header.root;
    if((FAU_t)curr_node == (FAU_t)0) { 
      // Could not acquire a cache bucket
      f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return 0;
#endif
    }
  }

  // Search the current node for the specified key
  BtreeKeyLocation_t key_location;
  if(curr_node->key_count > (BtreeKeyLocation_t)0) {
    if(curr_node->FindKeyLocation(key, compare_key, key_location) == 0) {
      if(curr_node->GetBranch(key_location) != (FAU_t)0) {
        curr_node = curr_node->GetBranch(key_location);
	if((FAU_t)curr_node == (FAU_t)0) { 
	  // Could not acquire a cache bucket
	  f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
	  throw gxCDatabaseException();
#else
	  return 0;
#endif
	}
	while(curr_node->left_child != (FAU_t)0) { // Get left most node
          curr_node = curr_node->left_child;
	  if((FAU_t)curr_node == (FAU_t)0) { 
	    // Could not acquire a cache bucket
	    f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
	    throw gxCDatabaseException();
#else
	    return 0;
#endif
	  }
	}
        if(curr_node->key_count <= (BtreeKeyCount_t)0) {
	  f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);
	  return 0;
	}
        curr_node->LoadKey(key, (BtreeKeyLocation_t)0);
	return 1;
      }
      else {
        if(key_location < curr_node->key_count-1) {
          curr_node->LoadKey(key, key_location+1);
	  return 1;
	}
      }
    }
  }

  // Did not find the key or next key in the current node so start searching
  // from the root node.
  curr_node = btree_header.root; 
  if((FAU_t)curr_node == (FAU_t)0) { 
    // Could not acquire a cache bucket
    f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return 0;
#endif
  }

  int rv;
  BtreeKeyLocation_t sav_location = (BtreeKeyLocation_t)0;
  BtreeCacheNode parent_node(curr_node);
   
  while((FAU_t)curr_node != (FAU_t)0) { 
    rv = curr_node->FindKeyLocation(key, compare_key, key_location);
    if(rv == 0) { // Found matching key, search for the next key in sort order
      if(curr_node->GetBranch(key_location) != (FAU_t)0) {
        curr_node = curr_node->GetBranch(key_location);
	  if((FAU_t)curr_node == (FAU_t)0) { 
	    // Could not acquire a cache bucket
	    f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
	    throw gxCDatabaseException();
#else
	    return 0;
#endif
	  }
	while(curr_node->left_child != (FAU_t)0) { // Get left most node
          curr_node = curr_node->left_child;
	  if((FAU_t)curr_node == (FAU_t)0) { 
	    // Could not acquire a cache bucket
	    f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
	    throw gxCDatabaseException();
#else
	    return 0;
#endif
	  }
	}
        if(curr_node->key_count <= (BtreeKeyCount_t)0) {
	  f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);
	  return 0;
	}
        curr_node->LoadKey(key, (BtreeKeyLocation_t)0);
	return 1;
      }
      else {
        if(key_location < curr_node->key_count-1) {
          curr_node->LoadKey(key, key_location+1);
	  return 1;
	}
	if((FAU_t)parent_node != (FAU_t)0) { // Go back to the parent node
          if(parent_node->key_count <= (BtreeKeyCount_t)0) {
	    f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);
	    return 0;
	  }
          if(++sav_location >= parent_node->key_count) {
	    f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);
	    return 0;
	  }
          parent_node->LoadKey(key, sav_location);
	  return 1;
	}
	return 0;
      }
    }
    if(key_location < curr_node->key_count-1) {
      parent_node = (FAU_t)curr_node;
      if((FAU_t)parent_node == (FAU_t)0) { 
	// Could not acquire a cache bucket
	f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
	throw gxCDatabaseException();
#else
	return 0;
#endif
      }
      sav_location = key_location;
    }

    // Continue the search until a node with no children is found.
    // Bucket assignment errors will be checked at the top of the loop.
    curr_node = curr_node->GetBranch(key_location); // No match, keep walking
  }

  f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);
  return 0;
}

int gxBtree::FindPrev(DatabaseKeyB &key, DatabaseKeyB &compare_key,
		      BtreeCacheNode &curr_node)
// Cached find previous function used to find the previous key before 
// the specified key, passing back the key in the "key" variable. 
// Returns true if successful or false if the key could not be 
// found. This version of the find previous function is optimized 
// for sort order searches. The "curr_node" variable is used to 
// store the node where the specified key was found. If the specified 
// key is found this function will search for the previous node 
// following the current node child pointers. If the previous sort 
// order node is found this function returns true. If the next sort 
// order node is not found the root node is used as the starting 
// point for this search. NOTE: To disable the optimized search set
// the  "curr_node" key count to zero before calling this function.
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return 0;
#endif
  }

  // Test this cached node to before using it.
  if((FAU_t)curr_node == (FAU_t)0) { 
    // This cached node is not bound to a bucket so
    // start the search at the root node.
    curr_node = btree_header.root;
    if((FAU_t)curr_node == (FAU_t)0) { 
      // Could not acquire a cache bucket
      f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return 0;
#endif
    }
  }

  // Search the current node for the specified key
  BtreeKeyLocation_t key_location;
  if(curr_node->key_count > (BtreeKeyLocation_t)0) {
    if(curr_node->FindKeyLocation(key, compare_key, key_location) == 0) {
      if(--key_location >= (BtreeKeyLocation_t)0) {
        while(curr_node->GetBranch(key_location) != (FAU_t)0) {
          curr_node = curr_node->GetBranch(key_location);
	  if((FAU_t)curr_node == (FAU_t)0) { 
	    // Could not acquire a cache bucket
	    f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
	    throw gxCDatabaseException();
#else
	    return 0;
#endif
	  }
          key_location = curr_node->key_count-1;
	}
        curr_node->LoadKey(key, key_location);
	return 1;
      }
      if(curr_node->left_child != (FAU_t)0) {
        curr_node = curr_node->left_child;
	if((FAU_t)curr_node == (FAU_t)0) { 
	  // Could not acquire a cache bucket
	  f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
	  throw gxCDatabaseException();
#else
	  return 0;
#endif
	}
        key_location = curr_node->key_count-1;
        while(curr_node->GetBranch(key_location) != (FAU_t)0) {
          curr_node = curr_node->GetBranch(key_location);
	  if((FAU_t)curr_node == (FAU_t)0) { 
	    // Could not acquire a cache bucket
	    f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
	    throw gxCDatabaseException();
#else
	    return 0;
#endif
	  }
          key_location = curr_node->key_count-1;
	}
        curr_node->LoadKey(key, key_location);
	return 1;
      }
    }
  }
  
  // Did not find the key or previous key in the current node so start 
  // searching from the root node.
  curr_node = btree_header.root;
  if((FAU_t)curr_node == (FAU_t)0) { 
    // Could not acquire a cache bucket
    f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return 0;
#endif
  }

  BtreeCacheNode parent_node(curr_node);
  int rv;
  BtreeKeyLocation_t sav_location = -1;

  while((FAU_t)curr_node != (FAU_t)0) {
    rv = curr_node->FindKeyLocation(key, compare_key, key_location);
    if(rv == 0) {
      // Found matching key, search for the previous key in sort order 
      if (--key_location >= (BtreeKeyLocation_t)0) {
        while(curr_node->GetBranch(key_location) != (FAU_t)0) {
          curr_node = curr_node->GetBranch(key_location);
	  if((FAU_t)curr_node == (FAU_t)0) { 
	    // Could not acquire a cache bucket
	    f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
	    throw gxCDatabaseException();
#else
	    return 0;
#endif
	  }
          key_location = curr_node->key_count-1;
	}
        curr_node->LoadKey(key, key_location);
	return 1;
      }
      if(curr_node->left_child != (FAU_t)0) {
        curr_node = curr_node->left_child;
	if((FAU_t)curr_node == (FAU_t)0) { 
	  // Could not acquire a cache bucket
	  f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
	  throw gxCDatabaseException();
#else
	  return 0;
#endif
	}
        key_location = curr_node->key_count-1;
        while(curr_node->GetBranch(key_location) != (FAU_t)0) {
          curr_node = curr_node->GetBranch(key_location);
	  if((FAU_t)curr_node == (FAU_t)0) { 
	    // Could not acquire a cache bucket
	    f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
	    throw gxCDatabaseException();
#else
	    return 0;
#endif
	  }
          key_location = curr_node->key_count-1;
	}
        curr_node->LoadKey(key, key_location);
	return 1;
      }
      if((FAU_t)parent_node != (FAU_t)0) { // Go back to the parent node
        parent_node->LoadKey(key, sav_location);
	return 1;
      }
      f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);
      return 0;
    }
    else if (rv > 0) { // Save the parent node only if it branches to the right
      parent_node = (FAU_t)curr_node;
      if((FAU_t)parent_node == (FAU_t)0) { 
	// Could not acquire a cache bucket
	f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
	throw gxCDatabaseException();
#else
	return 0;
#endif
      }
      sav_location = key_location;
    }

    // Continue the search until a node with no children is found.
    // Bucket assignment errors will be checked at the top of the loop.
    curr_node = curr_node->GetBranch(key_location); // Keep walking 
  }

  f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);
  return 0;
}

int gxBtree::FindLast(DatabaseKeyB &key, BtreeCacheNode &curr_node)
// Cached find last function used to find the last key in the Btree. 
// Returns true if successful or false if the key could not be found.
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return 0;
#endif
  }

  if (IsEmpty()) return 0;

  curr_node = btree_header.root;
  
  if((FAU_t)curr_node == (FAU_t)0) { 
    // Could not acquire a cache bucket
    f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return 0;
#endif
  }

  while(curr_node->GetBranch((BtreeKeyLocation_t)curr_node->key_count-1)) {
    curr_node = \
      curr_node->GetBranch((BtreeKeyLocation_t)curr_node->key_count-1);
    if((FAU_t)curr_node == (FAU_t)0) { 
      // Could not acquire a cache bucket
      f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return 0;
#endif
    }
  }

  if(curr_node->key_count <= (BtreeKeyCount_t)0) return 0;
  curr_node->LoadKey(key, curr_node->key_count-1);
  return 1;
}

int gxBtree::PartialFind(DatabaseKeyB &key, DatabaseKeyB &compare_key, 
			 BtreeCacheNode &curr_node)
// Cache partial lookup function used to find a key equal to or greater then 
// the specified key even if the specified key does not exist in the B-tree. 
// Loads the exact or partial match and returns true if successful. 
// Returns false if the tree is empty or a partial match greater then 
// the specified key cannot be found. This version of the partial function 
// is used with the optimized sort order search function. When the exact or
// partial match is found the node containing the key is passed back to the
// caller in the "curr_node" variable.
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return 0;
#endif
  }
  
  curr_node = btree_header.root;
  if((FAU_t)curr_node == (FAU_t)0) { 
    // Could not acquire a cache bucket
    f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return 0;
#endif
  }

  BtreeCacheNode parent_node(curr_node);
  BtreeCacheNode search_node(curr_node);
  FAU_t search_address = (FAU_t)0;
  FAU_t parent_address = (FAU_t)0;
  BtreeKeyLocation_t key_location = (BtreeKeyLocation_t)0;
  BtreeKeyLocation_t sav_location = (BtreeKeyLocation_t)0;
  
  while((FAU_t)curr_node != (FAU_t)0) {
    if(curr_node->FindKeyLocation(key, compare_key, key_location) == 0) {
      // Found an exact match, so load the key and return 
      curr_node->LoadKey(key, key_location); 
      return 1; 
    }
    
    // Record this node's parent address to allow us to point to the  
    // node in order if the search fails at a leaf node->
    if(key_location < curr_node->key_count-1) {
      parent_address = (FAU_t)curr_node;
      sav_location = key_location;
    }
    
    // Record the address where the search failed
    search_address = (FAU_t)curr_node; 
    
    // Continue the search until a node with no children is found.
    // Bucket assignment errors will be checked at the top of the loop.
    curr_node = curr_node->GetBranch(key_location);
  }
  
  search_node = search_address;
  if((FAU_t)search_node == (FAU_t)0) { 
    // Could not acquire a cache bucket
    f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return 0;
#endif
  }
  
  // Search for the next key where the search for an exact match failed.
  if(search_node->GetBranch(key_location) != (FAU_t)0) {
    search_node = search_node->GetBranch(key_location);
    if((FAU_t)search_node == (FAU_t)0) { 
      // Could not acquire a cache bucket
      f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return 0;
#endif
    }
    while(search_node->left_child != (FAU_t)0) { // Get left most node
      search_node = search_node->left_child;
      if((FAU_t)search_node == (FAU_t)0) { 
	// Could not acquire a cache bucket
	f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
	throw gxCDatabaseException();
#else
	return 0;
#endif
      }
    }
    if(search_node->key_count <= (BtreeKeyCount_t)0) {
      f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);
      return 0;
    }
    search_node->LoadKey(key, (BtreeKeyLocation_t)0);
    return 1;
  }
  else {
    if(key_location < search_node->key_count-1) {
      search_node->LoadKey(key, key_location+1);
      return 1;
    }
    if(parent_address != (FAU_t)0) { // Go back to the parent node
      parent_node = parent_address;
      if((FAU_t)parent_node == (FAU_t)0) { 
	// Could not acquire a cache bucket
	f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
	throw gxCDatabaseException();
#else
	return 0;
#endif
      }
      if(parent_node->key_count <= (BtreeKeyCount_t)0) {
	f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);
	return 0;
      }
      if(++sav_location >= parent_node->key_count) {
	f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);
	return 0;
      }
      parent_node->LoadKey(key, sav_location);
      return 1;
    }

    // Could not make a partial match
    f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);
    return 0;
  }
  
  // PC-lint 05/02/2005: Ingore PC-lint unreachable code warning
  // Some compilers require a return value even if code is not reached
  f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);
  return 0;
}

int gxBtree::Delete(DatabaseKeyB &key, DatabaseKeyB &compare_key, 
		    BtreeCache *btree_cache)
// Cached delete function used to delete the specified key and balance 
// the B-tree following the deletion. Returns true if successful or 
// false if the key could not be deleted.

{
  if(DeleteDatabaseKey(key, compare_key, btree_cache) != gxDBASE_NO_ERROR) 
    return 0;

  return 1;
}

gxDatabaseError gxBtree::DeleteDatabaseKey(DatabaseKeyB &key,
					   DatabaseKeyB &compare_key,
					   BtreeCache *btree_cache)
// Cached non-recursive balanced B-tree delete function used to ensure
// that all nodes, with the exception of the root node, are at least 
// half full following a deletion. Returns zero if successful or a
// non-zero value to indicate a failure.
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  BtreeKeyLocation_t key_location;
  BtreeCacheNode curr_node(btree_cache);
  FAU_t parent_address = (FAU_t)0;
  FAU_t leaf_address = (FAU_t)0;
  BtreeCacheNode parent_node(btree_cache);
  BtreeCacheNode sibling_node(btree_cache);
  BtreeNode *leaf_node;

  BtreeStack node_stack;  
  int found_key = 0;
  key.right_child = (FAU_t)0;
  curr_node = btree_header.root;
  if((FAU_t)curr_node == (FAU_t)0) { 
    // Could not acquire a cache bucket
    f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return f->GetDatabaseError();
#endif
  }

  if(!node_stack.Push(btree_header.root)) {
    node_stack.Clear();
    f->SetDatabaseError(gxDBASE_MEM_ALLOC_ERROR);
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return f->GetDatabaseError();
#endif
  }
  
  // Search the B-tree for the specified key
  while((FAU_t)curr_node != (FAU_t)0) {
    if(curr_node->FindKeyLocation(key, compare_key, key_location) == 0) {
      // Do not check for error here
      sibling_node = curr_node->GetBranch(key_location);  
      found_key = 1;
      break; // Found the specified key
    }
    
    // Push parent and grandparent addresses for rotations
    // that must be propogated up the tree
    if(!node_stack.Push((FAU_t)curr_node)) {
      node_stack.Clear();
      f->SetDatabaseError(gxDBASE_MEM_ALLOC_ERROR);
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return f->GetDatabaseError();
#endif
    }
    
    // Continue the searching for the key.
    // Bucket assignment errors will be checked at the top of the loop.
    curr_node = curr_node->GetBranch(key_location);  
  }

  if(!found_key) { // The key was not found
    f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return f->GetDatabaseError();
#endif    
  }

  if((FAU_t)sibling_node != (FAU_t)0) { // This is a parent node
    // Push the siblings parent node
    if(!node_stack.Push((FAU_t)curr_node)) {
      node_stack.Clear();
      f->SetDatabaseError(gxDBASE_MEM_ALLOC_ERROR);
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return f->GetDatabaseError();
#endif
    }
    
    // Follow the siblings to the parent node's left most child
    while(sibling_node->left_child != (FAU_t)0) {

      // Push all parent and grandparent nodes
      if(!node_stack.Push((FAU_t)sibling_node)) {
	node_stack.Clear();
	f->SetDatabaseError(gxDBASE_MEM_ALLOC_ERROR);
#ifdef __CPP_EXCEPTIONS__
	throw gxCDatabaseException();
#else
	return f->GetDatabaseError();
#endif
      }
      sibling_node = sibling_node->left_child;
    }
    
    // Take a the first key from the sibling and overwite the key
    // marked for deletion in parent node
    sibling_node->LoadKey(key, (BtreeKeyLocation_t)0); // Promote this key
    sibling_node->DeleteDatabaseKey((BtreeKeyLocation_t)0);
    key.right_child = compare_key.right_child; 
    curr_node->OverWriteDatabaseKey(key, key_location);

    // Signal to the cache that this node has been modified
    curr_node->SetDirty(); 
    sibling_node->SetDirty(); 

    btree_header.n_keys--;
    header_in_sync = 0;
    parent_address = node_stack.Pop();
    leaf_node = (BtreeNode *)sibling_node.GetBucket();
    leaf_address = (FAU_t)sibling_node;
  }
  else { // This is a leaf (node with no children)
    curr_node->DeleteDatabaseKey(key_location);

    // Signal to the cache that this node has been modified
    curr_node->SetDirty(); 

    btree_header.n_keys--;
    header_in_sync = 0;
    parent_address = node_stack.Pop();
    leaf_node = (BtreeNode *)curr_node.GetBucket();
    leaf_address = (FAU_t)curr_node;
  }

  // Insert balance if the node does not contain the minimum number of entries
  if((leaf_node->HasFew()) && (leaf_address != btree_header.root)) {
    parent_node = parent_address;
    if((FAU_t)parent_node == (FAU_t)0) { 
      // Could not acquire a cache bucket
      node_stack.Clear();
      f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return f->GetDatabaseError();
#endif
    }      
    parent_node->FindKeyLocation(key, compare_key, key_location);
    if(InsertBalance(parent_node, key_location, compare_key, btree_cache) !=
       gxDBASE_NO_ERROR) {
      node_stack.Clear();
      return f->GetDatabaseError();
    }

    // Ensure that the node's parent contains the minimum number of entries
    // NOTE: The root node is allowed to have less then the minimum number
    // of entires and may be empty.
    if((parent_node->HasFew()) && 
       (parent_address != btree_header.root)) {
      while(!node_stack.IsEmpty()) {
	// Loop until all the grandparent nodes are balanced
	BtreeCacheNode grandparent_node(btree_cache);
	grandparent_node = node_stack.Pop();
	if((FAU_t)grandparent_node == (FAU_t)0) { 
	  // Could not acquire a cache bucket
	  node_stack.Clear();
	  f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
	  throw gxCDatabaseException();
#else
	  return f->GetDatabaseError();
#endif
	}
        grandparent_node->FindKeyLocation(key, compare_key, key_location);
        if(InsertBalance(grandparent_node, key_location, compare_key, 
			 btree_cache) != gxDBASE_NO_ERROR) {
	  node_stack.Clear();
	  return f->GetDatabaseError();
	}
	if((grandparent_node->HasFew()) && 
	   (grandparent_node->node_address != btree_header.root)) {
	  continue;
	}
	else {
	  break;
	}
      }
    }
  }
    
  // Clear the node stack
  node_stack.Clear();
  
  // Check the root node and collapse if empty
  if(CollapseRoot(btree_cache) != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }

  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxBtree::InsertBalance(BtreeCacheNode &parent_node, 
				       BtreeKeyLocation_t key_location,
				       DatabaseKeyB &compare_key,
				       BtreeCache *btree_cache)
// Cached insert balance function used to balance this parent's sibling 
// node after performing a deletion. Returns zero if successful or a 
// non-zero value to indicate a failure.
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  BtreeCacheNode right_node(btree_cache);
  BtreeCacheNode left_node(btree_cache);  

  if(key_location == (BtreeKeyLocation_t)-1) {
    // The parent node does not have a left child
    right_node = parent_node->GetBranch((BtreeKeyLocation_t)0);
    if((FAU_t)right_node == (FAU_t)0) { 
      // Could not acquire a cache bucket
      f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return f->GetDatabaseError();
#endif
    }

    if(right_node->HasMany()) { // Take a key from the right child
      if(InsertLeftRotation(parent_node, (BtreeKeyLocation_t)0,
			    compare_key, btree_cache) != gxDBASE_NO_ERROR) {
	return f->GetDatabaseError();
      } 
    }
    else { 
      if(Merge(parent_node, (BtreeKeyLocation_t)0, compare_key,
	       btree_cache) != gxDBASE_NO_ERROR) {
	return f->GetDatabaseError();
      }
    }
  }
  else if(key_location == parent_node->HighestKey()) {
    // The parent node does not have a right child
    left_node = parent_node->GetBranch(key_location-1);
    if((FAU_t)left_node == (FAU_t)0) { 
      // Could not acquire a cache bucket
      f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return f->GetDatabaseError();
#endif
    }
    if(left_node->HasMany()) { // Take a key from the left child
      if(InsertRightRotation(parent_node, key_location, compare_key,
			     btree_cache) != gxDBASE_NO_ERROR) {
	return f->GetDatabaseError();
      }
    }
    else {
      if(Merge(parent_node, key_location, compare_key, btree_cache) 
	 != gxDBASE_NO_ERROR) {
	return f->GetDatabaseError();
      }
    }
  }
  else {
    // The parent node has both left and right siblings
    left_node = parent_node->GetBranch(key_location-1);
    if((FAU_t)left_node == (FAU_t)0) { 
      // Could not acquire a cache bucket
      f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return f->GetDatabaseError();
#endif
    }
    
    if(left_node->HasMany()) { // Take a key from the left child
      if(InsertRightRotation(parent_node, key_location, compare_key,
			     btree_cache) != gxDBASE_NO_ERROR) {
	return f->GetDatabaseError();
      }
    }
    else {
      right_node = parent_node->GetBranch(key_location+1);
      if((FAU_t)right_node == (FAU_t)0) { 
	// Could not acquire a cache bucket
	f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
	throw gxCDatabaseException();
#else
	return f->GetDatabaseError();
#endif
      }
      if(right_node->HasMany()) { // Take a key from the right child
        if(InsertLeftRotation(parent_node, key_location+1, compare_key,
			      btree_cache) != gxDBASE_NO_ERROR) {
	  return f->GetDatabaseError();
	}
      }
      else {
        if(Merge(parent_node, key_location, compare_key, btree_cache) 
	   != gxDBASE_NO_ERROR) {
	  return f->GetDatabaseError();
	}
      }
    }
  }
  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxBtree::InsertRightRotation(BtreeCacheNode &parent_node, 
					     BtreeKeyLocation_t key_location,
					     DatabaseKeyB &compare_key,
					     BtreeCache *btree_cache)
// Cached right rotation function used by the gxBtree::InsertBalance() 
// function to perform a right rotation. Returns zero if successful 
// or a non-zero value to indicate a failure.
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  BtreeCacheNode right_node(btree_cache);
  BtreeCacheNode left_node(btree_cache);

  right_node = parent_node->GetBranch(key_location);
  if((FAU_t)right_node == (FAU_t)0) { 
    // Could not acquire a cache bucket
    f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return f->GetDatabaseError();
#endif
  }
  
  left_node = parent_node->GetBranch(key_location-1);
  if((FAU_t)left_node == (FAU_t)0) { 
    // Could not acquire a cache bucket
    f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return f->GetDatabaseError();
#endif
  }

  // Move the key from the parent node to the right most location of the
  // right child. 
  parent_node->LoadKey(compare_key, key_location);
  compare_key.right_child = right_node->left_child;
  right_node->InsertDatabaseKey(compare_key, (BtreeKeyLocation_t)0);


  // Move the right most key of the left child into the parent node.
  BtreeKeyLocation_t highest_key = left_node->HighestKey();
  right_node->left_child = left_node->GetBranch(highest_key);
  left_node->LoadKey(compare_key, highest_key);
  compare_key.right_child = (FAU_t)right_node;
  parent_node->OverWriteDatabaseKey(compare_key, key_location);
  left_node->DeleteDatabaseKey(highest_key);

  // Signal to the cache that this node has been modified
  parent_node->SetDirty(); 
  right_node->SetDirty(); 
  left_node->SetDirty(); 

  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxBtree::InsertLeftRotation(BtreeCacheNode &parent_node,
					    BtreeKeyLocation_t key_location,
					    DatabaseKeyB &compare_key,
					    BtreeCache *btree_cache)
// Cached left rotation function used by the gxBtree::InsertBalance() 
// function to perform a left rotation. Returns zero if successful or 
// a non-zero value to indicate a failure.
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  BtreeCacheNode right_node(btree_cache);
  BtreeCacheNode left_node(btree_cache);

  right_node = parent_node->GetBranch(key_location);
  if((FAU_t)right_node == (FAU_t)0) { 
    // Could not acquire a cache bucket
    f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return f->GetDatabaseError();
#endif
  }
  left_node = parent_node->GetBranch(key_location-1);
  if((FAU_t)left_node == (FAU_t)0) { 
    // Could not acquire a cache bucket
    f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return f->GetDatabaseError();
#endif
  }

  // Move the key from the parent node to the left most location of the 
  // left child
  parent_node->LoadKey(compare_key, key_location);
  compare_key.right_child = right_node->left_child;
  left_node->AppendDatabaseKey(compare_key);

  // Move the move the right most key of the right child into the
  // parent node.
  right_node->left_child = right_node->GetBranch((BtreeKeyLocation_t)0);  
  right_node->LoadKey(compare_key, (BtreeKeyLocation_t)0);
  compare_key.right_child = (FAU_t)right_node;
  parent_node->OverWriteDatabaseKey(compare_key, key_location);
  right_node->DeleteDatabaseKey((BtreeKeyLocation_t)0);

  // Signal to the cache that this node has been modified
  parent_node->SetDirty(); 
  right_node->SetDirty(); 
  left_node->SetDirty(); 

  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxBtree::Merge(BtreeCacheNode &parent_node,
			       BtreeKeyLocation_t key_location,
			       DatabaseKeyB &compare_key,
			       BtreeCache *btree_cache)
// Cached merge function. Internal function used by the 
// gxBtree::InsertBalance() function to merge the parents 
// left and right siblings and remove the right sibling. 
// Returns zero if successful or a non-zero value to 
// indicate a failure.
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  BtreeCacheNode right_node(btree_cache);
  BtreeCacheNode left_node(btree_cache);

  right_node = parent_node->GetBranch(key_location);
  if((FAU_t)right_node == (FAU_t)0) { 
    // Could not acquire a cache bucket
    f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return f->GetDatabaseError();
#endif
  }
  left_node = parent_node->GetBranch(key_location-1);
  if((FAU_t)left_node == (FAU_t)0) { 
    // Could not acquire a cache bucket
    f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return f->GetDatabaseError();
#endif
  }

  // Move the key from the parent node to the left most location of the 
  // left child.
  parent_node->LoadKey(compare_key, key_location);
  compare_key.right_child = right_node->left_child;
  left_node->AppendDatabaseKey(compare_key);
  parent_node->DeleteDatabaseKey(key_location);

  // Merge the left and right siblings
  BtreeNode *right_child = (BtreeNode *)right_node.GetBucket();
  left_node->MergeNode(*(right_child));
  if(!right_node.Delete()) { // Remove the right child
    return f->GetDatabaseError();
  }
  btree_header.n_nodes--;
  header_in_sync = 0;
  
  // Signal to the cache that this node has been modified
  parent_node->SetDirty(); 
  left_node->SetDirty(); 

  return gxDBASE_NO_ERROR;
}

int gxBtree::FastDelete(DatabaseKeyB &key, DatabaseKeyB &compare_key,
			BtreeCache *btree_cache)
// Cached fast delete function used to delete the specified key without 
// balancing the tree following the deletion. Returns true if successful 
// or false if the key could not be deleted.
{
  if(FastDeleteDatabaseKey(key, compare_key, btree_cache) 
     != gxDBASE_NO_ERROR) return 0;

  return 1;
}

gxDatabaseError gxBtree::FastDeleteDatabaseKey(DatabaseKeyB &key,
					       DatabaseKeyB &compare_key,
					       BtreeCache *btree_cache)
// Cached fast unbalanced deletion method implemented specifically to 
// support concurrent database operations that are effected by the 
// excessive node splitting and merging that occurs during tree rotations.  
// Returns zero if successful or a non-zero value to indicate a failure. 
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  BtreeKeyLocation_t key_location;
  BtreeStack node_stack;
  BtreeCacheNode parent_node(btree_cache);
  BtreeCacheNode sibling_node(btree_cache);
  
  int found_key = 0;
  int num_siblings = 0;
  key.right_child = (FAU_t)0;
  parent_node = btree_header.root;
  if((FAU_t)parent_node == (FAU_t)0) { 
    // Could not acquire a cache bucket
    f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return f->GetDatabaseError();
#endif
  }

  while((FAU_t)parent_node != (FAU_t)0) {
    if(parent_node->FindKeyLocation(key, compare_key, key_location) == 0) {
      // Do not check for bucket errors here
      sibling_node = (FAU_t)parent_node->GetBranch(key_location);  
      found_key = 1;
      break; // Found the specified key
    }
    
    // Continue the searching for the spcified key
    parent_node = parent_node->GetBranch(key_location);  
  }

  if(!found_key) {
    f->SetDatabaseError(gxDBASE_ENTRY_NOT_FOUND);
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return f->GetDatabaseError();
#endif    
  }

  if((FAU_t)sibling_node == (FAU_t)0) { // This is leaf node 
    parent_node->DeleteDatabaseKey(key_location);
    parent_node->SetDirty(); // Signal to the cache to write this node
    btree_header.n_keys--;
    header_in_sync = 0;
    
    // Check the root node and collapse if empty
    if(CollapseRoot(btree_cache) != gxDBASE_NO_ERROR) 
      return f->GetDatabaseError();

    return gxDBASE_NO_ERROR;
  }
  else { // This is a parent node
    num_siblings++;
    if(!node_stack.Push((FAU_t)sibling_node)) {
      f->SetDatabaseError(gxDBASE_MEM_ALLOC_ERROR);
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return f->GetDatabaseError();
#endif
    }
    
    sibling_node = (FAU_t)sibling_node;
    if((FAU_t)sibling_node == (FAU_t)0) { 
      // Could not acquire a cache bucket
      f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return f->GetDatabaseError();
#endif
    }
    
    // Follow the siblings to the parent node's left most child
    while(sibling_node->left_child != (FAU_t)0) {
      sibling_node = sibling_node->left_child;
      if((FAU_t)sibling_node == (FAU_t)0) { 
	// Could not acquire a cache bucket
	node_stack.Clear();
	f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
	throw gxCDatabaseException();
#else
	return f->GetDatabaseError();
#endif
      }
      if(!node_stack.Push((FAU_t)sibling_node)) {
	node_stack.Clear();
	f->SetDatabaseError(gxDBASE_MEM_ALLOC_ERROR);
#ifdef __CPP_EXCEPTIONS__
	throw gxCDatabaseException();
#else
	return f->GetDatabaseError();
#endif
      }
      num_siblings++;
    }
    
    // Skip over the first entry
    BtreeCacheNode sibling_address(btree_cache);
    sibling_address = node_stack.Pop();

    // This sibling is empty due to a prior deletion
    if(sibling_node->IsEmpty()) {

      // Remove the empty node
      if(!sibling_address.Delete()) {
	node_stack.Clear();
	return f->GetDatabaseError();
      }

      btree_header.n_nodes--;
      header_in_sync = 0;
      num_siblings--;

      // Continue deleting the empty siblings
      while(!node_stack.IsEmpty()) {
	sibling_node = node_stack.Pop();
	if((FAU_t)sibling_node == (FAU_t)0) { 
	  // Could not acquire a cache bucket
	  node_stack.Clear();
	  f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
	  throw gxCDatabaseException();
#else
	  return f->GetDatabaseError();
#endif
	}
	if(!sibling_node->IsEmpty()) break;
	
	if(!sibling_node.Delete()) {
	  node_stack.Clear();
	  return f->GetDatabaseError();
	}
	btree_header.n_nodes--;
	header_in_sync = 0;
	num_siblings--;
      }
    } 

    if(num_siblings == 0) { // Did not find any siblings containing keys
      parent_node->DeleteDatabaseKey(key_location);
      parent_node->SetDirty();
      btree_header.n_keys--;
      header_in_sync = 0;
      
      // The root node is node empty
      if((parent_node->IsEmpty()) && 
	 ((FAU_t)parent_node == btree_header.root)) {
	if(CollapseRoot(parent_node) != gxDBASE_NO_ERROR) {
	  node_stack.Clear();
	  return f->GetDatabaseError();
	}
      }
      return gxDBASE_NO_ERROR;
    }
  }

  // Take a the first key from the sibling and overwite the key
  // marked for deletion in parent node
  sibling_node->LoadKey(key, (BtreeKeyLocation_t)0);
  sibling_node->left_child = key.right_child;
  sibling_node->DeleteDatabaseKey((BtreeKeyLocation_t)0);
  key.right_child = compare_key.right_child; 
  parent_node->OverWriteDatabaseKey(key, key_location);
  sibling_node->SetDirty();
  parent_node->SetDirty();

  btree_header.n_keys--;
  header_in_sync = 0;
  node_stack.Clear();

  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxBtree::CollapseRoot(BtreeCache *btree_cache)
// Cached collapse root function used to collapse the current 
// root node and shorten the tree. Returns zero if successful 
// or a non-zero value to indicate a failure.
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!f) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }
 
  BtreeCacheNode root_node(btree_cache);
  root_node = btree_header.root;
  if((FAU_t)root_node == (FAU_t)0) { 
    // Could not acquire a cache bucket
    f->SetDatabaseError(gxDBASE_CACHE_ERROR);
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return f->GetDatabaseError();
#endif
  }

  // Ensure that the root node is empty and the height is greater then one
  if((root_node->IsEmpty()) && (btree_header.btree_height > (BtreeSize_t)1)) {
    btree_header.root = root_node->left_child;
    btree_header.n_nodes--; 
    btree_header.btree_height--;
    header_in_sync = 0;
    if(!root_node.Delete()) {
	return f->GetDatabaseError();
    }
  }
 
  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxBtree::CollapseRoot(BtreeCacheNode &root_node)
// Cached collapse root function used to collapse the specified 
// root node after a read operation and shorten the tree. Returns 
// zero if successful or a non-zero value to indicate a failure.
{
  // Ensure that the root node is empty and the height is greater then one
  if((root_node->IsEmpty()) && (btree_header.btree_height > (BtreeSize_t)1))  {
    btree_header.root = root_node->left_child;
    btree_header.n_nodes--; 
    btree_header.btree_height--;
    header_in_sync = 0;
    if(!root_node.Delete()) {
      // PC-lint 05/02/2005: Possible use of null pointer
      if(f) {
	return f->GetDatabaseError();
      }
      else {
	return gxDBASE_NULL_PTR;
      }
    }
  }
  return gxDBASE_NO_ERROR;
}

int gxBtree::ChangeKey(DatabaseKeyB &key, DatabaseKeyB &new_key_vals,
		       DatabaseKeyB &compare_key, BtreeCache *btree_cache)
// Function used to change a fixed length key in the index file.
// Returns true if the change was changed or false if the key
// does not exist in the database or a file error occurred.
{
  BtreeCacheNode curr_node(btree_cache);
  return ChangeKey(key, new_key_vals, compare_key, curr_node);
}

int gxBtree::ChangeKey(DatabaseKeyB &key, DatabaseKeyB &new_key_vals,
		       DatabaseKeyB &compare_key, BtreeCacheNode &curr_node)
// Function used to change a fixed length key in the index file.
// Returns true if the change was changed or false if the key
// does not exist in the database or a file error occurred.
{
  if(!Find(key, compare_key, curr_node)) return 0;
  BtreeKeyLocation_t key_location;
  if(curr_node->FindKeyLocation(key, compare_key, key_location) == 0) {
    curr_node->OverWriteDatabaseKey(new_key_vals, key_location);
  }
  else {
    return 0;
  }

  // Signal to the cache that this node has been modified
  curr_node->SetDirty(); 

  return 1;
}


int gxBtree::ChangeOrAddKey(DatabaseKeyB &key, DatabaseKeyB &new_key_vals,
			    DatabaseKeyB &compare_key, BtreeCache *btree_cache)
{
  BtreeCacheNode curr_node(btree_cache);
  return ChangeOrAddKey(key, new_key_vals, compare_key, curr_node);

}

int gxBtree::ChangeOrAddKey(DatabaseKeyB &key, DatabaseKeyB &new_key_vals,
			    DatabaseKeyB &compare_key,
			    BtreeCacheNode &curr_node) 
{
  if(!Find(key, compare_key, curr_node)) {
    return Insert(key, compare_key, curr_node.GetCache());
  }
  BtreeKeyLocation_t key_location;
  if(curr_node->FindKeyLocation(key, compare_key, key_location) == 0) {
    curr_node->OverWriteDatabaseKey(new_key_vals, key_location);
  }
  else {
    return 0;
  }

  // Signal to the cache that this node has been modified
  curr_node->SetDirty(); 

  return 1;
}

#endif // __USE_BTREE_CACHE__
// =========================================================== //
// End - B-tree cache additions
// =========================================================== //

#ifdef __BCC32__
#pragma warn .8066
#pragma warn .8080
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
