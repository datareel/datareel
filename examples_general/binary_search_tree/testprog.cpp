// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: testprog.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/07/1996
// Date Last Modified: 06/17/2016
// Copyright (c) 2001-2016 DataReel Software Development
// ----------------------------------------------------------- // 
// ------------- Program description and details ------------- // 
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

Test program for the generic binary search tree class.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include <stdio.h>
#include <time.h>
#include "gxbstree.h"
#include "bstreei.h"
#include "ustring.h"
#include "dfileb.h"

#ifdef __MSVC_DEBUG__
#include "leaktest.h"
#endif

void InputData(UString &sbuf)
// Input a line of text from the console
{
  char buf[255];
  cout << "Input Data: ";
  cin.getline(buf, sizeof(buf));
  sbuf = buf;
}

void ClearInputStream(istream &s)
// Used to clear istream
{
  char c;
  s.clear();
  while(s.get(c) && c != '\n') { ; }
}

int Quit()
// Terminate the program
{
  cout << "Exiting..." << "\n";
  return 0;
}

void PausePrg()
// Pause the program 
{
  cout << "\n";
  cout << "Press enter to continue..." << "\n";
  cin.get();
}

void Menu(void)
// List the program options
{
  cout << "(A, a)    Add an item to the tree" << "\n";
  cout << "(B, b)    Build a tree of strings" << "\n";
  cout << "(C, c)    Clear the tree" << "\n";
  cout << "(D, d)    Delete an item from the tree" << "\n";
  cout << "(E, e)    Extract all the node in sort order" << "\n";
  cout << "(F, f)    Find an item in the tree" << "\n";
  cout << "(H, h, ?) Help (prints this menu)" << "\n"; 
  cout << "(I, i)    Import a dictionary file" << "\n"; 
  cout << "(L, l)    List tree items in order" << "\n";
  cout << "(Q, q)    Quit this program" << "\n";
  cout << "(S, s)    Display the tree statistics" << "\n";
  cout << "(R, r)    Extract all the node in reverse order" << "\n";
  cout << "(T, t)    Test tree insertion/deletion times" << "\n";
}

unsigned TreeHeight(gxBStreeNode_t *node)
// Recursive function used to calculate the tree height based
// on the maximum of the height of right and left subtrees.
{
  unsigned height = 0;
  if(node != 0) {
    unsigned left_height = TreeHeight(node->left);
    unsigned right_height = TreeHeight(node->right);
    if(left_height > right_height) {
      height = left_height+1;
    }
    else {
      height = right_height+1;
    }
  }
  return height;
}

void NumNodes(gxBStreeNode_t *node, unsigned &num_nodes)
// Recursive function used to calculate the total number of nodes.
{
  while(node) {
    NumNodes(node->left, num_nodes);
    num_nodes++;
    node = node->right;
  }
}

void TreeStats(gxBStree<UString> &tree)
{
  cout << "\n";
  cout << "----- Binary search tree statistics -----" << "\n";
  cout << "\n";
  if(tree.IsEmpty()) {
    cout << "The tree is empty" << "\n";
    return;
  }
  unsigned num_nodes = 0;
  unsigned height = 0;

  NumNodes(tree.GetRoot(), num_nodes);
  height = TreeHeight(tree.GetRoot());
  
  cout << "Number of nodes =   " << num_nodes << "\n";
  cout << "Height          =   " << height << "\n";

  cout << "\n";
  UString root, first, last;
  
  cout << "Root node = " << tree.GetRoot()->data << "\n";
  if(tree.FindFirst(first))
    cout << "First key = " << first << "\n";

  if(tree.FindLast(last))
    cout << "Last key  = " << last << "\n";
  cout << "\n";
}

