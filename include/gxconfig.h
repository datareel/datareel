// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Header File Name: gxconfig.h
// C++ Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/19/1996 
// Date Last Modified: 10/26/2026
// Copyright (c) 2001-2026 DataReel Software Development
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

10/26/2026: DataReel 7 rewrites gxConfig on std::string and 
std::vector. It no longer derives from gxListB. Parameters are 
stored as name/value pairs and lookups match names only. Load() 
and the Write functions return true on success. GetStrValue() 
returns a null pointer when the name is not found; GetString() 
returns a default value instead.
==============================================================
*/
// ----------------------------------------------------------- // 
#ifndef __GX_CONFIG_HPP__
#define __GX_CONFIG_HPP__

#include "gxdlcode.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace datareel {

// Configuration manager class
class GXDLCODE_API gxConfig
{
public:
  gxConfig() { }
  explicit gxConfig(const std::string &fname, 
		    const std::string &p_id = "=") :
    FileName(fname), parm_ID(p_id) { }
  ~gxConfig() { }

public: // Functions used to load and unload config file
  bool Load();
  bool Load(const std::string &fname);
  bool ReLoad();
  bool ReLoad(const std::string &fname);
  void UnLoad() { entries.clear(); }
  const std::string &GetFileName() const { return FileName; }
  void SetFileName(const std::string &s) { FileName = s; }
  
public: // Formatting functions
  // Treat all characters after the ID string as a parameter value
  void SetParmID(const std::string &s) { parm_ID = s; }
  const std::string &GetParmID() const { return parm_ID; }
  char GetCommentChar() const { return comment_char; }
  void SetCommentChar(char c) { comment_char = c; }
  void FilterComments() { filter_comments = true; }
  void ReadComments() { filter_comments = false; }
  
public: // Functions used to read config file values
  // Each lookup matches the whole parameter name, or any name that
  // contains it when full_name is false. The first match wins.
  const char *GetStrValue(std::string_view name, bool full_name = true) const;
  std::string GetString(std::string_view name, 
			const std::string &default_value = std::string(),
			bool full_name = true) const;
  bool HasValue(std::string_view name, bool full_name = true) const {
    return GetStrValue(name, full_name) != nullptr;
  }
  int GetIntValue(std::string_view name, bool full_name = true) const;
  long GetLongValue(std::string_view name, bool full_name = true) const;
  double GetFloatValue(std::string_view name, bool full_name = true) const;

  // Every name/value pair in file order
  const std::vector<std::pair<std::string, std::string>> &Entries() const {
    return entries;
  }

public: // Functions used to write config values
  bool WriteConfigLine(std::string_view parm, std::string_view value);
  bool WriteCommentLine(std::string_view s);
  bool WriteLine(std::string_view s);

  // Change a parameter in the file, or append it if it is not there.
  // Returns the number of lines changed, 1 for an append, 0 on error.
  int ChangeConfigValue(std::string_view parm, std::string_view value);
  int ChangeConfigValue(std::string_view parm, const char *value) {
    return ChangeConfigValue(parm, std::string_view(value ? value : ""));
  }
  int ChangeConfigValue(std::string_view parm, const std::string &value) {
    return ChangeConfigValue(parm, std::string_view(value));
  }
  template<class T, 
	   class = std::enable_if_t<std::is_arithmetic<T>::value>>
  int ChangeConfigValue(std::string_view parm, T value) {
    return ChangeConfigValue(parm, std::string_view(std::to_string(value)));
  }
  
private: // Internal proccessing functions
  void StoreCfgData(std::string_view cfg_line);
  const std::string *FindValue(std::string_view name, bool full_name) const;
  int ChangeConfigLine(std::string_view string_to_replace,
		       std::string_view string_to_insert,
		       bool check_case, bool append_lines);
  bool AppendToFile(const std::string &text);
  
private:
  std::string FileName;     // Name of the configuration file
  std::string parm_ID = "="; // String used identify parameters in the file
  char comment_char = '#';  // Char used to ID comments in the config file
  bool filter_comments = true; // If true all comments will be ignored
  std::vector<std::pair<std::string, std::string>> entries;
};

} // namespace datareel

#endif // __GX_CONFIG_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
