// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: devcache.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
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

The DeviceCache class is an abstract base class used to define
the methods needed to interface a device with memory cache.
The derived class determines the type of device the cache will
be linked to: a file, TCP/IP socket, serial port, etc. The derived
class is also responsible for defining the methods used to read
and write to and from the device as well as keep track of the
device status. The device cache will buffer a stream of binary
data as is it is written to or read from the device the derived
class is using.
*/
// ----------------------------------------------------------- //   
#include "gxdlcode.h"

#include "devcache.h"

gxDeviceCache::~gxDeviceCache()
{
  // Virtual destructor cannot be inlined
}

void gxDeviceBucket::FlushBucket(gxDeviceCache &dev)
// Flush this cache bucket to the specified device and
// reset the dirty flag.
{
  if(o_device && is_dirty) {
    WriteBucket(dev);
    is_dirty = 0;
  }
} 

void gxDeviceBucket::ReadBucket(gxDeviceCache &dev, unsigned bytes)
// Read a specified number of bytes from this device. If a zero byte
// size is specified the bucket will be locked and the bucket data
// must be loaded manually.
{
  is_dirty = 1; // Signal that this bucket is being modified
  MBTYPE *t = (MBTYPE *)this;
  if(t->length() != bytes) t->resize(bytes);
  if(bytes == 0) {
    // A zero byte size was specified, so reserve a bucket
    // zero bytes long and return without preforming a read
    // operation.
    return; 
  }
  dev.Read((char *)t->m_buf(), t->length(), i_device);
}

void gxDeviceBucket::WriteBucket(gxDeviceCache &dev)
// Write the contents of this cache bucket to the
// specified device.
{
  MBTYPE *t = (MBTYPE *)this;
  if(t->length() == 0) { // There are no bytes to write
    is_dirty = 0; // Reset the dirty bit
    return;
  }
  dev.Write((char *)t->m_buf(), t->length(), o_device);
  is_dirty = 0; // Signal that this bucket is empty and can be reused
}

void gxDeviceBucketCache::InitCache()
// Internal processing function used to initialize the cache variables
// and setup a circularly linked-list of buckets using a previously
// allocated array of cache buckets.
{
  if(IsEmpty()) return; // No memory was allocated for the cache buckets

  gxDeviceBucket *p = buckets; 
  unsigned bucket_size = sizeof(gxDeviceBucket);
  
  // Organize the buckets in a circularly linked-list. 
  for(unsigned i = 0; i < num_buckets; i++) {
    p->ResetBucket();
    // PC-lint 05/25/2002: Pointer-to-pointer conversion. This 
    // lint error can be ignored because this is a contiguous
    // array not a non-contiguous linked-list.
    p->prev = (gxDeviceBucket *)((char *)p - bucket_size);
    p->next = (gxDeviceBucket *)((char *)p + bucket_size);
    p = p->next;
  }

  tail = (gxDeviceBucket *)((char *)buckets + (num_buckets-1)*bucket_size);
  tail->next = buckets;
  buckets->prev = tail;
  head = buckets;
}

gxDeviceBucket *gxDeviceBucketCache::ConstructCache(unsigned num)
// Constructs and initialize and array of buckets. NOTE: This
// implementation uses contiguous memory for the cache buckets.
// Returns a pointer to the bucket array or a null value if an
// error occurs.
{
  if(!IsEmpty()) DestroyCache();
  buckets = new gxDeviceBucket[num]; // Construct an array of buckets
  num_buckets = num;
  if(buckets) InitCache();
  return buckets;
}

void gxDeviceBucketCache::DestroyCache()
// Flushes all the cache buckets and frees the memory allocated for the
// cache bucket array.
{
  if(devptr) Flush();
  Disconnect();
  if(buckets) delete[] buckets;
  buckets = 0;
}

void gxDeviceBucketCache::PromoteBucket(gxDeviceBucket *b)
// Promote this bucket to the front of list to become the
// most recently reserved bucket.
{
  if(IsEmpty()) return;
  
  if(b == head->prev) { // This is the tail bucket
     head = head->prev;
  }
  else if(b != head) {
    // Detach the bucket
    b->prev->next = b->next;
    b->next->prev = b->prev;

    // Move the bucket to the front of the list
    b->next = head;
    b->prev = head->prev;
    b->prev->next = b;
    head->prev = b;
    head = b;
  }
}

int gxDeviceBucketCache::Flush()
// Flushes all buckets in the cache. Returns true if successful
// or false if an error occurs.
{
  if(IsEmpty()) return 0;
  
  // Flush, starting at the tail so that cache will be flushed
  // in a first in, first out sequence (FIFO)
  gxDeviceBucket *b = head->prev; 

  while(1) { // PC-lint 05/24/2002: while(1) loop
    if(devptr) { // PC-lint 05/24/2002: Possible use of null pointer
      if(devptr->ReadyForWriting()) {
	// Flush if bucket is used and the output device is ready
	b->FlushBucket(*devptr);
      }
      else {
	return 0; // Error writing to the output device
      }
    }
    else {
      return 0; // Null device pointer
    }
    b = b->prev;
    if(b == head->prev) break; // Reached the end of the list
  }
  return 1; // No error reported
}

int gxDeviceBucketCache::rFlush()
// Flushes all buckets in reverse order. Returns true if successful
// or false if an error occurs.
{
  if(IsEmpty()) return 0;
  
  // Flush, starting at the front so that cache will be flushed
  // in a first in, last out sequence
  gxDeviceBucket *b = head;
  do {
    if(devptr) { // PC-lint 05/24/2002: Possible use of null pointer
      if(devptr->ReadyForWriting()) {
	// Flush if bucket is used and the output device is ready
	b->FlushBucket(*devptr);
      }
      else {
	return 0; // Error writing to the output device
      }
    }
    else {
      return 0; // Null device pointer
    }
    b = b->next;
  } while(b != head);

  return 0;
}

void gxDeviceBucketCache::Clear()
// Flush all buckets in the cache.
{
  if(devptr) Flush();
}

gxDeviceBucket *gxDeviceBucketCache::FindEmptyBucket()
// Internal processing function used to find the last
// bucket in the cache that is empty and promote the
// bucket to the front of the list. Returns a pointer
// to the bucket or a null value if no buckets can be
// made available or an error occurs. If the cache is
// full the least-recently reserved bucket will be
// flushed and re-allocated to prevent the cache from
// filling up.
{
  if(IsEmpty()) return 0;
  
  gxDeviceBucket *b = head->prev; // Least-recently reserved bucket
  while(1) { // PC-lint 05/24/2002: while(1) loop
 
    // Found an empty bucket in the cache 
    if(!b->IsDirty()) break;

    // Keep searching for the last empty bucket
    b = b->prev;
    
    // The cache is full, so use the last bucket
    if(b == head->prev) {
      if(devptr) { // PC-lint 05/24/2002: Possible use of null pointer
	if(devptr->ReadyForWriting()) {
	  // Flush the last bucket and move it to the head of the list
	  b->FlushBucket(*devptr);
	}
	else { 
	  return 0; // Error writing to the output device
	}
      }
      else {
	return 0; // Null device pointer
      }
      PromoteBucket(b); // Now this becomes the most recently used bucket
      return b;          
    }  
  }

  return b;
}

gxDeviceBucket *gxDeviceBucketCache::AllocBucket(gxDeviceTypes o_dev,
						 gxDeviceTypes i_dev,
						 unsigned bytes)
// Connects a cache bucket to the specified I/O device and read a
// specified number of bytes from the input device. Returns a pointer
// to the bucket or a null value if an error occurred.
{
  gxDeviceBucket *b;
  b = FindEmptyBucket();
  if(b) {
    b->o_device = o_dev;
    b->i_device = i_dev;
    if(devptr) { // PC-lint 05/24/2002: Possible use of null pointer
      if(devptr->ReadyForReading()) {
	// Read data into the bucket if the device is ready for reading
	b->ReadBucket(*devptr, bytes); 
      }
      else {
	b = 0; // Error reading from the device
      }
    }
    else {
      b = 0; // Null device pointer
    }
  }
  return b;
}

unsigned gxDeviceBucketCache::BucketsInUse()
// Returns the total number of dirty buckets.
{
  if(IsEmpty()) return 0;
  
  gxDeviceBucket *b = head;
  unsigned num_dirty = 0;
  do {
    if(b->IsDirty()) num_dirty++;
    b = b->next;
  } while(b != head);
  return num_dirty;
}

gxDeviceCachePtr::gxDeviceCachePtr(gxDeviceBucketCache &c,
				   gxDeviceTypes o_dev,
				   gxDeviceTypes i_dev)
// Constructs a cache pointer and connects it a previously
// constructed cache object.
{
  o_device = o_dev;
  i_device = i_dev;
  bucket = 0;
  cache = &c;
}

gxDeviceCachePtr::gxDeviceCachePtr(const gxDeviceCachePtr &ob)
{
  o_device = ob.o_device;
  i_device = ob.i_device;

  // PC-lint 05/25/2002: This direct pointer copy is safe
  // since the cache pointer is not deleted by this class.
  cache = ob.cache;
  bucket = 0; // Force this pointer to reference another bucket
}

gxDeviceCachePtr& gxDeviceCachePtr::operator=(const gxDeviceCachePtr &ob)
{
  if(this != &ob) { // PC-lint 05/25/2002: Prevent self assignment 
    o_device = ob.o_device;
    i_device = ob.i_device;

    // PC-lint 05/25/2002: This direct pointer copy is safe
    // since the cache pointer is not deleted by this class.
    cache = ob.cache;
    bucket = 0; // Force this pointer to reference another bucket
  }
  return *this;
}

gxDeviceBucket *gxDeviceCachePtr::AllocBucket(gxDeviceTypes o_dev,
					      gxDeviceTypes i_dev, 
					      unsigned buffer_size)
// Allocates a cache bucket during a read operation and returns 
// a pointer to the bucket or a null value if an error occurred.
// The Alloc function will load data into the bucket unless a
// zero byte size is specified bucket for the "buffer_size"
// variable. The  "buffer_size" variable is used to determine
// the size of memory buffer needed to store this bucket's data.
{
  Release(); // Release the bucket this pointer is currently holding
  bucket = cache->AllocBucket(o_dev, i_dev, buffer_size);
  if(bucket) {
    o_device = o_dev;
    i_device = i_dev;
  }
  else {
    // Could not allocate a bucket so point to a null device
    o_device = gxDEVICE_VOID;
    i_device = gxDEVICE_VOID;
  }
  return bucket;
}

void gxDeviceCachePtr::Connect()
// Internal processing function used to connect this pointer
// to a cache bucket of the correct type.
{
  if(bucket) {
    if((bucket->o_device == o_device) && (bucket->i_device == i_device)) { 
      if((o_device != gxDEVICE_VOID) && (i_device != gxDEVICE_VOID)) {
	// If this is the correct bucket type and the bucket is pointing
	// to a valid I/O device ensure that the bucket is not dirty
	// before connecting it to this pointer.
	if(!bucket->IsDirty()) return; 
      }
    }
  }

  // Connect the bucket without reading any data
  bucket = cache->AllocBucket(o_device, i_device);
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