void ImportDictionaryFile(gxBStree<UString> &tree)
// Import a list of words in sort order. This function demonstrates
// how unbalanced trees are degenerated when a sort ordered list of
// items are inserted into the tree.
{
  cout << "\n";
  cout << "Importing a dictionary file." << "\n";

  tree.Clear(); // Clear the tree before importing the file
  
  char LineBuffer[df_MAX_LINE_LENGTH];
  int status, key_count = 0;
  UString key;
  UString FileName;
  
  cout << "\n";
  cout << "Enter the name of the text file to import" << "\n";
  InputData(FileName);

  DiskFileB infile(FileName.c_str());
  if(!infile) { // Could not open the istream
    cout << "Could not open file: " << FileName << "\n";
    return;
  } 
  
  cout << "Adding words..." << "\n";

  // Get CPU clock cycles before entering loop
  clock_t begin = clock();

  while(!infile.df_EOF()) {
    if(infile.df_GetLine(LineBuffer, df_MAX_LINE_LENGTH, '\n', 1) != 
       DiskFileB::df_NO_ERROR) {
      break; // Error reading from the disk file
    }
    if(strcmp(LineBuffer, "") == 0) continue;
    key = LineBuffer;
    cout << key.c_str() << "\n";
    status = tree.Insert(key);
    if(status != 1) {
      cout << "\n" << "Problem adding " << key.c_str() << "\n";
      return;
    }
    else {
      key_count++;
    }
  }
  
  // Get CPU clock cycles after loop is completed 
  clock_t end =clock();
  
  // Calculate the elapsed time in seconds. 
  double elapsed_time = (double)(end - begin) / CLOCKS_PER_SEC;
  cout.precision(3); 
  cout << "Added " << key_count << " words in " 
       << elapsed_time << " seconds" << "\n";

  // Rewind the file
  infile.df_Rewind();

  cout << "Verifying the entries..." << "\n";
  begin = clock();
  key_count = 0;
  double search_time = 0;

  while(!infile.df_EOF()) {
    if(infile.df_GetLine(LineBuffer, df_MAX_LINE_LENGTH, '\n', 1) != 
       DiskFileB::df_NO_ERROR) {
      break; // Error reading from the disk file
    }
    if(strcmp(LineBuffer, "") == 0) continue;

    key = LineBuffer;
    clock_t begin_search = clock();
    status = tree.Find(key);
    clock_t end_search = clock();
    search_time += (double)(end_search - begin_search) / CLOCKS_PER_SEC;

    if (status != 1) {
      cout << "\n" << "Problem finding " << key << "\n";
      return;
    }
    else {
      key_count++;
    }
  }

  end =clock();
  elapsed_time = (double)(end - begin) / CLOCKS_PER_SEC;
  cout.precision(3);
  cout << "Verified " << key_count << " words in " 
       << elapsed_time << " seconds" << "\n";
  double avg_search_time = (search_time/(double)key_count) * 1000;
  cout << "Average search time = " << avg_search_time << " milliseconds"
       << "\n";
  PausePrg();

  // Rewind the file
  infile.df_Rewind();

  cout << "Deleting the entries..." << "\n";
  begin = clock();
  key_count = 0;
  while(!infile.df_EOF()) {
    if(infile.df_GetLine(LineBuffer, df_MAX_LINE_LENGTH, '\n', 1) != 
       DiskFileB::df_NO_ERROR) {
      break; // Error reading from the disk file
    }
    if(strcmp(LineBuffer, "") == 0) continue;

    key = LineBuffer;
    status = tree.Delete(key);

    if (status != 1) {
      cout << "\n" << "Problem deleting " << key << "\n";
      return;
    }
    else {
      key_count++;
    }
  }

  end =clock();
  cout.precision(3);
  elapsed_time = (double)(end - begin) / CLOCKS_PER_SEC;
  cout << "Deleted " << key_count << " words in " 
       << elapsed_time << " seconds" << "\n";

  // Rewind the file
  infile.df_Rewind();

  cout << "Re-inserting all the words..." << "\n";
  key_count = 0;
  begin = clock();

  while(!infile.df_EOF()) {
    if(infile.df_GetLine(LineBuffer, df_MAX_LINE_LENGTH, '\n', 1) != 
       DiskFileB::df_NO_ERROR) {
      break; // Error reading from the disk file
    }
    if(strcmp(LineBuffer, "") == 0) continue;

    key = LineBuffer;
    status = tree.Insert(key);
    
    if (status != 1) {
      cout << "\n" << "Problem adding " << key << "\n";
      return;
    }
    else {
      key_count++;
    }
  }
  
  end =clock();
  cout.precision(3);
  elapsed_time = (double)(end - begin) / CLOCKS_PER_SEC;
  cout << "Added " << key_count << " words in " 
       << elapsed_time << " seconds" << "\n";

  infile.df_Close();
  return;
}

void BuildTree(gxBStree<UString> &tree)
// Build a tree of strings.
{
  char *aa1 = "dog";
  char *bb1 = "cat";
  char *cc1 = "fish";
  char *dd1 = "mouse";
  char *ee1 = "bird";
  char *ff1 = "pig";
  char *gg1 = "horse";
  char *hh1 = "lion";
  char *ii1 = "snake";
  char *jj1 = "cow";
  char *kk1 = "armadillo";
  char *ll1 = "grouper";
  char *mm1 = "rat";
  char *nn1 = "monkey";
  char *oo1 = "zebra";
  char *pp1 = "starfish";
  char *qq1 = "lizard";
  char *rr1 = "crab";
  char *ss1 = "snail";
  char *tt1 = "gorilla";
  char *uu1 = "lobster";
  char *vv1 = "turkey";
  char *ww1 = "beetle";
  char *xx1 = "shark";
  char *yy1 = "clam";
  char *zz1 = "oyster";

  char *keys[26] = { aa1, bb1, cc1, dd1, ee1, ff1, gg1, hh1, ii1, jj1,
		     kk1, ll1, mm1, nn1, oo1, pp1, qq1, rr1, ss1, tt1,
		     uu1, vv1, ww1, xx1, yy1, zz1 };
  
  const int INSERTIONS = 26; // Number of keys to insert
  UString key;
  int i, status;

  tree.Clear();
  
  cout << "Inserting " << INSERTIONS << " keys..." << "\n";
  for(i = 0; i < INSERTIONS; i++) {
    key = keys[i];  

    status = tree.Insert(key);
    
    if(status != 1) {
      cout << "\n" << "Problem adding " << keys[i] << " - " << i << "\n";
      return;
    }
  }
  TreeStats(tree);
}

void TestTree(gxBStree<UString> &tree)
// Test the tree insertion and deletion times.
{
  // Adjust this number to set the number of insertions
  // const unsigned long INSERTIONS = 1 * 1000;       // 1K test
  // const unsigned long INSERTIONS = 10 * 1000;      // 10K test
  const unsigned long INSERTIONS = 100 * 1000;     // 100K test
  // const unsigned long INSERTIONS = 1000 * 1000;    // 1MEG test
  // const unsigned long INSERTIONS = 10000 * 1000;   // 10MEG test
  // const unsigned long INSERTIONS = 100000 * 1000;  // 100MEG test
  // const unsigned long INSERTIONS = 1000000 * 1000; // 1GIG test

  cout << "Inserting " << INSERTIONS << " keys..." << "\n";
  unsigned long i, key_count = 0;
  unsigned long curr_count = 0;
  int status;
  int verify_deletions = 0; // Set to ture to verify all deletions
  double insert_time = 0;
  const char *name = "Item";
  char sbuf[255];
  UString key;
  
  // Get CPU clock cycles before entering loop
  clock_t begin = clock();

  for(i = 0; i < INSERTIONS; i++) {
    sprintf(sbuf, "%s %d", name, (int)i);
    key = sbuf;
    clock_t begin_insert = clock();
    status = tree.Insert(key);
    clock_t end_insert = clock();
    insert_time += (double)(end_insert - begin_insert) / CLOCKS_PER_SEC;
    key_count++;
    curr_count++;

    if(status != 1) {
      cout << "\n" << "Problem adding key - " << key << "\n";
      return;
    }
    if(curr_count == 10000) {
      curr_count = 0;
      cout << "Inserted " << i << " keys in " << insert_time
	   << " seconds" << "\n";
    }

  }

  // Get CPU clock cycles after loop is completed 
  clock_t end =clock();

  // Calculate the elapsed time in seconds. 
  double elapsed_time = (double)(end - begin) / CLOCKS_PER_SEC;
  cout.precision(3); 
  cout << "Inserted " << key_count << " values in " 
       << elapsed_time << " seconds" << "\n";
  double avg_insert_time = (insert_time/(double)key_count) * 1000;
  cout << "Average insert time = " << avg_insert_time << " milliseconds"  
       << "\n"; 

  key_count = 0;
  double search_time = 0;
  cout << "Verifying the insertions..." << "\n";
  begin = clock();
  for(i = 0; i < INSERTIONS; i++) {
    sprintf(sbuf, "%s %d", name, (int)i);
    key = sbuf;
    clock_t begin_search = clock();
    status = tree.Find(key);
    clock_t end_search = clock();
    key_count++;
    search_time += (double)(end_search - begin_search) / CLOCKS_PER_SEC;
    
    if(status != 1) {
      cout << "Error finding key - " << key << "\n";
      return;
    }
  }

  end =clock();
  elapsed_time = (double)(end - begin) / CLOCKS_PER_SEC;
  cout.precision(3);
  cout << "Verified " << key_count << " values in " 
       << elapsed_time << " seconds" << "\n";
  double avg_search_time = (search_time/(double)key_count) * 1000;
  cout << "Average search time = " << avg_search_time << " milliseconds"
       << "\n";

  cout << "Deleting all the entries..." << "\n";
  key_count = 0;
  double delete_time = 0;
  begin = clock();
  for(i = 0; i < INSERTIONS; i++) {
    sprintf(sbuf, "%s %d", name, (int)i);
    key = sbuf;
    clock_t begin_delete = clock();
    status = tree.Delete(key);
    clock_t end_delete = clock();
    delete_time += (double)(end_delete - begin_delete) / CLOCKS_PER_SEC;
    key_count++;
    if(status != 1) {
      cout << "Error deleting key - " << key << "\n";
      return;
    }

    if(verify_deletions) { // Verify the remaining key locations
      for(unsigned long j = INSERTIONS-1; j != i; j--) {
	sprintf(sbuf, "%s %d", name, (int)j);
	key = sbuf;
	status = tree.Find(key);
	if(status != 1) {
	  cout << "Error finding key  - " << j << "\n";
	  cout << "After deleting key - " << i << "\n";
	  return;
	}
      }
    }
  }

  end =clock();
  cout.precision(3);
  elapsed_time = (double)(end - begin) / CLOCKS_PER_SEC;
  cout << "Deleted " << key_count << " values in " 
       << elapsed_time << " seconds" << "\n";
  double avg_delete_time = (delete_time/(double)key_count) * 1000;
  cout << "Average delete time = " << avg_delete_time << " milliseconds"
       << "\n";

  cout << "Re-inserting " << INSERTIONS << " keys..." << "\n";
  key_count = 0;
  insert_time = 0;
  begin = clock();
  for(i = 0; i < INSERTIONS; i++) {
    sprintf(sbuf, "%s %d", name, (int)i);
    key = sbuf;
    clock_t begin_insert = clock();
    status = tree.Insert(key);
    clock_t end_insert = clock();
    insert_time += (double)(end_insert - begin_insert) / CLOCKS_PER_SEC;
    key_count++;
    curr_count++;

    if(status != 1) {
      cout << "\n" << "Problem adding key - " << key << "\n";
      return;
    }
  }
  end =clock();
  elapsed_time = (double)(end - begin) / CLOCKS_PER_SEC;
  cout.precision(3); 
  cout << "Inserted " << key_count << " values in " 
       << elapsed_time << " seconds" << "\n";
  avg_insert_time = (insert_time/(double)key_count) * 1000;
  cout << "Average insert time = " << avg_insert_time << " milliseconds"  
       << "\n"; 
}

void ListInOrder(gxBStree<UString> &tree)
// List all the tree nodes using a tree iterator object.
{
  gxBStreeNode<UString> *node = tree.GetRoot();
  gxBStreeIterator tree_iterator(node);
  while((node = (gxBStreeNode<UString> *)tree_iterator.GetNext()) != 0) {
    cout << node->data << "\n";
  }
  cout << "\n";
}

void ExtractInOrder(gxBStree<UString> &tree)
// Extract all the tree nodes in sort order.
{
  UString key;
  while(!tree.IsEmpty()) {
    tree.ExtractFirst(key);
    cout << key << "\n";
  }
}

void ExtractInReverseOrder(gxBStree<UString> &tree)
// Extract all the tree nodes in reverse order.
{
  UString key;
  while(!tree.IsEmpty()) {
    tree.ExtractLast(key);
    cout << key << "\n";
  }
}

int main()
// Test program's main thread of execution
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

  Menu(); // Display the options menu
  
  char key;
  UString key_buf;
  gxBStree<UString> tree;
  int exists;
  int rv = 1;

  while(rv) {
    if (!cin) { 
       ClearInputStream(cin); 
       if (!cin) { 
          cout << "Input stream error" << "\n";
          return 0;
       }
    }
    cout << '>';
    cin >> key;
    if (!cin) continue;
    switch(key) {
      case 'a' : case 'A' :
	ClearInputStream(cin);
	InputData(key_buf);
	tree.Insert(key_buf, &exists);
	if(exists) {
	  cout << "An entry for " << key_buf << " already exists" << "\n";
	}
	break;

      case 'b' : case 'B' :
	ClearInputStream(cin);
	BuildTree(tree);
	break;

      case 'c' : case 'C' :
	ClearInputStream(cin);
	tree.Clear();
	break;

      case 'd' : case 'D' :
	ClearInputStream(cin);
	InputData(key_buf);
	if(tree.Delete(key_buf)) {
	  cout << "Removed " << key_buf << "\n";
	}
	else {
	  cout << key_buf << " does not exists in the tree" << "\n";
	}
	break;

      case 'e' : case 'E' :
	ClearInputStream(cin);
	ExtractInOrder(tree);
	break;

      case 'r' : case 'R' :
	ClearInputStream(cin);
	ExtractInReverseOrder(tree);
	break;

      case 'f' : case 'F' :
	ClearInputStream(cin);
	InputData(key_buf);
	if(tree.Find(key_buf)) {
	  cout << "Found entry " << key_buf << "\n";
	}
	else {
	  cout << key_buf << " does not exists in the tree" << "\n";
	}
	break;	
      case 'l' : case 'L' : 
	ClearInputStream(cin);
	ListInOrder(tree);
	break;	 

      case 's' : case 'S' :
	ClearInputStream(cin);
	TreeStats(tree);
	break;

      case 'i' : case 'I' :
	ClearInputStream(cin);
	ImportDictionaryFile(tree);
	break;

      case 'q' : case 'Q' :
	tree.Clear();
	rv = Quit();
	break;

      case 't' : case 'T' :
	ClearInputStream(cin);
	TestTree(tree);
	break;

      case '?' : case 'h' : case 'H' :
	ClearInputStream(cin);
	Menu();
	break;

      default:
        cout << "Unrecognized command" << "\n";
    }
  }
  
  return 0;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
