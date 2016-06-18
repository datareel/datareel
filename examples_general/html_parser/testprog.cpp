// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: testprog.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
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

This program is used to test gxsHTML classes. 
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include <stdlib.h>
#include "gxshtml.h"
#include "gxs_ver.h"

#ifdef __MSVC_DEBUG__
#include "leaktest.h"
#endif

// Version number and program name
const double ProgramVersionNumber = gxSocketVersion;
const char *ProgramName = "gxsHTML test program";

// Program globals
int parsing_tags = 0;
int listing_doc_tags = 0;
int listing_supported_tags = 0;

int CopyToMemory(const char *fname, MemoryBuffer &membuf)
{
  membuf.Clear();
  const unsigned buf_size = 1024;
  char sbuf[buf_size];
  DiskFileB f(fname);
  if(!f) return f.df_GetError();

  while(!f.df_EOF()) {
    if(f.df_Read((char *)sbuf, buf_size) != DiskFileB::df_NO_ERROR) {
      if(f.df_GetError() != DiskFileB::df_EOF_ERROR) 
	return f.df_GetError();
    }
    membuf.Cat((char *)sbuf, f.df_gcount());
  }

  f.df_Close();
  return 0;
}

void ParseTags(const char *fname)
{
  gxsHTML html;
  cout << "\n";
  cout << "Parsing " << fname << "\n";
  if(html.LoadHTMLFile(fname) != 0) {
    html.df_ExceptionMessage();
    return;
  }
  
  cout << "Found " << html.NumTags() << " tags" << "\n";

  cout << "\n";
  cout << "Parsing in-memory copy of " << fname << "\n";
  MemoryBuffer membuf;
  if(CopyToMemory(fname, membuf) != 0) {
    html.df_ExceptionMessage();
    return;
  }

  if(html.LoadMemoryBuffer(membuf) != 0) {
    cout << "A memory error occurred" << "\n";
    return;
  }
  cout << "Found " << html.NumTags() << " tags" << "\n";
  cout << "Found " << html.NumScripts() << " scripts" << "\n";
  cout << "Found " << html.NumStyleSheets() << " style sheets" << "\n";

  cout << "\n";
}

void PrintTagInfo(const gxsHTMLTagInfo &t)
{
  cout << "----- Tag's file information-----" << "\n";
  cout << "Tag file position (start): " << (long)t.start_tag << "\n";
  cout << "Tag file position (end):   " << (long)t.end_tag << "\n";
  cout << "Tag's total length <---->: " << t.tag_length << "\n";
  cout << "----- Tag Infomation ------" << "\n";
  cout << "Tag ID----: " << t.tag_id << "\n";
  cout << "Tag ------: " << t.tag.c_str() << "\n";
  cout << "Attributes: " << t.attr.c_str() << "\n";
  cout << "Original tag: " << t.tag_info.c_str() << "\n";

  cout << "----- Tag instructions -----" << "\n";
  if(t.start_instruction) cout << "Start of tag instruction" << "\n";
  if(t.end_instruction) cout << "End of tag instruction" << "\n";
  if(t.has_attributes) cout << "This tag has attributes associated with it"
			  << "\n";
  cout << "\n";
}

void ListSupportedTags()
{
  gxsHTML html;
  for(int i = 0; i < gxsMAX_SUPPORTED_TAGS; i++) {
    cout << "<" << html.GetTag(i) << ">" << "\n";
  }
}

void ListDocTags(const char *fname)
{
  gxsHTML html;
  html.LoadHTMLFile(fname);
  if(!html) {
    cout << "Could not open the " << fname << " file!" << "\n";
    return;
  }

  cout << "Found " << html.NumTags() << " tags" << "\n";
  cout << "Found " << html.NumScripts() << " scripts" << "\n";
  cout << "Found " << html.NumStyleSheets() << " style sheets" << "\n";
  cout << "Press enter to continue" << "\n";
  cin.get();

  gxList<gxsHTMLTagInfo> *tag_list = html.GetTagList();
  gxListNode<gxsHTMLTagInfo> *list_ptr = tag_list->GetHead();
  while(list_ptr) {
    PrintTagInfo(list_ptr->data);
    list_ptr = list_ptr->next; 
  }

  cout << "End of tags, listing scripts" << "\n";
  cout << "Press enter to continue" << "\n";
  cin.get();

  gxList<gxsScriptInfo> *script_list = html.GetScriptList();
  gxListNode<gxsScriptInfo> *script_ptr = script_list->GetHead();
  while(script_ptr) {
    cout << "\n";
    cout << script_ptr->data.script_code.c_str();
    cout << "\n";
    script_ptr = script_ptr->next; 
  }

  cout << "End scripts listing embedded style sheets" << "\n";
  cout << "Press enter to continue" << "\n";
  cin.get();

  gxList<gxsStyleInfo> *style_sheet_list = html.GetStyleSheetList();
  gxListNode<gxsStyleInfo> *style_sheet_ptr = style_sheet_list->GetHead();
  while(style_sheet_ptr) {
    cout << "\n";
    cout << style_sheet_ptr->data.style_code.c_str();
    cout << "\n";
    style_sheet_ptr = style_sheet_ptr->next; 
  }
}

void HelpMessage(const char *program_name, const double version_number)
{
  cout << "\n";
  cout.setf(ios::showpoint | ios::fixed);
  cout.precision(3);
  cout << "\n";
  cout << program_name << " version " << version_number  << "\n";
  cout << "Usage: " << program_name << " [switches] infile.html" << "\n";
  cout << "Example: " << program_name << " -l index.html" << "\n";
  cout << "Switches: " << "\n";
  cout << "          -l = List all the tags found in an HTML document." 
       << "\n";
  cout << "          -L = List all the tags supported by this program." 
       << "\n";
  cout << "          -p = Parse all the supported tags in an HTML document."
       << "\n";
  cout << "\n";
  return;
}

int ProcessArgs(char *arg)
{
  switch(arg[1]) {
    case 'l' :
      parsing_tags = 0;
      listing_doc_tags = 1;
      break;
      
    case 'L' :
      ListSupportedTags();
      return 0;

    case 'p' :
      parsing_tags = 1;
      listing_doc_tags = 0;
      break;

    default:
      cerr << "\n";
      cerr << "Unknown switch " << arg << "\n";
      cerr << "Exiting..." << "\n";
      cerr << "\n";
      return 0;
  }
  arg[0] = '\0';
  return 1; // All command line arguments were valid
}

// Program's main thread of execution.
// ----------------------------------------------------------- 
int main(int argc, char **argv)
// NOTE: None of the MSVC compilers will expand wildcard characters
// used in command-line arguments unless linked with the setargv.obj
// library. All the UNIX compilers will expand wildcard characters
// by default.
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

  if(argc < 2) { 
    HelpMessage(ProgramName, ProgramVersionNumber);
    return 0;
  }
  
  // Process command ling arguments and files 
  int narg;
  char *arg = argv[narg = 1];

  while (narg < argc) {
    if(arg[0] != '\0') {
      if(arg[0] == '-') { // Look for command line arguments
	if(!ProcessArgs(arg)) return 0; // Exit if argument is not valid
      }
      else { 
	if(parsing_tags) {
	  ParseTags((const char *)arg);
	}
	else if(listing_doc_tags) {
	  ListDocTags((const char *)arg);
	}
	else {
	  cout << "No operation specified!" << "\n";
	  cout << "Exiting..." << "\n";
	}
      }
      arg = argv[++narg];
    }
  }

  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
