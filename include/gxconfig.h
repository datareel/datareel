// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Header File Name: gxconfig.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/19/1996 
// Date Last Modified: 06/17/2016
// Copyright (c) 2001-2016 DataReel Software Development
// ----------------------------------------------------------- // 
// ---------- Include File Description and Details  ---------- // 
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
09/10/2002: Changed gxConfig::StoreCfgData() config line 
parameter from const UString type to const char * type.

10/13/2002: Removed all function with non-const char * type
arguments.
==============================================================
*/
// ----------------------------------------------------------- // 
#ifndef __GX_CONFIG_HPP__
#define __GX_CONFIG_HPP__

#include "gxdlcode.h"

#include "gxlistb.h"
#include "gxstring.h"

#ifdef __BCC32__
#pragma warn -8022
#endif

class GXDLCODE_API gxConfigListNode : public gxListNodeB
{
public:
  gxConfigListNode() { data = (void *)&node_data; }
  gxConfigListNode(const gxString &s) : node_data(s) {
    data = (void *)&node_data;
  }
  ~gxConfigListNode() { }
  
public:
  gxString node_data;
};

// Configuration manager class
class GXDLCODE_API gxConfig : public gxListB
{
public:
  gxConfig() {
    FileName = "\0"; parm_ID = "="; comment_char = '#';
    filter_comments = 1;
  }
  gxConfig(const gxString &fname) {
    FileName = fname; parm_ID = "="; comment_char = '#';
    filter_comments = 1;
  }
  gxConfig(const char *fname) {
    FileName = fname; parm_ID = "="; comment_char = '#';
    filter_comments = 1;
  }

  // Constructors add to set the parameter ID string: 09/17/1999
  gxConfig(const gxString &fname, const gxString &p_id) {
    FileName = fname; parm_ID = p_id;
  }
  gxConfig(const gxString &fname, const char *p_id) {
    FileName = fname; parm_ID = p_id;
  }
  ~gxConfig();

private:
  gxConfig(const gxConfig &ob) { }       // Disallow copying
  void operator=(const gxConfig &ob) { } // Disallow assignment

public: // Functions used to load and unload config file
  int Load();
  int Load(const char *fname);
  int Load(const gxString &fname);
  int ReLoad();
  int ReLoad(const char *fname);
  int ReLoad(const gxString &fname);
  void UnLoad();
  char *GetFileName() { return FileName.c_str(); }
  void SetFileName(const gxString &s) { FileName = s; }
  void SetFileName(const char *s) { FileName = s; }
  
public: // Formatting functions
  // Treat all characters after the ID string as a parameter value
  void SetParmID(const gxString &s) { parm_ID = s; }
  void SetParmID(const char *s) { parm_ID = s; }
  char *GetParmID() { return parm_ID.c_str(); }
  char GetCommentChar() { return comment_char; }
  void SetCommentChar(char c) { comment_char = c; }
  void FilterComments() { filter_comments = 1; }
  void ReadComments() { filter_comments = 0; }
  
public: // Functions used to read config file values
  double GetFloatValue(const char *Name, int fn=1);    
  double GetFloatValue(const gxString &Name, int fn=1); 
  double GetDFPValue(const char *Name, int fn=1);              
  double GetDFPValue(const gxString &Name, int fn=1);           
  char* GetStrValue(const char *Name, int fn=1);    
  char* GetStrValue(const gxString &Name, int fn=1); 
  int GetIntValue(const char *Name, int fn=1);      
  int GetIntValue(const gxString &Name, int fn=1);   
  long GetLongValue(const char *Name, int fn=1);    
  long GetLongValue(const gxString &Name, int fn=1); 

public: // Functions used to write config values
  int WriteConfigLine(const gxString &parm, const gxString &value);
  int WriteConfigLine(const char *parm, const char *value);
  int WriteCommentLine(const gxString &s);
  int WriteCommentLine(const char *s);
  int WriteLine(const gxString &s);
  int WriteLine(const char *s);
  int ChangeConfigValue(const gxString &parm, const gxString &value);
  int ChangeConfigValue(const char *parm, const gxString &value);
  int ChangeConfigValue(const gxString &parm, const char *value);
  int ChangeConfigValue(const char *parm, const char *value);
  int ChangeConfigValue(const gxString &parm, int value);
  int ChangeConfigValue(const char *parm, int value);
  int ChangeConfigValue(const gxString &parm, unsigned value);
  int ChangeConfigValue(const char *parm, unsigned value);
  int ChangeConfigValue(const gxString &parm, long value);
  int ChangeConfigValue(const char *parm, long value);
  int ChangeConfigValue(const gxString &parm, float value);
  int ChangeConfigValue(const char *parm, float value);
  int ChangeConfigValue(const gxString &parm, double value);
  int ChangeConfigValue(const char *parm, double value);
  
public: // Linked list functions
  // Ignore BCC32 hidden virtual function warning
  gxConfigListNode *Add(const gxString &s);
  gxConfigListNode *FindAny(const gxString &s, gxConfigListNode *ptr = 0);
  gxConfigListNode *Find(const gxString &s, gxConfigListNode *ptr = 0);
  
private: // Internal proccessing functions
  void FilterLineFeed(char &c);
  int StoreCfgData(const char *cfg_line);
  int ChangeConfigLine(const gxString &string_to_replace,
		       const gxString &string_to_insert,
		       int check_case, int append_lines);
  
private:
  gxString FileName;   // Name of the configuration file
  gxString parm_ID;    // String used identify parameters in the file
  char comment_char;   // Char used to ID comments in the config file
  int filter_comments; // If true all comments will be ignored
};


#ifdef __BCC32__
#pragma warn .8022
#endif

#endif // __GX_CONFIG_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //

