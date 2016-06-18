// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: testprog.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
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

Test program used to test the gxDatabase::Reclaim() function
using the first-fit or best-fit method of reclaiming deleted
and removed blocks.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include <time.h>
#include "gxdbase.h"
#include "gxdstats.h"
#include "ustring.h"

void PausePrg()
{
  cout << "\n";
  cout << "Press enter to continue..." << "\n";
  cin.get();
}

clock_t Start()
// Mark the starting time for the routine.
{
  return clock();
}

clock_t Stop()
// Mark the stop time for the routine. 
{
  return clock();
}

double ElapsedTime(clock_t begin, clock_t end)
// Calculate the elapsed time in milliseconds. 
{
  return (double)(end - begin) / CLOCKS_PER_SEC;
}

void DefragStats(gxDatabase *f)
{
  UString intbuf;
  FAU_t td, d, r, n, t;
  t = f->TotalBlocks();
  n = f->NormalBlocks();
  td = f->DeletedBlocks(&d, &r);
  
  cout << "\n";
  
  intbuf << clear << t;
  cout << "Number of blocks allocated: " << intbuf.c_str() << "\n";
  intbuf << clear << n;
  cout << "Total blocks in use: " << intbuf.c_str() << "\n";
  intbuf << clear << d;
  cout << "Total deleted/removed:  " << intbuf.c_str() << "/";
  intbuf << clear << r;
  cout << intbuf.c_str() << " (";
  intbuf << clear << td;
  cout << intbuf.c_str() << ")" << "\n";
}

void TestReclaimMethod(gxDatabaseReclaimMethod reclaim_method,
		       char rev_letter)
{
  const int NumObjects = 100; // Number of objects to allocate
  const char *fname = "defrag.gxd";
  
  gxDatabase *f = new gxDatabase;

  cout << "\n";
  cout << "Testing the best-fit/first-fit allocation methods." << "\n";
  
  if(reclaim_method == gxDBASE_RECLAIM_BESTFIT) {
  cout << "\n";
  cout << "Using the best-fit method to reclaim deleted/removed blocks."
       << "\n";
  }
  else { 
  cout << "\n";
  cout << "Using the first-fit method to reclaim deleted/removed blocks."
       << "\n";
  }

  if(!gxDatabase::Exists(fname)) {
    cout << "Creating new file..." << "\n";
    f->Create(fname, (FAU_t)0, rev_letter);
    if(CheckError(f) != 0) {
      delete f;
      return;
    }
  }
  else {
    cout << "Opening existing file..." << "\n";
    f->Open(fname);
    if(CheckError(f) != 0) {
      delete f;
      return;
    }
  }

  PausePrg();
  DatabaseStats(f);
  PausePrg();

  FAU_t addr;

  FAU_t addr_list[NumObjects];
  char data = 'X';
  int i,j;

  cout << "Adding " << NumObjects << " objects to the file." << "\n";
  cout << "Objects range from " << sizeof(data) << " to " << NumObjects
       << " bytes in length"
       << "\n";
  cout << "Writing..." << "\n";
  
  clock_t Begin = Start();
  for(i = 0; i < NumObjects; i++) {
    addr = f->Alloc(sizeof(data)+i, reclaim_method);
    addr_list[i] = addr; // Store the address of each node allocated
    for(j = 0; j <= i; j++) {
      f->Write(&data, sizeof(data), addr+j, 0, 0);
    }
  }
  clock_t End = Stop();

  cout.precision(3);
  cout << "Comleted in " << ElapsedTime(Begin, End) << " seconds" << "\n";
  DefragStats(f);
  PausePrg();
  
  cout << "Deleting all the blocks allocated that were just allocated."
       << "\n";
  cout << "Working..." << "\n";
  
  Begin = Start();
  for(i = 0; i < NumObjects; i++) f->Delete(addr_list[i]);;
  End = Stop();
  
  cout.precision(3);
  cout << "Comleted in " << ElapsedTime(Begin, End) << " seconds" << "\n";
  DefragStats(f);
  PausePrg();
  
  cout << "Fragmenting the file." << "\n";
  cout << "Undeleting every other block that was just deleted."
       << "\n";
  cout << "Working..." << "\n";
  
  Begin = Start();
  for(i = 0; i < NumObjects; i++) {
    f->UnDelete(addr_list[i]);
    i++; // Skip to the next block
  }
  End = Stop();
  
  cout.precision(3);
  cout << "Comleted in " << ElapsedTime(Begin, End) << " seconds" << "\n";
  DefragStats(f);
  PausePrg();
  
  cout << "Adding " << NumObjects << " objects to the file." << "\n";
  cout << "All objects are " << sizeof(data) << " bytes in length."
       << "\n";
  cout << "Writing..." << "\n";

  Begin = Start();
  for(i = 0; i < NumObjects; i++) {
    f->Alloc(sizeof(data), reclaim_method);
    f->Write(&data, sizeof(data), gxCurrAddress, 0, 0);
  }
  End = Stop();
  
  cout.precision(3);
  cout << "Comleted in " << ElapsedTime(Begin, End) << " seconds" << "\n";
  DefragStats(f);
  PausePrg();
  
  int offset = 1;
  cout << "Adding " << NumObjects << " objects to the file." << "\n";
  cout << "Objects range from " << (sizeof(data) + offset) << " to "
       << (NumObjects + offset) << " bytes in length"
       << "\n";
  cout << "Writing..." << "\n";

  Begin = Start();
  for(i = 0; i < NumObjects; i++) {
    addr = f->Alloc((sizeof(data)+i)+offset, reclaim_method);
    for(j = 0; j <= i; j++)
      f->Write(&data, sizeof(data), (addr+j)+offset, 0, 0);
  }
  End = Stop();  

  cout.precision(3);
  cout << "Comleted in " << ElapsedTime(Begin, End) << " seconds" << "\n";
  DefragStats(f);

  cout << "\n";
  cout << "Exiting..." << "\n";
  f->Close();
  if(CheckError(f) != 0) return;
  delete f;
}

int main(int argv, char **argc)
{
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

  TestReclaimMethod(gxDBASE_RECLAIM_BESTFIT, rev_letter);
  TestReclaimMethod(gxDBASE_RECLAIM_FIRSTFIT, rev_letter);
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
