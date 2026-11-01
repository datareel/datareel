// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxconfig.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/19/1996 
// Date Last Modified: 11/01/2026
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

#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <system_error>
#include "gxconfig.h"
#include "strutil.h"

namespace datareel {

// 11/01/2026: Numeric conversions with range checks. atoi()/atol() have
// undefined behavior on overflow. A value out of range now reads as 0,
// the same as a missing value; leading digits are still accepted, as
// atoi() did.
static long gxcfg_to_long(const std::string &s, long lo, long hi)
{
  errno = 0;
  char *end = nullptr;
  long v = std::strtol(s.c_str(), &end, 10);
  if(end == s.c_str() || errno == ERANGE || v < lo || v > hi) return 0;
  return v;
}

// 11/01/2026: True if s holds a CR or LF, which would let a name or 
// value write extra lines (and so extra parameters) into the file
static bool gxcfg_has_eol(std::string_view s)
{
  return s.find_first_of("\r\n") != std::string_view::npos;
}

static bool gxcfg_iequal(std::string_view a, std::string_view b)
{
  if(a.size() != b.size()) return false;
  for(std::size_t i = 0; i < a.size(); i++) {
    if(std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i]))
      return false;
  }
  return true;
}

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
  return s ? (int)gxcfg_to_long(*s, INT_MIN, INT_MAX) : 0; // 11/01/2026
}

long gxConfig::GetLongValue(std::string_view name, bool full_name) const
{
  const std::string *s = FindValue(name, full_name);
  return s ? gxcfg_to_long(*s, LONG_MIN, LONG_MAX) : 0; // 11/01/2026
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
  // 11/01/2026: Reject CR/LF (line injection), an empty name and a name
  // holding the parameter ID string, none of which would read back as
  // the same parameter
  if(parm.empty() || gxcfg_has_eol(parm) || gxcfg_has_eol(value)) return false;
  if(!parm_ID.empty() && parm.find(parm_ID) != std::string_view::npos) {
    return false;
  }
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
  // 11/01/2026: A line break would end the comment and let the rest of
  // the text be read as parameters
  if(gxcfg_has_eol(s)) return false;
  std::string line(1, comment_char);
  line += ' ';
  line += s;
  line += '\n';
  return AppendToFile(line);
}  

bool gxConfig::WriteLine(std::string_view s)
// NOTE: Writes s as given. Text holding line breaks is written as
// several lines; use WriteConfigLine() for values from outside sources.
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

    // 11/01/2026: This searched for the name anywhere in the line, so
    // changing "Port" also rewrote "AdminPort" (and commented out 
    // lines). Compare the whole trimmed name before the parameter ID
    // and leave comment lines alone.
    std::size_t first = line.find_first_not_of(" \t");
    std::size_t parm_id_offset = parm_ID.empty() ? std::string::npos :
      line.find(parm_ID);
    if(first == std::string::npos || line[first] == comment_char ||
       parm_id_offset == std::string::npos || parm_id_offset < first) {
      lines.push_back(std::move(line));
      continue;
    }
    std::string_view name = trim_right_view(
      std::string_view(line).substr(first, parm_id_offset - first));
    bool match = check_case ? (name == string_to_replace) :
      gxcfg_iequal(name, string_to_replace);
    if(!match) {
      lines.push_back(std::move(line));
      continue;
    }
    line.erase(first + name.size());
    line += parm_ID;
    line += string_to_insert;
    num_processed++;
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
  // 11/01/2026: Write a temporary file and rename it over the config
  // file, so a crash or full disk cannot leave a truncated config
  std::string tmp_name = FileName + ".tmp";
  {
    std::ofstream outfile(tmp_name, std::ios::out | std::ios::binary | 
			  std::ios::trunc);
    if(!outfile) return 0;
    for(const auto &l : lines) outfile << l << eol;
    outfile.close();
    if(!outfile) {
      std::error_code ec;
      std::filesystem::remove(tmp_name, ec);
      return 0;
    }
  }
  {
    std::error_code ec;
    // Keep the original file's permissions
    std::filesystem::file_status st = std::filesystem::status(FileName, ec);
    if(!ec) std::filesystem::permissions(tmp_name, st.permissions(), ec);
    ec.clear();
    std::filesystem::rename(tmp_name, FileName, ec);
    if(ec) {
      std::filesystem::remove(tmp_name, ec);
      return 0;
    }
  }
  
  // Make the changes to the config values in memory
  ReLoad();
  return num_processed;
}

int gxConfig::ChangeConfigValue(std::string_view parm, std::string_view value)
// Change a parameter in the config file, or append it if it does not
// exist. Do not include the parameter ID string in parm or value.
{
  // 11/01/2026: Reject names and values that would add lines to the file
  if(parm.empty() || gxcfg_has_eol(parm) || gxcfg_has_eol(value)) return 0;
  if(!parm_ID.empty() && parm.find(parm_ID) != std::string_view::npos) {
    return 0;
  }
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
