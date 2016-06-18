// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Header File Name: gxsfile.h
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

The gxsFile class is used to move data to and from disk
files using the device cache.
*/
// ----------------------------------------------------------- // 
#ifndef __GXS_FILE_HPP__
#define __GXS_FILE_HPP__

#include "gxdlcode.h"

#include <stdlib.h>
#include "devcache.h"
#include "membuf.h"
#include "memblock.h"
#include "gxstream.h"
#include "gxdatagm.h"
#include "scomserv.h"
#include "dfileb.h"

class gxsFile : public gxDeviceCache
{
public:
  gxsFile(int CacheSize = 1024);
  ~gxsFile() { }

public:
  void DeviceErrorHandler(const char *mesg);
  void Flush(); // Flush the cache buckets to the output device
  unsigned BucketsInUse() { return cache.BucketsInUse(); }
  void CloseOutputFile();
  void CloseInputFile();
  int OpenInputFile(const char *in);
  int OpenOutputFile(const char *out);
  FAU_t LoadFile(gxDeviceCachePtr p, FAU_t end_of_file);
  int CopyFileToFile(FAU_t &byte_count);
  int CopyFileToStreamSocket(gxStream *s, FAU_t &byte_count);
  int CopyFileToDatagramSocket(gxDatagram *s, FAU_t &byte_count);
  int CopyFileToSerialPort(gxSerialCommServer *s, FAU_t &byte_count);
  int CopyStreamSocketToFile(gxStream *s, gxBlockHeader &gxs);
  int CopyDatagramSocketToFile(gxDatagram *s, gxBlockHeader &gxs);
  int CopySerialPortToFile(gxSerialCommServer *s, gxBlockHeader &gxs);

private: // Base class interface
  void Read(void *buf, unsigned Bytes, gxDeviceTypes dev);
  void Write(const void *buf, unsigned Bytes, gxDeviceTypes dev);

private: // Device objects
  DiskFileB outfile;               // File used to output data
  DiskFileB infile;                // File used to input data
  gxStream *stream_sock;           // Stream socket (network device) 
  gxDatagram *datagram_sock;       // Datagram socket (network device)
  gxSerialCommServer *serial_port; // Serial port (RS232 device)

private: // Temporary variables used during read/write operations
  gxBlockHeader curr_stream_hdr;
  gxBlockHeader curr_datagram_hdr;
  gxBlockHeader curr_serial_hdr;

private: // Device cache
  gxDeviceBucketCache cache;

public: // Functions used to get the current device cache
  gxDeviceBucketCache *GetCache() { return &cache; }
};

#endif // __GXS_FILE_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
