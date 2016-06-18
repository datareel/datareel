// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: dbcache.h
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/04/2002
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

Experimental database cache classes.

The database file cache works through multiple inheritance with 
three primary classes comprising the user interface: the 
gxDatabaseSegment class, the gxDatabaseCache class, and the 
gxDBCacheNode class. 

The gxDatabaseSegment defines the methods used to read, write, and 
flush cached file data. Cache segments are stored in memory and 
linked to a file address. Each segment contains a dirty flag used 
to signal to the cache that the in-memory copy has been modified. 
The dirty flag is also used to lock memory segments.

The gxDatabaseCache class is responsible for the allocation and 
de-allocation of a fixed number of cache segments and incorporates 
functions used to handle requests for memory-based file data. The 
cache must determine whether a cache segment is already loaded. If 
the buffer is not loaded the cache reserves a memory segment and 
loads the node into memory. This cache design uses a cache node to 
reference cache segments directly, with each cache node being 
initialized after the segment is reserved. Cache segments are 
organized into a doubly linked list with the number of segments 
determined when a gxDatabaseCache object is constructed.

The gxDBCacheNode class is used to access cache segments directly, 
which allows classes using the database cache to load and unload 
cache segments as needed. Cache nodes are used to work in conjunction 
with the cached segments. Each cache node stores a pointer to the 
segment containing file data and a pointer to the cache the cache 
node is connected to. The segment's dirty flag is used to synchronize 
multiple cache nodes connected to the same cache.
*/
// ----------------------------------------------------------- //   
#ifndef __GX_DATABASE_CACHE_HPP__
#define __GX_DATABASE_CACHE_HPP__

#include "gxdlcode.h"

#ifdef __USE_DATABASE_CACHE__

#include "gxdfptr.h"
#include "gxderror.h"
#include "membuf.h"

// Set minimum number of file segments. 
const int MIN_DATABASE_SEGMENTS = 3;

// Variable length raw memory segments
class GXDLCODE_API gxDatabaseSegment
{
public:
  gxDatabaseSegment() { prev = next = 0; ResetSegment(); }
  gxDatabaseSegment(unsigned segment_size) {
    prev = next = 0;
    ResetSegment();
    ds.Alloc(segment_size);
  }
  ~gxDatabaseSegment() { }

public: // Segment functions
  void ResetSegment() { // Reset the segment variables 
    is_dirty = 0; ds_address = (FAU_t)-1; ds.Clear(0);
  }
  int IsDirty() { return is_dirty == 1; }
  int IsDirty() const { return is_dirty == 1; }
  void SetDirty() { is_dirty = 1; }
  int IsNull() { return ds.is_null(); }  
  int IsNull() const { return ds.is_null(); }  

public: // 32/64-bit file functions
  gxDatabaseError Flush(gxdFPTR *fp);
  gxDatabaseError Read(gxdFPTR *fp, __ULWORD__ bytes);
  gxDatabaseError Write(gxdFPTR *fp);

private:
  char is_dirty; // Signal to the cache to flush this segment

public:
  gxDatabaseSegment *prev; // Previous database segment in the list
  gxDatabaseSegment *next; // Next database segment in the list
  FAU_t ds_address;        // File address of this database segment

public: // Persistent data members
  MemoryBuffer ds; // In memory copy of raw file data
};

