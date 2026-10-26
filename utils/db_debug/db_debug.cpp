// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: db_debug.cpp
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

gxDatabase debug and recovery utility program used to troubleshoot 
and repair damaged or corrupt database files.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <iostream>

#include <string.h>
#include "dbugmgr.h"

#if defined (__64_BIT_DATABASE_ENGINE__)
const char *ProgramName = "db_debug64";
#else // Use the 32-bit version by default
const char *ProgramName = "db_debug";
#endif

// Function prototypes for display menu
void ClearInputStream(std::istream &s);
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
  std::cout << "\n";
  if(update_ver) {
    std::cout << "Rebuilding database file and updating to \
latest version..." << "\n";
  }
  else {
    std::cout << "Rebuilding damaged database file..." << "\n";
  }
  std::cout << "\n";

  char fname[255];
  int retry = 3;
  UString intbuf;

  while(1) { // Loop until a good file name is found
    std::cout << "Enter new name of file to build>";
    std::cin.getline(fname, sizeof(fname));
    std::cout << "\n";
    if(!*fname) { // Return if nothing is entered
      Menu(f);
      return;
    }
    if(gxDatabase::Exists(fname)) {
      std::cout << "File already exists!" << "\n" << "\n";
       retry--;
      if(!retry) {
	Menu(f);
	return;
      }
    }
    else
      break;
  }

  std::cout << "Rebuilding: " << fname << "\n" << std::flush;
  std::cout << "\n" << std::flush;

  f->Rebuild(fname, update_ver);
  std::cout << f->status_message.c_str();

  std::cout << "\n" << std::flush;
  std::cout << "Rebuild complete" << "\n" << std::flush;
  std::cout << "\n" << std::flush;
}

void DisplayStats(gxDatabaseDebugManager *f)
{
  gxFileHeader fh;

  f->Read(&fh, sizeof(gxFileHeader), (FAU_t)0);
  if(CheckError((gxDatabase *)f) != 0) return;
  
  if(memcmp(fh.gxd_sig, gxDatabase::gxSignature, (gxSignatureSize-1))) {
    // Test file type
    std::cout << "\n";
    std::cout << "Database file header is damaged or does not exist" << "\n";
    std::cout << "\n";
    return;
  }

  DatabaseStats((gxDatabase *)f);
}

void AnalyzeHeader(gxDatabaseDebugManager *f)
{
  std::cout << "\n";
  std::cout << "Analyzing..." << "\n";
  std::cout << "\n";

  f->AnalyzeHeader();
  std::cout << f->status_message.c_str();

  std::cout << "\n";
  std::cout << "Analysis complete" << "\n";
  std::cout << "\n";
}

void FindBlock(gxDatabaseDebugManager *f, int verbose, int walk)
// Serach the file for all database block.
{
  gxBlockHeader blk;
  int count = 0;
  int badgx = 0;
  UString intbuf;

  std::cout << "\n";
  std::cout << "Searching file for all database blocks..." << "\n";
  
  FAU_t StaticEOF = f->FileSize(f->DatabaseName());
  FAU_t addr = f->BlockSearch((FAU_t)0); // Search the entire file
  
  if(addr == (FAU_t)0) {
    std::cout << "\n";
    std::cout << "No database blocks found in file: "
	 << f->DatabaseName() << "\n";
    std::cout << "\n";
    return;
  }

  while(1) { 
    if(addr >= StaticEOF) break;
    f->Read(&blk, sizeof(gxBlockHeader), addr);
    if(CheckError((gxDatabase *)f) != 0) return;
    
    if(f->TestBlockHeader(blk)) { 
      count++; // Increment the database block count
      if(verbose) {
	BlockStats((gxDatabase *)f, (addr+f->BlockHeaderSize()));
	if(walk) {
	  char c;
	  std::cout << "\n";
	  std::cout << "Press enter to continue or enter 'X' to exit >";
	  std::cin.clear();
	  std::cin.get(c);
	  switch(c) {
	    case 'x' : case 'X' :
	      std::cout << "\n";
	      return;
	    default:
	      break;
	  }
	}
      }
      addr = addr + blk.block_length; // Go to the next database block
    }
    else {
      badgx++;
      std::cout << "\n";
      intbuf << clear << addr;
      std::cout << "Found bad database block at address " << intbuf.c_str() 
		  << "\n";
      std::cout << "Searching for next good database block..." << "\n";
      addr = f->BlockSearch(addr); // Search for the next good block
      if(!addr) {
	std::cout << "None found!" << "\n";
	std::cout << "\n";
	return;
      }
    }
  }
  std::cout << "\n";
  std::cout << "Found " << count << " good database blocks." << "\n";
  if(badgx) std::cout << "Found " << badgx << " bad database blocks." 
			<< "\n";
  std::cout << "\n";
}

void Menu(gxDatabaseDebugManager *f)
{
  std::cout << "\n";
  std::cout << "Analyzing file: " << f->DatabaseName() << "\n";
  std::cout << "Enter the letter of your selection at the prompt." << "\n";
  std::cout << "\n";
  std::cout << "(A, a)    - Analyze the database file header" << "\n";
  std::cout << "(D, d)    - Dump every database block" << "\n";
  std::cout << "(F, f)    - Find every database block in the file" << "\n";
  std::cout << "(H, h, ?) - Displays this menu" << "\n";
  std::cout << "(Q, q)    - Quit this program" << "\n";
  std::cout << "(r)       - Rebuild a damaged database file" << "\n";
  std::cout << "(R)       - Rebuild and update to current version/revision" 
	      << "\n";
  std::cout << "(S, s)    - Display database file statistics" << "\n";
  std::cout << "(W, s)    - Walk through every database block" << "\n";
  std::cout << "\n";
}

void ClearInputStream(std::istream &s)
// Used to clear istream
{
  char c;
  s.clear();
  while(s.get(c) && c != '\n') { ; }
}

void PausePrg()
{
  std::cout << "\n";
  std::cout << "Press enter to continue..." << "\n";
  std::cin.get();
}

int Quit()
{
  // Cleanup and exit the program
  std::cout << "Exiting..." << "\n";
  return 0;
}

void Version()
{
  std::cout << "\n";
  std::cout << ProgramName << " gxDatabase file recovery program." << "\n";
  EchoDatabaseVersion();
  std::cout << "\n";
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
    std::cout << "\n";
    Version();
    std::cout << "Usage: " << ProgramName << " infile" << "\n";
    std::cout << "Usage: " << ProgramName << " infile (command)" << "\n";
    std::cout << "\n";
    return 1;
   }

  gxDatabaseDebugManager *f = new gxDatabaseDebugManager;   
  if(!f) {
    std::cout << "Memory allocation error!" << "\n";
    return 1;
  }

  const char *fname = argv[1];
  if(!gxDatabase::Exists(fname)) {
    std::cout << "The specified file does not exist!" << "\n";
    std::cout << "Exiting..." << "\n";
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
    std::cout << f->status_message.c_str();
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
      if (!std::cin) { 
	ClearInputStream(std::cin);
	if (!std::cin) {
          std::cout << "Input stream error" << "\n";
          return 0;
	}
      }
      std::cout << '>';
      std::cin >> key;
      if (!std::cin) continue;
    }
    switch(key) {
      case 'a' : case 'A' :
	if(argc <= 2) ClearInputStream(std::cin);
	AnalyzeHeader(f);
	break;
      case 'f' : case 'F' :
	if(argc <= 2) ClearInputStream(std::cin);
	FindBlock(f);
	break;
      case 'd' : case 'D' :
	if(argc <= 2) ClearInputStream(std::cin);
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
	if(argc <= 2) ClearInputStream(std::cin);
	Rebuild(f);
	break;
      case 'R' :
	if(argc <= 2) ClearInputStream(std::cin);
	Rebuild(f, 1);
	break;
      case 's' : case 'S' :
	if(argc <= 2) ClearInputStream(std::cin);
	DisplayStats(f);
	break;
      case 'w' : case 'W' :
	if(argc <= 2) ClearInputStream(std::cin);
	FindBlock(f, 1, 1);
	break;
      default:
        std::cout << "Unrecognized command" << "\n";
    }
  }

  delete f;
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //

