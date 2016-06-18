// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxderror.h
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/14/1996  
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

The gxdError functions and classes are used to catch and/or
record database exceptions that occur during run-time. This
implementation can be used with or without C++ built-in exception
handling. If C++ exception handling is not enabled with the
__CPP_EXCEPTIONS__ macro, then the gxDatabaseError enumerated
constants can be used to record database errors.

Changes:
==============================================================
08/14/2001: Added the gxDBASE_CACHE_ERROR error code to the 
gxDatabaseError enumeration.

03/12/2003: Added the gxDBASE_OBJECT_NOT_FOUND error code to 
the gxDatabaseError enumeration.

03/18/2003: Added database status enumeration and status 
message function.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_DATABASE_ERROR_HPP__
#define __GX_DATABASE_ERROR_HPP__

#include "gxdlcode.h"

enum gxDatabaseError { // Database error codes
  gxDBASE_NO_ERROR = 0,        // No errors reported
  gxDBASE_INVALID_CODE,        // Invalid error code
  gxDBASE_ACCESS_DELETED,      // Accessing a deleted block
  gxDBASE_ACCESS_VIOLATION,    // Access Violation
  gxDBASE_ASSERT_ERROR,        // Assertion failed
  gxDBASE_BAD_CLASS_ID,        // Wrong object type
  gxDBASE_BAD_KEY,             // Bad index key
  gxDBASE_BAD_OBJECT_ADDRESS,  // Bad object address
  gxDBASE_BAD_REFERENCE,       // Bad Reference
  gxDBASE_BLOCK_ALLOC_ERROR,   // Block allocation error
  gxDBASE_BUFFER_OVERFLOW,     // Buffer overflow error
  gxDBASE_CACHE_ERROR,         // Error acquiring or loading cache bucket
  gxDBASE_CACHE_FULL,          // Cache full
  gxDBASE_CHECKSUM_ERROR,      // Checksum Error
  gxDBASE_DIVIDEBY_ZERO,       // Divide By Zero Error
  gxDBASE_DUPLICATE_ENTRY,     // Duplicate entry error
  gxDBASE_ENTRY_NOT_FOUND,     // Specified entry was not found
  gxDBASE_EOF_ERROR,           // Unexpected end of file
  gxDBASE_FILE_CLOSE_ERROR,    // Error closing file
  gxDBASE_FILE_CORRUPT,        // File corrupted
  gxDBASE_FILE_CREATION_ERROR, // Error creating file
  gxDBASE_FILE_EXISTS,         // File already exists
  gxDBASE_FILE_NOT_OPEN_ERROR, // Trying to use a closed file
  gxDBASE_FILE_NOT_READY,      // File not ready (failed or closed file)
  gxDBASE_FILE_NOT_WRITEABLE,  // Could not write to file 
  gxDBASE_FILE_OPEN_ERROR,     // Error opening file
  gxDBASE_FILE_POSITION_ERROR, // Cannot obtain the current file position
  gxDBASE_FILE_READ_ERROR,     // Error reading file  
  gxDBASE_FILE_SEEK_ERROR,     // Error seeking in file
  gxDBASE_FILE_WRITE_ERROR,    // Error writing to file
  gxDBASE_INSERTION_ERROR,     // Error inserting database entry
  gxDBASE_MEM_ALLOC_ERROR,     // Memory allocation error
  gxDBASE_NO_DATABASE_OPEN,    // No database open
  gxDBASE_NO_FILE_EXISTS,      // No such file exists
  gxDBASE_NO_OBJECTS_EXIST,    // No objects exist
  gxDBASE_NULL_PTR,            // Accessing a null pointer 
  gxDBASE_OBJECT_EXISTS,       // Object already exists
  gxDBASE_OBJECT_NOT_FOUND,    // No such object exists
  gxDBASE_OPEN_FILE_REFERENCE, // Another object is referencing this file
  gxDBASE_OVERFLOW,            // Math overflow
  gxDBASE_PARSE_ERROR,         // Parse error
  gxDBASE_PATH_ERROR,          // Invalid path
  gxDBASE_READ_ONLY_FILE,      // Trying to write to read-only file
  gxDBASE_STACK_EMPTY,         // Stack empty
  gxDBASE_STACK_FULL,          // Stack full
  gxDBASE_SYNC_ERROR,          // Synchronization Error
  gxDBASE_UNDERFLOW,           // Math under-flow
  gxDBASE_WRONG_FILE_TYPE,     // Wrong file type

