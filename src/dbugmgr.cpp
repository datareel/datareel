// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: dbugmgr.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/04/1997  
// Date Last Modified: 10/24/2026
// Copyright (c) 2001-2026 DataReel Software Development
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

gxDatabase debug manager class used by applications to troubleshoot 
and repair damaged or corrupt database files.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "dbugmgr.h"

#if defined (__USE_ANSI_CPP__)
#include <new>
#endif

#ifdef __BCC32__
#pragma warn -8066
#pragma warn -8080
#endif

// 10/14/2026: Internal helper used by the rebuild function. The memory
// allocation checks after each new[] call could never be reached
// because new[] throws on failure. Use the nothrow form when it is
// available so a failed allocation is reported in the status message.
static char *gxdDebugAlloc(__ULWORD__ bytes)
{
#if defined (__USE_ANSI_CPP__)
  return new(std::nothrow) char[bytes];
#else
  return new char[bytes];
#endif
}

// 10/14/2026: Internal helper used to restore the static gxDatabase
// version information changed by the rebuild function.
static void gxdRestoreVersionInfo(const __SBYTE__ *sig, FAU ver,
				  gxUINT32 check_word)
{
  memmove(gxDatabase::gxSignature, sig, gxSignatureSize);
  gxDatabase::gxVersion = ver;
  gxDatabase::gxInternalCheckWord = check_word;
}

gxDatabaseError gxDatabaseDebugManager::BlindOpen(const char *fname,
						  __SBYTE__ last_rev)
// Open the file in read only mode without check the file type
{
  status_message.Clear();

  // Set a default revision letter incase this open call cannot
  // read the revision letter stored in the file header.
  rev_letter = last_rev;

  if(!CheckFileSize(fname)) {
    gxd_error = gxDBASE_INVALID_FILE;
    return gxd_error;
  }

  // 10/14/2026: The name was copied into the fixed size file_name
  // buffer without a length check.
  if(strlen(fname) >= (size_t)gxMaxNameLength) {
    gxd_error = gxDBASE_FILE_OPEN_ERROR;
    return gxd_error;
  }

  // Close any open files
  if(Close() != gxDBASE_NO_ERROR) return gxd_error;
  
  fp = gxdFPTROpen(fname, gxDBASE_READONLY);
  if(fp == 0) {
    gxd_error = gxDBASE_FILE_OPEN_ERROR;
    return gxd_error;
  }

  ready_for_reading = 1;
  ready_for_writing = 0;
  is_open = 1;
  is_ok = 1;
  strcpy(file_name, fname);
  last_operation = gxDBASE_WRITE;
  
  gxFileHeader fh;
  FAU_t filesize = FileSize(fname);
  if(filesize < (FAU_t)sizeof(fh)) {
    return gxd_error = gxDBASE_INVALID_FILE;
  }
  
  // Read and test the file header
  if(Read(&fh, sizeof(fh), (FAU_t)0) != gxDBASE_NO_ERROR) {
    ResetFileHeader(file_header);
  }
  else {
    if(!VerifyFileHeader(fh)) {
      ResetFileHeader(file_header);
    }
    else {
      // The file header is valid so read the header on disk into
      // this object.
      if(ReadFileHdr()!= gxDBASE_NO_ERROR) {
	return gxd_error;
      }
      // Set the revision letter
      rev_letter = file_header.gxd_sig[gxSignatureSize-1];
    }
  }

  // Code used to support versions 1024-3002
  // Check the version number and set the previous version compatibility mode
  // NOTE: Version 4 and higher compatibility is set by default 
  if((int)file_header.gxd_ver < 2000) { 
    // Version 1 and 2 compatibility mode
    gxDatabase::gxVersion = file_header.gxd_ver;
    gxDatabase::gxInternalCheckWord = 0x0000fefe;
    memmove(gxDatabase::gxSignature, file_header.gxd_sig, 
	    (gxSignatureSize-1)); 
    // Set the revision letter
    rev_letter = file_header.gxd_sig[gxSignatureSize-1];
    status_message.Clear();
    status_message << "Detected version " 
		   << (int)file_header.gxd_ver << "\n" << "Signature ";
    status_message.InsertAt(status_message.length(), file_header.gxd_sig, 
			    (gxSignatureSize-1));
    status_message << "\n";
  }
  else if((int)file_header.gxd_ver < 4000) {
    // Version 3 compatibility mode
    gxDatabase::gxVersion = file_header.gxd_ver;
    gxDatabase::gxInternalCheckWord = 0xfefefefe;
    memmove(gxDatabase::gxSignature, file_header.gxd_sig, 
	    (gxSignatureSize-1));
    // Set the revision letter
    rev_letter = file_header.gxd_sig[gxSignatureSize-1];
    status_message.Clear();
    status_message << "Detected version " 
		   << (int)file_header.gxd_ver << "\n" << "Signature ";
    status_message.InsertAt(status_message.length(), file_header.gxd_sig, 
			    (gxSignatureSize-1));
    status_message << "\n";
  }

  return gxd_error = gxDBASE_NO_ERROR;
}

