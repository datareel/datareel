// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxbtree.h
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

The gxBtree class is a disk-base B-tree used by various database 
applications for searching and sorting structured data and indexing 
associated objects or records stored in a data file. The gxBtree 
uses the 32/64-bit gxDatabase engine to perform all disk operations, 
supporting large files and proprietary file system interfaces. 

Changes:
==============================================================
08/08/2001: Added gxBtree::PartialFind() function used to find a 
key equal to or greater then the specified key even if the specified 
key does not exist in the B-tree. 

08/17/2001: Added the following optimized un-cached sort order search 
and positioning functions: gxBtree::FindLast(), gxBtree::FindFirst(),
gxBtree::PartialFind(), gxBtree::Find(), gxBtree::FindNext(), 
gxBtree::FindPrev().

08/17/2001: Added the following functions and data members used
for B-tree tuning statistics: gxBtree::ResetStats(), 
gxBtree::node_writes, gxBtree::node_reads, gxBtree::header_writes,
gxBtree::header_reads.

02/03/2002: Changed default arguments for all functions using the
"flush" and "test_tree" variables to false. The caller or derived 
class must perform file flushing and tree testing as required by 
the application.

02/08/2002: All gxBtree functions using non-persistent file 
address units have been modified to use FAU_t data types instead 
of FAU types. FAU_t types are native or built-in integer types 
and require no additional overhead to process. All persistent 
file address units still use the FAU integer type, which is a 
platform independent type allowing database files to be shared 
across multiple platforms.

03/04/2002: Reassigned the optional gxBtreeHeader::class_id 
member to be used as a counter for the number of B-trees per
index file.  

04/15/2002: Added the gxBtree::ChangeKey() function used to 
change specified keys that already exist in the B-tree index.

3/12/2003: Added cached versions of the B-tree insert, delete, 
find, and change functions. NOTE: The __USE_BTREE_CACHE__ 
preprocessor directive must be defined at compile time in order 
to use the B-tree cache.

03/31/2003: Modified the gxBtreeHeader default constructor, 
copy constructor, and assignment operator to set the 
gxBtreeHeader::n_keys variable.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_DATABASE_BTREE_HPP__
#define __GX_DATABASE_BTREE_HPP__

#include "gxdlcode.h"

#include "btnode.h"
#include "btstack.h"

#ifdef __USE_BTREE_CACHE__
#include "btcache.h"
#endif

// Btree header
struct GXDLCODE_API gxBtreeHeader
{ 
  gxBtreeHeader() {
    node_order = (BtreeNodeOrder_t)0;
    key_size = (BtreeSize_t)0;
    n_keys = (BtreeSize_t)0;
    n_nodes = (BtreeSize_t)0;
    btree_height = (BtreeSize_t)0;
    root = (FAU_t)0;
    num_trees = (BtreeSize_t)1;
  }
  ~gxBtreeHeader() { }
  gxBtreeHeader(const gxBtreeHeader &ob) {
    node_order = ob.node_order;
    key_size = ob.key_size;
    n_keys = ob.n_keys;
    n_nodes = ob.n_nodes;
    btree_height = ob.btree_height;
    root = ob.root;
    num_trees = ob.num_trees;
  }
  gxBtreeHeader &operator=(const gxBtreeHeader &ob) {
    node_order = ob.node_order;
    key_size = ob.key_size;
    n_keys = ob.n_keys;
    n_nodes = ob.n_nodes;
    btree_height = ob.btree_height;
    root = ob.root;
    num_trees = ob.num_trees;
    return *this;
  }

  BtreeNodeOrder node_order; // Btree node order
  BtreeSize key_size;        // Database key size
  BtreeSize n_keys;          // Total number of entry keys
  BtreeSize n_nodes;         // Total number of nodes
  BtreeSize btree_height;    // Height of this Btree
  FAU root;                  // Btree root address  
  BtreeSize num_trees;       // Used to record the number of B-trees per index
};

// Btree class
class GXDLCODE_API gxBtree
{  
public:
  gxBtree(DatabaseKeyB &key_type, BtreeNodeOrder_t order);
  virtual ~gxBtree();

public: // Btree database file functions
  gxDatabaseError Create(const char *fname, int num_trees = 1,
			 __SBYTE__ RevisionLetter = gxDatabaseRevisionLetter);
  gxDatabaseError Create(const char *fname, __SBYTE__ RevisionLetter, 
			 int num_trees = 1);
  gxDatabaseError Open(const char *fname, 
		       gxDatabaseAccessMode mode = gxDBASE_READWRITE);
  gxDatabaseError Open(const char *fname, FAU_t header_address,
		       gxDatabaseAccessMode mode = gxDBASE_READWRITE); 
  gxDatabaseError InitBtree(gxDatabase *fptr, int create, 
			    FAU_t header_address, int num_trees = 1,
			    int flushdb = 1);
  gxDatabaseError InitBtree(int create, FAU_t header_address, 
			    int num_trees = 1, int flushdb = 1);
  gxDatabaseError Close();
  gxDatabaseError Flush(int write_header = 1);
  gxDatabaseError ReadBtreeHeader();
  gxDatabaseError ReadBtreeHeader(gxBtreeHeader &hdr, FAU_t header_addres);
  gxDatabaseError ReadBtreeHeader(gxBtreeHeader &hdr) {
    return ReadBtreeHeader(hdr, btree_header_address);
  }
  gxDatabaseError WriteBtreeHeader();
  gxDatabaseError WriteBtreeHeader(gxBtreeHeader &hdr, FAU_t header_addres);
  gxDatabaseError WriteBtreeHeader(gxBtreeHeader &hdr) {
    return WriteBtreeHeader(hdr, btree_header_address);
  }
  gxDatabaseError ReadNode(BtreeNode &node, FAU_t
			   node_address = gxCurrAddress);
  gxDatabaseError WriteNode(const BtreeNode &node,
			    FAU_t node_address = gxCurrAddress);

public: // Synchronization functions
  void Release() {
    // The release function is used to release this object's file pointer  
    // without closing the file pointer or performing any other action. 
    // NOTE: The release function is used to release this file pointer when  
    // more then one object is referencing this file pointer. After calling   
    // the release function the object previously bound to this file pointer 
    // can safely be deleted without affecting any other objects bound to 
    // this file pointer. This allows the file to remain open until all 
    // threads accessing this file pointer have exited or released it.    
    f = 0;
  }

public: // Btree insert and delete functions
  int Insert(DatabaseKeyB &key, DatabaseKeyB &compare_key, int flushdb = 0);
  int Delete(DatabaseKeyB &key, DatabaseKeyB &compare_key, int flushdb = 0);
  int LazyDelete(DatabaseKeyB &key, DatabaseKeyB &compare_key, int flushdb = 0);
  int FastDelete(DatabaseKeyB &key, DatabaseKeyB &compare_key, int flushdb = 0);

public: // Additional database key functions
  int ChangeKey(DatabaseKeyB &key, DatabaseKeyB &new_key_vals,
		DatabaseKeyB &compare_key, int test_tree = 0);
  int ChangeKey(DatabaseKeyB &key, DatabaseKeyB &new_key_vals,
		DatabaseKeyB &compare_key, BtreeNode &curr_node, 
		int test_tree = 0);
  int ChangeOrAddKey(DatabaseKeyB &key, DatabaseKeyB &new_key_vals,
		     DatabaseKeyB &compare_key, int test_tree = 0);
  int ChangeOrAddKey(DatabaseKeyB &key, DatabaseKeyB &new_key_vals,
		     DatabaseKeyB &compare_key, BtreeNode &curr_node, 
		     int test_tree = 0);
 
public: // Btree search functions
  int Find(DatabaseKeyB &key, DatabaseKeyB &compare_key, int test_tree = 0);
  int FindFirst(DatabaseKeyB &key, int test_tree = 0);
  int FindNext(DatabaseKeyB &key, DatabaseKeyB &compare_key, 
	       int test_tree = 0);
  int FindPrev(DatabaseKeyB &key, DatabaseKeyB &compare_key,
	       int test_tree = 0);
  int FindLast(DatabaseKeyB &key, int test_tree = 0);
  int PartialFind(DatabaseKeyB &key, DatabaseKeyB &compare_key, 
		  int test_tree = 0);
  
public:
  // Optimized un-cached sort order search positioning functions
  int FindFirst(DatabaseKeyB &key, BtreeNode &curr_node, 
		int test_tree = 0);
  int FindLast(DatabaseKeyB &key, BtreeNode &curr_node, 
	       int test_tree = 0);
  int PartialFind(DatabaseKeyB &key, DatabaseKeyB &compare_key, 
		  BtreeNode &curr_node, int test_tree = 0);

  // Optimized un-cached sort order search functions
  int Find(DatabaseKeyB &key, DatabaseKeyB &compare_key, 
	   BtreeNode &curr_node, int test_tree = 0); 
  int FindNext(DatabaseKeyB &key, DatabaseKeyB &compare_key,
	       BtreeNode &curr_node, int test_tree = 0);
  int FindPrev(DatabaseKeyB &key, DatabaseKeyB &compare_key,
	       BtreeNode &curr_node, int test_tree = 0);
  
public:
  int TestTree(FAU_t header_address, int reinit);
  int TestTree(int reinit = 1) {
    return TestTree(btree_header_address, reinit);
  }
  gxDatabaseError ReConnect(int tree_number);
  gxDatabaseError ReInit(FAU_t header_address, int flushdb = 0);
  gxDatabaseError ReInit(int flushdb = 0) {
    return ReInit(btree_header_address, flushdb);
  }
  int IsEmpty() { return btree_header.n_keys == (BtreeSize_t)0; }
  int IsEmpty() const { return btree_header.n_keys == (BtreeSize_t)0; }
  size_t TotalNodeSize();
  int HeaderInSync() { return header_in_sync == 1; }
  int HeaderInSync() const { return header_in_sync == 1; }
  gxDatabase *gxDatabasePtr() { return f; }
  gxBtreeHeader *BtreeHeader() { return &btree_header; }
  FAU Root() { return btree_header.root; }
  FAU Root() const { return btree_header.root; }
  FAU HeaderAddress() { return btree_header_address; }
  FAU HeaderAddress() const { return btree_header_address; }
  BtreeSize_t NumKeys() { return btree_header.n_keys; }
  BtreeSize_t NumKeys() const { return btree_header.n_keys; }
  BtreeSize_t NumNodes() { return btree_header.n_nodes; }
  BtreeSize_t NumNodes() const { return btree_header.n_nodes; }
  BtreeSize_t KeySize() { return btree_header.key_size; }
  BtreeSize_t KeySize() const { return btree_header.key_size; }
  BtreeNodeOrder_t NodeOrder() { return btree_header.node_order; }
  BtreeNodeOrder_t NodeOrder() const { return btree_header.node_order; }
  BtreeSize_t BtreeHeight() { return btree_header.btree_height; }
  BtreeSize_t BtreeHeight() const { return btree_header.btree_height; }
  BtreeSize_t NumTrees() { return btree_header.num_trees; }
  BtreeSize_t NumTrees() const { return btree_header.num_trees; }

public:  // Database exception handling functions
  gxDatabaseError GetDatabaseError() { return f->GetDatabaseError(); }
  gxDatabaseError GetDatabaseError() const {  return f->GetDatabaseError(); }
  gxDatabaseError SetDatabaseError(gxDatabaseError err) {
    return f->SetDatabaseError(err);
  }
  gxDatabaseError ResetDatabaseError() {
    return f->SetDatabaseError(gxDBASE_NO_ERROR);
  }
  const char *DatabaseExceptionMessage();

protected: // Internal Insert, delete, and balancing functions
  gxDatabaseError GrowNewRoot(DatabaseKeyB &key);
  gxDatabaseError CollapseRoot();
  gxDatabaseError CollapseRoot(BtreeNode &root_node);
  gxDatabaseError InsertDatabaseKey(DatabaseKeyB &key,
				    DatabaseKeyB &compare_key);
  gxDatabaseError FastDeleteDatabaseKey(DatabaseKeyB &key,
					DatabaseKeyB &compare_key);
  gxDatabaseError DeleteDatabaseKey(DatabaseKeyB &key,
				    DatabaseKeyB &compare_key);
  gxDatabaseError InsertBalance(BtreeNode &parent_node,
				BtreeKeyLocation_t key_location,
				DatabaseKeyB &compare_key);
  gxDatabaseError InsertRightRotation(BtreeNode &parent_node,
				      BtreeKeyLocation_t key_location,
				      DatabaseKeyB &compare_key);
  gxDatabaseError InsertLeftRotation(BtreeNode &parent_node,
				     BtreeKeyLocation_t key_location,
				     DatabaseKeyB &compare_key);
  gxDatabaseError Merge(BtreeNode &parent_node,
			BtreeKeyLocation_t key_location,
			DatabaseKeyB &compare_key);

protected:
  gxDatabase *f;              // Database the Btree is connected to
  FAU_t btree_header_address; // File address of the Btree header
  gxBtreeHeader btree_header; // In memory copy of the Btree header
  
  // True if the in-memory copy of the header is in sync with the disk copy
  int header_in_sync; 

public: // B-tree tuning statistics
  void ResetStats() {
    node_reads = node_writes = header_reads = header_writes = 0;
  }
  unsigned node_writes;   // Counts the number of un-cached node writes
  unsigned node_reads;    // Counts the number of un-cached node reads
  unsigned header_writes; // Counts the number of B-tree header writes
  unsigned header_reads;  // Counts the number of B-tree header reads

// =========================================================== //
// Begin - B-tree cache additions
// =========================================================== //
#ifdef __USE_BTREE_CACHE__
public:
  int ChangeKey(DatabaseKeyB &key, DatabaseKeyB &new_key_vals,
		DatabaseKeyB &compare_key, BtreeCache *btree_cache);
  int ChangeKey(DatabaseKeyB &key, DatabaseKeyB &new_key_vals,
		DatabaseKeyB &compare_key, BtreeCacheNode &curr_node); 
  int ChangeOrAddKey(DatabaseKeyB &key, DatabaseKeyB &new_key_vals,
		     DatabaseKeyB &compare_key, BtreeCache *btree_cache);
  int ChangeOrAddKey(DatabaseKeyB &key, DatabaseKeyB &new_key_vals,
		     DatabaseKeyB &compare_key, BtreeCacheNode &curr_node);

public: // Cached insert, find, and delete functions
  int Insert(DatabaseKeyB &key, DatabaseKeyB &compare_key, 
	     BtreeCache *btree_cache);
  int Delete(DatabaseKeyB &key, DatabaseKeyB &compare_key, 
	     BtreeCache *btree_cache);
  int FastDelete(DatabaseKeyB &key, DatabaseKeyB &compare_key,
		 BtreeCache *btree_cache);
  int Find(DatabaseKeyB &key, DatabaseKeyB &compare_key, 
	   BtreeCache *btree_cache);
  int FindFirst(DatabaseKeyB &key, BtreeCache *btree_cache);
  int FindNext(DatabaseKeyB &key, DatabaseKeyB &compare_key,
	       BtreeCache *btree_cache);
  int FindPrev(DatabaseKeyB &key, DatabaseKeyB &compare_key,
	       BtreeCache *btree_cache);
  int FindLast(DatabaseKeyB &key, BtreeCache *btree_cache);
  int PartialFind(DatabaseKeyB &key, DatabaseKeyB &compare_key, 
		  BtreeCache *btree_cache);

  // Optimized cached sort order search positioning functions
  int FindFirst(DatabaseKeyB &key, BtreeCacheNode &curr_node);
  int FindLast(DatabaseKeyB &key, BtreeCacheNode &curr_node);
  int PartialFind(DatabaseKeyB &key, DatabaseKeyB &compare_key, 
		  BtreeCacheNode &curr_node);

  // Optimized cached sort order search functions
  int Find(DatabaseKeyB &key, DatabaseKeyB &compare_key, 
	   BtreeCacheNode &curr_node);
  int FindNext(DatabaseKeyB &key, DatabaseKeyB &compare_key,
	       BtreeCacheNode &curr_node);
  int FindPrev(DatabaseKeyB &key, DatabaseKeyB &compare_key,
	       BtreeCacheNode &curr_node);

protected:
  gxDatabaseError InsertDatabaseKey(DatabaseKeyB &key,
				    DatabaseKeyB &compare_key, 
				    BtreeCache *btree_cache);
  gxDatabaseError FastDeleteDatabaseKey(DatabaseKeyB &key,
					DatabaseKeyB &compare_key,
					BtreeCache *btree_cache);
  gxDatabaseError DeleteDatabaseKey(DatabaseKeyB &key,
				    DatabaseKeyB &compare_key,
				    BtreeCache *btree_cache);
  gxDatabaseError CollapseRoot(BtreeCacheNode &root_node); 
  gxDatabaseError CollapseRoot(BtreeCache *btree_cache);
  gxDatabaseError InsertBalance(BtreeCacheNode &parent_node, 
				BtreeKeyLocation_t key_location,
				DatabaseKeyB &compare_key,
				BtreeCache *btree_cache);
  gxDatabaseError InsertRightRotation(BtreeCacheNode &parent_node, 
				      BtreeKeyLocation_t key_location,
				      DatabaseKeyB &compare_key,
				      BtreeCache *btree_cache);
  gxDatabaseError InsertLeftRotation(BtreeCacheNode &parent_node,
				     BtreeKeyLocation_t key_location,
				     DatabaseKeyB &compare_key,
				     BtreeCache *btree_cache);
  gxDatabaseError Merge(BtreeCacheNode &parent_node,
			BtreeKeyLocation_t key_location,
			DatabaseKeyB &compare_key,
			BtreeCache *btree_cache);

#endif // __USE_BTREE_CACHE__
// =========================================================== //
// End - B-tree cache additions
// =========================================================== //
};

#endif // __GX_DATABASE_BTREE_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //

