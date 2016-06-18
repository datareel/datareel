// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: testprog.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 08/22/2000 
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

This is a test program for the B-tree node class.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include "btnode.h"
#include "gxdbase.h"

// Set the node order and maximum key size for this key class
const BtreeNodeOrder_t MyKeyClassOrder = 8;
const __WORD__ MyKeyNameSize = 26;

// Key class for this database
class MyKeyClass : public DatabaseKeyB
{
public:
  MyKeyClass();
  MyKeyClass(const char *name);
  void operator=(const char *name);
  ~MyKeyClass() { }

public: // Base class interface
  size_t KeySize() { return sizeof(key_name); }
  int operator==(const DatabaseKeyB& key) const;
  int operator>(const DatabaseKeyB& key) const;
  
  // NOTE: This comparison function is only used if the 
  // __USE_SINGLE_COMPARE__ preprocessor directive is 
  // defined when the program is compiled.
  int CompareKey(const DatabaseKeyB& key) const;

public: // Persistent data member
  char key_name[MyKeyNameSize];
};

MyKeyClass::MyKeyClass() : DatabaseKeyB((char *)key_name)
{
  for(int i = 0; i < MyKeyNameSize; i++) key_name[i] = 0;
}

MyKeyClass::MyKeyClass(const char *name) : DatabaseKeyB((char *)key_name)
{
  for(int i = 0; i < MyKeyNameSize; i++) key_name[i] = 0;
  strcpy(key_name, name);
}

void MyKeyClass::operator=(const char *name)
{
  for(int i = 0; i < MyKeyNameSize; i++) key_name[i] = 0;
  strcpy(key_name, name);
}

int MyKeyClass::operator==(const DatabaseKeyB& key) const
{
  const MyKeyClass *kptr = (const MyKeyClass *)(&key);
  return (strcmp(key_name, (char *)kptr->db_key) == 0);
}

int MyKeyClass::operator>(const DatabaseKeyB& key) const
{
  const MyKeyClass *kptr = (const MyKeyClass *)(&key);
  return (strcmp(key_name, (char *)kptr->db_key) > 0);
}

int MyKeyClass::CompareKey(const DatabaseKeyB& key) const
// NOTE: This comparison function is only used if the 
// __USE_SINGLE_COMPARE__ preprocessor directive is 
// defined when the program is compiled.
{
  const MyKeyClass *kptr = (const MyKeyClass *)(&key);
  return strcmp(key_name, (char *)kptr->db_key);
}

void PrintNode(BtreeNode &n)
// Print a Btree node.
{
  MyKeyClass kbuf;
  BtreeKeyLocation_t key_location = (BtreeKeyLocation_t)0;
  for(BtreeKeyCount_t i = (BtreeKeyCount_t)0; i < n.key_count; i++) {
    n.LoadKey(kbuf, key_location++);
    cout << (char *)kbuf.key_name << ' ' << (long)kbuf.right_child << ' ';
    // NOTE: The right child FAU is cast to a long for compilers
    // with native 64-bit int ostream overloads
  }
  cout << "\n";
  cout << "Key count = " << n.key_count << "\n";
  cout << "Left child = " << (long)n.left_child << "\n";
  // NOTE: The left child FAU is cast to a long for compilers
  // with native 64-bit int ostream overloads
}

void PausePrg()
// Pause the program.
{
  cout << "\n";
  cout << "Press enter to continue..." << "\n";
  cin.get();
}

gxDatabaseError WriteNode(gxDatabase *f, const BtreeNode &n, FAU_t na)
// Function used to write a Btree node to an open database file.
// Returns zero if successful or a non-zero value to indicate a
// failure. 
{
  gxDatabaseError rv;
  rv = f->Write(&n.key_count, sizeof(n.key_count), na);
  if(rv != gxDBASE_NO_ERROR) return rv;
  rv = f->Write(&n.left_child, sizeof(n.left_child));
  if(rv != gxDBASE_NO_ERROR) return rv;
  rv = f->Write(n.key_entries, (n.key_size * n.node_order));
  if(rv != gxDBASE_NO_ERROR) return rv;
  return rv;
}

gxDatabaseError ReadNode(gxDatabase *f, BtreeNode &n, FAU_t na)
// Function used to read a Btree node from an open database file.
// Returns zero if successful or a non-zero value to indicate a
// failure. 
{
  gxDatabaseError rv;
  rv = f->Read(&n.key_count, sizeof(n.key_count), na);
  if(rv != gxDBASE_NO_ERROR) return rv;
  rv = f->Read(&n.left_child, sizeof(n.left_child));
  if(rv != gxDBASE_NO_ERROR) return rv;
  rv = f->Read(n.key_entries, (n.key_size * n.node_order));
  if(rv != gxDBASE_NO_ERROR) return rv;
  return rv;
}

int main()
{
  MyKeyClass key, compare_key;

  cout << "--------- Key and B-tree Node Statistics ---------" << "\n";
  cout << "Key name size     = " << key.KeySize() << "\n";
  cout << "Database key size = " << key.SizeOfDatabaseKey() << "\n";
  cout << "Key entry size    = "
       << (key.SizeOfDatabaseKey() * (MyKeyClassOrder-1)) << "\n";
  BtreeNode n(key.SizeOfDatabaseKey(), MyKeyClassOrder);
  cout << "B-tree order      = " << n.node_order << "\n";
  cout << "B-tree node size  = " << n.SizeOfBtreeNode() << "\n";

  PausePrg();
  
  BtreeKeyLocation_t key_location = (BtreeKeyLocation_t)0;
  int i, rv;
  const char *strings[7] = { // Array matching the Btree order
    "DOG", "CAT", "PIG", "COW", "FISH", "ZEBRA", "HORSE" 
  };

  cout << "Inserting " << n.node_order << " entries into the Btree node"
       << "\n";
  for(i = 0; i < n.node_order; i++) {
    key = strings[i];

    rv = n.FindKeyLocation(key, compare_key, key_location);

    // Do not insert duplicate keys
    if(rv != 0) {
      key_location++;
      n.InsertDatabaseKey(key, key_location);
    }
  }

  cout << "Reading back the database keys" << "\n";
  PrintNode(n);

  PausePrg();
  
  cout << "Testing database read/write operations" << "\n";
  gxDatabase *f = new gxDatabase();
  f->Create("testfile.gxd");
  FAU_t node_address = f->Alloc(n.SizeOfBtreeNode());
  WriteNode(f, n, node_address);
  BtreeNode nbuf(key.SizeOfDatabaseKey(), MyKeyClassOrder);
  ReadNode(f, nbuf, node_address);
  PrintNode(nbuf);
  delete f;

  PausePrg();
  
  cout << "Testing the delete function" << "\n";
  n.DeleteDatabaseKey((BtreeKeyLocation_t)0);
  PrintNode(n);

  PausePrg();
  
  cout << "Testing the split fucntion" << "\n";
  BtreeNode r(key.SizeOfDatabaseKey(), MyKeyClassOrder);
  n.SplitNode(r, n.node_order/2);
  PrintNode(n);
  cout << "\n";
  PrintNode(r);

  cout << "\n";
  cout << "Exiting..." << "\n";
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
