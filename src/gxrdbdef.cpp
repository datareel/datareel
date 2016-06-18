// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxrdbdef.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 08/09/2001
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

Relational database definition file functions used with the 
DataReel RDBMS definition file format.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "gxrdbdef.h"
#include "dfileb.h"
#include "systime.h"

#ifdef __BCC32__
#pragma warn -8080
#endif

GXDLCODE_API void gxrdFormatMessage(gxString &mesg, 
				    const gxString &process_name)
{
  SysTime systime;
  gxString sbuf(mesg);
  mesg.Clear();
  mesg << systime.GetSyslogTime() << " " << process_name
       << ": " << sbuf;
}

GXDLCODE_API int gxrdReadDefFile(const gxString &fname, 
				 const gxString &process_name,
				 const char *TAG, gxString &def, 
				 gxString &error_message)
{
  DiskFileB def_file;
  def.Clear();
  error_message.Clear();
  gxString sbuf, mesg;
  const int lbuf_size = 255;
  int found_start_tag = 0;
  int found_end_tag = 0;
  char lbuf[lbuf_size];

  if(def_file.df_Open(fname.c_str(), DiskFileB::df_READONLY) != 
     DiskFileB::df_NO_ERROR) {
    mesg.Clear();
    mesg << "Cannot open def file: " << fname;
    gxrdFormatMessage(mesg, process_name);
    error_message.Clear();
    error_message << mesg << "\n";
    mesg.Clear();
    mesg << def_file.df_ExceptionMessage();
    gxrdFormatMessage(mesg, process_name);
    error_message << mesg;
    return 0;
  }
  
  while(!def_file.df_EOF()) {
    def_file.df_GetLine(lbuf, lbuf_size, '\n', 1);
    if(def_file.df_GetError() != DiskFileB::df_NO_ERROR) {
      mesg.Clear();
      mesg << "Error reading def file: " << fname;
      gxrdFormatMessage(mesg, process_name);
      error_message.Clear();
      error_message << mesg << "\n";
      mesg.Clear();
      mesg << def_file.df_ExceptionMessage();
      gxrdFormatMessage(mesg, process_name);
      error_message << mesg;
      def_file.df_Close();
      return 0;
    }
    sbuf = lbuf;
    
    // Trim leading spaces
    sbuf.TrimLeadingSpaces();
    sbuf.TrimTrailingSpaces();
    
    // Filter comment strings, nulls, and blank lines
    if(sbuf[0] == '#') continue;
    if(sbuf == "") continue;
    if(sbuf == " ") continue;

    if(sbuf.IFind(TAG) != -1) {
      found_start_tag = 1;
    }
    if(found_start_tag == 1) {
      def << sbuf;
      if(sbuf.IFind("};") != -1) {
	found_end_tag = 1;
	break;
      }
    }
  } 
  def_file.df_Close();
  if(!found_start_tag) {
    mesg.Clear();
    mesg << "Syntax error in def file: " << fname;
    gxrdFormatMessage(mesg, process_name);
    error_message.Clear();
    error_message << mesg << "\n";
    mesg.Clear();
    mesg << TAG << " missing start tag in def file: " 
	 << fname;
    gxrdFormatMessage(mesg, process_name);
    error_message << mesg;
  }
  
  if(!found_end_tag) {
    mesg.Clear();
    mesg << "Syntax error in def file: " << fname;
    gxrdFormatMessage(mesg, process_name);
    error_message.Clear();
    error_message << mesg << "\n";
    mesg.Clear();
    mesg << TAG << " missing end tag in def file: " 
	 << fname;
    gxrdFormatMessage(mesg, process_name);
    error_message << mesg;
  }

  def.DeleteBeforeIncluding("{");
  def.DeleteAfterIncluding("};");

  return 1;
}

GXDLCODE_API int gxrdReadDefFile(const gxString &fname, 
				 const gxString &process_name,
				 gxString &table_def,
				 gxString &key_def,
				 gxString &error_message)
{
  if(!gxrdReadTableDef(fname, process_name, table_def, error_message)) {
    return 0;
  }
  if(!gxrdReadKeyDef(fname, process_name, key_def, error_message)) {
    return 0;
  }
  return 1;
}

GXDLCODE_API int gxrdReadDefFile(const gxString &fname, 
				 const gxString &process_name,
				 gxString &table_def,
				 gxString &key_def,
				 gxString &form_def,
				 gxString &fkey_def,
				 gxString &error_message)
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

GXDLCODE_API int gxrdReadDefFile(const gxString &fname, 
				 const gxString &process_name,
				 gxString &table_def,
				 gxString &key_def,
				 gxString &form_def,
				 gxString &fkey_def,
				 gxString &user_def,
				 gxString &group_def,
				 gxString &error_message)
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

GXDLCODE_API int gxrdReadTableDef(const gxString &fname, 
				  const gxString &process_name,
				  gxString &table_def, 
				  gxString &error_message)
{
  return gxrdReadDefFile(fname, process_name, "DEFINE TABLE", table_def,
			 error_message);

}

GXDLCODE_API int gxrdReadKeyDef(const gxString &fname, 
				const gxString &process_name,
				gxString &key_def, 
				gxString &error_message)
{
  return gxrdReadDefFile(fname, process_name, "DEFINE KEYS", key_def,
			  error_message);
}

GXDLCODE_API int gxrdReadFormDef(const gxString &fname, 
				   const gxString &process_name,
				   gxString &form_def, 
				   gxString &error_message)
{
 return gxrdReadDefFile(fname, process_name, "DEFINE FORM", form_def,
			error_message);
}

GXDLCODE_API int gxrdReadFKeyDef(const gxString &fname, 
				 const gxString &process_name,
				 gxString &fkey_def, 
				 gxString &error_message)
{
 return gxrdReadDefFile(fname, process_name, "DEFINE FKEYS", fkey_def,
			error_message);
}

GXDLCODE_API int gxrdReadUserDef(const gxString &fname, 
				 const gxString &process_name,
				 gxString &user_def, 
				 gxString &error_message)
{
 return gxrdReadDefFile(fname, process_name, "DEFINE USER ACCESS", 
			user_def, error_message);

}

GXDLCODE_API int gxrdReadGroupDef(const gxString &fname, 
				  const gxString &process_name,
				  gxString &group_def, 
				  gxString &error_message)
{
  return gxrdReadDefFile(fname, process_name, "DEFINE GROUP ACCESS", 
			 group_def, error_message);
}

#ifdef __BCC32__
#pragma warn .8080
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
