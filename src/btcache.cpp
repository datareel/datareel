// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: btcache.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 08/09/2001
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

The B-tree cache classes are a special adaptation of the Device 
Cache classes coded to use the BtreeNode class as the bucket type 
and optimized for B-tree operations. The B-tree cache is composed 
of a series of cooperating classes used to link disk-based B-tree 
nodes to memory-based cache buckets allowing the B-tree class to 
use cache buckets in place of disk-base nodes. This caching strategy 
is designed to eliminate excessive disk reads/writes allowing you to 
build faster B-tree insert, find, and delete functions. 
*/
// ----------------------------------------------------------- //   
#include "gxdlcode.h"

#ifdef __USE_BTREE_CACHE__

#include "gxbtree.h"
#include "btcache.h"

gxDatabaseError BtreeBucket::Flush(gxDatabase *f) 
// Function used to flush this bucket to disk and reset the 
// bucket variables. Returns zero if successful or a non-zero 
// value to indicate a failure.
{ 
  gxDatabaseError err = gxDBASE_NO_ERROR;
  if((node_address != (FAU_t)0) && (is_dirty == 1)) {
    err = WriteNode(f);
    if(err == gxDBASE_NO_ERROR) {
      key_count = (BtreeKeyCount_t)0; // Reset the key count
      node_address = (FAU_t)0;        // Reset the node's file offset 
      is_dirty = 0;                   // Reset the dirty flag
    }
  }
  return err;
}

gxDatabaseError BtreeBucket::ReadNode(gxDatabase *f)
// Function use to read a B-tree node from the database file 
// and load it into this cache bucket. Returns zero if 
// successful or a non-zero value to indicate a failure.
{
  if(!f) return gxDBASE_NULL_PTR;

  if(f->Read(&key_count, sizeof(key_count), node_address) !=
     gxDBASE_NO_ERROR)
    return f->GetDatabaseError();
  if(f->Read(&left_child, sizeof(left_child)) != gxDBASE_NO_ERROR)
    return f->GetDatabaseError();

  // Optimize the read by only reading the exact number of keys
  if(key_count > (BtreeKeyCount_t)0) { // Ensure node has entries
    if(f->Read(key_entries, (key_size * key_count)) != 
       gxDBASE_NO_ERROR)
      return f->GetDatabaseError();
  }

  is_dirty = 0; // Reset this bucket's dirty flag
  return gxDBASE_NO_ERROR;
}

gxDatabaseError BtreeBucket::WriteNode(gxDatabase *f)
// Function used to write a B-tree node to the database file.
// Returns zero if successful or a non-zero value to indicate 
// a failure.
{
  if(!f) return gxDBASE_NULL_PTR;

  if(f->Write(&key_count, sizeof(key_count), node_address, 0, 0) 
     != gxDBASE_NO_ERROR)
    return f->GetDatabaseError();

  if(f->Write(&left_child, sizeof(left_child), gxCurrAddress, 0, 0) 
     != gxDBASE_NO_ERROR)
    return f->GetDatabaseError();
  
  // Optimize the write by only writing the exact number of keys
  if(key_count > (BtreeKeyCount_t)0) { // Ensure node has entries
    if(f->Write(key_entries, (key_size * key_count),
		gxCurrAddress, 0, 0) != gxDBASE_NO_ERROR)
      return f->GetDatabaseError();
  }

  is_dirty = 0; // Reset this bucket's dirty flag
  return gxDBASE_NO_ERROR;
}

BtreeCache::BtreeCache(unsigned num_bkts, BtreeSize_t dbkey_size, 
		       BtreeNodeOrder_t order)
{
  f = 0;
  buckets = 0;
  head = tail = 0;
  cache_size = 0;
  // PC-lint 04/20/2004: Ignoring return value of function (ignore)
  ConstructCache(num_bkts, dbkey_size, order);
}

BtreeCache::BtreeCache(unsigned num_bkts, gxBtree *btree)
{
  f = btree->gxDatabasePtr();
  buckets = 0;
  head = tail = 0;
  cache_size = 0;
  // PC-lint 04/20/2004: Ignoring return value of function (ignore)
  ConstructCache(num_bkts, 
		 btree->BtreeHeader()->key_size, 
		 btree->BtreeHeader()->node_order);
}

void BtreeCache::InitCache(BtreeSize_t dbkey_size, BtreeNodeOrder_t order)
// Internal processing function used to initialize the cache variables
// and setup a circularly linked-list of buckets using a previously
// allocated array of cache buckets.
{
  if(IsEmpty()) return; // No memory was allocated for the cache buckets

  unsigned bucket_size = sizeof(BtreeBucket);

  // PC-lint 04/20/2004: Possible use of null pointer
  if(!buckets) return;

  // Organize the buckets in a circularly linked-list. 
  for(int i = 0; i < num_buckets; i++) {
    BtreeBucket *p = (BtreeBucket *)((char *)buckets + (i * bucket_size)); 
    p->key_size = dbkey_size;
    p->node_order = order-1;
    p->key_count = (BtreeKeyCount_t)0;
    p->left_child = (FAU_t)0;
    p->key_entries = new char[(p->key_size * p->node_order)];
    p->node_address = (FAU_t)0;
    p->ResetBucket();
    p->prev = (BtreeBucket *)((char *)p - bucket_size);
    p->next = (BtreeBucket *)((char *)p + bucket_size);
  }

  // Set the head and tail pointers
  tail = (BtreeBucket *)((char *)buckets + (num_buckets-1)*bucket_size);
  head = (BtreeBucket *)((char *)buckets);
  tail->next = head;
  head->prev = tail;
  
  // Calculate the total number of bytes allocated
  BtreeSize_t key_size = dbkey_size * (order-1);
  cache_size = bucket_size + key_size;
  cache_size *= num_buckets;
}

BtreeBucket *BtreeCache::ConstructCache(unsigned num_bkts, 
					BtreeSize_t dbkey_size, 
					BtreeNodeOrder_t order)
// Constructs and initialize and array of buckets. NOTE: This
// implementation uses contiguous memory for the cache buckets.
// Returns a pointer to the bucket array or a null value if an
// error occurs.
{
  if(!IsEmpty()) DestroyCache();
  // Set minimum number of buckets. The gxBtree tree insert function 
  // requires a minimum of 2 cache buckets (left and right pointers)
  // and both delete functions require a minimum of 3 cache buckets 
  // (left, right, and parent pointers).
  if((num_bkts == 0) || (num_bkts < (unsigned)MIN_BTREECACHE_SEGMENTS)) {
    num_bkts = MIN_BTREECACHE_SEGMENTS; 
  }
  num_buckets = num_bkts;
  buckets = new BtreeBucket[num_bkts]; // Construct an array of buckets
  if(buckets) InitCache(dbkey_size, order);
  ResetStats();
  return buckets;
}

void BtreeCache::DestroyCache()
// Flushes all the cache buckets and frees the memory allocated for the
// cache bucket array.
{
  if(f) Flush(); // PC-lint 04/20/2004: Ignoring return value (ignore)
  Disconnect();
  if(buckets) delete[] buckets;
  buckets = 0;
}

gxDatabaseError BtreeCache::Flush()
// Function used to flush all the dirty buckets to the database file.
// Returns zero if successful or a non-zero value to indicate 
// a failure.
{
  if((f == 0) || (buckets == 0)) return gxDBASE_NULL_PTR;

  // Flush, starting at the front so that cache will be flushed
  // in a first in, last out sequence
  BtreeBucket *bucket = head;

  // PC-lint 04/20/2004: Possible use of null pointer
  if(!bucket) return gxDBASE_NULL_PTR;

  do {
    if(f->ReadyForWriting()) {
      if(bucket->Flush(f) != gxDBASE_NO_ERROR) {
	return f->GetDatabaseError(); // Error writing to the file 
      }
    }
    else { // The file is not ready for writing
      return f->SetDatabaseError(gxDBASE_FILE_NOT_READY);
    }

    bucket = bucket->next;
  } while(bucket != head);

  return gxDBASE_NO_ERROR;
}

gxDatabaseError BtreeCache::LRUFlush(unsigned num_to_flush)
// Function used to flush a specified number of least recently
// used (LRU) dirty buckets. Returns zero if successful or a 
// non-zero value to indicate a failure.
{
  if((f == 0) || (buckets == 0)) return gxDBASE_NULL_PTR;

  // Flush, starting at the tail so that cache will be flushed
  // in a first in, first out sequence (FIFO)
  BtreeBucket *bucket = tail;

  unsigned num_flushed = 0;

  // PC-lint 04/20/2004: Possible use ot null pointer
  if(!bucket) return gxDBASE_NULL_PTR;

  while(1) {
    if(num_flushed == num_to_flush) break;
    if(f->ReadyForWriting()) {
      if(bucket->Flush(f) != gxDBASE_NO_ERROR) {
	return f->GetDatabaseError(); // Error writing to the file 
      }
    }
    else { // The file is not ready for writing
      return f->SetDatabaseError(gxDBASE_FILE_NOT_READY);
    }
    bucket = bucket->prev;
    if(bucket == tail) break; // Reached the end of the list
    num_flushed++;
  }

  return gxDBASE_NO_ERROR;
}

gxDatabaseError BtreeCache::Connect(gxDatabase *fptr)
// Function used to connect the cache to a currently open
// database. Returns zero if successful or a non-zero value 
// to indicate a failure.
{ 
  if(!fptr) return gxDBASE_NULL_PTR;

  if((f != 0) && (buckets != 0)) {
    gxDatabaseError err = Flush();
    if(err != gxDBASE_NO_ERROR) return err;
  }
  f = fptr; 
  return gxDBASE_NO_ERROR;
}

gxDatabaseError BtreeCache::Disconnect() 
// Function used to disconnect the cache from a currently open
// or closed database. Returns zero if successful or a non-zero
// value to indicate a failure.
{ 
  if((f != 0) && (buckets != 0)) {
    gxDatabaseError err = Flush();
    if(err != gxDBASE_NO_ERROR) return err;
  }
  f = 0; 
  return gxDBASE_NO_ERROR;
}

BtreeBucket *BtreeCache::FindEmptyBucket()
// Internal processing function used to find the last
// bucket in the cache that is empty and promote the
// bucket to the front of the list. Returns a pointer
// to the bucket or a null value if no buckets can be
// made available or an error occurs. For maximum speed
// performance the least-recently reserved bucket will 
// be always be flushed and re-allocated to prevent the 
// cache from filling up.
{
  if(IsEmpty()) {
    // Set the database error code here to ensure that an 
    // exception is properly signaled in the gxBtree
    // cached insert, delete, and find functions.
    if(f) f->SetDatabaseError(gxDBASE_CACHE_ERROR);
    return 0;
  }

  BtreeBucket *bucket = tail;

  // PC-lint 04/20/2004: Possible use of null pointer
  if(!bucket) return 0;

  if(f) { // PC-lint 04/20/2004: Possible use of null pointer
    if(f->ReadyForWriting()) {
      if(bucket->Flush(f) != gxDBASE_NO_ERROR) {
	return 0; // Error writing to the file 
      }
    }
    else { // The file is not ready for writing
      f->SetDatabaseError(gxDBASE_FILE_NOT_READY);
      return 0;
    }
  }

  PromoteBucket(bucket);
  return bucket;
}

BtreeBucket *BtreeCache::FindBucket(FAU_t node_address)
// Function used to serach for a bucket by its node_address.
// Returns a null value if the node could not be found.
{
  BtreeBucket *bucket = head;

  // PC-lint 04/20/2004: Possible use of null pointer
  if(!bucket) return 0;

  do {
    if(bucket->node_address == node_address) return bucket;
    bucket = bucket->next;
  } while(bucket != head);

  return 0;
}

void BtreeCache::DemoteBucket(BtreeBucket *bucket)
// Demote this bucket to the end of list to become the
// least recently reserved bucket.
{
  if(IsEmpty()) return;

  // PC-lint 04/20/2004: Possible use of null pointer
  if(!bucket) return;

  if(bucket == tail) { // This is the tail bucket
    return;
  }
  else if(bucket == head) { // This is the head bucket
    bucket->next->prev = bucket;
    head = bucket->next;
  }
  else { // We are some where in the middle of the list
    bucket->prev->next = bucket->next;
    bucket->next->prev = bucket->prev;
  }
  bucket->next = head;
  bucket->prev = tail;

  // PC-lint 04/20/2004: Possible use of null pointer
  if(tail) tail->next = bucket;

  tail = bucket;
}

void BtreeCache::PromoteBucket(BtreeBucket *bucket)
// Promote this bucket to the front of list to become the
// most recently reserved bucket.
{
  if(IsEmpty()) return;

  if(bucket == head) { // This is the head bucket
    return;
  }
  if(bucket == tail) { // This is the tail bucket
    // PC-lint 04/20/2004: Possible use of null pointer 
    if(bucket) tail = bucket->prev;
    if(head) head->prev = bucket;
    head = bucket;
    if(head) head->prev = tail;
  }
  else if(bucket != head) { // We are some where in the middle of the list
    // PC-lint 04/20/2004: Possible use of null pointer
    if(bucket) bucket->prev->next = bucket->next;
    if(bucket) bucket->next->prev = bucket->prev;

    // Move the bucket to the front of the list
    if(bucket) bucket->next = head;
    if(bucket) bucket->prev = tail;
    if(tail) tail->next = bucket;
    if(head) head->prev = bucket;
    head = bucket;
  }
}

BtreeBucket *BtreeCache::AllocBucket(FAU_t node_address, int load_bucket)
// Connects a B-tree bucket to the specified node and reads the node
// from disk if the "load_bucket" variable is true. Returns a pointer
// to the bucket or a null value if an error occurred. NOTE: If the 
// the node already exists in the cache this function will return a
// pointer to the bucket.
{
  if(node_address == (FAU_t)0) { 
    // Set the database error code here to ensure that an exception 
    // is properly signaled in the gxBtree cached insert, delete, 
    // and find functions.
    if(f) f->SetDatabaseError(gxDBASE_CACHE_ERROR);
    return 0; 
  }

  // Look for this node in the cache
  BtreeBucket *bucket = FindBucket(node_address);

  if(bucket == 0) { // This node is not in the B-tree cache
    uncached_reads++;
    bucket = FindEmptyBucket();
    if(bucket) {
      bucket->node_address = node_address;
      if((load_bucket == 1) && (node_address != (FAU_t)0)) {
	if(!f) return 0; // Not connected to a database file	
        if(bucket->ReadNode(f) != gxDBASE_NO_ERROR) {
	  return 0; // Error reading the node from disk
	}
      }
    }
    else {
      // Set the database error code here to ensure that an exception 
      // is properly signaled in the gxBtree cached insert, delete, 
      // and find functions.
      if(f) { // PC-lint 04/20/2004: Possible use of null pointer
	if(f->GetDatabaseError() == gxDBASE_NO_ERROR) {
	  f->SetDatabaseError(gxDBASE_CACHE_ERROR);
	}
      }
      return 0; // Could not find an empty bucket
    }
  }
  else {
    PromoteBucket(bucket);
    cached_reads++;
  }

  return bucket;
}

unsigned BtreeCache::BucketsInUse()
// Returns the total number of dirty buckets.
{
  if(IsEmpty()) return 0;
    
  BtreeBucket *bucket = head;
  unsigned num_dirty = 0;

  // PC-lint 04/20/2004: Possible use of null pointer
  if(!bucket) return 0;

  do {
    if(bucket->IsDirty()) num_dirty++;
    bucket = bucket->next;
  } while(bucket != head);

  return num_dirty;
}

int BtreeCacheNode::Alloc(BtreeNode *node, FAU_t address)
// Function used to allocate a cache bucket for a B-tree node.
// Returns ture if the bucket was allocated or false is the 
// a cache bucket could not be allocated. NOTE: This function
// will copy the contents of the specified node to ensure that
// each cached copy of the node is unique.
{
  // PC-lint 04/20/2004: Declaration of local bucket hides global bucket symbol
  // BtreeBucket *bucket = Alloc(node->SizeOfBtreeNode(), address);
  // TODO: Retest

  bucket = Alloc(node->SizeOfBtreeNode(), address);
  if(!bucket) {
    // Set the database error code here to ensure that an exception 
    // is properly signaled in the gxBtree cached insert, delete, 
    // and find functions.
    if(cache->gxDatabasePtr()->GetDatabaseError() == gxDBASE_NO_ERROR) {
      cache->gxDatabasePtr()->SetDatabaseError(gxDBASE_CACHE_ERROR);
    }
    return 0;
  }
  bucket->key_count = node->key_count;
  bucket->left_child = node->left_child;
  memmove(bucket->key_entries, node->key_entries, 
          (bucket->key_size * bucket->key_count));
  return 1;
}

BtreeBucket *BtreeCacheNode::Alloc(size_t node_size, FAU_t address)
// Allocates a cache bucket equal to the size of a B-tree node.
// The Alloc function will load data into the bucket the node
// unless the specified node address is equal to zero. Returns
// a pointer to the cache bucket or a null pointer if an error
// occurs.
{
  if(address == (FAU_t)0) {
    if(!cache->gxDatabasePtr()) return 0; // Not connected to a database file
    address = cache->gxDatabasePtr()->Alloc(node_size);
    if(address == (FAU_t)0) { // Could not allocate database block
      // Set the database error code here to ensure that an exception 
      // is properly signaled in the gxBtree cached insert, delete, 
      // and find functions.
      if(cache->gxDatabasePtr()->GetDatabaseError() == gxDBASE_NO_ERROR) {
	cache->gxDatabasePtr()->SetDatabaseError(gxDBASE_CACHE_ERROR);
      }
      return 0; 
    }
  }

  // Reserve the bucket without loading it.
  bucket = cache->AllocBucket(address, 0); 
  if(bucket) {
    node_address = address;
  }
  else {
    node_address = (FAU_t)0;
    // Set the database error code here to ensure that an exception 
    // is properly signaled in the gxBtree cached insert, delete, 
    // and find functions.
    if(cache->gxDatabasePtr()->GetDatabaseError() == gxDBASE_NO_ERROR) {
      cache->gxDatabasePtr()->SetDatabaseError(gxDBASE_CACHE_ERROR);
    }
  }
  return bucket;
}

int BtreeCacheNode::Delete()
// Function used to delete the B-tree node this caches node is
// connected to. Returns true if the node was deleted or false
// if an error occurs.
{
  if(!bucket) { // Not pointing to a valid bucket
    // Set the database error code here to ensure that an exception 
    // is properly signaled in the gxBtree cached insert, delete, 
    // and find functions.
    cache->gxDatabasePtr()->SetDatabaseError(gxDBASE_CACHE_ERROR);
    return 0; 
  }
  if(!cache->gxDatabasePtr()) return 0; // Not connected to a database file

  if(bucket->node_address == node_address) { // Pointing to valid bucket
    bucket->ResetBucket();   // Reset the B-tree node and bucket
    node_address = (FAU_t)0; // Reset the cache node address  
    cache->DemoteBucket(bucket);
  }
  else {
    // Set the database error code here to ensure that an exception 
    // is properly signaled in the gxBtree cached insert, delete, 
    // and find functions.
    if(cache->gxDatabasePtr()->GetDatabaseError() == gxDBASE_NO_ERROR) {
      cache->gxDatabasePtr()->SetDatabaseError(gxDBASE_CACHE_ERROR);
    }
    return 0;
  }
  return 1;
}

BtreeBucket *BtreeCacheNode::Bind()
// Function used to bind this cache node to a cache bucket 
// and load the B-tree node residing at node address. Returns
// a pointer to the bucket.
{
  if(bucket) {
    if(bucket->node_address == node_address) { // Pointing to valid bucket
      return bucket;
    }
  }

  // If this pointer is not assigned to a bucket or the bucket is locked
  // connect the bucket and load the node
  bucket = cache->AllocBucket(node_address, 1);
  if(!bucket) { // Could not allocate a cache bucket
    // Reset this buckets node address to signal that this bucket
    // has not been assigned. This allows the application to test
    // node address before using this cache pointer.
    node_address = (FAU_t)0; 
    if(cache->gxDatabasePtr()->GetDatabaseError() == gxDBASE_NO_ERROR) {
      cache->gxDatabasePtr()->SetDatabaseError(gxDBASE_CACHE_ERROR);
    }	
  }

  // Return a pointer to this nodes bucket
  return bucket;
}

#endif // __USE_BTREE_CACHE__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
