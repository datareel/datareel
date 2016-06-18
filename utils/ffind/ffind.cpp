// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: ffind.cpp 
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 03/09/1999 
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
  
This program is used to find and replace strings in a file.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "strutil.h"
#include "gxstring.h"
#include "gxlist.h"
#include "dfileb.h"
#include "futils.h"
#include "gxd_ver.h"

#ifdef __MSVC_DEBUG__
#include "leaktest.h"
#endif

// Version number and program name
gxString VersionString = "4.40";
gxString ProgramName = "ffind";

// Program global
const int MAX_LINE = 1024;   // Maximum characters per line
char *open_file = 0;         // Name of file currently opened
unsigned num_files = 0;      // Total number of files processed
gxString string_to_find;     // String to find in each line
int find_string = 0;         // Find specified string flag
int find_all = 0;            // Find all occurrences
int check_case = 1;          // Case sensitive/insensitive compare flag
int replace_once = 0;        // Replace one time only
unsigned num_matches = 0;    // Total number of matches found
int cat_file = 0;            // Concatenate file flag
gxString string_to_replace;  // String to replace
gxString string_to_insert;   // String to insert
int replacing_string = 0;    // Replace string flag
int num_replaced = 0;        // Number of strings replaced
int replacing_line = 0;      // Replace line flag
int inserting_string = 0;    // Insert line flag
int append_lines = 1;        // Append lines connected with a backslash
int num_processed = 0;       // Number of string processed per file
int terminate_line = 0;
int surround_by = 0;
gxString surround_by_str;
gxString terminate_str;
gxString input_file_name;
gxString input_string;
int replace_section = 0;
gxString replace_file_name;
gxString replace_string;
gxString trim_leading_char;
gxString trim_trailing_char;
int no_escape_seq = 0;

// Functions
void HelpMessage();
void VersionMessage();
int ProcessArgs(char *arg);
int ProcessTextFile(DiskFileB &iofile, GXSTD::ostream &stream);
int ReplaceBySection(DiskFileB &iofile, GXSTD::ostream &stream);
int InsertEscapeSeq(gxString &s);
int FixInputStrings();

// Program's main thread of execution.
// ----------------------------------------------------------- 
int main(int argc,   // Number of strings in array argv.
	 char *argv[], // Array of command-line argument strings.
	 char *envp[]) // Array of environment variable strings.
// NOTE: None of the MSVC compilers will expand wildcard characters
// used in command-line arguments unless linked with the setargv.obj
// library. All the UNIX compliers will expand wildcard characters
// by default.
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

  // If no arguments are given print usage message to the screen 
  if(argc < 2) {
    HelpMessage();
    return 0;
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
	if(futils_isdirectory((const char *)arg)) {
	  // Do not process directories
	  arg = argv[++narg];
	  continue;
	}
	open_file = arg; // Update the open file name pointer
	DiskFileB iofile(open_file);
	if(!iofile) { // Skip over directory names
	  if(narg < argc) {
	    arg = argv[++narg];
	    iofile.df_Close();
	    continue; 
	  }
	  else { // Cannot open the specified file
	    GXSTD::cerr << "\n";
	    GXSTD::cerr << "Cannot open file: " << open_file << "\n";
	    GXSTD::cerr << "Exiting..." << "\n";
	    GXSTD::cerr << "\n";
	    return 0;
	  }
	}
	num_files++;

	if(replace_section) {
	  num_matches = 0; num_replaced = 0; num_processed = 0;
	  if(!ReplaceBySection(iofile, GXSTD::cout)) return 0;
	  arg = argv[++narg];
	  if(num_files) iofile.df_Close();
	  continue;
	}

	// Echo the name of the file and search operation
	// ----------------------------------------------------------- 
	if(replacing_string == 1) {
	  if(string_to_insert.is_null()) {
	    GXSTD::cerr << "\n";
	    GXSTD::cerr << "The -R option must be used with the -W option." 
			<< "\n";
	    GXSTD::cerr << "Usage: -R\"string\" -W\"replacement\"" << "\n";
	    GXSTD::cerr << "\n";
	    return 0;
	  }
	  num_processed = 0; // Number of string processed for this file
	  GXSTD::cout << "\n";
	  GXSTD::cout << "Opening file: " << open_file << "\n";
	  GXSTD::cout << "Replacing: " << string_to_replace.c_str() << "\n";
	  GXSTD::cout << "With: " << string_to_insert.c_str() << "\n";
	}

	if(replacing_line) {
	  if(string_to_insert.is_null()) {
	    GXSTD::cerr << "\n";
	    GXSTD::cerr << "The -L option must be used with the -W option." 
			<< "\n";
	    GXSTD::cerr << "Usage: -L\"string\" -W\"replacement\"" << "\n";
	    return 0;
	  }
	  num_processed = 0; // Number of string processed for this file
	  GXSTD::cout << "\n";
	  GXSTD::cout << "Opening file: " << open_file << "\n";
	  GXSTD::cout << "Replacing line after: " << string_to_replace.c_str() << "\n";
	  GXSTD::cout << "With: " << string_to_insert.c_str() << "\n";
	}

	if(inserting_string ) {
	  if(string_to_insert.is_null()) {
	    GXSTD::cerr << "\n";
	    GXSTD::cerr << "The -I option must be used with the -W option." 
			<< "\n";
	    GXSTD::cerr << "Usage: -I\"string\" -W\"insert\"" << "\n";
	    GXSTD::cerr << "\n";
	    return 0;
	  }
	  num_processed = 0; // Number of string processed for this file
	  GXSTD::cout << "\n";
	  GXSTD::cout << "Opening file: " << open_file << "\n";
	  GXSTD::cout << "Inserting: " << string_to_replace.c_str() << "\n";
	  GXSTD::cout << "After: " << string_to_insert.c_str() << "\n";
	}

	if(find_string) {
	  num_processed = 0; // Number of string processed for this file
	  // Do not echo file status to the console
	}

	// Process the test file
	num_matches = 0; num_replaced = 0; num_processed = 0;
	ProcessTextFile(iofile, GXSTD::cout);
	if(num_files) iofile.df_Close();
	
	// Report the total number of strings replaced/inserted 
	// ----------------------------------------------------------- 
	if(replacing_string) {
	  GXSTD::cout << "Replaced " << num_processed << " occurrence(s)." 
		      << "\n";
	  GXSTD::cout << "\n";
	}

	if(replacing_line) {
	  GXSTD::cout << "Replaced " << num_processed << " occurrence(s)." 
		      << "\n";
	  GXSTD::cout << "\n";
	}

	if(inserting_string ) {
	  GXSTD::cout << "Inserted " << num_processed << " line(s)." << "\n";
	  GXSTD::cout << "\n";
	}

	if(find_string) {
	  if(find_all) {
	    if(num_processed > 0) {
	      GXSTD::cout << "\n";
	      GXSTD::cout << "File name: " << open_file << " String: " 
			  << string_to_find.c_str() << "\n";
	      if(num_processed > 1) 
		GXSTD::cout << "Found " << num_processed << " matches" << "\n";
	      else
		GXSTD::cout << "Found 1 match" << "\n";
	    }
	  }
	  if(num_matches == 0) { // Used by -f command if search fails
	    // Do not echo results to the console
	  }
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
  
  // Return the total number of strings replaced/inserted 
  // ----------------------------------------------------------- 
  if(replacing_string) return num_replaced;
  if(replacing_line) return num_replaced;
  if(inserting_string ) return num_replaced;
  if(find_string) return num_matches;

  return 0;
}

void VersionMessage()
{
  GXSTD::cout << "\n";
  GXSTD::cout << ProgramName.c_str() << " program version "
	      << VersionString.c_str()  << "\n";
}

void HelpMessage()
{
  VersionMessage();
  GXSTD::cout << "Usage: " << ProgramName.c_str() << " [switches] iofile.txt " 
	      << "\n"; 
  GXSTD::cout << "Switches: " << "\n";
  GXSTD::cout << "          -a = Do not append lines connected by a backslash"
	      << "\n";
  GXSTD::cout << "          -c = Concatenate file to stdout." << "\n";
  GXSTD::cout << "          -f = Find first occurrence of specified string: "
	      << "-f\"string\"" << "\n";
  GXSTD::cout << "          -F = Find all occurrences of specified string: "
	      << "-F\"string\"" << "\n";
  GXSTD::cout << "          -i = Perform case insensitive compare." << "\n";
  GXSTD::cout << "\n";
  GXSTD::cout << "          -R -W = Replace all occurrences of a string in a file." << "\n";
  GXSTD::cout << "          Usage: -R\"string\" -W\"replacement\"" << "\n";
  GXSTD::cout << "\n";
  GXSTD::cout << "          -L -W = Replace entire line after the specified string." << "\n";
  GXSTD::cout << "          Usage: -L\"string\" -W\"replacement\"" << "\n";
  GXSTD::cout << "\n";
  GXSTD::cout << "          -I -W = Insert a line after the specified string."
	      << "\n";
  GXSTD::cout << "          Usage: -I\"string\" -W\"insert\"" << "\n";
  GXSTD::cout << "\n";
  GXSTD::cout << "Press Enter for advanced options...";
  GXSTD::cin.get();
  GXSTD::cout << "          Advanced options: " << "\n";
  GXSTD::cout << "          --infile=\"textfile.txt\"" << "         Supply file with replacement text" << "\n";
  GXSTD::cout << "          --TrimLeading=\'char\'" << "            Filter char from input strings" << "\n";
  GXSTD::cout << "          --TrimTrailing=\'char\'" << "           Filter char from input strings" << "\n";
  GXSTD::cout << "          --SurroundBy=Quotes" << "             Put double quotes around replacement" << "\n";
  GXSTD::cout << "          --SurroundBy=SingleQuotes" << "       Put single quotes around replacement" << "\n";
  GXSTD::cout << "          --SurroundBy=\"string\"" << "           Put string around replacement" << "\n";
  GXSTD::cout << "          --TerminateWith=\"string\"" << "        End replacement with string" << "\n";
  GXSTD::cout << "          --ReplaceOnce" << "                   Replace string only once" << "\n";
  GXSTD::cout << "          --ReplaceSection" << "                Section replace input string" << "\n";
  GXSTD::cout << "          --ReplaceSection=\"textfile.txt\"" << " Supply file with text to replace" << "\n";
  GXSTD::cout << "          --NoEscapeSeq" << "                   Do not interpret escape sequences" << "\n";
  GXSTD::cout << "\n";
  GXSTD::cout << "Escape sequence chars: \\n, \\r, \\t, %20" << "\n"; 
  GXSTD::cout << "\n";
  exit(0);
}

int ProcessArgs(char *arg)
{
  char line_buf[1024];
  gxString dash_arg, dash_option, line;
  int has_dash_arg = 0;
  int num_lines = 0;
  DiskFileB infile;

  switch(arg[1]) {
    case 'a':
      append_lines = 0;
      break;
      
    case 'c':
      cat_file = 1;
      break;

    case 'f':				
      if(arg[2] == '\0') {
	GXSTD::cerr << "\n";
	GXSTD::cerr << "You must enter a string following the -f option." 
		    << "\n";
	GXSTD::cerr << "Example usage: " << ProgramName << " -f\"string\"" 
		    << "\n"; 
	GXSTD::cerr << "\n";
	return 0;
      }
      find_string = 1;
      find_all = 0;
      replacing_line = 0;  
      replacing_string = 0;  
      inserting_string = 0;
      string_to_find = arg+2;
      InsertEscapeSeq(string_to_find);
      break;

    case 'F':				
      if(arg[2] == '\0') {
	GXSTD::cerr << "\n";
	GXSTD::cerr << "You must enter a string following the -F option." 
		    << "\n";
	GXSTD::cerr << "Example usage: " << ProgramName << " -F\"string\"" 
		    << "\n"; 
	GXSTD::cerr << "\n";
	return 0;
      }
      find_string = 1;
      find_all =1;
      replacing_line = 0;  
      replacing_string = 0;  
      inserting_string = 0;
      string_to_find = arg+2;
      InsertEscapeSeq(string_to_find);
      break;

    case 'i':
      check_case = 0;
      break;

    case 'R':
      if(arg[2] == '\0') {
	GXSTD::cerr << "\n";
	GXSTD::cerr << "You must enter a string following the -R option." 
		    << "\n";
	GXSTD::cerr << "Example usage: " << ProgramName << " -R\"string\"" 
		    << "\n"; 
	GXSTD::cerr << "\n";
	return 0;
      }
      string_to_replace = arg+2;
      replacing_string = 1;  
      replacing_line = 0;
      inserting_string = 0;
      find_string = 0;
      find_all = 0;
      InsertEscapeSeq(string_to_replace);
      break;

    case 'L':
      if(arg[2] == '\0') {
	GXSTD::cerr << "\n";
	GXSTD::cerr << "You must enter a string following the -L option." 
		    << "\n";
	GXSTD::cerr << "Example usage: " << ProgramName << " -L\"string\"" 
		    << "\n"; 	GXSTD::cerr << "\n";
	return 0;
      }
      string_to_replace = arg+2;
      replacing_line = 1;  
      replacing_string = 0;
      inserting_string = 0;
      find_string = 0;
      find_all = 0;
      InsertEscapeSeq(string_to_replace);
      break;

    case 'I':
      if(arg[2] == '\0') {
	GXSTD::cerr << "\n";
	GXSTD::cerr << "You must enter a string following the -I option." 
		    << "\n";
	GXSTD::cerr << "Example usage: " << ProgramName << " -I\"string\"" 
		    << "\n"; 
	GXSTD::cerr << "\n";
	return 0;
      }
      string_to_replace = arg+2;
      InsertEscapeSeq(string_to_replace);
      inserting_string = 1;
      replacing_line = 0;  
      replacing_string = 0;  
      find_string = 0;
      find_all = 0;
      InsertEscapeSeq(string_to_replace);
      break;

    case 'W':
      if(arg[2] == '\0') {
	GXSTD::cerr << "\n";
	GXSTD::cerr << "You must enter a string following the -W option." 
		    << "\n";
	GXSTD::cerr << "Example usage: " << ProgramName << " -W\"string\"" 
		    << "\n"; 
	GXSTD::cerr << "\n";
	return 0;
      }

      if(string_to_replace.is_null()) {
	GXSTD::cerr << "\n";
	GXSTD::cerr << "The -W option must be used with the -R, -L, or -I option." << "\n";
	GXSTD::cerr << "Usage: -R\"string\" -W\"replacement\"" << "\n";
	GXSTD::cerr << "\n";
	return 0;
      }

      string_to_insert = arg+2;
      InsertEscapeSeq(string_to_insert);
      break;
      
    case '-':
      if(arg[2] == '\0') {
	GXSTD::cerr << "\n";
	GXSTD::cerr << "You must enter an option and -- argument." 
		    << "\n";
	GXSTD::cerr << "Example usage: " << ProgramName << " --infile=\"textfile.txt\"" 
		    << "\n"; 
	GXSTD::cerr << "\n";
	return 0;
      }

      dash_arg = arg+2;
      if(dash_arg.Find("=") != -1) {
	dash_option = arg+2;
	dash_option.DeleteAfterIncluding("=");
	dash_arg.DeleteBeforeIncluding("=");
      }
      else {
	dash_option.Clear();
      }

      if(dash_option == "infile") {
	if(dash_option.is_null()) {
	  GXSTD::cerr << "\n";
	  GXSTD::cerr << "Error you must supply a file name with the --infile argument" 
		      << "\n";
	  GXSTD::cerr << "Exiting..." << "\n";
	  GXSTD::cerr << "\n";
	  return 0;
	}
	input_file_name = dash_arg;
	has_dash_arg = 1;
	infile.df_Open(input_file_name.c_str());
	if(infile.df_CheckError()) {
	  GXSTD::cerr << "\n";
	  GXSTD::cerr << "Error opening infile " << input_file_name.c_str() << "\n";
	  GXSTD::cerr << "Exiting..." << "\n";
	  GXSTD::cerr << "\n";
	  return 0;
	}
	string_to_insert.Clear();
	while(!infile.df_EOF()) {
	  memset(line_buf, 0, sizeof(line_buf));
	  infile.df_Read(line_buf, sizeof(line_buf));
	  if(infile.df_GetError() != DiskFileB::df_NO_ERROR) {
	    if(infile.df_GetError() != DiskFileB::df_EOF_ERROR) {
	      GXSTD::cerr << "Error reading from the text file" << "\n";
	      GXSTD::cerr << infile.df_ExceptionMessage() << "\n";
	      GXSTD::cerr << "\n";
	      GXSTD::cerr << "Error reading infile " << input_file_name.c_str() << "\n";
	      GXSTD::cerr << "Exiting..." << "\n";
	      GXSTD::cerr << "\n";
	      return 0;
	    }
	  }
	  string_to_insert.Cat(line_buf, infile.df_gcount());
	} 
	infile.df_Close();
	string_to_insert.ReplaceString("\r\n", "\n");
	InsertEscapeSeq(string_to_insert);
      }

      if(dash_option == "SurroundBy") {
	surround_by = 1;
	has_dash_arg = 1;
	if(dash_option.is_null()) {
	  surround_by_str = "\""; // Assume double quotes
	}
	else {
	  if(dash_arg == "Quotes") {
	    surround_by_str = "\"";
	  }
	  else if(dash_arg == "SingleQuotes") {
	    surround_by_str = "\'";
	  }
	  else {
	    surround_by_str = dash_arg;
	  }
	}
      }

      if(dash_option == "TerminateWith") {
	if(dash_option.is_null()) {
	  GXSTD::cerr << "\n";
	  GXSTD::cerr << "Error you must supply a string with the --TerminateWith argument" 
		      << "\n";
	  GXSTD::cerr << "Exiting..." << "\n";
	  GXSTD::cerr << "\n";
	  return 0;
	}
	has_dash_arg = 1;
	terminate_line = 1;
	terminate_str = dash_arg;
	InsertEscapeSeq(terminate_str);
      }
      
      if(dash_option == "TrimLeading") {
	if(dash_option.is_null()) {
	  GXSTD::cerr << "\n";
	  GXSTD::cerr << "Error you must supply a string with the --TrimLeading argument" 
		      << "\n";
	  GXSTD::cerr << "Exiting..." << "\n";
	  GXSTD::cerr << "\n";
	  return 0;
	}
	has_dash_arg = 1;
	trim_leading_char << dash_arg;
	InsertEscapeSeq(trim_leading_char);
      }

      if(dash_option == "TrimTrailing") {
	if(dash_option.is_null()) {
	  GXSTD::cerr << "\n";
	  GXSTD::cerr << "Error you must supply a string with the --TrimTrialing argument" 
		      << "\n";
	  GXSTD::cerr << "Exiting..." << "\n";
	  GXSTD::cerr << "\n";
	  return 0;
	}
	has_dash_arg = 1;


	trim_trailing_char << dash_arg;
	InsertEscapeSeq(trim_trailing_char);
      }

      if(dash_option == "ReplaceSection") {
	replace_section = 1;
	has_dash_arg = 1;
	if(!dash_arg.is_null()) {
	  replace_file_name = dash_arg;
	  if(!replace_file_name.is_null()) {
	    infile.df_Open(replace_file_name.c_str());
	    if(infile.df_CheckError()) {
	      GXSTD::cerr << "\n";
	      GXSTD::cerr << "Error opening replace section file " << replace_file_name.c_str() << "\n";
	      GXSTD::cerr << "Exiting..." << "\n";
	      GXSTD::cerr << "\n";
	      return 0;
	    }
	    string_to_replace.Clear();
	    while(!infile.df_EOF()) {
	      memset(line_buf, 0, sizeof(line_buf));
	      infile.df_Read(line_buf, sizeof(line_buf));
	      if(infile.df_GetError() != DiskFileB::df_NO_ERROR) {
		if(infile.df_GetError() != DiskFileB::df_EOF_ERROR) {
		  GXSTD::cerr << "Error reading from the text file" << "\n";
		  GXSTD::cerr << infile.df_ExceptionMessage() << "\n";
		  GXSTD::cerr << "\n";
		  GXSTD::cerr << "Error reading replace section file " << replace_file_name.c_str() << "\n";
		  GXSTD::cerr << "Exiting..." << "\n";
		  GXSTD::cerr << "\n";
		}
	      }
	      string_to_replace.Cat(line_buf, infile.df_gcount());
	    } 
	    infile.df_Close();
	    string_to_replace.ReplaceString("\r\n", "\n");
	    InsertEscapeSeq(string_to_replace);
	  }
	}
      }

      if(dash_arg == "version") {
	VersionMessage();
	return 0;
      }

      if(dash_arg == "help") {
	HelpMessage();
	return 0;
      }

      if(dash_arg == "ReplaceOnce") {
	replace_once = 1;
	has_dash_arg = 1;
      }

      if(dash_arg == "NoEscapeSeq") {
	no_escape_seq = 1;
	has_dash_arg = 1;
      }

      if(!has_dash_arg) {
	GXSTD::cerr << "\n";
	GXSTD::cerr << "Invalid option and -- argument " << "\n";
	GXSTD::cerr << arg << "\n";
	GXSTD::cerr << "Exiting..." << "\n";
	GXSTD::cerr << "\n";
	return 0;
      }
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

int ProcessTextFile(DiskFileB &iofile, GXSTD::ostream &stream)
{
  FixInputStrings();

  char LineBuffer[df_MAX_LINE_LENGTH];
  gxString LineData;
  gxString appendlineBuffer;
  gxList<gxString> list;
  int rv, offset = 0;
  unsigned long line_number = 0;
  
  while(!iofile.df_EOF()) {
    if(iofile.df_GetLine(LineBuffer, df_MAX_LINE_LENGTH, '\n', 1) != 
       DiskFileB::df_NO_ERROR) {
      break; // Error reading from the disk file
    }
    offset = 0; // Reset string offset
    LineData.Clear(); // Reset the string buffer
    line_number++;
    
    // Detect backslashes used to mark the continuation of a line
    if((LineBuffer[strlen(LineBuffer)-1] == '\\') && (append_lines == 1)) {
      appendlineBuffer += LineBuffer;
      appendlineBuffer += '\n'; // Insert LF after the backslash 
      continue;  // Goto the top of loop and append next line
    }
    if(appendlineBuffer.length() > 0) { // Append the next line
      appendlineBuffer += LineBuffer;
      LineData = appendlineBuffer;
      appendlineBuffer.DeleteAt(0, appendlineBuffer.length());
    }
    else {
      LineData = LineBuffer;
    }

    // Routine to cat file
    // ----------------------------------------------------------- 
    if(cat_file) stream << LineData.c_str() << "\n";
    
    // Routine to find a string
    // ----------------------------------------------------------- 
    if((!string_to_find.is_null()) && (find_string != 0)) {
      if(find_all) {
	while(1) {
	  if(check_case)
	    offset = LineData.Find(string_to_find, offset);
	  else
	    offset = LineData.IFind(string_to_find, offset);
	  if(offset == -1) break;
	  num_matches++; num_processed++;
	  offset++;
	}
      }
      else {
	if(check_case)
	  rv = LineData.Find(string_to_find);
	else
	  rv = LineData.IFind(string_to_find);
	if(rv != -1) {
	  num_matches++; num_processed++;
	  GXSTD::cout << "\n";
	  GXSTD::cout << "File name: " << open_file << " String: " 
	       << string_to_find << "\n";
	  GXSTD::cout << "Found match on line: #" << line_number << "\n";
	}
      }
    }

    // Routine to replace a string
    // ----------------------------------------------------------- 
    if(replacing_string) {
      while(1) {
	if(num_replaced == 1) {
	  if(replace_once) break;
	}
	if(check_case) {
	  offset = LineData.Find(string_to_replace, offset);
	}
	else {
	  offset = LineData.IFind(string_to_replace, offset);
	}
	if(offset == -1) break;
	LineData.DeleteAt(offset, string_to_replace.length());
	LineData.InsertAt(offset, string_to_insert);
	num_replaced++;  // Update global replacement counter
	num_processed++; // Record number of string processed for this file
	offset += string_to_insert.length();
      }
      list.Add(LineData); // Store all the lines
    }
    
    // Routine to replace a line
    // ----------------------------------------------------------- 
    if(replacing_line) {
      if((num_replaced == 1) && (replace_once)) {
	list.Add(LineData); // Store all the lines 
      }
      else {
	if(check_case) {
	  offset = LineData.Find(string_to_replace, offset);
	}
	else {
	  offset = LineData.IFind(string_to_replace, offset);
	}
	if(offset != -1) {
	  offset += string_to_replace.length();
	  LineData.DeleteAt(offset, (LineData.length() - offset));
	  LineData.InsertAt(offset, string_to_insert);
	  num_replaced++;  // Update global replacement counter
	  num_processed++; // Record number of string processed for this file
	}
	
	list.Add(LineData); // Store all the lines 
      }
    }
    
    // Routine to insert a line
    // ----------------------------------------------------------- 
    if(inserting_string) {
      if((num_replaced == 1) && (replace_once)) {
	list.Add(LineData); // Store all the lines 
      }
      else {
	if(check_case) {
	  offset = LineData.Find(string_to_replace, offset);
	}
	else {
	  offset = LineData.IFind(string_to_replace, offset);
	}
	if(offset != -1) {
	  offset += string_to_replace.length();
	  LineData.InsertAt(offset, string_to_insert);
	  num_replaced++;  // Update global replacement counter
	  num_processed++; // Record number of string processed for this file
	}
	list.Add(LineData); // Store all the lines 
      }
    }
  } 

  // If any strings were replaced in the file, rewrite the file
  // ----------------------------------------------------------- 
  if(num_replaced > 0) {
    iofile.df_Close(); 
    iofile.df_Create(open_file);
    
    if(!iofile) {
      GXSTD::cerr << "\n";
      GXSTD::cerr << "Cannot write to: " << open_file << "\n";
      GXSTD::cerr << "Exiting..." << "\n";
      GXSTD::cerr << "\n";
      exit(0);
    }

    gxListNode<gxString> *list_ptr = list.GetHead();
    while(list_ptr) {
#if defined(__DOS__) || defined (__WIN32__)
      list_ptr->data.ReplaceString("\n", "\r\n");
      iofile << list_ptr->data.c_str() << "\r\n";
#else 
      iofile << list_ptr->data.c_str() << "\n";
#endif

      if(iofile.df_CheckError()) {
	GXSTD::cerr << "\n";
	GXSTD::cerr << "Error writing to: " << open_file << "\n";
	GXSTD::cerr << iofile.df_ExceptionMessage() << "\n";
	GXSTD::cerr << "Exiting..." << "\n";
	GXSTD::cerr << "\n";
	iofile.df_Close();
	exit(0);
      }
      list_ptr = list_ptr->next; 
    }

    iofile.df_Close();
  }

  list.ClearList();
  return 1;
}

int ReplaceBySection(DiskFileB &iofile, GXSTD::ostream &stream)
{
  GXSTD::cerr << "\n";
  GXSTD::cerr << "Replacing section in file " << open_file << "\n";

  if(string_to_insert.is_null()) {
    GXSTD::cerr << "\n";
    GXSTD::cerr << "Error you must an supply input string for section replace" 
		<< "\n";
    GXSTD::cerr << "Exiting..." << "\n";
    GXSTD::cerr << "\n";
    exit(0);
  }

  if(string_to_replace.is_null()) {
    GXSTD::cerr << "\n";
    GXSTD::cerr << "Error you must supply a string to replace" << "\n";
    GXSTD::cerr << "Exiting..." << "\n";
    GXSTD::cerr << "\n";
    exit(0);
  }

  FixInputStrings();

  char line_buf[1024];
  gxString memcopy;

  // Load the file into memory
  memcopy.Clear();
  while(!iofile.df_EOF()) {
    memset(line_buf, 0, sizeof(line_buf));
    iofile.df_Read(line_buf, sizeof(line_buf));
    if(iofile.df_GetError() != DiskFileB::df_NO_ERROR) {
      if(iofile.df_GetError() != DiskFileB::df_EOF_ERROR) {
	GXSTD::cerr << "\n";
	GXSTD::cerr << "Error reading from: " << open_file << "\n";
	GXSTD::cerr << iofile.df_ExceptionMessage() << "\n";
	GXSTD::cerr << "No sections were replaced" << "\n";
	GXSTD::cerr << "\n";
	iofile.df_Close();
	return 0;
      }
    }
    memcopy.Cat(line_buf, iofile.df_gcount());
  }

  memcopy.ReplaceString("\r\n", "\n");
  int offset = 0;
  while(1) {
    if(num_replaced == 1) {
      if(replace_once) break;
    }
    if(check_case) {
      offset = memcopy.Find(string_to_replace, offset);
    }
    else {
      offset = memcopy.IFind(string_to_replace, offset);
    }
    if(offset == -1) break;
    memcopy.DeleteAt(offset, string_to_replace.length());
    memcopy.InsertAt(offset, string_to_insert);
    num_replaced++;  // Update global replacement counter
    num_processed++; // Record number of string processed for this file
    offset += string_to_insert.length();
  }

  if(cat_file) stream << memcopy;

  // If any strings were replaced in the file, rewrite the file
  // ----------------------------------------------------------- 
  if(num_replaced > 0) {
    iofile.df_Close(); 
    iofile.df_Create(open_file);
    
    if(!iofile) {
      GXSTD::cerr << "\n";
      GXSTD::cerr << "Cannot write to: " << open_file << "\n";
      GXSTD::cerr << "No sections were replaced" << "\n";
      GXSTD::cerr << "\n";
      return 0;
    }

#if defined(__DOS__) || defined (__WIN32__)
    memcopy.ReplaceString("\n", "\r\n");
#endif

    if(iofile.df_Write(memcopy.c_str(), memcopy.length()) != 
       DiskFileB::df_NO_ERROR) {
      GXSTD::cerr << "\n";
      GXSTD::cerr << "Error writing to: " << open_file << "\n";
      GXSTD::cerr << iofile.df_ExceptionMessage() << "\n";
      GXSTD::cerr << "No sections were replaced" << "\n";
      GXSTD::cerr << "\n";
      iofile.df_Close();
      return 0;
    }
    iofile.df_Close();
  }

  if(num_replaced == 0) {
    GXSTD::cerr << "No sections were found to replaced" << "\n";
    GXSTD::cerr << "\n";
  }
  else if(num_replaced == 1) {
    GXSTD::cerr << "Replaced " << num_replaced << " section." 
		<< "\n";
    GXSTD::cerr << "\n";
  }
  else {
    GXSTD::cerr << "Replaced " << num_replaced << " sections." 
		<< "\n";
    GXSTD::cerr << "\n";
  }

  return 1;
}

int InsertEscapeSeq(gxString &s) 
{
  if(no_escape_seq == 1) return 1;
  if(s.Find("\\n") != -1) s.ReplaceString("\\n", "\n");
  if(s.Find("\\r") != -1) s.ReplaceString("\\r", "\r");
  if(s.Find("\\t") != -1) s.ReplaceString("\\t", "\t");
  if(s.Find("%20") != -1) s.ReplaceString("%20", " ");
  return 1;
}

int FixInputStrings()
{
  if(!trim_leading_char.is_null()) {
    int numarray = trim_leading_char.length();
    int i;
    for(i = 0; i < numarray; i++) {
      string_to_find.TrimLeading(trim_leading_char[i]); 
      string_to_replace.TrimLeading(trim_leading_char[i]);
      string_to_insert.TrimLeading(trim_leading_char[i]);
    }
  }

  if(!trim_trailing_char.is_null()) {
    int numarray = trim_trailing_char.length();
    int i;
    for(i = 0; i < numarray; i++) {
      string_to_find.TrimTrailing(trim_trailing_char[i]); 
      string_to_replace.TrimTrailing(trim_trailing_char[i]);
      string_to_insert.TrimTrailing(trim_trailing_char[i]);
    }
  }

  if(surround_by) {
    string_to_insert.InsertAt(0, surround_by_str);
    string_to_insert << surround_by_str;
  }

  // NOTE: The terminate option must be evaluated last
  if(terminate_line) {
    string_to_insert << terminate_str;
  }

  return 1;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
