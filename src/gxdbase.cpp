// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxdbase.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/04/1997 
// Date Last Modified: 06/05/2016
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

The gxDatabase engine is a low-level file handler used to 
manipulate contiguous blocks of binary data. Low-level methods 
include read, write, and seek operations, create and open functions, 
linear navigation, and large file support. Optional methods include 
CRC checking, file and node locking.

Changes:
==============================================================
10/16/2002: Modified the gxDatabase::FindFirstBlock() function to 
use the TestBlockHeader() function to test for valid blocks.

10/30/2002: Modified the gxDatabase::FindPrevBlock()function to 
use the TestBlockHeader() function to test for valid blocks.

10/30/2002: Modified the gxDatabase::WriteObjectChecksum() and 
gxDatabase::ReadObjectChecksum() functions to use the ReadBlockHdr() 
function to read and verify blocks.

10/30/2002: Modified the Database::ReadBlockHdr() function to use 
TestBlockHeader() function to test for valid blocks. NOTE: This change 
will affect all function that call ReadBlockHdr().

10/30/2002: Modified the following functions to use TestBlockHeader() 
function to test for valid blocks: gxDatabase::TotalBlocks(), 
gxDatabase::DeletedBlocks(), gxDatabase::UnDelete(), 
gDatabase::ReclaimBestFit(), and gxDatabase::ReclaimFirstFit().
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "gxdbase.h"

#ifdef __BCC32__
#pragma warn -8080
#pragma warn -8071
#pragma warn -8072
#endif

// Initialize all static variables and data members
static char gxDefaultFileName[gxMaxNameLength] = "closed.gxd";

// Current database file manager version number.
FAU gxDatabase::gxVersion = gxDatabaseVersionNumber;

// Initialize the database file manager signature and revision letter.
#if defined (__64_BIT_DATABASE_ENGINE__)
__SBYTE__ gxDatabase::gxSignature[gxSignatureSize] = {
  'G', 'X', 'D', 'B', 'A', 'S', 'E', '6', '4', gxDatabaseRevisionLetter
};
#else // Use the 32-bit version by default
__SBYTE__ gxDatabase::gxSignature[gxSignatureSize] = {
  'G', 'X', 'D', 'B', 'A', 'S', 'E', gxDatabaseRevisionLetter
};
#endif

// Current 32-bit synchronization word
gxUINT32 gxDatabase::gxInternalCheckWord = gxCheckWord;

gxDatabase::gxDatabase()
// Creates a database file object.
{
  strcpy(file_name, gxDefaultFileName); // Set the initial file name
  fp = 0;

  // Reset the last reported error and the file status members
  gxd_error = gxDBASE_NO_ERROR;
  is_open = is_ok = 0;
  ready_for_reading = ready_for_writing = 0;
  
  // Always set the revision letter when the object is constructed
  // so that the size functions work correctly.
  rev_letter = gxDatabaseRevisionLetter;

  // PC-lint 09/08/2005: Variable not set in constructor warning.
  // Last operation variable is set during an open or create call.
  last_operation = gxDBASE_SEEK;
}

gxDatabase::~gxDatabase()
{
  // PC-lint 09/08/2005: Function may throw exception in destructor
  Close();
}

void gxDatabase::Release()
// The release function is used to release this object's file pointer  
// without closing the file pointer or performing any other action. 
// NOTE: The release function is used to release this file pointer when  
// more then one object is referencing this file pointer. After calling   
// the release function the object previously bound to this file pointer 
// can safely be deleted without affecting any other objects bound to 
// this file pointer. This allows the file to remain open until all 
// threads accessing this file pointer have exited or released it.
{
  strcpy(file_name, gxDefaultFileName); // Reset the open file name
  fp = 0; 

  // Reset the last reported error and the file status members
  gxd_error = gxDBASE_NO_ERROR;
  is_open = is_ok = 0;
  ready_for_reading = ready_for_writing = 0;
}

const char *gxDatabase::DatabaseExceptionMessage()
// Returns a null terminated string that can
// be used to log or print a database exception.
{
  return gxDatabaseExceptionMessage(gxd_error);
}

gxDatabaseError gxDatabase::Create(const char *fname, FAU_t static_size,
				   __SBYTE__ RevisionLetter)
// Creates a new file and truncate the file if it already exists.
// The "static_size" variable is used to reserve a specified number
// of bytes that will not be affected by the dynamic allocation
// routines. The "RevisionLetter" variable is used to select a
// specific revision letter when the file is created. The revision
// letter is used to enable or disable specific features that will
// determine the amount of overhead per block. Returns a non-zero
// value to indicate an error condition or zero if successful.
{
  // Close any open files 
  if(Close() != gxDBASE_NO_ERROR) return gxd_error;

  // Create and truncate existing files
  fp = gxdFPTRCreate(fname);

  if(fp == 0) {
    is_ok = 0;
    is_open = 0;
    ready_for_writing = 0;
    ready_for_reading = 0;
    gxd_error = gxDBASE_FILE_CREATION_ERROR;
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxd_error;
#endif
  }
  else {
    is_open = 1;
    is_ok = 1;
    ready_for_writing = 1;
    ready_for_reading = 1;
    strcpy(file_name, fname);

    // Set the specified revision letter. NOTE This will default to the
    // current revision unless a lower version is specified.
    SetRevisionLetter(RevisionLetter);
    
    file_header.gxd_hs_fptr = static_size + FileHeaderSize();
    last_operation = gxDBASE_READ;
    if(InitFileHdr() != gxDBASE_NO_ERROR) return gxd_error;
  }
  
  // Returns 0 if the file was successfully created and opened.
  return gxd_error = gxDBASE_NO_ERROR;
}

void gxDatabase::SetRevisionLetter(__SBYTE__ RevisionLetter)
// Set the database revision letter to a specified value. If
// a valid revision is not specified the current revision
// letter will be used. The function is used to detect revision
// letter errors when new database files are created.
{
  // Check the revision letter to ensure a valid revision was specified
  switch(RevisionLetter) {
    case '\0': case ' ': // Database revision zero
      rev_letter = '\0';
      break;
    case 'A': case 'a':  // Persistent checksum revision 
      rev_letter = 'A';
      break;
    case 'B': case 'b':  // Persistent file lock revision
      rev_letter = 'B';
      break;
    case 'C': case 'c':  // Persistent record lock revision
      rev_letter = 'C';
      break;
    case 'D': case 'd':  // Persistent file stats with record locking
      rev_letter = 'D';
      break;
    case 'E': case 'e':  // File stats without persistent CRC or record locking
      rev_letter = 'E';
      break;
    default:
      rev_letter = gxDatabaseRevisionLetter;
      break;
  }
}

gxDatabaseError gxDatabase::InitFileHdr()
// Initialize the file header with default values and write it
// to disk. Returns a non-zero value to indicate an error
// condition or zero if successful.
{
  file_header.gxd_fs_fptr = (FAU_t)0;
  file_header.gxd_eof = file_header.gxd_hs_fptr;
  file_header.gxd_hb_fptr = (FAU_t)0; 
  memcpy(file_header.gxd_sig, gxDatabase::gxSignature, (gxSignatureSize-1)); 
  file_header.gxd_ver = gxDatabase::gxVersion;

  // Set the specified revision letter accroding to current
  // "rev_letter" value;
  file_header.gxd_sig[gxSignatureSize-1] = rev_letter;
  
  // Initialize and the file stats header for rev D files and higher
  InitFileStatsHdr(file_stats_header);

  // Write the file header and flush the disk buffers to
  // maintain file integrity during multiple file access.
  if(WriteFileHdr()!= gxDBASE_NO_ERROR) return gxd_error;

  // Initialize and write the file lock header for rev B files and higher
  gxFileLockHeader lh;
  InitFileLockHdr(lh);
  if(WriteFileLockHdr(lh) != gxDBASE_NO_ERROR) return gxd_error;

  if(file_header.gxd_hs_fptr > (FAU_t)FileHeaderSize()) {
     __SBYTE__ zero_byte = 0;
     if(Write(&zero_byte, 1, file_header.gxd_hs_fptr-1) != gxDBASE_NO_ERROR)
       return gxd_error;
  }

  return gxd_error = gxDBASE_NO_ERROR;
}

gxDatabaseError gxDatabase::Open(const char *fname,
				 gxDatabaseAccessMode mode)
// Open an existing file. The "mode" variable determines if the file
// is opened for read only or read/write access. This function will
// check the revision letter when opening an existing file. Returns a
// non-zero value to indicate an error condition or zero if successful.
// NOTE: This version of the open functions will only accept:
// gxDBASE_READONLY and gxDBASE_READWRITE access modes.
{
  // Close any open files
  if(Close() != gxDBASE_NO_ERROR) return gxd_error;

  if(mode == gxDBASE_READONLY) { // Open with read only access
    ready_for_reading = 1;
    ready_for_writing = 0;
    fp = gxdFPTROpen(fname, gxDBASE_READONLY);
  }
  else { // Open with read/write access
    ready_for_reading = 1;
    ready_for_writing = 1;
    fp = gxdFPTROpen(fname, gxDBASE_READWRITE);
  }
    
  if(fp == 0) {
    ready_for_reading = 0;
    ready_for_writing = 0;
    gxd_error = gxDBASE_FILE_OPEN_ERROR;
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxd_error;
#endif
  }
  else {
    is_open = 1;
    is_ok = 1;
    strcpy(file_name, fname);
    last_operation = gxDBASE_WRITE;
    
    if(ReadFileHdr()!= gxDBASE_NO_ERROR) return gxd_error;

    // Test the file type without checking the revision letter
    if(memcmp(file_header.gxd_sig, gxDatabase::gxSignature,
	      (gxSignatureSize-1))) { 
      gxd_error = gxDBASE_WRONG_FILE_TYPE;
      is_ok = 0;
#ifdef __CPP_EXCEPTIONS__
       throw gxCDatabaseException();
#else
       return gxd_error;
#endif
    }
  }

  // Ensure that true end of file is stored in the file header. 
  FAU_t filesize = FileSize(fname);
  if(filesize != -1) {
    if(file_header.gxd_eof < filesize) {
      file_header.gxd_eof = filesize;
      if(Flush() != gxDBASE_NO_ERROR) return gxd_error;
    }
  }
  
  return gxd_error = gxDBASE_NO_ERROR;
}

gxDatabaseError gxDatabase::Close()
// Close the open database file. Returns a non-zero value
// to indicate an error condition or zero if successful.
{
  if(IsOpen()) {

    // Write the header if this file was opened with write access
    if(ReadyForWriting()) WriteFileHdr();

    if(gxdFPTRClose(fp) != 0) {
      gxd_error = gxDBASE_FILE_CLOSE_ERROR;
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return gxd_error;
#endif
    }
    strcpy(file_name, gxDefaultFileName); // Set the initial file name
  }

  is_open = 0;
  is_ok = 0;
  ready_for_reading = 0;
  ready_for_writing = 0;
  fp = 0; // Set the file pointer to zero after the file is closed

  return gxd_error = gxDBASE_NO_ERROR; 
}

gxDatabaseError gxDatabase::Flush()
// Flush the file header and any open disk buffers. Returns
// a non-zero value to indicate an error condition or zero
// if successful. No error will be returned if the flush 
// function is call for read only files.
{
  if((IsOK()) && (ReadyForWriting())) {
    if(WriteFileHdr() != gxDBASE_NO_ERROR) return gxd_error;
    if(gxdFPTRFlush(fp) != 0) {
      gxd_error = gxDBASE_FILE_WRITE_ERROR;
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return gxd_error;
#endif
    }
    Seek((FAU_t)0, gxDBASE_SEEK_CUR);
  }
  return gxd_error = gxDBASE_NO_ERROR;
}

gxDatabaseError gxDatabase::Seek(FAU_t offset, gxDatabaseSeekMode mode)
// Seek to the specified offset starting at the beginning (SEEK_SET),
// end (SEEK_END) or current offset (SEEK_CUR). Returns a non-zero
// value to indicate an error condition or zero if successful.
{
  if(IsOK()) {
    if(gxdFPTRSeek(fp, offset, mode)  == (FAU_t)-1) {
      gxd_error = gxDBASE_FILE_SEEK_ERROR;
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return gxd_error;
#endif
    }
    last_operation = gxDBASE_SEEK;
  }
  else {
    gxd_error = gxDBASE_FILE_NOT_READY;
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxd_error;
#endif
  }
  return gxd_error = gxDBASE_NO_ERROR;
}

FAU_t gxDatabase::SeekTo(FAU_t file_address)
// Seek to the specified address, optimizing the seek
// operation by moving the file position indicator based
// on the current stream position. Returns the current
// file position after performing the seek operation.
{
  // Get the current stream position
  gxStreamPos pos = FilePosition();

  if(file_address == gxCurrAddress) { // Do not perform a seek operation
    return pos;
  }
  else if(file_address > pos) { // Seek forward to the specified address
    gxStreamPos offset = file_address - pos;
    Seek(offset, gxDBASE_SEEK_CUR);
  }
  else if(file_address < pos) { // Seek backward to the specified address
    Seek(file_address, gxDBASE_SEEK_BEG);
  }
  else { // Current file position equals the specified address
    // Find current the position
    Seek((FAU_t)0, gxDBASE_SEEK_CUR);
  }
  
  return FilePosition(); // Return current file position after seeking
}

gxDatabaseError gxDatabase::Read(void *buf, __ULWORD__ bytes, 
				 FAU_t file_address)
// Read a specified number of bytes from the specified file offset
// into a memory buffer. Returns a non-zero value to indicate an error
// condition or zero if successful.
{
  if((IsOK()) && (ReadyForReading())) {
    if(file_address == gxCurrAddress) {
      if(last_operation == gxDBASE_WRITE) {
	if(Seek((FAU_t)0, gxDBASE_SEEK_CUR) != gxDBASE_NO_ERROR) 
	  return gxd_error;
      }
    }
    else {
      if(Seek(file_address, gxDBASE_SEEK_BEG) != gxDBASE_NO_ERROR)
	return gxd_error;
    }

    __ULWORD__ bytes_read = gxdFPTRRead(fp, buf, bytes);
    if(bytes_read == 0) bytes_read = bytes;
    if(bytes_read != bytes) {
      gxd_error = gxDBASE_FILE_READ_ERROR;
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return gxd_error;
#endif
    }
    last_operation = gxDBASE_READ;
  }
  else {
    gxd_error = gxDBASE_FILE_NOT_READY;
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxd_error;
#endif
  }

  return gxd_error = gxDBASE_NO_ERROR;
}

gxDatabaseError gxDatabase::Write(const void *buf, __ULWORD__ bytes, 
				  FAU_t file_address, int flushdb, int bit_test)
// Write a specific number of bytes from a memory buffer to a
// specified file offset. If the "flushdb" variable is true, the file
// buffers will be flushed to disk with each write operation. If the
// bit_test variable if true, the CRC of the buffer will be compared
// to the CRC of the actual bytes written to disk. Returns a non-zero
// value to indicate an error condition or zero if successful.
{
  // PC-lint 09/08/2005: Variable init warning
  FAU_t buf_address = gxCurrAddress; // Current offset used for bit testing

  if((IsOK()) && (ReadyForWriting())) {
    if(file_address == gxCurrAddress) {
      if(last_operation == gxDBASE_READ) {
	if(Seek((FAU_t)0, gxDBASE_SEEK_CUR) != gxDBASE_NO_ERROR) return gxd_error;
      }
    }
    else {
      if(Seek(file_address, gxDBASE_SEEK_BEG) != gxDBASE_NO_ERROR)
	return gxd_error;
    }
    
    if(bit_test) { // Record the current write offest for bit testing
      if(file_address == gxCurrAddress) 
	buf_address = FilePosition(); 
      else 
	buf_address = file_address; 
      if(gxd_error != gxDBASE_NO_ERROR) return gxd_error;
    }
    
    if(gxdFPTRWrite(fp, buf, bytes) != bytes) {
      gxd_error = gxDBASE_FILE_WRITE_ERROR;
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return gxd_error;
#endif
    }
    last_operation = gxDBASE_WRITE;
  }
  else {
    gxd_error = gxDBASE_FILE_NOT_WRITEABLE;
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxd_error;
#endif
  }

  // Allow application to flush disk buffers after each write
  // operation to ensure the file data stays in sync during multiple
  // file access.
  if(flushdb) {  
    if(gxdFPTRFlush(fp) != 0) {
      gxd_error = gxDBASE_FILE_WRITE_ERROR;
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return gxd_error;
#endif
    }
    if(Seek((FAU_t)0, gxDBASE_SEEK_CUR) != gxDBASE_NO_ERROR) 
      return gxd_error;
  }
  
  if(bit_test) {
    __ULWORD__ w_csum = calcCRC32((char *)buf, bytes);
    __ULWORD__ r_csum = CalcChecksum(bytes, buf_address);

    // Check for file errors
    if(gxd_error != gxDBASE_NO_ERROR) return gxd_error;
    
    if(w_csum ^ r_csum) {
      gxd_error = gxDBASE_CHECKSUM_ERROR;
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return gxd_error;
#endif
    }
  }
  return gxd_error = gxDBASE_NO_ERROR;
}

gxUINT32 gxDatabase::WriteObjectChecksum(FAU_t object_address)
// Used to write a 32-bit checksum for the object at the
// end of a block. The address variable must be set to the
// file address of the block data, not the block header.
// This function assumes that the data has already been
// written to the block. Returns the 32-bit CRC checksum
// value for the object stored in the block or zero if
// this is the wrong revision or an error occurs.
{
  // Perform operation according to the revision letter
  switch(rev_letter) {
    case '\0' : case ' ' : case 'e' : case 'E' :
      // Return zero for any file below rev 'A' or rev 'E' files
      return (gxUINT32)0; 
    default: // Rev 'A', 'B', 'C', and 'D'
      break;
  }

  gxUINT32 CRC;
  __ULWORD__ bytes;
  gxBlockHeader blk_hdr;

  // Calculate the address of the block header
  FAU_t block_address = object_address - BlockHeaderSize();
  
  if(IsOK()) {
    // Make sure that the this is a pre-alloacted block.
    if(ReadBlockHdr(blk_hdr, block_address) != gxDBASE_NO_ERROR) {
      return (gxUINT32)0;
    }

    // Calculate a checksum based on the block data
    bytes = (blk_hdr.block_length - BlockHeaderSize()) - sizeof(gxChecksum);
    CRC = CalcChecksum(bytes, object_address);

    // Check for file errors
    if(gxd_error != gxDBASE_NO_ERROR) return (gxUINT32)0;

    // offset address to point to the checksum field located
    // at the end of the block.
    object_address += bytes;

    // Write the CRC for the block header and the block data.
    if(Write(&CRC, sizeof(CRC), object_address) != gxDBASE_NO_ERROR)
      return (gxUINT32)0;
  }

  return CRC;
}

int gxDatabase::ReadObjectChecksum(FAU_t object_address, 
				   __ULWORD__ *object_crc, 
				   __ULWORD__ *calc_crc)
// Tests the object's CRC value stored on disk against
// the actual CRC of the bytes stored on disk. The address
// variable must be set to the file address of the block
// data, not the block header. This function assumes that
// the data has already been written to the block. Returns
// true if the object's CRC test good or false if the CRC
// tests bad. Passes back the object's CRC stored on disk
// in the object_crc variable and the calculated CRC value
// in the calc_crc variable. Returns true if the checksum
// value is good or zero if the checksum value is bad or
// a file error occurs.
{
  // Perform operation according to the revision letter
  switch(rev_letter) {
    case '\0' : case ' ' : case 'e' : case 'E' :
      // Return true for any file below rev 'A' or rev 'E' files
      *object_crc = 0;
      *calc_crc = 0;
      return 1; 
    default: // Rev 'A', 'B', 'C', and 'D'
      break;
  }
  
  gxUINT32 CRC, objectCRC;
  gxBlockHeader blk_hdr;
  __ULWORD__ bytes;

  // Calculate the address of the block header
  FAU_t block_address = object_address - BlockHeaderSize();
  
  if(IsOK()) {
    // Make sure that the this is a pre-alloacted block.
    if(ReadBlockHdr(blk_hdr, block_address) != gxDBASE_NO_ERROR) {
      return (gxUINT32)0;
    }

    // Calculate a checksum based on the block data
    bytes = (blk_hdr.block_length - BlockHeaderSize()) - sizeof(gxChecksum);
    CRC = CalcChecksum(bytes, object_address);

    // Check for file errors
    if(gxd_error != gxDBASE_NO_ERROR) return 0;

    // offset address to point to the checksum field located
    // at the end of the block.
    object_address += bytes;

    // Read the CRC value stored on disk
    if(Read(&objectCRC, sizeof(objectCRC), object_address) != gxDBASE_NO_ERROR)
      return 0;
  }

  if(object_crc) *object_crc = objectCRC;
  if(calc_crc) *calc_crc = CRC;
     
  if(CRC ^ objectCRC) return 0; // Return false if CRC check fails

  return 1; // Return true if the CRC values match
}

__ULWORD__ gxDatabase::CalcChecksum(__ULWORD__ bytes, FAU_t file_address, 
				    int mem_alloc)
// Calculate a 32-bit CRC checksum for a given number
// of bytes starting at the specified address. Returns a
// 32-bit CRC value. If the mem_alloc variable is true, a
// buffer equal to the specified number of bytes will be
// created in memory. If the mem_alloc variable is false
// or memory allocation fails, the CRC will be calculated
// byte by byte starting at the specified address. NOTE:
// the calling function must check for disk file errors.
// Returns the CRC value.
{
  // PC-lint 09/08/2005: Variable init warning
  __ULWORD__ CRC = 0;
  __ULWORD__ len = bytes;
  unsigned char data;
  char *buf = 0;  

  // Create a buffer equal to the object length
  if(mem_alloc) buf = new char[bytes]; 

  if(buf) {
    if(IsOK()) {
      if(Read(buf, bytes, file_address) != gxDBASE_NO_ERROR) {
	delete[] buf; // Prevent any memory leaks
	return 0;
      }
      CRC = calcCRC32(buf, bytes);
      delete[] buf;
    }
  }
  else {
    if(IsOK()) {
      // Seek to the specified file address
      SeekTo(file_address);
      if(gxd_error != gxDBASE_NO_ERROR) return 0; 
      CRC = 0xffffffffL;
      while(len--) {
	if(Read(&data, sizeof(data)) != gxDBASE_NO_ERROR) return 0;
	CRC = calcCRC32(data, CRC);
      }
      CRC ^= 0xffffffffL;
    }
  }

  return CRC; 
}

gxDatabaseError gxDatabase::ReadBlockHdr(gxBlockHeader &hdr, 
					 FAU_t block_address)
// Reads a block header and tests the block's checkword to ensure
// that this is a valid block. Returns a non-zero value to indicate
// an error condition or zero if successful.
{
  if(Read(&hdr, sizeof(gxBlockHeader), block_address) != gxDBASE_NO_ERROR)
    return gxd_error;
  if(!TestBlockHeader(hdr)) { 
    gxd_error = gxDBASE_SYNC_ERROR;
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxd_error;
#endif
  }
  return gxd_error = gxDBASE_NO_ERROR;
}

gxDatabaseError gxDatabase::WriteBlockHdr(const gxBlockHeader &hdr,
					  FAU_t block_address)
// Writes a block header to the specified block address. Returns
// a non-zero value to indicate an error condition or zero if
// successful.
{
  return Write(&hdr, sizeof(gxBlockHeader), block_address);
}

gxDatabaseError gxDatabase::ReadFileHdr()
// Read the database file header. Returns a non-zero value to
// indicate an error condition or zero if successful. This 
// function will set the revision letter each time the file
// header is read.
{
  if(Read(&file_header, sizeof(gxFileHeader), gxStartOfFile) != 
     gxDBASE_NO_ERROR) {
    return gxd_error;
  }

  // Set the revision letter according to the file header
  rev_letter = file_header.gxd_sig[gxSignatureSize-1];

  switch(rev_letter) {
    case 'd' : case 'D' : case 'e' : case 'E' :
      if(ReadFileStatsHdr(file_stats_header) != gxDBASE_NO_ERROR) {
	return gxd_error;
      }
      break;
    default: // Rev '\0', 'A', 'B', and 'C'
      break;
  }

  return gxd_error = gxDBASE_NO_ERROR; 
}

gxDatabaseError gxDatabase::WriteFileHdr()
// Writes the database file header. Returns a non-zero value to
// indicate an error condition or zero if successful.
{
  if(Write(&file_header, sizeof(gxFileHeader), gxStartOfFile) != 
     gxDBASE_NO_ERROR) {
    return gxd_error;
  }
  switch(rev_letter) {
    case 'd' : case 'D' : case 'e' : case 'E' :
      if(WriteFileStatsHdr(file_stats_header) != gxDBASE_NO_ERROR) {
	return gxd_error;
      }
      break;
    default: // Rev '\0', 'A', 'B', and 'C'
      break;
  }

  return gxd_error = gxDBASE_NO_ERROR; 
}

FAU_t gxDatabase::Alloc(__ULWORD__ bytes, gxDatabaseReclaimMethod method)
// Allocates a contiguous number of bytes. The number of bytes allocated
// is adjusted according to the revision letter set when the file was
// created or opened. Only the block header and/or record lock header
// is written to the allocated space. Returns the file address of the newly
// allocated block or zero if an error occurs.
{
  FAU_t file_address = (FAU_t)0;
  gxBlockHeader blk_hdr;
  gxRecordLockHeader rlh; // Revision C and higher
  
  if((IsOK()) && (ReadyForWriting())) {
    // Adjust the number of bytes to allocate space for the block header
    // according to the revision letter.
    switch(rev_letter) {
      case '\0' : case ' ' : case 'e' : case 'E' :
	bytes += BlockHeaderSize(); // Database rev zero and rev E 
	break;
      default:
	bytes += BlockHeaderSize();
	bytes += sizeof(gxChecksum); // Rev 'A', 'B', 'C', and 'D'
	break;
    }

    // Try to reclaim a deleted or removed block
    if(file_header.gxd_fs_fptr != (FAU_t)0) {
      if(method == gxDBASE_RECLAIM_FIRSTFIT) {
	file_address = ReclaimFirstFit(bytes);
      }
      else if(method == gxDBASE_RECLAIM_BESTFIT) {
	file_address = ReclaimBestFit(bytes);
      }
      else {
	// Extend the file without reclaiming any deleted/removed blocks
	file_address = (FAU_t)0;
      }
    }
    
    if(IsOK() && file_address == (FAU_t)0) { 
      file_address = file_header.gxd_eof;
      file_header.gxd_eof += bytes;

      // 05/04/2016: Fix to prevent data holes
      char *mbuf = new char[bytes];
      memset(mbuf, 0, bytes);
      if(Write(mbuf, bytes, file_address) != gxDBASE_NO_ERROR) {
	delete mbuf;
	return (FAU_t)0;
      }
      delete mbuf;

      // Update the file stats for each new block allocated
      file_stats_header.num_blocks++;   

      // Update the highest block pointer
      file_header.gxd_hb_fptr = file_address;  
    }
   
    // Mark this newly allocated block as normal
    blk_hdr.block_status = gxNormalBlock;  
    blk_hdr.block_nd_fptr = 0;

    // Assign the current synchronization word value
    blk_hdr.block_check_word = gxDatabase::gxInternalCheckWord; 

    blk_hdr.block_length = bytes; // Total number of bytes for this block
    InitRecordLockHdr(rlh);  // Initialize the lock header

    // Write header for this block
    if(WriteBlockHdr(blk_hdr, file_address) != gxDBASE_NO_ERROR) return (FAU)0; 
    // Write a new record lock header (Revision C and higher)
    switch(rev_letter) {
      case 'c' : case 'C' : case 'd' : case 'D' :
	if(Write(&rlh, sizeof(gxRecordLockHeader)) != gxDBASE_NO_ERROR)
	  return (FAU_t)0;
	break;
      default: // Rev '\0', 'A', 'B', and 'E'
	break;
    }
  }
  else {
    gxd_error = gxDBASE_FILE_NOT_WRITEABLE;
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return (FAU_t)0;
#endif
  }
  if(file_address > file_header.gxd_hb_fptr)
    file_header.gxd_hb_fptr = file_address;  
  
  // Ensure that the database file header stays in sync
  // during multiple file access.
  WriteFileHdr();
  
  return SeekTo(file_address + BlockHeaderSize());  
}

int gxDatabase::Remove(FAU_t object_address)
// Marked the database block removed indicating that the
// object cannot be undeleted. Returns true if the block
// was or removed or false if the block was not removed.
{
  return Delete(object_address, 1);
}

int gxDatabase::Delete(FAU_t object_address, int remove_object)
// Marks the block at object address deleted and leaves the
// object unchanged, allowing it to be undeleted. The deleted
// block is placed on the front of the free space list. If 
// the "remove_object" variable is true the block is marked removed,
// indicating that the object cannot be undeleted. Returns
// true if the block was deleted/removed or false if the
// block was not deleted/removed.
{
  gxBlockHeader blk_hdr;
  
  // Address of the block header
  FAU_t block_address = object_address - BlockHeaderSize(); 
  if(ReadBlockHdr(blk_hdr, block_address) != gxDBASE_NO_ERROR) return 0;

  // Return false if block is already deleted
  if((__SBYTE__(blk_hdr.block_status & 0xff)) != gxNormalBlock) return 0; 
  
  // Mark the block removed and set the status 
  // accounting for all the status bytes
  __ULWORD__ block_status = blk_hdr.block_status;
  block_status &= 0xffffff00;
  if(remove_object) {
    block_status += gxRemovedBlock;
    file_stats_header.removed_blocks++; // Update the file stats header
  }
  else {
    block_status += gxDeletedBlock;
    file_stats_header.deleted_blocks++; // Update the file stats header
  }
  blk_hdr.block_status = block_status;
  
  if(ReadyForWriting()) {
    blk_hdr.block_nd_fptr = file_header.gxd_fs_fptr; 
    if(WriteBlockHdr(blk_hdr, block_address) != gxDBASE_NO_ERROR) return 0;
    
    // Make sure the free space list is not corrupt
    if(file_header.gxd_fs_fptr != gxFSListCorrupt) {
      file_header.gxd_fs_fptr = block_address;
      WriteFileHdr();
    }
    return 1; // Return true if successful
  }
  else {
    gxd_error = gxDBASE_FILE_NOT_WRITEABLE;
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else 
    return 0;
#endif
  }

  // 06/02/2002: Eliminate unreachable code in function warning under BCC32
#if !defined (__BCC32__) && !defined(__PCLINT_CHECK__)
  return 0; // Ensure that all paths return a value
#endif
}

gxStreamPos gxDatabase::FilePosition()
// Returns the current file position or -1 to indicate an error 
// condition.
{
  if(!IsOK()) {
    gxd_error = gxDBASE_FILE_NOT_READY;
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return (gxStreamPos)-1;
#endif
  }

  gxStreamPos pos = gxdFPTRTell(fp);
  if(pos < 0) {
    gxd_error = gxDBASE_FILE_POSITION_ERROR;
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#endif
  }
  return pos;
}

const char *gxDatabase::GetSignature() const
// Return the database file signature with no revision letter.
// NOTE: The calling function must delete the memory allocated
// for the database signature string.
{
  size_t len = sizeof(file_header.gxd_sig);
  char *s = new char[len];
  memcpy(s, file_header.gxd_sig, len);
  s[len-1] = 0; 
  return (const char *)s;
}

char *gxDatabase::GetSignature() 
// Return the database file signature with no revision letter
// NOTE: The calling function must delete the memory allocated
// for the database signature string.
{
  size_t len = sizeof(file_header.gxd_sig);
  char *s = new char[len];
  memcpy(s, file_header.gxd_sig, len);
  s[len-1] = 0;
  return s;
}

FAU_t gxDatabase::TotalBlocks()
// Returns the total number of valid blocks in the file.
{
  // Ensure that the database file header stays in sync
  // during multiple file access
  TestFileHeader();

  switch(rev_letter) {
    case 'd' : case 'D' : case 'e' : case 'E' :
      return file_stats_header.num_blocks;
    default: // Rev '\0', 'A', 'B', and 'C'
      break;
  }

  gxBlockHeader blk_hdr;
  FAU_t block_address = FindFirstBlock(0); // Search the entire file
  FAU_t i = (FAU_t)0;

  while(block_address) { // Until a block of a valid size is found
    if(block_address >= file_header.gxd_eof) break;
    if(Read(&blk_hdr, sizeof(gxBlockHeader)) != gxDBASE_NO_ERROR)
      break; // Return the number counted if an error occurs;

    if(!IsOK()) break;

    // If this is not a valid block, find the next one
    if(!TestBlockHeader(blk_hdr)) {
      block_address = FindFirstBlock(block_address);
    }
    else {
      block_address += blk_hdr.block_length;
      SeekTo(block_address);
      i++;
    }
  }
  return i;
}

FAU_t gxDatabase::NormalBlocks() 
// Returns the total number of normal blocks
{
  FAU_t d, r;
  FAU_t normal_blocks = TotalBlocks();
  FAU_t deleted_blocks = DeletedBlocks(&d, &r);
  return normal_blocks - deleted_blocks; 
}

FAU gxDatabase::DeleteBlocks(FAU *d, FAU *r)
// Depreciated function included for backward compatibility only.
{
  if(d) *d = (FAU)0; if(r) *r = (FAU)0;
  FAU_t tl, dl, rm;
  tl = DeletedBlocks(&dl, &rm);
  if(d) *d = (FAU)dl; if(r) *r = (FAU)rm;
  return (FAU)tl;
}

