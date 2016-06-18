// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: testprog.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
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

This is a test program for the MemoryBuffer class.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include "membuf.h"

#ifdef __MSVC_DEBUG__
#include "leaktest.h"
#endif

void PausePrg()
{
  cout << "\n";
  cout << "Press enter to continue..." << "\n";
  cin.get();
}

int main()
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

  cout << "Testing MemoryBuffer constructors..." << "\n";
  char *test_pattern = "The quick brown fox jumps over the lazy dog";
  char *test_message = "This is a test...";

  MemoryBuffer a((char *)test_pattern, strlen(test_pattern)+1);
  a[strlen(test_pattern)] = 0; // Null terminate the buffer
  cout << (char *)a << "\n";
  
  MemoryBuffer b;

  MemoryBuffer c((char *)test_message, strlen(test_message)+1);
  c[strlen(test_message)] = 0; // Null terminate the buffer
  cout << (char *)c << "\n";
  
  PausePrg();

  cout << "Testing overloaded assignment operators..." << "\n";
  b = a;
  cout << (char *)b << "\n";
  b = c;
  cout << (char *)b << "\n";
  
  PausePrg();

  cout << "Testing copy constructor..." << "\n";
  MemoryBuffer aa(a);
  cout << (char *)aa << "\n";
  MemoryBuffer bb(b);
  cout << (char *)bb << "\n";

  PausePrg();
  
  cout << "Testing overloaded += operator..." << "\n";
  char *num_test = " 0123456789";
  MemoryBuffer sbuf((char *)test_pattern, strlen(test_pattern));
  MemoryBuffer buf((char *)num_test, strlen(num_test));
  sbuf += buf;
  sbuf += '\0'; // Null terminate the buffer
  cout << (char *)sbuf << "\n";

  PausePrg();
  
  cout << "Testing concatenation..." << "\n";
  char *s1 = "DOG "; char *s2 = "CAT "; char *s3 = "MOUSE";
  MemoryBuffer m1;
  m1.Load(s1, strlen(s1));
  m1.Cat(s2, strlen(s2));
  m1.Cat(s3, strlen(s3));
  m1.Cat('\0'); // Null terminate the buffer
  cout << (char *)m1 << "\n";

  PausePrg();

  cout << "Testing find functions..." << "\n";
  cout << "String = " << (char *)m1 << "\n";
  if(m1.Find(s2, strlen(s2), 0) == __MEMBUF_NO_MATCH__)
    cout << "Pattern not found!" << "\n";
  else
    cout << "Found pattern: " << s1 << "\n";

  if(m1.Find(s3, strlen(s3), 0) == __MEMBUF_NO_MATCH__)
    cout << "Pattern not found!" << "\n";
  else
    cout << "Found pattern: " << s3 << "\n";

  cout << "\n";

  cout << "Testing repeated pattern finding..." << "\n";
  MemoryBuffer m2(c);
  cout << "String = " << (char *)m2 << "\n";
  char *s4 = "is";
  unsigned offset = 0;
  while(1) {
    offset = m2.Find((char *)s4, strlen(s4), offset);
    if (offset == __MEMBUF_NO_MATCH__) break;
    cout << "Found pattern \"" << s4 << "\" at index: " << offset << "\n";
    offset++;
  }

  PausePrg();

  cout << "Testing delete function..." << "\n";
  MemoryBuffer x1(a);
  cout << (char *)x1 << "\n";
  char *xx = "fox";
  int Index = x1.Find((char *)xx);
  cout << "Deleting fox from string..." << "\n";
  x1.DeleteAt(Index, strlen(xx));
  cout << (char *)x1 << "\n";

  PausePrg();

  cout << "Testing replace functions..." << "\n";
  MemoryBuffer x2(a);
  cout << (char *)x2 << "\n";
  char *xy = "cat";
  Index = x2.Find(xx);
  cout << "Replacing fox:" << "\n";
  x2.ReplaceAt(Index, xy, strlen(xy));  
  cout << (char *)x2 << "\n";
  cout << "Replacing jumps:" << "\n";
  Index = x2.Find((void *)"jumps", strlen("jumps"));
  x2.ReplaceAt(Index, "runs ", strlen("runs "));
  cout << (char *)x2 << "\n";

  MemoryBuffer x3((char *)"cow", 3);
  cout << "Replacing dog:" << "\n";
  Index = x2.Find((void *)"dog");
  x2.ReplaceAt(Index, x3);
  cout << (char *)x2 << "\n";
  
  PausePrg();
  
  cout << "Testing the insert functions..." << "\n";
  MemoryBuffer x4(a);
  cout << (char *)x4 << "\n";
  char *xz = "and yellow ";
  cout << "Inserting text into string:" << "\n";
  Index = x4.Find(xx);
  x4.InsertAt(Index, xz, strlen(xz));
  cout << (char *)x4 << "\n";
  Index = x4.Find((void *)"over");
  x4.InsertAt(Index, (char *)"around and ", strlen("around and "));
  cout << (char *)x4 << "\n";

  MemoryBuffer x5((char *)"cow and ", strlen("cow and "));
  Index = x4.Find((void *)"dog");
  x4.InsertAt(Index, x5);
  cout << (char *)x4 << "\n";

  PausePrg();
  
  cout << "Testing dynamic allocation functions..." << "\n";
  MemoryBuffer z(c);
  cout << "String = " << (char *)z << "\n";
  cout << "Buffer's logical length = " << z.length() << "\n";
  cout << "Buffer's allocated length = " << z.dlength() << "\n";
  cout << "Changing allocated length..." << "\n";
  z.Realloc(z.length()+16);
  cout << "Buffer's allocated length = " << z.dlength() << "\n";
  cout << "Destorying the buffer" << "\n";
  z.Destroy();
  cout << "Buffer's logical length = " << z.length() << "\n";
  cout << "Buffer's allocated length = " << z.dlength() << "\n";
  
  PausePrg();

  cout << "Testing resizing function" << "\n";
  MemoryBuffer rs(a);
  cout << "String = " << (char *)rs << "\n";
  cout << "Buffer's logical length = " << rs.length() << "\n";
  cout << "Buffer's allocated length = " << rs.dlength() << "\n";
  cout << "Resizing the buffer to 24 bytes" << "\n";
  rs.resize(24);
  cout << "Buffer's logical length = " << rs.length() << "\n";
  cout << "Buffer's allocated length = " << rs.dlength() << "\n";
  cout << "Freeing unused allocated space..." << "\n";
  rs.FreeBytes();
  cout << "Buffer's logical length = " << rs.length() << "\n";
  cout << "Buffer's allocated length = " << rs.dlength() << "\n";

  PausePrg();
  
  cout << "Testing overload subscript operator..." << "\n";
  char *Message = "This is a test message";
  MemoryBuffer ss((char *)Message, strlen(Message));
  unsigned i;
  for(i = 0; i < strlen(Message); i++)
    cout << ss[i];
  cout << "\n";

  PausePrg();
  cout << "Testing memset function..." << "\n";
  unsigned bytes = 25;
  MemoryBuffer fill_buf(bytes);
  fill_buf.MemSet('*'); // Fill the entire memory buffer
  for(i = 0; i < bytes; i++) cout << fill_buf[i];
  cout << "\n";
  // Fill the buffer with 5 bytes starting at zero based offset 10
  fill_buf.MemSet('?', 10, 5); 
  for(i = 0; i < bytes; i++) cout << fill_buf[i];
  cout << "\n";
  // Set the first and last bytes
  fill_buf.MemSet('!', 0, 1); 
  fill_buf.MemSet('!', (bytes-1), 1); 
  for(i = 0; i < bytes; i++) cout << fill_buf[i];
  cout << "\n";

  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
