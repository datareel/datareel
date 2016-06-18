// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: btnode.h
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

The BtreeNode class is used by the gxBtree class to insert, delete, 
and search for database keys in a node. Database keys are derived 
from the DatabaseKeyB class and are kept in sorted order within 
each B-tree node.

Changes:
==============================================================
02/08/2002: All B-tree node functions using non-persistent file 
address units have been modified to use FAU_t data types instead 
of FAU types. FAU_t types are native or built-in integer types 
and require no additional overhead to process. All persistent 
file address units still use the FAU integer type, which is a 
platform independent type allowing database files to be shared 
across multiple platforms.

04/10/2003: The BtreeNode::Copy() function is now thread safe 
and the BtreeNode copy constructor and assignment operator are 
now public functions.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_BTREE_NODE_HPP__
#define __GX_BTREE_NODE_HPP__

#include "gxdlcode.h"

#include "dbasekey.h"

class GXDLCODE_API BtreeNode
{
public:
  BtreeNode();
  BtreeNode(BtreeSize_t dbkey_size, BtreeNodeOrder_t order);
  ~BtreeNode();
  BtreeNode(const BtreeNode &ob) { Copy(ob); }
  BtreeNode& operator=(const BtreeNode &ob);

public: 
  void Copy(const BtreeNode &ob);
  void Release();

public:
  size_t SizeOfBtreeNode();
  int FindKeyLocation(const DatabaseKeyB &key, DatabaseKeyB &compare_key,
		      BtreeKeyLocation_t &key_location);
  void SplitNode(BtreeNode &node, BtreeKeyLocation_t split_location);
  void InsertDatabaseKey(DatabaseKeyB &key, BtreeKeyLocation_t key_location);
  void OverWriteDatabaseKey(DatabaseKeyB &key,
			    BtreeKeyLocation_t key_location);
  void DeleteDatabaseKey(BtreeKeyLocation_t key_location);
  FAU GetBranch(BtreeKeyLocation_t key_location);
  void LoadKey(DatabaseKeyB &key, BtreeKeyLocation_t key_location);
  void AppendDatabaseKey(DatabaseKeyB &key) { 
    InsertDatabaseKey(key, key_count); 
  }
  void MergeNode(BtreeNode &node);
  int IsEmpty() { return key_count <= (BtreeKeyCount_t)0; }
  int IsFull() { return key_count >= (BtreeKeyCount_t)node_order; }
  int HasFew() { return key_count < (BtreeKeyCount_t)((node_order)/2); }
  int HasMany() { return key_count > (BtreeKeyCount_t)((node_order)/2); }
  BtreeKeyLocation_t HighestKey() { return (BtreeKeyLocation_t)key_count-1; }
  
public: // Non-persistent data members used for internal processing
  BtreeNodeOrder_t node_order; // Btree order
  BtreeSize_t key_size;        // Size of key plus overhead
  FAU_t node_address;          // File address of this node
  
public: // Persistent data members
  BtreeKeyCount key_count; // Counts how many keys are in use
  FAU left_child;          // File pointer to this node's left child
  char *key_entries;       // Variable order of key entries
};

#endif // __GX_BTREE_NODE_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
