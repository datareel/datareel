// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: txfilter.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/17/1997
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

Text file filter program used to filter out unwanted characters
from ASCII text files. This program can also be used to convert
DOS text files to a UNIX format of UNIX text files to a DOS
format.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <iostream>

#include <string.h>
#include <stdlib.h> 
#include <stdio.h>
#include <ctype.h>
#include "ustring.h"
#include "gxlist.h"
#include "futils.h"
#include "dfileb.h"
#include "gxd_ver.h"


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
  std::cout << "\n";
  std::cout << "ASCII Text file filter program version "
       << gxuffer  << "\n";
  std::cout << "Usage: " << program_name << " [switches] infile.txt " 
	      << "\n";
  std::cout << "Switches:  -?      = Display this help message." << "\n";
  std::cout << "           -A      = Output alphabetic characters only."
       << "\n";
  std::cout << "           -d      = Output text file in DOS CR/LF format."
       << "\n";
  std::cout << "           -D      = Do not insert line feeds." << "\n";
  std::cout << "           -M      = Output alphanumeric characters only."
       << "\n";
  std::cout << "           -n      = No text filtering (defaults to \
filtered)." << "\n";
  std::cout << "           -N      = Output numeric characters only."
       << "\n";
  std::cout << "           -o      = Write output to the existing file \
(defaults to stdout)." << "\n";
  std::cout << "           -P      = Output printable characters only \
(default)." << "\n";
  std::cout << "           -t      = Filter tabs." << "\n";
  std::cout << "           -s      = Filter spaces." << "\n";
  std::cout << "           -u      = Output text file in UNIX format \
(default)." << "\n";
  std::cout << "\n";
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
      std::cerr << "\n";
      std::cerr << "Unknown switch " << arg << "\n";
      std::cerr << "Exiting..." << "\n";
      std::cerr << "\n";
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
      std::cout << iofile.df_ExceptionMessage() << "\n";
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
      std::cout << list_ptr->data.c_str();
      if(DOS_TEXT_FORMAT) {
	std::cout.write(&carriage_return, 1);
	std::cout.write(&line_feed, 1);
      }
      if(UNIX_TEXT_FORMAT)
	std::cout.write(&line_feed, 1);
      list_ptr = list_ptr->next; 
    }
  }
  else {
    iofile.df_Close(); 
    iofile.df_Create(open_file);
    if(!iofile) {
      std::cerr << "\n";
      std::cerr << "Cannot write to: " << open_file << "\n";
      std::cerr << "Exiting..." << "\n";
      std::cerr << "\n";
      exit(0);
    }

    gxListNode<UString> *list_ptr = list.GetHead();
    while(list_ptr) {
      iofile.df_Write(list_ptr->data.c_str(), list_ptr->data.length());
      if(iofile.df_CheckError()) {
	std::cout << iofile.df_ExceptionMessage() << "\n";
	break;
      }
      if(DOS_TEXT_FORMAT) {
	iofile.df_Write(&carriage_return, 1);
	if(iofile.df_CheckError()) {
	  std::cout << iofile.df_ExceptionMessage() << "\n";
	  break;
	}
	iofile.df_Write(&line_feed, 1);
	if(iofile.df_CheckError()) {
	  std::cout << iofile.df_ExceptionMessage() << "\n";
	  break;
	}
	
      }
      if(UNIX_TEXT_FORMAT) {
	iofile.df_Write(&line_feed, 1);
	if(iofile.df_CheckError()) {
	  std::cout << iofile.df_ExceptionMessage() << "\n";
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
	    std::cerr << "\n";
	    std::cerr << "Cannot open file: " << open_file << "\n";
	    std::cerr << "Exiting..." << "\n";
	    std::cerr << "\n";
	    return 0;
	  }
	  num_files++;
	  if(!write_to_stdio) std::cout << "Processing: " << open_file 
					  << "\n";
	  ConvertTextFile(iofile);
	}
      } 
      arg = argv[++narg];
    }
  }
    
  if(num_files == 0) {
    std::cerr << "\n";
    std::cerr << "You must enter a file name." << "\n";
    std::cerr << "Exiting..." << "\n";
    std::cerr << "\n";
    return 0;
  }
  
  if(!write_to_stdio) {
    std::cout << "\n";
    std::cout << "Processed " << num_files << " files." << "\n";
    std::cout << "\n";
  }

  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
