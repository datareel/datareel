// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: pod.cpp
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

The POD (Persistent Object Database) manager is used to manage
file pointers to database data files and index files.
*/
// ----------------------------------------------------------- //   
#include "gxdlcode.h"

#include "pod.h"

#ifdef __BCC32__
#pragma warn -8080
#endif

PODIndexFile::PODIndexFile(unsigned num_indexes)
{
  // Ensure that the maximum number of index file is not exceeded
  if(num_indexes > POD_MAX_INDEX) num_indexes = POD_MAX_INDEX;

  // Clear the index file array
  unsigned i;
  for(i = 0; i < POD_MAX_INDEX; i++) index_file[i] = 0;

  n_indexes = num_indexes; 
  curr_index = 0;

  // Allocate memory for the specified number of index files
  for(i = 0; i < n_indexes; i++) {
    index_file[i] = new PODIndexFile_t;
    if(!index_file[i]) { // Check for any memory allocation errors
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#endif
    }
  }
}

PODIndexFile::~PODIndexFile()
{
  unsigned i;
  // PC-lint 09/08/2005: Function may throw exception in destructor
  for(i = 0; i < n_indexes; i++) delete index_file[i];
}

gxDatabaseError PODIndexFile::Open(const char *ifname,
				   unsigned index_number, 
				   DatabaseKeyB &key_type,
				   BtreeNodeOrder_t order,
				   gxDatabaseAccessMode mode,
				   int num_trees,
				   __SBYTE__ rev_letter)
// Function used to open an existing index file or create a new
// one if the specified file name does not exist. Returns a non-zero
// value to indicate an error condition or zero if successful.
{
  // Construct a new Btree for the specified index
  gxBtree *btree = new gxBtree(key_type, order);
  if(!btree) { 
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_MEM_ALLOC_ERROR;
#endif
  }
  index_file[index_number]->btree = btree;  
  curr_index = index_number;

  if(gxDatabase::Exists(ifname)) { // Open the index file if it exists
    return btree->Open(ifname, mode);
  }
  else { // Create a the index file
    return btree->Create(ifname, num_trees, rev_letter);
  }
}

gxDatabaseError PODIndexFile::Close(unsigned index_number)
// Function used to close an open index file. Returns a non-zero
// value to indicate an error condition or zero if successful.
{
  if(index_file[index_number]->btree) {
    if(index_file[index_number]->btree->Close() != gxDBASE_NO_ERROR) {
      return index_file[index_number]->btree->GetDatabaseError();
    }
    delete index_file[index_number]->btree;
    index_file[index_number]->btree = 0;
  }
  return gxDBASE_NO_ERROR;
}

POD::POD()
{
  opendatafile = 0;
  openindexfile = 0;
  exists = 0;
  using_index = 0;
}

POD::~POD() 
{ 
  // PC-lint 09/08/2005: Function may throw exception in destructor
  Close();
}

gxDatabaseError POD::Open(const char *dfname, const char *ifname,
			  DatabaseKeyB &key_type, BtreeNodeOrder_t order,
			  gxDatabaseAccessMode mode, int use_index,
			  FAU_t static_size, int num_trees,
			  __SBYTE__ df_rev_letter, __SBYTE__ if_rev_letter)
// Function used to open an existing database or create a new one using
// a single index file. Returns a non-zero value to indicate an error
// condition or zero if successful.
{
  // Close any currently open data and/or index files
  gxDatabaseError err = Close();
  if(err != gxDBASE_NO_ERROR) return err;
  
  unsigned num_indexes = 1;

  if(use_index) { // Initialize the indexing subsystem
    openindexfile = new PODIndexFile(num_indexes);
    if(!openindexfile) { // Could not allocate memory for the index file
      using_index = 0;
    }
    else {
      using_index = use_index;
      openindexfile->n_indexes = num_indexes;
      openindexfile->curr_index = 0;
      openindexfile->index_file[0]->dbkey_size = key_type.SizeOfDatabaseKey();
      openindexfile->index_file[0]->order = order;
      openindexfile->index_file[0]->use_index = 1;
    }
  }

  if(!gxDatabase::Exists(dfname)) { // Create a new data file
    if(CreateDataFile(dfname, static_size, df_rev_letter) != 
       gxDBASE_NO_ERROR) {
      return GetDataFileError();
    }
    exists = 0;
    if(using_index) {
      // Overwrite the index file if it exists since the index keys will
      // be useless without a valid data file.
      if(CreateIndex(ifname, 0, key_type, order, num_trees, if_rev_letter) !=
	 gxDBASE_NO_ERROR) {
	return GetIndexFileError();
      }
    }
  }
  else { // Open the existing data file
    if(OpenDataFile(dfname, mode) != gxDBASE_NO_ERROR) {
      return GetDataFileError();
    }
    exists = 1;
    if(using_index) { // Open the index file if it exist
      if(!gxDatabase::Exists(ifname)) {
	// This index file will have to be rebuilt by the application
	// PC-lint 09/15/2005: Possible use of null pointer
	if(openindexfile) openindexfile->index_file[0]->rebuild_index = 1;
	if(CreateIndex(ifname, 0, key_type, order, num_trees, if_rev_letter) !=
	   gxDBASE_NO_ERROR) {
	  return GetIndexFileError();
	}
      }
      else {
	if(OpenIndex(ifname, 0, key_type, order, mode) != gxDBASE_NO_ERROR) {
	  return GetIndexFileError();
	}
      }
    }
  }
  return gxDBASE_NO_ERROR;
}

gxDatabaseError POD::Open(const char *dfname, PODIndexFile *ix_ptr,
			  gxDatabaseAccessMode mode, FAU_t static_size,
			  __SBYTE__ df_rev_letter)
// Function used to open an existing database or create a new one using
// multiple index files with different key types. Returns a non-zero value
// to indicate an error condition or zero if successful.
{
  // Close any currently open data and/or index files
  gxDatabaseError err = Close();
  if(err != gxDBASE_NO_ERROR) return err;
  unsigned i;

  if(!gxDatabase::Exists(dfname)) { // Create a new datafile
    if(CreateDataFile(dfname, static_size, df_rev_letter) != 
       gxDBASE_NO_ERROR) {
      return GetDataFileError();
    }
    exists = 0;
    using_index = 1;

    // PC-lint 09/15/2005: Possible memory leak if openindexfile is not 
    // freed before assignment.
    if(openindexfile) {
      for(i = 0; i < openindexfile->n_indexes; i++) {
	if(CloseIndex(i) != gxDBASE_NO_ERROR) {
	  openindexfile->curr_index = i;
	  return GetIndexFileError(i);
	}
      }
      delete openindexfile;
      openindexfile = 0;
    }
    openindexfile = ix_ptr;
  }
  else { // Open the existing data file
    if(OpenDataFile(dfname, mode) != gxDBASE_NO_ERROR) {
      return GetDataFileError();
    }
    exists = 1;
    using_index = 1;

    // PC-lint 09/15/2005: Possible memory leak if openindexfile is not 
    // freed before assignment.
    if(openindexfile) {
      for(i = 0; i < openindexfile->n_indexes; i++) {
	if(CloseIndex(i) != gxDBASE_NO_ERROR) {
	  openindexfile->curr_index = i;
	  return GetIndexFileError(i);
	}
      }
      delete openindexfile;
      openindexfile = 0;
    }
    openindexfile = ix_ptr;
  }

  return gxDBASE_NO_ERROR;
}

gxDatabaseError POD::Open(const char *dfname, char *ifname[POD_MAX_INDEX],
			  unsigned num_indexes, DatabaseKeyB &key_type,
			  BtreeNodeOrder_t order, gxDatabaseAccessMode mode,
 			  FAU_t static_size, int num_trees,
			  __SBYTE__ df_rev_letter, __SBYTE__ if_rev_letter)
// Function used to open an existing database or create a new one using
// multiple index files that use the same key type. Returns a non-zero
// value to indicate an error condition or zero if successful.
{
  // Close any currently open data and/or index files
  gxDatabaseError err = Close();
  if(err != gxDBASE_NO_ERROR) return err;

  unsigned i;
  // PC-lint 09/15/2005: Possible memory leak if openindexfile is not 
  // freed before assignment.
  if(openindexfile) {
    for(i = 0; i < openindexfile->n_indexes; i++) {
      if(CloseIndex(i) != gxDBASE_NO_ERROR) {
	openindexfile->curr_index = i;
	return GetIndexFileError(i);
      }
    }
    delete openindexfile;
    openindexfile = 0;
  }
  openindexfile = new PODIndexFile(num_indexes);
  if(!openindexfile) {  // Memory allocation error
    using_index = 0;
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_MEM_ALLOC_ERROR;
#endif
  }
  else {
    using_index = 1;
    openindexfile->n_indexes = num_indexes;
    openindexfile->curr_index = 0;
    for(i = 0; i < num_indexes; i++) {
      openindexfile->index_file[i]->dbkey_size = key_type.SizeOfDatabaseKey();
      openindexfile->index_file[i]->order = order;
      openindexfile->index_file[i]->use_index = 1;
    }
  }
  
  if(!gxDatabase::Exists(dfname)) { // Create a new data file
    if(CreateDataFile(dfname, static_size, df_rev_letter) != 
       gxDBASE_NO_ERROR) {
      return GetDataFileError();
    }
    exists = 0;
    for(i = 0; i < num_indexes; i++) {
      // Overwrite all the index files since the index keys will
      // be useless without a valid data file.
      if(CreateIndex((const char *)ifname[i], i, key_type, order, 
		     num_trees, if_rev_letter) != gxDBASE_NO_ERROR) {
	return GetIndexFileError();
      }
    }
  }
  else { // Open the existing data file
    if(OpenDataFile(dfname, mode) != gxDBASE_NO_ERROR) {
      return GetDataFileError();
    }
    exists = 1;
    for(i = 0; i < num_indexes; i++) {
      if(!gxDatabase::Exists((const char *)ifname[i])) {
	// These index files will have to be rebuilt by the application
	openindexfile->index_file[i]->rebuild_index = 1;
	if(CreateIndex((const char *)ifname[i], i, key_type, order, 
		       num_trees, if_rev_letter) != gxDBASE_NO_ERROR) {
	  return GetIndexFileError();
	}
	
      }
      else {
	if(OpenIndex((const char *)ifname[i], i, key_type, order, mode) !=
	   gxDBASE_NO_ERROR) {
	  return GetIndexFileError();
	}
      }
    }
  }
  return gxDBASE_NO_ERROR;
}

void POD::Release()
// Reset the data file and index file pointers to zero without
// closing the files or performing any other action. NOTE: This
// is function used to reset the file pointers when more the one
// object is referencing this POD database and the database has
// been closed or the file pointers have been deleted.
{
#if defined (__PCLINT_CHECK__)
  return;
#else 
  opendatafile = 0;
  openindexfile = 0;
#endif
}

gxDatabaseError POD::Close()
// Function used to close the database, disconnecting both the data and
// index files. Returns a non-zero value to indicate an error condition
// or zero if successful.
{
  if(CloseDataFile() != gxDBASE_NO_ERROR) {
    return GetDataFileError();
  }
  if(openindexfile) {
    for(unsigned i = 0; i < openindexfile->n_indexes; i++) {
      if(CloseIndex(i) != gxDBASE_NO_ERROR) {
	openindexfile->curr_index = i;
	return GetIndexFileError(i);
      }
    }
    delete openindexfile;
    openindexfile = 0;
  }
  return gxDBASE_NO_ERROR;
}

gxDatabaseError POD::Flush()
// Function used to flush the data file and all the index files.
// Returns a non-zero value if an error occurs.
{
  gxDatabaseError err = gxDBASE_NO_ERROR;
  err = FlushDataFile();
  if(!openindexfile) return err;
  for(unsigned i = 0; i < openindexfile->n_indexes; i++) {
    err = FlushIndexFile(i);
  }
  return err;
}

int POD::TestDatabase()
// Function used to test the data file and all index files
// during multiple file access.
{
  int err = 0;
  err = TestDataFile();
  if(!openindexfile) return err;
  for(unsigned i = 0; i < openindexfile->n_indexes; i++) {
    err = TestIndexFile(i);
  }
  return err;
}

gxDatabaseError POD::CreateDataFile(const char *fname, FAU_t static_size,
				    __SBYTE__ rev_letter)
// Function used to create a new data file. The "static_size"
// variable is used to reserve a specified number of bytes that
// will not be affected by the dynamic allocation routines.
// Returns a non-zero value to indicate an error condition or
// zero if successful.
{
  // Close the current data file before creating a new one
  if(CloseDataFile() != gxDBASE_NO_ERROR) {
    return GetDataFileError();
  }
  opendatafile = new gxDatabase;
  if(!opendatafile) { // Check for any memory allocation errors
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_MEM_ALLOC_ERROR;
#endif
  }
  return opendatafile->Create(fname, static_size, rev_letter);
}

gxDatabaseError POD::CreateIndex(const char *fname, unsigned index_number, 
				 DatabaseKeyB &key_type,
				 BtreeNodeOrder_t order, 
				 int num_trees, __SBYTE__ rev_letter)
{
  if(num_trees <= 0) num_trees = 1;
  if(CloseIndex(index_number) != gxDBASE_NO_ERROR) {
    return GetIndexFileError(index_number);
  }
  
  gxBtree *btree = new gxBtree(key_type, order);
  if(!btree) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_MEM_ALLOC_ERROR;
#endif
  }

  // PC-lint 09/15/2005: Possible use of null pointer
  if(openindexfile) {
    openindexfile->index_file[index_number]->btree = btree;
  }
  
  return btree->Create(fname, num_trees, rev_letter);
}

void POD::ReleaseDataFile()
// Reset the data file pointer to zero without closing the
// file or performing any other action. NOTE: This function is
// used to reset the file pointer when more the one object is
// referencing this file pointer and the file has been closed
// or the pointer has been deleted.
{
#if defined (__PCLINT_CHECK__)
  return;
#else 
  opendatafile = 0;
#endif
}

gxDatabaseError POD::CloseDataFile()
// Disconnects the database from the file.
{
  if(opendatafile) {
    if(opendatafile->Close() != gxDBASE_NO_ERROR) {
      return GetDataFileError();
    }
    delete opendatafile;
    opendatafile = 0;
  }
  return gxDBASE_NO_ERROR;
}

void POD::ReleaseIndexFile()
// Reset the index file pointer to zero without closing the
// file or performing any other action. NOTE: This function is
// used to reset the file pointer when more the one object is
// referencing this file pointer and the file has been closed
// or the pointer has been deleted.
{
#if defined (__PCLINT_CHECK__)
  return;
#else 
  openindexfile = 0;
#endif
}

gxDatabaseError POD::CloseIndex(unsigned index_number)
// Disconnects the database from the index file.
{
  // PC-lint 09/15/2005: Possible use of null pointer
  if(!openindexfile) return gxDBASE_NO_ERROR;
  if(!openindexfile->index_file) return gxDBASE_NO_ERROR;

  gxBtree *btree = openindexfile->index_file[index_number]->btree;
 
  if(btree) {
    if(btree->Close() != gxDBASE_NO_ERROR) {
      return GetIndexFileError(index_number);
    }
    delete btree;
    openindexfile->index_file[index_number]->btree = 0;
    openindexfile->index_file[index_number]->use_index = 0;
  }
  return gxDBASE_NO_ERROR;
}

gxDatabaseError POD::FlushDataFile() 
// Function used to flush the open data file.
{ 
  if(!opendatafile) return gxDBASE_NO_ERROR;
  return opendatafile->Flush();
}

gxDatabaseError POD::FlushIndexFile(unsigned index_number) 
// Function used to flush the specified index file.
{ 
  if(!Index(index_number)) return gxDBASE_NO_ERROR;
  return Index(index_number)->Flush();
}

int POD::TestDataFile() 
// This function is used to ensure that the in memory copy
// of the data file header and the disk copy stay in sync
// during multiple file access.
{ 
  if(!opendatafile) return 0;
  return opendatafile->TestFileHeader();
}

int POD::TestIndexFile(unsigned index_number) 
// This function is used to ensure that the index file data stays in 
// sync during multiple file access.
{ 
  if(!Index(index_number)) return 0;
  return Index(index_number)->TestTree();
}

gxDatabaseError POD::OpenDataFile(const char *fname, gxDatabaseAccessMode mode)
{
  if(CloseDataFile() != gxDBASE_NO_ERROR) {
    return GetDataFileError();
  }
  
  // PC-lint 09/15/2005: Possible memory leak
  if(opendatafile) delete opendatafile;

  opendatafile = new gxDatabase;
  if(!opendatafile) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_MEM_ALLOC_ERROR;
#endif
  }
  return opendatafile->Open(fname, mode);
}

gxDatabaseError POD::OpenIndex(const char *fname, unsigned index_number, 
			       DatabaseKeyB &key_type, BtreeNodeOrder_t order, 
			       gxDatabaseAccessMode mode)
{
  if(CloseIndex(index_number) != gxDBASE_NO_ERROR) {
    return GetIndexFileError(index_number);
  }
  gxBtree *btree = new gxBtree(key_type, order);
  if(!btree) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    return gxDBASE_MEM_ALLOC_ERROR;
#endif
  }

  // PC-lint 09/15/2005: Possible use of null pointer
  if(!openindexfile) {
#ifdef __CPP_EXCEPTIONS__
    throw gxCDatabaseException();
#else
    delete btree;
    return gxDBASE_NULL_PTR;
#endif
  }
  if(!openindexfile->index_file) {
#ifdef __CPP_EXCEPTIONS__
    delete btree;
    throw gxCDatabaseException();
#else
    return gxDBASE_NULL_PTR;
#endif
  }

  openindexfile->index_file[index_number]->btree = btree;
  return btree->Open(fname, mode);
}

gxDatabase *POD::IndexDBPtr(unsigned index_number)
// Returns a pointer to the Btree's database file pointer for 
// the specified index number.
{
  gxBtree *btx = Index(index_number);
  if(btx) return btx->gxDatabasePtr();
  return 0;
}

gxDatabase *POD::IndexDBPtr(unsigned index_number) const
// Returns a pointer to the Btree's database file pointer for 
// the specified index number.
{
  gxBtree *btx = Index(index_number);
  if(btx) return btx->gxDatabasePtr();
  return 0;
}

gxBtree *POD::Index(unsigned index_number)
// Returns a pointer to the Btree index file for the specified
// index number.
{
  if(!openindexfile) return 0;
  if(index_number > openindexfile->n_indexes) return 0;
  // PC-lint 09/15/2005: Possible null pointer
  if(!openindexfile->index_file) return 0;
  return openindexfile->index_file[index_number]->btree;
}

gxBtree *POD::Index(unsigned index_number) const
// Returns a pointer to the Btree index file for the specified
// index number.
{
  if(!openindexfile) return 0;
  if(index_number > openindexfile->n_indexes) return 0;
  // PC-lint 09/15/2005: Possible null pointer
  if(!openindexfile->index_file) return 0;
  return openindexfile->index_file[index_number]->btree;
}

gxDatabaseError POD::GetDataFileError()
{
  if(!opendatafile) return gxDBASE_NO_DATABASE_OPEN;
  return opendatafile->GetDatabaseError();
}

gxDatabaseError POD::GetDataFileError() const
{
  if(!opendatafile) return gxDBASE_NO_DATABASE_OPEN;
  return opendatafile->GetDatabaseError();
}

gxDatabaseError POD::SetDataFileError(gxDatabaseError err)
{
  if(!opendatafile) return gxDBASE_NO_DATABASE_OPEN;
  return opendatafile->SetDatabaseError(err);
}

gxDatabaseError POD::GetIndexFileError(unsigned index_number)
{
  if(!Index(index_number)) return gxDBASE_NO_DATABASE_OPEN;
  // PC-lint 09/15/2005: Possible null pointer
  gxBtree * gxbtree = Index(index_number);
  if(!gxbtree) return gxDBASE_NO_DATABASE_OPEN;
  return gxbtree->GetDatabaseError();
}

gxDatabaseError POD::GetIndexFileError(unsigned index_number) const
{
  if(!Index(index_number)) return gxDBASE_NO_DATABASE_OPEN;
  // PC-lint 09/15/2005: Possible null pointer
  gxBtree * gxbtree = Index(index_number);
  if(!gxbtree) return gxDBASE_NO_DATABASE_OPEN;
  return gxbtree->GetDatabaseError();
}

gxDatabaseError POD::SetIndexFileError(gxDatabaseError err,
				       unsigned index_number)
{
  if(!Index(index_number)) {
    return gxDBASE_NO_DATABASE_OPEN;
  }

  // PC-lint 09/15/2005: Possible null pointer
  gxBtree * gxbtree = Index(index_number);
  if(!gxbtree) return gxDBASE_NO_DATABASE_OPEN;
  return gxbtree->SetDatabaseError(err);
}

const char *POD::DataFileExceptionMessage()
{
  if(!opendatafile) {
    return gxDatabaseExceptionMessage(gxDBASE_NO_DATABASE_OPEN);
  }
  return opendatafile->DatabaseExceptionMessage();
}

const char *POD::IndexFileExceptionMessage(unsigned index_number)
{
  if(!Index(index_number)) {
    return gxDatabaseExceptionMessage(gxDBASE_NO_DATABASE_OPEN);
  }

  // PC-lint 09/15/2005: Possible null pointer
  gxBtree * gxbtree = Index(index_number);
  if(!gxbtree) return gxDatabaseExceptionMessage(gxDBASE_NO_DATABASE_OPEN);
  return gxbtree->DatabaseExceptionMessage();
}

#ifdef __BCC32__
#pragma warn .8080
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