  // Persistent lock error codes
  gxDBASE_INVALID_LOCK_TYPE,        // Invalid lock type specified
  gxDBASE_FILELOCK_ACCESS_ERROR,    // The file lock cannot be accessed
  gxDBASE_FILELOCK_ERROR,           // Error locking the file
  gxDBASE_RECORDLOCK_ACCESS_ERROR,  // The record lock cannot be accessed
  gxDBASE_RECORDLOCK_ERROR,         // Error locking a record

  // gxDatabase debug manager error codes
  gxDBASE_BAD_HEADER,      // DB header is damaged or does not exist
  gxDBASE_REV_MISMATCH,    // DB file revision letters do not match
  gxDBASE_VER_MISMATCH,    // DB file version numbers do not match
  gxDBASE_DBEOF_ERROR,     // Actual len is longer than recorded len
  gxDBASE_HEAPSTART_ERROR, // Bad heap start value
  gxDBASE_FREESPACE_ERROR, // Bad free space value
  gxDBASE_HIGHBLOCK_ERROR, // Bad highest block value
  gxDBASE_NOBLOCK_ERROR,   // No database blocks found
  gxDBASE_BADBLOCK_ERROR,  // Bad database block(s)
  gxDBASE_INVALID_FILE,    // Not a valid gxDatabase file

  // gxRDBMS error codes
  gxDBASE_BAD_TABLE,          // Bad table
  gxDBASE_BAD_TABLE_DEF,      // Bad table definition
  gxDBASE_BAD_TABLE_FIELD,    // Bad table field
  gxDBASE_BAD_TABLE_HEADER,   // Bad table header
  gxDBASE_BAD_TABLE_ID,       // Bad table ID
  gxDBASE_BAD_RECORD,         // Bad record
  gxDBASE_BAD_RECORD_DEF,     // Bad record definition
  gxDBASE_BAD_FIELD_TYPE,     // Bad field type
  gxDBASE_BAD_FIELD_LENGTH,   // Bad field length
  gxDBASE_BAD_COL_LENGTH,     // Bad column length
  gxDBASE_RECORD_EXISTS,      // Record already exists
  gxDBASE_RECORD_NOT_FOUND,   // No such record exists
  gxDBASE_BAD_KEY_DEF,        // Bad key definition 
  gxDBASE_BAD_FAMILY_DEF,     // Bad family definition
  gxDBASE_BAD_FKEY_DEF,       // Bad foreign definition
  gxDBASE_VIRTUAL_LIST_ERROR, // Error building virtual list
  gxDBASE_BAD_RANGE_LIMIT,    // Bad range limit specified
  gxDBASE_ILLEGAL_CHAR,       // Illegal table or field name character

  // Data file and Index file error codes
  gxDBASE_DATAFILE_EMPTY,    // Data file is empty
  gxDBASE_BAD_DATAFILE,      // Bad data file
  gxDBASE_BAD_DATAFILE_NAME, // Bad data file name
  gxDBASE_INDEX_EMPTY,       // Index file is empty
  gxDBASE_BAD_INDEX_FILE,    // Bad index file
  gxDBASE_BAD_INDEX_NUMBER,  // Bad index file number 
  gxDBASE_BAD_INDEX_NAME,    // Bad index file name
  gxDBASE_FILE_RENAME_ERROR, // Error renaming file
  gxDBASE_FILE_DELETE_ERROR  // Error deleting file
};

enum gxDatabaseStatus { // Database status codes
  gxDBASE_NO_STATUS = 0,
  gxDBASE_INVALID_STATUS,
  gxDBASE_OPEN_RW,
  gxDBASE_OPEN_RO,
  gxDBASE_UNAVAIL,
  gxDBASE_LOCKED,
  gxDBASE_USER_NOT_ALLOWED,
  gxDBASE_GROUP_NOT_ALLOWED,
  gxDBASE_NO_ACCESS,
  gxDBASE_TESTING,
  gxDBASE_DOES_NOT_EXIST,
  gxDBASE_NO_USER_ACCESS,
  gxDBASE_REBUILDING_DATAFILE,
  gxDBASE_REBUILDING_INDEXES
};

// Standalone function used to report database errors
GXDLCODE_API const char *gxDatabaseExceptionMessage(gxDatabaseError err);

// Standalone function used to report database status messages
GXDLCODE_API const char *gxDatabaseStatusMessage(gxDatabaseStatus status);

#ifdef __CPP_EXCEPTIONS__
// Class declarations for exceptions representing program errors. 
// This implementation is provided for use C++'s built-in exception
// handling routines.

// A fatal file error condition has been reached. All database engine
// calls must catch this exception and evaluate it by testing the
// gxDatabase or gxDatabase64 internal error variable.
class GXDLCODE_API gxCDatabaseException { };  

#endif // __CPP_EXCEPTIONS__

#endif // __GX_DATABASE_ERROR_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
