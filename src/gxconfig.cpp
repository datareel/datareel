// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxconfig.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/19/1996 
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

The configuration manager class is used to load program parameters 
from a text based (ASCII) configuration file. 

Changes:
==============================================================
03/10/2002: The gxConfig class no longer uses the C++ fstream 
class as the underlying file system. The gxDatabase file pointer 
routines are now used to define the underlying file system used by 
the gxConfig class. This change was made to support large files 
and NTFS file system enhancements. To enable large file support 
users must define the __64_BIT_DATABASE_ENGINE__ preprocessor 
directive.

09/10/2002: Removed redundant temporary holding buffer from the 
gxConfig::Load() function.

09/10/2002: Changed gxConfig::StoreCfgData() config line 
parameter from const UString type to const char * type.

10/13/2002: Remove all function with non-const char * type
arguments.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "gxconfig.h"
#include "dfileb.h"

#ifdef __BCC32__
#pragma warn -8080
#endif

gxConfig::~gxConfig()
{
  // PC-lint 09/08/2005: Function may throw exception in destructor
  UnLoad();
}

void gxConfig::UnLoad()
{
  ClearList();
}         

int gxConfig::Load(const char *fname)
{
  FileName = fname;
  return Load();
}

int gxConfig::Load(const gxString &fname)
{
  FileName = fname;
  return Load();
}

int gxConfig::ReLoad()
{
  UnLoad();
  return Load();
}

int gxConfig::ReLoad(const char *fname)
{
  UnLoad();
  return Load(fname);
}

int gxConfig::ReLoad(const gxString &fname)
{
  UnLoad();
  return Load(fname);
}

void gxConfig::FilterLineFeed(char &c)
// Added to filter the end of line sequence from a line of text.
{
  switch(c) {
    case '\r':
      c = '\0';
      break;
    case '\n':
      c = '\0';
      break;
    default:
      break;
  }
}

int gxConfig::Load()
{
  const int MaxLine = 1024;
  char LineBuffer[MaxLine];
  int Status, i;
  
  DiskFileB infile(FileName.c_str(), 
		   DiskFileB::df_READONLY, DiskFileB::df_NO_CREATE);
  if(!infile) return 0; // Cannot open the file

  while(!infile.df_EOF()) {
    // Clear the line buffer before each read
    for(i = 0; i < MaxLine; i++) LineBuffer[i] = '\0';
    
    // Read in config file line by line
    if(infile.df_GetLine(LineBuffer, MaxLine, '\n', 1) != 
       DiskFileB::df_NO_ERROR) {
      break; // Error reading file
    }

    // Load config file data into singly-linked list
    Status = StoreCfgData((const char *)LineBuffer);
    if(!Status) return 0;
  }

  infile.df_Close();
  return 1; // Indicates success
}

int gxConfig::StoreCfgData(const char *cfg_line)
{
  gxConfigListNode *chsptr;
  gxString buf(cfg_line);
  
  // Trim leading and trailing spaces
  buf.TrimLeadingSpaces(); buf.TrimTrailingSpaces();

  if(filter_comments) { // Ignore all lines starting with a comment ID
    if(buf[0] == comment_char)
      return 1; 
  }
  
  // Look for lines containing a parm ID string
  int offset = buf.Find(parm_ID.c_str());
  if(offset != -1) {
    gxString Name(buf);
    gxString Value(buf);

    // Separate the parameter name and value
    Name.DeleteAt(offset, (Name.length() - offset));
    offset = Name.Find(" "); // Remove any trailing spaces
    if(offset != -1) Name.DeleteAt(offset, 1);
    chsptr = Add(Name); // Store the parameter name
    if(!chsptr) return 0;

    // Store the config file parameter value 
    offset = Value.Find(parm_ID.c_str());
    Value.DeleteAt(0, (offset+parm_ID.length()));
    offset = Value.Find(" "); // Remove any leading spaces
    if(offset == 0) Value.DeleteAt(offset, 1);
    offset = Value.Find(" "); // Look for any trailing spaces or other chars
    if(offset != -1) {
      // Remove any comments following a space but leave long
      // character strings for values such as file names with
      // white spaces or multiple config values on one line
      if(filter_comments) {
      if(Value[offset+1] == comment_char) 
	Value.DeleteAt(offset, (Value.length() - offset));
      }
    }
    chsptr = Add(Value);
    if(!chsptr) return 0;
  }

  return 1; // Indicate success
}

gxConfigListNode *gxConfig::Add(const gxString &s)
{
  gxConfigListNode *node = new gxConfigListNode(s);
  if(!node) return 0; // Could not allocate memory for the node
  InsertAtTail((gxListNodeB *)node);
  return node;
}

gxConfigListNode *gxConfig::FindAny(const gxString &s, gxConfigListNode *ptr)
{
  if(ptr == 0) ptr = (gxConfigListNode *)GetHead();

  int offset = 0;
  
  while(ptr) { // Scan until end of list
    offset = 0;
    offset = ptr->node_data.Find(s.c_str());
    if(offset != -1) return ptr; // Match found
    ptr = (gxConfigListNode *)ptr->next;
  }
  return 0; // No match
}

gxConfigListNode *gxConfig::Find(const gxString &s, gxConfigListNode *ptr)
{
  if(ptr == 0) ptr = (gxConfigListNode *)GetHead();

  while(ptr) { // Scan until end of list
    if(ptr->node_data == s) return ptr; // Match found
    ptr = (gxConfigListNode *)ptr->next;
  }
  return 0; // No match
}

char* gxConfig::GetStrValue(const gxString &Name, int fn)
// Search for string matching the "Name" variable. Will search
// using the full name unless the "fn" variable is false.
{
  gxConfigListNode *ptr;

  if(fn)
    ptr = Find(Name);
  else
    ptr = FindAny(Name);
  
  if(ptr) {
    gxString *s = (gxString *)ptr->next->data;
    if(s) return s->c_str();
  }

  // Return NULL if config value is not found
  return 0;
}

char* gxConfig::GetStrValue(const char *Name, int fn)
{
  gxString buf(Name);
  return GetStrValue(buf, fn);
}

int gxConfig::GetIntValue(const gxString &Name, int fn)
// Search for string matching the "Name" variable. Will search
// using the full name unless the "fn" variable is false.
{
  gxConfigListNode *ptr;

  if(fn)
    ptr = Find(Name);
  else
    ptr = FindAny(Name);
  
  if(ptr) {
    gxString *s = (gxString *)ptr->next->data;
    if(s) return s->Atoi();
  }

  //Return NULL if config value is not found
  return 0;
}

int gxConfig::GetIntValue(const char *Name, int fn)
{
  gxString buf(Name);
  return GetIntValue(buf, fn);
}

double gxConfig::GetFloatValue(const gxString &Name, int fn)
// Search for string matching the "Name" variable. Will search
// using the full name unless the "fn" variable is false.
{
  gxConfigListNode *ptr;

  if(fn)
    ptr = Find(Name);
  else
    ptr = FindAny(Name);

  if(ptr) {
    gxString *s = (gxString *)ptr->next->data;
    if(s) return s->Atof();
  }

  //Return NULL if config value is not found
  return 0;
}

double gxConfig::GetFloatValue(const char *Name, int fn)
{
  gxString buf(Name);
  return GetFloatValue(buf, fn);
}

double gxConfig::GetDFPValue(const gxString &Name, int fn)
{
  return GetFloatValue(Name, fn);
}

double gxConfig::GetDFPValue(const char *Name, int fn)
{
  gxString buf(Name);
  return GetFloatValue(buf, fn);
}

long gxConfig::GetLongValue(const gxString &Name, int fn)
// Search for string matching the "Name" variable. Will search
// using the full name unless the "fn" variable is false.
{
  gxConfigListNode *ptr;

  if(fn)
    ptr = Find(Name);
  else
    ptr = FindAny(Name);


  if(ptr) {
    gxString *s = (gxString *)ptr->next->data;
    if(s) return s->Atol();
  }

  //Return NULL if config value is not found
  return 0;
}

long gxConfig::GetLongValue(const char *Name, int fn)
{
  gxString buf(Name);
  return GetLongValue(buf, fn);
}

int gxConfig::WriteConfigLine(const gxString &parm, const gxString &value)
// Write a line to the config file.  NOTE: All parameter values should
// not contain a parameter ID label at the end of the string. The 
// parameter ID label will be added to mark it as a config file parameter.
// By default an equal sign will be used as a parameter ID.
{
  DiskFileB outfile(FileName.c_str(), DiskFileB::df_READWRITE, 
		    DiskFileB::df_NO_CREATE, DiskFileB::df_APPEND);
  if(!outfile) return 0; // Cannot open the file

  outfile << parm.c_str() << parm_ID.c_str() << value.c_str() << "\n";
  outfile.df_Close();
  return 1;
}  

int gxConfig::WriteConfigLine(const char *parm, const char *value)
{
  gxString p(parm); gxString v(value);
  return WriteConfigLine(p, v);
}

int gxConfig::WriteCommentLine(const gxString &s)
// Write a comment line to the config file.  NOTE: Do not include the
// comment ID character. The comment ID will by automatically inserted.
{
  DiskFileB outfile(FileName.c_str(), DiskFileB::df_READWRITE, 
		    DiskFileB::df_NO_CREATE, DiskFileB::df_APPEND);
  if(!outfile) return 0; // Cannot open the file

  outfile << comment_char << ' ' << s.c_str() << "\n";
  outfile.df_Close();
  return 1;
}  

int gxConfig::WriteCommentLine(const char *s)
{
  gxString cm(s);
  return WriteCommentLine(cm);
}

int gxConfig::WriteLine(const gxString &s)
// Write a line of text to the config file.  
{
  DiskFileB outfile(FileName.c_str(), DiskFileB::df_READWRITE, 
		    DiskFileB::df_NO_CREATE, DiskFileB::df_APPEND);
  if(!outfile) return 0; // Cannot open the file

  outfile << s.c_str() << "\n";
  outfile.df_Close();
  return 1;
}  

int gxConfig::WriteLine(const char *s)
{
  gxString line(s);
  return WriteLine(line);
}

int gxConfig::ChangeConfigLine(const gxString &string_to_replace,
			       const gxString &string_to_insert,
			       int check_case, int append_lines)
// This function is used to replace strings in the config file.
// Returns the number of strings processed or zero if an error occurs.
{
  const int MAX_LINE = 1024;   // Maximum characters per line
  char LineBuffer[MAX_LINE];
  char rawLineBuffer[MAX_LINE];
  gxString LineData;
  gxString appendLineBuffer;
  gxConfig list;
  int i, j;
  int offset = 0;
  int num_processed = 0;      
  
  DiskFileB infile(FileName.c_str(), 
		   DiskFileB::df_READONLY, DiskFileB::df_NO_CREATE);
  if(!infile) return 0; // Cannot open the file
	
  while(!infile.df_EOF()) { // Read in the file line by line

    // Clear the buffers
    for(i = 0; i < MAX_LINE; i++) LineBuffer[i] = '\0';
    for(i = 0; i < MAX_LINE; i++) rawLineBuffer[i] = '\0';
    LineData.DeleteAt(0, LineData.length());
    offset = 0; // Reset string offset
    
    if(infile.df_GetLine(rawLineBuffer, MAX_LINE) != DiskFileB::df_NO_ERROR) {
      break; // Error reading file
    }
        
    // Filter each line of text
    for(i = 0, j = 0; i < MAX_LINE; i++) {
      if((rawLineBuffer[i] == '\r') || (rawLineBuffer[i] == '\n')) break;
      LineBuffer[j++] = rawLineBuffer[i];
    }

    // Detect backslashes used to mark the continuation of a line
    if((LineBuffer[strlen(LineBuffer)-1] == '\\') && (append_lines == 1)) {
      appendLineBuffer += LineBuffer;
      appendLineBuffer += '\n'; // Insert LF after the backslash 
      continue;  // Goto the top of loop and append next line
    }
    if(appendLineBuffer.length() > 0) { // Append the next line
      appendLineBuffer += LineBuffer;
      LineData = appendLineBuffer;
      appendLineBuffer.DeleteAt(0, appendLineBuffer.length());
    }
    else
      LineData = LineBuffer;
    
    // Get rid of EOF marker
    if((strcmp(LineData.c_str(), "\0") == 0) && (infile.df_EOF())) break;

    // Routine to replace a line
    // ----------------------------------------------------------- 
    if(check_case) { // Find entire match checking the case of the string
      offset = LineData.Find((char *)string_to_replace.c_str(),
			     string_to_replace.length(), 0);
    }
    else {
      offset = LineData.IFind((char *)string_to_replace.c_str(),
			      string_to_replace.length(), 0);
    }

    if(offset == -1) { // The configurable parameter was not found
      list.Add(LineData); // Store the line and continue
      continue;
    }
    
    int parm_id_offset = LineData.Find(parm_ID.c_str());
    if(parm_id_offset == -1) { // This is not a configurable parameter
      list.Add(LineData); // Store the line and continue
      continue;
    }
    
    offset += string_to_replace.length();

    // Ensure the complete string is matched before modifying the parameter
    if((LineData[offset] == ' ') || 
       (LineData[offset] == parm_ID[0])) {
      LineData.DeleteAt(offset, (LineData.length() - offset));
      LineData.InsertAt(offset, parm_ID.c_str()); // Insert parameter ID string
      LineData.InsertAt(++offset, string_to_insert);
      num_processed++;    // Record number of string processed for this file
    }
    list.Add(LineData); // Store all the lines and continue
  }
  infile.df_Close(); // Close the input file
  
  // If any strings were replaced in the file, rewrite the file
  // ----------------------------------------------------------- 
  if(num_processed > 0) {
    char *EndOfLineSequence;
    
    DiskFileB outfile(FileName.c_str(), DiskFileB::df_READWRITE, 
		    DiskFileB::df_CREATE);
    if(!outfile) return 0; // Cannot open the file
    
#if defined (__WIN32__) || defined(__DOS__)
    EndOfLineSequence = "\r\n"; 
#else 
    EndOfLineSequence = "\n"; 
#endif
    
    gxConfigListNode *list_ptr = (gxConfigListNode *)list.GetHead();
    while(list_ptr) {
      if(list_ptr->data) {
	if(outfile.df_Write(list_ptr->node_data.c_str(), 
			    list_ptr->node_data.length()) != 
	   DiskFileB::df_NO_ERROR) {
	  break; // Error writing to the file
	}
	if(outfile.df_Write(EndOfLineSequence, strlen(EndOfLineSequence)) !=
	   DiskFileB::df_NO_ERROR) {
	  break; // Error writing to the file
	}  
      }
      list_ptr = (gxConfigListNode *)list_ptr->next; 
    }
    
    outfile.df_Close(); // Close the output file
    
    // Make the changes to the config list in already in memory
    ReLoad();
  }
  
  list.ClearList();
  return num_processed;
}

int gxConfig::ChangeConfigValue(const gxString &parm, const gxString &value)
// Modify the config file. NOTE: All parameter values should not contain 
// a parameter ID label at the end of the string. The parameter ID label
// will be added to mark it as a config file parameter. By default an
// equal sign will be used as a parameter ID.
{
  int rv = ChangeConfigLine(parm, value, 1, 1);

  // Write the parameter if it does not exist
  if(!rv) return WriteConfigLine(parm, value);

  return rv;
}

int gxConfig::ChangeConfigValue(const char *parm, const gxString &value)
{
  gxString p(parm); gxString v(value);
  return ChangeConfigValue(p, v);
}

int gxConfig::ChangeConfigValue(const gxString &parm, const char *value)
{
  gxString p(parm); gxString v(value);
  return ChangeConfigValue(p, v);
}

int gxConfig::ChangeConfigValue(const char *parm, const char *value)
{
  gxString p(parm); gxString v(value);
  return ChangeConfigValue(p, v);
}

int gxConfig::ChangeConfigValue(const gxString &parm, int value)
{
  gxString val;
  val << value;
  return ChangeConfigValue(parm, val);
}

int gxConfig::ChangeConfigValue(const char *parm, int value)
{
  gxString p(parm);
  return ChangeConfigValue(p, value);
}

int gxConfig::ChangeConfigValue(const gxString &parm, unsigned value)
{
  gxString val;
  val << value;
  return ChangeConfigValue(parm, val);
}

int gxConfig::ChangeConfigValue(const char *parm, unsigned value)
{
  gxString p(parm);
  return ChangeConfigValue(p, value);
}

int gxConfig::ChangeConfigValue(const gxString &parm, long value)
{
  gxString val;
  val << value;
  return ChangeConfigValue(parm, val);
}

int gxConfig::ChangeConfigValue(const char *parm, long value)
{
  gxString p(parm);
  return ChangeConfigValue(p, value);
}

int gxConfig::ChangeConfigValue(const gxString &parm, float value)
{
  gxString val;
  val << value;
  return ChangeConfigValue(parm, val);
}

int gxConfig::ChangeConfigValue(const char *parm, float value)
{
  gxString p(parm);
  return ChangeConfigValue(p, value);
}
  
int gxConfig::ChangeConfigValue(const gxString &parm, double value)
{
  gxString val;
  val << value;
  return ChangeConfigValue(parm, val);
}

int gxConfig::ChangeConfigValue(const char *parm, double value)
{
  gxString p(parm);
  return ChangeConfigValue(p, value);
}

#ifdef __BCC32__
#pragma warn .8080
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
