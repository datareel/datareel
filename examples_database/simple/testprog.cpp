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

Simple demo of the gxDatabase class.
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
#include "gxdbase.h"
#include "gxfloat.h"
#include "gxdstats.h"
#include "ustring.h"

const int name_length = 16;

class DatabaseObject
{ 
public:
  DatabaseObject() { name[0] = 0; oid = (gxINT32)0, cid = (gxFLOAT64)0; }
  DatabaseObject(const char *s, long i, double d);

public:
  void DisplayObject();
  
public: // Platform independent data members
  char name[name_length]; // Fixed string type
  gxINT32 oid;            // Integer type
  gxFLOAT64 cid;          // Floating point type
};

DatabaseObject::DatabaseObject(const char *s, long i, double d)
{
  for(int j = 0; j < name_length; j++) name[j] = 0; // Clear the name string
  strcpy(name, s);
  oid = i;  
  cid = d;
}

void DatabaseObject::DisplayObject()
{
  cout << "Database object name: " << name << "\n";
  cout << "Database object OID:  " << oid << "\n";
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

  // Using pointer semantics to prevent copying
  gxDatabase *f = new gxDatabase; 
  if(!f) {
    cout << "Could not allocate memory for file pointer" << "\n";
    return 1;
  }

  
  cout << "\n";
  cout << "Simple demo of the file manager class." << "\n";

  cout << "Creating new file..." << "\n";
  f->Create(fname, (FAU_t)0, rev_letter);
  if(CheckError(f) != 0) {
    delete f;
    return 1;
  }

  // Echo the file version and revision letter
  EchoDatabaseVersion(f);
  
  PausePrg();
  DatabaseStats(f);
  PausePrg();
  
  cout << "Adding object to database file..." << "\n";
  cout << "Size of object = " << sizeof(DatabaseObject) << "\n";
  
  // Allocate a block to store the object
  FAU_t addr = f->Alloc(sizeof(DatabaseObject)); 
  if(CheckError(f) != 0) {
    delete f;
    return 1;
  }

  // Construct the datbase object
  DatabaseObject ob("Mouse", (long)addr, 5000.101);
  
  // Write the objects data
  f->Write(&ob, sizeof(DatabaseObject));
  UString intbuf;
  if(CheckError(f) != 0) {
    delete f;
    return 1;
  }
  intbuf << clear << (FAU_t)(addr - f->BlockHeaderSize());
  cout << "Block Address: " << intbuf.c_str()  << "\n";
  intbuf << clear << (FAU_t)addr;
  cout << "Writing object to FAU: " << intbuf.c_str() << "\n";
  
  PausePrg();
  BlockStats(f, addr);
  PausePrg();

  DatabaseObject buf;
  cout << "Reading the first object from the database file..." << "\n";
  addr = f->FindFirstBlock();

  // Read the objects data
  if(addr) {
    f->Read(&buf, sizeof(buf), (FAU_t)(addr+f->BlockHeaderSize()));
    if(CheckError(f) != 0) {
      delete f;
      return 1;
    }
    buf.DisplayObject();
  }
  
  PausePrg();
  DatabaseStats(f);
  PausePrg();

  cout << "Exiting..." << "\n";

  // Close the file and check for any errors
  f->Close();
  if(CheckError(f) != 0) {
    delete f;
    return 1;
  }

  // Delete the object
  delete f; // The destructor call will close the file if it is open 
  
  return 0; 
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
