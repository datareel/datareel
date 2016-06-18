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

Simple test program for the gxBtree class.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include "gxdstats.h"
#include "gxbtree.h"
#include "ustring.h"

const BtreeNodeOrder_t MyKeyClassOrder = 7;
const __WORD__ MyKeyNameSize = 64;

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
  strncpy(key_name, name,  MyKeyNameSize);
  key_name[ MyKeyNameSize-1] = 0; // Ensure null termination
}

void MyKeyClass::operator=(const char *name)
{
  strncpy(key_name, name,  MyKeyNameSize);
  key_name[ MyKeyNameSize-1] = 0; // Ensure null termination
}

int MyKeyClass::CompareKey(const DatabaseKeyB& key) const
// NOTE: This comparison function is only used if the 
// __USE_SINGLE_COMPARE__ preprocessor directive is 
// defined when the program is compiled.
{
  const MyKeyClass *kptr = (const MyKeyClass *)(&key);
  return strcmp(key_name, (char *)kptr->db_key);
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

void PrintNode(BtreeNode *n)
// Prints a single B-tree node.
{
  UString intbuf;
  MyKeyClass key;
  BtreeKeyCount_t i = (BtreeKeyCount_t)0;
  cout << "[";
  intbuf << clear << (FAU_t)n->node_address;
  cout << "Node Address: " << intbuf.c_str(); 
  cout << ", ";
  intbuf << clear << (FAU_t)n->left_child;
  cout << "Left Node: " << intbuf.c_str();
  cout << "]";
  while (i < n->key_count) {
    n->LoadKey(key, (BtreeKeyLocation_t)i);
    cout << " <";
    cout << key.key_name;
    intbuf << clear << (FAU_t)key.right_child;
    cout << ", " << intbuf.c_str();
    cout << "> ";
    i++;
  }
  cout << "\n";
}

typedef void (*BtreeVisitFunc)(BtreeNode *Node); 

void BtreeWalk(FAU_t t, BtreeVisitFunc Visit, gxBtree *tree)
// This is a recursive function demo used to walk through
// the B-tree node by node.
{
  BtreeKeyCount_t i;

  // Ensure that the in memory buffers and the file data
  // stay in sync during multiple file access.
  if(tree) {
    tree->TestTree();
  }
  else {
    return;
  }

  BtreeNode n(tree->KeySize(), tree->NodeOrder());
  
  if(t != (FAU_t)0) {
    tree->ReadNode(n, t);
    n.node_address = t;
    (*Visit)(&n); // Process the node data
    BtreeKeyCount_t nc = n.key_count;
    FAU_t p;

    for(i = (BtreeKeyCount_t)-1; i < nc; i++) {
      p = n.GetBranch(i);
      BtreeWalk(p, Visit, tree);  
    }
  }
}

void PausePrg()
{
  cout << "\n";
  cout << "Press enter to continue..." << "\n";
  cin.get();
}

void BtreeStatus(gxBtree &btx)
{
  UString intbuf;
  cout << "\n";
  intbuf << clear << (FAU_t)btx.Root();
  cout << "Root address =      " << intbuf.c_str() << "\n";
  cout << "Number of trees =   " << btx.NumTrees() << "\n";
  cout << "Number of entries = " << btx.NumKeys() << "\n";
  cout << "Number of nodes =   " << btx.NumNodes() << "\n";
  cout << "B-Tree order =      " << btx.NodeOrder() << "\n";
  cout << "B-Tree height =     " << btx.BtreeHeight() << "\n";
  PausePrg();
}

void BuildTree(gxBtree &btx)
{
  // Set to true to print the tree with each insertion and deletion
  int print_tree = 0;

  char *aa1 = "DOG";
  char *bb1 = "CAT";
  char *cc1 = "FISH";
  char *dd1 = "MOUSE";
  char *ee1 = "BIRD";
  char *ff1 = "PIG";
  char *gg1 = "HORSE";
  char *hh1 = "LION";
  char *ii1 = "SNAKE";
  char *jj1 = "COW";
  char *kk1 = "ARMADILLO";
  char *ll1 = "GROUPER";
  char *mm1 = "RAT";
  char *nn1 = "MONKEY";
  char *oo1 = "ZEBRA";
  char *pp1 = "STARFISH";
  char *qq1 = "LIZARD";
  char *rr1 = "CRAB";
  char *ss1 = "SNAIL";
  char *tt1 = "GORILLA";
  char *uu1 = "LOBSTER";
  char *vv1 = "TURKEY";
  char *ww1 = "BEETLE";
  char *xx1 = "SHARK";
  char *yy1 = "CLAM";
  char *zz1 = "OYSTER";

  char *aa2 = "FLEA";
  char *bb2 = "BUTTERFLY";
  char *cc2 = "SPARROW";
  char *dd2 = "GOLDFISH";
  char *ee2 = "TIGER";
  char *ff2 = "BEAR";
  char *gg2 = "TROUTE";
  char *hh2 = "MOOSE";
  char *ii2 = "DEAR";
  char *jj2 = "SALMON";
  char *kk2 = "TUNA";
  char *ll2 = "GAZELLE";
  char *mm2 = "SLOTH";
  char *nn2 = "SPIDER";
  char *oo2 = "LEAPORD";
  char *pp2 = "GIRAFFE";
  char *qq2 = "MUSTANG";
  char *rr2 = "CONDOR";
  char *ss2 = "KANGAROO";
  char *tt2 = "SKUNK";
  char *uu2 = "FOX";
  char *vv2 = "PANTER";
  char *ww2 = "CHEETAH";
  char *xx2 = "TOUCAN";
  char *yy2 = "PARROT";
  char *zz2 = "BUFFALO";

  char *aa3 = "KOALA";
  char *bb3 = "HORSEFLY";
  char *cc3 = "ANACONDA";
  char *dd3 = "CROCODILE";
  char *ee3 = "RACCOON";
  char *ff3 = "ALLIGATOR";
  char *gg3 = "RABBIT";
  char *hh3 = "WHALE";
  char *ii3 = "ANT";
  char *jj3 = "CRANE";
  char *kk3 = "LONGHORN";
  char *ll3 = "CANARY";
  char *mm3 = "WOMBAT";
  char *nn3 = "WOLFHOUND";
  char *oo3 = "COUGAR";
  char *pp3 = "BAT";
  char *qq3 = "OWL";
  char *rr3 = "SHRIMP";
  char *ss3 = "SCALLOP";
  char *tt3 = "SQUID";
  char *uu3 = "PYTHON";
  char *vv3 = "SARDINE";
  char *ww3 = "TAPIR";
  char *xx3 = "ELEPHANT";
  char *yy3 = "EEL";
  char *zz3 = "RHINOCEROS";

  char *aa4 = "LAMB";
  char *bb4 = "BISON";
  char *cc4 = "GRASSHOPPER";
  char *dd4 = "MACKEREL";
  char *ee4 = "FERRET";
  char *ff4 = "WASP";
  char *gg4 = "CATERPILLAR";
  char *hh4 = "MILLIPEDE";
  char *ii4 = "CENTIPEDE";
  char *jj4 = "MOSQUITO";
  char *kk4 = "POSSUM";
  char *ll4 = "DUCK";
  char *mm4 = "WEASEL";
  char *nn4 = "CARIBOU";
  char *oo4 = "ANTELOPE";
  char *pp4 = "SALAMANDER";
  char *qq4 = "NEWT";
  char *rr4 = "CHICKEN";
  char *ss4 = "BULL";
  char *tt4 = "COBRA";
  char *uu4 = "CHIMPANZEE";
  char *vv4 = "URCHIN";
  char *ww4 = "CROW";
  char *xx4 = "WOLF";
  char *yy4 = "SPONGE";
  char *zz4 = "JELLYFISH";

  const int NKEYS = 104;
  char *keys[NKEYS] = { aa1, bb1, cc1, dd1, ee1, ff1, gg1, hh1, ii1, jj1,
			kk1, ll1, mm1, nn1, oo1, pp1, qq1, rr1, ss1, tt1,
                        uu1, vv1, ww1, xx1, yy1, zz1,

			aa2, bb2, cc2, dd2, ee2, ff2, gg2, hh2, ii2, jj2,
			kk2, ll2, mm2, nn2, oo2, pp2, qq2, rr2, ss2, tt2,
                        uu2, vv2, ww2, xx2, yy2, zz2,

			aa3, bb3, cc3, dd3, ee3, ff3, gg3, hh3, ii3, jj3,
			kk3, ll3, mm3, nn3, oo3, pp3, qq3, rr3, ss3, tt3,
                        uu3, vv3, ww3, xx3, yy3, zz3,

			aa4, bb4, cc4, dd4, ee4, ff4, gg4, hh4, ii4, jj4,
			kk4, ll4, mm4, nn4, oo4, pp4, qq4, rr4, ss4, tt4,
                        uu4, vv4, ww4, xx4, yy4, zz4 };

  MyKeyClass key;
  MyKeyClass compare_key;
  int i, rv;

  const int INSERTIONS = 104;

  cout << "Inserting " << INSERTIONS << " keys..." << "\n";
  for(i = 0; i < INSERTIONS; i++) {
    key = keys[i];  

    rv = btx.Insert(key, compare_key);
    
    if(print_tree) {
      cout << "Inserting " << keys[i] << " - " << i << "\n";
      BtreeWalk(btx.Root(), PrintNode, &btx);
      PausePrg();
    }

    if(rv != 1) {
      cout << "\n" << "Problem adding " << keys[i] << " - " << i << "\n";
      return;
    }
  }

  btx.Flush(); // Flush the tree following a batch insert
  BtreeStatus(btx);

  cout << "Verifying the insertions..." << "\n";
  for(i = 0; i < INSERTIONS; i++) {
    key = keys[i];      
    rv = btx.Find(key, compare_key);
    if(rv != 1) {
      cout << "Error finding key " << keys[i] << " - " << i << "\n";
      return;
    }
  }

  cout << "Deleting all the entries..." << "\n";
  for(i = 0; i < INSERTIONS; i++) {
    key = keys[i];      

    // Testing the deletion functions
    // rv = btx.LazyDelete(key, compare_key);
    rv = btx.Delete(key, compare_key);

    if(rv != 1) {
      cout << "Error deleting key " << keys[i] << " - " << i << "\n";
      return;
    }

    if(print_tree) {
      cout << "Deleting " << keys[i] << " - " << i << "\n";
      BtreeWalk(btx.Root(), PrintNode, &btx);
      PausePrg();
    }

    // Verify the remaining key locations
    for(int j = INSERTIONS-1; j != i; j--) {
      key = keys[j];      
      rv = btx.Find(key, compare_key);
      if(rv != 1) {
	cout << "Error finding key " << keys[j] << " - " << j << "\n";
	cout << "After deleting key " << keys[i] << " - " << i << "\n";
	return;
      }
    }
  }

  btx.Flush(); // Flush the tree following a batch delete
  BtreeStatus(btx);

  cout << "Re-inserting " << INSERTIONS << " keys..." << "\n";
  for(i = 0; i < INSERTIONS; i++) {
    key = keys[i];  

    rv = btx.Insert(key, compare_key);

    if(rv != 1) {
      cout << "\n" << "Problem adding " << keys[i] << " - " << i << "\n";
      return;
    }
  }

  btx.Flush(); // Flush the tree following a batch insert
  BtreeStatus(btx);
}

int main(int argv, char **argc)
{
  const char *fname = "testfile.btx"; // File name of this database
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

  MyKeyClass key, compare_key;
  gxBtree btx(key, MyKeyClassOrder);

  // Create a new B-Tree index file
  btx.Create(fname, rev_letter);
  if(CheckError(btx.gxDatabasePtr()) != 0) return 1;
  
  cout << "Database key size      = " << key.SizeOfDatabaseKey() << "\n";
  cout << "Key entry size         = "
       << (key.SizeOfDatabaseKey() * (MyKeyClassOrder-1)) << "\n";

  BtreeNode n(key.SizeOfDatabaseKey(), MyKeyClassOrder);
  cout << "B-Tree node size       = " << n.SizeOfBtreeNode() << "\n";
  cout << "B-Tree header size     = " << sizeof(gxBtreeHeader) << "\n";
  cout << "B-Tree order           = " << MyKeyClassOrder << "\n";
  cout << "Total B-Tree node size = " << btx.TotalNodeSize() << "\n";

  PausePrg();

  // Build the Btree
  BuildTree(btx);
  
  btx.FindFirst(key);
  cout << "First key = " << key.key_name << "\n";

  btx.FindLast(key);
  cout << "Last key = " << key.key_name << "\n";

  cout << "\n";
  
  cout << "Recursively walking through the B-Tree node by node..." << "\n";
  PausePrg();

  BtreeWalk(btx.Root(), PrintNode, &btx);

  cout << "\n";
  cout << "Walking through the tree in sort order" << "\n";
  PausePrg();
  
  // Walk through the tree starting at the first node
  if(btx.FindFirst(key)) {
    cout << key.key_name << ' ';
    while(btx.FindNext(key, compare_key))
      cout << key.key_name << ' ';
  }
  else {
    cout << "Could not find first key" << "\n";
    cout << btx.DatabaseExceptionMessage();
  }
  cout  << "\n";
  PausePrg();

  // Walk backward through the tree starting at the last node
  if(btx.FindLast(key)) {
    cout << key.key_name << ' ';
    while(btx.FindPrev(key, compare_key)) {
      cout << key.key_name << ' ';
    }
  }
  else {
    cout << "Could not find last key" << "\n";
    cout << btx.DatabaseExceptionMessage();
  }
  cout << "\n";

  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
