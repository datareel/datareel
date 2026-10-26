// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: testprog.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 08/09/2001
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

Program used to test the gxBtree cached insert, find, and delete 
functions.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if !defined (__USE_BTREE_CACHE__)
int main() { return 0; }
#else

#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library

#include "gxdstats.h"
#include "gxbtree.h"
#include <string>

const BtreeNodeOrder_t MyKeyClassOrder = 5;
const __WORD__ MyKeyNameSize = 64;
const int num_buckets = 3;

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

void PrintNode(BtreeNode *n)
// Prints a single btree node.
{
  MyKeyClass key;
  std::string intbuf;
  BtreeKeyCount_t i = (BtreeKeyCount_t)0;
  cout << "[";
  intbuf = std::to_string((long long)((FAU_t)n->node_address));
  cout << "Node Address: " << intbuf.c_str(); 
  cout << ", ";
  intbuf = std::to_string((long long)((FAU_t)n->left_child));
  cout << "Left Node: " << intbuf.c_str();
  cout << "]";
  while (i < n->key_count) {
    n->LoadKey(key, (BtreeKeyLocation_t)i);
    cout << " <";
    cout << key.key_name;
    intbuf = std::to_string((long long)((FAU_t)key.right_child));
    cout << ", " << intbuf.c_str();
    cout << "> ";
    i++;
  }
  cout << "\n";
}

typedef void (*BtreeVisitFunc)(BtreeNode *Node); 

void BtreeWalk(FAU_t t, BtreeVisitFunc Visit, gxBtree *tree)
// This is a recursive function demo used to walk through
// the btree node by node.
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
  std::string intbuf;
  cout << "\n";
  intbuf = std::to_string((long long)(btx.Root()));
  cout << "Root address =      " << intbuf.c_str() << "\n";
  cout << "Number of trees =   " << btx.NumTrees() << "\n";
  cout << "Number of entries = " << btx.NumKeys() << "\n";
  cout << "Number of nodes =   " << btx.NumNodes() << "\n";
  cout << "B-Treeorder =       " << btx.NodeOrder() << "\n";
  cout << "B-Treeheight =      " << btx.BtreeHeight() << "\n";
  PausePrg();
}

void BtreeCacheStats(BtreeCache &btree_cache)
{
  cout << "\n";
  cout << "--------- Cache tuning statistics ---------" << "\n";
  cout << "Number of dirty buckets = " 
       << btree_cache.BucketsInUse() << "\n";
  cout << "Number of disk reads    = " 
       << btree_cache.uncached_reads << "\n";
  cout << "Number of cached reads  = " 
       << btree_cache.cached_reads << "\n";
  PausePrg();
}

int BuildTree(gxBtree &btx)
{
  // Set to true to print the tree with each insertion and deletion
  int print_tree = 0;

  const char *aa1 = "DOG";
  const char *bb1 = "CAT";
  const char *cc1 = "FISH";
  const char *dd1 = "MOUSE";
  const char *ee1 = "BIRD";
  const char *ff1 = "PIG";
  const char *gg1 = "HORSE";
  const char *hh1 = "LION";
  const char *ii1 = "SNAKE";
  const char *jj1 = "COW";
  const char *kk1 = "ARMADILLO";
  const char *ll1 = "GROUPER";
  const char *mm1 = "RAT";
  const char *nn1 = "MONKEY";
  const char *oo1 = "ZEBRA";
  const char *pp1 = "STARFISH";
  const char *qq1 = "LIZARD";
  const char *rr1 = "CRAB";
  const char *ss1 = "SNAIL";
  const char *tt1 = "GORILLA";
  const char *uu1 = "LOBSTER";
  const char *vv1 = "TURKEY";
  const char *ww1 = "BEETLE";
  const char *xx1 = "SHARK";
  const char *yy1 = "CLAM";
  const char *zz1 = "OYSTER";

  const char *aa2 = "FLEA";
  const char *bb2 = "BUTTERFLY";
  const char *cc2 = "SPARROW";
  const char *dd2 = "GOLDFISH";
  const char *ee2 = "TIGER";
  const char *ff2 = "BEAR";
  const char *gg2 = "TROUTE";
  const char *hh2 = "MOOSE";
  const char *ii2 = "DEAR";
  const char *jj2 = "SALMON";
  const char *kk2 = "TUNA";
  const char *ll2 = "GAZELLE";
  const char *mm2 = "SLOTH";
  const char *nn2 = "SPIDER";
  const char *oo2 = "LEAPORD";
  const char *pp2 = "GIRAFFE";
  const char *qq2 = "MUSTANG";
  const char *rr2 = "CONDOR";
  const char *ss2 = "KANGAROO";
  const char *tt2 = "SKUNK";
  const char *uu2 = "FOX";
  const char *vv2 = "PANTER";
  const char *ww2 = "CHEETAH";
  const char *xx2 = "TOUCAN";
  const char *yy2 = "PARROT";
  const char *zz2 = "BUFFALO";

  const char *aa3 = "KOALA";
  const char *bb3 = "HORSEFLY";
  const char *cc3 = "ANACONDA";
  const char *dd3 = "CROCODILE";
  const char *ee3 = "RACCOON";
  const char *ff3 = "ALLIGATOR";
  const char *gg3 = "RABBIT";
  const char *hh3 = "WHALE";
  const char *ii3 = "ANT";
  const char *jj3 = "CRANE";
  const char *kk3 = "LONGHORN";
  const char *ll3 = "CANARY";
  const char *mm3 = "WOMBAT";
  const char *nn3 = "WOLFHOUND";
  const char *oo3 = "COUGAR";
  const char *pp3 = "BAT";
  const char *qq3 = "OWL";
  const char *rr3 = "SHRIMP";
  const char *ss3 = "SCALLOP";
  const char *tt3 = "SQUID";
  const char *uu3 = "PYTHON";
  const char *vv3 = "SARDINE";
  const char *ww3 = "TAPIR";
  const char *xx3 = "ELEPHANT";
  const char *yy3 = "EEL";
  const char *zz3 = "RHINOCEROS";

  const char *aa4 = "LAMB";
  const char *bb4 = "BISON";
  const char *cc4 = "GRASSHOPPER";
  const char *dd4 = "MACKEREL";
  const char *ee4 = "FERRET";
  const char *ff4 = "WASP";
  const char *gg4 = "CATERPILLAR";
  const char *hh4 = "MILLIPEDE";
  const char *ii4 = "CENTIPEDE";
  const char *jj4 = "MOSQUITO";
  const char *kk4 = "POSSUM";
  const char *ll4 = "DUCK";
  const char *mm4 = "WEASEL";
  const char *nn4 = "CARIBOU";
  const char *oo4 = "ANTELOPE";
  const char *pp4 = "SALAMANDER";
  const char *qq4 = "NEWT";
  const char *rr4 = "CHICKEN";
  const char *ss4 = "BULL";
  const char *tt4 = "COBRA";
  const char *uu4 = "CHIMPANZEE";
  const char *vv4 = "URCHIN";
  const char *ww4 = "CROW";
  const char *xx4 = "WOLF";
  const char *yy4 = "SPONGE";
  const char *zz4 = "JELLYFISH";

  const int NKEYS = 104;
  const char *keys[NKEYS] = { aa1, bb1, cc1, dd1, ee1, ff1, gg1, hh1, ii1, jj1,
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

  cout << "Constructing a B-tree cache" << "\n";
  BtreeCache btree_cache(num_buckets, &btx);
  if(!btree_cache) {
    cout << "Error constructing the B-tree cache object" << "\n";
    return 0;
  }
  cout << "Cache size = " << btree_cache.CacheSize() << " bytes" << "\n"; 

  const int INSERTIONS = 104;

  cout << "Inserting " << INSERTIONS << " keys..." << "\n";
  for(i = 0; i < INSERTIONS; i++) {
    key = keys[i];  

    // Cached insertion method
    rv = btx.Insert(key, compare_key, &btree_cache);

    if(print_tree) {
      cout << "Inserting " << keys[i] << " - " << i << "\n";
      BtreeWalk(btx.Root(), PrintNode, &btx);
      PausePrg();
    }

    if(rv != 1) {
      cout << "\n" << "Problem adding " << keys[i] << " - " << i << "\n";
      cout << btx.DatabaseExceptionMessage() << "\n";
      btree_cache.Flush();
      btx.Flush();
      return 0;
    }
  }

  BtreeCacheStats(btree_cache);

  cout << "Verifying the insertions..." << "\n";
  for(i = 0; i < INSERTIONS; i++) {
    key = keys[i];      

    rv = btx.Find(key, compare_key, &btree_cache);

    if(rv != 1) {
      cout << "Error finding key " << keys[i] << " - " << i << "\n";
      cout << btx.DatabaseExceptionMessage() << "\n";
      btree_cache.Flush();
      btx.Flush();
      return 0;
    }
  }

  BtreeCacheStats(btree_cache);

  cout << "Cache buckets in use = " << btree_cache.BucketsInUse() << "\n";
  cout << "Flushing the B-tree cache" << "\n";

  if(btree_cache.Flush() != gxDBASE_NO_ERROR) {
    cout << "Error flushing cache" << "\n";
    return 0;
  }

  cout << "Deleting all the entries..." << "\n";
  for(i = 0; i < INSERTIONS; i++) {
    key = keys[i];      

    // Testing the deletion functions
    // rv = btx.FastDelete(key, compare_key, &btree_cache);

    // Testing the balanced delete function
    rv = btx.Delete(key, compare_key, &btree_cache);

    if(rv != 1) {
      cout << "Error deleting key " << keys[i] << " - " << i << "\n";
      cout << btx.DatabaseExceptionMessage() << "\n";
      btree_cache.Flush();
      btx.Flush();
      return 0;
    }

    if(print_tree) {
      cout << "Deleting " << keys[i] << " - " << i << "\n";
      BtreeWalk(btx.Root(), PrintNode, &btx);
      PausePrg();
    }

    for(int j = INSERTIONS-1; j != i; j--) {
      key = keys[j];      

      // Optimized sort order search
      rv = btx.Find(key, compare_key, &btree_cache);

      if(rv != 1) {
	cout << "Error finding key " << keys[j] << " - " << j << "\n";
	cout << "After deleting key " << keys[i] << " - " << i << "\n";
	cout << btx.DatabaseExceptionMessage() << "\n";
	btree_cache.Flush();
	btx.Flush();
	return 0;
      }
    }
  }
  
  BtreeStatus(btx);

  cout << "Re-inserting " << INSERTIONS << " keys..." << "\n";

  for(i = 0; i < INSERTIONS; i++) {
    key = keys[i];  

    rv = btx.Insert(key, compare_key, &btree_cache);

    if(rv != 1) {
      cout << "\n" << "Problem adding " << keys[i] << " - " << i << "\n";
      cout << btx.DatabaseExceptionMessage() << "\n";
      btree_cache.Flush();
      btx.Flush();
      return 0;
    }
  }

  cout << "Cache buckets in use = " << btree_cache.BucketsInUse() << "\n";
  cout << "Flushing the B-tree cache" << "\n";
  if(btree_cache.Flush() != gxDBASE_NO_ERROR) {
    cout << "Error flushing cache" << "\n";
    return 0;
  }
  cout << "Flushing the B-tree header" << "\n";
  btx.Flush();

  BtreeStatus(btx);
  return 1;
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

  // Create a new B-Tree index file with one tree
  btx.Create(fname, rev_letter);
  if(CheckError(btx.gxDatabasePtr()) != 0) return 1;
  
  cout << "--------- Key and B-Tree Node Statistics ---------" << "\n";
  cout << "Key name size        = " << key.KeySize() << "\n";
  cout << "Database key size    = " << key.SizeOfDatabaseKey() << "\n";
  cout << "Key entry size       = "
       << (key.SizeOfDatabaseKey() * (MyKeyClassOrder-1)) << "\n";
  BtreeNode n(key.SizeOfDatabaseKey(), MyKeyClassOrder);
  cout << "B-Tree order         = " << n.node_order << "\n";
  cout << "B-Tree node size     = " << n.SizeOfBtreeNode() << "\n";
  cout << "Size of BtreeBucket  = " << sizeof(BtreeBucket) << "\n";
  cout << "Size of Cache Bucket = " 
       <<  sizeof(BtreeBucket) + \
    (key.SizeOfDatabaseKey() * (MyKeyClassOrder-1)) << "\n";
  
  PausePrg();

  // Build the B-tree index file
  if(!BuildTree(btx)) return 1;
  
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
  
  // Node used for optimized sort order searches
  BtreeCache btree_cache(num_buckets, &btx);
  if(!btree_cache) {
    cout << "Error constructing the B-tree cache object" << "\n";
    return 1;
  }

  unsigned count = 0;

  // Walk through the B-tree starting at the first node.
  if(btx.FindFirst(key, &btree_cache)) {
    cout << key.key_name << ' ';
    count++;
    while(btx.FindNext(key, compare_key, &btree_cache)) {
      cout << key.key_name << ' ';
      count++;
    }
  }
  cout << "\n";
  cout << "Traversed " << count << " keys" << "\n";
  BtreeCacheStats(btree_cache);

  count = 0;

  // Walk through the B-tree starting at the last node.
  if(btx.FindLast(key, &btree_cache)) {
    cout << key.key_name << ' ';
    count++;
    while(btx.FindPrev(key, compare_key, &btree_cache)) {
      cout << key.key_name << ' ';
      count++;
    }
  }
  cout << "\n";
  cout << "Traversed " << count << " keys" << "\n";
  BtreeCacheStats(btree_cache);

  return 0;
}

#endif // __USE_BTREE_CACHE__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
