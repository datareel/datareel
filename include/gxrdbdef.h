// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxrdbdef.h 
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 08/09/2001
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

Relational database definition file functions used with the 
DataReel RDBMS definition file format.
*/
// ----------------------------------------------------------- //  
#ifndef __GX_RDBMS_DEF_HPP__
#define __GX_RDBMS_DEF_HPP__

#include "gxdlcode.h"

#include "gxrdbhdr.h"
#include "gxrdbms.h"

GXDLCODE_API void gxrdFormatMessage(gxString &mesg, 
				    const gxString &process_name);
GXDLCODE_API int gxrdReadDefFile(const gxString &fname, 
				 const gxString &process_name,
				 const char *TAG, gxString &def, 
				 gxString &error_message);
GXDLCODE_API int gxrdReadDefFile(const gxString &fname, 
				 const gxString &process_name,
				 gxString &table_def,
				 gxString &key_def,
				 gxString &error_message);
GXDLCODE_API int gxrdReadDefFile(const gxString &fname, 
				 const gxString &process_name,
				 gxString &table_def,
				 gxString &key_def,
				 gxString &form_def,
				 gxString &fkey_def,
				 gxString &error_message);
GXDLCODE_API int gxrdReadDefFile(const gxString &fname, 
				 const gxString &process_name,
				 gxString &table_def,
				 gxString &key_def,
				 gxString &form_def,
				 gxString &fkey_def,
				 gxString &user_def,
				 gxString &group_def,
				 gxString &error_message);
GXDLCODE_API int gxrdReadTableDef(const gxString &fname, 
				  const gxString &process_name,
				  gxString &table_def, 
				  gxString &error_message);
GXDLCODE_API int gxrdReadKeyDef(const gxString &fname, 
				const gxString &process_name,
				gxString &key_def, 
				gxString &error_message);
GXDLCODE_API int gxrdReadFormDef(const gxString &fname, 
				 const gxString &process_name,
				 gxString &form_def, 
				 gxString &error_message);
GXDLCODE_API int gxrdReadFKeyDef(const gxString &fname, 
				 const gxString &process_name,
				 gxString &fkey_def, 
				 gxString &error_message);
GXDLCODE_API int gxrdReadUserDef(const gxString &fname, 
				 const gxString &process_name,
				 gxString &user_def, 
				 gxString &error_message);
GXDLCODE_API int gxrdReadGroupDef(const gxString &fname, 
				  const gxString &process_name,
				  gxString &group_def, 
				  gxString &error_message);

#endif // __GX_RDBMS_DEF_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //

