// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxsfile.cpp
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

The gxsFile class is used to move data to and from disk
files using the device cache.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include <sys/types.h>
#include <sys/stat.h>
#include "gxsfile.h"

gxsFile::gxsFile(int CacheSize) : cache(CacheSize) 
{ 
  ready_for_writing = ready_for_reading = 1;
  cache.Connect(this); 
  stream_sock = 0;
  datagram_sock = 0;
  serial_port = 0;
}

void gxsFile::Read(void *buf, unsigned Bytes, gxDeviceTypes dev) 
{
  switch(dev) {
    case gxDEVICE_DISK_FILE:
      if(!infile) { ready_for_reading = 0; return; }
      else { ready_for_reading = 1; }
      if(infile.df_Read((char *)buf, Bytes) != DiskFileB::df_NO_ERROR) {
	DeviceErrorHandler(infile.df_ExceptionMessage());
      }
      break;
      
    case  gxDEVICE_STREAM_SOCKET:
      if(!stream_sock) { ready_for_reading = 0; return; }
      else { ready_for_reading = 1; }
      if(stream_sock->ReadRemoteBlock((char *)buf, curr_stream_hdr) != 0)
	DeviceErrorHandler(stream_sock->SocketExceptionMessage());
      break;

    case gxDEVICE_DATAGRAM_SOCKET:
      if(!datagram_sock) { ready_for_reading = 0; return; }
      else { ready_for_reading = 1; }
      if(datagram_sock->ReadRemoteBlock((char *)buf, curr_datagram_hdr) != 0)
	DeviceErrorHandler(datagram_sock->SocketExceptionMessage());
      break;
      
    case gxDEVICE_SERIAL_PORT:
      if(!serial_port) {  ready_for_reading = 0; return; }
      else { ready_for_reading = 1; }
      if(serial_port->ReadBlock((char *)buf, curr_serial_hdr) != 0)
	DeviceErrorHandler(serial_port->SerialCommExceptionMessage());
      break;

    case gxDEVICE_NULL:
      break;
      
    default:
      break;
  }
}

void gxsFile::Write(const void *buf, unsigned Bytes, gxDeviceTypes dev) 
{
  switch(dev) {
    case gxDEVICE_CONSOLE:
      GXSTD::cout.write((char *)buf, Bytes);
      break;

    case gxDEVICE_DISK_FILE:
      if(!outfile) { ready_for_writing = 0; return; }
      else { ready_for_writing = 1; }
      if(outfile.df_Write((char *)buf, Bytes) != DiskFileB::df_NO_ERROR) {
	DeviceErrorHandler(outfile.df_ExceptionMessage());
      }
      break;

    case  gxDEVICE_STREAM_SOCKET: 
      if(!stream_sock) { ready_for_writing = 0; return; }
      else { ready_for_writing = 1; }
      if(stream_sock->WriteBlock((char *)buf, Bytes) != 0)
	DeviceErrorHandler(stream_sock->SocketExceptionMessage());
      break;
      
    case gxDEVICE_DATAGRAM_SOCKET:
      if(!datagram_sock) { ready_for_writing = 0; return; }
      else { ready_for_writing = 1; }
      if(datagram_sock->WriteBlock((char *)buf, Bytes) != 0)
	DeviceErrorHandler(datagram_sock->SocketExceptionMessage());
      break;

    case gxDEVICE_SERIAL_PORT: 
      if(!serial_port) { ready_for_writing = 0; return; }
      else { ready_for_writing = 1; }
      if(serial_port->WriteBlock((char *)buf, Bytes) != 0)
	DeviceErrorHandler(serial_port->SerialCommExceptionMessage());
      break;

    case gxDEVICE_NULL:
      break;
      
    default:
      break;
  }
}

int gxsFile::CopyFileToStreamSocket(gxStream *s, FAU_t &byte_count)
// Copy a disk file to a stream socket. Returns true if no errors
// occurred. NOTE: This function assumes that file has been open
// by the calling OpenInputFile/OpenOutputFile function and the
// socket has already been initialized. The calling function is
// responsible for flushing the cache buffers and closing any
// open disk files.
{
  if(!infile) return 0;
  if(!s) return 0;
  stream_sock = s;

  gxDeviceTypes o_device = gxDEVICE_STREAM_SOCKET; // Output device
  gxDeviceTypes i_device = gxDEVICE_DISK_FILE;     // Input device

  // Setup a pointer to the cache buckets
  gxDeviceCachePtr p(cache, o_device, i_device); 

  // Load the file into the cache 
  byte_count = LoadFile(p, infile.df_Length());

  return 1;
}

int gxsFile::CopyFileToDatagramSocket(gxDatagram *s, FAU_t &byte_count) 
// Copy a disk file to a datagram socket. Returns true if no errors
// occurred. NOTE: This function assumes that file has been open
// by the calling OpenInputFile/OpenOutputFile function and the
// socket has already been initialized. The calling function is
// responsible for flushing the cache buffers and closing any
// open disk files.
{
  if(!infile) return 0;
  if(!s) return 0;
  datagram_sock = s;

  gxDeviceTypes o_device = gxDEVICE_DATAGRAM_SOCKET; // Output device
  gxDeviceTypes i_device = gxDEVICE_DISK_FILE; 

  // Setup a pointer to the cache buckets
  gxDeviceCachePtr p(cache, o_device, i_device); 

  // Load the file into the cache 
  byte_count = LoadFile(p, infile.df_Length());

  return 1;
}

int gxsFile::CopyFileToSerialPort(gxSerialCommServer *s, FAU_t &byte_count)
// Copy a disk file to a serial port. Returns true if no errors
// occurred. NOTE: This function assumes that file has been open
// by the calling OpenInputFile/OpenOutputFile function and the
// port has already been initialized. The calling function is
// responsible for flushing the cache buffers and closing any
// open disk files.
{
  if(!outfile) return 0;
  if(!s) return 0;
  serial_port = s;

  gxDeviceTypes o_device = gxDEVICE_SERIAL_PORT; // Output device
  gxDeviceTypes i_device = gxDEVICE_DISK_FILE; 

  // Setup a pointer to the cache buckets
  gxDeviceCachePtr p(cache, o_device, i_device); 

  // Load the file into the cache 
  byte_count = LoadFile(p, infile.df_Length());

  return 1;
}

int gxsFile::CopyFileToFile(FAU_t &byte_count)
// Copy a disk file to another disk file . Returns true if no errors
// occurred. NOTE: This function assumes that files have been open
// by the calling OpenInputFile/OpenOutputFile function. The calling
// function is responsible for flushing the cache buffers and closing
// any open disk files.

{
  if(!infile) return 0;
  if(!outfile) return 0;
  
  gxDeviceTypes o_device = gxDEVICE_DISK_FILE; // Output device
  gxDeviceTypes i_device = gxDEVICE_DISK_FILE; 

  // Setup a pointer to the cache buckets
  gxDeviceCachePtr p(cache, o_device, i_device); 

  // Load the file into the cache 
  byte_count = LoadFile(p, infile.df_Length());
  return 1;
}

FAU_t gxsFile::LoadFile(gxDeviceCachePtr p, FAU_t end_of_file)
// Load a file from disk into the device cache. Returns the
// number bytes read from the file.
{
  FAU_t byte_count = (FAU_t)0;

  // The file is smaller then one block
  if(end_of_file <= (FAU_t)MEMORY_BLOCK_SIZE) { 
    byte_count = end_of_file;
    p.Alloc((unsigned)end_of_file); // Load the cache bucket
  }
  else { // The file is larger then one block
    while(end_of_file > (FAU_t)MEMORY_BLOCK_SIZE) {
      byte_count += MEMORY_BLOCK_SIZE;
      p.Alloc(MEMORY_BLOCK_SIZE); // Load the cache bucket
      end_of_file -= MEMORY_BLOCK_SIZE;
      if(end_of_file <= (FAU_t)MEMORY_BLOCK_SIZE) {
	byte_count += end_of_file;
	p.Alloc((unsigned)end_of_file); // Load the cache bucket
	break;
      }
      else
	continue;
    }
  }
  return  byte_count;
}

int gxsFile::OpenInputFile(const char *in)
// Open the file if it exists. Returns false
// it the file cannot be opened or if it does
// not exist.
{
  infile.df_Open(in);
  if(!infile) return 0;
  return 1;
}

int gxsFile::OpenOutputFile(const char *out)
// Open the specified file for writing and truncate
// it. Returns false if the file cannot be opened.
{
  outfile.df_Create(out);
  if(!outfile) return 0;
  return 1;
}

int gxsFile::CopyStreamSocketToFile(gxStream *s, gxBlockHeader &gxs)
{
  if(!s) return 0; 
  stream_sock = s;
  curr_stream_hdr = gxs;

  gxDeviceTypes o_device = gxDEVICE_DISK_FILE;     // Output device
  gxDeviceTypes i_device = gxDEVICE_STREAM_SOCKET; // Input device

  // Setup a pointer to the cache buckets
  gxDeviceCachePtr p(cache, o_device, i_device); 

  // Load the cache buckets
  p.Alloc((__ULWORD__)gxs.block_length);

  return 1;
}

int gxsFile::CopyDatagramSocketToFile(gxDatagram *s, gxBlockHeader &gxs)
{
  if(!s) return 0; 
  datagram_sock = s;
  curr_datagram_hdr = gxs;

  gxDeviceTypes o_device = gxDEVICE_DISK_FILE;       // Output device
  gxDeviceTypes i_device = gxDEVICE_DATAGRAM_SOCKET; // Input device

  // Setup a pointer to the cache buckets
  gxDeviceCachePtr p(cache, o_device, i_device); 

  // Load the cache buckets
  p.Alloc((__ULWORD__)gxs.block_length);

  return 1;
}

void gxsFile::CloseOutputFile()
{
  outfile.df_Close();
}

void gxsFile::CloseInputFile()
{
  infile.df_Close();
}

void gxsFile::Flush()
{
  cache.Flush();
}

int gxsFile::CopySerialPortToFile(gxSerialCommServer *s, gxBlockHeader &gxs)
{
  if(!outfile) return 0;
  if(!s) return 0;
  serial_port = s;
  curr_serial_hdr = gxs;

  gxDeviceTypes o_device = gxDEVICE_DISK_FILE;   // Output device
  gxDeviceTypes i_device = gxDEVICE_SERIAL_PORT; // Input device 

  // Setup a pointer to the cache buckets
  gxDeviceCachePtr p(cache, o_device, i_device); 

  // Load the cache buckets
  p.Alloc((__ULWORD__)gxs.block_length);

  return 1;
}

void gxsFile::DeviceErrorHandler(const char *mesg)
{
  // Display the device error and exit the program
  GXSTD::cout << mesg << "\n";
  exit(0);
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
