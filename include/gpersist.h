// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gpersist.h
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/18/1997
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

The Persistent base class is used to define the interface 
that makes an object persistent. 

Changes:
==============================================================
02/08/2002: All gxPersistent functions using non-persistent file 
address units have been modified to use FAU_t data types instead 
of FAU types. FAU_t types are native or built-in integer types 
and require no additional overhead to process. All persistent 
file address units still use the FAU integer type, which is a 
platform independent type allowing database files to be shared 
across multiple platforms. NOTE: This change was not applied to
any pure virtual functions.

02/22/2002: Changed default arguments for all functions using the
"flush" and "test_tree" variables to false. The caller or derived 
class must perform file flushing and tree testing as required by 
the application.

05/07/2002: Added the gxPersistent::Flush() function used by
applications to flush the POD database.

05/07/2002: Added the gxPersistent::TestDatabase() function
used by applications to test the POD database.

05/07/2002: Added the gxPersistent::GetDatabase() function
used to return the current POD database an object is bound
to.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_DATABASE_PERSISTENT_BASE_HPP__
#define __GX_DATABASE_PERSISTENT_BASE_HPP__

#include "gxdlcode.h"

#include "pod.h"

// Object header use to ID objects in the data file
struct GXDLCODE_API gxObjectHeader
{
  gxClassID ClassID;   // Class Identification number
  gxObjectID ObjectID; // Object Identification number
};

// GXD Persistent base class
class GXDLCODE_API gxPersistent
{
public:
  gxPersistent(POD *DB) { Connect(DB); objectaddress = 0; }
  gxPersistent(const POD *DB) { Connect(DB); objectaddress = 0; }
  gxPersistent() { pod = 0; } 

private:
  gxPersistent(const gxPersistent &ob) { }   // Disallow coping
  void operator=(const gxPersistent &ob) { } // Disallow assignment
  
protected: // Derived class interface
  virtual gxDatabaseError Read(FAU object_address) = 0;
  virtual gxDatabaseError Write() = 0;  
  virtual int Find() = 0;
  virtual int Delete() = 0;
  virtual __UWORD__ ObjectLength() = 0;
  
protected: 
  gxDatabaseError WriteObjectHeader(const gxObjectHeader &oh, 
				    FAU_t object_address = (FAU_t)-1);
  gxDatabaseError ReadObjectHeader(gxObjectHeader &oh, 
				   FAU_t object_address = (FAU_t)-1);
  gxDatabaseError Connect(POD *DB);
  gxDatabaseError Connect(const POD *DB);
  __UWORD__ StringFileLength(const char *s);
  __UWORD__ StringFileLength(char *s); 
  gxDatabaseError WriteString(const char *s, FAU_t file_address = (FAU_t)-1);
  gxDatabaseError WriteString(char *s, FAU_t file_address = (FAU_t)-1);
  char *ReadString(FAU_t file_address = (FAU_t)-1);
  
  // Index file functions
  int AddKey(DatabaseKeyB &key, DatabaseKeyB &compare_key,
	     unsigned index_number = 0, int flushdb = 0);
  int FindKey(DatabaseKeyB &key, DatabaseKeyB &compare_key,
	      unsigned index_number = 0, int test_tree = 0);
  int DeleteKey(DatabaseKeyB &key, DatabaseKeyB &compare_key,
		unsigned index_number = 0, int flushdb = 0);
  
public: // User interface
  int WriteObject(int find = 1);
  int ReadObject(FAU_t object_address);
  int ReadObject();
  int FindObject() { return Find(); }
  int DeleteObject(FAU_t object_address);
  int DeleteObject() { return Delete(); }
  FAU_t ObjectAddress() { return objectaddress; }
  void Flush() { if(pod) pod->Flush(); }
  int TestDatabase() { return pod->TestDatabase(); }
  POD *GetDatabase() { return pod; }
  int UsingIndex() { return pod->UsingIndex() == 1; }
  int UsingIndex() const { return pod->UsingIndex() == 1; }

  // TODO: 02/24/2009: Need to make the connect function public
  // TODO: 02/24/2009: Need to updata mthread examples after made public
  // gxDatabaseError Connect(POD *DB);
  // gxDatabaseError Connect(const POD *DB);

  
protected:
  POD *pod; // Pointer to the current POD manager
  gxObjectID_t objectaddress; // Data file address of this object
};

#endif // __GX_DATABASE_PERSISTENT_BASE_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
