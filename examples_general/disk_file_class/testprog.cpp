// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: testprog.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 01/25/2000
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

This is a test program for the DiskFileB class.
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
#include <stdio.h>
#include "dfileb.h"

#ifdef __MSVC_DEBUG__
#include "leaktest.h"
#endif

void PausePrg() 
{
  cout << "\n";
  cout << "Press enter to continue..." << "\n";
  cin.get();
}

void DirectoryTest()
{
  cout << "Testing general purpose directory functions" 
       << "\n";

  const char *dirname = "testdir"; // Directory to be created

  // Directory immediately above the current directory position
  const char *parentdir = "..";    
  
  DiskFileB dir;
  cout << "Creating " << dirname << " directory" <<"\n";
  if(dir.df_mkdir(dirname) != 0) {
    cout << dir.df_ExceptionMessage() << "\n";
    return;
  }

  if((dir.df_Exists(dirname)) && (dir.df_IsDirectory(dirname))) {
    cout << "The " << dirname << " directory was created" << "\n";
  }

  cout << "Changing directory to " << dirname << " directory" <<"\n";
  if(dir.df_chdir(dirname) != 0) {
    cout << dir.df_ExceptionMessage() << "\n";
    return;
  }

  char *pwd[df_MAX_DIR_LENGTH]; char drive_letter;
  if(dir.df_pwd((char *)pwd) == 0) {
    cout << "Present working directory: " << (char *)pwd << "\n";
    if(dir.df_HasDriveLetter((const char *)pwd, drive_letter)) {
      cout << "DOS Drive letter = " << drive_letter << "\n";
    }
  }

  cout << "Changing directory to " << parentdir << " directory" <<"\n";
  if(dir.df_chdir(parentdir) != 0) {
    cout << dir.df_ExceptionMessage() << "\n";
    return;
  }

  if(dir.df_pwd((char *)pwd) == 0) {
    cout << "Present working directory: " << (char *)pwd << "\n";
    if(dir.df_HasDriveLetter((const char *)pwd, drive_letter)) {
      cout << "DOS Drive letter = " << drive_letter << "\n";
    }
  }

  cout << "Removing " << dirname << " directory" <<"\n";
  if(dir.df_rmdir(dirname) != 0) {
    cout << dir.df_ExceptionMessage() << "\n";
    return;
  }
}

void FileTest()
{
  cout << "Testing general purpose file functions"
       << "\n";

  const char *testfile1 = "testfile1.dat";
  const char *testfile2 = "testfile2.dat";
  const char *testfile3 = "testfile3.dat";
  DiskFileB dfile(testfile1, DiskFileB::df_READWRITE, DiskFileB::df_CREATE); 
  if(!dfile) {
    cout << "Could not open the " << testfile1 << "\n";
    return;
  }

  if((dfile.df_Exists(testfile1)) && (dfile.df_IsFile(testfile1))) {
    cout << "The " << testfile1 << " file was opened with read/write access"
	 << "\n";
   }
  
  const char *teststr1 = "The quick brown fox jumps over the lazy dog \
0123456789\r\n";
  
  cout << "Writing " << strlen(teststr1) << " bytes to " << testfile1 << "\n";
  if(dfile.df_Write((const char *)teststr1, strlen(teststr1)) != 0) {
    cout << dfile.df_ExceptionMessage() << "\n";
    return;
  }
  
  cout << testfile1 << " Dynamic file size = " << (int)dfile.df_GetEOF() 
       << "\n";
  dfile.df_Flush();
  cout << testfile1 << " Static file size  = " 
       << (int)dfile.df_FileSize(testfile1) << "\n";
  
  char *sbuf = new char[strlen(teststr1)+1];
  cout << "Reading the file contents..." << "\n";
  if(dfile.df_Read((char *)sbuf, strlen(teststr1), (FAU_t)0) != 0) {
    cout << dfile.df_ExceptionMessage() << "\n";
    delete[] sbuf;
    return;
  }
  sbuf[strlen(teststr1)] = '\0';
  cout << sbuf;
  delete[] sbuf;
  
  dfile.df_Close();
  cout << "Renaming the " << testfile1 << " to " << testfile2 << "\n";
  if(dfile.df_rename(testfile1, testfile2) != 0) {
    cout << dfile.df_ExceptionMessage() << "\n";
    return;
  }
  cout << "Copying " << testfile2 << " to " << testfile3 << "\n";
  if(dfile.df_copy(testfile2, testfile3) != 0) {
    cout << dfile.df_ExceptionMessage() << "\n";
    return;
  }
  cout << "Removing " << testfile2 << " and " << testfile3 << "\n";
  if(dfile.df_remove(testfile2) != 0) {
    cout << dfile.df_ExceptionMessage() << "\n";
    return;
  }
  if(dfile.df_remove(testfile3) != 0) {
    cout << dfile.df_ExceptionMessage() << "\n";
    return;
  }

  // Add routine to change file attributes/permissions here
  // ------------------------------------------------------
}

void RandomSeekTest()
{
  struct fileblock {
    unsigned block_length;
  };

  cout << "Performing random read/write test" << "\n";

  const char *testfile = "testfile.dat";
  DiskFileB dfile(testfile, DiskFileB::df_READWRITE, DiskFileB::df_CREATE);

  if(!dfile) {
    cout << "Could not open the " << testfile << "\n";
    return;
  }
  
  if((dfile.df_Exists(testfile)) && (dfile.df_IsFile(testfile))) {
    cout << "The " << testfile << " file was opened with read/write access"
	 << "\n";
  }
  
  const char *teststr = "The quick brown fox jumps over the lazy dog";

  unsigned i;
  unsigned len = strlen(teststr)+4;
  char *str = new char[len+1];
  for(i = 0; i< len; i++) str[i] = '\0';
  memmove(str, teststr, strlen(teststr));

  const unsigned blocks_to_write = 10;
  FAU_t pos[blocks_to_write];
  fileblock hdr;
  char sbuf[df_MAX_LINE_LENGTH];

  cout << "Writing " << blocks_to_write << " blocks of data..." << "\n";
  for(i = 0; i < blocks_to_write; i++) {
    pos[i] = dfile.df_Tell();  
    sprintf(sbuf, " %u", i);
    memmove(str+strlen(teststr), sbuf, strlen(sbuf));
    str[strlen(teststr)+strlen(sbuf)] = '\0';
    hdr.block_length = sizeof(fileblock) + (strlen(str)-1);
    dfile.df_Write(&hdr,sizeof(fileblock)); 
    dfile.df_Write((char *)str, hdr.block_length);
  }

  cout << "Reading back the blocks..." << "\n";
  PausePrg();

  const int max_lines = 10;
  int lines_printed = 0;
  hdr.block_length = 0;
  int ii;

  for(i = 0; i <  blocks_to_write; i++) {
    for(ii = 0; ii < (int)len; ii++) str[ii] = '\0';
    dfile.df_Read(&hdr, sizeof(fileblock), pos[i]);
    dfile.df_Read((char *)str, hdr.block_length, (pos[i]+sizeof(fileblock)));
    cout << "Block address = " << (long)pos[i] << "\n";
    cout << "Block size = " << hdr.block_length << "\n";
    cout << "Data = " << str << "\n";
    lines_printed++;
    if(lines_printed >= max_lines) {
      PausePrg();
      lines_printed = 0;
    }
  }

  dfile.df_Rewind();
  
  cout << "Reading back the blocks in reverse order..." << "\n";
  PausePrg();

  for(ii = (int)blocks_to_write-1; ii >= 0; ii--) {
    for(i = 0; i < len; i++) str[i] = '\0';
    dfile.df_Read((char *)&hdr, sizeof(fileblock), pos[ii]);
    dfile.df_Read((char *)str, hdr.block_length);
    cout << "Block address = " << (long)pos[ii] << "\n";
    cout << "Block size = " << hdr.block_length << "\n";
    cout << "Data = " << str << "\n";
    lines_printed++;
    if(lines_printed >= max_lines) {
      PausePrg();
      lines_printed = 0;
    }
  }

  delete[] str;
  dfile.df_Close();

  cout << "Removing " << testfile << "\n";
  if(dfile.df_remove(testfile) != 0) {
    cout << dfile.df_ExceptionMessage() << "\n";
    return;
  }
  return;
}

void GetLineTest()
{
  cout << "Testing the read line function" << "\n";

  const char *DOSTextLine = "The quick brown fox jumps over the lazy dog\r\n";
  const char *UNIXTextLine = "The quick brown fox jumps over the lazy dog\n";
  
  cout << "Creating DOS and UNIX text files" << "\n";
  const char *fname1 = "dostext.txt";
  const char *fname2 = "unixtext.txt";

  DiskFileB f1(fname1, DiskFileB::df_READWRITE, DiskFileB::df_CREATE);
  if(!f1) {
    cout << "Error opening text file" << "\n";
    return; 
  }
  DiskFileB f2(fname2, DiskFileB::df_READWRITE, DiskFileB::df_CREATE);
  if(!f2) {
    cout << "Error opening text file" << "\n";
    return; 
  }

  const int num_lines = 10;
  int i;
  cout << "Writing " << num_lines << " lines to the text files" << "\n";
  for(i = 0; i < num_lines; i++) {
    f1.df_Write(DOSTextLine, strlen(DOSTextLine));
    f2.df_Write(UNIXTextLine, strlen(UNIXTextLine));
    if((f1.df_CheckError()) || (f2.df_CheckError())) {
      cout << "Error writing to text file" << "\n";
      return;
    }
  }
  
  cout << "Reading the DOS text file" << "\n";
  PausePrg();
  char sbuf[255];
  int line_number = 1;
  f1.df_Rewind(); // Reset the file pointer to the beginning of the stream
  while(!f1.df_EOF()) {
    f1.df_GetLine(sbuf, sizeof(sbuf));
    if(f1.df_GetError() != DiskFileB::df_NO_ERROR) {
      cout << "Error reading from the text file" << "\n";
      cout << f1.df_ExceptionMessage() << "\n";
      break;
    }
    cout << line_number++ << ' ' << sbuf << "\n";
  } 

  cout << "\n";
  cout << "Reading the UNIX text file" << "\n";
  PausePrg();

  line_number = 1;
  f2.df_Rewind(); // Reset the file pointer to the beginning of the stream
  while(!f2.df_EOF()) { // Loop until EOF error
    f2.df_GetLine(sbuf, sizeof(sbuf));
    if(f2.df_GetError() != DiskFileB::df_NO_ERROR) {
      cout << "Error reading from the text file" << "\n";
      cout << f2.df_ExceptionMessage() << "\n";
      break;
    }
    cout << line_number++ << ' ' << sbuf << "\n";
  }

  // Close and remove the text files
  f1.df_Close(); f2.df_Close();
  f1.df_remove(fname1); f2.df_remove(fname2);
}

void OverLoadTest() 
{
  cout << "Testing overloaded << and >> operators" << "\n";
  cout << "\n";

  const char *fname = "strtest.dat";

  DiskFileB stream(fname, DiskFileB::df_READWRITE, DiskFileB::df_CREATE);
  if(!stream) {
    cout << "Error opening text file" << "\n";
    cout << stream.df_ExceptionMessage() << "\n";
    return; 
  }  

  const char *s1 = "The quick brown fox jumps over the lazy dog";
  char *s2 = "0123456789";
  char c1 = 'A';
  const char c2 = 'B';
  
  cout << "Writing some text to the stream" << "\n";
  stream << s1 << ' ' << s2 << "\n";
  stream << c1 << ' ' << c2 << "\n";


  cout << "Reading back the text from the stream" << "\n";
  stream.df_Rewind();
  char sbuf1[df_MAX_LINE_LENGTH]; char sbuf2[df_MAX_LINE_LENGTH];
  char ch1, ch2, ch3;
  stream >> sbuf1;
  stream >> ch1; stream >> ch2; stream >> ch3;

  // Echo the text to the console
  cout << sbuf1;
  cout << ch1 << ch2 << ch3 << "\n";

  cout << "Writing some integer values to the stream in binary" << "\n";
  FAU_t stream_pos = stream.df_Tell(); // Record the current position
  const short i1 = 1; short i2 = 2;
  const unsigned short i3 = 3; unsigned short i4 = 4;
  const int i5 = 5; int i6 = 6;
  const unsigned int i7 = 7; unsigned int i8 = 8;
  const long i9 = 9; long i10 = 10;
  const unsigned long i11 = 11; unsigned long i12 = 12;

  stream << i1 << i2 << i3 << i4 << i5 << i6 << i7 << i8 << i9
	 << i10 << i11 << i12;

  cout << "Reading the binary integer values from the stream" << "\n";
  stream.df_Seek(stream_pos); // Reset the stream position to the first value
  short in1, in2; unsigned short in3, in4;
  int in5, in6; unsigned int in7, in8;
  long in9, in10; unsigned long in11, in12;
  
  stream >> in1 >> in2 >> in3 >> in4 >> in5 >> in6 >> in7 >> in8 >> in9
	 >> in10 >> in11 >> in12;

  // Echo the values to the console
  cout << in1 << " " << in2 << " " << in3 << " " << in4 << " " << in5 
       << " " << in6 << " " << in7 << " " << in8 << " " << in9 
       << " " << in10 << " " << in11 << " " << in12 << "\n";
  
  cout << "Writing some integer values to the stream in text mode" << "\n";
  stream_pos = stream.df_Tell(); // Record the current position

  // Use decimal and hex text modes 
  stream << text << i1 << " " << i2 << " " << i3 << " " << i4 << " " 
	 << i5 << " " << i6 << " " << i7 << " " << i8 << " " << i9 << " "
	 << i10 << " " << i11 << " " << i12 << "\n" << flush;

  stream.df_SetWidth(2); stream.df_SetFill('0');
  stream << hex << i1 << " " << i2 << " " << i3 << " " << i4 << " " 
	 << i5 << " " << i6 << " " << i7 << " " << i8 << " " << i9 << " "
	 << i10 << " " << i11 << " " << i12 << "\n" << clear;

  cout << "Reading the text strings from the stream" << "\n";
  stream.df_Seek(stream_pos); // Reset the stream position to the first value

  stream >> sbuf1 >> sbuf2;

  // Echo the text to the console
  cout << sbuf1 << "\n" << sbuf2 << "\n";

  cout << "Writing some floating point values to the stream in binary" << "\n";
  stream_pos = stream.df_Tell(); // Record the current position
  float f1 = 1.1011; const float f2 = 2.2022;
  double f3 = 3.3033; const double f4 = 4.4044;

  stream << f1 << f2 << f3 << f4;

  cout << "Reading the binary floating point values from the stream" << "\n";
  stream.df_Seek(stream_pos); // Reset the stream position to the first value
  float fn1, fn2;
  double fn3, fn4;

  stream >> fn1 >> fn2 >> fn3 >> fn4;

  // Echo the values to the console
  cout << fn1 << " " << fn2 << " " << fn3 << " " << fn4 << "\n";

  cout << "Writing some floating point values to the stream in text mode" 
       << "\n";
  stream_pos = stream.df_Tell(); // Record the current position

  stream << text << f1 << " " << f2 << " " << f3 << " " << f4 << "\n";
  stream.df_Precision(4);
  stream << f1 << " " << f2 << " " << f3 << " " << f4 << "\n"<< clear;

  cout << "Reading the text strings from the stream" << "\n";
  stream.df_Seek(stream_pos); // Reset the stream position to the first value

  stream >> sbuf1 >> sbuf2;

  // Echo the text to the console
  cout << sbuf1 << "\n" << sbuf2 << "\n";

#if defined (__64_BIT_DATABASE_ENGINE__)
  PausePrg();

  cout << "Writing some 64-bit integer values to the stream in binary" << "\n";
  stream_pos = stream.df_Tell(); // Record the current position
  __LLWORD__ LL1 = (__LLWORD__)1; const __LLWORD__ LL2 = (__LLWORD__)2;
  __ULLWORD__ LL3 = (__ULLWORD__)3; const __ULLWORD__ LL4 = (__LLWORD__)4;

  stream << LL1 << LL2 << LL3 << LL4;

  cout << "Reading the binary 64-bit integer point values from the stream" 
       << "\n";
  stream.df_Seek(stream_pos); // Reset the stream position to the first value
  __LLWORD__ LLn1, LLn2;
  __ULLWORD__ LLn3, LLn4;

  stream >> LLn1 >> LLn2 >> LLn3 >> LLn4;

  // Echo the values to the console
  cout << (int)LLn1 << " " << (int)LLn2 << " " << (int)LLn3 << " " 
       << (int) LLn4 << "\n";

  cout << "Writing some 64-bit integer values to the stream in text mode" 
       << "\n";
  stream_pos = stream.df_Tell(); // Record the current position

  // Use decimal and hex text modes 
  LL1 = __LLWORD__(9223372036854775808);
  LL3 = __ULLWORD__(18446744073709551615);

  stream << text << LL1 << " " << LL2 << " " << LL3 << " " << LL4 << "\n";

  stream.df_SetWidth(4); stream.df_SetFill('0');
  stream << hex << LL1 << " " << LL2 << " " << LL3 << " " << LL4 << "\n"
	 << clear;

  cout << "Reading the text strings from the stream" << "\n";
  stream.df_Seek(stream_pos); // Reset the stream position to the first value

  stream >> sbuf1 >> sbuf2;

  // Echo the text to the console
  cout << sbuf1 << "\n" << sbuf2 << "\n";
#endif // __64_BIT_DATABASE_ENGINE__

  // Close and remove the text files
  stream.df_Close();
  stream.df_remove(fname);
}

int main()
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

  cout << "\n";

  OverLoadTest();
  PausePrg();

  DirectoryTest();
  PausePrg();

  FileTest();
  PausePrg();

  RandomSeekTest();
  PausePrg();

  GetLineTest();

  cout << "\n";
  cout << "Exiting..." << "\n";
  cout << "\n";
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
