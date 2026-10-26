// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: gxdstats.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/03/1997  
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

The database file statistics functions are used to display
detailed file information about open database files. 
This information is used to analyze and troubleshoot 
database files. 

Changes:
==============================================================
03/25/2002: Modified all functions that print 64-bit native 
integers to the console to use the std::string class overloads.
This correction was made to allow code to be compilers using 
the __USE_NATIVE_INT_TYPES__ preprocessor directive.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__CONSOLE__)
#include <iostream>
#include <iomanip>
#endif // __CONSOLE__

#include "gxdstats.h"
#include <string>
#include "strutil.h"

using namespace datareel;


GXDLCODE_API void DatabaseStats(gxDatabase *f)
{
#if defined (__CONSOLE__)
  std::string intbuf;
  std::cout << "\n";
  const char *FName = f->DatabaseName();
  char rev_letter = f->GetRevLetter();
  char *signature = f->GetSignature();
  std::cout << "----- Database file statistics -----" << "\n";
  std::cout << "\n";
  std::cout << "----- Static information -----" << "\n";
  std::cout << "File Name:         " << FName << "\n";
  std::cout << "File Signature:    " << signature << "\n"; 
  if(rev_letter == '\0' || rev_letter == ' ')
    std::cout << "Revision Letter:   " << "Zero" << "\n";
  else
    std::cout << "Revision Letter:   " << rev_letter << "\n";
  intbuf.clear();
  str_append(intbuf, (FAU_t)f->GetVersion());
  std::cout << "Library Version:   " << intbuf.c_str() << "\n";
  std::cout << "File Header Size:  " << f->FileHeaderSize() << "\n";
  std::cout << "Block Header Size: " << f->BlockHeaderSize() << "\n";
  intbuf.clear();
  str_append(intbuf, (FAU_t)f->StaticArea());
  std::cout << "Static area size:  " << intbuf.c_str() << "\n";
  intbuf.clear();
  str_append(intbuf, (FAU_t)f->FileSize(FName));
  std::cout << "Static file size:  " << intbuf.c_str() << "\n";
  std::cout << "\n";
  std::cout << "----- Dynamic Information -----" << "\n";
  std::cout << "File Status:     ";
  if(f->IsOpen()) std::cout << "Open "; else std::cout << "Closed ";
  if(f->ReadyForWriting()) std::cout << "Read/Write "; 
  else std::cout << "Read/Only ";
  std::cout << "\n";
  intbuf.clear();
  str_append(intbuf, (FAU_t)f->GetDatabaseFreeSpace());
  std::cout << "Free Space:      " << intbuf.c_str() << "\n";
  intbuf.clear();
  str_append(intbuf, (FAU_t)f->GetEOF());
  std::cout << "End of File:     " << intbuf.c_str() << "\n";
  intbuf.clear();
  str_append(intbuf, (FAU_t)f->GetHeapStart());
  std::cout << "Heap Start:      " << intbuf.c_str() << "\n";
  intbuf.clear();
  str_append(intbuf, (FAU_t)f->GetHighestBlock());
  std::cout << "Highest Block:   " << intbuf.c_str() << "\n";
  
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

  intbuf.clear();
  str_append(intbuf, tb);
  std::cout << "Total Blocks:    " << intbuf.c_str() << "\n";
  intbuf.clear();
  str_append(intbuf, nb);
  std::cout << "Normal Blocks:   " << intbuf.c_str() << "\n";
  intbuf.clear();
  str_append(intbuf, dl);
  std::cout << "Deleted/Removed: " << intbuf.c_str() << "/";
  intbuf.clear();
  str_append(intbuf, rm);
  std::cout << intbuf.c_str() << " (";
  intbuf.clear();
  str_append(intbuf, tl);
  std::cout << intbuf.c_str() << ")" << "\n";

  gxFileLockHeader flh;
  switch(rev_letter) {
    case 'b': case 'B': case 'c': case 'C': 
    case 'd': case 'D': case 'e': case 'E':
      std::cout << "\n";
      std::cout << "----- Lock Header Information -----" << "\n";
      f->ReadFileLockHdr(flh);
      std::cout << "File Lock Protect:     " << (int)flh.file_lock_protect 
		  << "\n";
      std::cout << "File Read Lock Value:  " << (int)flh.file_read_lock 
		  << "\n";
      std::cout << "File Write Lock Value: " << (int)flh.file_write_lock 
		  << "\n";
      break;
    default:
      break;
  }

  delete[] signature; // 10/14/2026: Was a scalar delete of new[]
  std::cout << "\n";
#endif // Display type for stats messaging
}

GXDLCODE_API  void BlockStats(gxDatabase *f, FAU oa)
{
#if defined (__CONSOLE__)
  std::string intbuf;
  gxBlockHeader blk_hdr;
  FAU blk_address = oa - f->BlockHeaderSize();
  f->Read(&blk_hdr, sizeof(gxBlockHeader), blk_address);
  char rev_letter = f->GetRevLetter();
  __ULWORD__ object_crc, calc_crc;
  
  // 10/14/2026: Save the stream format. The fill character and other
  // format changes made below were left set after this function.
  std::ios::fmtflags prev_flags = std::cout.flags();
  char prev_fill = std::cout.fill();

  std::cout << "\n";
  std::cout << "----- Block Statistics -----" << "\n";
  
  std::cout << "Check Word   = ";
  std::cout.setf(std::ios::uppercase);
  std::cout << "0x" << std::setfill('0') << std::setw(8) 
	      << std::hex << blk_hdr.block_check_word << "\n";
  std::cout.unsetf(std::ios::uppercase);
  std::cout << "Length       = " << std::dec << blk_hdr.block_length 
	      << "\n";
  std::cout << "Status       = " << (__SBYTE__)(blk_hdr.block_status & 0xff) 
	      << "\n";
  intbuf.clear();
  str_append(intbuf, (FAU_t)blk_hdr.block_nd_fptr);
  std::cout << "Next Deleted = " << intbuf.c_str() << "\n";
  std::cout << "\n";
  std::cout << "----- Object Statistics -----" << "\n";
  std::cout << "Object Length  = " << f->ObjectLength(oa) << "\n";

  // Peristent checksum revisions
  switch(rev_letter) {
    case 'a': case 'A': case 'b': case 'B': 
    case 'c': case 'C': case 'd': case 'D': 
      if(f->ReadObjectChecksum(oa, &object_crc, &calc_crc)) {
	// Display all good persistent checksums
	std::cout.setf(std::ios::uppercase);
	std::cout << "Stored CRC     = 0x" << std::setfill('0') 
		    << std::setw(8) << std::hex << object_crc << "\n";
	std::cout << "Calculated CRC = 0x" << std::setfill('0') 
		    << std::setw(8) << std::hex << calc_crc << std::dec 
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
      std::cout << "\n";
      std::cout << "----- Record Lock Statistics -----" << "\n";
      f->ReadRecordLockHdr(rlh, blk_address);
      std::cout << "Record Lock Protect:     " 
		  << (int)rlh.record_lock_protect << "\n";
      std::cout << "Record Read Lock Value:  " << (int)rlh.record_read_lock
		  << "\n";
      std::cout << "Record Write Lock Value: " << (int)rlh.record_write_lock
		  << "\n";
      break;
    default: // Rev '\0', 'A', 'B', and 'E '
      break;
  }

  std::cout << "\n";
  std::cout.flags(prev_flags); // 10/14/2026: Restore the stream format
  std::cout.fill(prev_fill);
#endif // Display type for stats messaging
}

GXDLCODE_API int CheckError(gxDatabase *f)
// Check the last reported error. Prints the error
// and returns a non-zero value if an error condition
// was reached.
{

  if(f->GetDatabaseError() != gxDBASE_NO_ERROR) {
#if defined (__CONSOLE__)
    std::cout << f->DatabaseExceptionMessage() << "\n";
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
    std::cout << gxDatabaseExceptionMessage(err) << "\n";
#endif // Display type for stats messaging
    return 1;
  }
  return 0;
}

GXDLCODE_API void EchoDatabaseVersion()
// Echo the current library version and revision to the console.
{
#if defined (__CONSOLE__)
  // 10/14/2026: Save the stream format. The fixed point format and
  // precision set here were left set after this function.
  std::ios::fmtflags prev_flags = std::cout.flags();
  std::streamsize prev_precision = std::cout.precision();
  std::cout.setf(std::ios::showpoint | std::ios::fixed);
  std::cout.precision(3);
  std::cout << "Default Database Library Version:  " << gxDatabaseVersion 
	      << "\n";
  
  // PC-lint 09/08/2005: Constant value boolean in below if statement
  char null_rev = '\0'; char space_rev = ' ';
  
    // Ignore BCC32 condition and unreachable code warnings
  if((gxDatabaseRevisionLetter == null_rev) || (gxDatabaseRevisionLetter == space_rev)) {
    std::cout << "Default Database Library Revision: Zero" << "\n";
  }
  else {
    std::cout << "Default Database Library Revision: " 
		<< gxDatabaseRevisionLetter << "\n";
  }
  std::cout.flags(prev_flags); // 10/14/2026: Restore the stream format
  std::cout.precision(prev_precision);
#endif // Display type for stats messaging
}

GXDLCODE_API void EchoDatabaseVersion(gxDatabase *f)
// Echo the version and revision of an open database file to the console.
{
#if defined (__CONSOLE__)
  // 10/14/2026: The values come from the file, but they were printed
  // with the "Default Database Library" labels used for the library
  // defaults. The stream format was also changed and not restored, 
  // although the version is printed as an integer string.
  std::string intbuf;
  char rev_letter = f->GetRevLetter();
  intbuf.clear();
  str_append(intbuf, (FAU_t)f->GetVersion());
  std::cout << "Database File Version:  " << intbuf.c_str()
	      << "\n";
  if((rev_letter == '\0') || (rev_letter == ' ')) {
    std::cout << "Database File Revision: Zero" << "\n";
  }
  else {
    std::cout << "Database File Revision: " << rev_letter << "\n";
  }
#endif // Display type for stats messaging
}

// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
