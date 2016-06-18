// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: devcache.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
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
#ifndef __GX_DEVICE_CACHE_HPP__
#define __GX_DEVICE_CACHE_HPP__

#include "gxdlcode.h"

#include <stddef.h>
#include "devtypes.h"

// General definition of the bucket data type.
// Each data type is set by a conditional directive that allows the
// device cache classes to be used for several data types. Each type
// must be determined at compile time and only one type can be used
// for each compile. Variable length cache buckets will be used by
// default.
#if defined(__SMEMORY_BLOCK__) // Fixed length memory (static) blocks
#include "memblock.h"
typedef MemoryBlock MBTYPE; 

#elif defined(__VMEMORY_BUFFER__) // Variable length memory buffers
#include "membuf.h"
typedef MemoryBuffer MBTYPE; 

#else // Default to variable length memory buffers
#include "membuf.h"
typedef MemoryBuffer MBTYPE; 
#endif

class GXDLCODE_API gxDeviceCache
{
public:
  gxDeviceCache() { }
  virtual ~gxDeviceCache();
  
public: // Derived class interface
  virtual void Read(void *buf, unsigned Bytes, gxDeviceTypes dev) = 0;
  virtual void Write(const void *buf, unsigned Bytes, gxDeviceTypes dev) = 0;

public:
  int ReadyForWriting() { return ready_for_writing; }
  int ReadyForWriting() const { return ready_for_writing; }
  int ReadyForReading() { return ready_for_reading; }
  int ReadyForReading() const { return ready_for_reading; }

protected: // Device flags set by the derived class
  int ready_for_writing;
  int ready_for_reading;
};

class GXDLCODE_API gxDeviceBucket: public MBTYPE
// The gxDeviceBucket class is used to define how each bucket will
// be loaded/stored and which device will be used for every input/output
// operation. gxDeviceBucket objects are organized into a circular doubly
// linked list by the gxDeviceBucketCache class.
{
public:
  gxDeviceBucket() {
    is_dirty = 0; o_device = i_device = gxDEVICE_VOID;
    prev = next = 0;
  } 
  ~gxDeviceBucket() { }

private: // Disallow copying and assignment
  gxDeviceBucket(const gxDeviceBucket &ob) { }
  void operator=(const gxDeviceBucket &ob) { }
  
public: // Cache functions
  void FlushBucket(gxDeviceCache &dev);
  void ReadBucket(gxDeviceCache &dev, unsigned bytes = 0);
  void WriteBucket(gxDeviceCache &dev);

public: // Bucket functions
  void ResetBucket() {
    is_dirty = 0; o_device = i_device = gxDEVICE_VOID;
  }
  int IsDirty() { return is_dirty == 1; }
  int IsDirty() const { return is_dirty == 1; }
  
private:
  // True when the bucket is in use or false when empty.
  // The "is_dirty" flag locks the bucket following a
  // read operation and unlocks the bucket following a
  // write operation.
  char is_dirty; 

public:
  gxDeviceTypes o_device; // Output device this bucket is connected to
  gxDeviceTypes i_device; // Input device this bucket is connected to
  gxDeviceBucket *prev;   // Previous bucket in the list
  gxDeviceBucket *next;   // Next bucket in the list
};

class GXDLCODE_API gxDeviceBucketCache
// The gxDeviceBucketCache class is used to handle the allocation and
// de-allocation of buckets. The cache must determine whether an object
// is already loaded. If the object is not loaded the cache reserves a
// bucket and loads the object into memory.
{
public:
  gxDeviceBucketCache(unsigned num) {
    if(num == 0) num = 1; devptr = 0; buckets = 0; ConstructCache(num);
  }
  ~gxDeviceBucketCache() { DestroyCache(); }

public:
  gxDeviceBucket *ConstructCache(unsigned num);
  void DestroyCache();
  int IsEmpty() { return buckets == 0; }
  int IsEmpty() const { return buckets == 0; }
  void Connect(gxDeviceCache *dev) { if(devptr) Clear(); devptr = dev; } 
  void Disconnect() { if(devptr) Clear(); devptr = 0; }
  int Flush();
  int rFlush();
  void Clear();
  gxDeviceBucket *AllocBucket(gxDeviceTypes o_dev, gxDeviceTypes i_dev,
			      unsigned bytes = 0);
  gxDeviceBucket *GetHead() { return head; }
  gxDeviceBucket *GetHead() const { return head; } 
  gxDeviceBucket *GetTail() { return tail; }
  gxDeviceBucket *GetTail() const { return tail; } 
  int GetBuckets() { return num_buckets; }
  unsigned BucketsInUse();

private: // Internal processing functions
  void PromoteBucket(gxDeviceBucket *b);
  gxDeviceBucket *FindEmptyBucket();
  void InitCache();
  
public: // Overloaded operators
  int operator!() const { return buckets == 0; }
  int operator!() { return buckets == 0; }
  operator const int () const { return buckets != 0; }
  operator int () { return buckets != 0; }
  
private:
  unsigned num_buckets;    // Total number of buckets allocated
  gxDeviceCache *devptr;   // Device cache is connected to
  gxDeviceBucket *head;    // Most recently reserved bucket
  gxDeviceBucket *tail;    // Least recently reserved bucket
  gxDeviceBucket *buckets; // Array of cache buckets
};

class GXDLCODE_API gxDeviceCachePtr
// Cache pointers connect cache buckets to the cache by storing
// a pointer to the bucket containing the memory buffer of the
// object and a pointer to the cache the cache pointer is connected to.
{
public:
  gxDeviceCachePtr(gxDeviceBucketCache &c, gxDeviceTypes o_dev,
		   gxDeviceTypes i_dev);
  gxDeviceCachePtr(const gxDeviceCachePtr &ob);
  gxDeviceCachePtr& operator=(const gxDeviceCachePtr &ob);
  ~gxDeviceCachePtr() { }

public: 
  MBTYPE *Alloc(unsigned bytes = 0)
  // Reserves a cache bucket and automatically loads a
  // specified number of bytes into the bucket from the
  // input device the bucket is connected to. NOTE: If
  // a zero byte value is specified no data will be read
  // from the input device.
  {
    return (MBTYPE *)((gxDeviceBucket *)(AllocBucket(this->o_device,
						     this->i_device, bytes))); 
  }

  void Release() { bucket = 0; } // Release this bucket
  void SetOutputDevice(gxDeviceTypes dev) { o_device = dev; }
  void SetInputDevice(gxDeviceTypes dev) { i_device = dev; }
  void SetCache(gxDeviceBucketCache *c) { cache = c; }
  gxDeviceTypes GetOutputDevice() { return o_device; }
  gxDeviceTypes GetOutputDevice() const { return o_device; }
  gxDeviceTypes GetInputDevice() { return i_device; }
  gxDeviceTypes GetInputDevice() const { return i_device; }
  

public: // Overloaded operators
  gxDeviceBucket &operator*() {
    // Overloaded indirection operator that allows
    // device cache pointers to de-referenced the
    // bucket this object is currently referencing. 
    Connect(); return *(gxDeviceBucket *)bucket;
  }

  gxDeviceBucket *operator->() {
    // Overloaded arrow operator used to reference
    // a device cache pointer to the bucket it
    // is currently referencing. 
    Connect(); return (gxDeviceBucket *)bucket;
  }

  int operator!() { return bucket == 0; } 
  int operator!() const { return bucket == 0; }
  operator const int () const { return bucket != 0; }
  operator int () { return bucket != 0; }

private: // Internal processing functions
  gxDeviceBucket *AllocBucket(gxDeviceTypes o_dev, gxDeviceTypes i_dev,
			      unsigned buffer_size = 0);
  void Connect();

private:
  gxDeviceTypes o_device;     // Output device this bucket is connected to
  gxDeviceTypes i_device;     // Input device this bucket is connected to
  gxDeviceBucket *bucket;     // Current cache bucket
  gxDeviceBucketCache *cache; // Pointer to this object's cache
};

#endif // __GX_DEVICE_CACHE_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
