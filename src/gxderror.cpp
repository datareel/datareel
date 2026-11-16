// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxderror.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/14/1996  
// Date Last Modified: 10/26/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- // 
// ------------- Program Description and Details ------------- // 
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

The gxdError functions and classes are used to catch and/or
record database exceptions that occur during run-time. This
implementation can be used with or without C++ built-in exception
handling. If C++ exception handling is not enabled with the
__CPP_EXCEPTIONS__ macro, then the gxDatabaseError enumerated
constants can be used to record database errors.

Changes:
==============================================================
08/14/2001: Added the gxDBASE_CACHE_ERROR error message to the 
gxdExceptionMessages array.

03/12/2003: Added the gxDBASE_OBJECT_NOT_FOUND error message to 
the gxdExceptionMessages array.

03/18/2003: Added database status messages.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "gxderror.h"

namespace datareel {


// NOTE: This array must contain the same number of exceptions as the
// gxDatabaseError enumeration. 
const int gxdMaxExceptionMessages = 91;
const char *gxdExceptionMessages[gxdMaxExceptionMessages] = {
  "Database exception: No exception reported",               // None
  "Database exception: Invalid error code reported",         // Invalid
  "Database exception: Accessing a deleted block",           // AccessDeleted
  "Database exception: Access Violation",                    // AccessViolation
  "Database exception: Assertion failed",                    // AssertError
  "Database exception: Wrong object type",                   // BadClassID
  "Database exception: Bad index key",                       // BadKey 
  "Database exception: Bad object address",                  // BadObjectAddr
  "Database exception: Bad reference",                       // BadReference
  "Database exception: Block allocation error",              // BlockAllocError
  "Database exception: Buffer overflow error",               // BufferOverflow
  "Database exception: Error acquiring or loading cache bucket", // CacheError
  "Database exception: Cache full",                          // CacheFull
  "Database exception: Checksum error",                      // ChecksumError
  "Database exception: Divide by zero error",                // DivideByZero
  "Database exception: Duplicate entry error",               // DupEntryError
  "Database exception: Specified entry was not found",       // EntryNotFound
  "Database exception: Unexpected end of file was reached",  // EOFError
  "Database exception: Error closing file",                  // FileCloseError
  "Database exception: File is corrupt",                     // FileCorrupt
  "Database exception: Error creating file",                 // CreationError
  "Database exception: File already exists",                 // FileExists
  "Database exception: Trying to use a closed file",         // FileNotOpenErr
  "Database exception: File not ready for reading/writing",  // FileNotReady
  "Database exception: Could not write to file",             // NotWriteable
  "Database exception: Error opening file",                  // FileOpenError
  "Database exception: Cannot obtain a file position",       // FilePostionErr
  "Database exception: Error reading from file",             // FileReadError
  "Database exception: Error seeking in file",               // FileSeekError
  "Database exception: Error writing to file",               // FileWriteError
  "Database exception: Error inserting database entry",      // InsertionError
  "Database exception: Memory allocation error",             // MemoryAllocErr
  "Database exception: No database open",                    // NoDatabaseOpen
  "Database exception: No such file exists",                 // NoFileExists
  "Database exception: No objects exist database empty",     // NoObjectsExist
  "Database exception: Accessing a null pointer",            // NullPtr
  "Database exception: Object already exists",               // ObjectExists
  "Database exception: No such object exists",               // ObjectNotFound;
  "Database exception: Another object is referencing this file", // Referenced
  "Database exception: Math overflow error",                 // OverFlow
  "Database exception: Parse error",                         // ParseError
  "Database exception: Invalid path",                        // PathError
  "Database exception: Trying to write to a read-only file", // ReadOnlyFile
  "Database exception: Stack empty",                         // StackEmpty
  "Database exception: Stack full",                          // StackFull
  "Database exception: Synchronization Error",               // SyncError    
  "Database exception: Math under-flow error",               // UnderFlow
  "Database exception: Wrong file type",                     // WrongFileType

  // Persistent lock error messages
  "Database exception: Invalid lock type specified",          // LOCK_TYPE
  "Database exception: Cannot access the file lock header",   // FLK_ACCESS
  "Database exception: Cannot lock the file",                 // FLK_ERROR
  "Database exception: Cannot access the record lock header", // RLK_ACCESS
  "Database exception: Cannot lock the specified record",     // RLK_ERROR

  // gxDatabase debug manager error messages
  "Database exception: File header is damaged or does not exist",
  "Database exception: The file revision letters do not match",
  "Database exception: The file version numbers do not match",
  "Database exception: DB actual file length is longer than recorded length",
  "Database exception: Bad heap start value in database file",
  "Database exception: Bad free space value in database file",
  "Database exception: Bad highest block value in database file",
  "Database exception: No database blocks found in database file",
  "Database exception: Encountered bad database block(s) during block search",
  "Database exception: Not a valid gxDatabase file",

  // Reserved error messages (former gxRDBMS slots)
  "Database exception: Reserved error code",         // RESERVED_1
  "Database exception: Reserved error code",         // RESERVED_2
  "Database exception: Reserved error code",         // RESERVED_3
  "Database exception: Reserved error code",         // RESERVED_4
  "Database exception: Reserved error code",         // RESERVED_5
  "Database exception: Reserved error code",         // RESERVED_6
  "Database exception: Reserved error code",         // RESERVED_7
  "Database exception: Reserved error code",         // RESERVED_8
  "Database exception: Reserved error code",         // RESERVED_9
  "Database exception: Reserved error code",         // RESERVED_10
  "Database exception: Reserved error code",         // RESERVED_11
  "Database exception: Reserved error code",         // RESERVED_12
  "Database exception: Reserved error code",         // RESERVED_13
  "Database exception: Reserved error code",         // RESERVED_14
  "Database exception: Reserved error code",         // RESERVED_15
  "Database exception: Reserved error code",         // RESERVED_16
  "Database exception: Reserved error code",         // RESERVED_17
  "Database exception: Reserved error code",         // RESERVED_18

  // Data and index file error messages
  "Database exception: Data file is empty",    // DATAFILE_EMPTY
  "Database exception: Bad data file",         // BAD_DATAFILE
  "Database exception: Bad data file name",    // BAD_DATAFILE_NAME
  "Database exception: Index file is empty",   // INDEX_EMPTY
  "Database exception: Bad index file",        // BAD_INDEX_FILE
  "Database exception: Bad index file number", // BAD_INDEX_NUMBER
  "Database exception: Bad index file name",   // BAD_INDEX_NAME
  "Database exception: Error renaming file",   // FILE_RENAME_ERROR
  "Database exception: Error deleting file",   // FILE_DELETE_ERROR 

  // 11/16/2026 (7.4)
  "Database exception: The file was written by the other database engine (32-bit or 64-bit file addresses)" // FILE_ENGINE_ERROR
};

// NOTE: This array must contain the same number of status messages
// as the gxDatabaseStatus enumeration. 
const int gxdMaxStatusMessages = 14;
const char *gxdStatusMessages[gxdMaxStatusMessages] = {
  "Database Status: No status available",
  "Database Status: Invalid status code",
  "Database Status: Open for read/write access",
  "Database Status: Open for read only access",
  "Database Status: Unavailable to all users",
  "Database Status: Locked by another process",
  "Database Status: Insufficient user privileges",
  "Database Status: Insufficient group privileges",
  "Database Status: Access forbidden",
  "Database Status: Undergoing consistency checks",
  "Database Status: No such database exists",
  "Database Status: Not open for user access",
  "Database Status: Rebuilding data file",
  "Database Status: Rebuilding indexes"
};

GXDLCODE_API const char *gxDatabaseExceptionMessage(gxDatabaseError err)
// Standalone function that returns a null terminated string that can
// be use to log or print a database exception message.
{
  int error = (int)err;
  if(error > (gxdMaxExceptionMessages-1)) error = gxDBASE_INVALID_CODE;

  // Find the corresponding message in the exception array
  return gxdExceptionMessages[error];
}

GXDLCODE_API const char *gxDatabaseStatusMessage(gxDatabaseStatus status)
// Standalone function that returns a null terminated string that can
// be use to log or print a database status message.
{
  int s = (int)status;
  if(s > (gxdMaxStatusMessages-1)) s = gxDBASE_INVALID_STATUS;

  // Find the corresponding message in the exception array
  return gxdStatusMessages[s];
}
} // namespace datareel

// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //

