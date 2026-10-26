// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxrdbdef.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 08/09/2001
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

Relational database definition file functions used with the 
DataReel RDBMS definition file format.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <cerrno>
#include <cstring>
#include <fstream>
#include "gxrdbdef.h"
#include "strutil.h"
#include "systime.h"

namespace datareel {




GXDLCODE_API void gxrdFormatMessage(std::string &mesg, 
				    const std::string &process_name)
{
  mesg = syslog_time() + " " + process_name + ": " + mesg;
}

GXDLCODE_API int gxrdReadDefFile(const std::string &fname, 
				 const std::string &process_name,
				 const char *TAG, std::string &def, 
				 std::string &error_message)
{
  def.clear();
  error_message.clear();
  std::string sbuf, mesg;
  int found_start_tag = 0;
  int found_end_tag = 0;
  std::string line;

  std::ifstream def_file(fname, std::ios::in | std::ios::binary);
  if(!def_file) {
    mesg.clear();
    str_append(mesg, "Cannot open def file: ", fname);
    gxrdFormatMessage(mesg, process_name);
    error_message.clear();
    str_append(error_message, mesg, "\n");
    mesg.clear();
    str_append(mesg, std::strerror(errno));
    gxrdFormatMessage(mesg, process_name);
    str_append(error_message, mesg);
    return 0;
  }
  
  while(std::getline(def_file, line)) {
    if(def_file.bad()) {
      mesg.clear();
      str_append(mesg, "Error reading def file: ", fname);
      gxrdFormatMessage(mesg, process_name);
      error_message.clear();
      str_append(error_message, mesg, "\n");
      mesg.clear();
      str_append(mesg, std::strerror(errno));
      gxrdFormatMessage(mesg, process_name);
      str_append(error_message, mesg);
      return 0;
    }
    sbuf = line;
    
    // Trim leading spaces
    trim_in_place(sbuf);
    
    // Filter comment strings, nulls, and blank lines
    if(sbuf[0] == '#') continue;
    if(sbuf == "") continue;
    if(sbuf == " ") continue;

    if(ifind(sbuf, TAG) != std::string::npos) {
      found_start_tag = 1;
    }
    if(found_start_tag == 1) {
      str_append(def, sbuf);
      if(ifind(sbuf, "};") != std::string::npos) {
	found_end_tag = 1;
	break;
      }
    }
  } 
  def_file.close();
  if(!found_start_tag) {
    mesg.clear();
    str_append(mesg, "Syntax error in def file: ", fname);
    gxrdFormatMessage(mesg, process_name);
    error_message.clear();
    str_append(error_message, mesg, "\n");
    mesg.clear();
    str_append(mesg, TAG, " missing start tag in def file: ", fname);
    gxrdFormatMessage(mesg, process_name);
    str_append(error_message, mesg);
  }
  
  if(!found_end_tag) {
    mesg.clear();
    str_append(mesg, "Syntax error in def file: ", fname);
    gxrdFormatMessage(mesg, process_name);
    error_message.clear();
    str_append(error_message, mesg, "\n");
    mesg.clear();
    str_append(mesg, TAG, " missing end tag in def file: ", fname);
    gxrdFormatMessage(mesg, process_name);
    str_append(error_message, mesg);
  }

  delete_before_including(def, "{");
  delete_after_including(def, "};");

  return 1;
}

GXDLCODE_API int gxrdReadDefFile(const std::string &fname, 
				 const std::string &process_name,
				 std::string &table_def,
				 std::string &key_def,
				 std::string &error_message)
{
  if(!gxrdReadTableDef(fname, process_name, table_def, error_message)) {
    return 0;
  }
  if(!gxrdReadKeyDef(fname, process_name, key_def, error_message)) {
    return 0;
  }
  return 1;
}

GXDLCODE_API int gxrdReadDefFile(const std::string &fname, 
				 const std::string &process_name,
				 std::string &table_def,
				 std::string &key_def,
				 std::string &form_def,
				 std::string &fkey_def,
				 std::string &error_message)
{
  if(!gxrdReadTableDef(fname, process_name, table_def, error_message)) {
    return 0;
  }
  if(!gxrdReadKeyDef(fname, process_name, key_def, error_message)) {
    return 0;
  }
  if(!gxrdReadFormDef(fname, process_name, form_def, error_message)) {
    return 0;
  }
  if(!gxrdReadFKeyDef(fname, process_name, fkey_def, error_message)) {
    return 0;
  }
  return 1;
}

GXDLCODE_API int gxrdReadDefFile(const std::string &fname, 
				 const std::string &process_name,
				 std::string &table_def,
				 std::string &key_def,
				 std::string &form_def,
				 std::string &fkey_def,
				 std::string &user_def,
				 std::string &group_def,
				 std::string &error_message)
{
  if(!gxrdReadTableDef(fname, process_name, table_def, error_message)) {
    return 0;
  }
  if(!gxrdReadKeyDef(fname, process_name, key_def, error_message)) {
    return 0;
  }
  if(!gxrdReadFormDef(fname, process_name, form_def, error_message)) {
    return 0;
  }
  if(!gxrdReadFKeyDef(fname, process_name, fkey_def, error_message)) {
    return 0;
  }
  if(!gxrdReadUserDef(fname, process_name, user_def, error_message)) {
    return 0;
  }
  if(!gxrdReadGroupDef(fname, process_name, group_def, error_message)) {
    return 0;
  }
  return 1;
}

GXDLCODE_API int gxrdReadTableDef(const std::string &fname, 
				  const std::string &process_name,
				  std::string &table_def, 
				  std::string &error_message)
{
  return gxrdReadDefFile(fname, process_name, "DEFINE TABLE", table_def,
			 error_message);

}

GXDLCODE_API int gxrdReadKeyDef(const std::string &fname, 
				const std::string &process_name,
				std::string &key_def, 
				std::string &error_message)
{
  return gxrdReadDefFile(fname, process_name, "DEFINE KEYS", key_def,
			  error_message);
}

GXDLCODE_API int gxrdReadFormDef(const std::string &fname, 
				   const std::string &process_name,
				   std::string &form_def, 
				   std::string &error_message)
{
 return gxrdReadDefFile(fname, process_name, "DEFINE FORM", form_def,
			error_message);
}

GXDLCODE_API int gxrdReadFKeyDef(const std::string &fname, 
				 const std::string &process_name,
				 std::string &fkey_def, 
				 std::string &error_message)
{
 return gxrdReadDefFile(fname, process_name, "DEFINE FKEYS", fkey_def,
			error_message);
}

GXDLCODE_API int gxrdReadUserDef(const std::string &fname, 
				 const std::string &process_name,
				 std::string &user_def, 
				 std::string &error_message)
{
 return gxrdReadDefFile(fname, process_name, "DEFINE USER ACCESS", 
			user_def, error_message);

}

GXDLCODE_API int gxrdReadGroupDef(const std::string &fname, 
				  const std::string &process_name,
				  std::string &group_def, 
				  std::string &error_message)
{
  return gxrdReadDefFile(fname, process_name, "DEFINE GROUP ACCESS", 
			 group_def, error_message);
}

} // namespace datareel

// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
