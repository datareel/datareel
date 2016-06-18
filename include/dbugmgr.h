// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: dbugmgr.h
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 02/04/1997
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

gxDatabase debug manager class used by applications to troubleshoot 
and repair damaged or corrupt gxDatabase files.
*/
// ----------------------------------------------------------- //   
#ifndef __GX_DATABASE_DEBUG_MANAGER_HPP__
#define __GX_DATABASE_DEBUG_MANAGER_HPP__

#include "gxdlcode.h"

#include "gxdstats.h"
#include "gxstring.h"

class GXDLCODE_API gxDatabaseDebugManager : public gxDatabase
{
public:
  gxDatabaseDebugManager() { }
  ~gxDatabaseDebugManager() { }

public: // Database debug functions
  gxDatabaseError BlindOpen(const char *fname, 
			    __SBYTE__ last_rev = gxDatabaseRevisionLetter);
  FAU_t BlockSearch(FAU_t offset); 

public: // Database debug utilities
  int CheckFileSize(const char *fname);
  gxDatabaseError AnalyzeHeader(int test_ver = 1, int test_rev = 1);
  gxDatabaseError AnalyzeHeader(const char *sig, int sig_len,
				FAU ver, int test_ver, int test_rev);
  int Rebuild(const char *fname, int update_ver = 0);
  int Rebuild(const char *fname, FAU_t *num_objects, FAU ver, __SBYTE__ rev, 
	      FAU_t static_area, int copy_static_data);
  int Rebuild(const char *fname, FAU_t *num_objects, const char *sig, 
	      int sig_len, FAU ver, __SBYTE__ rev, gxUINT32 check_word,
	      FAU_t static_area, int copy_static_data);
  void SetLastKnownRev(__SBYTE__ last_rev) { rev_letter = last_rev; }
  int VerifyBlockHeader(const gxBlockHeader &hdr);
  int VerifyFileHeader(const gxFileHeader &fh);
  int VerifyRevLetter(__SBYTE__ rev);
  int VerifyVersion(long ver);
  void ResetFileHeader(gxFileHeader &fh);

public:
  gxString status_message;
};

#endif // __GX_DATABASE_DEBUG_MANAGER_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
