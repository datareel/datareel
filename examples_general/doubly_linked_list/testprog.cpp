// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: testprog.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 04/05/1996
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

Doubly linked list class test program.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include <string.h>
#include "gxlist.h"

#ifdef __MSVC_DEBUG__
#include "leaktest.h"
#endif

void PausePrg()
{
  // Pause the program until enter is pressed
  cout << "\n";
  cout << "Press Enter to continue..." << "\n";
  cin.get();
}

void ScrollForward(gxList<char> &list)
{
  cout << "Walking through the list..." << "\n";
  gxListNode<char> *ptr = list.GetHead();
  while(ptr) {
    cout << ptr->data << ' ';
    ptr = ptr->next;
  } 
  cout << "\n";
}

void RewindList(gxList<char> &list)
{
  cout << "Walking through the list in a backward direction..." << "\n";
  gxListNode<char> *ptr = list.GetTail();

  // Walk through the list in a backward direction
  while(ptr) {
    cout << ptr->data << ' ';
    ptr = ptr->prev;
  } 

  cout << "\n";
}

int main()
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

  gxList<char> list;
  gxListNode<char> *ptr;

  // Construct some list nodes
  cout << "Loading data elements into linked list..." << "\n";
  gxListNode<char> *na = new gxListNode<char>;
  gxListNode<char> *nb = new gxListNode<char>;
  gxListNode<char> *nc = new gxListNode<char>;
  gxListNode<char> *nd = new gxListNode<char>;
  gxListNode<char> *n1 = new gxListNode<char>;
  gxListNode<char> *n2 = new gxListNode<char>;
  gxListNode<char> *n3 = new gxListNode<char>;
  gxListNode<char> *n4 = new gxListNode<char>;

  // Statically allocated list data
  const unsigned num_blocks = 9;
  char buf[num_blocks];
  unsigned i;

  // Load some values into the buffer starting with ASCII character 'A'
  for(i = 0; i < num_blocks; i++) buf[i] = (char)(65+i); 
  
  na->data = buf[0];
  nb->data = buf[1];
  nc->data = buf[2];
  nd->data = buf[3];

  // Load some list elements
  list.InsertAfter(list.GetHead(), na);
  list.InsertAfter(na, nb);
  list.InsertAfter(nb, nc);
  list.InsertAfter(nc, nd);
  
  n1->data = buf[4];
  n2->data = buf[5];
  n3->data = buf[6];
  n4->data = buf[7];
  
  // Testing insert functions
  list.InsertBefore(na, n1);
  list.InsertBefore(nc, n2);
  list.InsertAfter(nb, n3);
  list.InsertAfter(nd, n4);

  // Reordering the list
  list.MoveToBack(na);
  list.MoveAfter(na, nc);
  list.MoveBefore(nc, nb);
  list.MoveAfter(nc, nd);
  list.MoveAfter(nd, n2);
  list.MoveBefore(n2, n1);
  list.MoveAfter(n2, n3);
  list.MoveAfter(n3, n4);
  
  ScrollForward(list);
  RewindList(list);

  PausePrg();

  cout << "Adding another item to the list..." << "\n";
  ptr = list.Add(buf[8]);
  ScrollForward(list);

  PausePrg();

  cout << "Moving list item to the front of the list" << "\n";
  list.MoveToFront(ptr);
  ScrollForward(list);

  cout << "Moving item to the back of the list..." << "\n";
  list.MoveToBack(ptr);
  ScrollForward(list);
  
  PausePrg();
  
  cout << "Removing the head of the list..." << "\n";
  ptr = list.GetHead();
  list.Remove(ptr);
  ScrollForward(list);

  cout << "Removing the tail of the list..." << "\n";
  ptr = list.GetTail();
  list.Remove(ptr);
  ScrollForward(list);

  // NOTE: A gxList::ClearList() call is used here to free the memory
  // allocated for the node since the node data was statically allocated.
  list.ClearList();
  
  PausePrg();

  cout << "Testing for memory leaks..." << "\n";
  gxList<char *> dlist;
  const char *test_str = "The quick brown fox jumps over the lazy dog"; 
  unsigned len = strlen(test_str);
  const unsigned num_to_allocate = 10 * 100;
  const unsigned mem_bufs = \
    (sizeof(gxListNode<char *>) + len) * num_to_allocate;
  cout << "Allocating " << mem_bufs << " bytes..." << "\n";
  
  unsigned count1 = 0;
  unsigned count2 = 0;
  for(i = 0; i < num_to_allocate; i++) {
    char *sbuf = new char[len+1]; // Allocate memory for the string 
    count1 += sizeof(sbuf);
    if(!sbuf) {
      cout << "Memory allocation error at " << (count1+count2) << " bytes"
	   << "\n";
      break;
    }
    memmove(sbuf, test_str, len);
    sbuf[len] = 0; // Null terminate the string
    count2 += sizeof(gxListNode<char *>);
    if(!dlist.Add(sbuf))  {
      cout << "Memory allocation error at " << (count1+count2) << " bytes"
	   << "\n";
      break;
    }
  }
  cout << "Done" << "\n";
  PausePrg();

  cout << "Releasing memory allocated back to the heap..." << "\n";
  // NOTE: A gxList::DestroyList() call will free the memory allocated for
  // both the node and its data.
  dlist.DestroyList();

  PausePrg();
  
  cout << "Exiting..." << "\n";
  cout << "\n";
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //


