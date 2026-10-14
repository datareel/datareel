// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: ix_debug.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 08/09/2001
// Date Last Modified: 06/17/2016
// Copyright (c) 2001-2025 DataReel Software Development
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

B-tree index file debugging utility for the gxBtree class.
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
#include "gxbtree.h"
#include "ustring.h"

#if defined (__64_BIT_DATABASE_ENGINE__)
const char *ProgramName = "ix_debug64";
#else // Use the 32-bit version by default
const char *ProgramName = "ix_debug";
#endif

void Version()
{
  GXSTD::cout << "\n";
  // 10/14/2026: Was "file recovery program". This program only reads
  // the index file and displays its B-tree headers.
  GXSTD::cout << ProgramName << " gxBtree index file debug program." << "\n";
  EchoDatabaseVersion();
  GXSTD::cout << "\n";
}

void PausePrg()
{
  GXSTD::cout << "\n";
  GXSTD::cout << "Press enter to continue..." << "\n";
  GXSTD::cin.get();
}

void DisplayBtreeHeader(gxBtreeHeader &bh, int tree_number = 1)
{
  UString intbuf;
  GXSTD::cout << "\n";
  GXSTD::cout << "B-tree header stats for tree number " << tree_number
	      << "\n";
  GXSTD::cout << "B-Tree node order = " << bh.node_order << "\n";
  GXSTD::cout << "B-Tree key size   = " << bh.key_size << "\n";
  GXSTD::cout << "Number of keys    = " << bh.n_keys << "\n";
  GXSTD::cout << "Number of nodes   = " << bh.n_nodes << "\n";
  GXSTD::cout << "B-Tree height     = " << bh.btree_height << "\n";
  intbuf << clear << (FAU_t)bh.root;
  GXSTD::cout << "Root node address = " << intbuf.c_str() << "\n";
  GXSTD::cout << "Number of trees   = " << bh.num_trees << "\n";
  PausePrg();
}

int main(int argc, char **argv)
{
  // 10/14/2026: The usage message offered an optional command after the
  // file name, but no command was ever read. The program now takes
  // exactly one argument and reports an error for extra arguments.
  if(argc != 2) {
    GXSTD::cout << "\n";
    Version();
    GXSTD::cout << "Usage: " << ProgramName << " infile" << "\n";
    GXSTD::cout << "Usage: " << ProgramName << " version" << "\n";
    GXSTD::cout << "\n";
    return 1;
  }

  const char *fname = argv[1];

  // Display the program version information and exit the program.
  // 10/14/2026: A file named "version" could not be opened. The switch
  // is now only used when no file with that name exists.
  if(strcmp(fname, "version") == 0) {
    if(!gxDatabase::Exists(fname)) {
      Version();
      return 0;
    }
  }

  gxDatabaseDebugManager *f = new gxDatabaseDebugManager;   
  if(!f) {
    GXSTD::cout << "Memory allocation error!" << "\n";
    return 1;
  }

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

  // 10/14/2026: The static area checks below returned a zero (success)
  // exit status. They now return 1 so scripts can detect the error.
  FAU_t sa = (FAU_t)f->StaticArea();
  if(sa < (FAU_t)sizeof(gxBtreeHeader)) {
    GXSTD::cout << "Cannot connect to B-tree header" << "\n";
    if(sa <= (FAU_t)0) {
      GXSTD::cout << "This gxDatabase file has no static storage area \
allocated" << "\n";
    }
    else {
      GXSTD::cout << "The static storage area is not large enough to store a \
B-tree header" << "\n";
    }
    delete f;
    return 1;
  }

  int test_tree_hdr = (int)(sa % (FAU_t)sizeof(gxBtreeHeader));
  if(test_tree_hdr != 0) {
    GXSTD::cout << "Detected a storage area error" << "\n";
    // 10/14/2026: Corrected the spelling of "information"
    GXSTD::cout << "The static storage area does not contain valid B-tree \
header information" << "\n"; 
    delete f;
    return 1;
  }

  // 10/14/2026: Only the first B-tree header was displayed. Display
  // every header stored in the static storage area.
  FAU_t num_trees = sa / (FAU_t)sizeof(gxBtreeHeader);
  UString intbuf;
  intbuf << clear << num_trees;
  GXSTD::cout << "Found room for " << intbuf.c_str() 
	      << " B-tree header(s) in the static storage area" << "\n";

  for(FAU_t i = (FAU_t)0; i < num_trees; i++) {
    if(i == (FAU_t)0) {
      // 10/14/2026: Corrected the word order of this message
      GXSTD::cout << "Connecting to the first B-tree header..." << "\n";
    }
    gxBtreeHeader btree_header;
    FAU_t hdr_address = (FAU_t)f->FileHeaderSize() + 
      (i * (FAU_t)sizeof(gxBtreeHeader));
    f->Read(&btree_header, sizeof(gxBtreeHeader), hdr_address);
    if(CheckError((gxDatabase *)f) != 0) {
      delete f;
      return 1;
    }
    DisplayBtreeHeader(btree_header, (int)(i+1));
  }

  delete f;
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
