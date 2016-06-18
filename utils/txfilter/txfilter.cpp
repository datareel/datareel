// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: txfilter.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/17/1997
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

Text file filter program used to filter out unwanted characters
from ASCII text files. This program can also be used to convert
DOS text files to a UNIX format of UNIX text files to a DOS
format.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include <string.h>
#include <stdlib.h> 
#include <stdio.h>
#include <ctype.h>
#include "ustring.h"
#include "gxlist.h"
#include "futils.h"
#include "dfileb.h"
#include "gxd_ver.h"

#ifdef __MSVC_DEBUG__
#include "leaktest.h"
#endif

// Txfilter version number and program name
const double TXFilterVersionNumber = gxDatabaseVersion;

#if defined (__64_BIT_DATABASE_ENGINE__)
const char *ProgramName = "txfilter64";
#else // Use the 32-bit version by default
const char *ProgramName = "txfilter";
#endif

// Program globals
char *open_file = 0;       // Name of file currently opened
unsigned num_files = 0;    // Total number of files processed
int UNIX_TEXT_FORMAT = 1;  // Output text file in UNIX format
int DOS_TEXT_FORMAT = 0;   // Output text file in DOS CR/LF format
int filter_text = 1;       // Filter the text
int printable_chars = 1;   // Output all printable characters only
int alpha_chars = 0;       // Output alphabetic characters only
int alpha_num_chars = 0;   // Output alphanumeric characters only
int numeric_chars = 0;     // Output numeric characters only
const int txLF = 0x0A;     // Line feed
const int txCR = 0x0D;     // Carriage return
int write_to_stdio = 1;    // Write output to stdio 
int filter_spaces = 0;     // Filter spaces
int filter_tabs = 0;       // Filter tabs

// Program functions
int ProcessArgs(char *arg);
void HelpMessage(const char *program_name, const double version_number);
int ConvertTextFile(DiskFileB &iofile);
void FilterChar(char OutputBuffer[df_MAX_LINE_LENGTH], char c);

void HelpMessage(const char *program_name, const double version_number)
{
  char gxuffer[255];
  sprintf(gxuffer, "%.3f", version_number);
  GXSTD::cout << "\n";
  GXSTD::cout << "ASCII Text file filter program version "
       << gxuffer  << "\n";
  GXSTD::cout << "Usage: " << program_name << " [switches] infile.txt " 
	      << "\n";
  GXSTD::cout << "Switches:  -?      = Display this help message." << "\n";
  GXSTD::cout << "           -A      = Output alphabetic characters only."
       << "\n";
  GXSTD::cout << "           -d      = Output text file in DOS CR/LF format."
       << "\n";
  GXSTD::cout << "           -D      = Do not insert line feeds." << "\n";
  GXSTD::cout << "           -M      = Output alphanumeric characters only."
       << "\n";
  GXSTD::cout << "           -n      = No text filtering (defaults to \
filtered)." << "\n";
  GXSTD::cout << "           -N      = Output numeric characters only."
       << "\n";
  GXSTD::cout << "           -o      = Write output to the existing file \
(defaults to stdout)." << "\n";
  GXSTD::cout << "           -P      = Output printable characters only \
(default)." << "\n";
  GXSTD::cout << "           -t      = Filter tabs." << "\n";
  GXSTD::cout << "           -s      = Filter spaces." << "\n";
  GXSTD::cout << "           -u      = Output text file in UNIX format \
(default)." << "\n";
  GXSTD::cout << "\n";
  exit(0);
}