class GXDLCODE_API gxDatabaseCache
{
public:
  gxDatabaseCache(unsigned num_segs);
  gxDatabaseCache(unsigned num_segs, gxdFPTR *fptr);
  ~gxDatabaseCache() { DestroyCache(); }

public:
  gxDatabaseSegment *ConstructCache(unsigned num_segs);
  void DestroyCache();
  int IsEmpty() { return segments == 0; }
  int IsEmpty() const { return segments == 0; }
  gxDatabaseError Connect(gxdFPTR *fptr);
  gxDatabaseError Disconnect();
  gxDatabaseError Flush();
  gxDatabaseError LRUFlush(unsigned num_to_flush);
  gxDatabaseSegment *AllocSegment(FAU_t ds_address, 
				  __ULWORD__ bytes = (__ULWORD__)0);
  gxDatabaseSegment *FindSegment(FAU_t ds_address);
  gxdFPTR *gxFilePtr() { return f; }
  gxDatabaseSegment *GetHead() { return head; }
  gxDatabaseSegment *GetHead() const { return head; } 
  gxDatabaseSegment *GetTail() { return tail; }
  gxDatabaseSegment *GetTail() const { return tail; } 
  gxDatabaseSegment *DemoteSegment(gxDatabaseSegment *segment);
  gxDatabaseSegment *PromoteSegment(gxDatabaseSegment *segment);
  void ReleaseFile() { f = 0; }
  void ReleaseSegments() { segments = 0; }

public: // Cache statistics
  int NumSegments() { return num_segments; }
  int NumSegments() const { return num_segments; }
  unsigned CacheSize();
  unsigned SegmentsInUse();

private: // Internal processing functions
  gxDatabaseSegment *FindEmptySegment();
  void InitCache();

public: // Overloaded operators
  int operator!() const { return segments == 0; }
  int operator!() { return segments == 0; }
  operator const int () const { return segments != 0; }
  operator int () { return segments != 0; }

private:
  unsigned num_segments;       // Total number of segments allocated
  unsigned cache_size;         // Number of bytes allocated for the cache
  gxdFPTR *f;                  // File the cache is connected to
  gxDatabaseSegment *head;     // Most recently reserved segment
  gxDatabaseSegment *tail;     // Least recently reserved segment
  gxDatabaseSegment *segments; // Array of cache segments

public: // Cache statistics
  void ResetStats() {
    uncached_reads = cached_reads = 0;
  }
  unsigned uncached_reads; // Counts the number of disk reads
  unsigned cached_reads;   // Counts the number of cached reads
};

class GXDLCODE_API gxDBCacheNode
{
public:
  gxDBCacheNode(gxDatabaseCache &ds_cache, FAU_t address = (FAU_t)-1) {
    // Constructs a cache pointer and connects it a previously
    // constructed cache object.
    cache = &ds_cache; 
    ds_address = address; 
    segment = 0;
  }
  gxDBCacheNode(gxDatabaseCache *ds_cache, FAU_t address = (FAU_t)-1) {
    // Constructs a cache pointer and connects it a previously
    // constructed cache object.
    cache = ds_cache; 
    ds_address = address; 
    segment = 0;
  }
  gxDBCacheNode(const gxDBCacheNode &ob) {
    segment = ob.segment; 
    cache = ob.cache; 
    ds_address = (FAU_t)0;
  }
  gxDBCacheNode& operator=(const gxDBCacheNode &ob) {
    segment = ob.segment; 
    cache = ob.cache;
    ds_address = (FAU_t)0;
    return *this;
  }
  ~gxDBCacheNode() { }

  void operator=(FAU_t address) {
    // Assignment operator used to assign this node to a file offset
    // and bind the cached node to a segment.
    ds_address = address; 
    Bind();
  }

public:
  int Delete();
  gxDatabaseSegment *CachedWrite(const void *buf, __ULWORD__ bytes, 
				FAU_t address);
  gxDatabaseSegment *CachedRead(FAU_t address,
			       __ULWORD__ bytes = ( __ULWORD__)0);
  gxDatabaseSegment *Bind();
  FAU_t GetNodeAddress() { return ds_address; }
  FAU_t GetNodeAddress() const { return ds_address; }
  gxDatabaseSegment *GetSegment() { return segment; }
  gxDatabaseSegment *GetSegment() const { return segment; }
  gxDatabaseCache *GetCache() { return cache; }
  gxDatabaseCache *GetCache() const { return cache; }
  int HasSegment() { return segment != 0; }
  int HasSegment() const { return segment != 0; }
  void Release() { segment = 0; }
  void ReleaseCache() { cache = 0; }

public: // Conversion functions
  operator FAU_t() const { return ds_address; }
  operator FAU_t() { return ds_address; }

public: // Overloaded operators
  gxDatabaseSegment &operator*() {
    // Overloaded indirection operator that allows
    // a data segment cache node to dereferenced the
    // segment this object is currently referencing. 
    return *(gxDatabaseSegment *)Bind();
  }

  gxDatabaseSegment *operator->() {
    // Overloaded arrow operator used to reference
    // a data segment cache node to the segment it
    // is currently referencing. 
    return (gxDatabaseSegment *)Bind();
  }

private:
  FAU_t ds_address;           // Database segment's file address
  gxDatabaseSegment *segment; // Current cache segment
  gxDatabaseCache *cache;     // Pointer to this object's cache
};

#endif // __USE_DATABASE_CACHE__

#endif // __GX_DATABASE_CACHE_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //

