// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: pod.h
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

The POD (Persistent Object Database) manager is used to manage
file pointers to database data files and index files.

Changes:
==============================================================
03/05/2002: Removed the PODIndexFile_t::cid optional class ID
member. Version 4.2 and higher no longer use the optional class
ID within B-tree headers.

03/05/2002: Modified the PODIndexFile::Open() function to allow
the number of trees and gxDatabase revision number to be specified
when creating a new index file. The optional class ID is no longer
used.

03/05/2002: Added the POD::Flush(), FlushDataFile(), and
FlushIndexFile() functions used by applications to flush
the POD database or the data and index files individually.

03/05/2002: Added the POD::TestDatabase(), TestDataFile(), and
TestIndexFile() functions used by applications to test the
POD database or the data and index files individually.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_POD_HPP__
#define __GX_POD_HPP__

#include "gxdlcode.h"

#include "gxdbase.h"
#include "gxbtree.h"

const unsigned POD_MAX_INDEX = 255;  // Max number of index files per database

// NOTE: The values are only used if no value is specified
const BtreeNodeOrder_t POD_DEFAULT_ORDER = 29; // Default index file order
const BtreeSize_t POD_DEFAULT_KEY_SIZE = 64;   // Default key size

// POD index file type
struct GXDLCODE_API PODIndexFile_t
{
  PODIndexFile_t() { 
    btree = 0;
    dbkey_size = POD_DEFAULT_KEY_SIZE;
    order = POD_DEFAULT_ORDER;
    use_index = rebuild_index = 0;
  }
  ~PODIndexFile_t() { }

  gxBtree *btree;         // Pointer to the open index file
  BtreeSize_t dbkey_size; // Size of the entry key used by this index
  BtreeNodeOrder_t order; // Order of the Btree index
  int use_index;          // True if this index file is being used
  int rebuild_index;      // True if this index needs to be rebuilt
};

// POD index file sub-system
class GXDLCODE_API PODIndexFile
{
public:
  PODIndexFile(unsigned num_indexes);
  ~PODIndexFile();

public:
  gxDatabaseError Open(const char *ifname, unsigned index_number, 
		       DatabaseKeyB &key_type,
		       BtreeNodeOrder_t order = POD_DEFAULT_ORDER,  
		       gxDatabaseAccessMode mode = gxDBASE_READWRITE,
		       int num_tree = 1,
		       __SBYTE__ rev_letter = gxDatabaseRevisionLetter);
  gxDatabaseError Close(unsigned index_number);

public:
  PODIndexFile_t *index_file[POD_MAX_INDEX]; // Array of POD index files
  unsigned n_indexes;  // Total number of index files in use by this database
  unsigned curr_index; // Current index file in use
};
  
// Persistent Object Database manager class
class GXDLCODE_API POD
{
public:
  POD();
  ~POD();
  
private:
  POD(const POD &ob) { }            // Disallow copying
  void operator=(const POD &ob) { } // Disallow assignment

public: // POD database functions
  gxDatabaseError Open(const char *dfname, const char *ifname,
		       DatabaseKeyB &key_type,
		       BtreeNodeOrder_t order = POD_DEFAULT_ORDER,
		       gxDatabaseAccessMode mode = gxDBASE_READWRITE,
		       int use_index = 1, FAU_t static_size = (FAU_t)0, 
		       int num_trees = 1, 
		       __SBYTE__ df_rev_letter = gxDatabaseRevisionLetter,
		       __SBYTE__ if_rev_letter = gxDatabaseRevisionLetter);
  gxDatabaseError Open(const char *dfname, PODIndexFile *ix_ptr,
		       gxDatabaseAccessMode mode = gxDBASE_READWRITE,
		       FAU_t static_size = (FAU_t)0,
		        __SBYTE__ df_rev_letter = gxDatabaseRevisionLetter);
  gxDatabaseError Open(const char *dfname, char *ifname[POD_MAX_INDEX],
		       unsigned num_indexes, DatabaseKeyB &key_type,
		       BtreeNodeOrder_t order = POD_DEFAULT_ORDER,
		       gxDatabaseAccessMode mode = gxDBASE_READWRITE,
		       FAU_t static_size = (FAU_t)0, int num_trees = 1,
		       __SBYTE__ df_rev_letter = gxDatabaseRevisionLetter,
		       __SBYTE__ if_rev_letter = gxDatabaseRevisionLetter);
  gxDatabaseError Close();
  void Release();
  gxDatabaseError Flush();
  int TestDatabase();

public: // POD data file functions
  gxDatabaseError OpenDataFile(const char *fname,
			       gxDatabaseAccessMode mode = gxDBASE_READWRITE);
  gxDatabaseError CloseDataFile();
  void ReleaseDataFile();
  gxDatabaseError CreateDataFile(const char *fname,
				 FAU_t static_size = (FAU_t)0,
			 __SBYTE__ rev_letter = gxDatabaseRevisionLetter);
  gxDatabase *OpenDataFile() { return opendatafile; }
  gxDatabase *OpenDataFile() const { return opendatafile; }
  int Exists() const { return exists == 1; }
  int Exists() { return exists == 1; }
  gxDatabaseError FlushDataFile();
  int TestDataFile();

public: // POD index file functions
  gxDatabaseError OpenIndex(const char *fname, unsigned index_number,
			    DatabaseKeyB &key_type, BtreeNodeOrder_t order,
			    gxDatabaseAccessMode mode = gxDBASE_READWRITE);
  gxDatabaseError CloseIndex(unsigned index_number);
  void ReleaseIndexFile();
  gxDatabaseError CreateIndex(const char *fname, unsigned index_number, 
			      DatabaseKeyB &key_type, BtreeNodeOrder_t order,
			      int num_trees = 1,
			      __SBYTE__ rev_letter = gxDatabaseRevisionLetter);
  PODIndexFile *OpenIndexFile() { return openindexfile; }
  PODIndexFile *OpenIndexFile() const { return openindexfile; }
  gxBtree *Index(unsigned index_number = 0);
  gxBtree *Index(unsigned index_number = 0) const;
  gxDatabase *IndexDBPtr(unsigned index_number = 0);
  gxDatabase *IndexDBPtr(unsigned index_number = 0) const;
  int UsingIndex() { return using_index == 1; }
  int UsingIndex() const { return using_index == 1; }
  int RebuildIndex(unsigned index_number = 0) {
    return openindexfile->index_file[index_number]->rebuild_index == 1;
  }
  int RebuildIndex(unsigned index_number = 0) const {
    return openindexfile->index_file[index_number]->rebuild_index == 1;
  }
  
  // These functions allow the application to enable and disable 
  // indexing sub-system for certain functions.
  void DisableIndex() { using_index = 0; }
  void EnableIndex() { using_index = 1; }

  gxDatabaseError FlushIndexFile(unsigned index_number = 0);
  int TestIndexFile(unsigned index_number = 0);

public: // Execption handling functions
  gxDatabaseError GetDataFileError();
  gxDatabaseError GetDataFileError() const;
  gxDatabaseError SetDataFileError(gxDatabaseError err);
  gxDatabaseError GetIndexFileError(unsigned index_number = 0);
  gxDatabaseError GetIndexFileError(unsigned index_number = 0) const;
  gxDatabaseError SetIndexFileError(gxDatabaseError err,
				    unsigned index_number = 0);
  const char *DataFileExceptionMessage();
  const char *IndexFileExceptionMessage(unsigned index_number = 0);

private:
  gxDatabase *opendatafile;    // Pointer the open data file
  PODIndexFile *openindexfile; // Pointer to the indexing sub-system
  int exists;                  // True if data file already exists
  int using_index;             // True if using the indexing sub-system
};

#endif // __GX_POD_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
