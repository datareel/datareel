// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: testprog.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/18/1997
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

This is a test program for the POD class.
*/
// ----------------------------------------------------------- //   
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include "pod.h"
#include "gxbtree.h"
#include "gxdstats.h"

const int NAME_LENGTH = 64;
const BtreeNodeOrder_t NODE_ORDER = 7;

struct DatabaseKey
{
  char object_name[NAME_LENGTH];
  FAU object_id;          
};

class DatabaseObject : public DatabaseKeyB
{ 
public:
  DatabaseObject();
  DatabaseObject(const char *name, FAU oid);

public: // Base class interface
  size_t KeySize() { return sizeof(primary_key); }
  int operator==(const DatabaseKeyB& key) const;
  int operator>(const DatabaseKeyB& key) const;
  
  // NOTE: This comparison function is only used if the 
  // __USE_SINGLE_COMPARE__ preprocessor directive is 
  // defined when the program is compiled.
  int CompareKey(const DatabaseKeyB& key) const;

public:
  DatabaseKey primary_key;
};

DatabaseObject::DatabaseObject() : DatabaseKeyB((char *)&primary_key)
{
  for(int i = 0; i < NAME_LENGTH; i++) 
    primary_key.object_name[i] = 0;
  primary_key.object_id = (FAU)0;
}

DatabaseObject::DatabaseObject(const char *name, FAU oid) : 
  DatabaseKeyB((char *)&primary_key)
{
  strncpy(primary_key.object_name, name,  NAME_LENGTH);
  primary_key.object_name[NAME_LENGTH-1] = 0; // Ensure null termination
  primary_key.object_id = oid;
}

int DatabaseObject::operator==(const DatabaseKeyB& key) const
{
  const DatabaseObject *kptr = (const DatabaseObject *)(&key);
  int rv = strcmp(primary_key.object_name, kptr->primary_key.object_name);
  return ((rv == 0) && (primary_key.object_id == kptr->primary_key.object_id));
}

int DatabaseObject::operator>(const DatabaseKeyB& key) const
{
  const DatabaseObject *kptr = (const DatabaseObject *)(&key);
  int rv = strcmp(primary_key.object_name, kptr->primary_key.object_name);
  return ((rv > 0) && (primary_key.object_id > kptr->primary_key.object_id));
}

int DatabaseObject::CompareKey(const DatabaseKeyB& key) const
// NOTE: This comparison function is only used if the 
// __USE_SINGLE_COMPARE__ preprocessor directive is 
// defined when the program is compiled.
{
  const DatabaseObject *kptr = (const DatabaseObject *)(&key);
  int rv = strcmp(primary_key.object_name, kptr->primary_key.object_name);
  if(rv == 0) {
    if(primary_key.object_id > kptr->primary_key.object_id) return 1;
    if(primary_key.object_id < kptr->primary_key.object_id) return -1;
  }
  return rv;
}

void PausePrg()
{
  cout << "\n";
  cout << "Press enter to continue..." << "\n";
  cin.get();
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

  const char *data_file1 = "testfile.gxd";
  const char *index_file1 = "testfile.btx";
  DatabaseObject key_type;

  // Open the POD database using a single index file and check for any errors
  POD pod;
  cout << "Opening a POD database using a single index file" << "\n";
  gxDatabaseAccessMode mode = gxDBASE_READWRITE;
  int num_trees = 1; // Number of trees per index file
  int use_index = 1;
  FAU_t static_size = (FAU_t)0;
  __SBYTE__ df_rev_letter = rev_letter;
  __SBYTE__ if_rev_letter = rev_letter;
  gxDatabaseError err = pod.Open(data_file1, index_file1, key_type,
				 NODE_ORDER, mode, use_index, static_size,
				 num_trees, df_rev_letter, if_rev_letter);
  if(CheckError(err) != 0) return 1; // See the src/gxdstats.cpp file

  // Multiple index file test using the same key type and node order
  const unsigned num_indexes = 5;
  unsigned index_number;

  // Construct and initialize an index file name array
  char *index_file2[POD_MAX_INDEX] = { 
    "idx0.btx", "idx1.btx", "idx2.btx", "idx3.btx", "idx4.btx" 
  };
  const char *data_file2 = "testdat2.gxd";

  cout << "Opening a database using multiple index files with the \
same key type" << "\n";
  err = pod.Open(data_file2, index_file2, num_indexes, key_type, NODE_ORDER,
		 mode, static_size, num_trees, df_rev_letter, if_rev_letter);
  if(CheckError(err) != 0) return 1;
  
  // Multiple index file test using varying node orders.
  // NOTE: In this example varying node orders were used in place of
  // multiple key types because only one key class is defined.
  cout << "Opening a database using multiple index files with varying \
node order" << "\n";
  const char *index_file3[num_indexes] = { 
    "idx5.btx", "idx6.btx", "idx7.btx", "idx8.btx", "idx9.btx" 
  };
  const char *data_file3 = "testdat3.gxd";
  PODIndexFile *openindexfile = new PODIndexFile(num_indexes);
  // NOTE: The memory allocated for the openindexfile variable 
  // will be freed when the POD opject is destroyed for the POD
  // file is closed. Following a Close() call you must allocate 
  // new memory if the openindexfile varaible is reused.

  for(index_number = 0; index_number < num_indexes; index_number++) {
    err = openindexfile->Open(index_file3[index_number], index_number,
			      key_type, (NODE_ORDER+index_number), mode,
			      num_trees, if_rev_letter);
    if(CheckError(err) != 0) return 1;
  }
  
  err = pod.Open(data_file3, openindexfile, mode, static_size, df_rev_letter);
  if(CheckError(err) != 0) return 1;
  cout << "\n";
  
  // NOTE: The POD destructor will automatically delete the dynamically
  // allocated PODIndexFile file object.
  return 0; 
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