FAU_t gxDatabaseDebugManager::BlockSearch(FAU_t offset)
// Search through the database file until a valid block is found.
// The search starts at the beginning of the file or the
// offset value. Returns 0 if no valid block is found in the
// file.
{

  gxBlockHeader blk;
  FAU_t file_address = offset;
  FAU_t StaticEOF = FileSize(file_name);

  if(file_address >= StaticEOF) { // Prevent offsetting past EOF
    return 0; // Invalid address
  }
  
  while(1) {
    if((FAU_t)(file_address + sizeof(gxBlockHeader)) >= StaticEOF) {
      return (FAU_t)0;
    }
    if(Read(&blk, sizeof(gxBlockHeader), file_address) != gxDBASE_NO_ERROR)
      return (FAU_t)0;
    if(!TestBlockHeader(blk)) { 
      file_address++; // Loop through the file byte by byte
    }
    else {
      return file_address; // Found valid block
    }
  }

  // PC-lint 04/21/2004: Unreachable code but required for some compilers
  return (FAU_t)0;  
}


gxDatabaseError gxDatabaseDebugManager::AnalyzeHeader(int test_ver, 
						      int test_rev)
{
  // NOTE: The gxDatabase::gxSignature should always contain the
  // revision letter in the last byte.
  return AnalyzeHeader(gxDatabase::gxSignature, gxSignatureSize,
		       gxDatabase::gxVersion, test_ver, test_rev);
}

gxDatabaseError gxDatabaseDebugManager::AnalyzeHeader(const char *sig, 
						      int sig_len,
						      FAU ver,
						      int test_ver, 
						      int test_rev)
{
  gxFileHeader fh;
  gxBlockHeader blk;
  int errors = 0;

  // 10/14/2026: Record the first header error and any mismatch warning.
  // Later successful reads reset gxd_error, so the function could
  // return gxDBASE_NO_ERROR for a header with errors.
  gxDatabaseError header_error = gxDBASE_NO_ERROR;
  gxDatabaseError header_warning = gxDBASE_NO_ERROR;

  // 10/14/2026: Keep the signature length inside the file header
  // signature. A longer length read past the end of fh.gxd_sig.
  if(sig_len > gxSignatureSize) sig_len = gxSignatureSize;
  if((sig == 0) || (sig_len < 1)) return gxd_error = gxDBASE_BAD_HEADER;

  // Record the EOF mark
  FAU_t StaticEOF = FileSize(DatabaseName());

  // Clear the status message
  status_message.Clear();
  status_message << "Analyzing database file header" << "\n";

  Read(&fh, sizeof(gxFileHeader), (FAU_t)0);
  if(gxd_error != gxDBASE_NO_ERROR) {
    status_message << "Cannot read gxDatabase file header" << "\n";
    status_message << DatabaseExceptionMessage() << "\n";
    return gxd_error;
  }

  status_message << "Checking database signature" << "\n";
  if(memcmp(fh.gxd_sig, sig, (sig_len-1))) {
    // Test file type
    status_message << "Database file header is damaged or does not exist" 
		   << "\n";
    return gxd_error = gxDBASE_BAD_HEADER;
  }

  status_message << "Checking the revision letter" << "\n";
  char rev = fh.gxd_sig[sig_len-1];
  if(!VerifyRevLetter(rev)) {
    status_message << "Bad revision letter file header is not valid" 
		   << "\n";
    return gxd_error = gxDBASE_BAD_HEADER;
  }
  
  // 10/14/2026: Compare with the revision letter in the specified
  // signature. The static library signature was used in all cases.
  char curr_rev = sig[sig_len-1];
  if(rev != curr_rev) {
    if(test_rev) { // Report rev mismatch warning
      gxd_error = header_warning = gxDBASE_REV_MISMATCH;
      status_message << "NOTE: The database file revision letters do \
not match." << "\n";
      if(curr_rev == 0)
	status_message << "Current database library revision = Zero" << "\n";
      else
	status_message << "Current database library revision = " 
		       << curr_rev << "\n";
      if(rev == 0)
	status_message << "File revision = Zero" << "\n";
      else
	status_message << "File revision = " << rev << "\n";
      status_message << "Backward compatibility rules will be enforced." 
		     << "\n";
    }
  }

  status_message << "Checking the database version number" << "\n";
  if(!VerifyVersion((long)fh.gxd_ver)) {
    status_message << "Bad version number file header is not valid" 
		   << "\n";
    return gxd_error = gxDBASE_BAD_HEADER;
  }

  if(fh.gxd_ver != ver) {
    if(test_ver) { // Report ver mismatch warning
      gxd_error = header_warning = gxDBASE_VER_MISMATCH;
      status_message << "NOTE: The version numbers do not match." << "\n";
      // 10/14/2026: Report the version being tested, not the static version
      status_message << "Current database File Version = " 
		     << ver << "\n";
      status_message << "The file reads: " << fh.gxd_ver << "\n";
      status_message << "Backward compatibility rules will be enforced." 
		     << "\n";
    }
  }

  status_message << "Checking for End of File errors" << "\n";
  if(fh.gxd_eof < (FAU_t)0) {
    gxd_error = gxDBASE_DBEOF_ERROR;
    status_message << "Bad end of file mark in file: " << DatabaseName() 
		   << "\n";
    errors++;
    if(header_error == gxDBASE_NO_ERROR) header_error = gxd_error;
  }
  else if(StaticEOF > fh.gxd_eof) {
    gxd_error = gxDBASE_DBEOF_ERROR;
    status_message << "End of file error in file: " << DatabaseName() << "\n";
    status_message << "The actual length is longer then the allocated length!" 
		   << "\n";
    errors++;
    if(header_error == gxDBASE_NO_ERROR) header_error = gxd_error;
  }

  status_message << "Checking the heap start value" << "\n";
  // 10/14/2026: The heap start checks tested the highest block
  // pointer (gxd_hb_fptr) in place of the heap start (gxd_hs_fptr).
  // The highest block is still used to detect a file with no blocks.
  if((fh.gxd_hs_fptr < (FAU_t)FileHeaderSize()) || 
     (fh.gxd_hs_fptr > StaticEOF)) {
    gxd_error = gxDBASE_HEAPSTART_ERROR;
    status_message << "Bad heap start value in file: " << DatabaseName() 
		   << "\n";
    errors++;
    if(header_error == gxDBASE_NO_ERROR) header_error = gxd_error;
  }
  else {
    if(fh.gxd_hb_fptr == (FAU_t)0) {  // This is a new file
      status_message << "No database blocks allocated" << "\n";
    }
    else {
      if((FAU_t)(fh.gxd_hs_fptr + sizeof(gxBlockHeader)) > StaticEOF) {
	gxd_error = gxDBASE_HEAPSTART_ERROR;
	status_message << "Bad heap start value in file: " << DatabaseName() 
		       << "\n";
	status_message << "The heap start is past the end of file" << "\n";
	errors++;
	if(header_error == gxDBASE_NO_ERROR) header_error = gxd_error;
      }
      else {
	Read(&blk, sizeof(gxBlockHeader), fh.gxd_hs_fptr);
	if(gxd_error != gxDBASE_NO_ERROR) {
	  status_message << "Error reading block header at heap start" 
			 << "\n";
	  status_message << DatabaseExceptionMessage() << "\n";
	  errors++;
	  if(header_error == gxDBASE_NO_ERROR) header_error = gxd_error;
	}
	else {
	  if(!VerifyBlockHeader(blk)) { 
	    gxd_error = gxDBASE_HEAPSTART_ERROR;
	    status_message << "Bad heap start value in file: " 
			   << DatabaseName() << "\n";
	    status_message << "No database block found at file address: "
			   << fh.gxd_hs_fptr << "\n";
	    errors++;
	    if(header_error == gxDBASE_NO_ERROR) header_error = gxd_error;
	  }
	}
      }
    }
  }

  status_message << "Checking the free space value" << "\n";
  if(fh.gxd_fs_fptr == (FAU_t)0) {
    status_message << "Free space list is empty" << "\n";
  }
  else if(fh.gxd_fs_fptr == gxCurrAddress) {
    gxd_error = gxDBASE_FREESPACE_ERROR;
    status_message << "Bad free space list in file: " << DatabaseName() 
		   << "\n";
    errors++;
    if(header_error == gxDBASE_NO_ERROR) header_error = gxd_error;
  }
  else if(fh.gxd_fs_fptr < (FAU_t)0) {
    gxd_error = gxDBASE_FREESPACE_ERROR;
    status_message << "Bad free space value in file: " << DatabaseName() 
		   << "\n";
    errors++;
    if(header_error == gxDBASE_NO_ERROR) header_error = gxd_error;
  }
  else if(fh.gxd_fs_fptr != (FAU_t)0) {
    if((FAU_t)(fh.gxd_fs_fptr + sizeof(gxBlockHeader)) > StaticEOF) {
      gxd_error = gxDBASE_FREESPACE_ERROR;
      status_message << "Bad free space value in file: " << DatabaseName() 
		     << "\n";
      status_message << "Free space is past the end of file"<< "\n";
      errors++;
      if(header_error == gxDBASE_NO_ERROR) header_error = gxd_error;
    }
    else {
      Read(&blk, sizeof(gxBlockHeader), fh.gxd_fs_fptr);
      if(gxd_error != gxDBASE_NO_ERROR) {
	status_message << "Error reading block header at free space start" 
		       << "\n";
	status_message << DatabaseExceptionMessage() << "\n";
	errors++;
	if(header_error == gxDBASE_NO_ERROR) header_error = gxd_error;
      }
      else {
	if(!VerifyBlockHeader(blk)) { 
	  gxd_error = gxDBASE_FREESPACE_ERROR;
	  status_message << "Bad free space value in file: " << DatabaseName() 
			 << "\n";
	  status_message << "No database block found at file address: "
			 << fh.gxd_fs_fptr << "\n";
	  errors++;
	  if(header_error == gxDBASE_NO_ERROR) header_error = gxd_error;
	}
      }
    }
  }

  status_message << "Checking the highest block value" << "\n";
  if(fh.gxd_hb_fptr < (FAU_t)0) {
    gxd_error = gxDBASE_HIGHBLOCK_ERROR;
    status_message << "Bad highest block value in file: " << DatabaseName() 
		   << "\n";
    errors++;
    if(header_error == gxDBASE_NO_ERROR) header_error = gxd_error;
    
  }
  else {
    if(fh.gxd_hb_fptr == (FAU_t)0) {
      // This is a new file
      status_message << "No database blocks allocated" << "\n";
    }
    else { // Test the highest block
      if((FAU_t)(fh.gxd_hb_fptr + sizeof(gxBlockHeader)) > StaticEOF) {
	gxd_error = gxDBASE_HIGHBLOCK_ERROR;
	status_message << "Bad highest block value in file: " 
		       << DatabaseName() << "\n";
	status_message << "The highest block is past the end of file" << "\n";
	errors++;
	if(header_error == gxDBASE_NO_ERROR) header_error = gxd_error;
      }
      else {
	Read(&blk, sizeof(gxBlockHeader), fh.gxd_hb_fptr);
	if(gxd_error != gxDBASE_NO_ERROR) {
	  status_message << "Error reading block header at highest block \
address" << "\n";
	  status_message << DatabaseExceptionMessage() << "\n";
	  errors++;
	  if(header_error == gxDBASE_NO_ERROR) header_error = gxd_error;
	}
	else {
	  if(!VerifyBlockHeader(blk)) { 
	    gxd_error = gxDBASE_HIGHBLOCK_ERROR;
	    status_message << "Bad highest block value in file: " 
			   << DatabaseName() << "\n";
	    status_message << "No database block found at file address: "
			   << fh.gxd_hb_fptr << "\n";
	    errors++;
	    if(header_error == gxDBASE_NO_ERROR) header_error = gxd_error;
	  }
	}
      }
    }
  }

  if(errors) {
    status_message << "Database file header has errors!" << "\n";
    gxd_error = header_error;
  }
  else {
    status_message << "Database file header checks good." << "\n";
    gxd_error = header_warning;
  }

  return gxd_error;
}

int gxDatabaseDebugManager::Rebuild(const char *fname, int update_ver) 
{
  char curr_rev = rev_letter;
  FAU curr_ver = gxDatabase::gxVersion;
  if(update_ver) {
    curr_rev = gxDatabaseRevisionLetter;
    curr_ver = gxDatabaseVersionNumber;
  }
  FAU_t num_objects;
  return Rebuild(fname, &num_objects, curr_ver, curr_rev, 
		 (FAU_t)0, 1);
}

int gxDatabaseDebugManager::Rebuild(const char *fname, FAU_t *num_objects,
				    FAU ver, __SBYTE__ rev, FAU_t static_area,
				    int copy_static_data)
{
  FAU prev_ver = gxDatabase::gxVersion;  
  int rv;
  rv = Rebuild(fname, num_objects, gxDatabase::gxSignature,
	       gxSignatureSize, ver, rev, 
	       gxDatabase::gxInternalCheckWord,
	       static_area, copy_static_data);
  gxDatabase::gxVersion = prev_ver;
  return rv;
}

int gxDatabaseDebugManager::Rebuild(const char *fname, FAU_t *num_objects,
				    const char *sig, int sig_len,
				    FAU ver, __SBYTE__ rev, 
				    gxUINT32 check_word, FAU_t static_area, 
				    int copy_static_data)
// NOTE: The caller must ensure that any existing file with the 
// same fname can be overwritten. 
{
  // Clear the status message
  status_message.Clear();
  status_message << "Rebuild results for: " << fname << "\n";

  // Record the EOF mark
  FAU_t StaticEOF = FileSize(DatabaseName());

  if((rev == '\0') || (rev == ' ')) {
    status_message << "Database revision: Zero" << "\n";
  }
  else {
    status_message << "Database revision: " << rev << "\n";
  }
  status_message << "Database version: " << ver << "\n";

  gxFileHeader fh;
  gxBlockHeader blk;
  int errors = 0;
  if(num_objects) *num_objects = 0;

  // Save the previous state of all the version specific information
  gxUINT32 prev_check_word = gxDatabase::gxInternalCheckWord;
  FAU prev_ver = gxDatabase::gxVersion;
  // 10/14/2026: Also save the signature so it can be restored. The
  // static signature was left set to "sig" after a rebuild.
  __SBYTE__ prev_sig[gxSignatureSize];
  memmove(prev_sig, gxDatabase::gxSignature, gxSignatureSize);

  // 10/14/2026: Count errors found after the header checks. The
  // function returned true even when blocks could not be recovered.
  int rebuild_errors = 0;
  
  // Analyze the database file header to determine if the file has
  // a pre-allocated static area. The debug manager constructor will 
  // ensure that the byte size of the file is equal to at least one
  // file header, so do not check for EOF error be reading the file
  // header.
  Read(&fh, sizeof(gxFileHeader), (FAU_t)0);
  if(gxd_error != gxDBASE_NO_ERROR) { // Cannot read the file header
    status_message << "Error reading database file header" << "\n";
    status_message << DatabaseExceptionMessage() << "\n";
    errors++;
  }
  else {
    // Check the database file header's signature. 
    if (memcmp(fh.gxd_sig, gxDatabase::gxSignature, (gxSignatureSize-1))) {
      gxd_error = gxDBASE_BAD_HEADER;
      status_message << "Database file header is damaged or does not exist" 
		     << "\n";
      errors++; // Header is damaged and cannot be read
    }

    if(!errors) { // Check the heap start value 
      // 10/14/2026: The heap start checks tested the highest block
      // pointer (gxd_hb_fptr) in place of the heap start (gxd_hs_fptr).
      // The highest block is still used to detect a file with no blocks.
      if((fh.gxd_hs_fptr < (FAU_t)FileHeaderSize()) ||
	 (fh.gxd_hs_fptr > StaticEOF)) {
	gxd_error = gxDBASE_HEAPSTART_ERROR; 
	status_message << "Database has a bad heap start value" << "\n";
	errors++;
      }
      else if(fh.gxd_hb_fptr != (FAU_t)0) {
	if((FAU_t)(fh.gxd_hs_fptr + sizeof(gxBlockHeader)) > StaticEOF) {
	  gxd_error = gxDBASE_HEAPSTART_ERROR;
	  status_message << "Database has a bad heap start value" << "\n";
	  status_message << "The heap start is past the end of file" << "\n";
	  errors++;
	}
	else {
	  Read(&blk, sizeof(gxBlockHeader), fh.gxd_hs_fptr);
	  if(gxd_error != gxDBASE_NO_ERROR) {
	    status_message << "Database has a bad heap start value" << "\n";
	    errors++;
	  }
	  else {
	    if(!VerifyBlockHeader(blk)) { 
	      status_message << "Database has a bad heap start value" << "\n";
	      errors++;
	    }
	  }
	}
      }
    }
  }

  // If no errors, calculate the the size of the static area.
  if((!errors) && (static_area == (FAU_t)0)) {
    static_area = fh.gxd_hs_fptr - (FAU_t)FileHeaderSize();
  }

  FAU_t addr = BlockSearch((FAU_t)0); // Search the entire file
  gxDatabaseDebugManager NewFile;  

  // Set the specified version information
  gxDatabase::gxVersion = ver;
  gxDatabase::gxInternalCheckWord = check_word;
  if(sig_len > gxSignatureSize) sig_len = gxSignatureSize;
  if((sig) && (sig_len > 0)) memmove(gxDatabase::gxSignature, sig, sig_len);

  if(addr == (FAU_t)0) {
    status_message << "No database blocks found in file: "
	 << DatabaseName() << "\n";
    gxd_error = gxDBASE_NOBLOCK_ERROR;

    // Create an empty file with no blocks and return
    NewFile.Create(fname, static_area, rev);
    if(NewFile.gxd_error != gxDBASE_NO_ERROR) {
      status_message << "Error creating new database file" << "\n";
      status_message << NewFile.DatabaseExceptionMessage() << "\n";
      gxd_error = NewFile.gxd_error;
      // 10/14/2026: Restore the static version information
      gxdRestoreVersionInfo(prev_sig, prev_ver, prev_check_word);
      return 0;
    }
    NewFile.Close();
    // 10/14/2026: Restore the static version information. Return false
    // if the file header was damaged, as the full rebuild does.
    gxdRestoreVersionInfo(prev_sig, prev_ver, prev_check_word);
    if(errors) {
      status_message << DatabaseName() << " file header is damaged!" << "\n";
      return 0;
    }
    return 1;
  }

  // Create the new file and overwrite any existing file with 
  // the same name.
  NewFile.Create(fname, static_area, rev);
  if(NewFile.gxd_error != gxDBASE_NO_ERROR) {
    status_message << "Error creating new database file" << "\n";
    status_message << NewFile.DatabaseExceptionMessage() << "\n";
    gxd_error = NewFile.gxd_error;
    // 10/14/2026: Restore the static version information
    gxdRestoreVersionInfo(prev_sig, prev_ver, prev_check_word);
    return 0;
  }
  
  FAU_t count = (FAU_t)0;
  FAU_t badgx = (FAU_t)0;
  char *v;

  // Copy the static data area if the user requested the data be
  // copied into the rebuilt file.
  if((static_area != (FAU_t)0) && (copy_static_data)) { 
   // Write the static area data
    v = gxdDebugAlloc((__ULWORD__)static_area); // 10/14/2026: See gxdDebugAlloc()
    if(!v) {
      rebuild_errors++; // 10/14/2026: Count the error
      gxd_error = gxDBASE_MEM_ALLOC_ERROR;
      status_message << "Error copying static data area" << "\n";
      status_message << DatabaseExceptionMessage() << "\n";
      status_message << "No static data copied" << "\n";
      static_area = (FAU_t)0;
    }
    else {
      // Do not read past the end of file
      if((FAU_t)(static_area + FileHeaderSize()) > StaticEOF) {
	status_message << "Error copying static data area" << "\n";
	status_message << "Static data area is past the end of file" << "\n";
	status_message << "No static data copied" << "\n";
	rebuild_errors++; // 10/14/2026: Count the error
	static_area = (FAU_t)0;
	// PC-lint 09/08/2005: Inappropriate deallocation delete for new[]
	delete[] v;
      }
      else {
	Read(v, (__ULWORD__)static_area, (FAU_t)FileHeaderSize());
	if(gxd_error != gxDBASE_NO_ERROR) {
	  status_message << "Error copying static data area" << "\n";
	  status_message << DatabaseExceptionMessage() << "\n";
	  status_message << "No static data copied" << "\n";
	  rebuild_errors++; // 10/14/2026: Count the error
	  static_area = (FAU_t)0;
	  // PC-lint 09/08/2005: Inappropriate deallocation delete for new[]
	  delete[] v;
	}
	else {
	  // 10/14/2026: Write the static data after the new file's header.
	  // The old file's header size was used, which put the data in the
	  // wrong place when the new revision has a different header size.
	  NewFile.Write(v, (__ULWORD__)static_area, 
			(FAU_t)NewFile.FileHeaderSize());
	  if(NewFile.gxd_error != gxDBASE_NO_ERROR) {
	    status_message << "Error copying static data area" << "\n";
	    status_message << NewFile.DatabaseExceptionMessage() << "\n";
	    status_message << "No static data copied" << "\n";
	    rebuild_errors++; // 10/14/2026: Count the error
	    // PC-lint 09/08/2005: Inappropriate deallocation delete for new[]
	    delete[] v;
	    gxd_error = NewFile.gxd_error;
	    copy_static_data = 0;
	  }
	  else {
	    // PC-lint 09/08/2005: Inappropriate deallocation delete for new[]
	    delete[] v;
	  }
	}
      }
    }
  }

  FAU_t object_length;
  gxINT32 checksum;
  FAU_t curr_addr = addr;

  while(1) { 
    // Use the previous version information before each read
    gxDatabase::gxVersion = prev_ver;
    gxDatabase::gxInternalCheckWord = prev_check_word;

    // Do not read past the end of file
    if((FAU_t)(addr + BlockHeaderSize()) >= StaticEOF) {
      if(curr_addr != StaticEOF) {
	// Report EOF warning here if an error is detected
	status_message << "Reached the end of file mark rebuild stopped"
		       << "\n";
      }
      break;
    }

    // Read the block header and set the current address pointer
    Read(&blk, sizeof(gxBlockHeader), addr);
    curr_addr = FAU_t(addr + sizeof(gxBlockHeader));
    if(gxd_error != gxDBASE_NO_ERROR) {
      status_message << "Error reading database block header" 
		     << "\n";
      status_message << DatabaseExceptionMessage() << "\n";
      badgx++;
      addr = BlockSearch(curr_addr); // Search for the next good block
      if(addr == (FAU_t)0) break; 
      continue;
    }
    // 10/14/2026: Treat a block whose length is shorter than its header
    // or runs past the end of file as a bad block. A zero length looped
    // forever and a short length gave a negative object length.
    if((TestBlockHeader(blk)) && 
       (((FAU_t)blk.block_length < (FAU_t)BlockHeaderSize()) ||
	((FAU_t)(addr + blk.block_length) > StaticEOF))) {
      status_message << "Bad block length found at file address: "
	     << addr << "\n";
      badgx++;
      addr = BlockSearch(curr_addr); // Search for the next good block
      if(addr == (FAU_t)0) break; 
      continue;
    }
    if(TestBlockHeader(blk)) { // This is a valid database block 
      if(blk.block_status == gxNormalBlock) { // Only copy normal blocks
	switch(rev_letter) {
	  case '\0' : case ' ' : case 'e' : case 'E' :
	    object_length = blk.block_length-BlockHeaderSize();
	    break;
	  default:
	    object_length = (blk.block_length-(BlockHeaderSize()+sizeof(gxChecksum)));
	    break;
	}
	if(object_length < (FAU_t)0) { // 10/14/2026: See above
	  status_message << "Bad block length found at file address: "
			 << addr << "\n";
	  badgx++;
	  addr = BlockSearch(curr_addr); // Search for the next good block
	  if(addr == (FAU_t)0) break; 
	  continue;
	}

	// Check for record locks
	if((rev_letter == 'c') || (rev_letter == 'C') || (rev_letter == 'd')
	   || (rev_letter == 'D')) {
	  // Seek past the record lock header
	  curr_addr += sizeof(gxRecordLockHeader);
	  if(curr_addr >= StaticEOF) { // Do not seek past the end of file
	    status_message << "Reached the end of file mark rebuild stopped"
			   << "\n";
	    break;
	  } 

	  // Seek past the record lock header and set the current address
	  SeekTo(curr_addr);
	  if(gxd_error != gxDBASE_NO_ERROR) {
	    status_message << "Error seeking in file" << "\n";
	    status_message << DatabaseExceptionMessage() << "\n";
	    badgx++;
	    addr = BlockSearch(curr_addr); // Search for the next good block
	    if(addr == (FAU_t)0) break; 
	    continue;
	  }
	}

	// 10/14/2026: The length was cast to int, which truncated object
	// lengths over 2GB in the 64-bit engine. See gxdDebugAlloc().
	v = gxdDebugAlloc((__ULWORD__)object_length);
	if(!v) {
	  status_message << "Memory allocation error rebuild stopped" << "\n";
	  gxd_error = gxDBASE_MEM_ALLOC_ERROR;
	  rebuild_errors++; // 10/14/2026: Count the error
	  break;
	}
	
	// Set the current address
	curr_addr += object_length;
	if(curr_addr > StaticEOF) {
	  // Do not read past the end of file
	  status_message << "Reached the end of file mark rebuild stopped"
			 << "\n";
	  // PC-lint 09/08/2005: Inappropriate deallocation delete for new[]
	  delete[] v;
	  break;
	} 
	Read(v, (__ULWORD__)object_length);
	if(gxd_error != gxDBASE_NO_ERROR) {
	  status_message << "Error reading database block" << "\n";
	  status_message << DatabaseExceptionMessage() << "\n";
	  // PC-lint 09/08/2005: Inappropriate deallocation delete for new[]
	  delete[] v;
	  badgx++;
	  addr = BlockSearch(curr_addr); // Search for the next good block
	  if(addr == (FAU_t)0) break; 
	  continue;
	}

	// Set the current address
	switch(rev_letter) {
	  case '\0' : case ' ' : case 'e' : case 'E' :
	    break;
	  default:
	    curr_addr += sizeof(gxChecksum);
	    break;
	}
	gxDatabase::gxVersion = ver;
	gxDatabase::gxInternalCheckWord = check_word;
	NewFile.Write(v, (__ULWORD__)object_length,
		      NewFile.Alloc((__ULWORD__)object_length));
	if(NewFile.gxd_error != gxDBASE_NO_ERROR) {
	  status_message << "Error writing block to specified database file"
			 << "\n";
	  status_message << NewFile.DatabaseExceptionMessage() << "\n";
	  // PC-lint 09/08/2005: Inappropriate deallocation delete for new[]
	  delete[] v;
	  gxd_error = NewFile.gxd_error;
	  badgx++;
	  // 10/14/2026: Search the old file with the old check word. The
	  // new check word was still set here.
	  gxDatabase::gxVersion = prev_ver;
	  gxDatabase::gxInternalCheckWord = prev_check_word;
	  addr = BlockSearch(curr_addr); // Search for the next good block
	  if(addr == (FAU_t)0) break; 
	  continue;
	}
	
	// Check for persistent checksums
	if((rev == 'a') || (rev == 'A') || (rev == 'b') || (rev == 'B') ||
	   (rev == 'c') || (rev == 'C') || (rev == 'd') || (rev == 'D')) {
	  checksum = calcCRC32(v, (__ULWORD__)object_length);
	  NewFile.Write(&checksum, sizeof(checksum));
	  if(NewFile.gxd_error != gxDBASE_NO_ERROR) {
	    status_message << "Error writing persistent checksum value"
			   << "\n";
	    status_message << NewFile.DatabaseExceptionMessage() << "\n";
	    gxd_error = NewFile.gxd_error;
	    rebuild_errors++; // 10/14/2026: Count the error
	  }
	}
	// 10/14/2026: Use the old version information for the old file
	gxDatabase::gxVersion = prev_ver;
	gxDatabase::gxInternalCheckWord = prev_check_word;
	count++; // Increment the database block count
	// PC-lint 09/08/2005: Inappropriate deallocation delete for new[]
	delete[] v;
      }
      addr = addr + blk.block_length; // Go to the next database block
    }
    else {
      badgx++;
      addr = BlockSearch(curr_addr); // Search for the next good block
      if(addr == (FAU_t)0) break; 
    }
  }

  // Total number of objects restored
  if(num_objects) *num_objects = count;

  status_message << "Wrote " << count << " good database blocks to file: "
		 << NewFile.DatabaseName() << "\n";
  
  if((static_area != (FAU_t)0) && (copy_static_data)) { 
    status_message << "Wrote " << static_area << " bytes of Static area data."
		   << "\n";
  }
  if(rebuild_errors) { // 10/14/2026: Report errors found during the rebuild
    status_message << "Found " << rebuild_errors 
	   << " errors during the rebuild" << "\n";
  }
  if(badgx) {
    gxd_error = gxDBASE_BADBLOCK_ERROR;
    status_message << "Did not write " << badgx 
		   << " bad blocks found in file: "
		   << DatabaseName() << "\n";
  }
  if(errors) {
    gxd_error = gxDBASE_BAD_HEADER;
    status_message << DatabaseName() << " file header is damaged!" << "\n";
    status_message << "No header information was copied to "
		   << NewFile.DatabaseName() << "\n";
  }

  // 10/14/2026: Close the new file with its version information set,
  // then restore the static version information. The static signature
  // was never restored and the function always returned true.
  gxDatabase::gxVersion = ver;
  gxDatabase::gxInternalCheckWord = check_word;
  NewFile.Close();
  gxdRestoreVersionInfo(prev_sig, prev_ver, prev_check_word);

  if((errors) || (badgx) || (rebuild_errors)) return 0;
  return 1;
}

