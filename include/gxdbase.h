// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxdbase.h
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/04/1997 
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

The gxDatabase engine is a low-level file handler used to 
manipulate contiguous blocks of binary data. Low-level methods 
include read, write, and seek operations, create and open functions, 
linear navigation, and large file support. Optional methods include 
CRC checking, file and node locking.

Changes:
==============================================================
02/03/2002: Changed gxDatabase::Write() default arguments for 
flushing and bit testing. The Write() function will not flush
open file buffers and perform CRC testing by default. The caller
or derived class must perform file flushing and bit testing as
required by the application.

02/08/2002: All gxDatabase functions using non-persistent file 
address units have been modified to use FAU_t data types instead 
of FAU types. FAU_t types are native or built-in integer types 
and require no additional overhead to process. All persistent 
file address units still use the FAU integer type, which is a 
platform independent type allowing database files to be shared 
across multiple platforms.

02/22/2002: Added the gxDatabase::file_stats_header data member 
to keep persistent file statistics for the number of blocks 
allocated, deleted, and removed. The gxFileStatsHeader data 
structure is a new type declared in the gxheader.h include 
file added specifically for gxDatabase revisions 'D' and 'E'. 
Using persistent file statistics significantly reduces the 
amount of overhead normally required to obtain stats by 
sequentially searching the database file for normal, deleted, 
and removed blocks.
 
02/23/2002: Add three new functions to support persistent file 
statistics in revisions 'D' and 'E':
gxDatabaseError gxDatabase::WriteFileStatsHdr(const gxFileStatsHeader &hdr);
gxDatabaseError gxDatabase::ReadFileStatsHdr(gxFileStatsHeader &hdr);
void gxDatabase::InitFileStatsHdr(gxFileStatsHeader &hdr);

03/24/2002: Added the gxDatabase::Release() synchronization 
function. Used to signal to the class destructor not to close 
the file when an object is deleted or no longer visible.

03/25/2002: Added the gxdFPTR *gxDatabase::GetFP() function 
used to return an object's file pointer.

10/16/2002: Added the gxDatabase::TestBlockHeader() function used 
to test a block header to ensure that this is a valid block header 
and not a block of binary data containing words equal to a block 
checkword.

02/27/2003: Added the gxDatabase::Tell() function used to tell the 
current stream pointer position in the open file.
==============================================================
*/
// ----------------------------------------------------------- //  
#ifndef __GX_DATABASE_HPP__
#define __GX_DATABASE_HPP__

#include "gxdlcode.h"

#include "gxcrc32.h"
#include "gxdfptr.h"
#include "gxderror.h"

class GXDLCODE_API gxDatabase
{
public:
  gxDatabase();
  virtual ~gxDatabase();

private:
  // Disallow copying and assignment to prevent multiple copies of
  // database objects. Forcing pointer semantics to helps to ensure
  // the safe deletion or modification of a database object.
  gxDatabase(const gxDatabase &ob) { }
  void operator=(const gxDatabase &ob) { }

public: // Database functions
  virtual gxDatabaseError Create(const char *fname, 
				 FAU_t static_size = (FAU_t)0,
			 __SBYTE__ RevisionLetter = gxDatabaseRevisionLetter);
  virtual gxDatabaseError Open(const char *fname, 
			       gxDatabaseAccessMode mode = gxDBASE_READWRITE);
  virtual gxDatabaseError Close();
  virtual gxDatabaseError Flush();
  gxDatabaseError Read(void *buf, __ULWORD__ bytes, 
		       FAU_t file_address = gxCurrAddress);
  gxDatabaseError Write(const void *buf, __ULWORD__ bytes, 
			FAU_t file_address = gxCurrAddress,
			int flushdb = 0, int bit_test = 0);
  gxDatabaseError Seek(FAU_t offset, 
		       gxDatabaseSeekMode mode = gxDBASE_SEEK_BEG);
  FAU_t SeekTo(FAU_t file_address);
  gxStreamPos FilePosition();
  gxdFPTR *GetFP() { return fp; }
  gxStreamPos Tell() { return FilePosition(); }
 
public: // Allocation/De-allocation functions
  virtual FAU_t Alloc(__ULWORD__ bytes,
	      gxDatabaseReclaimMethod method = gxDBASE_RECLAIM_FIRSTFIT);
  FAU_t ReclaimBestFit(__ULWORD__ bytes);
  FAU_t ReclaimFirstFit(__ULWORD__ bytes);
  int Delete(FAU_t object_address, int remove_object = 0); 
  int Remove(FAU_t object_address);
  int UnDelete(FAU_t object_address);

protected: // Internal processing functions
  gxDatabaseError InitFileHdr();
  
public: // Functions used to calculate file offsets
  size_t FileHeaderSize() const;
  size_t FileHeaderSize(); 
  size_t BlockHeaderSize() const;
  size_t BlockHeaderSize();
  __ULWORD__ ObjectLength(FAU_t object_address = gxCurrAddress);
  __ULWORD__ BlockLength(FAU_t object_address = gxCurrAddress);

public:  // File Status functions
  int IsOK() { return ((is_ok == 1) && (is_open == 1)); }
  int IsOK() const { return ((is_ok == 1) && (is_open == 1)); }
  int IsOpen() { return ((is_ok == 1) && (is_open == 1)); }
  int IsOpen() const { return ((is_ok == 1) && (is_open == 1)); }
  int ReadyForReading() const { return ready_for_reading == 1; }
  int ReadyForReading() { return ready_for_reading == 1; }
  int ReadyForWriting() const { return ready_for_writing == 1; }
  int ReadyForWriting() { return ready_for_writing == 1; }

public: // File and block header functions
  gxDatabaseError ReadFileHdr();
  gxDatabaseError WriteFileHdr();
  gxDatabaseError ReadBlockHdr(gxBlockHeader &hdr, 
			       FAU_t block_address = gxCurrAddress);
  gxDatabaseError WriteBlockHdr(const gxBlockHeader &hdr, 
				FAU_t block_address = gxCurrAddress);

public:  // Exception handling functions
  gxDatabaseError GetDatabaseError() { return gxd_error; }
  gxDatabaseError GetDatabaseError() const { return gxd_error; }
  gxDatabaseError SetDatabaseError(gxDatabaseError err) {
    return gxd_error = err;
  }
  gxDatabaseError ResetDatabaseError() { return gxd_error = gxDBASE_NO_ERROR; }
  const char *DatabaseExceptionMessage();

public: // Static file statistics
  const char *GetSignature() const;
  char *GetSignature();
  char GetRevLetter() { return rev_letter; }
  const char GetRevLetter() const { return rev_letter; }
  FAU GetVersion() const { return file_header.gxd_ver; }
  FAU GetVersion() { return file_header.gxd_ver; }
  const char *DatabaseName() const { return (const char *)file_name; }
  char *DatabaseName() { return file_name; }
  FAU_t StaticArea();
    
public: // Dynamic file statistics
  int TestFileHeader(); // Keeps database file header's dynamic data in sync
  int TestBlockHeader(const gxBlockHeader &hdr); // Test for valid blocks
  FAU GetDatabaseFreeSpace();
  FAU GetEOF();
  FAU GetHeapStart();
  FAU GetHighestBlock();
  FAU_t TotalBlocks();  // Return total number of Blocks
  FAU_t NormalBlocks(); 
  FAU_t DeletedBlocks(FAU_t *d, FAU_t *r);
  
  // 02/25/2002: Depreciated function include for backward compatibility only
  FAU DeleteBlocks(FAU *d = 0, FAU *r = 0);
  
public: // Linear navigation methods
  FAU_t FindFirstBlock(FAU_t offset = (FAU_t)0);  // Finds first block
  FAU_t FindPrevBlock(FAU_t offset);              // Finds previous block 
  FAU_t FindNextBlock(FAU_t offset = (FAU_t)0);   // Finds next after first 
  FAU_t FindFirstObject(FAU_t offset = (FAU_t)0); // Finds first object address
  FAU_t FindPrevObject(FAU_t offset);             // Finds the previous object 
  FAU_t FindNextObject(FAU_t offset = (FAU_t)0);  // Finds next after first 
  
public: // General purpose file utilities
  static int Exists(const char *fname);
  static FAU_t FileSize(const char *fname);
  
public: // Revision letter functions
  void SetRevisionLetter(__SBYTE__ RevisionLetter);

public: // 32-bit CRC checksum routines (revision A and higher)
  __ULWORD__ CalcChecksum(__ULWORD__ bytes, FAU_t file_address, 
			  int mem_alloc = 1);
  gxUINT32 WriteObjectChecksum(FAU_t object_address);
  int ReadObjectChecksum(FAU_t object_address, __ULWORD__ *object_crc = 0,
			 __ULWORD__ *calc_crc = 0);

public: // File lock functions (revision B and higher)
  void InitFileLockHdr(gxFileLockHeader &hdr);
  gxDatabaseError ResetFileLock();
  gxDatabaseError WriteFileLockHdr(const gxFileLockHeader &hdr);
  gxDatabaseError ReadFileLockHdr(gxFileLockHeader &hdr);
  int LockFile(gxDatabaseLockType l_type = gxDBASE_WRITELOCK);
  int UnlockFile(gxDatabaseLockType l_type = gxDBASE_WRITELOCK);

public: // Record lock functions (revision C and higher)
  void InitRecordLockHdr(gxRecordLockHeader &hdr);
  gxDatabaseError ResetRecordLock(FAU_t block_address = gxCurrAddress);
  gxDatabaseError ReadRecordLockHdr(gxRecordLockHeader &hdr, 
				    FAU_t block_address = gxCurrAddress);
  gxDatabaseError WriteRecordLockHdr(const gxRecordLockHeader &hdr, 
				     FAU_t block_address = gxCurrAddress);
  int LockRecord(gxDatabaseLockType l_type = gxDBASE_WRITELOCK,
		 FAU_t block_address = gxCurrAddress);
  int UnlockRecord(gxDatabaseLockType l_type = gxDBASE_WRITELOCK,
		   FAU_t block_address = gxCurrAddress);

protected:
  char file_name[gxMaxNameLength];    // Open database file name
  char rev_letter;                    // Database revision letter
  gxFileHeader file_header;           // Database file header
  gxdFPTR *fp;                        // Stream file handle
  gxDatabaseOperation last_operation; // Last I/O operation preformed
  gxDatabaseError gxd_error;          // Last reported file error

  // File status members
  int is_ok;             // Used to signal a fatal error condition
  int is_open;           // True if the file is open
  int ready_for_reading; // True if the file is ready for reading
  int ready_for_writing; // True if the file is ready for writing

public: // Static data members used in place of global variables
  static __SBYTE__ gxSignature[gxSignatureSize]; // Signature for database files
  static FAU gxVersion;                // Version number database files
  static gxUINT32 gxInternalCheckWord; // Synchronization word

public: // Overloaded operators
  int operator!() const { return ((is_ok == 0) || (is_open == 0)); }
  int operator!() { return ((is_ok == 0) || (is_open == 0)); }
  operator const int () const { return ((is_ok == 1) && (is_open == 1)); }
  operator int () { return ((is_ok == 1) && (is_open == 1)); }

protected: // Data members added to keep persistent stats
  gxFileStatsHeader file_stats_header; // Database file statistics

public: // Rev 'D' and higher file stat functions
  gxDatabaseError WriteFileStatsHdr(const gxFileStatsHeader &hdr);
  gxDatabaseError ReadFileStatsHdr(gxFileStatsHeader &hdr);
  void InitFileStatsHdr(gxFileStatsHeader &hdr);

public: // Synchronization functions
  void Release();
};

#endif // __GX_DATABASE_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
