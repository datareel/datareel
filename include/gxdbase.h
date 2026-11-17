// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxdbase.h
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/04/1997 
// Date Last Modified: 10/26/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- // 
// ---------- Include File Description and Details  ---------- // 
// ----------------------------------------------------------- // 
/*
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.
 
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library. If not, see
<https://www.gnu.org/licenses/>.

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
#include "gxdpcache.h"
#include "gxderror.h"

namespace datareel {


class GXDLCODE_API gxDatabase
{
public:
  gxDatabase();
  virtual ~gxDatabase();

private:
  // Disallow copying and assignment to prevent multiple copies of
  // database objects. Forcing pointer semantics to helps to ensure
  // the safe deletion or modification of a database object.
  gxDatabase(const gxDatabase &) { }
  void operator=(const gxDatabase &) { }

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

public: // 10/31/2026: Durability and page cache functions (7.2)
  gxDatabaseError Sync(int full = 0);
  void SetSyncOnFlush(int sync) { sync_on_flush = sync; }
  int SyncOnFlush() const { return sync_on_flush; }
  gxDatabaseError EnablePageCache(size_t bytes = gxdDefaultPageCacheSize,
				  int use_map = 1);
  void DisablePageCache();
  gxdPageCache *PageCache() { return pcache; }
  // Counts the writes made through this object, so cached reads can
  // tell if the file changed. NoteFileChanged() counts a change made
  // another way (a journal rollback, for example).
  unsigned long WriteCount() const { return write_count; }
  void NoteFileChanged() { write_count++; }
 
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
  char GetRevLetter() const { return rev_letter; }
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

public: // 11/15/2026: the on-disk layout
  // DataReel 7.4 writes one layout on every platform, the canonical one: the 64-bit Linux layout, where the
  // header fields the structs keep as "long" (gxUINT32, gxINT32, a 32-bit FAU) take 8 bytes. Files written by
  // 64-bit Windows and by 32-bit builds before 7.4 have them as 4 bytes (the old 4-byte layout). A file's layout
  // is found when it is opened, and the file is then read and written in that layout, so its object addresses
  // never move; gxd_upgrade rewrites one into the canonical layout. The structs in memory are unchanged.
  int DiskLongSize() const { return disk_long; }  // 8 (canonical) or 4 (old)
  int DiskFAUSize() const { return disk_fau; }    // 8, or 4 for an old file of a 32-bit FAU build
  int IsOldLayout() const { return disk_long == 4; }
  // A file with no block cannot show its layout (64-bit FAU builds): canonical was taken (see SetOpenLayout())
  int LayoutGuessed() const { return layout_guessed; }
  // Before Open(): 0 finds the layout (the default), 8 or 4 takes that one (an empty old-layout file)
  void SetOpenLayout(int long_size) { open_layout = long_size; }
  // Before Create(): 0 or 8 the canonical layout (the default), 4 the old 4-byte layout (for old readers only)
  void SetCreateLayout(int long_size) { create_layout = long_size; }
  size_t DiskFileHdrSize() const;      // the gxFileHeader on disk
  size_t DiskLockHdrSize() const;      // a gxFileLockHeader or a gxRecordLockHeader on disk
  size_t DiskStatsHdrSize() const;     // the gxFileStatsHeader on disk
  size_t DiskBlockHdrSize() const;     // a gxBlockHeader on disk
  size_t DiskChecksumSize() const { return (size_t)disk_long; }
  gxDatabaseError ReadBlockHdrRaw(gxBlockHeader &hdr, FAU_t block_address = gxCurrAddress);
  gxDatabaseError WriteBlockHdrRaw(const gxBlockHeader &hdr, FAU_t block_address = gxCurrAddress);
  // A "long" field of the file's layout (lock words, checksums, InfoHog counts and lengths)
  gxDatabaseError ReadDiskLong(__ULWORD__ &v, FAU_t file_address = gxCurrAddress);
  gxDatabaseError WriteDiskLong(__ULWORD__ v, FAU_t file_address = gxCurrAddress);
  // A FAU field of the file's layout (gxBtree's node links)
  void PackFAU(unsigned char *p, FAU_t v) const;
  FAU_t UnpackFAU(const unsigned char *p) const;
  void PackLong(unsigned char *p, __ULWORD__ v) const;
  __ULWORD__ UnpackLong(const unsigned char *p) const;
  int RawLayout() const { return raw_ok; }   // the structs in memory are the file's layout (read as they are)

protected:
  void set_layout(int long_size);
  gxDatabaseError detect_layout();
  gxDatabaseError ReadFileHdrRaw(gxFileHeader &fh);
  gxDatabaseError WriteFileHdrRaw(const gxFileHeader &fh);
  gxDatabaseError ReadLock3(gxUINT32 &a, gxUINT32 &b, gxUINT32 &c, FAU_t file_address);
  gxDatabaseError WriteLock3(const gxUINT32 &a, const gxUINT32 &b, const gxUINT32 &c, FAU_t file_address);
  int disk_long, disk_fau, layout_guessed, raw_ok, open_layout, create_layout;

public: // 10/25/2026: Write notification
  // The hook is called after each successful Write(), so an application
  // can tell its own changes to a file apart from changes made by others
  // and update data that depends on the file, such as a MAC.
  void SetWriteHook(void (*hook)(void *), void *data) {
    write_hook = hook; write_hook_data = data;
  }

protected:
  void (*write_hook)(void *); // 10/25/2026: See SetWriteHook()
  void *write_hook_data;
  char file_name[gxMaxNameLength];    // Open database file name
  char rev_letter;                    // Database revision letter
  gxFileHeader file_header;           // Database file header
  gxdFPTR *fp;                        // Stream file handle
  gxdPageCache *pcache;               // 10/31/2026: Optional page cache
  int sync_on_flush;                  // 10/31/2026: Flush() also syncs
  unsigned long write_count;          // 10/31/2026: See WriteCount()
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
  operator int () const { return ((is_ok == 1) && (is_open == 1)); }
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

} // namespace datareel

#endif // __GX_DATABASE_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