FAU_t gxDatabase::DeletedBlocks(FAU_t *d, FAU_t *r)
// Returns the total number of removed and deleted blocks.
{
  // Ensure that the database file header stays in sync
  // during multiple file access
  TestFileHeader();

  switch(rev_letter) {
    case 'd' : case 'D' : case 'e' : case 'E' :
      // Set the deleted and removed pointers to zero
      if(d) *d = (FAU_t)file_stats_header.deleted_blocks; 
      if(r) *r = (FAU_t)file_stats_header.removed_blocks; 
      return FAU_t(file_stats_header.deleted_blocks + \
		   file_stats_header.removed_blocks);
    default: // Rev '\0', 'A', 'B', and 'C'
      break;
  }

  gxBlockHeader blk_hdr;
  FAU_t block_address = file_header.gxd_fs_fptr; 
  FAU_t i = (FAU_t)0;

  // Set the deleted and removed pointers to zero
  if(d) *d = (FAU_t)0; if(r) *r = (FAU_t)0;
  
  if(block_address == gxFSListCorrupt) return gxFSListCorrupt;
  SeekTo(block_address);

  while(block_address) { // Until a block of a valid size is found
    if(Read(&blk_hdr, sizeof(gxBlockHeader)) != gxDBASE_NO_ERROR)
      break; // Return the number counted if an error occurs

    if(!IsOK()) break;

    // If this is not a valid block, the free space list is corrupt
    if(!TestBlockHeader(blk_hdr)) {
      file_header.gxd_fs_fptr = gxFSListCorrupt;
      WriteFileHdr(); 
      return gxFSListCorrupt;
    }

    // Added to prevent an infinite loop. 
    // If the block is not marked deleted or removed, the
    // Next deleted database pointer is bad. This will cause
    // an infinite loop if the end of the free space
    // list is pointing to valid block.
    switch((__SBYTE__)(blk_hdr.block_status & 0xff)) {
      case gxDeletedBlock :
	// Make sure the block is not pointing to itself
        if(block_address == blk_hdr.block_nd_fptr) { 
	  file_header.gxd_fs_fptr = gxFSListCorrupt;
	  WriteFileHdr(); 
	  return (FAU_t)0;
	}
	break;

      case gxRemovedBlock :
	// Make sure the block is not pointing to itself
        if(block_address == blk_hdr.block_nd_fptr) { 
	  file_header.gxd_fs_fptr = gxFSListCorrupt;
	  WriteFileHdr(); 
	  return (FAU_t)0;
	}
	break;

      default :
	file_header.gxd_fs_fptr = gxFSListCorrupt;
	WriteFileHdr(); 
	return (FAU_t)0;
    }

    block_address = blk_hdr.block_nd_fptr;
    if(block_address == gxFSListCorrupt) return gxFSListCorrupt;
    SeekTo(block_address);

    if(d) {
      if((__SBYTE__(blk_hdr.block_status & 0xff)) == gxDeletedBlock)
	*d = *d+(FAU_t)1;
    }

    if(r) {
      if((__SBYTE__(blk_hdr.block_status & 0xff)) == gxRemovedBlock)
	*r = *r+(FAU_t)1;
    }

    i++;
  }
  return i;
}

__ULWORD__ gxDatabase::ObjectLength(FAU_t object_address)
// Returns the object length in bytes at the specified object address.
{
  gxBlockHeader blk_hdr;
  FAU_t block_address;

  // Calculate the address of the block header
  if(object_address == gxCurrAddress)
    block_address = FilePosition() - BlockHeaderSize(); 
  else
    block_address = object_address - BlockHeaderSize(); 
  
  if(ReadBlockHdr(blk_hdr, block_address) != gxDBASE_NO_ERROR)
    return 0;

  __ULWORD__ len;

  // Adjust the number of bytes to allocate space of block header
  // according to the revision letter.
  switch(rev_letter) {
    case '\0' : case ' ' : case 'e' : case 'E' :
      len = blk_hdr.block_length - BlockHeaderSize();    
      break;
    default: // Rev 'A', 'B', 'C', and 'D'
      len = blk_hdr.block_length - BlockHeaderSize();
      len -= sizeof(gxChecksum);
      break;
  }

  return len;
}

__ULWORD__ gxDatabase::BlockLength(FAU_t object_address)
// Returns the total block length in bytes at the specified
// object address.
{
  gxBlockHeader blk_hdr;
  FAU_t block_address;
  
  // Calculate the address of the block header
  if(object_address == gxCurrAddress)
    block_address = FilePosition() - BlockHeaderSize(); 
  else
    block_address = object_address - BlockHeaderSize(); 

  if(ReadBlockHdr(blk_hdr, block_address) != gxDBASE_NO_ERROR)
    return 0;

  return blk_hdr.block_length;
}

int gxDatabase::UnDelete(FAU_t object_address)
// Undeletes a block if it has not been removed or reclaimed.
{
  gxBlockHeader blk_hdr, prev_blk; 
  FAU_t addr, block_address;
  
  // 06/02/2002: Initialize to eliminate W8013 warning under BCC32. 
  // Possible use of variable before definition in function.
  FAU_t prev_addr = (FAU_t)0;

  addr = file_header.gxd_fs_fptr; 

  // Address of block header
  block_address = object_address - BlockHeaderSize(); 
  if(ReadBlockHdr(blk_hdr, block_address) != gxDBASE_NO_ERROR)
    return 0;
  
  // Return false if block is not marked deleted 
  if((__SBYTE__(blk_hdr.block_status & 0xff)) != gxDeletedBlock) 
    return 0; 

  // Loop until the block is found in the free space list
  while(addr) { 
    if(Read(&blk_hdr, sizeof(gxBlockHeader), addr) != gxDBASE_NO_ERROR)
      return 0;

    if(!IsOK()) break;
    
    // Signal not to use the free space list.
    // If this is not a valid block, the free space list is corrupt
    if(!TestBlockHeader(blk_hdr)) {
      file_header.gxd_fs_fptr = gxFSListCorrupt;
      WriteFileHdr();  
      return 0;
    }
    
    // If the block is not marked deleted or removed, the
    // Next deleted database pointer is bad. This will cause
    // an infinite loop if the end of the free space
    // list is pointing to valid block.
    switch((__SBYTE__)(blk_hdr.block_status & 0xff)) {
      case gxDeletedBlock :
	// Make sure the block is not pointing to itself 
        if(addr == blk_hdr.block_nd_fptr) { 
	  file_header.gxd_fs_fptr = gxFSListCorrupt;
	  WriteFileHdr(); 
	  return 0;
	}
	break;

      case gxRemovedBlock :
	// Make sure the block is not pointing to itself 
        if(addr == blk_hdr.block_nd_fptr) { 
	  file_header.gxd_fs_fptr = gxFSListCorrupt;
	  WriteFileHdr(); 
	  return 0;
	}
	break;

      default :
	file_header.gxd_fs_fptr = gxFSListCorrupt;
	WriteFileHdr(); 
	return 0;
    }

    // Found the block in the free space list
    if(addr == block_address) { 

      // Update the file stats header
      if(file_stats_header.deleted_blocks > (FAU)0)
	file_stats_header.deleted_blocks--;

      if(prev_addr == (FAU_t)0) { // Adjust the free space list
	// At the head of freespace list, so make a new head
        file_header.gxd_fs_fptr = blk_hdr.block_nd_fptr;
	if(WriteFileHdr() != gxDBASE_NO_ERROR) return 0; 
      }
      else {
	// In the middle of free space, so link prev to Next
        prev_blk.block_nd_fptr = blk_hdr.block_nd_fptr;
	if(WriteBlockHdr(prev_blk, prev_addr)!= gxDBASE_NO_ERROR) return 0;
      }

      // Undelete the specified block
      // Mark the block with a normal attribute
      __ULWORD__ block_status = blk_hdr.block_status;
      block_status &= 0xffffff00;
      block_status += gxNormalBlock;
      blk_hdr.block_status = block_status;
      
      blk_hdr.block_nd_fptr = 0;  // Always 0 unless block is marked deleted

      // Write header for this block
      if(WriteBlockHdr(blk_hdr, addr)!= gxDBASE_NO_ERROR) return 0;  

      return 1; // Return true if successful
    }
    
    // Keep looping through the free space list
    prev_addr = addr;
    prev_blk = blk_hdr;
    addr = blk_hdr.block_nd_fptr;
  } 

  return 0; // Return false if block was not undeleted
}

FAU gxDatabase::GetDatabaseFreeSpace() 
{
  gxFileHeader fh;
  Read(&fh, sizeof(gxFileHeader), gxStartOfFile);

  // Ensure the in memory copy and the disk copy are the same
  if(fh.gxd_fs_fptr != file_header.gxd_fs_fptr) { 
    ReadFileHdr();
  }

  return file_header.gxd_fs_fptr;
}

FAU gxDatabase::GetEOF() 
{
  gxFileHeader fh;
  Read(&fh, sizeof(gxFileHeader), gxStartOfFile);

  // Ensure the in memory copy and the disk copy are the same
  if(fh.gxd_eof != file_header.gxd_eof) { 
    ReadFileHdr();
  }

  return file_header.gxd_eof;
}

FAU gxDatabase::GetHeapStart() 
{
  gxFileHeader fh;
  Read(&fh, sizeof(gxFileHeader), gxStartOfFile);

  // Ensure the in memory copy and the disk copy are the same
  if(fh.gxd_hs_fptr != file_header.gxd_hs_fptr) { 
    ReadFileHdr();
  }

  return file_header.gxd_hs_fptr;
}

FAU gxDatabase::GetHighestBlock()
{
  gxFileHeader fh;
  Read(&fh, sizeof(gxFileHeader), gxStartOfFile);

  // Ensure the in memory copy and the disk copy are the same
  if(fh.gxd_hb_fptr != file_header.gxd_hb_fptr) { 
    ReadFileHdr();
  }

  return file_header.gxd_hb_fptr;
}

FAU_t gxDatabase::StaticArea()
{
  gxFileHeader fh;
  Read(&fh, sizeof(gxFileHeader), gxStartOfFile);

  // Ensure the in memory copy and the disk copy are the same
  if(fh.gxd_hs_fptr != file_header.gxd_hs_fptr) { 
    ReadFileHdr();
  }

  return (FAU_t)(file_header.gxd_hs_fptr - FileHeaderSize());
}   

int gxDatabase::TestFileHeader()
// This function is used to ensure that the in memory copy
// of the database file header and the disk copy stay in sync
// during multiple file access.
{
  gxFileHeader fh;
  int errors = 0;
  
  Read(&fh, sizeof(gxFileHeader), gxStartOfFile);

  if(fh.gxd_fs_fptr != file_header.gxd_fs_fptr) { 
    errors++;
  }

  if(fh.gxd_eof != file_header.gxd_eof) { 
    errors++;
  }

  if(fh.gxd_hs_fptr != file_header.gxd_hs_fptr) { 
    errors++;
  }

  if(fh.gxd_hb_fptr != file_header.gxd_hb_fptr) { 
    errors++;
  }

  if(fh.gxd_hs_fptr != file_header.gxd_hs_fptr) { 
    errors++;
  }

  // Re-read the file header any errors are reported
  if(errors > 0) ReadFileHdr();

  return errors;
}

int gxDatabase::TestBlockHeader(const gxBlockHeader &hdr)
// Function used to test a block header to ensure that this is a 
// valid block header and not a block of binary data containing 
// words equal to a block checkword. Returns true if this is a 
// valid header or false if this not a valid header 
{
  // Test the block's checkword
  if(hdr.block_check_word != gxDatabase::gxInternalCheckWord) {
    return 0; // This is not a database block
  }

  // Read the block status
  __ULWORD__ block_status = hdr.block_status;
  __SBYTE__ unused_1 = __SBYTE__((block_status & 0xFF000000)>>24);
  __SBYTE__ unused_2 = __SBYTE__((block_status & 0xFF0000)>>16);
  if((unused_1 != 0) && (unused_2 != 0)) {
    return 0; // This is not a valid block
  }
  
  // Test the status and contol characters 
  __SBYTE__ control = __SBYTE__((block_status & 0xFF00)>>8);
  __SBYTE__ status = __SBYTE__(block_status & 0xff);

  switch(status) {
    case gxBadBlock: 
      break;

    case gxDeletedBlock:
      break;

    case gxNormalBlock:
      break;

    case gxRemovedBlock :
      break;

    case gxRemoteDeviceBlock :
      break;

    default :
      return 0; // This is not a valid block
  }

  if(status == gxRemoteDeviceBlock) { // This is a device block
    switch(control) {
      case gxAddRemoteBlock :
	break;
      case gxChangeRemoteBlock :
	break;
      case gxDeleteRemoteBlock :
	break;
      case gxRequestFailed :
	break;
      case gxCloseConnection :
	break;
      case gxKillServer :
	break;
      case gxRequestBlock :
	break;
      case gxSendBlock :
	break;
      case gxAcknowledgeBlock :
	break;
    default :
      return 0; // This is not a valid block
    }
  }
  else { // This is not a device block
    if(control != 0) return 0;
  }

  return 1; // This is a valid block
}

// ==============================================================
// General purpose file utilities (BEGIN HERE)
// ==============================================================
int gxDatabase::Exists(const char *fname)
// Returns true if the file exists
{
  return gxdFPTRExists(fname);
}

