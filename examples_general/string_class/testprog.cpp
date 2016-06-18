// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: testprog.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/29/1996  
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

This is a test program for the gxString class.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include "gxstring.h"

#ifdef __MSVC_DEBUG__
#include "leaktest.h"
#endif

void PausePrg()
{
#ifndef __AUTORUN_TESTPROG__
  cout << "\n";
  cout << "Press enter to continue..." << "\n";
  cin.get();
#endif
}

void ASCIIStringTest()
{
  cout << "Testing all ASCII string class functions..." << "\n";
  PausePrg();

  cout << "Testing constructors..." << "\n";

  gxString a("The quick brown fox jumps over the lazy dog");
  cout << a << "\n";
  
  gxString b;

  gxString c("This is a test");
  cout << c << "\n";

  cout << "Copy constructing a string..." << "\n";
  gxString d(c);
  cout << d << "\n";

  cout << "\n";
  cout << "Testing overloaded assignment operators..." << "\n";
  b = a;
  cout << b << "\n";
  b = c;
  cout << b << "\n";
  b = "0123456789";
  cout << b << "\n";
  
  cout << "\n";
  cout << "Testing copy constructor..." << "\n";
  gxString aa(a);
  cout << aa << "\n";
  gxString bb(b);
  cout << bb << "\n";

  PausePrg();

  cout << "Testing overloaded += operator..." << "\n";
  gxString buf(" 0123456789");
  a += buf;
  cout << a << "\n";
  c += " message";
  cout << c << "\n";
  c += 'X';
  cout << c << "\n";

  PausePrg();

  cout << "Testing concatenation..." << "\n";
  gxString s1("String 1"), s2("String 2");
  char *str3 = " and String 3";
  s1.Cat(" and ", 5);
  s1 += s2;
  cout << s1 << "\n"; 
  s1.Cat(str3);
  cout << s1 << "\n";

  PausePrg();
  
  cout << "Testing find functions..." << "\n";
  cout << s1 << "\n";
  cout << "Searching for: " << "xyz" << "\n";
  if(s1.Find("xyz") == -1)
    cout << "Pattern not found!" << "\n";
  else
    cout << "Pattern found." << "\n";
  cout << "Searching for: " << str3 << "\n";
  if(s1.Find(str3, gxstrlen(str3)) == -1)
    cout << "Pattern not found!" << "\n";
  else
    cout << "Pattern found." << "\n";
  cout << "Searching for: " << s2 << "\n";
  if(s1.Find(s2) == -1)
    cout << "Pattern not found!" << "\n";
  else
    cout << "Pattern found." << "\n";

  PausePrg();

  cout << "Testing ifind functions..." << "\n";
  cout << s1 << "\n";
  cout << "Searching for: " << "xyz" << "\n";
  if(s1.IFind("xyz") == -1)
    cout << "Pattern not found!" << "\n";
  else
    cout << "Pattern found." << "\n";
  cout << "Searching for: " << str3 << "\n";
  if(s1.IFind(str3, gxstrlen(str3)) == -1)
    cout << "Pattern not found!" << "\n";
  else
    cout << "Pattern found." << "\n";
  cout << "Searching for: " << s2 << "\n";
  if(s1.IFind(s2) == -1)
    cout << "Pattern not found!" << "\n";
  else
    cout << "Pattern found." << "\n";

  PausePrg();

  cout << "Testing repeated pattern finding..." << "\n";
  s1 = "This is a test message";
  cout << s1 << "\n";
  cout << "Searching for: " << "is" << "\n";
  int offset = 0;
  while(1) {
    offset = s1.Find("is", offset);
    if (offset == -1) break;
    cout << "Pattern match found at index: " << offset << "\n";
    offset++;
  }

  PausePrg();

  cout << "Testing delete function..." << "\n";
  gxString x1("The quick brown fox jumps over the lazy dog");
  cout << x1 << "\n";
  char *xx = "fox";
  int Index = x1.Find(xx);
  cout << "Deleting fox from string..." << "\n";
  x1.DeleteAt(Index, gxstrlen(xx));
  cout << x1 << "\n";

  PausePrg();
  
  cout << "Testing replace functions..." << "\n";
  x1 = "The quick brown fox jumps over the lazy dog";
  cout << x1 << "\n";
  char *xy = "cat";
  Index = x1.Find(xx);
  cout << "Replacing fox:" << "\n";
  x1.ReplaceAt(Index, xy, gxstrlen(xy));  
  cout << x1 << "\n";
  cout << "Replacing jumps:" << "\n";
  Index = x1.Find("jumps");
  x1.ReplaceAt(Index, "runs ");
  cout << x1 << "\n";
  gxString x2("cow");
  cout << "Replacing dog:" << "\n";
  Index = x1.Find("dog");
  x1.ReplaceAt(Index, x2);
  cout << x1 << "\n";
  
  PausePrg();
  
  cout << "Testing the insert functions..." << "\n";
  x1 = "The quick brown fox jumps over the lazy dog";
  cout << x1 << "\n";
  char *xz = "and yellow ";
  cout << "Inserting text into string:" << "\n";
  Index = x1.Find(xx);
  x1.InsertAt(Index, xz, gxstrlen(xz));
  cout << x1 << "\n";
  Index = x1.Find("over");
  x1.InsertAt(Index, "around and ");
  cout << x1 << "\n";
  gxString x3("cow and ");
  Index = x1.Find("dog");
  x1.InsertAt(Index, x3);
  cout << x1 << "\n";
  
  PausePrg();
  
  cout << "Testing c_str functions" << "\n";
  gxString gs("COW");
  const gxString &cs = gs;
  cout << "String = " << gs.c_str() << "\n";
  cout << "Const String = " << cs.c_str() << "\n";
  PausePrg();
  
  cout << "Testing overload subscript operator..." << "\n";
  char *Message = "This is a test message";
  gxString ss(Message);
  for(unsigned i = 0; i < gxstrlen(Message); i++)
    cout << ss[i];
  cout << "\n";
  
  PausePrg();

  cout << "Testing general-purpose filter functions" << "\n";
  cout << "\n";
  
    gxString s("         ?The quick brown fox jumps over the lazy dog\n\r\
0123456789\r\n?~!@#$%^&*()_+<>            ");

  unsigned num;
  cout << "Original string: " << "\n";
  cout << s.c_str() << "\n";
  cout << "\n";

  cout << "Trimming all leading and trailing spaces" << "\n";
  cout << "begin" << s.c_str() << "end" << "\n";
  num = s.TrimLeadingSpaces();
  cout << "Leading spaces = " << num << "\n";
  cout << s.c_str() << "\n";
  num = s.TrimTrailingSpaces();
  cout << "Trialing spaces = " << num << "\n";
  cout << s.c_str();
  cout << "end";

  PausePrg();
  
  cout << "Changing to upper case" << "\n";
  if(s.ToUpper()) cout << s.c_str() << "\n";
  cout << "Changing to lower case" << "\n";
  if(s.ToLower()) cout << s.c_str() << "\n";

  PausePrg();
  
  cout << "Replacing carriage returns/line feeds with spaces" << "\n";
  num = s.ReplaceChar('\r', ' ');
  num = s.ReplaceString("\n", " ");
  cout << s.c_str() << "\n";

  PausePrg();
  
  cout << "Filtering all \'?\' characters" << "\n";
  num = s.FilterChar('?');
  cout << s.c_str() << "\n";

  PausePrg();
  
  cout << "Filtering the \"he\" string" << "\n";
  s.FilterString("he");
  cout << s.c_str() << "\n";
  
  PausePrg();

  cout << "Testing overloaded << operators" << "\n";
  gxString stream;
  const int i1 = 1; const long i2 = 2; const short i3 = 3;
  int i4 = 255; long i5 =1024; short i6 = 2048;
  unsigned int i7 = 4096; unsigned long i8 = 8192; unsigned short i9 = 16384;
  float f1 = (float)4.22; double f2 = (double)8.092; 
  char c1 = 'A'; char c2 = 'B'; char c3 = 'C'; char c4 = 'D';
  char *str1 = "Output test = "; gxString str2 = "Output test = ";
  
  stream << "Output test = " << c1 << ' ' << c2 << ' ' << c3 << ' '
	 << c4 << "\n";
  cout << stream;

  stream << clear << str1 << i1 << " " << i2 << " " << i3 << " " << i4 << " "
	 << i5 << " " << i6 << " " << i7 << " " << i8 << " " << i9 << "\n";
  cout << stream;
  
  stream.Clear();
  stream.SetWidth(4); stream.SetFill('0');
  stream << str2 << hex << i1 << " " << i2 << " " << i3 << " " 
	 << i4 << " " << i5 << " " << i6 << " " << i7 << " " << i8 
	 << " " << i9 << "\n";
  cout << stream;

  stream.Clear();
  stream.Precision(2);
  stream << "Output test = " << f1 << " " << f2 << "\n";
  stream.Precision(4);
  stream << "Output test = " << f1 << " " << f2 << "\n";
  cout << stream;
  stream << clear;

  PausePrg();
  cout << "Testing overloaded >> operators" << "\n";
  char csbuf[255];
  gxString sb1 = "Input test = ";
  sb1 << (int)15 << " " << (long)25 << " " << (short)35 << " " 
      << (unsigned int)45 << " " << (unsigned long)55 << " "
      << (unsigned short)65 << " " << (double)1.5 << " "
      << (float)2.5 << " " << 'A' << 'B' << 'C' << "\n";

  sb1 >> csbuf;
  cout << csbuf;

  gxString sb2, sb3, sb4, sb5, sb6, sb7, sb8, sb9, sb10, sb11, sb12;
  sb2 << (int)15; sb3 << (long)25; sb4 << (short)35; 
  sb5 << (unsigned int)45; sb6 << (unsigned long)55; 
  sb7 << (unsigned short)65; sb8 << (double)1.5; 
  sb9 << (float)2.5; sb10 << 'A'; sb11 << 'B'; sb12 << 'C'; 

  cout << "Input test = " << sb2 << " " << sb3 << " " << sb4 << " "
       << sb5 << " " << sb6 << " " << sb7 << " " <<  sb8 << " " << sb9
       << " " << sb10 << sb11 << sb12 << "\n";

  int ib2 = 0; long ib3 = 0; short ib4 = 0; unsigned int ib5 = 0; 
  unsigned long ib6 = 0; unsigned short ib7 = 0; double ib8 = 0; 
  float ib9 = 0; char ib10 = 0; char ib11 = 0; char ib12 = 0;
  
  sb2 >> ib2;
  sb3 >> ib3;
  sb4 >> ib4;
  sb5 >> ib5;
  sb6 >> ib6;
  sb7 >> ib7;
  sb8 >> ib8;
  sb9 >> ib9;
  sb10 >> ib10;
  sb11 >> ib11;
  sb12 >> ib12;

  cout << "Input test = " << ib2 << " " << ib3 << " " << ib4 << " "
       << ib5 << " " << ib6 << " " << ib7 << " " <<  ib8 << " " << ib9
       << " " << ib10 << ib11 << ib12 << "\n";

#if defined (__64_BIT_DATABASE_ENGINE__)
  PausePrg();

  cout << "Testing overloaded >> and << 64-bit operators" << "\n";
  __LLWORD__ LL1 = (__LLWORD__)255; 
  const __LLWORD__ LL2 = (__LLWORD__)1024;
  __ULLWORD__ LL3 = (__ULLWORD__)2048; 
  const __ULLWORD__ LL4 = (__LLWORD__)4096;

  stream << clear << "64-bit test = " << LL1 << " " << LL2 << " " << LL3 
	 << " " << LL4 << "\n";

  stream >> csbuf;
  cout << csbuf;

  stream.Clear();
  stream.SetWidth(4); stream.SetFill('0');
  stream << "64-bit test = " << hex << LL1 << " " << LL2 << " " 
	 << LL3 << " " << LL4 << "\n";

  cout << stream;

  gxString sb64_1, sb64_2, sb64_3, sb64_4;
  sb64_1 << LL1;
  sb64_2 << LL2;
  sb64_3 << LL3;
  sb64_4 << LL4;

  __LLWORD__ LLn1, LLn2;
  __ULLWORD__ LLn3, LLn4;

  sb64_1 >> LLn1;
  sb64_2 >> LLn2;
  sb64_3 >> LLn3;
  sb64_4 >> LLn4;

  cout << "64-bit test = " << (int)LLn1 << " " << (int)LLn2 << " " 
       << (int)LLn3 << " " << (int) LLn4 << "\n";
#endif

  PausePrg();

  cout << "Testing escape sequence conversion function" << "\n";
  cout << "\n";
  gxString esc_string("This%20is%20a%20test%20for%20%%%20escape%20chars");
  cout << esc_string.c_str() << "\n";
  cout << "Converting escape sequences" << "\n";
  int num_esc = esc_string.ConvertEscapeSeq('%');
  cout << "Found " << num_esc << " escape sequences" << "\n"; 
  cout << esc_string.c_str() << "\n";
  cout << "\n";

  esc_string = "This#0020is#0020a#0020test#0020for#0020###0020escape\
#0020chars";
  cout << esc_string.c_str() << "\n";
  cout << "Converting escape sequences" << "\n";
  num_esc = esc_string.ConvertEscapeSeq('#', 4);
  cout << "Found " << num_esc << " escape sequences" << "\n"; 
  cout << esc_string.c_str() << "\n";
  cout << "\n";

  esc_string = "This;020is;020a;020test;020for;020;;;020escape;020chars";
  cout << esc_string.c_str() << "\n";
  cout << "Converting escape sequences" << "\n";
  num_esc = esc_string.ConvertEscapeSeq(';', 3);
  cout << "Found " << num_esc << " escape sequences" << "\n"; 
  cout << esc_string.c_str() << "\n";
  
  PausePrg();

  cout << "Testing the comparison operators" << "\n";

#ifndef __AUTORUN_TESTPROG__
  gxString acomp(255), bcomp;
  cout << "Enter string A>"; cin >> acomp;
  cout << "Enter string B>"; cin >> bcomp;
#else
  // In auto run mode so do not prompt user for any input
  gxString acomp("CAT"), bcomp("cat");
  cout << "String A = CAT, String B = cat" << "\n";
#endif

  cout << "String A == B "; acomp == bcomp ? cout << 'y' : cout << 'n';
  cout << "\n";
  cout << "String A != B "; acomp != bcomp ? cout << 'y' : cout << 'n';
  cout << "\n";
  cout << "String A <  B "; acomp < bcomp ? cout << 'y' : cout << 'n';
  cout << "\n";
  cout << "String A <= B "; acomp <= bcomp ? cout << 'y' : cout << 'n';
  cout << "\n";
  cout << "String A >  B "; acomp > bcomp ? cout << 'y' : cout << 'n';
  cout << "\n";
  cout << "String A >= B "; acomp >= bcomp ? cout << 'y' : cout << 'n';
  cout << "\n";
  
  PausePrg();

  cout << "Testing the case insensitive comparison functions" << "\n";
  cout << "String A == B "; 
  acomp.StringICompare(bcomp) == 0 ? cout << 'y' : cout << 'n';
  cout << "\n";
  cout << "String A != B "; 
  acomp.StringICompare(bcomp) != 0 ? cout << 'y' : cout << 'n';
  cout << "\n";
  cout << "String A <  B "; 
  acomp.StringICompare(bcomp) < 0 ? cout << 'y' : cout << 'n';
  cout << "\n";
  cout << "String A <= B "; 
  acomp.StringICompare(bcomp) <= 0 ? cout << 'y' : cout << 'n';
  cout << "\n";
  cout << "String A >  B "; 
  acomp.StringICompare(bcomp) > 0 ? cout << 'y' : cout << 'n';
  cout << "\n";
  cout << "String A >= B "; 
  acomp.StringICompare(bcomp) >= 0 ? cout << 'y' : cout << 'n';
  cout << "\n";

  PausePrg();

  cout << "Testing the find last function" << "\n";
  gxString url("http://www.example.com:80/docs/index.htm");
  cout << "Complete URL: " << url.c_str() << "\n"; 
  cout << "Searching for last \"/\"" << "\n";
  offset = url.FindLast("/");
  if(offset != -1) {
    cout << "Found last \"/\" at offset: " << offset << "\n";
  }
  else {
    cout << "Could not find last \"/\"" << "\n";
  }
  cout << "\n";

  cout << "Testing delete before and after functions" << "\n";
  gxString ubuf = url;  
  ubuf.DeleteBeforeIncluding("://");
  ubuf.DeleteAfterIncluding(":");
  cout << "Complete URL: " << url.c_str() << "\n";
  cout << "Server: " << ubuf.c_str() << "\n";
  ubuf = url;
  ubuf.DeleteAfter("http");
  cout << "Protocol: " << ubuf.c_str() << "\n";
  ubuf = url;
  ubuf.DeleteBeforeLast(":");
  ubuf.DeleteAfterIncluding("/");
  ubuf.DeleteAt(0, 1);
  cout << "Port: " << ubuf.c_str() << "\n";
  ubuf = url;
  ubuf.DeleteBeforeIncluding("://");
  ubuf.DeleteBeforeIncluding("/");
  ubuf.DeleteAfterLastIncluding("/");
  ubuf.InsertAt(0, "/");
  cout << "Directory: " << ubuf.c_str() << "\n";
  ubuf = url;
  ubuf.DeleteBeforeLastIncluding("/");
  cout << "File: " << ubuf.c_str() << "\n";
  ubuf.DeleteBefore(".");
  cout << "Dot extension: " << ubuf.c_str() << "\n";

  PausePrg();

  cout << "Testing shorthand notation nomenclature" << "\n";
  long by1 = 519;
  unsigned long by2 = 2133;
  gxString byte_string;
  byte_string << clear << "Byte value 1 = " << shn << by1 << " bytes " 
	      << "Byte value 2 = " << by2 << " bytes" << "\n";
  cout << byte_string.c_str();
  by1 *= (1000 * 1000);
  by2 *= (1000 * 1000);
  byte_string << clear << "Byte value 1 = " << shn << by1 << " bytes " 
	      << "Byte value 2 = " << by2 << " bytes" << "\n";
  cout << byte_string.c_str();

#if defined (__64_BIT_DATABASE_ENGINE__)
  __LLWORD__ llby1 = 519;
  __ULLWORD__ llby2 = 2133;
   byte_string << clear << "Byte value 1 = " << shn << llby1 << " bytes " 
	      << "Byte value 2 = " << llby2 << " bytes" << "\n";
  cout << byte_string.c_str();
  llby1 *= (1000 * 1000);
  llby2 *= (1000 * 1000);
  byte_string << clear << "Byte value 1 = " << shn << llby1 << " bytes " 
	      << "Byte value 2 = " << llby2 << " bytes" << "\n";
  cout << byte_string.c_str();
  llby1 *= 1000;
  llby2 *= 1000;
  byte_string << clear << "Byte value 1 = " << shn << llby1 << " bytes " 
	      << "Byte value 2 = " << llby2 << " bytes" << "\n";
  cout << byte_string.c_str();
#endif

  PausePrg();

  cout << "Testing octal conversion functions" << "\n";
  gxString o_string;
  int onum1 = 0755;
  int onum2 = 0644;
  int onum3 = 0220;
  o_string << clear << oct << onum1 << " " << onum2 << " " << onum3 << "\n";
  cout << o_string.c_str();

#if defined (__64_BIT_DATABASE_ENGINE__)
  __LLWORD__ lonum1 = 0755;
  __LLWORD__ lonum2 = 0644;
  __LLWORD__ lonum3 = 0220;
  o_string << clear << oct << lonum1 << " " << lonum2 << " " << lonum3 << "\n";
  cout << o_string.c_str();
#endif

  PausePrg();

  cout << "Testing char trim functions" << "\n";
  gxString trim_str = "######Test String______";
  cout << "String = " << trim_str.c_str() << "\n";
  trim_str.TrimLeading('#'); trim_str.TrimTrailing('_');
  cout << "String = " << trim_str.c_str() << "\n";

  PausePrg();

  cout << "Testing substring functions" << "\n";
  gxString substr("The quick brown fox jumps over the lazy dog");
  cout << substr.c_str() << "\n";
  Index = substr.Find("fox");
  cout << "Testing mid function:" << "\n";
  gxString Fox = substr.Mid(Index, 3); // 3 characters for fox
  cout << Fox.c_str() << "\n";
  cout << "Testing left function:" << "\n";
  gxString The = substr.Left(3); // 3 characters for the
  cout << The.c_str() << "\n";
  cout << "Testing right function:" << "\n";
  gxString Dog = substr.Right(3); // 3 characters for dog
  cout << Dog.c_str() << "\n";

  PausePrg();
  cout << "Testing string parser" << "\n";
  gxString input_str = "a | b | c | d | e | f | g";
  unsigned num_arr = 0;
  gxString delimiter = "|";
  unsigned arr_index;
  gxString *output_arr = 0;

  cout << "Parsing: " << input_str.c_str() << "\n"; 
  output_arr = ParseStrings(input_str, delimiter, num_arr, 1, 1);
  if(output_arr) {
    for(arr_index = 0; arr_index < num_arr; arr_index++) {
      cout << "[" << arr_index << "] " << output_arr[arr_index].c_str() << "\n";
    }
  }
  if(output_arr) delete[] output_arr;

  cout << "\n";
  input_str = "This is a test with no delimiter";
  cout << "Parsing: " << input_str.c_str() << "\n"; 
  output_arr = ParseStrings(input_str, delimiter, num_arr, 1, 1);
  if(output_arr) {
    for(arr_index = 0; arr_index < num_arr; arr_index++) {
      cout << "[" << arr_index << "] " << output_arr[arr_index].c_str() << "\n";
    }
  }
  if(output_arr) delete[] output_arr;

  cout << "\n";
  input_str = "\"This is a CSV test\",\"One\",\"Two\",\"With a \\, comma\",No Quotes, Spaces ,   Extra spaces   ,,,";
  delimiter = ",";
  cout << "Parsing: " << input_str.c_str() << "\n"; 
  output_arr = ParseStrings(input_str, delimiter, num_arr, 1, 1);
  if(output_arr) {
    for(arr_index = 0; arr_index < num_arr; arr_index++) {
      cout << "[" << arr_index << "] " << output_arr[arr_index].c_str() << "\n";
    }
  }
  if(output_arr) delete[] output_arr;
  

  cout << "\n";
  cout << "Exiting ASCII string test" << "\n";
  PausePrg();
}

void UNICODEStringTest()
{
  cout << "Testing all Unicode string class functions..." << "\n";
  PausePrg();

  cout << "Testing constructors..." << "\n";

  wchar_t wsbuf[255];

  gxStringw a(gxLTCHAR("The quick brown fox jumps over the lazy dog", wsbuf));
  cout << a << "\n";
  
  gxStringw b;
  gxStringw c(gxLTCHAR("This is a test", wsbuf));
  cout << c << "\n";
  cout << "Copy constructing a string..." << "\n";
  gxStringw d(c);
  cout << d << "\n";
  cout << "\n";


  cout << "Testing overloaded assignment operators..." << "\n";
  b = a;
  cout << b << "\n";
  b = c;
  cout << b << "\n";
  b = gxLTCHAR("0123456789", wsbuf);
  cout << b << "\n";

  cout << "\n";
  cout << "Testing copy constructor..." << "\n";
  gxStringw aa(a);
  cout << aa << "\n";
  gxStringw bb(b);
  cout << bb << "\n";

  PausePrg();

  cout << "Testing overloaded += operator..." << "\n";
  gxStringw buf(gxLTCHAR(" 0123456789", wsbuf));

  a += buf;

  cout << a << "\n";
  c += gxLTCHAR(" message", wsbuf);
  cout << c << "\n";
  c += gxLTCHAR('X', wsbuf);
  cout << c << "\n";

  PausePrg();

  cout << "Testing concatenation..." << "\n";
  gxStringw s1(gxLTCHAR("String 1", wsbuf)), s2(gxLTCHAR("String 2", wsbuf));
  char *str3 = " and String 3";
  s1.Cat(gxLTCHAR(" and ", wsbuf), 5);
  s1 += s2;
  cout << s1 << "\n"; 
  s1.Cat(str3);
  cout << s1 << "\n";

  PausePrg();
  
  cout << "Testing find functions..." << "\n";
  cout << s1 << "\n";
  cout << "Searching for: " << "xyz" << "\n";
  if(s1.Find(gxLTCHAR("xyz", wsbuf)) == -1)
    cout << "Pattern not found!" << "\n";
  else
    cout << "Pattern found." << "\n";
  cout << "Searching for: " << str3 << "\n";
  if(s1.Find(str3, gxstrlen(str3)) == -1)
    cout << "Pattern not found!" << "\n";
  else
    cout << "Pattern found." << "\n";
  cout << "Searching for: " << s2 << "\n";
  if(s1.Find(s2) == -1)
    cout << "Pattern not found!" << "\n";
  else
    cout << "Pattern found." << "\n";

  PausePrg();
  
  cout << "Testing ifind functions..." << "\n";
  cout << s1 << "\n";
  cout << "Searching for: " << "xyz" << "\n";
  if(s1.IFind(gxLTCHAR("xyz", wsbuf)) == -1)
    cout << "Pattern not found!" << "\n";
  else
    cout << "Pattern found." << "\n";
  cout << "Searching for: " << str3 << "\n";
  if(s1.IFind(str3, gxstrlen(str3)) == -1)
    cout << "Pattern not found!" << "\n";
  else
    cout << "Pattern found." << "\n";
  cout << "Searching for: " << s2 << "\n";
  if(s1.IFind(s2) == -1)
    cout << "Pattern not found!" << "\n";
  else
    cout << "Pattern found." << "\n";

  PausePrg();

  cout << "Testing repeated pattern finding..." << "\n";
  s1 = "This is a test message";
  cout << s1 << "\n";
  cout << "Searching for: " << "is" << "\n";
  int offset = 0;
  while(1) {
    offset = s1.Find(gxLTCHAR("is", wsbuf), offset);
    if (offset == -1) break;
    cout << "Pattern match found at index: " << offset << "\n";
    offset++;
  }

  PausePrg();

  cout << "Testing delete function..." << "\n";
  gxStringw x1(gxLTCHAR("The quick brown fox jumps over the lazy dog", wsbuf));
  cout << x1 << "\n";
  wchar_t *xx = gxLTCHAR("fox", wsbuf);
  int Index = x1.Find(xx);
  cout << "Deleting fox from string..." << "\n";
  x1.DeleteAt(Index, gxstrlen(xx));
  cout << x1 << "\n";

  PausePrg();
  
  cout << "Testing replace functions..." << "\n";
  x1 = "The quick brown fox jumps over the lazy dog";
  cout << x1 << "\n";
  wchar_t *xy = gxLTCHAR("cat", wsbuf);
  Index = x1.Find(gxLTCHAR("fox", wsbuf));
  cout << "Replacing fox:" << "\n";
  x1.ReplaceAt(Index, xy, gxstrlen(xy));  
  cout << x1 << "\n";
  cout << "Replacing jumps:" << "\n";
  Index = x1.Find(gxLTCHAR("jumps", wsbuf));
  x1.ReplaceAt(Index, gxLTCHAR("runs ", wsbuf));
  cout << x1 << "\n";
  gxStringw x2(gxLTCHAR("cow", wsbuf));
  cout << "Replacing dog:" << "\n";
  Index = x1.Find(gxLTCHAR("dog", wsbuf));
  x1.ReplaceAt(Index, x2);
  cout << x1 << "\n";
  
  PausePrg();
  
  cout << "Testing the insert functions..." << "\n";
  x1 = gxLTCHAR("The quick brown fox jumps over the lazy dog", wsbuf);
  cout << x1 << "\n";
  cout << "Inserting text into string:" << "\n";
  Index = x1.Find(gxLTCHAR("fox", wsbuf));
  x1.InsertAt(Index, gxLTCHAR("and yellow ", wsbuf), gxstrlen("and yellow "));
  cout << x1 << "\n";
  Index = x1.Find(gxLTCHAR("over", wsbuf));
  x1.InsertAt(Index, gxLTCHAR("around and ", wsbuf));
  cout << x1 << "\n";
  gxStringw x3(gxLTCHAR("cow and ", wsbuf));
  Index = x1.Find(gxLTCHAR("dog", wsbuf));
  x1.InsertAt(Index, x3);
  cout << x1 << "\n";
  
  PausePrg();
  
  cout << "Testing c_str functions" << "\n";
  char sbuf[255]; // ASCII string buffer for Unicode to ASCII conversion
  gxStringw gs(gxLTCHAR("COW", wsbuf));
  const gxStringw &cs = gs;
  cout << "String = " << gs.c_str(sbuf) << "\n";
  cout << "Const String = " << cs.c_str(sbuf) << "\n";
  PausePrg();
  
  cout << "Testing overload subscript operator..." << "\n";
  wchar_t *Message = gxLTCHAR("This is a test message", wsbuf);
  gxStringw ss(Message);
  for(unsigned i = 0; i < gxstrlen(Message); i++)
    cout << (char)ss[i];
  cout << "\n";
  
  PausePrg();

  cout << "Testing general-purpose filter functions" << "\n";
  cout << "\n";
  
    gxStringw s(gxLTCHAR(\
"         ?The quick brown fox jumps over the lazy dog\n\r\
0123456789\r\n?~!@#$%^&*()_+<>            ", wsbuf));

  unsigned num;
  cout << "Original string: " << "\n";
  cout << s.c_str(sbuf) << "\n";
  cout << "\n";

  cout << "Trimming all leading and trailing spaces" << "\n";
  cout << "begin" << s.c_str(sbuf) << "end" << "\n";
  num = s.TrimLeadingSpaces();
  cout << "Leading spaces = " << num << "\n";
  cout << s.c_str(sbuf) << "\n";
  num = s.TrimTrailingSpaces();
  cout << "Trialing spaces = " << num << "\n";
  cout << s.c_str(sbuf);
  cout << "end";

  PausePrg();
  
  cout << "Changing to upper case" << "\n";
  if(s.ToUpper()) cout << s.c_str(sbuf) << "\n";
  cout << "Changing to lower case" << "\n";
  if(s.ToLower()) cout << s.c_str(sbuf) << "\n";

  PausePrg();
  
  cout << "Replacing carriage returns/line feeds with spaces" << "\n";
  wchar_t wsbuf2[255];
  num = s.ReplaceChar(gxLTCHAR('\r', wsbuf), gxLTCHAR(' ', wsbuf2));
  num = s.ReplaceString(gxLTCHAR("\n", wsbuf), gxLTCHAR(" ", wsbuf2));
  cout << s.c_str(sbuf) << "\n";

  PausePrg();
  
  cout << "Filtering all \'?\' characters" << "\n";
  num = s.FilterChar(gxLTCHAR('?', wsbuf));
  cout << s.c_str(sbuf) << "\n";

  PausePrg();
  
  cout << "Filtering the \"he\" string" << "\n";
  s.FilterString(gxLTCHAR("he", wsbuf));
  cout << s.c_str(sbuf) << "\n";
  
  PausePrg();

  cout << "Testing overloaded << operators" << "\n";
  gxStringw stream;
  const int i1 = 1; const long i2 = 2; const short i3 = 3;
  int i4 = 255; long i5 =1024; short i6 = 2048;
  unsigned int i7 = 4096; unsigned long i8 = 8192; unsigned short i9 = 16384;
  float f1 = (float)4.22; double f2 = (double)8.092; 
  char c1 = 'A';  char c2 = 'B'; 
  wchar_t c3 = gxLTCHAR('C', wsbuf);  wchar_t c4 = gxLTCHAR('D', wsbuf2);
  char *str1 = "Output test = "; gxStringw str2 = "Output test = ";
  
  stream << "Output test = " << c1 << ' ' << c2 << ' ' << (char)c3 << ' '
	 << (char)c4 << "\n";
  cout << stream;

  stream << clear << str1 << i1 << " " << i2 << " " << i3 << " " << i4 << " "
	 << i5 << " " << i6 << " " << i7 << " " << i8 << " " << i9 << "\n";
  cout << stream;
  
  stream.Clear();
  stream.SetWidth(4); stream.SetFill('0');
  stream << str2 << hex << i1 << " " << i2 << " " << i3 << " " 
	 << i4 << " " << i5 << " " << i6 << " " << i7 << " " << i8 
	 << " " << i9 << "\n";
  cout << stream;

  stream.Clear();
  stream.Precision(2);
  stream << "Output test = " << f1 << " " << f2 << "\n";
  stream.Precision(4);
  stream << "Output test = " << f1 << " " << f2 << "\n";
  cout << stream;
  stream << clear;

  PausePrg();
  cout << "Testing overloaded >> operators" << "\n";
  char csbuf[255];
  gxStringw sb1 = "Input test = ";
  sb1 << (int)15 << " " << (long)25 << " " << (short)35 << " " 
      << (unsigned int)45 << " " << (unsigned long)55 << " "
      << (unsigned short)65 << " " << (double)1.5 << " "
      << (float)2.5 << " " << 'A' << 'B' << 'C' << "\n";

  sb1 >> csbuf;
  cout << csbuf;

  gxStringw sb2, sb3, sb4, sb5, sb6, sb7, sb8, sb9, sb10, sb11, sb12;
  sb2 << (int)15; sb3 << (long)25; sb4 << (short)35; 
  sb5 << (unsigned int)45; sb6 << (unsigned long)55; 
  sb7 << (unsigned short)65; sb8 << (double)1.5; 
  sb9 << (float)2.5; sb10 << 'A'; sb11 << 'B'; sb12 << 'C'; 

  cout << "Input test = " << sb2 << " " << sb3 << " " << sb4 << " "
       << sb5 << " " << sb6 << " " << sb7 << " " <<  sb8 << " " << sb9
       << " " << sb10 << sb11 << sb12 << "\n";

  int ib2 = 0; long ib3 = 0; short ib4 = 0; unsigned int ib5 = 0; 
  unsigned long ib6 = 0; unsigned short ib7 = 0; double ib8 = 0; 
  float ib9 = 0; char ib10 = 0; char ib11 = 0; char ib12 = 0;
  
  sb2 >> ib2;
  sb3 >> ib3;
  sb4 >> ib4;
  sb5 >> ib5;
  sb6 >> ib6;
  sb7 >> ib7;
  sb8 >> ib8;
  sb9 >> ib9;
  sb10 >> ib10;
  sb11 >> ib11;
  sb12 >> ib12;

  cout << "Input test = " << ib2 << " " << ib3 << " " << ib4 << " "
       << ib5 << " " << ib6 << " " << ib7 << " " <<  ib8 << " " << ib9
       << " " << ib10 << ib11 << ib12 << "\n";

#if defined (__64_BIT_DATABASE_ENGINE__)
  PausePrg();

  cout << "Testing overloaded >> and << 64-bit operators" << "\n";
  __LLWORD__ LL1 = (__LLWORD__)255; 
  const __LLWORD__ LL2 = (__LLWORD__)1024;
  __ULLWORD__ LL3 = (__ULLWORD__)2048; 
  const __ULLWORD__ LL4 = (__LLWORD__)4096;

  stream << clear << "64-bit test = " << LL1 << " " << LL2 << " " << LL3 
	 << " " << LL4 << "\n";

  stream >> csbuf;
  cout << csbuf;

  stream.Clear();
  stream.SetWidth(4); stream.SetFill('0');
  stream << "64-bit test = " << hex << LL1 << " " << LL2 << " " 
	 << LL3 << " " << LL4 << "\n";

  cout << stream;

  gxStringw sb64_1, sb64_2, sb64_3, sb64_4;
  sb64_1 << LL1;
  sb64_2 << LL2;
  sb64_3 << LL3;
  sb64_4 << LL4;

  __LLWORD__ LLn1, LLn2;
  __ULLWORD__ LLn3, LLn4;

  sb64_1 >> LLn1;
  sb64_2 >> LLn2;
  sb64_3 >> LLn3;
  sb64_4 >> LLn4;

  cout << "64-bit test = " << (int)LLn1 << " " << (int)LLn2 << " " 
       << (int)LLn3 << " " << (int) LLn4 << "\n";
#endif

  PausePrg();

  cout << "Testing escape sequence conversion function" << "\n";
  cout << "\n";
  gxStringw esc_string(gxLTCHAR(\
"This%20is%20a%20test%20for%20%%%20escape%20chars", wsbuf));
  cout << esc_string.c_str(sbuf) << "\n";
  cout << "Converting escape sequences" << "\n";
  wchar_t es = gxLTCHAR('%', wsbuf);
  int num_esc = esc_string.ConvertEscapeSeq(es);
  cout << "Found " << num_esc << " escape sequences" << "\n"; 
  cout << esc_string.c_str(sbuf) << "\n";
  cout << "\n";

  esc_string = gxLTCHAR("This#0020is#0020a#0020test#0020for#0020###0020escape\
#0020chars", wsbuf);
  cout << esc_string.c_str(sbuf) << "\n";
  cout << "Converting escape sequences" << "\n";
  num_esc = esc_string.ConvertEscapeSeq(gxLTCHAR('#', wsbuf), 4);
  cout << "Found " << num_esc << " escape sequences" << "\n"; 
  cout << esc_string.c_str(sbuf) << "\n";
  cout << "\n";

  esc_string = gxLTCHAR(\
"This;020is;020a;020test;020for;020;;;020escape;020chars", wsbuf);
  cout << esc_string.c_str(sbuf) << "\n";
  cout << "Converting escape sequences" << "\n";
  num_esc = esc_string.ConvertEscapeSeq(';', 3);
  cout << "Found " << num_esc << " escape sequences" << "\n"; 
  cout << esc_string.c_str(sbuf) << "\n";
  
  PausePrg();

  cout << "Testing the comparison operators" << "\n";

#ifndef __AUTORUN_TESTPROG__
  gxStringw acomp(255), bcomp;
  cout << "Enter string A>"; cin >> acomp;
  cout << "Enter string B>"; cin >> bcomp;
#else
  // In auto run mode so do not prompt user for any input
  gxStringw acomp(gxLTCHAR("CAT", wsbuf)), bcomp(gxLTCHAR("cat", wsbuf));
  cout << "String A = CAT, String B = cat" << "\n";
#endif

  cout << "String A == B "; acomp == bcomp ? cout << 'y' : cout << 'n';
  cout << "\n";
  cout << "String A != B "; acomp != bcomp ? cout << 'y' : cout << 'n';
  cout << "\n";
  cout << "String A <  B "; acomp < bcomp ? cout << 'y' : cout << 'n';
  cout << "\n";
  cout << "String A <= B "; acomp <= bcomp ? cout << 'y' : cout << 'n';
  cout << "\n";
  cout << "String A >  B "; acomp > bcomp ? cout << 'y' : cout << 'n';
  cout << "\n";
  cout << "String A >= B "; acomp >= bcomp ? cout << 'y' : cout << 'n';
  cout << "\n";
  
  PausePrg();

  cout << "Testing the case insensitive comparison functions" << "\n";
  cout << "String A == B "; 
  acomp.StringICompare(bcomp) == 0 ? cout << 'y' : cout << 'n';
  cout << "\n";
  cout << "String A != B "; 
  acomp.StringICompare(bcomp) != 0 ? cout << 'y' : cout << 'n';
  cout << "\n";
  cout << "String A <  B "; 
  acomp.StringICompare(bcomp) < 0 ? cout << 'y' : cout << 'n';
  cout << "\n";
  cout << "String A <= B "; 
  acomp.StringICompare(bcomp) <= 0 ? cout << 'y' : cout << 'n';
  cout << "\n";
  cout << "String A >  B "; 
  acomp.StringICompare(bcomp) > 0 ? cout << 'y' : cout << 'n';
  cout << "\n";
  cout << "String A >= B "; 
  acomp.StringICompare(bcomp) >= 0 ? cout << 'y' : cout << 'n';
  cout << "\n";

  PausePrg();

  cout << "Testing the find last function" << "\n";
  gxStringw url(gxLTCHAR("http://www.example.com:80/docs/index.htm", wsbuf));
  cout << "Complete URL: " << url.c_str(sbuf) << "\n"; 
  cout << "Searching for last \"/\"" << "\n";
  offset = url.FindLast(gxLTCHAR("/", wsbuf));
  if(offset != -1) {
    cout << "Found last \"/\" at offset: " << offset << "\n";
  }
  else {
    cout << "Could not find last \"/\"" << "\n";
  }
  cout << "\n";

  cout << "Testing delete before and after functions" << "\n";
  gxStringw ubuf = url;
  ubuf.DeleteBeforeIncluding(gxLTCHAR("://", wsbuf));
  ubuf.DeleteAfterIncluding(gxLTCHAR(":", wsbuf));
  cout << "Complete URL: " << url.c_str(sbuf) << "\n";
  cout << "Server: " << ubuf.c_str(sbuf) << "\n";
  ubuf = url;
  ubuf.DeleteAfter(gxLTCHAR("http", wsbuf));
  cout << "Protocol: " << ubuf.c_str(sbuf) << "\n";
  ubuf = url;
  ubuf.DeleteBeforeLast(gxLTCHAR(":", wsbuf));
  ubuf.DeleteAfterIncluding(gxLTCHAR("/", wsbuf));
  ubuf.DeleteAt(0, 1);
  cout << "Port: " << ubuf.c_str(sbuf) << "\n";
  ubuf = url;
  ubuf.DeleteBeforeIncluding(gxLTCHAR("://", wsbuf));
  ubuf.DeleteBeforeIncluding(gxLTCHAR("/", wsbuf));
  ubuf.DeleteAfterLastIncluding(gxLTCHAR("/", wsbuf));
  ubuf.InsertAt(0, gxLTCHAR("/", wsbuf));
  cout << "Directory: " << ubuf.c_str(sbuf) << "\n";
  ubuf = url;
  ubuf.DeleteBeforeLastIncluding(gxLTCHAR("/", wsbuf));
  cout << "File: " << ubuf.c_str(sbuf) << "\n";
  ubuf.DeleteBefore(gxLTCHAR(".", wsbuf));
  cout << "Dot extension: " << ubuf.c_str(sbuf) << "\n";

  PausePrg();

  cout << "Testing shorthand notation nomenclature" << "\n";
  long by1 = 519;
  unsigned long by2 = 2133;
  gxStringw byte_string;
  byte_string << clear << "Byte value 1 = " << shn << by1 << " bytes " 
	      << "Byte value 2 = " << by2 << " bytes" << "\n";
  cout << byte_string.c_str(sbuf);
  by1 *= (1000 * 1000);
  by2 *= (1000 * 1000);
  byte_string << clear << "Byte value 1 = " << shn << by1 << " bytes " 
	      << "Byte value 2 = " << by2 << " bytes" << "\n";
  cout << byte_string.c_str(sbuf);

#if defined (__64_BIT_DATABASE_ENGINE__)
  __LLWORD__ llby1 = 519;
  __ULLWORD__ llby2 = 2133;
   byte_string << clear << "Byte value 1 = " << shn << llby1 << " bytes " 
	      << "Byte value 2 = " << llby2 << " bytes" << "\n";
  cout << byte_string.c_str(sbuf);
  llby1 *= (1000 * 1000);
  llby2 *= (1000 * 1000);
  byte_string << clear << "Byte value 1 = " << shn << llby1 << " bytes " 
	      << "Byte value 2 = " << llby2 << " bytes" << "\n";
  cout << byte_string.c_str(sbuf);
  llby1 *= 1000;
  llby2 *= 1000;
  byte_string << clear << "Byte value 1 = " << shn << llby1 << " bytes " 
	      << "Byte value 2 = " << llby2 << " bytes" << "\n";
  cout << byte_string.c_str(sbuf);
#endif

  PausePrg();

  cout << "Testing octal conversion functions" << "\n";
  gxStringw o_string;
  int onum1 = 0755;
  int onum2 = 0644;
  int onum3 = 0220;
  o_string << clear << oct << onum1 << " " << onum2 << " " << onum3 << "\n";
  cout << o_string.c_str(sbuf);

#if defined (__64_BIT_DATABASE_ENGINE__)
  __LLWORD__ lonum1 = 0755;
  __LLWORD__ lonum2 = 0644;
  __LLWORD__ lonum3 = 0220;
  o_string << clear << oct << lonum1 << " " << lonum2 << " " << lonum3 << "\n";
  cout << o_string.c_str(sbuf);
#endif

  PausePrg();

  cout << "Testing char trim functions" << "\n";
  gxStringw trim_str = gxLTCHAR("######Test String______", wsbuf);
  cout << "String = " << trim_str.c_str(sbuf) << "\n";
  trim_str.TrimLeading((wchar_t)'#'); 
  trim_str.TrimTrailing((wchar_t)'_');
  cout << "String = " << trim_str.c_str(sbuf) << "\n";

  PausePrg();

  cout << "Testing substring functions" << "\n";
  gxStringw substr(gxLTCHAR("The quick brown fox jumps over the lazy dog", 
			    wsbuf));
  cout << substr.c_str(sbuf) << "\n";
  Index = substr.Find(gxLTCHAR("fox", wsbuf));
  cout << "Testing mid function:" << "\n";
  gxStringw Fox = substr.Mid(Index, 3); // 3 characters for fox
  cout << Fox.c_str(sbuf) << "\n";
  cout << "Testing left function:" << "\n";
  gxStringw The = substr.Left(3); // 3 characters for the
  cout << The.c_str(sbuf) << "\n";
  cout << "Testing right function:" << "\n";
  gxStringw Dog = substr.Right(3); // 3 characters for dog
  cout << Dog.c_str(sbuf) << "\n";


  PausePrg();
  cout << "Testing string parser" << "\n";
  gxStringw input_str = gxLTCHAR("a | b | c | d | e | f | g", wsbuf);
  unsigned num_arr = 0;
  gxStringw delimiter = gxLTCHAR("|", wsbuf);
  unsigned arr_index;
  gxStringw *output_arr = 0;

  cout << "Parsing: " << input_str.c_str(sbuf) << "\n"; 
  output_arr = ParseStrings(input_str, delimiter, num_arr, 1, 1);
  if(output_arr) {
    for(arr_index = 0; arr_index < num_arr; arr_index++) {
      cout << "[" << arr_index << "] " << output_arr[arr_index].c_str(sbuf) << "\n";
    }
  }
  if(output_arr) delete[] output_arr;

  cout << "\n";
  input_str = gxLTCHAR("This is a test with no delimiter", wsbuf);
  cout << "Parsing: " << input_str.c_str(sbuf) << "\n"; 
  output_arr = ParseStrings(input_str, delimiter, num_arr, 1, 1);
  if(output_arr) {
    for(arr_index = 0; arr_index < num_arr; arr_index++) {
      cout << "[" << arr_index << "] " << output_arr[arr_index].c_str(sbuf) << "\n";
    }
  }
  if(output_arr) delete[] output_arr;

  cout << "\n";
  input_str = gxLTCHAR("\"This is a CSV test\",\"One\",\"Two\",\"With a \\, comma\",No Quotes, Spaces ,   Extra spaces   ,,,", wsbuf);
  delimiter = ",";
  cout << "Parsing: " << input_str.c_str(sbuf) << "\n"; 
  output_arr = ParseStrings(input_str, delimiter, num_arr, 1, 1);
  if(output_arr) {
    for(arr_index = 0; arr_index < num_arr; arr_index++) {
      cout << "[" << arr_index << "] " << output_arr[arr_index].c_str(sbuf) << "\n";
    }
  }
  if(output_arr) delete[] output_arr;
  

  cout << "\n";
  cout << "Exiting Unicode string test" << "\n";
  PausePrg();
}

void MixedStringTest()
{
  cout << "Testing ASCII and UNICODE string functions..." << "\n"; 
  PausePrg();

  cout << "Displaying platform dependent byte alignments" << "\n";
  cout << "ASCII byte alignment = " << sizeof(char) << "\n";
  cout << "ASCII bit alignment = " << (sizeof(char) * 8) << "\n";
  cout << "Unicode byte alignment = " << sizeof(wchar_t) 
       << "\n";
  cout << "Unicode bit alignment = " << (sizeof(wchar_t) * 8) 
       << "\n";
  cout << "gxwchar_t byte alignment = " << sizeof(gxwchar_t) 
       << "\n";
  cout << "gxwchar_t bit alignment = " << (sizeof(gxwchar_t) * 8) 
       << "\n";

  PausePrg();

  cout << "Constructing ASCII and Unicode strings" << "\n";
  char sbuf[255];
  wchar_t wsbuf[255];
  const char *mesg = "This is a test";
  wchar_t *umesg = ASCII2Unicode(mesg);
  gxString as(mesg);
  gxStringw wstr1((const wchar_t *)umesg);

  cout << "ASCII string: " << as.c_str() << "\n";
  cout << "Unicode string: " << wstr1.c_str(sbuf) << "\n";
  cout << "\n";
  cout << "Testing string length functions" << "\n";
  cout << "ASCII string length = " << gxstrlen(as.c_str()) << "\n";
  cout << "Unicode string length = " << gxstrlen(wstr1.c_wstr()) << "\n";

  PausePrg();

  cout << "Testing the gxwchar_t strings functions" << "\n";
  gxwchar_t *gxws_t1 = ASCII2gxwchar_t(mesg);
  gxString gxas = gxws_t1;
  gxStringw gxws = gxws_t1;
  cout << "gxwchar_t String: " << (gxStringw)gxws_t1 << "\n";
  cout << "gxwchar_t String: " << (gxString)gxws_t1 << "\n";
  cout << "ASCII string: " << gxas.c_str() << "\n";
  cout << "Unicode string: " << gxws.c_str(sbuf) << "\n";
  cout << "\n";
  cout << "Testing string length functions" << "\n";
  cout << "ASCII string length = " << gxstrlen(gxas.c_str()) << "\n";
  cout << "Unicode string length = " << gxstrlen(gxws.c_wstr()) << "\n";
  cout << "gxwchar_t string length = " << gxstrlen(gxws_t1) << "\n";
  cout << "\n";
  cout << "Testing gxwchar_t string copy, cat, and cmp functions" << endl;
  gxwchar_t gxws_tbuf[255];
  gxwchar_t gxws_t2[255]; gxwchar_t gxws_t3[255]; gxwchar_t gxws_t4[255];
  gxstrcpy(gxws_t2, gxLTCHAR("Cat", gxws_tbuf));
  gxstrcpy(gxws_t3, gxLTCHAR("Dog", gxws_tbuf));
  gxstrcpy(gxws_t4, gxLTCHAR("Cow", gxws_tbuf));
  gxstrcpy(gxws_tbuf, gxws_t2);
  gxstrcat(gxws_tbuf, gxws_t3);
  gxstrcat(gxws_tbuf, gxws_t4);
  cout << (gxStringw)gxws_tbuf << "\n";
  cout << "Comparing \"Cat\" to \"Dog\"" << "\n";
  cout << "String compare = " << gxstrcmp(gxws_t2, gxws_t3); 

  PausePrg();

  cout << "Testing the string class length functions" << "\n";
  cout << "ASCII string length: " << as.length() << "\n";
  cout << "ASCII bytes allocated: " << as.dlength() << "\n";
  cout << "Unicode string length: " << wstr1.length() << "\n";
  cout << "Unicode bytes allocated: " << wstr1.dlength() << "\n";

  PausePrg();

  cout << "Testing standalone string copy functions" << "\n";
  gxstrcpy(sbuf, as.c_str());
  cout << sbuf << "\n";
  gxstrcpy(wsbuf, wstr1.c_wstr());
  cout << (gxStringw)wsbuf << "\n";

  PausePrg();

  cout << "Testing standalone string cat functions" << "\n";
  const char *s1 = "The quick brown";
  const char *s2 = " fox jumps over";
  const char *s3 = " the lazy dog";
  char *comp = StringCat(s1, s2, s3);
  cout << comp << "\n";
  
  wchar_t *ws1 = ASCII2Unicode(s1);
  wchar_t *ws2 = ASCII2Unicode(s2);
  wchar_t *ws3 = ASCII2Unicode(s3);
  wchar_t *wcomp = StringCat(ws1, ws2, ws3);
  cout << (gxStringw)wcomp << "\n";

  PausePrg();

  cout << "Testing the standalone string parser..." << "\n";
  const char *as_strings = "MOUSE DOG CAT LION FLEA COW HORSE";
  wchar_t *ws_strings = ASCII2Unicode(as_strings);
  char as_words[MAXWORDS][MAXWORDLENGTH];
  wchar_t ws_words[MAXWORDS][MAXWORDLENGTH];
  int num_words;
  char as_sep = ' ';
  wchar_t ws_sep = (wchar_t)as_sep;
  int i;
  int rv = parse(as_strings, as_words, &num_words, as_sep);
  if(rv != 0) {
    cout << "ASCII parse error" << "\n";
  }
  else {
    cout << "Parsed " << num_words << " ASCII words" << "\n";
    for(i = 0; i < num_words; i++) {
      if(*as_words[i] == 0) break;
      cout << as_words[i] << ' ';
    }
    cout << "\n";
  }

  parse(ws_strings, ws_words, &num_words, ws_sep);
  if(rv != 0) {
    cout << "Unicode parse error" << "\n";
  }
  else {
    cout << "Parsed " << num_words << " Unicode words" << "\n";
    for(i = 0; i < num_words; i++) {
      if(*ws_words[i] == 0) break;
      cout << (gxStringw)ws_words[i] << ' ';
    }
    cout << "\n";
  }

  PausePrg();

  cout << "Testing standalone string compare functions" << "\n";
  char *acomp = "CAT"; char *bcomp = "cat";
  cout << "String A = CAT, String B = cat" << "\n";
  wchar_t *wacomp = ASCII2Unicode(acomp);
  wchar_t *wbcomp = ASCII2Unicode(bcomp);
  rv = gxstrcmp(acomp, bcomp);
  cout << "ASCII case compare = " << rv << "\n";
  rv = gxstrcmp(wacomp, wbcomp);
  cout << "Unicode case compare = " << rv << "\n";
  rv = CaseICmp(acomp, bcomp);
  cout << "ASCII casei compare = " << rv << "\n";
  rv = CaseICmp(wacomp, wbcomp);
  cout << "Unicode casei compare = " << rv << "\n";

  PausePrg();

  cout << "Testing standalone pattern matching functions" << "\n";
  const char *search_str = "The quick brown fox jumps over the lazy dog";
  wchar_t *wsearch_str = ASCII2Unicode(search_str);
  cout << "Searching for pattern \"fox\"" << "\n";
  int offset = FindMatch(search_str, "fox", 0, 0, 0);
  if(offset != -1) {
    cout << "Found ASCII match at character offset: " << offset << "\n";
  }
  else {
    cout << "Could not find ASCII match" << "\n";
  }
  wchar_t *wpattern = ASCII2Unicode("fox");
  offset = FindMatch(wsearch_str, wpattern);
  if(offset != -1) {
    cout << "Found Unicode match at character offset: " << offset << "\n";
  }
  else {
    cout << "Could not find Unicode match" << "\n";
  }
  cout << "\n";
  cout << "Search using non-case sensitive pattern match" << "\n";
  cout << "Searching for pattern \"FOX\"" << "\n";
  offset = IFindMatch(search_str, "FOX");
  if(offset != -1) {
    cout << "Found ASCII match at character offset: " << offset << "\n";
  }
  else {
    cout << "Could not find ASCII match" << "\n";
  }
  delete wpattern; // Free memory before calling conversion function again
  wpattern = ASCII2Unicode("FOX");
  offset = IFindMatch(wsearch_str, wpattern);
  if(offset != -1) {
    cout << "Found Unicode match at character offset: " << offset << "\n";
  }
  else {
    cout << "Could not find Unicode match" << "\n";
  }

  // Free all memory buffers
  delete umesg;
  delete gxws_t1;
  delete comp; delete wcomp;
  delete ws1; delete ws2; delete ws3;
  delete ws_strings;
  delete wacomp; delete wbcomp;
  delete wsearch_str; delete wpattern;
  
  cout << "\n";
  cout << "Exiting mixed string test" << "\n";
  PausePrg();
}

void NULLStringTest()
{
  cout << "Testing ASCII and UNICODE with NULL strings..." << "\n"; 
  PausePrg();

  wchar_t wsbuf[255];
  char sbuf[255];

  cout << "Testing the NULL string class" << "\n";
  cout << "Global null char = " << (int)UStringNULLPtr::UStringNUllChar 
       << "\n";
  cout << "Global null wchar_t = " << UStringNULLPtr::UStringwNUllChar << "\n";
  cout << "Global null wchar_t address = " 
       << &UStringNULLPtr::UStringwNUllChar << "\n";
  cout << "Global null string address = " << &UStringNULLPtr::UStringNUllStr 
       << "\n";
  cout << "Global null wstring address = " << &UStringNULLPtr::UStringwNUllStr 
       << "\n";

  PausePrg();

  cout << "Testing NULL string construction and manipulation" << "\n";
  // Test the constructors and assignment operators
  gxString nstr; UString nstr2(""); UString nstr3(gxLTCHAR("", wsbuf));
  UStringw nwstr; UStringw nwstr2(""); UStringw nwstr3(gxLTCHAR("", wsbuf));
  nstr = ""; nstr = nstr2; nstr = nstr3; nstr = nwstr3.c_wstr();
  nstr = gxLTCHAR("", wsbuf);
  nwstr = gxLTCHAR("", wsbuf); nwstr = nwstr2; nwstr = nwstr3;
  nwstr = nstr2.c_str();
  cout << "ASCII length = " << nstr.length() << endl;
  cout << "ASCII dimensioned length = " << nstr.dlength() << endl;
  cout << "Unicode length = " << nwstr.length() << endl;
  cout << "Unicode dimensioned length = " << nwstr.dlength() << endl;

  PausePrg();

  cout << "Testing for wild pointers and buffer overflows" << "\n";
  cout << nstr[100]; cout << (char)nwstr[100];
  cout << nstr.c_str(); cout << gxLTCHAR(nwstr.c_wstr(), sbuf);
  cout << gxLTCHAR(nstr.c_wstr(wsbuf), sbuf);
  cout << nwstr.c_str(sbuf);
  nstr.DeleteAt(0, 100);
  nstr += "";
  nstr.ReplaceAt(0, "");
  nstr.InsertAt(0, "");
  nstr.Atoi();
  nstr.Atol();
  nstr.Atof();
  nstr.ToUpper();
  nstr.ToLower();
  nstr.FindLast("");
  nstr.ReplaceChar('\0', '\0');
  nstr.ReplaceString("", "");
  nstr.FilterString("");
  nstr.FilterChar('\0');
  nstr.TrimLeadingSpaces();
  nstr.TrimTrailingSpaces();
  nstr.DeleteAfter("");
  nstr.DeleteBefore("");
  nstr.DeleteAfterIncluding("");
  nstr.DeleteBeforeIncluding("");
  nstr.DeleteAfterLast("");
  nstr.DeleteAfterLastIncluding("");
  nstr.DeleteBeforeLast("");
  nstr.DeleteBeforeLastIncluding("");
  nstr.ConvertEscapeSeq('%');
  nstr.Find("");
  nstr.Find("", 100, 100);
  nstr.IFind("");
  nstr.IFind("", 100, 100);
  nwstr.DeleteAt(0, 100);
  nstr += "";
  nwstr.ReplaceAt(0, "");
  nwstr.InsertAt(0, "");
  nwstr.Atoi();
  nwstr.Atol();
  nwstr.Atof();
  nwstr.ToUpper();
  nwstr.ToLower();
  nwstr.FindLast("");
  nwstr.ReplaceChar('\0', '\0');
  nwstr.ReplaceString("", "");
  nwstr.FilterString("");
  nwstr.FilterChar('\0');
  nwstr.TrimLeadingSpaces();
  nwstr.TrimTrailingSpaces();
  nwstr.DeleteAfter("");
  nwstr.DeleteBefore("");
  nwstr.DeleteAfterIncluding("");
  nwstr.DeleteBeforeIncluding("");
  nwstr.DeleteAfterLast("");
  nwstr.DeleteAfterLastIncluding("");
  nwstr.DeleteBeforeLast("");
  nwstr.DeleteBeforeLastIncluding("");
  nwstr.ConvertEscapeSeq('%');
  nwstr.Find("");
  nwstr.Find("", 100, 100);
  nwstr.IFind("");
  nwstr.IFind("", 100, 100);
  cout << "\n";
  cout << "Passed" << "\n";

  PausePrg();

  cout << "Testing comparison operator on null strings" << "\n";
  cout << "ASCII length = " << nstr.length() << endl;
  cout << "ASCII dimensioned length = " << nstr.dlength() << endl;
  cout << "Unicode length = " << nwstr.length() << endl;
  cout << "Unicode dimensioned length = " << nwstr.dlength() << endl;
  cout << "ASCII String == null "; nstr == "" ? cout << 'y' : cout << 'n';
  cout << "\n";
  cout << "ASCII String != null "; nstr != "" ? cout << 'y' : cout << 'n';
  cout << "\n";
  cout << "ASCII String <  null "; nstr < "" ? cout << 'y' : cout << 'n';
  cout << "\n";
  cout << "ASCII String A <= B "; nstr <= "" ? cout << 'y' : cout << 'n';
  cout << "\n";
  cout << "ASCII String >  null "; nstr > "" ? cout << 'y' : cout << 'n';
  cout << "\n";
  cout << "ASCII String >= null "; nstr >= "" ? cout << 'y' : cout << 'n';
  cout << "\n";
  
  cout << "Unicode String == null "; 
  nwstr == gxLTCHAR("", wsbuf) ? cout << 'y' : cout << 'n';
  cout << "\n";
  cout << "Unicode String != null "; 
  nwstr != gxLTCHAR("", wsbuf) ? cout << 'y' : cout << 'n';
  cout << "\n";
  cout << "Unicode String <  null "; 
  nwstr < gxLTCHAR("", wsbuf) ? cout << 'y' : cout << 'n';
  cout << "\n";
  cout << "Unicode String A <= B "; 
  nwstr <= gxLTCHAR("", wsbuf) ? cout << 'y' : cout << 'n';
  cout << "\n";
  cout << "Unicode String >  null "; 
  nwstr > gxLTCHAR("", wsbuf) ? cout << 'y' : cout << 'n';
  cout << "\n";
  cout << "Unicode String >= null "; 
  nwstr >= gxLTCHAR("", wsbuf) ? cout << 'y' : cout << 'n';
  cout << "\n";

  // Test the global pointer reset functions
  // NOTE: A reset is only required if the global nulls have
  // been accidentally modified.
  nstr.ResetGlobalNull();
  nwstr.ResetGlobalNull();

  cout << "\n";
  cout << "Exiting NULL string test" << "\n";
  PausePrg();
}

int main()
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

  MixedStringTest();
  NULLStringTest();
  ASCIIStringTest();
  UNICODEStringTest();
  cout << "All tests complete. Exiting..." << "\n";
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
