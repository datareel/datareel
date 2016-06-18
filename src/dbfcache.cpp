// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: dbcache.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/04/2002
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

Experimental database cache classes.

The database file cache classes are a special adaptation of the 
Device Cache classes coded to use raw file data as the segment 
type. The database file cache is composed of a series of
cooperating classes used to link disk-based database nodes to 
memory-based cache segments allowing classes using the database 
cache to use cache segments in place of disk-based file data. This 
caching strategy is designed to eliminate excessive disk reads and
writes allowing users to build faster database functions.
*/
// ----------------------------------------------------------- //   
#include "gxdlcode.h"

#ifdef __USE_DATABASE_CACHE__

#include "dbfcache.h"

gxDatabaseError gxDatabaseSegment::Flush(gxdFPTR *fp) 
// Function used to flush this segment to disk and reset the 
// segment variables. Returns zero if successful or a non-zero 
// value to indicate a failure.
{ 
  if(!fp) return gxDBASE_NULL_PTR;
  if(is_dirty == 0) return gxDBASE_NO_ERROR;

  gxDatabaseError err = gxDBASE_NO_ERROR;

  if((ds_address != (FAU_t)-1) && (is_dirty == 1)) {
    err = Write(fp);
    if(err == gxDBASE_NO_ERROR) {
      ds_address = (FAU_t)-1; // Reset the segment's file offset 
      is_dirty = 0; // Reset the dirty flag
    }
  }
  return err;
}

gxDatabaseError gxDatabaseSegment::Read(gxdFPTR *fp, __ULWORD__ bytes)
// Function use to read a node from the database file 
// and load it into this cache segment. Returns zero if 
// successful or a non-zero value to indicate a failure.
{
  if(!fp) return gxDBASE_NULL_PTR;
  unsigned char *buf = (unsigned char*)ds.Alloc(bytes);
  if(!buf) return gxDBASE_MEM_ALLOC_ERROR;

  if(gxdFPTRSeek(fp, ds_address, gxDBASE_SEEK_BEG) == (FAU_t)-1) {
    return gxDBASE_FILE_SEEK_ERROR;
  }
  if(gxdFPTRRead(fp, (unsigned char*)buf, bytes) != bytes) {
    return gxDBASE_FILE_READ_ERROR;
  }
  is_dirty = 0; // Reset this data segment's dirty flag
  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxDatabaseSegment::Write(gxdFPTR *fp)
// Function used to write a cache segment to the database file.
// Returns zero if successful or a non-zero value to indicate 
// a failure.
{
  if(!fp) return gxDBASE_NULL_PTR;
  if(ds.is_null()) return gxDBASE_NO_ERROR;

  if(gxdFPTRSeek(fp, ds_address, gxDBASE_SEEK_BEG) == (FAU_t)-1) {
    return gxDBASE_FILE_SEEK_ERROR;
  }

  __ULWORD__ bytes = ds.length();
  if(gxdFPTRWrite(fp, (const unsigned char*)ds.m_buf(), bytes) != 
     bytes) {
    return gxDBASE_FILE_WRITE_ERROR;
  }
  is_dirty = 0; // Reset this data segment's dirty flag
  return gxDBASE_NO_ERROR;
}

gxDatabaseCache::gxDatabaseCache(unsigned num_segs)
{
  f = 0;
  segments = 0;
  head = tail = 0;
  cache_size = 0;
  ConstructCache(num_segs);
}

gxDatabaseCache::gxDatabaseCache(unsigned num_segs, gxdFPTR *fptr)
{
  f = fptr;
  segments = 0;
  head = tail = 0;
  cache_size = 0;
  ConstructCache(num_segs);
}

void gxDatabaseCache::InitCache()
// Internal processing function used to initialize the cache variables
// and setup a circularly linked-list of segments using a previously
// allocated array of cache segments.
{
  if(IsEmpty()) return; // No memory was allocated for the cache segments
  gxDatabaseSegment *p = segments;
  unsigned segment_size = sizeof(gxDatabaseSegment);

  // Organize the segments in a circularly linked-list. 
  for(unsigned i = 0; i < num_segments; i++) {
    if(p) { // PC-lint 04/21/2004: Possible use of null pointer
      p->ResetSegment();
      p->prev = (gxDatabaseSegment *)((char *)p - segment_size);
      p->next = (gxDatabaseSegment *)((char *)p + segment_size);
      p = p->next;
    }
  }

  // Set the head and tail pointers
  if(segments) { // PC-lint 04/21/2004: Possible use of null pointer
    tail = (gxDatabaseSegment *)((char *)segments+(num_segments-1)*segment_size);
    tail->next = segments;
    segments->prev = tail;
    head = segments;
  }
}

unsigned gxDatabaseCache::CacheSize() 
// Returns the size of cache accounting for all 
// variable length memory segments.
{ 
  if(segments == 0) return 0;
  unsigned segment_size = sizeof(gxDatabaseSegment);
  cache_size = segment_size * num_segments;
  unsigned total_segment_size = 0;
  gxDatabaseSegment *segment = head;
  // PC-lint 04/21/2004: Possible use of null pointer
  if(!segment) return 0;
  do {
    total_segment_size += segment->ds.dlength();
    segment = segment->next;
  } while(segment != head);

  return cache_size + total_segment_size; 
}

gxDatabaseSegment *gxDatabaseCache::ConstructCache(unsigned num_segs) 
// Constructs and initialize and array of segments. NOTE: This
// implementation uses contiguous memory for the cache segments.
// Returns a pointer to the segment array or a null value if an
// error occurs.
{
  if(!IsEmpty()) DestroyCache();
  if((num_segs == 0) || (num_segs < (unsigned)MIN_DATABASE_SEGMENTS)) { 
    num_segs = MIN_DATABASE_SEGMENTS; 
  }
  num_segments = num_segs;
  segments = new gxDatabaseSegment[num_segs]; // Construct an array of segments
  if(segments) InitCache();
  ResetStats();
  return segments;
}

void gxDatabaseCache::DestroyCache()
// Flushes all the cache segments and frees the memory allocated for the
// cache segment array.
{
  if(f) Flush();
  Disconnect();
  if(segments) delete[] segments;
  segments = 0;
}

gxDatabaseError gxDatabaseCache::Flush()
// Function used to flush all the dirty segments to the database file.
// Returns zero if successful or a non-zero value to indicate 
// a failure.
{
  if((f == 0) || (segments == 0)) return gxDBASE_NULL_PTR;

  // Flush, starting at the front so that cache will be flushed
  // in a first in, last out sequence
  gxDatabaseSegment *segment = head;

  // PC-lint 04/21/2004: Possible use of null pointer
  if(!segment) return gxDBASE_NULL_PTR;

  do {
    if(segment->IsDirty()) {
      gxDatabaseError err = segment->Flush(f);
      if(err != gxDBASE_NO_ERROR) {
	return err; // Error writing to the file 
      }
    }
    segment = segment->next;
  } while(segment != head);

  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxDatabaseCache::LRUFlush(unsigned num_to_flush)
// Function used to flush a specified number of least recently
// used (LRU) dirty segments. Returns zero if successful or a 
// non-zero value to indicate a failure.
{
  if((f == 0) || (segments == 0)) return gxDBASE_NULL_PTR;

  // Flush, starting at the tail so that cache will be flushed
  // in a first in, first out sequence (FIFO)
  gxDatabaseSegment *segment = tail;

  unsigned num_flushed = 0;

  // PC-lint 04/21/2004: Possible use of null pointer
  if(!segment) return gxDBASE_NULL_PTR;

  while(1) {
    if(num_flushed == num_to_flush) break;
    if(segment->IsDirty()) {
      gxDatabaseError err = segment->Flush(f);
      if(err != gxDBASE_NO_ERROR) {
	return err; // Error writing to the file 
      }
    }
    segment = segment->prev;
    if(segment == tail) break; // Reached the end of the list
    num_flushed++;
  }

  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxDatabaseCache::Connect(gxdFPTR *fptr)
// Function used to connect the cache to a currently open
// database. Returns zero if successful or a non-zero value 
// to indicate a failure.
{ 
  if(!fptr) return gxDBASE_NULL_PTR;

  if((f != 0) && (segments != 0)) {
    gxDatabaseError err = Flush();
    if(err != gxDBASE_NO_ERROR) return err;
  }
  f = fptr; 
  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxDatabaseCache::Disconnect() 
// Function used to disconnect the cache from a currently open
// or closed database. Returns zero if successful or a non-zero
// value to indicate a failure.
{ 
  if((f != 0) && (segments != 0)) {
    gxDatabaseError err = Flush();
    if(err != gxDBASE_NO_ERROR) return err;
  }
  f = 0; 
  return gxDBASE_NO_ERROR;
}

gxDatabaseSegment *gxDatabaseCache::FindEmptySegment()
// Internal processing function used to find the last
// segment in the cache that is empty and promote the
// segment to the front of the list. Returns a pointer
// to the segment or a null value if no segments can be
// made available or an error occurs. If the cache is
// full the least-recently reserved segment will be
// flushed and re-allocated to prevent the cache from
// filling up.
{
  if(IsEmpty()) return 0;
  
  gxDatabaseSegment *segment = tail; // Least-recently reserved segment

  // PC-lint 04/21/2004: Possible use of null pointer
  if(!segment) return 0;

  while(1) {
     // Found an empty segment in the cache 
    if(!segment->IsDirty()) break;

    // Keep searching for the last empty segment
    segment = segment->prev;

    // The cache is full, so flush the last segment and 
    // move it to the head of the list.
    if(segment == tail) {
      if(segment) { // PC-lint 04/21/2004: Possible use of null pointer
	if(segment->Flush(f) != gxDBASE_NO_ERROR) {
	  return 0; // Error writing to the file
	}
      }
      return PromoteSegment(segment); 
    }
  }  

  return segment;
}

gxDatabaseSegment *gxDatabaseCache::FindSegment(FAU_t ds_address)
// Function used to search for a segment by its ds_address.
// Returns a null value if the node could not be found.
{
  gxDatabaseSegment *segment = head;

  // PC-lint 04/21/2004: Possible use of null pointer
  if(!segment) return 0;

  do {
    if(segment->ds_address == ds_address) return segment;
    segment = segment->next;
  } while(segment != head);

  return 0;
}

gxDatabaseSegment *gxDatabaseCache::DemoteSegment(gxDatabaseSegment *segment)
// Demote this segment to the end of list to become the
// least recently reserved segment.
{
  if(IsEmpty()) return 0;

  if(segment == tail) { // This is the tail segment
    return segment;
  }
  else if(segment == head) { // This is the head segment
    if(segment) { // PC-lint 04/21/2004: Possible use of null pointer
      segment->next->prev = segment;
      head = segment->next;
    }
  }
  else { // We are some where in the middle of the list
    segment->prev->next = segment->next;
    segment->next->prev = segment->prev;
  }
  // PC-lint 04/21/2004: Possible use of null pointer
  if(segment) {
    segment->next = head;
    segment->prev = tail;
  }
  if(tail) tail->next = segment;
  tail = segment;
  return segment;
}

gxDatabaseSegment *gxDatabaseCache::PromoteSegment(gxDatabaseSegment *segment)
// Promote this segment to the front of list to become the
// most recently reserved segment.
{
  if(IsEmpty()) return 0;

  // PC-lint 04/21/2004: Possible use of null pointer
  if(!segment) return 0;

  if(segment == head) { // This is the head segment
    return segment;
  }
  if(segment == tail) { // This is the tail segment
    tail = segment->prev;
    // PC-lint 04/21/2004: Possible use of null pointer
    if(head) head->prev = segment;
    head = segment;
    if(head) head->prev = tail;
  }
  else if(segment != head) { // We are some where in the middle of the list
    if(segment) { // PC-lint 04/21/2004: Possible use of null pointer
      segment->prev->next = segment->next;
      segment->next->prev = segment->prev;
      // Move the segment to the front of the list
      segment->next = head;
      segment->prev = tail;
    }
    // PC-lint 04/21/2004: Possible use of null pointer
    if(tail) tail->next = segment;
    if(head) head->prev = segment;
    head = segment;
  }
  return segment;
}

gxDatabaseSegment *gxDatabaseCache::AllocSegment(FAU_t ds_address, 
						 __ULWORD__ bytes)
// Connects a cache segment to the specified node and reads the node
// from disk if the "bytes" variable is greater than zero and the 
// segment address matches the "ds_address" variable. Returns a pointer
// to the segment or a null value if an error occurred. NOTE: If the 
// the node already exists in the cache this function will return a
// pointer to the segment.
{
  if(ds_address == (FAU_t)-1) return 0; 

  // Look for this node in the cache
  gxDatabaseSegment *segment = FindSegment(ds_address);

  if(segment == 0) { // This node is not in the cache
    uncached_reads++;
    segment = FindEmptySegment();
    if(segment) {
      segment->ds_address = ds_address;
      if((bytes > (__ULWORD__)0) && (ds_address != (FAU_t)-1)) {
	if(!f) return 0; // Not connected to a database file	
	if(segment->Read(f, bytes) != gxDBASE_NO_ERROR) {
	  return 0; // Error reading the node from disk
	}
      }
    }
    else {
      return 0; // Could not find an empty segment
    }
  }
  else {
    PromoteSegment(segment);
    cached_reads++;
  }

  return segment;
}

unsigned gxDatabaseCache::SegmentsInUse()
// Returns the total number of dirty segments.
{
  if(IsEmpty()) return 0;
    
  gxDatabaseSegment *segment = head;
  unsigned num_dirty = 0;

  // PC-lint 04/21/2004: Possible use of null pointer
  if(!segment) return 0;

  do {
    if(segment->IsDirty()) num_dirty++;
    segment = segment->next;
  } while(segment != head);

  return num_dirty;
}

gxDatabaseSegment *gxDBCacheNode::CachedWrite(const void *buf, 
					      __ULWORD__ bytes, 
					      FAU_t address)
// Write file data into a cache node equal to the number of bytes
// specified. Returns a pointer to the cache segment or a null 
// pointer if an error occurs. NOTE: The file address must point
// to file offset where the data will be written.
{
  segment = cache->AllocSegment(address, (__ULWORD__)0); 
  if(segment) {
    ds_address = address;
    segment->ds.Load(buf, bytes);
    segment->SetDirty();
  }
  else {
    ds_address = (FAU_t)-1;
  }
  return segment;
}

gxDatabaseSegment *gxDBCacheNode::CachedRead(FAU_t address, __ULWORD__ bytes)
// Read file data into a cache node equal to the number of bytes
// specified. Returns a pointer to the cache segment or a null 
// pointer if an error occurs.
{
  segment = cache->AllocSegment(address, bytes); 
  if(segment) {
    ds_address = address;
  }
  else {
    ds_address = (FAU_t)-1;
  }
  return segment;
}

int gxDBCacheNode::Delete()
// Function used to delete the cache node this caches node is
// connected to. Returns true if the node was deleted or false
// if an error occurs.
{
  if(!segment) return 0; // Not pointing to a valid segment
  if(!cache->gxFilePtr()) return 0; // Not connected to a database file

  if(segment->ds_address == ds_address) { // Pointing to valid segment
    segment->ResetSegment();   // Reset the segment
    ds_address = (FAU_t)-1; // Reset the cache node address  
    cache->DemoteSegment(segment);
  }
  else {
    return 0;
  }
  return 1;
}

gxDatabaseSegment *gxDBCacheNode::Bind()
// Function used to bind this cache node to a cache segment 
// and load the file data residing at the node address. 
// Returns a pointer to the segment.
{
  if(segment) {
    if(segment->ds_address == ds_address) { // Pointing to valid segment
      return segment;
    }
  }

  // If this pointer is not assigned to a segment or the segment is locked
  // connect the segment and load the node
  segment = cache->AllocSegment(ds_address, 1);
  if(!segment) { // Could not allocate a cache segment
    // Reset this segments node address to signal that this segment
    // has not been assigned. This allows the application to test
    // node address before using this cache pointer.
    ds_address = (FAU_t)-1; 
  }

  // Return a pointer to this nodes segment
  return segment;
}

#endif // __USE_DATABASE_CACHE__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //

