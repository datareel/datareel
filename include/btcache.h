// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: btcache.h
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 08/09/2001
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

The B-tree cache classes are a special adaptation of the Device 
Cache classes coded to use the BtreeNode class as the bucket type 
and optimized for B-tree operations. The B-tree cache is composed 
of a series of cooperating classes used to link disk-based B-tree 
nodes to memory-based cache buckets allowing the B-tree class to 
use cache buckets in place of disk-base nodes. This caching strategy 
is designed to eliminate excessive disk reads/writes allowing you to 
build faster B-tree insert, find, and delete functions. 

The B-tree cache works through multiple inheritance with three 
primary classes comprising the user interface: the BtreeBucket 
class, the BtreeCache class, and the BtreeCacheNode class. 

The BtreeBucket class inherits the BtreeNode class and defines the 
methods use to read, write, and flush B-tree nodes. Cache buckets are 
stored in memory and linked to a file address. Each bucket contains a 
dirty flag used to signal to the cache that the in-memory copy has been 
modified. The dirty flag is also used to lock the bucket. In addition 
to the dirty flag the BtreeBucket class adds previous and next pointers 
used to link memory-based buckets together.

The BtreeCache class is responsible for the allocation and de-allocation 
of a fixed number of cache buckets and incorporates functions used to 
handle requests for memory-based B-tree nodes. The cache must determine 
whether a cache bucket is already loaded. If the buffer is not loaded the 
cache reserves a bucket and loads the node into memory. This cache design 
uses cache node to reference cache buckets directly, with each cache 
node being initialized after the bucket is reserved. Cache buckets are 
organized into a doubly linked list with the number of buckets determined 
when a BtreeCache object is constructed.

The BtreeCacheNode class is used to access cache buckets directly, which 
allows the B-tree class to load and unload cache buckets as needed. Cache 
nodes are used to work in conjunction with the cache buckets. Each 
cache node stores a pointer to the bucket containing the B-tree node 
and a pointer to the cache the cache node is connected to. The bucket's 
dirty flag is used to synchronize multiple cache nodes connected to the 
same cache.

Changes:
==============================================================
02/08/2002: All B-tree cache functions using non-persistent file 
address units have been modified to use FAU_t data types instead 
of FAU types. FAU_t types are native or built-in integer types 
and require no additional overhead to process. All persistent 
file address units still use the FAU integer type, which is a 
platform independent type allowing database files to be shared 
across multiple platforms.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __BTREE_CACHE_HPP__
#define __BTREE_CACHE_HPP__

#include "gxdlcode.h"

#ifdef __USE_BTREE_CACHE__

#include "gxdbase.h"
#include "btnode.h"

// Set minimum number of cache segments. NOTE: The gxBtree tree insert  
// function requires a minimum of 2 cached nodes (left and right 
// pointers) and both delete functions require a minimum of 3 cached 
// nodes (left, right, and parent pointers).
const int MIN_BTREECACHE_SEGMENTS = 3;

class gxBtree; // Foward class declaration

class GXDLCODE_API BtreeBucket : public BtreeNode
{
public:
  BtreeBucket() { ResetBucket(); }
  ~BtreeBucket() { }

public: // Bucket functions
  void ResetBucket() { // Reset the bucket variables 
    is_dirty = 0; node_address = (FAU_t)0; key_count = (BtreeKeyCount_t)0; 
  }
  void ResetNode() { // Reset the B-tree node without resetting the bucket
    key_count = (BtreeKeyCount_t)0;
  }
  int IsDirty() { return is_dirty == 1; }
  int IsDirty() const { return is_dirty == 1; }
  void SetDirty() { is_dirty = 1; }
  int IsNull() { return node_address == (FAU_t)0; }  
  int IsNull() const { return node_address == (FAU_t)0; }  
  int HasKeys() { return key_count > (BtreeKeyCount_t)0; }
  int HasKeys() const { return key_count > (BtreeKeyCount_t)0; }

public: // Type casting functions
  BtreeNode *GetNode() { return (BtreeNode *)this; }

public: // Database engine functions
  gxDatabaseError Flush(gxDatabase *f);
  gxDatabaseError ReadNode(gxDatabase *f);
  gxDatabaseError WriteNode(gxDatabase *f);

private:
  char is_dirty; // Signal to the cache to flush this bucket

public:
  BtreeBucket *prev; // Previous bucket in the list
  BtreeBucket *next; // Next bucket in the list
};

class GXDLCODE_API BtreeCache
{
public:
  BtreeCache(unsigned num_bkts, gxBtree *btree);
  BtreeCache(unsigned num_bkts, BtreeSize_t dbkey_size, 
	     BtreeNodeOrder_t order);
  ~BtreeCache() { DestroyCache(); }

public:
  BtreeBucket *ConstructCache(unsigned num_bkts, BtreeSize_t dbkey_size, 
			      BtreeNodeOrder_t order);
  void DestroyCache();
  int IsEmpty() { return buckets == 0; }
  int IsEmpty() const { return buckets == 0; }
  gxDatabaseError Connect(gxDatabase *fptr);
  gxDatabaseError Disconnect();
  gxDatabaseError Flush();
  gxDatabaseError LRUFlush(unsigned num_to_flush);
  BtreeBucket *AllocBucket(FAU_t node_address, int load_bucket = 1);
  BtreeBucket *FindBucket(FAU_t node_address);
  gxDatabase *gxDatabasePtr() { return f; }
  BtreeBucket *GetHead() { return head; }
  BtreeBucket *GetHead() const { return head; } 
  BtreeBucket *GetTail() { return tail; }
  BtreeBucket *GetTail() const { return tail; } 
  void DemoteBucket(BtreeBucket *bucket);
  void PromoteBucket(BtreeBucket *bucket);

public: // Cache statistics
  int NumBuckets() { return num_buckets; }
  int NumBuckets() const { return num_buckets; }
  unsigned CacheSize() { return cache_size; }
  unsigned CacheSize() const { return cache_size; }
  unsigned BucketsInUse();

private: // Internal processing functions
  BtreeBucket *FindEmptyBucket();
  void InitCache(BtreeSize_t dbkey_size, BtreeNodeOrder_t order);

public: // Overloaded operators
  int operator!() const { return buckets == 0; }
  int operator!() { return buckets == 0; }
  operator const int () const { return buckets != 0; }
  operator int () { return buckets != 0; }

private:
  int num_buckets;      // Total number of buckets allocated
  unsigned cache_size;  // Number of bytes allocated for the cache
  gxDatabase *f;        // Database file the cache is connected to
  BtreeBucket *head;    // Most recently reserved bucket
  BtreeBucket *tail;    // Least recently reserved bucket
  BtreeBucket *buckets; // Array of cache buckets

public: // Cache statistics
  void ResetStats() {
    uncached_reads = cached_reads = 0;
  }
  unsigned uncached_reads; // Counts the number of disk reads
  unsigned cached_reads;   // Counts the number of cached reads
};

// B-tree cache node class
class GXDLCODE_API BtreeCacheNode
{
public:
  BtreeCacheNode(BtreeCache &btree_cache, FAU_t address = (FAU_t)0) {
    // Constructs a cache pointer and connects it a previously
    // constructed cache object.
    cache = &btree_cache; 
    node_address = address; 
    bucket = 0;
  }
  BtreeCacheNode(BtreeCache *btree_cache, FAU_t address = (FAU_t)0) {
    // Constructs a cache pointer and connects it a previously
    // constructed cache object.
    cache = btree_cache; 
    node_address = address; 
    bucket = 0;
  }
  BtreeCacheNode(const BtreeCacheNode &ob) {
    bucket = ob.bucket; 
    cache = ob.cache; 
    node_address = (FAU_t)0;
  }
  BtreeCacheNode& operator=(const BtreeCacheNode &ob) {
    bucket = ob.bucket; 
    cache = ob.cache;
    node_address = (FAU_t)0;
    return *this;
  }
  ~BtreeCacheNode() { }

  void operator=(FAU_t address) {
    // Assignment operator used to assign this node to a file offset
    // and bind the cached node to a bucket.
    node_address = address; 
    Bind();
  }

public:
  int Alloc(BtreeNode *node, FAU_t address = (FAU_t)0);
  int Alloc(BtreeNode &node, FAU_t address = (FAU_t)0) { 
    return Alloc(&node, address); 
  }
  int Delete();
  BtreeBucket *Alloc(size_t node_size, FAU_t address = (FAU_t)0);
  BtreeBucket *Bind();
  FAU_t GetNodeAddress() { return node_address; }
  FAU_t GetNodeAddress() const { return node_address; }
  BtreeBucket *GetBucket() { return bucket; }
  BtreeBucket *GetBucket() const { return bucket; }
  BtreeCache *GetCache() { return cache; }
  BtreeCache *GetCache() const { return cache; }
  int HasBucket() { return bucket != 0; }
  int HasBucket() const { return bucket != 0; }
  void Release() { bucket = 0; }

public: // Conversion functions
  // PC-lint 05/02/2005: Reports multiple errors for duplicate converstion 
  // functions. Only the non-const version is used so the const version has
  // be commented out for PC-lint checks
  // operator FAU_t() const { return node_address; }
  operator FAU_t() { return node_address; }

public: // Overloaded operators
  BtreeBucket &operator*() {
    // Overloaded indirection operator that allows
    // a B-tree cache node to dereferenced the
    // bucket this object is currently referencing. 
    return *(BtreeBucket *)Bind();
  }

  BtreeBucket *operator->() {
    // Overloaded arrow operator used to reference
    // a B-tree cache node to the bucket it
    // is currently referencing. 
    return (BtreeBucket *)Bind();
  }

private:
  FAU_t node_address;  // B-tree node address
  BtreeBucket *bucket; // Current cache bucket
  BtreeCache *cache;   // Pointer to this object's cache
};
#endif // __USE_BTREE_CACHE__

#endif // __BTREE_CACHE_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
