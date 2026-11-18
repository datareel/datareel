// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: db_debug.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/04/1997  
// Date Last Modified: 10/31/2026
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

gxDatabase debug and recovery utility program used to troubleshoot 
and repair damaged or corrupt database files.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include <string.h>
#include "dbugmgr.h"

#if defined (__64_BIT_DATABASE_ENGINE__)
const char *ProgramName = "db_debug64";
#else // Use the 32-bit version by default
const char *ProgramName = "db_debug";
#endif

// Function prototypes for display menu
void ClearInputStream(GXSTD::istream &s);
int Quit();
void Menu(gxDatabaseDebugManager *f);
void PausePrg();
void Version();

// Function prototypes for database utilities 
void FindBlock(gxDatabaseDebugManager *f, int verbose = 0, int walk = 0);
void AnalyzeHeader(gxDatabaseDebugManager *f);
void DisplayStats(gxDatabaseDebugManager *f);
void Rebuild(gxDatabaseDebugManager *f, int update_ver = 0);

void Rebuild(gxDatabaseDebugManager *f, int update_ver)
{
  GXSTD::cout << "\n";
  if(update_ver) {
    GXSTD::cout << "Rebuilding database file and updating to \
latest version..." << "\n";
  }
  else {
    GXSTD::cout << "Rebuilding damaged database file..." << "\n";
  }
  GXSTD::cout << "\n";

  char fname[255];
  int retry = 3;
  UString intbuf;

  while(1) { // Loop until a good file name is found
    GXSTD::cout << "Enter new name of file to build>";
    GXSTD::cin.getline(fname, sizeof(fname));
    GXSTD::cout << "\n";
    if(!*fname) { // Return if nothing is entered
      Menu(f);
      return;
    }
    if(gxDatabase::Exists(fname)) {
      GXSTD::cout << "File already exists!" << "\n" << "\n";
       retry--;
      if(!retry) {
	Menu(f);
	return;
      }
    }
    else
      break;
  }

  GXSTD::cout << "Rebuilding: " << fname << "\n" << GXSTD::flush;
  GXSTD::cout << "\n" << GXSTD::flush;

  f->Rebuild(fname, update_ver);
  GXSTD::cout << f->status_message.c_str();

  GXSTD::cout << "\n" << GXSTD::flush;
  GXSTD::cout << "Rebuild complete" << "\n" << GXSTD::flush;
  GXSTD::cout << "\n" << GXSTD::flush;
}

void DisplayStats(gxDatabaseDebugManager *f)
{
  gxFileHeader fh;

  f->Read(&fh, sizeof(gxFileHeader), (FAU_t)0);
  if(CheckError((gxDatabase *)f) != 0) return;
  
  if(memcmp(fh.gxd_sig, gxDatabase::gxSignature, (gxSignatureSize-1))) {
    // Test file type
    GXSTD::cout << "\n";
    GXSTD::cout << "Database file header is damaged or does not exist" << "\n";
    GXSTD::cout << "\n";
    return;
  }

  DatabaseStats((gxDatabase *)f);
}

void AnalyzeHeader(gxDatabaseDebugManager *f)
{
  GXSTD::cout << "\n";
  GXSTD::cout << "Analyzing..." << "\n";
  GXSTD::cout << "\n";

  f->AnalyzeHeader();
  GXSTD::cout << f->status_message.c_str();

  GXSTD::cout << "\n";
  GXSTD::cout << "Analysis complete" << "\n";
  GXSTD::cout << "\n";
}

void FindBlock(gxDatabaseDebugManager *f, int verbose, int walk)
// Serach the file for all database block.
{
  gxBlockHeader blk;
  int count = 0;
  int badgx = 0;
  UString intbuf;

  GXSTD::cout << "\n";
  GXSTD::cout << "Searching file for all database blocks..." << "\n";
  
  FAU_t StaticEOF = f->FileSize(f->DatabaseName());
  FAU_t addr = f->BlockSearch((FAU_t)0); // Search the entire file
  
  if(addr == (FAU_t)0) {
    GXSTD::cout << "\n";
    GXSTD::cout << "No database blocks found in file: "
	 << f->DatabaseName() << "\n";
    GXSTD::cout << "\n";
    return;
  }

  while(1) { 
    if(addr >= StaticEOF) break;
    f->Read(&blk, sizeof(gxBlockHeader), addr);
    if(CheckError((gxDatabase *)f) != 0) return;
    
    // 10/31/2026: A block whose next address is not past this one and
    // inside the file is bad. With native types on 64-bit systems a
    // damaged length wrapped the address back to an earlier block and
    // the search never ended (found by the DataReel 7.3 security sweep).
    // 10/31/2026: Compare the length unsigned with the room left in the
    // file before adding it (DataReel 7.3.1: addr plus a huge length
    // overflowed where FAU_t is 32 bits), and a length shorter than a
    // block header is bad, as in gxDatabaseDebugManager::Rebuild().
    FAU_t next_addr = (FAU_t)0;
    int good = f->TestBlockHeader(blk) &&
      ((gxUINT32)blk.block_length >= (gxUINT32)f->BlockHeaderSize()) &&
      ((unsigned long long)blk.block_length <= 
       (unsigned long long)(StaticEOF - addr));
    if(good) next_addr = addr + (FAU_t)blk.block_length;
    if(good && (next_addr > addr)) { 
      count++; // Increment the database block count
      if(verbose) {
	BlockStats((gxDatabase *)f, (addr+f->BlockHeaderSize()));
	if(walk) {
	  char c;
	  GXSTD::cout << "\n";
	  GXSTD::cout << "Press enter to continue or enter 'X' to exit >";
	  GXSTD::cin.clear();
	  GXSTD::cin.get(c);
	  switch(c) {
	    case 'x' : case 'X' :
	      GXSTD::cout << "\n";
	      return;
	    default:
	      break;
	  }
	}
      }
      addr = next_addr; // Go to the next database block
    }
    else {
      badgx++;
      GXSTD::cout << "\n";
      intbuf << clear << addr;
      GXSTD::cout << "Found bad database block at address " << intbuf.c_str() 
		  << "\n";
      GXSTD::cout << "Searching for next good database block..." << "\n";
      // 10/31/2026: From the next byte, so the search always moves on
      addr = f->BlockSearch(addr + 1); // Search for the next good block
      if(!addr) {
	GXSTD::cout << "None found!" << "\n";
	GXSTD::cout << "\n";
	return;
      }
    }
  }
  GXSTD::cout << "\n";
  GXSTD::cout << "Found " << count << " good database blocks." << "\n";
  if(badgx) GXSTD::cout << "Found " << badgx << " bad database blocks." 
			<< "\n";
  GXSTD::cout << "\n";
}

void Menu(gxDatabaseDebugManager *f)
{
  GXSTD::cout << "\n";
  GXSTD::cout << "Analyzing file: " << f->DatabaseName() << "\n";
  GXSTD::cout << "Enter the letter of your selection at the prompt." << "\n";
  GXSTD::cout << "\n";
  GXSTD::cout << "(A, a)    - Analyze the database file header" << "\n";
  GXSTD::cout << "(D, d)    - Dump every database block" << "\n";
  GXSTD::cout << "(F, f)    - Find every database block in the file" << "\n";
  GXSTD::cout << "(H, h, ?) - Displays this menu" << "\n";
  GXSTD::cout << "(Q, q)    - Quit this program" << "\n";
  GXSTD::cout << "(r)       - Rebuild a damaged database file" << "\n";
  GXSTD::cout << "(R)       - Rebuild and update to current version/revision" 
	      << "\n";
  GXSTD::cout << "(S, s)    - Display database file statistics" << "\n";
  GXSTD::cout << "(W, s)    - Walk through every database block" << "\n";
  GXSTD::cout << "\n";
}

void ClearInputStream(GXSTD::istream &s)
// Used to clear istream
{
  char c;
  s.clear();
  while(s.get(c) && c != '\n') { ; }
}

void PausePrg()
{
  GXSTD::cout << "\n";
  GXSTD::cout << "Press enter to continue..." << "\n";
  GXSTD::cin.get();
}

int Quit()
{
  // Cleanup and exit the program
  GXSTD::cout << "Exiting..." << "\n";
  return 0;
}

void Version()
{
  GXSTD::cout << "\n";
  GXSTD::cout << ProgramName << " gxDatabase file recovery program." << "\n";
  EchoDatabaseVersion();
  GXSTD::cout << "\n";
}

int main(int argc, char **argv)
{
  // Display the program version information and exit the program
  if(argc >= 2) {
    if(strcmp(argv[1], "version") == 0) {
      Version();
      return 0;
    }
  }

  if(argc < 2) {
    GXSTD::cout << "\n";
    Version();
    GXSTD::cout << "Usage: " << ProgramName << " infile" << "\n";
    GXSTD::cout << "Usage: " << ProgramName << " infile (command)" << "\n";
    GXSTD::cout << "\n";
    return 1;
   }

  gxDatabaseDebugManager *f = new gxDatabaseDebugManager;   
  if(!f) {
    GXSTD::cout << "Memory allocation error!" << "\n";
    return 1;
  }

  const char *fname = argv[1];
  if(!gxDatabase::Exists(fname)) {
    GXSTD::cout << "The specified file does not exist!" << "\n";
    GXSTD::cout << "Exiting..." << "\n";
    delete f;
    return 1;
  }
  else {
    f->BlindOpen(fname);
    if(CheckError((gxDatabase *)f) != 0) {
      delete f;
      return 1;
    }
  }

  // Check for status messages
  if(!f->status_message.is_null()) {
    GXSTD::cout << f->status_message.c_str();
  }

  char key;
  if(argc <= 2) Menu(f); // Not processing a command
  int rv = 1;

  while(rv) {
    if(argc > 2) { // Process a single command and exit the loop
      key = *(argv[2]);
      rv = 0;
    }
    else {
      if (!GXSTD::cin) { 
	ClearInputStream(GXSTD::cin);
	if (!GXSTD::cin) {
          GXSTD::cout << "Input stream error" << "\n";
          return 0;
	}
      }
      GXSTD::cout << '>';
      GXSTD::cin >> key;
      if (!GXSTD::cin) continue;
    }
    switch(key) {
      case 'a' : case 'A' :
	if(argc <= 2) ClearInputStream(GXSTD::cin);
	AnalyzeHeader(f);
	break;
      case 'f' : case 'F' :
	if(argc <= 2) ClearInputStream(GXSTD::cin);
	FindBlock(f);
	break;
      case 'd' : case 'D' :
	if(argc <= 2) ClearInputStream(GXSTD::cin);
	FindBlock(f, 1);
	break;
      case 'h' : case 'H' :
	Menu(f);
	break;
      case '?' :
	Menu(f);
	break; 
      case 'q' : case 'Q' :
	rv = Quit();
	break;
      case 'r' : 
	if(argc <= 2) ClearInputStream(GXSTD::cin);
	Rebuild(f);
	break;
      case 'R' :
	if(argc <= 2) ClearInputStream(GXSTD::cin);
	Rebuild(f, 1);
	break;
      case 's' : case 'S' :
	if(argc <= 2) ClearInputStream(GXSTD::cin);
	DisplayStats(f);
	break;
      case 'w' : case 'W' :
	if(argc <= 2) ClearInputStream(GXSTD::cin);
	FindBlock(f, 1, 1);
	break;
      default:
        GXSTD::cout << "Unrecognized command" << "\n";
    }
  }

  delete f;
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //

