// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: testprog.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 01/25/2001
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

This program is used to test the XML parser.

This is example is using DOM parsing, so the entire XML tree is
loaded into memory. The DataReel XML parser support SAX parsing
and you can define event handlers. SAX parsing does not load 
the tree in memory so you must define event handlers to process
nodes as the file is read by the parser. 
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

#include "gxsxml.h"
#include "gxsrss.h"
#include "gxstring.h"
#include "gxs_ver.h"

#ifdef __MSVC_DEBUG__
#include "leaktest.h"
#endif

// Version number and program name
gxString ProgramVersionNumber;
gxString ProgramName = "gxsXML test program";

// Program globals
gxString program_run_name;
gxString outfile;
int write_test_file = 0;

void HelpMessage()
{
  cout << "\n";
  cout << ProgramName.c_str() << " version " << ProgramVersionNumber  << "\n";
  cout << "Usage: " << program_run_name << " [switches] infile.xml" << "\n";
  cout << "Example: " << program_run_name << " testfile.xml" << "\n";
  cout << "Switches: " << "\n";
  cout << "          -?                = Display this help message and exit." 
       << "\n";
  cout << "          -t                = Build and write a test XML file." 
       << "\n";
  cout << "          -w\"outfile.xml\" = Write a copy of the in-memory tree" 
       << "\n";
  cout << "\n";
  return;
}

int ProcessArgs(int argc, char *argv[])
{
  // process the program's argument list
  int i;
  for(i = 1; i < argc; i++ ) {
    if(*argv[i] == '-') {
      char sw = *(argv[i] +1);
      switch(sw) {
	case '?':
	  HelpMessage();
	  return 0; // Signal program to exit

	case 't':
	  write_test_file = 1;
	  break;

	case 'w':
	  outfile = argv[i]+2;
	  break;
	  
	default:
	  cout << "\n" << flush;
	  cout << "Unknown command line switch " << argv[i] << "\n" << flush;
	  cout << "Exiting..." << "\n" << flush;
	  cout << "\n" << flush;
	  return 0;
      }
    }
  }

  return 1; // All command line arguments were valid
}

void PrintDocInfo(gxsXML &xml);
void PrintPIList(gxsXML &xml);
void PrintDTDList(gxsXML &xml);
void PrintCDATA(gxsXML &xml);
void PrintComments(gxsXML &xml);
void PrintXS(gxsXML &xml);
void PrintTree(gxsXML &xml);
int WriteTestTree(const char *outfile);

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

  // Set this program's runtime name and version number
  program_run_name = argv[0];
  ProgramVersionNumber << clear << gxSocketVersion;

  if(argc < 2) { 
    HelpMessage();
    return 1;
  }

  // Process command ling arguments and files 
  gxString fname;
  int narg;
  char *arg = argv[narg = 1];
  while(narg < argc) {
    if(arg[0] != '\0') {
      if(arg[0] == '-') { // Look for command line arguments
	// Exit if argument is not valid or argument signals program to exit
	if(!ProcessArgs(argc, argv)) return 1; 
      }
      else {
	fname = arg;
      }
    }
    arg = argv[++narg];
  }

  gxsXML xml;

  if(fname.is_null()) {
    if(write_test_file) {
      cout << "No output file name specified on the command line" 
	   << "\n" << flush;
    }
    else {
      cout << "No input file name specified on the command line" 
	   << "\n" << flush;
    }
    return 1;
  }
 
  if(write_test_file) {
    // Write our test tree and exit
    if(!WriteTestTree(fname.c_str())) return 1;
    return 0;
  }

  // Read and parse an existing XML file
  cout << "Loading XML file..." << "\n" << flush;

  if(!xml.LoadXMLFile(fname.c_str())) {
    cout << "Error loading XML file" << "\n" << flush;
    cout << xml.GetError() << "\n";
    return 1;
  }

  PrintDocInfo(xml);
  cout << "\n" << flush;
  PrintPIList(xml);
  cout << "\n" << flush;
  PrintDTDList(xml);
  cout << "\n" << flush;
  PrintCDATA(xml);
  cout << "\n" << flush;
  PrintComments(xml);
  cout << "\n" << flush;
  PrintXS(xml);
  cout << "\n" << flush;

  PrintTree(xml);  
  cout << "\n" << flush;

  if(!outfile.is_null()) {
    cout << "\n" << flush;
    cout << "Writing a copy of the XML file to " << outfile.c_str() 
	 << "\n" << flush;
    if(!xml.WriteXMLFile(outfile.c_str())) {
      cout << "Error writing XML file" << "\n" << flush;
      cout << xml.GetError() << "\n";
      return 1;
    }
    cout << "Write complete" << "\n";
  }

  return 0;
}

void PrintDocInfo(gxsXML &xml)
{
  cout << "XML Doc Info" << "\n" << flush;
  cout << "------------" << "\n" << flush;
  cout << "File name: " << xml.doc_info.fname << "\n" << flush;
  cout << "Version: " << xml.doc_info.xml_version_str
       << "\n" << flush;
  cout << "Enoding: " << xml.doc_info.xml_encoding
       << "\n" << flush;
  if(!xml.doc_info.xml_style_sheet.is_null()) {
    cout << "Style Sheet: " << xml.doc_info.xml_style_sheet
	 << "\n" << flush;
  }
  else {
    cout << "Style Sheet: None" << "\n" << flush;
  }
  cout << "Standalone: " << xml.doc_info.xml_standalone
       << "\n" << flush;
}

void PrintPIList(gxsXML &xml) 
{
  if(xml.pi_list.IsEmpty()) {
    cout << "Process instruction list is empty" << "\n";
    cout << "---------------------------------" << "\n";
    return;
  }

  gxListNode<gxsXMLNode> *pi_ptr = xml.pi_list.GetHead();
  cout << "Process instruction list" << "\n";
  cout << "------------------------" << "\n";
  while(pi_ptr) {
    cout << "Tag: " << pi_ptr->data.attributes.c_str() << "\n";
    pi_ptr = pi_ptr->next;
  }
}

void PrintDTDList(gxsXML &xml) 
{
  if(xml.dtd_list.IsEmpty()) {
    cout << "Document type declaration list is empty" << "\n";
    cout << "--------------------------------------" << "\n";
    return;
  }

  gxListNode<gxsXMLNode> *dtd_ptr = xml.dtd_list.GetHead();
  cout << "Document type declaration list" << "\n";
  cout << "------------------------------" << "\n";
  while(dtd_ptr) {
    cout << dtd_ptr->data.attributes.c_str() << "\n";
    dtd_ptr = dtd_ptr->next;
  }
  cout << "------------------------------" << "\n";
}

void PrintCDATA(gxsXML &xml) 
{
  if(xml.cdata_list.IsEmpty()) {
    cout << "CDATA list is empty" << "\n";
    cout << "-------------------" << "\n";
    return;
  }

  gxListNode<gxsXMLNode> *cdata_ptr = xml.cdata_list.GetHead();
  cout << "CDATA list" << "\n";
  cout << "----------" << "\n";
  while(cdata_ptr) {
    cout << cdata_ptr->data.attributes.c_str() << "\n";
    cdata_ptr = cdata_ptr->next;
  }
  cout << "----------" << "\n";
}

void PrintComments(gxsXML &xml) 
{
  if(xml.comment_list.IsEmpty()) {
    cout << "Comment list is empty" << "\n";
    cout << "---------------------" << "\n";
    return;
  }

  gxListNode<gxsXMLNode> *comment_ptr = xml.comment_list.GetHead();
  cout << "Comment list be for root node" << "\n";
  cout << "-----------------------------" << "\n";
  while(comment_ptr) {
    cout << comment_ptr->data.attributes.c_str() << "\n";
    comment_ptr = comment_ptr->next;
  }
  cout << "-----------------------------" << "\n";
}

void PrintXS(gxsXML &xml) 
{
  if(xml.xs.IsEmpty()) {
    cout << "No XML Schema" << "\n";
    cout << "------------" << "\n";
    return;
  }

  gxString xs_string;
  cout << "XML Schema" << "\n";
  cout << "------------" << "\n";
  if(!xml.WriteXS(xs_string)) {
    cout << xml.GetError() << "\n";
  }
  else {
    cout << xs_string.c_str() << "\n";
  }
  cout << "------------" << "\n";
}

void PrintTree(gxsXML &xml)
// 10/14/2026: Print the tree with the library's own writer. The old
// hand-written loop repeated the writer's earlier nesting logic, so it
// showed nested elements at the wrong level once the parser was fixed.
{
  gxString tree;
  if(!xml.WriteTree(tree)) {
    cout << xml.GetError() << "\n";
    return;
  }
  cout << tree.c_str();
}

int WriteTestTree(const char *outfile)
{
  gxsXML xml;
 
  cout << "Building a test XML tree" << "\n" << flush;
  xml.Clear(); // Clear the tree and headers before creating a new tree

  // Add our root node
  gxsXMLNode *root_node = xml.AddRoot("xmlroot");
  if(!root_node) {
    // Check for any errors adding the root node
    cout << "Error adding node to XML tree" << "\n" << flush;
    cout << xml.GetError() << "\n";
    return 0;
  }

  gxsXMLNode *n;

  // Add a parent node with no attraibutes or data
  n = xml.AddParent("Author");
  if(!n) {
    // Check for any errors adding the node
    cout << "Error adding node to XML tree" << "\n" << flush;
    cout << xml.GetError() << "\n";
    return 0;
  }

  // Add a parent with no attributes, with data
  n = xml.AddParent("publisher", 0,
		    "DataReel Software Development Group");

  // Add a parent node with attraibutes and child nodes
  n = xml.AddParent("support1", "link=\"http://www.example.com\"");
  gxsXMLNode_t *element = xml.AddChild(n, "email", 0, "example@example.com");
  if(!element) {
    // Check for any errors adding the node
    cout << "Error adding node to XML tree" << "\n" << flush;
    cout << xml.GetError() << "\n";
    return 0;
  }
  element = xml.AddChild(n, "name", 0, "support");
  element = xml.AddChild(n, "fax", 0, "fax number");


  // Add a parent with children and child siblings
  n = xml.AddParent("subtree_depth_test");

  // Add the child
  element = xml.AddChild(n, "email", "accountinfo=\"datareel\"");

  // Add a sub-child 
  gxsXMLSubElement *sub_element = xml.AddSubChild(element, "username", 0, "uname");
  if(!sub_element) {
    // Check for any errors adding the node
    cout << "Error adding node to XML tree" << "\n" << flush;
    cout << xml.GetError() << "\n";
    return 0;
  }
  sub_element = xml.AddSubChild(element, "server");

  // Add the sub-child siblings
  gxsXMLSubElement *sibling = xml.AddSibling(element, sub_element, 
					     "hostname", 0, "pop3 host");
  if(!sibling) {
    // Check for any errors adding the node
    cout << "Error adding node to XML tree" << "\n" << flush;
    cout << xml.GetError() << "\n";
    return 0;
  }

  sibling = xml.AddSibling(element, sub_element, "pop3");
  xml.AddSibling(element, sibling, "port", 0, "default");
  xml.AddSibling(element, sibling, "usessl", 0, "yes");

  // Add a sibling to the sub-child sibling
  gxsXMLSubElement *subsibling =   xml.AddSibling(element, sibling, "local");
  if(!subsibling) {
    // Check for any errors adding the node
    cout << "Error adding node to XML tree" << "\n" << flush;
    cout << xml.GetError() << "\n";
    return 0;
  }
  // Add sub-siblings to the sibling
  xml.AddSibling(element, subsibling, "filters", 0, "yes");
  xml.AddSibling(element, subsibling, "arch_dir", 0, "local");


  cout << "Writing a copy of the XML tree to " << outfile 
       << "\n" << flush;
  if(!xml.WriteXMLFile(outfile)) {
    cout << "Error writing XML file" << "\n" << flush;
    cout << xml.GetError() << "\n";
    return 0;
  }
  cout << "Write complete" << "\n";

  return 1;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
