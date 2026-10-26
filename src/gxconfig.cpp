// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxconfig.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/19/1996 
// Date Last Modified: 10/26/2026
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

10/26/2026: DataReel 7 rewrites gxConfig on std::string, 
std::vector and std::fstream.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <fstream>
#include "gxconfig.h"
#include "strutil.h"

namespace datareel {

bool gxConfig::Load(const std::string &fname)
{
  FileName = fname;
  return Load();
}

bool gxConfig::ReLoad()
{
  UnLoad();
  return Load();
}

bool gxConfig::ReLoad(const std::string &fname)
{
  UnLoad();
  return Load(fname);
}

bool gxConfig::Load()
{
  std::ifstream infile(FileName, std::ios::in | std::ios::binary);
  if(!infile) return false; // Cannot open the file

  std::string line;
  while(std::getline(infile, line)) {
    if(!line.empty() && line.back() == '\r') line.pop_back();
    StoreCfgData(line);
  }
  return true;
}

void gxConfig::StoreCfgData(std::string_view cfg_line)
{
  std::string_view buf = trim_right_view(trim_left_view(cfg_line));
  if(buf.empty()) return;

  // Ignore all lines starting with a comment ID
  if(filter_comments && buf[0] == comment_char) return;
  
  // Look for lines containing a parm ID string
  if(parm_ID.empty()) return;
  std::size_t offset = buf.find(parm_ID);
  if(offset == std::string_view::npos) return;

  std::string name = trim(buf.substr(0, offset));
  std::string_view value = trim_left_view(buf.substr(offset + parm_ID.size()));

  // Remove a comment following a space, but leave values such as
  // file names that contain spaces
  if(filter_comments) {
    char pat[3] = { ' ', comment_char, 0 };
    std::size_t c = value.find(pat);
    if(c != std::string_view::npos) value = value.substr(0, c);
  }
  entries.emplace_back(std::move(name), trim(value));
}

const std::string *gxConfig::FindValue(std::string_view name, 
				       bool full_name) const
{
  for(const auto &e : entries) {
    if(full_name) {
      if(e.first == name) return &e.second;
    }
    else {
      if(e.first.find(name) != std::string::npos) return &e.second;
    }
  }
  return nullptr;
}

const char *gxConfig::GetStrValue(std::string_view name, bool full_name) const
{
  const std::string *s = FindValue(name, full_name);
  return s ? s->c_str() : nullptr;
}

std::string gxConfig::GetString(std::string_view name, 
				const std::string &default_value,
				bool full_name) const
{
  const std::string *s = FindValue(name, full_name);
  return s ? *s : default_value;
}

int gxConfig::GetIntValue(std::string_view name, bool full_name) const
{
  const std::string *s = FindValue(name, full_name);
  return s ? to_int(*s) : 0;
}

long gxConfig::GetLongValue(std::string_view name, bool full_name) const
{
  const std::string *s = FindValue(name, full_name);
  return s ? to_long(*s) : 0;
}

double gxConfig::GetFloatValue(std::string_view name, bool full_name) const
{
  const std::string *s = FindValue(name, full_name);
  return s ? to_double(*s) : 0;
}

bool gxConfig::AppendToFile(const std::string &text)
{
  std::ofstream outfile(FileName, std::ios::out | std::ios::app);
  if(!outfile) return false;
  outfile << text;
  return bool(outfile);
}

bool gxConfig::WriteConfigLine(std::string_view parm, std::string_view value)
// Write a parameter line to the config file. The parameter ID string
// is inserted between the name and value, an equal sign by default.
{
  std::string line(parm);
  line += parm_ID;
  line += value;
  line += '\n';
  return AppendToFile(line);
}  

bool gxConfig::WriteCommentLine(std::string_view s)
// Write a comment line to the config file. Do not include the comment
// ID character, which is inserted automatically.
{
  std::string line(1, comment_char);
  line += ' ';
  line += s;
  line += '\n';
  return AppendToFile(line);
}  

bool gxConfig::WriteLine(std::string_view s)
{
  std::string line(s);
  line += '\n';
  return AppendToFile(line);
}  

int gxConfig::ChangeConfigLine(std::string_view string_to_replace,
			       std::string_view string_to_insert,
			       bool check_case, bool append_lines)
// Replace the value of a parameter in the config file. Returns the 
// number of lines changed or zero if none were changed.
{
  std::ifstream infile(FileName, std::ios::in | std::ios::binary);
  if(!infile) return 0; // Cannot open the file

  std::vector<std::string> lines;
  std::string raw, pending;
  int num_processed = 0;

  while(std::getline(infile, raw)) {
    while(!raw.empty() && (raw.back() == '\r' || raw.back() == '\n')) {
      raw.pop_back();
    }

    // A trailing backslash continues the line on the next line
    if(append_lines && !raw.empty() && raw.back() == '\\') {
      pending += raw;
      pending += '\n';
      continue;
    }
    std::string line = pending + raw;
    pending.clear();

    std::size_t offset = check_case ? line.find(string_to_replace) :
      ifind(line, string_to_replace);
    std::size_t parm_id_offset = line.find(parm_ID);
    if(offset == std::string::npos || parm_id_offset == std::string::npos) {
      lines.push_back(std::move(line));
      continue;
    }
    
    offset += string_to_replace.size();

    // Ensure the complete name matched before changing the value
    if(offset < line.size() && 
       (line[offset] == ' ' || line.compare(offset, parm_ID.size(), parm_ID) == 0)) {
      line.erase(offset);
      line += parm_ID;
      line += string_to_insert;
      num_processed++;
    }
    lines.push_back(std::move(line));
  }
  if(!pending.empty()) lines.push_back(pending);
  infile.close();
  
  if(num_processed == 0) return 0;

  // Rewrite the file with the changed lines
#if defined(__WIN32__)
  const char *eol = "\r\n"; 
#else 
  const char *eol = "\n"; 
#endif
  std::ofstream outfile(FileName, std::ios::out | std::ios::binary | 
			std::ios::trunc);
  if(!outfile) return 0;
  for(const auto &l : lines) outfile << l << eol;
  outfile.close();
  
  // Make the changes to the config values in memory
  ReLoad();
  return num_processed;
}

int gxConfig::ChangeConfigValue(std::string_view parm, std::string_view value)
// Change a parameter in the config file, or append it if it does not
// exist. Do not include the parameter ID string in parm or value.
{
  int rv = ChangeConfigLine(parm, value, true, true);
  if(rv) return rv;

  // Write the parameter if it does not exist
  if(!WriteConfigLine(parm, value)) return 0;
  entries.emplace_back(std::string(parm), std::string(value));
  return 1;
}

} // namespace datareel

// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
