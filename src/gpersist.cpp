// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: gpersist.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/18/1997
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

The Persistent base class is used to define the interface 
that makes an object persistent. 

Changes:
==============================================================
05/27/2002: Fixed possible memory leak in the gxPersistent::ReadString()
function that could occur during a database read error condition.
==============================================================
*/
// ----------------------------------------------------------- //   
#include "gxdlcode.h"

#include "gpersist.h"

#ifdef __BCC32__
#pragma warn -8080
#endif

gxDatabaseError gxPersistent::Connect(POD *DB) 
{
  if(!DB->OpenDataFile()) {
#ifdef CPP_EXCEPTIONS
    throw gxCDatabaseException();
#endif
    return gxDBASE_NO_DATABASE_OPEN;
  }
  pod = DB;
  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxPersistent::Connect(const POD *DB) 
{
  if(!DB->OpenDataFile()) {
#ifdef CPP_EXCEPTIONS
    throw gxCDatabaseException();
#endif
    return gxDBASE_NO_DATABASE_OPEN;
  }

  // PC-lint 05/27/2002: Does not like type cast to
  // non-const but cast is required.
  pod = (POD *)DB;
  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxPersistent::WriteObjectHeader(const gxObjectHeader &oh,
						FAU_t object_address)
{
  return pod->OpenDataFile()->Write(&oh, sizeof(gxObjectHeader),
				    object_address);
}

gxDatabaseError gxPersistent::ReadObjectHeader(gxObjectHeader &oh,
					       FAU_t object_address)
{
  return pod->OpenDataFile()->Read(&oh, sizeof(gxObjectHeader),
				   object_address);
}

__UWORD__ gxPersistent::StringFileLength(const char *s)
// Calculates the total number of bytes to be allocated for
// a character string in the data file.
{
  __UWORD__ len;
  // Ensure at least one byte is alloacted
  if(s) len = strlen(s); else len = 1; 
  return len + sizeof(gxUINT32); 
}

__UWORD__ gxPersistent::StringFileLength(char *s)
// Calculates the total number of bytes to be allocated for
// a character string in the data file.
{
  __UWORD__ len;
  // Ensure at least one byte is alloacted
  if(s) len = strlen(s); else len = 1;
  return len + sizeof(gxUINT32); 
}

gxDatabaseError gxPersistent::WriteString(const char *s, FAU_t file_address)
// Function used to write a null terminated character string
// to the data file. Returns a non-zero value to indicate an error
// condition or zero if successful.
{
  gxUINT32 len;
  const char null_byte = 0; 
  
  // Record the logical length of the string and ensure at least one
  // byte is written if this is a null pointer
  if(s)
    len = strlen(s);
  else
    len = 1;

  // Record the string length
  if(pod->OpenDataFile()->Write(&len, sizeof(gxUINT32), file_address) !=
     gxDBASE_NO_ERROR) {
    return pod->GetDataFileError();
  }

  if(s) { // Write the string to the file
    if(pod->OpenDataFile()->Write(s, len) != gxDBASE_NO_ERROR) {
      return pod->GetDataFileError();
    }
  }
  else { // Ensure that one byte is written if this a null pointer
    if(pod->OpenDataFile()->Write(&null_byte, len) != gxDBASE_NO_ERROR) {
      return pod->GetDataFileError();
    }
  }

  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxPersistent::WriteString(char *s, FAU_t file_address)
// Function used to write a null terminated character string
// to the data file. Returns a non-zero value to indicate an
// error condition or zero if successful.
{
  gxUINT32 len;
  const char null_byte = 0; 
  
  // Record the logical length of the string and ensure at least one
  // byte is written if this is a null pointer
  if(s)
    len = strlen(s);
  else
    len = 1;

  // Record the string length
  if(pod->OpenDataFile()->Write(&len, sizeof(gxUINT32), file_address) !=
     gxDBASE_NO_ERROR) {
    return pod->GetDataFileError();
  }

  if(s) { // Write the string to the file
    if(pod->OpenDataFile()->Write(s, len) != gxDBASE_NO_ERROR) {
      return pod->GetDataFileError();
    }
  }
  else { // Ensure that one byte is written if this a null pointer
    if(pod->OpenDataFile()->Write(&null_byte, len) != gxDBASE_NO_ERROR) {
      return pod->GetDataFileError();
    }
  }

  return gxDBASE_NO_ERROR;
}

char *gxPersistent::ReadString(FAU_t file_address)
// Function used to read a character string from the data file.
// Returns a pointer to a null terminated string or a null
// value if an error occurs. NOTE: The calling function must
// free the memory allocated for this string.
{
  gxUINT32 len;

  // Obtain the string length
  if(pod->OpenDataFile()->Read(&len, sizeof(gxUINT32), file_address) !=
     gxDBASE_NO_ERROR) {
    return 0;
  }

  char *sbuf = new char[len +1]; // Account for the null terminator

  if(!sbuf) { // Could not allocate memory for the string
    pod->SetDataFileError(gxDBASE_MEM_ALLOC_ERROR);
    return 0;
  }
  
  if(pod->OpenDataFile()->Read(sbuf, len) != gxDBASE_NO_ERROR) {
    delete[] sbuf;
    return 0;
  }
  
  sbuf[len] = 0; // Null terminate the string

  return sbuf;
}

int gxPersistent::WriteObject(int find)
// Will search the entire index file or database for the object
// if find is true.
{
  if(find) {
    if(!Find()) {
      if(Write() != gxDBASE_NO_ERROR) return 0;
    }
  }
  else {
    if(Write() != gxDBASE_NO_ERROR) return 0;
  }
  return 1;
}

int gxPersistent::DeleteObject(FAU_t object_address)
// Delete the object at the specified address
{
  return pod->OpenDataFile()->Delete(object_address);
}

int gxPersistent::AddKey(DatabaseKeyB &key, DatabaseKeyB &compare_key,
			 unsigned index_number, int flushdb)
{
  return pod->Index(index_number)->Insert(key, compare_key, flushdb);
}

int gxPersistent::FindKey(DatabaseKeyB &key, DatabaseKeyB &compare_key,
			  unsigned index_number, int test_tree)
{
  return pod->Index(index_number)->Find(key, compare_key, test_tree);
}

int gxPersistent::DeleteKey(DatabaseKeyB &key, DatabaseKeyB &compare_key,
			    unsigned index_number, int flushdb)
{
  return pod->Index(index_number)->Delete(key, compare_key, flushdb);
}

int gxPersistent::ReadObject(FAU_t object_address)
{
  if(Read(object_address) != gxDBASE_NO_ERROR) return 0;
  objectaddress = object_address;
  return 1;
}

int gxPersistent::ReadObject()
{
  if(Read(objectaddress) != gxDBASE_NO_ERROR) return 0;
  return 1;
}

#ifdef __BCC32__
#pragma warn .8080
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
