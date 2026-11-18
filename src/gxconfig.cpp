// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxconfig.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/19/1996 
// Date Last Modified: 10/31/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- // 
// ------------- Program Description and Details ------------- // 
// ----------------------------------------------------------- // 
/*
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.
 
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library. If not, see
<https://www.gnu.org/licenses/>.

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
#include <errno.h>
#include <limits.h>
#include "gxconfig.h"
#include "dfileb.h"

// 10/31/2026: Used to rewrite the config file in one step
#if defined (__WIN32__)
#include <windows.h>
#elif defined (__UNIX__)
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#ifdef __BCC32__
#pragma warn -8080
#endif

static int gxconfig_read_line(DiskFileB &f, gxString &line)
// 10/31/2026: Read one whole line, of any length, into "line" without
// the line ending. Text after a CR is dropped, as before. Returns 0 at
// the end of the file when nothing was read. Lines used to be read in
// 1023-byte pieces, so the end of a long value was read as a line of
// its own: a long value holding "name=value" added a setting when the
// file was loaded and split the line when the file was rewritten
// (found by the DataReel 7.3.1 security sweep review of 6.x).
{
  line.Clear();
  int got_any = 0, cr_seen = 0;
  char buf[256];
  unsigned n = 0;
  while(!f.df_EOF()) {
    char c;
    if(f.df_Read(&c, 1) != DiskFileB::df_NO_ERROR) break;
    got_any = 1;
    if(c == '\n') break;
    if(c == '\r') cr_seen = 1;
    if(cr_seen) continue;
    buf[n++] = c;
    if(n == sizeof(buf)) { line.InsertAt(line.length(), buf, n); n = 0; }
  }
  if(n) line.InsertAt(line.length(), buf, n);
  return got_any;
}

static int gxconfig_to_long(const gxString &s, long lo, long hi, long &val)
// 10/31/2026: Number conversion with range checks. atoi()/atol() have
// undefined behavior when the value does not fit. A value out of range
// now reads as 0, the same as a missing value. Leading digits are still
// accepted, as atoi() did.
{
  val = 0;
  const char *p = s.c_str();
  if(!p) return 0;
  char *end = 0;
  errno = 0;
  long v = strtol(p, &end, 10);
  if((end == p) || (errno == ERANGE) || (v < lo) || (v > hi)) return 0;
  val = v;
  return 1;
}

static gxString *gxconfig_value(gxConfig *cfg, const gxString &Name, int fn)
// 10/31/2026: Find the value of a setting. Only the name nodes are
// searched (the list holds name, value, name, value...). Find() and
// FindAny() also matched value nodes, so a value equal to the name
// returned the next setting's name, and a match on the last value read
// through a null pointer.
{
  gxConfigListNode *ptr = (gxConfigListNode *)cfg->GetHead();
  while(ptr && ptr->next) {
    gxConfigListNode *vptr = (gxConfigListNode *)ptr->next;
    int match;
    if(fn) match = (ptr->node_data == Name);
    else match = (ptr->node_data.Find(Name.c_str()) != -1);
    if(match) return (gxString *)vptr->data;
    ptr = (gxConfigListNode *)vptr->next;
  }
  return 0;
}

static int gxconfig_bad_name(const gxString &parm, const gxString &parm_ID)
// 10/31/2026: True if "parm" cannot be written as a setting name: it is
// empty or holds the parameter ID string, so it would not read back as
// the same setting.
{
  if(parm.length() == 0) return 1;
  if((parm_ID.length() > 0) && (parm.Find(parm_ID.c_str()) != -1)) return 1;
  return 0;
}

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
  int Status;
  gxString LineBuffer;
  
  DiskFileB infile(FileName.c_str(), 
		   DiskFileB::df_READONLY, DiskFileB::df_NO_CREATE);
  if(!infile) return 0; // Cannot open the file

  // 10/31/2026: Read whole lines (see gxconfig_read_line())
  while(gxconfig_read_line(infile, LineBuffer)) {
    // Load config file data into singly-linked list
    Status = StoreCfgData(LineBuffer.length() ? LineBuffer.c_str() : "");
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

gxListNodeB *gxConfig::Add(void *)
// 10/24/2026: Untyped data cannot be stored in the config list. Every
// node must be a gxConfigListNode, so nothing is added.
{
  return 0;
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
  // 10/31/2026: Searches the setting names only (gxconfig_value())
  gxString *s = gxconfig_value(this, Name, fn);
  if(s) return s->c_str();

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
  // 10/31/2026: Searches the setting names only (gxconfig_value()), and
  // a number that does not fit in an int reads as 0 (was atoi())
  gxString *s = gxconfig_value(this, Name, fn);
  long val;
  if(s && gxconfig_to_long(*s, INT_MIN, INT_MAX, val)) return (int)val;

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
  // 10/31/2026: Searches the setting names only (gxconfig_value())
  gxString *s = gxconfig_value(this, Name, fn);
  if(s) return s->Atof();

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
  // 10/31/2026: Searches the setting names only (gxconfig_value()), and
  // a number that does not fit in a long reads as 0 (was atol())
  gxString *s = gxconfig_value(this, Name, fn);
  long val;
  if(s && gxconfig_to_long(*s, LONG_MIN, LONG_MAX, val)) return val;

  //Return NULL if config value is not found
  return 0;
}

long gxConfig::GetLongValue(const char *Name, int fn)
{
  gxString buf(Name);
  return GetLongValue(buf, fn);
}

static int gxconfig_one_line(const gxString &s)
// 10/31/2026: True if "s" has no line break. A name, value or comment
// with a CR or LF used to write extra lines into the config file, so a
// value could add or replace other settings (found by the DataReel 7.3
// security sweep).
{
  const char *p = s.c_str();
  return p && !strchr(p, '\n') && !strchr(p, '\r');
}

int gxConfig::WriteConfigLine(const gxString &parm, const gxString &value)
// Write a line to the config file.  NOTE: All parameter values should
// not contain a parameter ID label at the end of the string. The 
// parameter ID label will be added to mark it as a config file parameter.
// By default an equal sign will be used as a parameter ID.
// 10/31/2026: Returns 0 if the name or value has a line break.
{
  if(!gxconfig_one_line(parm) || !gxconfig_one_line(value)) return 0;
  // 10/31/2026: An empty name, or one holding the parameter ID string,
  // would not read back as the same setting
  if(gxconfig_bad_name(parm, parm_ID)) return 0;
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
// 10/31/2026: Returns 0 if the comment has a line break.
{
  if(!gxconfig_one_line(s)) return 0;
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

static int gxconfig_rewrite(const gxString &fname, gxConfig &list)
// 10/31/2026: Write the lines in "list" to a new file next to the
// config file and rename it over the config file, so a crash or a full
// disk cannot leave a truncated or half written config file. The file
// used to be truncated and written in place. On UNIX the new file gets
// the old file's mode (and owner, if allowed) and a config file that is
// a symbolic link is changed at the link's target. Returns 1 if
// successful.
{
#if defined (__WIN32__) || defined(__DOS__)
  const char *eol = "\r\n"; 
#else 
  const char *eol = "\n"; 
#endif

  gxString target(fname);
  gxString tmp_name;
  FILE *fp = 0;

#if defined (__UNIX__)
  struct stat st;
  int have_st = 0;
  char *real = realpath(fname.c_str(), 0);
  if(real) { target = real; free(real); }
  if(stat(target.c_str(), &st) == 0) have_st = 1;
  tmp_name << target << ".XXXXXX";
  int fd = mkstemp(tmp_name.c_str()); // Created 0600, O_EXCL
  if(fd < 0) return 0;
  if(have_st) {
    // Keep the owner (when allowed) and the access of the old file
    if(fchown(fd, st.st_uid, st.st_gid) != 0) { /* Not the owner */ }
    fchmod(fd, st.st_mode & 07777);
  }
  fp = fdopen(fd, "wb");
  if(!fp) { close(fd); unlink(tmp_name.c_str()); return 0; }
#else
  tmp_name << target << ".tmp";
  fp = fopen(tmp_name.c_str(), "wb");
  if(!fp) return 0;
#endif

  int ok = 1;
  size_t eol_len = strlen(eol);
  gxConfigListNode *list_ptr = (gxConfigListNode *)list.GetHead();
  while(list_ptr && ok) {
    if(list_ptr->data) {
      size_t len = list_ptr->node_data.length();
      if(len && (fwrite(list_ptr->node_data.c_str(), 1, len, fp) != len)) ok = 0;
      if(ok && (fwrite(eol, 1, eol_len, fp) != eol_len)) ok = 0;
    }
    list_ptr = (gxConfigListNode *)list_ptr->next; 
  }
  if(fflush(fp) != 0) ok = 0;
#if defined (__UNIX__)
  if(ok && (fsync(fileno(fp)) != 0)) ok = 0;
#endif
  if(fclose(fp) != 0) ok = 0;

  if(ok) {
#if defined (__WIN32__)
    ok = MoveFileExA(tmp_name.c_str(), target.c_str(),
		     MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH) != 0;
#else
    ok = rename(tmp_name.c_str(), target.c_str()) == 0;
#endif
  }
  if(!ok) remove(tmp_name.c_str());
  return ok;
}

int gxConfig::ChangeConfigLine(const gxString &string_to_replace,
			       const gxString &string_to_insert,
			       int check_case, int append_lines)
// This function is used to replace strings in the config file.
// Returns the number of strings processed or zero if an error occurs.
// 10/31/2026: Returns 0 if the name or the new value has a line break.
{
  if(!gxconfig_one_line(string_to_replace) || !gxconfig_one_line(string_to_insert)) return 0;
  if(gxconfig_bad_name(string_to_replace, parm_ID)) return 0; // 10/31/2026
  gxString LineData;
  gxString LineBuffer;
  gxString appendLineBuffer;
  gxConfig list;
  int offset = 0;
  int num_processed = 0;      
  
  DiskFileB infile(FileName.c_str(), 
		   DiskFileB::df_READONLY, DiskFileB::df_NO_CREATE);
  if(!infile) return 0; // Cannot open the file
	
  // 10/31/2026: Read whole lines (see gxconfig_read_line()). Long lines
  // were read in 1023-byte pieces and written back as several lines.
  while(gxconfig_read_line(infile, LineBuffer)) {
    offset = 0; // Reset string offset

    // Detect backslashes used to mark the continuation of a line
    unsigned line_len = LineBuffer.length();
    if((line_len > 0) && (LineBuffer[line_len-1] == '\\') &&
       (append_lines == 1)) {
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
    
    // Routine to replace a line
    // ----------------------------------------------------------- 
    // 10/31/2026: The name must start the line (after spaces or tabs).
    // It used to be found anywhere in the line, so a change rewrote the
    // wrong settings: a name that ends with this name, a value or a
    // comment that holds it (found by the DataReel 7.3 security sweep).
    {
      unsigned start = 0;
      while((start < LineData.length()) &&
	    ((LineData[start] == ' ') || (LineData[start] == '\t'))) start++;
      offset = -1;
      if((start < LineData.length()) && (LineData[start] != comment_char) &&
	 (LineData.length() - start >= string_to_replace.length())) {
	const char *name = LineData.c_str() + start;
	const char *want = string_to_replace.c_str();
	int same = 1;
	for(unsigned k = 0; same && k < string_to_replace.length(); k++) {
	  if(check_case) same = (name[k] == want[k]);
	  else same = (tolower((unsigned char)name[k]) == tolower((unsigned char)want[k]));
	}
	if(same) offset = (int)start;
      }
    }

    if(offset == -1) { // The configurable parameter was not found
      list.Add(LineData); // Store the line and continue
      continue;
    }
    
    offset += string_to_replace.length();

    // 10/31/2026: The whole name must match: only spaces or tabs may
    // come between the name and the parameter ID string (the first
    // character of the ID used to be enough, so "Port x=1" or "Port:=1"
    // with an ID of "=" matched "Port"). The new value is inserted after
    // the whole ID string; it used to go one character after the start
    // of the ID, inside an ID of more than one character.
    unsigned id_pos = (unsigned)offset;
    while((id_pos < LineData.length()) &&
	  ((LineData[id_pos] == ' ') || (LineData[id_pos] == '\t'))) id_pos++;
    if((parm_ID.length() > 0) && (id_pos < LineData.length()) &&
       (strncmp(LineData.c_str() + id_pos, parm_ID.c_str(), 
		parm_ID.length()) == 0)) {
      LineData.DeleteAt(offset, (LineData.length() - offset));
      LineData += parm_ID;
      LineData += string_to_insert;
      num_processed++;    // Record number of string processed for this file
    }
    list.Add(LineData); // Store all the lines and continue
  }
  if(appendLineBuffer.length() > 0) { // 10/31/2026: Last line ends in '\\'
    appendLineBuffer.DeleteAt(appendLineBuffer.length()-1, 1); // Added LF
    list.Add(appendLineBuffer);
  }
  infile.df_Close(); // Close the input file
  
  // If any strings were replaced in the file, rewrite the file
  // ----------------------------------------------------------- 
  if(num_processed > 0) {
    if(!gxconfig_rewrite(FileName, list)) { // 10/31/2026: See above
      list.ClearList();
      return 0;
    }

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
  // 10/31/2026: Refuse names and values that cannot be written as one
  // setting, so a failed change is not appended as a new line
  if(!gxconfig_one_line(parm) || !gxconfig_one_line(value)) return 0;
  if(gxconfig_bad_name(parm, parm_ID)) return 0;
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