int ProcessArgs(char *arg)
// Process the program's argument list
{
  switch(arg[1]) {
    case '?' :
      HelpMessage(ProgramName, TXFilterVersionNumber);
      break;

    case 'd' :
      DOS_TEXT_FORMAT = 1;   
      UNIX_TEXT_FORMAT = 0;  
      break;
      
    case 'D':
      DOS_TEXT_FORMAT = 0;   
      UNIX_TEXT_FORMAT = 0;  
      break;
	  
    case 'n' :
      filter_text = 0;
      break;

    case 'o' :
      write_to_stdio = 0; // Overwrite existing file
      break;
      
    case 's' :
      filter_spaces = 1;
      break;

    case 't' :
      filter_tabs = 1;
      break;
      
    case 'u' :
      DOS_TEXT_FORMAT = 0;   
      UNIX_TEXT_FORMAT = 1;  
      break;
      
    case 'P' :
      printable_chars = 1;   // Output all printable characters only
      alpha_chars = 0;       // Output alphabetic characters only
      alpha_num_chars = 0;   // Output alphanumeric characters only
      numeric_chars = 0;     // Output numeric characters only
      break;
      
    case 'A' :
      printable_chars = 0;   // Output all printable characters only
      alpha_chars = 1;       // Output alphabetic characters only
      alpha_num_chars = 0;   // Output alphanumeric characters only
      numeric_chars = 0;     // Output numeric characters only
      break;
      
    case 'M' :
      printable_chars = 0;   // Output all printable characters only
      alpha_chars = 0;       // Output alphabetic characters only
      alpha_num_chars = 1;   // Output alphanumeric characters only
      numeric_chars = 0;     // Output numeric characters only
      break;
      
    case 'N' :
      printable_chars = 0;   // Output all printable characters only
      alpha_chars = 0;       // Output alphabetic characters only
      alpha_num_chars = 0;   // Output alphanumeric characters only
      numeric_chars = 1;     // Output numeric characters only
      break;
      
    default:
      GXSTD::cerr << "\n";
      GXSTD::cerr << "Unknown switch " << arg << "\n";
      GXSTD::cerr << "Exiting..." << "\n";
      GXSTD::cerr << "\n";
      return 0;
  }
  arg[0] = '\0';
  return 1; // All command line arguments were valid
}

void FilterChar(char OutputBuffer[df_MAX_LINE_LENGTH], char c)
// Filter out a specified character 
{
  char LineBuffer[df_MAX_LINE_LENGTH];
  unsigned i, j = 0;
  for(i = 0; i < df_MAX_LINE_LENGTH; i++) LineBuffer[i] = '\0';
  strcpy(LineBuffer, OutputBuffer);
  for(i = 0; i < df_MAX_LINE_LENGTH; i++) OutputBuffer[i] = '\0';
  for(i = 0, j = 0; i < df_MAX_LINE_LENGTH; i++) 
    if(LineBuffer[i] != c)  OutputBuffer[j++] = LineBuffer[i];
}

int ConvertTextFile(DiskFileB &iofile)
{
  char LineBuffer[df_MAX_LINE_LENGTH];    // Unfiltered line with no line feeds
  char OutputBuffer[df_MAX_LINE_LENGTH];  // Filtered line of text
  UString LineData;
  //  UString appendlineBuffer;
  gxList<UString> list;
  unsigned i, j = 0;
  
  while(!iofile.df_EOF()) { // Read in the file line by line
    LineData.Clear(); // Clear the string buffer
    
    if(iofile.df_GetLine(LineBuffer, df_MAX_LINE_LENGTH, '\n', 1) != 
       DiskFileB::df_NO_ERROR) {
      GXSTD::cout << iofile.df_ExceptionMessage() << "\n";
      iofile.df_Close();
      return 0;
    }
    
    // Filter each line of text allowing tabs and spaces
    if(filter_text) { // Output specified character sets only
      for(i = 0, j = 0; i < df_MAX_LINE_LENGTH; i++) {
	if(LineBuffer[i] == '\0') break; // Reached the end of the line
	if(printable_chars)
	  if(isgraph(LineBuffer[i]) || LineBuffer[i] == ' ' ||
	   LineBuffer[i] == '\t')
	    OutputBuffer[j++] = LineBuffer[i];
	
	if(alpha_chars)
	  if(isalpha(LineBuffer[i]) || LineBuffer[i] == ' '  ||
	     LineBuffer[i] == '\t')
	    OutputBuffer[j++] = LineBuffer[i];
	
	if(alpha_num_chars)
	  if(isalnum(LineBuffer[i]) || LineBuffer[i] == ' '  ||
	     LineBuffer[i] == '\t')
	    OutputBuffer[j++] = LineBuffer[i];
	
	if(numeric_chars)
	  if(isdigit(LineBuffer[i]) || LineBuffer[i] == ' ' ||
	     LineBuffer[i] == '\t')
	    OutputBuffer[j++] = LineBuffer[i];
      }

      OutputBuffer[j++] = '\0'; // Null terminate the filtered buffer

      // Filter tabs or 
      if(filter_tabs) FilterChar(OutputBuffer, '\t');
      if(filter_spaces) FilterChar(OutputBuffer, ' ');
      LineData = OutputBuffer;
    }
    else
      LineData = LineBuffer;

    list.Add(LineData); // Store the file line by line 
  } 

  iofile.df_Close();

  char line_feed = (char)txLF;
  char carriage_return = (char)txCR;

  if(write_to_stdio) {
    gxListNode<UString> *list_ptr = list.GetHead();
    while(list_ptr) {
      GXSTD::cout << list_ptr->data.c_str();
      if(DOS_TEXT_FORMAT) {
	GXSTD::cout.write(&carriage_return, 1);
	GXSTD::cout.write(&line_feed, 1);
      }
      if(UNIX_TEXT_FORMAT)
	GXSTD::cout.write(&line_feed, 1);
      list_ptr = list_ptr->next; 
    }
  }
  else {
    iofile.df_Close(); 
    iofile.df_Create(open_file);
    if(!iofile) {
      GXSTD::cerr << "\n";
      GXSTD::cerr << "Cannot write to: " << open_file << "\n";
      GXSTD::cerr << "Exiting..." << "\n";
      GXSTD::cerr << "\n";
      exit(0);
    }

    gxListNode<UString> *list_ptr = list.GetHead();
    while(list_ptr) {
      iofile.df_Write(list_ptr->data.c_str(), list_ptr->data.length());
      if(iofile.df_CheckError()) {
	GXSTD::cout << iofile.df_ExceptionMessage() << "\n";
	break;
      }
      if(DOS_TEXT_FORMAT) {
	iofile.df_Write(&carriage_return, 1);
	if(iofile.df_CheckError()) {
	  GXSTD::cout << iofile.df_ExceptionMessage() << "\n";
	  break;
	}
	iofile.df_Write(&line_feed, 1);
	if(iofile.df_CheckError()) {
	  GXSTD::cout << iofile.df_ExceptionMessage() << "\n";
	  break;
	}
	
      }
      if(UNIX_TEXT_FORMAT) {
	iofile.df_Write(&line_feed, 1);
	if(iofile.df_CheckError()) {
	  GXSTD::cout << iofile.df_ExceptionMessage() << "\n";
	  break;
	}
	   
      }
      list_ptr = list_ptr->next; 
    }
    
    iofile.df_Close();
  }
  
  list.ClearList();
  return 1;
}

// Program's main thread of execution.
// ----------------------------------------------------------- 
int main(int argc,   // Number of strings in array argv.
	 char *argv[]) // Array of command-line argument strings.
// NOTE: None of the MSVC compilers will expand wildcard characters
// used in command-line arguments unless linked with the setargv.obj
// library. All the UNIX compliers will expand wildcard characters
// by default.
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

  // If no argument is given print usage message to the screen 1
  if(argc < 2) {
    HelpMessage(ProgramName, TXFilterVersionNumber);
    return(0);
  }

  // Process command ling arguments and files 
  int narg;
  char *arg = argv[narg = 1];
  while (narg < argc) {
    if (arg[0] != '\0') {
      if (arg[0] == '-') { // Look for command line arguments
	if(!ProcessArgs(arg)) return 0; // Exit if argument is not valid
      }
      else { 
	if(futils_isfile((const char *)arg)) {
	  open_file = arg; // Update the open file name pointer
	  DiskFileB iofile(open_file);
	  if(!iofile) {
	    GXSTD::cerr << "\n";
	    GXSTD::cerr << "Cannot open file: " << open_file << "\n";
	    GXSTD::cerr << "Exiting..." << "\n";
	    GXSTD::cerr << "\n";
	    return 0;
	  }
	  num_files++;
	  if(!write_to_stdio) GXSTD::cout << "Processing: " << open_file 
					  << "\n";
	  ConvertTextFile(iofile);
	}
      } 
      arg = argv[++narg];
    }
  }
    
  if(num_files == 0) {
    GXSTD::cerr << "\n";
    GXSTD::cerr << "You must enter a file name." << "\n";
    GXSTD::cerr << "Exiting..." << "\n";
    GXSTD::cerr << "\n";
    return 0;
  }
  
  if(!write_to_stdio) {
    GXSTD::cout << "\n";
    GXSTD::cout << "Processed " << num_files << " files." << "\n";
    GXSTD::cout << "\n";
  }

  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
