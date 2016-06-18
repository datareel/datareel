// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: gxdstats.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/03/1997  
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

The database file statistics functions are used to display
detailed file information about open database files. 
This information is used to analyze and troubleshoot 
database files. 

Changes:
==============================================================
03/25/2002: Modified all functions that print 64-bit native 
integers to the console to use the UString class overloads.
This correction was made to allow code to be compilers using 
the __USE_NATIVE_INT_TYPES__ preprocessor directive.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__CONSOLE__)
#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
#include <iomanip>
#else // Use the old iostream library by default
#include <iostream.h>
#include <iomanip.h>
#endif // __USE_ANSI_CPP__
#endif // __CONSOLE__

#include "gxdstats.h"
#include "ustring.h"

#ifdef __BCC32__
#pragma warn -8008
#pragma warn -8066
#pragma warn -8080
#endif

GXDLCODE_API void DatabaseStats(gxDatabase *f)
{
#if defined (__CONSOLE__)
  UString intbuf;
  GXSTD::cout << "\n";
  const char *FName = f->DatabaseName();
  char rev_letter = f->GetRevLetter();
  char *signature = f->GetSignature();
  GXSTD::cout << "----- Database file statistics -----" << "\n";
  GXSTD::cout << "\n";
  GXSTD::cout << "----- Static information -----" << "\n";
  GXSTD::cout << "File Name:         " << FName << "\n";
  GXSTD::cout << "File Signature:    " << signature << "\n"; 
  if(rev_letter == '\0' || rev_letter == ' ')
    GXSTD::cout << "Revision Letter:   " << "Zero" << "\n";
  else
    GXSTD::cout << "Revision Letter:   " << rev_letter << "\n";
  intbuf.Clear();
  intbuf << (FAU_t)f->GetVersion();
  GXSTD::cout << "Library Version:   " << intbuf.c_str() << "\n";
  GXSTD::cout << "File Header Size:  " << f->FileHeaderSize() << "\n";
  GXSTD::cout << "Block Header Size: " << f->BlockHeaderSize() << "\n";
  intbuf.Clear();
  intbuf << (FAU_t)f->StaticArea();
  GXSTD::cout << "Static area size:  " << intbuf.c_str() << "\n";
  intbuf.Clear();
  intbuf << (FAU_t)f->FileSize(FName);
  GXSTD::cout << "Static file size:  " << intbuf.c_str() << "\n";
  GXSTD::cout << "\n";
  GXSTD::cout << "----- Dynamic Information -----" << "\n";
  GXSTD::cout << "File Status:     ";
  if(f->IsOpen()) GXSTD::cout << "Open "; else GXSTD::cout << "Closed ";
  if(f->ReadyForWriting()) GXSTD::cout << "Read/Write "; 
  else GXSTD::cout << "Read/Only ";
  GXSTD::cout << "\n";
  intbuf.Clear();
  intbuf << (FAU_t)f->GetDatabaseFreeSpace();
  GXSTD::cout << "Free Space:      " << intbuf.c_str() << "\n";
  intbuf.Clear();
  intbuf << (FAU_t)f->GetEOF();
  GXSTD::cout << "End of File:     " << intbuf.c_str() << "\n";
  intbuf.Clear();
  intbuf << (FAU_t)f->GetHeapStart();
  GXSTD::cout << "Heap Start:      " << intbuf.c_str() << "\n";
  intbuf.Clear();
  intbuf << (FAU_t)f->GetHighestBlock();
  GXSTD::cout << "Highest Block:   " << intbuf.c_str() << "\n";
  
  // Calculate the total number of normal and deleted blocks
  FAU_t tl, dl, rm, nb, tb;
  tb = f->TotalBlocks();
  tl = f->DeletedBlocks(&dl, &rm);
  if((rev_letter == 'd') || (rev_letter == 'D') ||
     (rev_letter == 'e') || (rev_letter == 'E')) {
    nb = f->NormalBlocks();
  }
  else {
    nb = tb - tl;
  }

  intbuf.Clear();
  intbuf << tb;
  GXSTD::cout << "Total Blocks:    " << intbuf.c_str() << "\n";
  intbuf.Clear();
  intbuf << nb;
  GXSTD::cout << "Normal Blocks:   " << intbuf.c_str() << "\n";
  intbuf.Clear();
  intbuf << dl;
  GXSTD::cout << "Deleted/Removed: " << intbuf.c_str() << "/";
  intbuf.Clear();
  intbuf << rm;
  GXSTD::cout << intbuf.c_str() << " (";
  intbuf.Clear();
  intbuf << tl;
  GXSTD::cout << intbuf.c_str() << ")" << "\n";

  gxFileLockHeader flh;
  switch(rev_letter) {
    case 'b': case 'B': case 'c': case 'C': 
    case 'd': case 'D': case 'e': case 'E':
      GXSTD::cout << "\n";
      GXSTD::cout << "----- Lock Header Information -----" << "\n";
      f->ReadFileLockHdr(flh);
      GXSTD::cout << "File Lock Protect:     " << (int)flh.file_lock_protect 
		  << "\n";
      GXSTD::cout << "File Read Lock Value:  " << (int)flh.file_read_lock 
		  << "\n";
      GXSTD::cout << "File Write Lock Value: " << (int)flh.file_write_lock 
		  << "\n";
      break;
    default:
      break;
  }

  delete signature;
  GXSTD::cout << "\n";
#endif // Display type for stats messaging
}

GXDLCODE_API  void BlockStats(gxDatabase *f, FAU oa)
{
#if defined (__CONSOLE__)
  UString intbuf;
  gxBlockHeader blk_hdr;
  FAU blk_address = oa - f->BlockHeaderSize();
  f->Read(&blk_hdr, sizeof(gxBlockHeader), blk_address);
  char rev_letter = f->GetRevLetter();
  __ULWORD__ object_crc, calc_crc;
  
  GXSTD::cout << "\n";
  GXSTD::cout << "----- Block Statistics -----" << "\n";
  
  GXSTD::cout << "Check Word   = ";
  GXSTD::cout.setf(GXSTD::ios::uppercase);
  GXSTD::cout << "0x" << GXSTD::setfill('0') << GXSTD::setw(8) 
	      << GXSTD::hex << blk_hdr.block_check_word << "\n";
  GXSTD::cout.unsetf(GXSTD::ios::uppercase);
  GXSTD::cout << "Length       = " << GXSTD::dec << blk_hdr.block_length 
	      << "\n";
  GXSTD::cout << "Status       = " << (__SBYTE__)(blk_hdr.block_status & 0xff) 
	      << "\n";
  intbuf.Clear();
  intbuf << (FAU_t)blk_hdr.block_nd_fptr;
  GXSTD::cout << "Next Deleted = " << intbuf.c_str() << "\n";
  GXSTD::cout << "\n";
  GXSTD::cout << "----- Object Statistics -----" << "\n";
  GXSTD::cout << "Object Length  = " << f->ObjectLength(oa) << "\n";

  // Peristent checksum revisions
  switch(rev_letter) {
    case 'a': case 'A': case 'b': case 'B': 
    case 'c': case 'C': case 'd': case 'D': 
      if(f->ReadObjectChecksum(oa, &object_crc, &calc_crc)) {
	// Display all good persistent checksums
	GXSTD::cout.setf(GXSTD::ios::uppercase);
	GXSTD::cout << "Stored CRC     = 0x" << GXSTD::setfill('0') 
		    << GXSTD::setw(8) << GXSTD::hex << object_crc << "\n";
	GXSTD::cout << "Calculated CRC = 0x" << GXSTD::setfill('0') 
		    << GXSTD::setw(8) << GXSTD::hex << calc_crc << GXSTD::dec 
		    << "\n";
      }
      else {
	// Checksum not set or persistent value is bad
	CheckError(f);
      }
      break;
    default: // Rev '\0' and 'E '
      break;
  }

  // Persistent record locking revisions
  gxRecordLockHeader rlh;
  switch(rev_letter) {
    case 'c': case 'C': case 'd': case 'D': 
      GXSTD::cout << "\n";
      GXSTD::cout << "----- Record Lock Statistics -----" << "\n";
      f->ReadRecordLockHdr(rlh, blk_address);
      GXSTD::cout << "Record Lock Protect:     " 
		  << (int)rlh.record_lock_protect << "\n";
      GXSTD::cout << "Record Read Lock Value:  " << (int)rlh.record_read_lock
		  << "\n";
      GXSTD::cout << "Record Write Lock Value: " << (int)rlh.record_write_lock
		  << "\n";
      break;
    default: // Rev '\0', 'A', 'B', and 'E '
      break;
  }

  GXSTD::cout << "\n";
#endif // Display type for stats messaging
}

GXDLCODE_API int CheckError(gxDatabase *f)
// Check the last reported error. Prints the error
// and returns a non-zero value if an error condition
// was reached.
{

  if(f->GetDatabaseError() != gxDBASE_NO_ERROR) {
#if defined (__CONSOLE__)
    GXSTD::cout << f->DatabaseExceptionMessage() << "\n";
#endif // Display type for stats messaging
    return 1;
  }
  return 0;
}

GXDLCODE_API int CheckError(gxDatabaseError err)
// Check the last reported error. Prints the error
// and returns a non-zero value if an error condition 
// was reached.
{
  if(err != gxDBASE_NO_ERROR) {
#if defined (__CONSOLE__)
    GXSTD::cout << gxDatabaseExceptionMessage(err) << "\n";
#endif // Display type for stats messaging
    return 1;
  }
  return 0;
}

GXDLCODE_API void EchoDatabaseVersion()
// Echo the current library version and revision to the console.
{
#if defined (__CONSOLE__)
  GXSTD::cout.setf(GXSTD::ios::showpoint | GXSTD::ios::fixed);
  GXSTD::cout.precision(3);
  GXSTD::cout << "Default Database Library Version:  " << gxDatabaseVersion 
	      << "\n";
  
  // PC-lint 09/08/2005: Constant value boolean in below if statement
  char null_rev = '\0'; char space_rev = ' ';
  
    // Ignore BCC32 condition and unreachable code warnings
  if((gxDatabaseRevisionLetter == null_rev) || (gxDatabaseRevisionLetter == space_rev)) {
    GXSTD::cout << "Default Database Library Revision: Zero" << "\n";
  }
  else {
    GXSTD::cout << "Default Database Library Revision: " 
		<< gxDatabaseRevisionLetter << "\n";
  }
#endif // Display type for stats messaging
}

GXDLCODE_API void EchoDatabaseVersion(gxDatabase *f)
// Echo the current library version and revision to the console.
{
#if defined (__CONSOLE__)
  UString intbuf;
  GXSTD::cout.setf(GXSTD::ios::showpoint | GXSTD::ios::fixed);
  GXSTD::cout.precision(3);
  char rev_letter = f->GetRevLetter();
  intbuf.Clear();
  intbuf << (FAU_t)f->GetVersion();
  GXSTD::cout << "Default Database Library Version:  " << intbuf.c_str()
	      << "\n";
  if((rev_letter == '\0') || (rev_letter == ' ')) {
    GXSTD::cout << "Default Database Library Revision: Zero" << "\n";
  }
  else {
    GXSTD::cout << "Default Database Library Revision: " << rev_letter << "\n";
  }
#endif // Display type for stats messaging
}

#ifdef __BCC32__
#pragma warn .8008
#pragma warn .8066
#pragma warn .8080
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