int gxDatabaseDebugManager::CheckFileSize(const char *fname)
{
  FAU_t fsize = gxdFPTRFileSize(fname);
  if(fsize < (FAU_t)sizeof(gxFileHeader)) return 0;
  return 1;
}

int gxDatabaseDebugManager::VerifyBlockHeader(const gxBlockHeader &hdr)
// Function used to verify a block header to ensure that this is a 
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
  // 10/14/2026: Both reserved status bytes must be zero (was &&, which
  // accepted a header when only one of them was set). The library never
  // writes these bytes, so every block it created still passes.
  if((unused_1 != 0) || (unused_2 != 0)) {
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

int gxDatabaseDebugManager::VerifyFileHeader(const gxFileHeader &fh) 
{
  // Test the signature minus the revision letter
  if(memcmp(fh.gxd_sig, gxDatabase::gxSignature,
	    (gxSignatureSize-1)) != 0) {
    return 0;
  }
  
  char rev = fh.gxd_sig[gxSignatureSize-1];
  if(!VerifyRevLetter(rev)) return 0;
  
  long version = (long)fh.gxd_ver;
  if(!VerifyVersion(version)) return 0;
  
  if(fh.gxd_eof < (FAU_t)0) return 0;
  if(fh.gxd_hs_fptr < (FAU_t)0) return 0;
  if(fh.gxd_hb_fptr < (FAU_t)0) return 0;
  
  return 1;
}

int gxDatabaseDebugManager::VerifyVersion(long ver)
// Returns true if the version number is in the range of versions 
// written by this library or false if the version is out of range.
// 10/14/2026: The function listed known version numbers but returned
// true for every version in the range, so the list was never used. 
// The gxDatabase engine does not check the version number when it 
// opens a file, and the library has written versions that are not in
// the list (such as the versions between 4400 and the current one), 
// so only the range is tested. The list was removed.
{
  // Test the version ranges
  if(ver < 1024) return 0; // Lowest version written by the library
  if(ver > gxDatabaseVersionNumber) return 0;
  return 1;
}

int gxDatabaseDebugManager::VerifyRevLetter(__SBYTE__ rev) 
{
  switch(rev) {
    case '\0' : case ' ' :
      return 1;
    case 'a' : case 'A' :
      return 1;
    case 'b' : case 'B' :
      return 1;
    case 'c' : case 'C' :
      return 1;
    case 'd' : case 'D' :
      return 1;
    case 'e' : case 'E' :
      return 1;
    default: // Unknown revision letter
      break;
  }
  return 0;
}

void gxDatabaseDebugManager::ResetFileHeader(gxFileHeader &fh)
{
  // 10/14/2026: The end of file was set from this object's header in
  // place of "fh", and a heap start of zero (an unread header) gave a
  // negative static area. Use the smallest valid heap start in that case.
  if(fh.gxd_hs_fptr < (FAU_t)FileHeaderSize()) {
    fh.gxd_hs_fptr = (FAU_t)FileHeaderSize();
  }
  fh.gxd_fs_fptr = (FAU_t)0;
  fh.gxd_eof = fh.gxd_hs_fptr;
  fh.gxd_hb_fptr = (FAU_t)0; 
  memcpy(fh.gxd_sig, gxDatabase::gxSignature, (gxSignatureSize-1)); 
  fh.gxd_ver = gxDatabase::gxVersion;

  // Set the specified revision letter according to current
  // "rev_letter" value;
  fh.gxd_sig[gxSignatureSize-1] = rev_letter;
}

#ifdef __BCC32__
#pragma warn .8066
#pragma warn .8080
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //

