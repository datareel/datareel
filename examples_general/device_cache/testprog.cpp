// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: testprog.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
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

This is a test program for the device cache classes.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include <sys/types.h>
#include <sys/stat.h>
#include "dfileb.h"
#include "devcache.h"
#include "memblock.h"
#include "membuf.h"

#ifdef __MSVC_DEBUG__
#include "leaktest.h"
#endif

// Example class used to create the device cache
class gxFileCache : public gxDeviceCache
{
public:
  gxFileCache(unsigned CacheSize = 1024);
  ~gxFileCache() { }

public:
  void Flush() { cache.Flush(); } // Flush the cache buckets
  unsigned BucketsInUse() { return cache.BucketsInUse(); }
  void CloseOutputFile() { infile.df_Close(); }
  void CloseInputFile() { outfile.df_Close(); }
  int OpenInputFile(const char *in);
  int OpenOutputFile(const char *out);
  unsigned LoadFile(gxDeviceCachePtr p);
  int CopyFileToFile(const char *in, const char *out, int *bc = 0);

private: // Base class interface
  void Read(void *buf, unsigned Bytes, gxDeviceTypes dev);
  void Write(const void *buf, unsigned Bytes, gxDeviceTypes dev);

private: // Device objects
  DiskFileB outfile; // File used to output data
  DiskFileB infile;  // File used to input data
  
private: // Device cache
  gxDeviceBucketCache cache;

public: // Functions used to get the current device cache
  gxDeviceBucketCache *GetCache() { return &cache; }
};

gxFileCache::gxFileCache(unsigned CacheSize) : cache(CacheSize) 
{ 
  ready_for_writing = 1; 
  ready_for_reading = 1;
  cache.Connect(this); 
}

void gxFileCache::Read(void *buf, unsigned Bytes, gxDeviceTypes dev) 
{
  switch(dev) {
    case gxDEVICE_DISK_FILE:
      if(!infile) { ready_for_reading = 0; return; }
      else { ready_for_reading = 1; }
      infile.df_Read((char *)buf, Bytes);
      break;

    default:
      break;
  }
}
  
void gxFileCache::Write(const void *buf, unsigned Bytes, gxDeviceTypes dev) 
{
  switch(dev) {
    case gxDEVICE_CONSOLE:
      cout.write((char *)buf, Bytes);
      break;

    case gxDEVICE_DISK_FILE:
      if(!outfile) { ready_for_writing = 0; return; }
      else { ready_for_writing = 1; }
      outfile.df_Write((char *)buf, Bytes);
      break;
      
    default:
      break;
  }
}

int gxFileCache::CopyFileToFile(const char *in, const char *out, int *bc)
{
  if(!OpenInputFile(in)) return 0;
  if(!OpenOutputFile(out)) return 0;

  if(!cache) return 0;
  
  gxDeviceTypes o_device = gxDEVICE_DISK_FILE; // Output device
  gxDeviceTypes i_device = gxDEVICE_NULL;      // No input buffering

  // Setup a pointer to the cache buckets
  gxDeviceCachePtr p(cache, o_device, i_device); 
  *bc = LoadFile(p); // Load the file into the cache 
  cache.Flush(); // Ensure all the buckets a written to the output device
  infile.df_Close();
  outfile.df_Close();
  return 1;
}

unsigned gxFileCache::LoadFile(gxDeviceCachePtr p)
// Load a file from disk into the device cache. Returns the
// number bytes read from the file.
{
  unsigned byte_count = 0;
  char sbuf[MEMORY_BLOCK_SIZE];

  infile.df_Rewind();

  while(!infile.df_EOF()) {
    if(infile.df_Read(sbuf, MEMORY_BLOCK_SIZE) != DiskFileB::df_NO_ERROR) {
      if(infile.df_GetError() != DiskFileB::df_EOF_ERROR) {
	break;
      }
    }
    p->Load(sbuf, infile.df_gcount());
    byte_count += infile.df_gcount();
  }

  return  byte_count;
}

int gxFileCache::OpenInputFile(const char *in)
// Open the file if it exists. Returns false
// it the file cannot be opened or if it does
// not exist.
{
  infile.df_Open(in);
  if(!infile) return 0;
  return 1;
}

int gxFileCache::OpenOutputFile(const char *out)
// Open the specified file for writing and truncate
// it. Returns false if the file cannot be opened.
{
  outfile.df_Create(out);
  if(!outfile) return 0;
  return 1;
}

int main(int argc, char **argv)
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

  if(argc < 3) {
    cout << "\n";
    cout << "Usage: " << argv[0] << " infile outfile" << "\n";
    cout << "\n";
    return 0;
  }

  char *in = argv[1];
  char *out = argv[2];

  int cache_size = 1024;
  gxFileCache dev(cache_size); // Device cache used to process a file

  cout << "Creating a device cache using " << cache_size 
       << " cache buckets." << "\n";
  cout << "Reserving " << (sizeof(gxDeviceBucket) * cache_size) 
       << " bytes of memory." << "\n";

  int byte_count;
  
  if(!dev.CopyFileToFile(in, out, &byte_count)) {
    cout << "Error copying file." << "\n";
    return 0;
  }

  cout << "Finished processing file." << "\n";
  cout << "Byte count = " << byte_count << "\n";

  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
