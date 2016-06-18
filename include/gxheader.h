// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxheader.h
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

This include file contains type definitions, constants,
enumerations, and data structures needed to create 32-bit
and 64-bit database files. 

Changes:
==============================================================
02/08/2002: All gxheader constants using non-persistent file 
address units have been modified to use FAU_t data types instead 
of FAU types. FAU_t types are native or built-in integer types 
and require no additional overhead to process. All persistent 
file address units still use the FAU integer type, which is a 
platform independent type allowing database files to be shared 
across multiple platforms.

02/22/2002: Added the gxFileStatsHeader data structure to 
support persistent file statistics in gxDatabase revisions 
'D' and 'E'.

03/24/2002: Changed the length of the gxMaxNameLength constant
from 255 to 1024. This change was made to account for long file 
names plus directory information.

03/12/2003: Added the gxAckBlock constant used as a short form 
of the gxAcknowledgeBlock constant.

03/12/2003: Added the gxRestartServer constant used to send a 
restart message to the client or server.

03/18/2003: Added additional block control constants used with the
gxStream class.

03/18/2003: Add authentication constants used with applications that 
require username and password auth. 

03/18/2003: Added the gxDBConnectionHeader, gxAuthHeader, and
gxWarningBanner headers.

07/14/2003: Added session certificate header and constants.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_BLOCK_HEADER_HPP__
#define __GX_BLOCK_HEADER_HPP__

#include "gxdlcode.h"

// --------------------------------------------------------------
// Include files
// --------------------------------------------------------------
#include <string.h>
#include "gxdtypes.h"
#include "gxint32.h"
#include "gxuint32.h"
#include "gxd_ver.h"

#if defined (__64_BIT_DATABASE_ENGINE__)
#include "gxint64.h"
#include "gxuint64.h"
#endif
// --------------------------------------------------------------

// --------------------------------------------------------------
// Type definitions 
// --------------------------------------------------------------
// typedefs for 64-bit and 32-bit file operations and stream position
#if defined (__64_BIT_DATABASE_ENGINE__)
typedef gxINT64 FAU; // 64-bit File Address Unit, physical file address type
typedef __LLWORD__ FAU_t;       // Native 64-bit FAU type
typedef __LLWORD__ gxStreamPos; // 64-bit stream position
#else // Use the 32-bit version by default
typedef gxINT32 FAU; // 32-bit File Address Unit, physical file address type
typedef __LWORD__ FAU_t;       // Native 32-bit FAU type
typedef __LWORD__ gxStreamPos; // 32-bit stream position
#endif

// CRC-32 checksum used to detect bit errors
typedef gxUINT32 gxChecksum;
// --------------------------------------------------------------

// --------------------------------------------------------------
// Constants
// --------------------------------------------------------------
// Constants for dynamic data attributes used by the first byte of the
// block header block_status member.
const __SBYTE__ gxBadBlock     = 'B'; // Bad Block
const __SBYTE__ gxDeletedBlock = 'D'; // Deleted Block 
const __SBYTE__ gxNormalBlock  = 'N'; // Normal Read/Write attribute 
const __SBYTE__ gxRemovedBlock = 'R'; // Removed Block

// This block was received from a remote device. Added in revision C
const __SBYTE__ gxRemoteDeviceBlock = 'V'; 

// Constants for block control characters used by the second byte of the
// block header "block_status" member for device control commands.
const __SBYTE__ gxAddRemoteBlock    = 'A'; // Add a remote block
const __SBYTE__ gxBadClientAuth     = 'B'; // Client sent bad username/password
const __SBYTE__ gxBatchInsert       = 'b'; // Batch insert request
const __SBYTE__ gxChangeRemoteBlock = 'C'; // Change a remote block
const __SBYTE__ gxSessionCert       = 'c'; // Negotiate cli/serv certificates
const __SBYTE__ gxDeleteRemoteBlock = 'D'; // Delete a remote block
const __SBYTE__ gxBatchDelete       = 'd'; // Batch delete request
const __SBYTE__ gxAcessDatabase     = 'E'; // Request access to named database
const __SBYTE__ gxDatabaseList      = 'G'; // Sending current database listing
const __SBYTE__ gxRequestFailed     = 'F'; // The client request failed
const __SBYTE__ gxCloseConnection   = 'L'; // Close Client/Server connection
const __SBYTE__ gxKillServer        = 'K'; // Shutdown Client/Server
const __SBYTE__ gxOpenConnection    = 'N'; // Open new connection to the server
const __SBYTE__ gxOpenDatabase      = 'O'; // Open connection to specified DB
const __SBYTE__ gxDumpDatabase      = 'P'; // Dump all DB records to client
const __SBYTE__ gxRequestBlock      = 'R'; // Requesting a block
const __SBYTE__ gxSendBlock         = 'S'; // Sending raw data block
const __SBYTE__ gxRestartServer     = 'T'; // Restart Client/Server
const __SBYTE__ gxBadSessionCert    = 't'; // Bad session certificate
const __SBYTE__ gxAcknowledgeBlock  = 'W'; // Acknowledge a data block
const __SBYTE__ gxAckBlock          = 'W'; // Acknowledge a data block
const __SBYTE__ gxListInOrder       = '>'; // List database in order
const __SBYTE__ gxListInROrder      = '<'; // List database in reverse order
const __SBYTE__ gxQuery             = '?'; // Query with search criteria

// Constants used for client/server and local authentication
const char gxAuthRequired      = 'r'; // Username/password auth required
const char gxNoAuthRequired    = 'n'; // No username/password auth required
const char gxAuthAccountDB     = 'd'; // User account disabled
const char gxAuthAccountNA     = 'n'; // Access not allowed 
const char gxAuthAccountOK     = 'k'; // Access OK
const char gxAuthAccountPW     = 'p'; // User requested password change
const char gxAuthAccountRO     = 'o'; // Read-only account
const char gxAuthAccountUA     = 'u'; // User account unavailable
const char gxAuthRealmBasic    = 0;   // Basic auth (no encryption)
const char gxAuthRealmAES128   = 1;   // Auth using AES 128-bit encryption 
const char gxAuthRealmAES192   = 2;   // Auth using AES 192-bit encryption 
const char gxAuthRealmAES256   = 3;   // Auth using AES 256-bit encryption 
const char gxAuthRealmBlowFish = 4;   // Auth using BlowFish encryption 
const char gxAuthRealmTwoFish  = 5;   // Auth using TwoFish encryption 
const char gxAuthRealmMD5      = 6;   // Auth using MD5 hash
const char gxAuthRealmBase64   = 7;   // Auth using base64 encoding
const char gxAuthRealmEDS      = 8;   // Auth using EDS encoding

// Header version constants
const char gxAuthVersion1 = 1; // Auth header version

// Authentication header constants
const int gxUsernameLen      = 256;  // 256 char username  
const int gxPasswordLen      = 256;  // 256 char user password
const int gxPublicKeyLen     = 256;  // 2048-bit public encryption keys
const int gxSessionCertLen   = 1024; // 8192-bit session certificates
const int gxWarningBannerLen = 1497; // Max warning/motd/exit message length

// Database connection header constants
const int gxDBConnectionNameLen = 255; // Max length for a connection name

// Constants for file operations and stream position 
const int gxMaxNameLength   = 1024;      // Max length of names buffers
const gxUINT32 gxCheckWord  = 0xfefe;    // Default synchronization word
const FAU_t gxStartOfFile   = (FAU_t)0;  // First byte in the file
const FAU_t gxCurrAddress   = (FAU_t)-1; // Indicates current location
const FAU_t gxFSListCorrupt = (FAU_t)-1; // Free space list is corrupt

// Constants used to determine the 32-bit and 64-bit signature sizes
#if defined (__64_BIT_DATABASE_ENGINE__)
const int gxSignatureSize = 10;
#else // Use the 32-bit version by default
const int gxSignatureSize = 8; 
#endif
// --------------------------------------------------------------

// --------------------------------------------------------------
// Enumerations
// --------------------------------------------------------------
// gxDatabase access/open mode enumeration
enum gxDatabaseAccessMode { 
    gxDBASE_READONLY,   // Open file with read access only
    gxDABSE_WRITEONLY,  // Open file with write access only
    gxDBASE_READWRITE,  // Open file with read and write access
    gxDBASE_CREATE,     // Create the file if it does not exist
    gxDBASE_NO_CREATE,  // Do not create the file if it does not exist
    gxDBASE_TRUNCATE,   // Truncate the file  
    gxDBASE_APPEND,     // Append to the file
    gxDBASE_SHARE,      // Enable file sharing  (Platform specific)
    gxDBASE_EXCLUSIVE   // Disable file sharing (Platform specific)
  };

// gxDatabase I/O operation codes
enum gxDatabaseOperation{ 
  gxDBASE_READ,         // A read was performed
  gxDBASE_WRITE,        // A write operation was performed
  gxDBASE_REWIND,       // A rewind operation was performed
  gxDBASE_NO_OPERATION, // No operation was performed
  gxDBASE_SEEK          // A seek operation was preformed 
};

// gxDatabase seek mode enumeration
enum gxDatabaseSeekMode { 
  gxDBASE_SEEK_BEG, // Seek starting from the beginning of the file
  gxDBASE_SEEK_CUR, // Seek starting from the current location
  gxDBASE_SEEK_END  // Seek starting from the end of the file
};

// gxDatabase file/record lock enumeration
enum gxDatabaseLockType { 
  gxDBASE_READLOCK,
  gxDBASE_WRITELOCK
};

// gxDatabase Block reclamation methods
enum gxDatabaseReclaimMethod {
  gxDBASE_RECLAIM_NONE = 0, // Do not reclaim deleted blocks
  gxDBASE_RECLAIM_BESTFIT,  // Use the best fit reclamation method
  gxDBASE_RECLAIM_FIRSTFIT  // Use the first fit reclamation method
};
// --------------------------------------------------------------

// --------------------------------------------------------------
// Data Sturctures
// --------------------------------------------------------------
// Database File Header
struct GXDLCODE_API gxFileHeader // Database file header information
{ 
  gxFileHeader() { 
    gxd_fs_fptr = gxd_eof = gxd_hs_fptr = gxd_hb_fptr = (FAU)0;
    gxd_ver = (FAU)0; gxd_sig[0] = 0;
  }
  ~gxFileHeader() { }

  FAU gxd_fs_fptr; // Address to first block of free heap space
  FAU gxd_eof;     // Address of byte after end of file
  FAU gxd_hs_fptr; // Address of the start of the heap space
  FAU gxd_hb_fptr; // Highest allocated database block
  __SBYTE__ gxd_sig[gxSignatureSize]; // Signature used to ID a gxDatabase 
  FAU gxd_ver;     // Database engine version number
  
private: // Disallow copying and assignment
  gxFileHeader(const gxFileHeader &ob) { }
  void operator=(const gxFileHeader &ob) { }
};

// Database/Device Block Header 
struct GXDLCODE_API gxBlockHeader // Marks each data block
{
  gxBlockHeader() {
    block_check_word = gxCheckWord;
    block_status = gxNormalBlock;
    block_length = (gxUINT32)0;
    block_nd_fptr = (FAU)0;
  }
  ~gxBlockHeader() { }
  gxBlockHeader(const gxBlockHeader &ob) {
    block_check_word = ob.block_check_word; 
    block_length = ob.block_length; 
    block_status = ob.block_status;
    block_nd_fptr = ob.block_nd_fptr;
  }
  gxBlockHeader& operator=(const gxBlockHeader &ob) {
    block_check_word = ob.block_check_word; 
    block_length = ob.block_length; 
    block_status = ob.block_status;
    block_nd_fptr = ob.block_nd_fptr;
    return *this;
  }

  gxUINT32 block_check_word; // Block synchronization marker
  gxUINT32 block_length;     // Block length (object length + overhead)  
  gxUINT32 block_status;     // First byte  = status of the block's data 
                             // Second byte = block control commands
                             // Third byte  = reserved for future use
                             // Fourth byte = reserved for future use
  FAU block_nd_fptr;         // Pointer to next deleted block
};

// File lock header added to allow applications to lock the entire file during
// a multi-threaded/multi-machine read or write operation. The advisory 
// file locking scheme includes a lock protect member required to protect the 
// lock values during multiple file access. NOTE: The lock protect member is 
// required in the platform interoperable locking scheme because file lock 
// headers operate independently of the I/O subsystem and are manipulated
// by the database engine in the same manner as database blocks.
struct GXDLCODE_API gxFileLockHeader
{
  gxFileLockHeader() {
    file_lock_protect = (gxUINT32)0;
    file_read_lock = file_write_lock = (gxUINT32)0; 
  }
  ~gxFileLockHeader() { }
  gxFileLockHeader(const gxFileLockHeader &ob) {
    file_lock_protect = ob.file_lock_protect;
    file_read_lock = ob.file_read_lock;
    file_write_lock = ob.file_write_lock;
  }
  gxFileLockHeader& operator=(const gxFileLockHeader &ob) {
    file_lock_protect = ob.file_lock_protect;
    file_read_lock = ob.file_read_lock;
    file_write_lock = ob.file_write_lock;
    return *this;
  }

  // The lock protect is used to serialize access to the file lock
  // values. The locking scheme will allow a total of 2^32 or
  // 4,294,967,295 threads to read lock a file and a single
  // thread to write lock a file.
  gxUINT32 file_lock_protect; // Serialize access to the file lock members
  gxUINT32 file_read_lock;    // Shared (or read-only) file lock 
  gxUINT32 file_write_lock;   // Exclusive (or write-only) file lock
};

// Record lock header used by an application to lock a specific block during 
// a multi-threaded/multi-machine read or write operation. The advisory 
// record locking scheme includes a lock protect member required to protect 
// the lock values during multiple file access. NOTE: The lock protect member 
// is required in the platform interoperable locking scheme because record 
// lock headers operate independently of the I/O subsystem and are manipulated
// by the database engine in the same manner as database blocks.
struct GXDLCODE_API gxRecordLockHeader // Lock a contiguous set of bytes
{
  gxRecordLockHeader() {
    record_lock_protect = (gxUINT32)0;
    record_read_lock = record_write_lock = (gxUINT32)0;
  }
  ~gxRecordLockHeader() { }
  gxRecordLockHeader(const gxRecordLockHeader &ob) {
    record_lock_protect = ob.record_lock_protect;
    record_read_lock = ob.record_read_lock;
    record_write_lock = ob.record_write_lock;
  }
  gxRecordLockHeader& operator=(const gxRecordLockHeader &ob) {
    record_lock_protect = ob.record_lock_protect;
    record_read_lock = ob.record_read_lock;
    record_write_lock = ob.record_write_lock;
    return *this;
  }

  // The lock protect is used to serialize access to each record 
  // lock. The locking scheme will allow a total of 2^32 or
  // 4,294,967,295 threads to read lock a record and a single
  // thread to write lock a record.
  gxUINT32 record_lock_protect; // Serialize access to the record lock
  gxUINT32 record_read_lock;    // Shared (or read-only) file lock 
  gxUINT32 record_write_lock;   // Exclusive (or write-only) file lock
};

// Header added in revisions 'D' and 'E' used to record file
// statistics. In revision 'D' and revision 'E' the file stats 
// header will be written after the file lock header.
struct GXDLCODE_API gxFileStatsHeader
{
  gxFileStatsHeader() { 
    num_blocks = deleted_blocks = removed_blocks = (FAU)0;
  }
  ~gxFileStatsHeader() { }
  gxFileStatsHeader(const gxFileStatsHeader &ob) {
    num_blocks = ob.num_blocks;
    deleted_blocks = ob.deleted_blocks;
    removed_blocks = ob.removed_blocks;
  }
  gxFileStatsHeader& operator=(const gxFileStatsHeader &ob) {
    num_blocks = ob.num_blocks;
    deleted_blocks = ob.deleted_blocks;
    removed_blocks = ob.removed_blocks;
    return *this;
  }

  FAU num_blocks;     // Used to record the total number of blocks
  FAU deleted_blocks; // Used to record the number deleted blocks
  FAU removed_blocks; // Used to record the number removed blocks
};

// Header used to transfer a database connection list.
struct GXDLCODE_API gxDBConnectionHeader
{
  gxDBConnectionHeader() {
    memset(db_name, 0, gxDBConnectionNameLen);
    status = 0;
    auth_version = gxAuthVersion1;
    auth_realm = gxAuthRealmBasic;
    auth_required = gxNoAuthRequired;
  }
  ~gxDBConnectionHeader() { }
  gxDBConnectionHeader(const gxDBConnectionHeader &ob) {
    Copy(ob);
  }
  void operator=(const gxDBConnectionHeader &ob) {
    Copy(ob);
  }
  void Copy(const gxDBConnectionHeader &ob) {
    status = ob.status;
    auth_version = ob.auth_version;
    auth_realm = ob.auth_realm;
    auth_required = ob.auth_required;
    memmove(db_name, ob.db_name, gxDBConnectionNameLen);
  }

  char status;
  char auth_version;
  char auth_realm;
  char auth_required;
  char db_name[gxDBConnectionNameLen];
};

// Header used authenticate database users.
struct GXDLCODE_API gxAuthHeader
{
  gxAuthHeader() { Reset(); }
  ~gxAuthHeader() { 
    // Clear any buffer information before releasing this 
    // header so that no sensitive data is left in memory.
    Clear();
  }
  gxAuthHeader(const gxAuthHeader &ob) {
    Copy(ob);
  }
  void operator=(const gxAuthHeader &ob) {
    Copy(ob);
  }
  
  void Clear() {
    // NOTE: All encrypted/decrypted buffers should be cleared immediately 
    // after use so that no sensitive data is left in memory while other 
    // data is being processed. 
    memset(username, 0, gxUsernameLen);
    memset(password, 0, gxPasswordLen);
    memset(key, 0, gxPublicKeyLen);
  }
  void Reset() {
    auth_version = gxAuthVersion1;
    auth_realm = gxAuthRealmBasic;
    Clear();
  }
  void Copy(const gxAuthHeader &ob) {
    auth_version = ob.auth_version;
    auth_realm = ob.auth_realm;
    memmove(username, ob.username, gxUsernameLen); 
    memmove(password, ob.password, gxPasswordLen); 
    memmove(key, ob.key, gxPublicKeyLen);
  }

  char auth_version;
  char auth_realm;
  char username[gxUsernameLen];
  char password[gxPasswordLen];
  char key[gxPublicKeyLen];
};

// Header used during initial connection display a warning banner 
// and to tell the user if authentication is or is not required to 
// use a database.
struct GXDLCODE_API gxWarningBanner
{
  gxWarningBanner() {
    memset(warning_banner, 0, gxWarningBannerLen);
    auth_version = gxAuthVersion1;
    auth_realm = gxAuthRealmBasic;
    auth_required = gxNoAuthRequired;
  }
  ~gxWarningBanner() { }
  gxWarningBanner(const gxWarningBanner &ob) {
    Copy(ob);
  }
  void operator=(const gxWarningBanner &ob) {
    Copy(ob);
  }

  void Copy(const gxWarningBanner &ob) {
    auth_version = ob.auth_version;
    auth_realm = ob.auth_realm;
    auth_required = ob.auth_required;
    memmove(warning_banner, ob.warning_banner, gxWarningBannerLen);
  }

  char auth_version;
  char auth_realm;
  char auth_required;
  char warning_banner[gxWarningBannerLen];
};

// Header used to exchange site certificates 
struct GXDLCODE_API gxSessionCertHeader
{
  gxSessionCertHeader() { Reset(); }
  ~gxSessionCertHeader() { 
    // Clear any buffer information before releasing this 
    // header so that no sensitive data is left in memory.
    Clear();
  }
  gxSessionCertHeader(const gxSessionCertHeader &ob) {
    Copy(ob);
  }
  void operator=(const gxSessionCertHeader &ob) {
    Copy(ob);
  }
  
  void Clear() {
    // NOTE: All encrypted/decrypted buffers should be cleared immediately 
    // after use so that no sensitive data is left in memory while other 
    // data is being processed. 
    memset(key, 0, gxPublicKeyLen);
    memset(cert, 0, gxSessionCertLen);
  }
  void Reset() {
    Clear();
    auth_version = gxAuthVersion1;
    auth_realm = gxAuthRealmBasic;
  }
  void Copy(const gxSessionCertHeader &ob) {
    auth_version = ob.auth_version;
    auth_realm = ob.auth_realm;
    memmove(key, ob.key, gxPublicKeyLen);
    memmove(cert, ob.cert, gxSessionCertLen); 
  }

  char auth_version;
  char auth_realm;
  char key[gxPublicKeyLen];
  char cert[gxSessionCertLen];
};
// --------------------------------------------------------------

#endif // __GX_BLOCK_HEADER_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
