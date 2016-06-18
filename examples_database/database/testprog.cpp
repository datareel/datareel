// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: testprog.cpp
// Compiler Used: MSVC, BCC32, GCC HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/04/1997  
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

Test program used to compare the features of the 32-bit database
engine to the 64-bit database engine..
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
#include <iomanip>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#include <iomanip.h>
#endif // __USE_ANSI_CPP__

#include <string.h>
#include <time.h>
#include "gxfloat.h"
#include "gxdbase.h"
#include "gxdstats.h"
#include "ustring.h"

// Various static data area sizes used to test large files
const FAU_t STATIC_AREA_SIZE = (FAU_t)255*255;

// 1 GIG file test                              
// WARNING: This test requires 1.1 GIG of free disk space
// const FAU_t STATIC_AREA_SIZE = (FAU_t)1100000000;

// 2.1 GIG file test                              
// WARNING: This test requires 2.1 GIG of free disk space
// const FAU_t STATIC_AREA_SIZE = (FAU_t)2100000000;
                                                   
// 2.1 GIG large file test                               
// WARNING: This test requires 2.3 GIG of free disk space
// const FAU_t_ STATIC_AREA_SIZE = (__LLWORD__)2147483647;

// 4 GIG large file test
// WARNING: This test requires 4.3 GIG of free disk space
// const FAU_t_ STATIC_AREA_SIZE = (__LLWORD__)4294967290;

const int NAME_LENGTH = 64;

class DatabaseObject
{ 
public:
  DatabaseObject() { name[0] = 0; oid = (FAU)0, cid = (gxFLOAT64)0; }
  DatabaseObject(const char *s, long i, double d);

public:
  void DisplayObject();
  
public: // Platform independent data members
  char name[NAME_LENGTH]; // Fixed string type
  FAU oid;                // Integer type
  gxFLOAT64 cid;          // Floating point type
};

DatabaseObject::DatabaseObject(const char *s, long i, double d)
{
  for(int j = 0; j < NAME_LENGTH; j++) name[j] = 0; // Clear the name string
  strcpy(name, s);
  oid = i;  
  cid = d;
}

void DatabaseObject::DisplayObject()
{
  UString intbuf;
  cout << "Database object name: " << name << "\n";
  intbuf << (FAU_t)oid;
  cout << "Database object OID:  " << intbuf.c_str() << "\n";
  cout.setf(ios::showpoint | ios::fixed);
  cout.precision(3);
  cout << "Database object CID:  " << cid << "\n";
}

void PausePrg()
{
  cout << "\n";
  cout << "Press enter to continue..." << "\n";
  cin.get();
}

int main(int argv, char **argc)
{
  const char *fname = "simple.gxd"; // File name of this database
  char rev_letter = gxDatabaseRevisionLetter; // Set the default rev letter
  if(argv == 2) { // Set a specified revision letter
    rev_letter = *argc[1];
    if(rev_letter == '0') rev_letter = '\0';
    // Valid rev letters are:
    // Rev 0
    // Rev 'A' or 'a'
    // Rev 'B' or 'b'
    // Rev 'C' or 'c'
    // Rev 'D' or 'd'
    // Rev 'E' or 'e'
    // NOTE: The gxDatabase class will set invalid revision letters
    // to the version set by the gxDatabaseRevisionLetter constant.  
  }

  // Adjust this number to set the number of insertions
  const unsigned long INSERTIONS = 1 * 1000;       // 1K test
  // const unsigned long INSERTIONS = 10 * 1000;      // 10K test
  // const unsigned long INSERTIONS = 100 * 1000;     // 100K test
  // const unsigned long INSERTIONS = 1000 * 1000;    // 1MEG test
  // const unsigned long INSERTIONS = 10000 * 1000;   // 10MEG test
  // const unsigned long INSERTIONS = 100000 * 1000;  // 100MEG test
  // const unsigned long INSERTIONS = 1000000 * 1000; // 1GIG test

  gxDatabase *f = new gxDatabase(); 
  unsigned long i;
  FAU_t static_area_size(STATIC_AREA_SIZE);
  DatabaseObject buf;
  
  cout << "\n";
  cout << "Insertion time benchmark of the database engine." << "\n";
  
  cout << "Creating new file..." << "\n";
  f->Create(fname, static_area_size, rev_letter);
  if(CheckError(f) != 0) {
    delete f;
    return 1;
  }

  PausePrg();
  DatabaseStats(f);
  PausePrg();

  cout << "Adding " << INSERTIONS << " objects to the database file..." 
       << "\n";
  cout << "Size of each object = " << sizeof(DatabaseObject) << "\n";
  PausePrg();
  
  unsigned long curr_count = 0;
  double insert_time = 0;
  
  // Get CPU clock cycles before entering loop
  clock_t begin = clock();

  for(i = 0; i < INSERTIONS; i++) {
    // Construct the datbase object
    char name[NAME_LENGTH];
    sprintf(name, "Mouse %i", (int)i);
    DatabaseObject ob(name, (FAU_t)i, 5000.101);

    clock_t begin_insert = clock();  
    f->Alloc(sizeof(DatabaseObject));
    if(CheckError(f) != 0) {
      delete f; 
      // The gxDatabase destructor will close the database file
      // but in an application the Close() call should be tested 
      // to prevent any possible data corruption. 
      return 1;
    }
    f->Write(&ob, sizeof(DatabaseObject));
    if(CheckError(f) != 0) {
      delete f;
      return 1;
    }
    clock_t end_insert = clock();
    insert_time += (double)(end_insert - begin_insert) / CLOCKS_PER_SEC;
    curr_count++;

    if(CheckError(f) != 0) {
      delete f;
      return 1;
    }
    
    if(curr_count == 10000) {
      curr_count = 0;
      cout << "Inserted " << i << " objects in " << insert_time
	   << " seconds" << "\n";
    }
  }
  
  // Get CPU clock cycles after loop is completed 
  clock_t end = clock();

  // Calculate the elapsed time in seconds. 
  double elapsed_time = (double)(end - begin) / CLOCKS_PER_SEC;
  cout.precision(3); 
  cout << "Inserted " << i << " values in " 
       << elapsed_time << " seconds" << "\n";
  double avg_insert_time = (insert_time/(double)i) * 1000;
  cout << "Average insert time = " << avg_insert_time << " milliseconds"  
       << "\n"; 
  
  PausePrg();

  cout << "Testing the flush the function following batch insert" << "\n";
  f->Flush();

  PausePrg();
  
  cout << "Reading " << INSERTIONS << " objects from the database file..." 
       << "\n";
  PausePrg();

  double search_time = 0;  
  curr_count = 0;
  FAU_t offset = f->GetHeapStart() + f->BlockHeaderSize();
  begin = clock();

  for(i = 0; i < INSERTIONS; i++) {
    clock_t begin_search = clock();
    f->Read(&buf, sizeof(buf), offset);
    clock_t end_search = clock();
    search_time += (double)(end_search - begin_search) / CLOCKS_PER_SEC;
    if(CheckError(f) != 0) {
      delete f;
      return 1;
    }
    curr_count++;
    offset += (f->BlockHeaderSize() + sizeof(buf)); 
    if(curr_count == 10000) {
      curr_count = 0;
      cout << "Read " << i << " objects in " << search_time
	   << " seconds" << "\n";
    }
  }
  
  end =clock();
  elapsed_time = (double)(end - begin) / CLOCKS_PER_SEC;
  cout.precision(3);
  cout << "Verified " << i << " values in " 
       << elapsed_time << " seconds" << "\n";
  double avg_search_time = (search_time/(double)i) * 1000;
  cout << "Average search time = " << avg_search_time << " milliseconds"
       << "\n";

  PausePrg();
  DatabaseStats(f);
  PausePrg();

  cout << "Exiting..." << "\n";
  f->Close();
  if(CheckError(f) != 0) {
    delete f;
    return 1;
  }

  delete f;
  return 0; 
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