FAU_t gxDatabase::FileSize(const char *fname)
// Returns the file size. Use after file has been closed
// and re-opened to ensure that all the buffers are flushed
// to disk. Returns -1 to indicate an error condition.
{
  return (FAU_t)gxdFPTRFileSize(fname);
}
// ==============================================================
// General purpose file utilites (END HERE)
// ==============================================================

// ==============================================================
// Reclaim functions. (BEGIN HERE)
// ==============================================================
FAU_t gxDatabase::ReclaimBestFit(__ULWORD__ bytes)
// Searches the free space list for a block that can be reused.
// This function will search the free space list for an "exact- 
// fit" first and then try to find the "best-fit" for the number
// of bytes requested. NOTE: The byte size is adjusted by the
// Alloc() function to allocate space for the block header plus
// the object. Returns address of the reclaimed space, or zero
// if a deleted or removed block of the appropriate size is not
// found. An exact-fit is a block that matches the exact number
// of bytes requested. If an exact-fit cannot be found, the next
// block big enough to hold number of bytes requested plus the
// a block header with overhead and least one byte left over becomes
// a best-fit block. The search continues until a best-fit
// block with the least number of unused bytes is found. The
// used bytes in the best-fit block are used to create a new
// block that will be put back on the free space list. This
// will keep the gaps between the blocks as small as possible,
// with the smallest gap being as large as a single block header
// plus any block overhead plus one byte.
{
  // Cannot reuse any blocks if the free space list is corrupt
  if(file_header.gxd_fs_fptr == gxFSListCorrupt) return (FAU_t)0;

  gxBlockHeader blk_hdr, prev_blk, new_blk;
  gxBlockHeader best_fit_prev_blk, best_fit_blk;
  FAU_t addr, prev_addr, new_addr;
  FAU_t best_fit_addr;
  // PC-lint 09/08/2005: Variable init warning
  FAU_t best_fit_prev_addr = (FAU_t)0;
  __ULWORD__ avail_len, unused_len, best_fit_unused_len = 0;
  __SBYTE__ status;
  
  // Constants for the best-fit criteria. NOTE: The maximum length
  // of a block to reuse equals: (max_limit * byte_multiple) * bytes 
  const unsigned max_limit = 10;    // Maximum number of byte multiples
  const double byte_multiple = .25; // Byte multiples  

  double best_byte_len, byte_percent = 0;
  double bytes_requested = (double)bytes;
  unsigned i;
  unsigned best_length[max_limit];
  
  // Calculate the best-fit byte values
  for(i = 0; i < max_limit; i++) {
    byte_percent += byte_multiple;
    best_byte_len = bytes_requested * byte_percent;
    best_length[i] = (unsigned)best_byte_len;
  }

  addr = file_header.gxd_fs_fptr;
  SeekTo(addr);
  prev_addr = best_fit_addr = (FAU_t)0;
  
  // Search the entire free space list until an exact-fit 
  // or a best-fit block is found.
  while(addr) { 
    if(Read(&blk_hdr, sizeof(gxBlockHeader)) != gxDBASE_NO_ERROR) 
      return (FAU_t)0;
    if(!IsOK()) break;
    
    // Signal not to use the free space list.
    // If this is not a valid block, the free space list is corrupt
    if(!TestBlockHeader(blk_hdr)) {
      file_header.gxd_fs_fptr = gxFSListCorrupt;
      WriteFileHdr(); 
      return (FAU_t)0;
    }

    // If the block is not marked deleted or removed, the
    // Next deleted pointer is bad. This will cause
    // an infinite loop if the end of the free space
    // list is pointing to valid block.
    status = (__SBYTE__)(blk_hdr.block_status & 0xff);
    switch(status) { 
      case gxDeletedBlock :
	// Make sure the block is not pointing to itself 
        if(addr == blk_hdr.block_nd_fptr) { 
	  file_header.gxd_fs_fptr = gxFSListCorrupt;
	  WriteFileHdr(); 
	  return (FAU_t)0;
	}
	break;

      case gxRemovedBlock :
	// Make sure the block is not pointing to itself 
        if(addr == blk_hdr.block_nd_fptr) { 
	  file_header.gxd_fs_fptr = gxFSListCorrupt;
	  WriteFileHdr(); 
	  return (FAU_t)0;
	}
	break;

      default :
	file_header.gxd_fs_fptr = gxFSListCorrupt;
	WriteFileHdr(); 
	return (FAU_t)0;
    }

    // Length of object plus sizeof block header
    avail_len = blk_hdr.block_length; 

    // Unused length must be big enough to hold two block headers
    // plus the block overhead plus the object.
    __ULWORD__ total_len = bytes;
    switch(rev_letter) {
      case '\0' : case ' ' : case 'e' : case 'E' :
	total_len += BlockHeaderSize(); // Database rev zero and rev E 
	break;
      default: // Rev 'A', 'B', 'C', and 'D'
        total_len += BlockHeaderSize();
	total_len += sizeof(gxChecksum);
	break;
    }

    if(avail_len > total_len) {
      unused_len = avail_len - bytes;
    }
    else {
      unused_len = 0;
    }
    
    if(avail_len == bytes) { // Block is an exact fit
      // Update the file stats header for exact fit blocks
      if(status == gxDeletedBlock) {
	if(file_stats_header.deleted_blocks > (FAU)0)
	  file_stats_header.deleted_blocks--;
      }
      else if(status == gxRemovedBlock) {
	if(file_stats_header.removed_blocks > (FAU)0)
	  file_stats_header.removed_blocks--;
      }

      if(prev_addr == (FAU_t)0) {
	// At the head of freespace list
        file_header.gxd_fs_fptr = blk_hdr.block_nd_fptr;
	// Update all the file headers
	if(WriteFileHdr() != gxDBASE_NO_ERROR) return (FAU_t)0; 
      }
      else {
	// In the middle of free space
        prev_blk.block_nd_fptr = blk_hdr.block_nd_fptr;
	if(WriteBlockHdr(prev_blk, prev_addr) != gxDBASE_NO_ERROR)
	  return (FAU_t)0;
	// Update the file stats header 
	if(WriteFileStatsHdr(file_stats_header) != gxDBASE_NO_ERROR) 
	  return (FAU_t)0;
      }
      return (FAU_t)IsOK() ? (FAU_t)addr : (FAU_t)0;
    }

    if(unused_len > 0) { // Found bigger block with room for header
      for(i = 0; i < max_limit; i++) {
	if(unused_len <= best_length[i]) { 
	  // Use the block matching the best-fit criteria
	  if(best_fit_unused_len > best_length[i]) { 
	    // Use the block if it is a better then the current one
	    best_fit_addr = addr;
	    best_fit_prev_addr = prev_addr;
            best_fit_blk = blk_hdr;
	    best_fit_prev_blk = prev_blk;
	    best_fit_unused_len = unused_len;
	  }
	}
      }
    }
    
    // Block is not big enough
    prev_addr = addr;
    prev_blk = blk_hdr;
    addr = blk_hdr.block_nd_fptr;
    SeekTo(addr);

  } // End of block search

  // Could not find a best fit
  if(best_fit_addr == (FAU_t)0) return (FAU_t)0; 

  // Update the file stats header for exact best fit blocks
  if(status == gxDeletedBlock) {
    if(file_stats_header.deleted_blocks > (FAU)0)
      file_stats_header.deleted_blocks--;
  }
  else if(status == gxRemovedBlock) {
    if(file_stats_header.removed_blocks > (FAU)0)
      file_stats_header.removed_blocks--;
  }

  // Reuse the block and any remaining bytes
  new_addr = best_fit_addr + bytes;
  new_blk.block_check_word = gxDatabase::gxInternalCheckWord;

  // Mark the block with a removed attribute 
  new_blk.block_status = gxRemovedBlock;
  
  new_blk.block_nd_fptr = best_fit_blk.block_nd_fptr;
  new_blk.block_length = best_fit_unused_len;

  // Update the file statistics header
  file_stats_header.num_blocks++; // Another block has been allocated
  file_stats_header.removed_blocks++;

  if(WriteBlockHdr(new_blk, new_addr) != gxDBASE_NO_ERROR) return (FAU_t)0;

  // Adjust the free space list
  if(best_fit_prev_addr == (FAU_t)0) {
    // At the head of freespace
    file_header.gxd_fs_fptr = new_addr;
    // Update all headers
    if(WriteFileHdr() != gxDBASE_NO_ERROR) return (FAU_t)0; 
  }
  else { // In the middle of freespace
    best_fit_prev_blk.block_nd_fptr = new_addr;
    if(WriteBlockHdr(best_fit_prev_blk, best_fit_prev_addr) != 
       gxDBASE_NO_ERROR) 
      return (FAU_t)0;
    // Update the file stats header 
    if(WriteFileStatsHdr(file_stats_header) != gxDBASE_NO_ERROR) {
      return (FAU_t)0;
    }
  }
  
  return (FAU_t)IsOK() ? (FAU_t)best_fit_addr : (FAU_t)0;
}

FAU_t gxDatabase::ReclaimFirstFit(__ULWORD__ bytes)
// Searchs the free space list for the first block that can
// be reused. This function will search the free space list
// for a "first-fit" big enough to hold the number of bytes
// requested. NOTE: The byte size is adjusted by the Alloc()
// function to allocate space for the block header plus the object.
// Returns address of the reclaimed space, or zero if a deleted
// or removed block of the appropriate size is not found. If an
// "exact-fit" is found (a block that matches the exact number
// of bytes requested) the address of that block is returned.
// Otherwise the address of the first block big enough to hold
// number of bytes requested plus the size a block header with 
// overhead and at least one byte left over is returned. The used
// bytes in the first-fit block are used to create a new block that
// will be put back on the free space list. 
{
  // Cannot reuse any blocks if the free space list is corrupt
  if(file_header.gxd_fs_fptr == gxFSListCorrupt) return (FAU_t)0;

  gxBlockHeader blk_hdr, prev_blk, new_blk;
  FAU_t addr, prev_addr, new_addr;
  __ULWORD__ avail_len, unused_len;
  __SBYTE__ status;

  addr = file_header.gxd_fs_fptr; prev_addr = (FAU_t)0;
  SeekTo(addr);

  // Search the free space list until a first-fit is found
  while(addr) { 
    if(Read(&blk_hdr, sizeof(gxBlockHeader)) != gxDBASE_NO_ERROR)
      return (FAU_t)0;

    if(!IsOK()) break;
    
    // Signal not to use the free space list.
    // If this is not a valid block, the free space list is corrupt
    if(!TestBlockHeader(blk_hdr)) {
      file_header.gxd_fs_fptr = gxFSListCorrupt;
      WriteFileHdr(); 
      return (FAU_t)0;
    }

    // If the block is not marked deleted or removed, the
    // Next deleted pointer is bad. This will cause
    // an infinite loop if the end of the free space
    // list is pointing to valid block.
    status = (__SBYTE__)(blk_hdr.block_status & 0xff);
    switch(status) {
      case gxDeletedBlock :
	// Make sure the block is not pointing to itself 
        if(addr == blk_hdr.block_nd_fptr) { 
	  file_header.gxd_fs_fptr = gxFSListCorrupt;
	  WriteFileHdr(); 
	  return (FAU_t)0;
	}
	break;

      case gxRemovedBlock :
	// Make sure the block is not pointing to itself 
        if(addr == blk_hdr.block_nd_fptr) { 
	  file_header.gxd_fs_fptr = gxFSListCorrupt;
	  WriteFileHdr(); 
	  return (FAU_t)0;
	}
	break;

      default :
	file_header.gxd_fs_fptr = gxFSListCorrupt;
	WriteFileHdr(); 
	return (FAU_t)0;
    }

    // Length of object plus sizeof block header
    avail_len = blk_hdr.block_length; 

    // Unused length must be big enough to hold two block headers
    // plus the block overhead plus the object.
    __ULWORD__ total_len = bytes;
    switch(rev_letter) {
      case '\0' : case ' ' : case 'e' : case 'E' :
	total_len += BlockHeaderSize(); // Database rev zero and rev E 
	break;
      default: // Rev 'A', 'B', 'C', and 'D'
	total_len += BlockHeaderSize();
	total_len += sizeof(gxChecksum);
	break;
    }

    if(avail_len > total_len) {
      unused_len = avail_len - bytes;
    }
    else {
      unused_len = 0;
    }

    if((avail_len == bytes) || (unused_len > 0)) {
      // Update the file stats header for exact fit and first fit blocks
      if(status == gxDeletedBlock) {
	if(file_stats_header.deleted_blocks > (FAU)0)
	  file_stats_header.deleted_blocks--;
      }
      else if(status == gxRemovedBlock) {
	if(file_stats_header.removed_blocks > (FAU)0)
	  file_stats_header.removed_blocks--;
      }
    }

    if(avail_len == bytes) { // Block is an exact fit
      if(prev_addr == (FAU_t)0) {
	// At the head of freespace list
        file_header.gxd_fs_fptr = blk_hdr.block_nd_fptr;
	// Update all the file headers
	if(WriteFileHdr() != gxDBASE_NO_ERROR) return (FAU_t)0; 
      }
      else {
	// In the middle of free space
        prev_blk.block_nd_fptr = blk_hdr.block_nd_fptr;
	if(WriteBlockHdr(prev_blk, prev_addr)!= gxDBASE_NO_ERROR) 
	  return (FAU_t)0;
	// Update the file stats header 
	if(WriteFileStatsHdr(file_stats_header) != gxDBASE_NO_ERROR) 
	  return (FAU_t)0;
      }
      break;
    }

    if(unused_len > 0) { // Block is a first fit, reuse any remaining bytes
      new_addr = addr + bytes;
      new_blk.block_check_word = gxDatabase::gxInternalCheckWord;

      // Mark the block with a removed attribute 
      new_blk.block_status = gxRemovedBlock;

      new_blk.block_nd_fptr = blk_hdr.block_nd_fptr;
      new_blk.block_length = unused_len;

      // Update the file statistics header
      file_stats_header.num_blocks++; // Another block has been allocated
      file_stats_header.removed_blocks++;

      if(WriteBlockHdr(new_blk, new_addr) != gxDBASE_NO_ERROR) return (FAU_t)0;
      if(prev_addr == (FAU_t)0) {
	// At the head of freespace
	file_header.gxd_fs_fptr = new_addr;
	// Update all headers
	if(WriteFileHdr() != gxDBASE_NO_ERROR) return (FAU_t)0;
      }
      else { // In the middle of freespace
	prev_blk.block_nd_fptr = new_addr;
	if(WriteBlockHdr(prev_blk, prev_addr) != gxDBASE_NO_ERROR)
	  return (FAU_t)0;
	// Update the file stats header 
	if(WriteFileStatsHdr(file_stats_header) != gxDBASE_NO_ERROR) {
	  return (FAU_t)0;
	}
      }
      break;
    }

    // Block is not big enough
    prev_addr = addr;
    prev_blk = blk_hdr;
    addr = blk_hdr.block_nd_fptr;
    SeekTo(addr);
  } // End of block search
  
  return (FAU_t)IsOK() ? (FAU_t)addr : (FAU_t)0;
}
// ==============================================================
// Reclaim functions. (END HERE)
// ==============================================================

// ==============================================================
// Header Size Functions. (BEGIN HERE)
// ==============================================================
size_t gxDatabase::FileHeaderSize() const
{
  size_t fh = sizeof(gxFileHeader);
  size_t flh = sizeof(gxFileLockHeader);
  size_t fsh = sizeof(gxFileStatsHeader);

  // Perform operation according to the revision letter
  switch(rev_letter) {
    case 'b' : case 'B' : case 'c' : case 'C' :
      return fh + flh;
    case 'd' : case 'D' : case 'e' : case 'E' :
      return ((fh + flh) + fsh);
    default: // Rev '\0' and 'A'
      break;
  }
  return fh;
}

size_t gxDatabase::FileHeaderSize()
{
  size_t fh = sizeof(gxFileHeader);
  size_t flh = sizeof(gxFileLockHeader);
  size_t fsh = sizeof(gxFileStatsHeader);

  // Perform operation according to the revision letter
  switch(rev_letter) {
    case 'b' : case 'B' : case 'c' : case 'C' :
      return fh + flh;
    case 'd' : case 'D' : case 'e' : case 'E' :
      return ((fh + flh) + fsh);
    default: // Rev '\0' and 'A'
      break;
  }
  return fh;
}

size_t gxDatabase::BlockHeaderSize() const 
{
  size_t bh = sizeof(gxBlockHeader);
  size_t blh = sizeof(gxRecordLockHeader);
  
  // Perform operation according to the revision letter
  switch(rev_letter) {
    case 'c' : case 'C' : case 'd' : case 'D' :
      return bh + blh;
    default: // Rev '\0', 'A', 'B', and 'E'
      break;
  }
  return bh;
}

size_t gxDatabase::BlockHeaderSize()
{
  size_t bh = sizeof(gxBlockHeader);
  size_t blh = sizeof(gxRecordLockHeader);
  
  // Perform operation according to the revision letter
  switch(rev_letter) {
    case 'c' : case 'C' : case 'd' : case 'D' :
      return bh + blh;
    default: // Rev '\0', 'A', 'B', and 'E'
      break;
  }
  return bh;
}
// ==============================================================
// Header Size Functions. (END HERE)
// ==============================================================

// ==============================================================
// Database Linear Navigation Functions (BEGIN HERE)
// ==============================================================
FAU_t gxDatabase::FindFirstBlock(FAU_t offset)
// Search through the database until a valid block is found.
// The search starts at the heap start or the offset value.
// Returns 0 if no valid block is found in the file or the end
// of file is reached. NOTE: This function does not check the
// block's status and will return the address of normal, deleted,
// and removed blocks.
{
  gxBlockHeader blk_hdr;
  FAU_t file_address = (FAU_t)0;

  // Ensure that the database header stays in sync
  // during multiple file access
  TestFileHeader();

  // No blocks have been allocated yet
  if(file_header.gxd_hs_fptr == file_header.gxd_eof) return (FAU_t)0;
     
  if(!offset)
    file_address = file_header.gxd_hs_fptr; // If no offset, start at heap
  else {
    file_address = file_address + offset; // offset the starting address 

    if(file_address >= file_header.gxd_eof) // Prevent offsetting past EOF
      return (FAU_t)0; // Invalid address
  }
  
  while(1) {
    if((FAU_t)(file_address + BlockHeaderSize()) >= file_header.gxd_eof ||
       !IsOK()) return (FAU_t)0;

    if(Read(&blk_hdr, sizeof(gxBlockHeader), file_address) != gxDBASE_NO_ERROR)
      return (FAU_t)0;
    if(!TestBlockHeader(blk_hdr)) {
      file_address ++; // Find the next checkword
    }
    else {
      break; // Found valid block
    }
  }
  return SeekTo(file_address);
}

FAU_t gxDatabase::FindFirstObject(FAU_t offset)
// Search through the database until a valid block is found and
// then return the object's address. If the block is marked deleted
// continue searching until the first normal block is found. The
// search starts at the heap start or the offset. Returns 0 if
// no valid block is found in the file or the end of file is reached.
{
  gxBlockHeader blk_hdr;
  FAU_t file_address = FindFirstBlock(offset);
  if(!file_address) return (FAU_t)0;

  while(1) { // Loop until a normal block status is found
    if(ReadBlockHdr(blk_hdr, file_address) != gxDBASE_NO_ERROR) 
      return (FAU_t)0;
    if((__SBYTE__(blk_hdr.block_status & 0xff)) == gxNormalBlock) 
      break;
    file_address = FindFirstBlock(file_address+blk_hdr.block_length);
    if(!file_address) return (FAU_t)0;
  }

  return file_address + BlockHeaderSize();
}

FAU_t gxDatabase::FindNextBlock(FAU_t offset)
// Search through the database until the next valid block after the
// first valid block is found. The search starts at the heap start
// or the offset value. Returns 0 if no valid block is found or the
// end of file is reached.
{
  gxBlockHeader blk_hdr;

  // No blocks have been allocated yet
  if(file_header.gxd_hs_fptr == file_header.gxd_eof) return (FAU_t)0;

  FAU_t file_address = FindFirstBlock(offset);

  if(!file_address) return (FAU_t)0; // No Vaild block found

  if(ReadBlockHdr(blk_hdr, file_address) != gxDBASE_NO_ERROR) return (FAU_t)0;
  FAU_t next_blk = file_address + blk_hdr.block_length;

  // This is last the blocks
  if(next_blk >= file_header.gxd_eof) return file_address; 

  // Ensure block header is valid
  if(ReadBlockHdr(blk_hdr, next_blk) != gxDBASE_NO_ERROR) return (FAU_t)0;

  return next_blk;
}

FAU_t gxDatabase::FindNextObject(FAU_t offset)
// Search through the database until the next valid block after the
// first valid block is found and then return the object's address.
// If the block is marked deleted continue searching until the next
// normal block is found. The search starts at the heap start or the
// offset value. Returns 0 if no valid block is found or the end of
// file is reached.
{
  gxBlockHeader blk_hdr;
  FAU_t file_address = FindNextBlock(offset);
  if(!file_address) return (FAU_t)0;
  
  while(1) { // Loop until a normal block status is found
    if(ReadBlockHdr(blk_hdr, file_address) != gxDBASE_NO_ERROR) 
      return (FAU_t)0;
    if((__SBYTE__(blk_hdr.block_status & 0xff)) == gxNormalBlock) 
      break;
    file_address = FindNextBlock(file_address+blk_hdr.block_length);
    if(!file_address) return (FAU_t)0;
  }

  return file_address + BlockHeaderSize();
}

FAU_t gxDatabase::FindPrevBlock(FAU_t offset)
// Search backwards through the database until a valid block is found and
// then return the block's address. If the block is marked deleted
// continue searching until the first normal block is found. The
// search starts at the offset. Returns 0 if no valid block is found in
// the file or the start of file is reached.
{
  const FAU_t DEF_BUF_SIZE = 1000 * sizeof(FAU_t);
  gxBlockHeader blk_hdr;
  FAU_t file_address = (FAU_t)0;
  FAU_t bufsize, rdsize;
  char *buf;

  // Ensure that the database header stays in sync
  // during multiple file access.
  TestFileHeader();
 
  // No blocks have been allocated yet
  if(file_header.gxd_hs_fptr == file_header.gxd_eof) return (FAU_t)0;
  
  if(!offset) return (FAU_t)0;
  else {
    // offset the starting address 
    file_address = ((file_address + offset) - BlockHeaderSize()); 
    
    // Prevent offsetting past EOF
    if((file_address <= file_header.gxd_hs_fptr) ||
       (file_address > file_header.gxd_eof)) 
      return 0; // Invalid address
  }
  bufsize = file_address - file_header.gxd_hs_fptr;
  if(bufsize == 0) {
    file_address = FAU_t(file_header.gxd_hs_fptr + BlockHeaderSize());
    return file_address;
  }
  if(bufsize > DEF_BUF_SIZE) bufsize = DEF_BUF_SIZE;

  buf = new char[(__ULWORD__)bufsize];

  while (1) {
    if(!IsOK()) {
      delete[] buf; // Prevent any memory leaks
      return (FAU_t)0;
    }
    rdsize = bufsize;
    if((file_address - rdsize) < file_header.gxd_hs_fptr) {
      rdsize = file_address - file_header.gxd_hs_fptr;
    }
    if(rdsize < (FAU_t)sizeof(gxINT32)) {
      delete[] buf; // Prevent any memory leaks
      return (FAU_t)0;
    }

    file_address -= rdsize;
    if(Read(buf, (__ULWORD__)rdsize, file_address) !=
       gxDBASE_NO_ERROR) {
      delete[] buf; // Prevent any memory leaks
      return (FAU_t)0;
    }
    for(FAU_t i = rdsize - sizeof(FAU_t); i >= 0; i--) {
      if((file_address+i == file_header.gxd_hs_fptr) ||
	 (*((gxUINT32*)(buf+(FAU_t)i)) == \
gxDatabase::gxInternalCheckWord)) {
        Read(&blk_hdr, sizeof(gxBlockHeader), file_address+i);
	if(TestBlockHeader(blk_hdr)) { // Found valid block
	  if((__SBYTE__(blk_hdr.block_status & 0xff)) == gxNormalBlock) {
	    delete[] buf; // Prevent any memory leaks
	    return file_address+i;
	  }
	}
      }
    }
  }
}

FAU_t gxDatabase::FindPrevObject(FAU_t offset)
// Search backwards through the database until a valid block is found
// and then return the object's address. If the block is marked deleted
// continue searching until the first normal block is found. The
// search starts at the offset. Returns 0 if no valid block is found in
// the file or the start of file is reached.
{
  gxBlockHeader blk_hdr;
  FAU_t file_address = FindPrevBlock(offset);
  if(!file_address) return (FAU_t)0;

  while(1) { // Loop until a normal block status is found
    if(ReadBlockHdr(blk_hdr, file_address) != gxDBASE_NO_ERROR) 
      return (FAU_t)0;
    if((__SBYTE__(blk_hdr.block_status & 0xff)) == gxNormalBlock) 
      break;
    file_address = FindPrevBlock(file_address+blk_hdr.block_length);
    if(!file_address) return (FAU_t)0;
  }

  return file_address + BlockHeaderSize();
}  
// ==============================================================
// Database Linear Navigation Functions (END HERE)
// ==============================================================

// ==============================================================
// Database File Lock Functions (BEGIN HERE)
// ==============================================================
void gxDatabase::InitFileLockHdr(gxFileLockHeader &hdr)
// Initialize a the lock header for a newly 
// constructed file lock header.
{
  hdr.file_lock_protect = (gxUINT32)0;
  hdr.file_read_lock = (gxUINT32)0;
  hdr.file_write_lock = (gxUINT32)0;
}

gxDatabaseError gxDatabase::WriteFileLockHdr(const gxFileLockHeader &hdr)
// Write the lock header to the file. Returns a non-zero value
// to indicate an error condition or zero if successful.
{
  switch(rev_letter) {
    case 'b' : case 'B' : case 'c' : case 'C' :
      break;
    case 'd' : case 'D' : case 'e' : case 'E' :
      break;
    default: // Rev '\0' and 'A'
      return gxd_error = gxDBASE_NO_ERROR;
  }

  return Write(&hdr, sizeof(gxFileLockHeader), sizeof(gxFileHeader));
}

gxDatabaseError gxDatabase::ReadFileLockHdr(gxFileLockHeader &hdr)
// Read the lock header from the file. Returns a non-zero value to
// indicate an error condition or zero if successful.
{
  switch(rev_letter) {
    case 'b' : case 'B' : case 'c' : case 'C' :
      break;
    case 'd' : case 'D' : case 'e' : case 'E' :
      break;
    default: // Rev '\0' and 'A'
      InitFileLockHdr(hdr);
      return gxd_error = gxDBASE_NO_ERROR;
  }

  return Read(&hdr, sizeof(gxFileLockHeader), sizeof(gxFileHeader));
}

int gxDatabase::LockFile(gxDatabaseLockType l_type) 
// Lock the file and set the lock access mode to shared or exclusive.
// Returns a non-zero value if the file cannot be locked or the lock 
// variable cannot be changed because it is exclusive or another thread 
// is currently updating it. 
{
  switch(rev_letter) {
    case 'b' : case 'B' : case 'c' : case 'C' :
      break;
    case 'd' : case 'D' : case 'e' : case 'E' :
      break;
    default: // Rev '\0' and 'A'
      return gxd_error = gxDBASE_NO_ERROR;
  }

  // Read the record lock to ensure that this record has a lock header
  // and to keep the header in sync during multiple file access.
  gxFileLockHeader lock_header;
  if(ReadFileLockHdr(lock_header) != gxDBASE_NO_ERROR) return gxd_error;

  // Cannot modifiy this lock until the thread holding it releases the
  // lock header.
  if(lock_header.file_lock_protect != (gxUINT32)0) {
    gxd_error = gxDBASE_FILELOCK_ACCESS_ERROR;
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxd_error;
#endif
  }
  
  switch(l_type) {
    case gxDBASE_WRITELOCK :
      // Cannot obtain an exclusive lock until all the threads in the
      // read lock queue have finished
      if((lock_header.file_read_lock != (gxUINT32)0) || 
	 (lock_header.file_write_lock != (gxUINT32)0)) {
	gxd_error = gxDBASE_FILELOCK_ERROR;
#ifdef __CPP_EXCEPTIONS__
	throw gxCDatabaseException();
#else
	return gxd_error;
#endif
      }
      else {
	lock_header.file_write_lock = 1;
      }

      lock_header.file_lock_protect = 1;
      if(Write(&lock_header.file_lock_protect, sizeof(gxUINT32),
	       sizeof(gxFileHeader)) != gxDBASE_NO_ERROR)
	return gxd_error;

      // Skip over the read lock member
      SeekTo(sizeof(gxFileHeader)+ (sizeof(gxUINT32) * 2));
      if(Write(&lock_header.file_write_lock, sizeof(gxUINT32)) !=
	 gxDBASE_NO_ERROR)
	return gxd_error;

      lock_header.file_lock_protect = (gxUINT32)0;
      if(Write(&lock_header.file_lock_protect, sizeof(gxUINT32),
	       sizeof(gxFileHeader)) != gxDBASE_NO_ERROR)
	return gxd_error;
      break;

    case gxDBASE_READLOCK :
      // This lock is exclusively owned and cannot be read locked
      // until the thread holding this lock release the exclusive lock.
      if(lock_header.file_write_lock == 1) {
	gxd_error = gxDBASE_FILELOCK_ERROR;
#ifdef __CPP_EXCEPTIONS__
	throw gxCDatabaseException();
#else
	return gxd_error;
#endif
      }
      
      if(lock_header.file_read_lock < 0xFFFFFFFF) {
	lock_header.file_read_lock++;
      }
      else {
	gxd_error = gxDBASE_FILELOCK_ERROR;
#ifdef __CPP_EXCEPTIONS__
	throw gxCDatabaseException();
#else
	return gxd_error;
#endif
      }
      
      lock_header.file_lock_protect = 1;
      if(Write(&lock_header.file_lock_protect, sizeof(gxUINT32),
	       sizeof(gxFileHeader)) != gxDBASE_NO_ERROR)
	return gxd_error;
      
      if(Write(&lock_header.file_read_lock, sizeof(gxUINT32)) !=
	 gxDBASE_NO_ERROR)
	return gxd_error; 
      
      lock_header.file_lock_protect = (gxUINT32)0;
      if(Write(&lock_header.file_lock_protect, sizeof(gxUINT32),
	       sizeof(gxFileHeader)) != gxDBASE_NO_ERROR)
	return gxd_error;
      break;
      
    default: // Invalid lock type specified
      gxd_error =  gxDBASE_INVALID_LOCK_TYPE;
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return gxd_error;
#endif
  }
  
  return gxd_error = gxDBASE_NO_ERROR;
}

int gxDatabase::UnlockFile(gxDatabaseLockType l_type)
// Unlock the file. Returns a non-zero value if the file cannot be 
// unlocked or the lock variable cannot be changed because it is 
// exclusive or another thread is currently updating it. 
{
  switch(rev_letter) {
    case 'b' : case 'B' : case 'c' : case 'C' :
      break;
    case 'd' : case 'D' : case 'e' : case 'E' :
      break;
    default: // Rev '\0' and 'A'
      return gxd_error = gxDBASE_NO_ERROR;
  }
  
  // Read the record lock to ensure that this record has a lock header
  // and to keep the header in sync during multiple file access.
  gxFileLockHeader lock_header;
  if(ReadFileLockHdr(lock_header) != gxDBASE_NO_ERROR) return gxd_error;

  // Cannot modifiy this lock until the thread holding it releases the
  // lock header.
  if(lock_header.file_lock_protect != (gxUINT32)0) {
    gxd_error = gxDBASE_FILELOCK_ACCESS_ERROR;
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxd_error;
#endif
  }
  
  switch(l_type) {
    case gxDBASE_WRITELOCK :
      if(lock_header.file_write_lock == (gxUINT32)0)
	return gxDBASE_NO_ERROR;
      else
	lock_header.file_write_lock = (gxUINT32)0;
      
      lock_header.file_lock_protect = 1;
      if(Write(&lock_header.file_lock_protect, sizeof(gxUINT32),
	       sizeof(gxFileHeader)) != gxDBASE_NO_ERROR)
	return gxd_error;

      // Skip over the read lock member
      SeekTo(sizeof(gxFileHeader)+ (sizeof(gxUINT32) * 2));
      if(Write(&lock_header.file_write_lock, sizeof(gxUINT32)) !=
	 gxDBASE_NO_ERROR)
	return gxd_error;

      lock_header.file_lock_protect = (gxUINT32)0;
      if(Write(&lock_header.file_lock_protect, sizeof(gxUINT32),
	       sizeof(gxFileHeader)) != gxDBASE_NO_ERROR)
	return gxd_error;
      break;

    case gxDBASE_READLOCK :
      if(lock_header.file_read_lock == (gxUINT32)0)      // This record is not locked
	return gxd_error = gxDBASE_NO_ERROR;
      else if(lock_header.file_read_lock  > (gxUINT32)0) // Prevent read lock rollover
      	lock_header.file_read_lock--;
      else
	lock_header.file_read_lock = (gxUINT32)0;
      
      lock_header.file_lock_protect = 1;
      if(Write(&lock_header.file_lock_protect, sizeof(gxUINT32),
	       sizeof(gxFileHeader)) != gxDBASE_NO_ERROR)
	return gxd_error;

      if(Write(&lock_header.file_read_lock, sizeof(gxUINT32)) !=
	 gxDBASE_NO_ERROR)
	return gxd_error; 

      lock_header.file_lock_protect = (gxUINT32)0;
      if(Write(&lock_header.file_lock_protect, sizeof(gxUINT32),
	       sizeof(gxFileHeader)) != gxDBASE_NO_ERROR)
	return gxd_error;
      break;

    default: // Invalid lock type specified
      gxd_error =  gxDBASE_INVALID_LOCK_TYPE;
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxd_error;
#endif
  }

  return gxd_error = gxDBASE_NO_ERROR;
}

gxDatabaseError gxDatabase::ResetFileLock()
// Reset the file's lock header. This function will clear
// all the file lock fields without testing the lock or the
// lock protect. Returns a non-zero value to indicate an error
// condition or zero if successful.
{
  switch(rev_letter) {
    case 'b' : case 'B' : case 'c' : case 'C' :
      break;
    case 'd' : case 'D' : case 'e' : case 'E' :
      break;
    default: // Rev '\0' and 'A'
      return gxd_error = gxDBASE_NO_ERROR;
  }
  gxFileLockHeader lock_header;
  InitFileLockHdr(lock_header);
  return WriteFileLockHdr(lock_header);
}
// ==============================================================
// Database File Lock Functions (END HERE)
// ==============================================================

// ==============================================================
// Database Record Lock Functions (BEGIN HERE)
// ==============================================================
void gxDatabase::InitRecordLockHdr(gxRecordLockHeader &hdr)
// Initialize a the lock header for a newly constructed 
// record lock header.
{
  hdr.record_lock_protect = 0;
  hdr.record_read_lock = 0;
  hdr.record_write_lock = 0;
}

gxDatabaseError gxDatabase::ReadRecordLockHdr(gxRecordLockHeader &hdr, 
					      FAU_t block_address)
// Read the block's record lock header. Returns a non-zero value to
// indicate an error condition or zero if successful.
{
  switch(rev_letter) {
    case 'c' : case 'C' : case 'd' : case 'D' :
      break;
    default: // Rev '\0', 'A', 'B', and 'E'
      InitRecordLockHdr(hdr);
      return gxd_error = gxDBASE_NO_ERROR;
  }

  // Ensure that this is a valid database block
  gxBlockHeader block_header;
  if(ReadBlockHdr(block_header, block_address) != gxDBASE_NO_ERROR)
    return gxd_error;

  // Read the record lock directly following the block header 
  return Read(&hdr, sizeof(gxRecordLockHeader));
}

gxDatabaseError gxDatabase::WriteRecordLockHdr(const gxRecordLockHeader &hdr, 
					       FAU_t block_address)
// Write the block's record lock header. Returns a non-zero value to
// indicate an error condition or zero if successful.
{
  switch(rev_letter) {
    case 'c' : case 'C' : case 'd' : case 'D' :
      break;
    default: // Rev '\0', 'A', 'B', and 'E'
      return gxd_error = gxDBASE_NO_ERROR;
  }

  // Ensure that this is a valid database block
  gxBlockHeader block_header;
  if(ReadBlockHdr(block_header, block_address) != gxDBASE_NO_ERROR)
    return gxd_error;

  return Write(&hdr, sizeof(gxRecordLockHeader));
}

int gxDatabase::LockRecord(gxDatabaseLockType l_type, FAU_t block_address)
// Lock block at the specified address. Returns a non-zero value
// if the record cannot be locked or the lock variable cannot be
// changed because it is exclusive or another thread is currently 
// updating it.
{
  switch(rev_letter) {
    case 'c' : case 'C' : case 'd' : case 'D' :
      break;
    default: // Rev '\0', 'A', 'B', and 'E'
      return gxd_error = gxDBASE_NO_ERROR;
  }

  // Read the record lock to ensure that this record has a lock header
  // and to keep the header in sync during multiple file access.
  gxRecordLockHeader lock_header;
  if(ReadRecordLockHdr(lock_header, block_address) != gxDBASE_NO_ERROR) 
    return gxd_error;

  // Cannot modifiy this lock until the thread holding it releases the
  // lock header.
  if(lock_header.record_lock_protect != (gxUINT32)0) {
    gxd_error = gxDBASE_RECORDLOCK_ACCESS_ERROR;
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxd_error;
#endif
  }
  
  if(block_address == gxCurrAddress) block_address = FilePosition();
  FAU_t header_address = block_address+sizeof(gxBlockHeader);

  switch(l_type) {
    case gxDBASE_WRITELOCK :
      // Cannot obtain an exclusive lock until all the threads in the
      // read lock queue have finished
      if(lock_header.record_read_lock != (gxUINT32)0) {
	gxd_error = gxDBASE_RECORDLOCK_ERROR;
#ifdef __CPP_EXCEPTIONS__
	throw gxCDatabaseException();
#else
	return gxd_error;
#endif
      }

      if(lock_header.record_write_lock == (gxUINT32)0) {
	lock_header.record_write_lock = 1;
      }
      else { 
	gxd_error = gxDBASE_RECORDLOCK_ERROR;
#ifdef __CPP_EXCEPTIONS__
	throw gxCDatabaseException();
#else
	return gxd_error;
#endif
      }
      
      lock_header.record_lock_protect = 1;
      if(Write(&lock_header.record_lock_protect, sizeof(gxUINT32),
	       header_address) != gxDBASE_NO_ERROR)
	return gxd_error;

      // Skip over the read lock member
      SeekTo(header_address + (sizeof(gxUINT32) * 2));
      if(Write(&lock_header.record_write_lock, sizeof(gxUINT32)) !=
	 gxDBASE_NO_ERROR)
	return gxd_error;

      lock_header.record_lock_protect = (gxUINT32)0;
      if(Write(&lock_header.record_lock_protect, sizeof(gxUINT32),
	       header_address) != gxDBASE_NO_ERROR)
	return gxd_error;
      break;

    case gxDBASE_READLOCK :
      // This lock is exclusively owned and cannot be read locked
      // until the thread holding this lock release the exclusive lock.
      if(lock_header.record_write_lock == 1) {
	gxd_error = gxDBASE_RECORDLOCK_ERROR;
#ifdef __CPP_EXCEPTIONS__
	throw gxCDatabaseException();
#else
	return gxd_error;
#endif
      }

      if(lock_header.record_read_lock < 0xFFFFFFFF) {
	lock_header.record_read_lock++;
      }
      else {
	gxd_error = gxDBASE_RECORDLOCK_ERROR;
#ifdef __CPP_EXCEPTIONS__
	throw gxCDatabaseException();
#else
	return gxd_error;
#endif
      }
      
      lock_header.record_lock_protect = 1;
      if(Write(&lock_header.record_lock_protect, sizeof(gxUINT32),
	       header_address) != gxDBASE_NO_ERROR)
	return gxd_error;

      if(Write(&lock_header.record_read_lock, sizeof(gxUINT32)) != 
	 gxDBASE_NO_ERROR)
	return gxd_error; 

      lock_header.record_lock_protect = (gxUINT32)0;
      if(Write(&lock_header.record_lock_protect, sizeof(gxUINT32),
	       header_address) != gxDBASE_NO_ERROR)
	return gxd_error;
      break;

    default: // Invalid lock type specified
      gxd_error =  gxDBASE_INVALID_LOCK_TYPE;
#ifdef __CPP_EXCEPTIONS__
      throw gxCDatabaseException();
#else
      return gxd_error;
#endif
  }

  return gxd_error = gxDBASE_NO_ERROR;
}

int gxDatabase::UnlockRecord(gxDatabaseLockType l_type, FAU_t block_address)
// Unlock the record and reset the lock protect value. Returns a non-zero
// value if the record cannot be unlocked or the lock variable cannot 
// be changed because it is exclusive or another thread is currently 
// updating it.
{
  switch(rev_letter) {
    case 'c' : case 'C' : case 'd' : case 'D' :
      break;
    default: // Rev '\0', 'A', 'B', and 'E'
      return gxd_error = gxDBASE_NO_ERROR;
  }

  // Read the record lock to ensure that this record has a lock header
  // and to keep the header in sync during multiple file access.
  gxRecordLockHeader lock_header;
  if(ReadRecordLockHdr(lock_header, block_address) != gxDBASE_NO_ERROR) 
    return gxd_error;

  // Cannot modifiy this lock until the thread holding it releases the
  // lock header.
  if(lock_header.record_lock_protect != (gxUINT32)0) {
    gxd_error = gxDBASE_RECORDLOCK_ACCESS_ERROR;
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxd_error;
#endif
  }
  
  if(block_address == gxCurrAddress) block_address = FilePosition();
  FAU_t header_address = block_address+sizeof(gxBlockHeader);

  switch(l_type) {
    case gxDBASE_WRITELOCK :
      if(lock_header.record_write_lock == (gxUINT32)0)
	return gxDBASE_NO_ERROR;
      else
	lock_header.record_write_lock = (gxUINT32)0;
      
      lock_header.record_lock_protect = 1;   
      if(Write(&lock_header.record_lock_protect, sizeof(gxUINT32),
	       header_address) != gxDBASE_NO_ERROR)
	return gxd_error;

      // Skip over the read lock member
      SeekTo(header_address + (sizeof(gxUINT32) * 2));
      if(Write(&lock_header.record_write_lock, sizeof(gxUINT32)) !=
	 gxDBASE_NO_ERROR)
	return gxd_error;

      lock_header.record_lock_protect = (gxUINT32)0;
      if(Write(&lock_header.record_lock_protect, sizeof(gxUINT32),
	       header_address) != gxDBASE_NO_ERROR)
	return gxd_error;
      break;

    case gxDBASE_READLOCK :
      if(lock_header.record_read_lock == (gxUINT32)0)      // This record is not locked
	return gxd_error = gxDBASE_NO_ERROR;
      else if(lock_header.record_read_lock  > (gxUINT32)0) // Prevent read lock rollover
      	lock_header.record_read_lock--;
      else
	lock_header.record_read_lock = (gxUINT32)0;
      
      lock_header.record_lock_protect = 1;
      if(Write(&lock_header.record_lock_protect, sizeof(gxUINT32),
	       header_address) != gxDBASE_NO_ERROR)
	return gxd_error;

      if(Write(&lock_header.record_read_lock, sizeof(gxUINT32)) != 
	 gxDBASE_NO_ERROR)
	return gxd_error; 

      lock_header.record_lock_protect = (gxUINT32)0;
      if(Write(&lock_header.record_lock_protect, sizeof(gxUINT32),
	       header_address) != gxDBASE_NO_ERROR)
	return gxd_error;
      break;

    default: // Invalid lock type specified
      gxd_error =  gxDBASE_INVALID_LOCK_TYPE;
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxd_error;
#endif
  }

  return gxd_error = gxDBASE_NO_ERROR;
}

gxDatabaseError gxDatabase::ResetRecordLock(FAU_t block_address)
// Reset the block's record lock. This function will clear
// all the record lock fields without testing the lock or the
// lock protect. Returns a non-zero value to indicate an error
// condition or zero if successful.
{
  switch(rev_letter) {
    case 'c' : case 'C' : case 'd' : case 'D' :
      break;
    default: // Rev '\0', 'A', 'B', and 'E'
      return gxd_error = gxDBASE_NO_ERROR;
  }

  gxRecordLockHeader lock_header;
  InitRecordLockHdr(lock_header);
  return WriteRecordLockHdr(lock_header, block_address);
}
// ==============================================================
// Database Record Lock Functions (END HERE)
// ==============================================================

// ==============================================================
// Database File Stat Functions (BEGIN HERE)
// ==============================================================
void gxDatabase::InitFileStatsHdr(gxFileStatsHeader &hdr)
// Initialize a the lock header for a newly 
// constructed file lock header.
{
  hdr.num_blocks = (FAU)0;
  hdr.deleted_blocks = (FAU)0;
  hdr.removed_blocks = (FAU)0;  
}

gxDatabaseError gxDatabase::WriteFileStatsHdr(const gxFileStatsHeader &hdr)
// Write the lock header to the file. Returns a non-zero value
// to indicate an error condition or zero if successful.
{
  switch(rev_letter) {
    case 'd' : case 'D' : case 'e' : case 'E' :
      break;
    default: // Rev 'A', 'B', and 'C'
      return gxd_error = gxDBASE_NO_ERROR;
  }

  return Write(&hdr, sizeof(gxFileStatsHeader), 
	       (sizeof(gxFileHeader)+sizeof(gxFileLockHeader)));
}

gxDatabaseError gxDatabase::ReadFileStatsHdr(gxFileStatsHeader &hdr)
// Read the lock header from the file. Returns a non-zero value to
// indicate an error condition or zero if successful.
{
  switch(rev_letter) {
    case 'd' : case 'D' : case 'e' : case 'E' :
      break;
    default: // Rev 'A', 'B', and 'C'
      InitFileStatsHdr(hdr);
      return gxd_error = gxDBASE_NO_ERROR;
  }

  return Read(&hdr, sizeof(gxFileStatsHeader),
	      (sizeof(gxFileHeader)+sizeof(gxFileLockHeader)));
}
// ==============================================================
// Database File Stat Functions (END HERE)
// ==============================================================

#ifdef __BCC32__
#pragma warn .8080
#pragma warn .8071
#pragma warn .8072
#endif

// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
