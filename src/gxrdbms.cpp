// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxrdbms.cpp
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

Relational database management system used with the DataReel 
library gxDatabase, gxBtree, and gxStream classes.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "gxrdbms.h"
#include "dbugmgr.h"
#include "dfileb.h"

#ifdef __BCC32__
#pragma warn -8057
#pragma warn -8080
#endif

gxRDBMS::gxRDBMS()
{
  // Null all pointers
  pod = 0;
  sec_key_fields = 0; 
  indexfile_names = 0;

  num_sec_keys = dup = 0;
  compare_case = 1;   // Default string case compare
  compare_alpha = 0;  // Default alphanumeric compare
  primary_key = -1;   // Default key uses all columns
  num_trees = 1;      // Default number of B-trees index files
  mode = gxDBASE_READWRITE;  // Default file access mode

  // NOTE: All B-tree indexes use the same key type so the node order
  // is the same for all index files.
  order = gxrdDEFAULT_ORDER; // Default node order for all B-trees

  df_rev_letter = 'D';    // Allow file and node locking
  if_rev_letter = 'E';    // No node locking overhead
  datafile_ext = ".gxd";  // Default data file dot extension
  indexfile_ext = ".gix"; // Default index file dot extension

  // Set a default key definition that uses all fields for the key
  // value. This default key def is used for def files that do not
  // contain a key definition.
  key_def = "PRI: COL 1,ATTRIB: NOCASE, ATTRIB: ANCOMPARE";

  // Set the default reclamation method
  method = gxDBASE_RECLAIM_FIRSTFIT;
}

gxRDBMS::~gxRDBMS()
{
  // PC-lint 09/08/2005: Function may throw exception in destructor
  if(pod) delete pod;
  pod = 0;
  if(indexfile_names) delete[] indexfile_names;
  indexfile_names = 0;
  if(sec_key_fields) delete sec_key_fields; 
  sec_key_fields = 0;
}

gxDatabaseError gxRDBMS::Write(gxrdDatabaseRecord *r, int flushdb, 
			       int test_tree)
{
  FAU_t block_address;
  return Write(r, block_address, flushdb, test_tree);
}


gxDatabaseError gxRDBMS::Write(gxrdDatabaseRecord *r, FAU_t &block_address,
			       int flushdb, int test_tree)
{
  if(IsClosed()) return gxDBASE_FILE_NOT_OPEN_ERROR;

  // PC-lint 05/02/2005: Possible use of null pointer
  if(!pod) return gxDBASE_NULL_PTR;

  if(!r) return pod->SetDataFileError(gxDBASE_NULL_PTR);

  if(table.record_def != r->RecordDef()) {
    return pod->SetDataFileError(gxDBASE_BAD_RECORD);
  }

  // Create a disk record that will be stored in a single
  // contiguous block of disk space
  MemoryBuffer disk_record(r->SizeOf()+ sizeof(gxrdRecordHeader));
  gxDatabaseError dberr = r->CreateDiskRecord(disk_record);
  if(dberr != gxDBASE_NO_ERROR) {
    return pod->SetDataFileError(dberr);
  }

  FAU_t record_address = \
    pod->OpenDataFile()->Alloc(disk_record.length(), method);

  // Check for any allocation errors
  if(record_address == (FAU_t)0) {
    return pod->GetDataFileError();
  }

  // Calculate the block address
  block_address = record_address-pod->OpenDataFile()->BlockHeaderSize();

  // Write the disk record
  if(pod->OpenDataFile()->Write(disk_record.m_buf(), disk_record.length(),
				gxCurrAddress, flushdb) != gxDBASE_NO_ERROR) {
    return pod->GetDataFileError();
  }

  // Add the entry to the Index file
  if(UsingIndex()) {
    gxDatabaseError dberr2 = AddKey(r, block_address, flushdb, test_tree);
    if(dberr2 != gxDBASE_NO_ERROR) {
      pod->OpenDataFile()->Delete(record_address); // Delete data file record
      return dberr2;
    }
  }

  return pod->SetDataFileError(gxDBASE_NO_ERROR);
}

gxDatabaseError gxRDBMS::Read(gxrdDatabaseRecord *r, gxBlockHeader &blk, 
			      FAU_t block_address)
{
  if(IsClosed()) return gxDBASE_FILE_NOT_OPEN_ERROR;

  // PC-lint 05/02/2005: Possible use of null pointer
  if(!pod) return gxDBASE_NULL_PTR;

  if(!r) return pod->SetDataFileError(gxDBASE_NULL_PTR);
  
  gxDatabaseError err;
  gxDatabase *f = pod->OpenDataFile();
  if(table.record_def != r->RecordDef()) {
    err = r->CreateRecord(table.record_def);
    if(err != gxDBASE_NO_ERROR) {
      return pod->SetDataFileError(err);
    }
  }

  // Seek to the record addess
  if(block_address != gxCurrAddress) {
    f->SeekTo(block_address + f->BlockHeaderSize());
    if(pod->GetDataFileError() != gxDBASE_NO_ERROR) {
      return pod->GetDataFileError();
    }
  }

  // Adjust the number of bytes allocated for the record space 
  // according to the data file revision letter.
  __ULWORD__ length;
  switch(f->GetRevLetter()) {
    case '\0' : case ' ' : case 'e' : case 'E' :
      length = blk.block_length - f->BlockHeaderSize();
      break;
    default:
      length = (blk.block_length-(f->BlockHeaderSize()+ sizeof(gxChecksum)));
      break;
  }

  // Create a disk record used to read a contiguous block of data
  // from the file. The record length must be adjusted to account
  // for the record header. Account for variable length records
  // by using the pre-read length stored in the block header instead
  // of the record size set by the table definition.
  MemoryBuffer disk_record(length);
  
  // Clear the disk record
  disk_record.MemSet(0, 0, length);

  // Read the record stored on disk
  if(pod->OpenDataFile()->Read(disk_record.m_buf(), length) 
     != gxDBASE_NO_ERROR) {
    return pod->GetDataFileError();
  }

  // Load the disk record into the specified record
  err = r->LoadDiskRecord(disk_record);
  if(err != gxDBASE_NO_ERROR) {
    return pod->SetDataFileError(err);
  }

  return pod->SetDataFileError(gxDBASE_NO_ERROR);
}

gxDatabaseError gxRDBMS::ReadBlockHeader(gxBlockHeader &blk, 
					 FAU_t block_address)
{
  if(IsClosed()) return gxDBASE_FILE_NOT_OPEN_ERROR;

  // PC-lint 05/02/2005: Possible use of null pointer
  if(!pod) return gxDBASE_NULL_PTR;

  gxDatabase *f = pod->OpenDataFile();
  if(f->ReadBlockHdr(blk, block_address) != 
     gxDBASE_NO_ERROR) {
    return pod->GetDataFileError();
  }
  
  FAU_t curr_address = f->Tell();
  if(pod->GetDataFileError() != gxDBASE_NO_ERROR) {
    return pod->GetDataFileError();
  }

  switch(f->GetRevLetter()) {
    case 'c' : case 'C' : case 'd' : case 'D' :
      if(block_address != gxCurrAddress) {
	curr_address = f->Tell();
	if(pod->GetDataFileError() != gxDBASE_NO_ERROR) {
	  return pod->GetDataFileError();
	}
      }
      // Seek past the record lock header
      f->SeekTo(curr_address + sizeof(gxRecordLockHeader));
      if(pod->GetDataFileError() != gxDBASE_NO_ERROR) {
	return pod->GetDataFileError();
      }
      break;
    default:
      break;
  }

  return pod->SetDataFileError(gxDBASE_NO_ERROR);
}

gxDatabaseError gxRDBMS::Read(gxrdDatabaseRecord *r, FAU_t block_address)
{
  if(IsClosed()) return gxDBASE_FILE_NOT_OPEN_ERROR;

  // PC-lint 05/02/2005: Possible use of null pointer
  if(!pod) return gxDBASE_NULL_PTR;

  if(!r) return pod->SetDataFileError(gxDBASE_NULL_PTR);

  gxBlockHeader blk;   // Block Header
  if(ReadBlockHeader(blk, block_address) != gxDBASE_NO_ERROR) {
    return pod->GetDataFileError();
  }

  return Read(r, blk);
}

gxDatabaseError gxRDBMS::Close()
{
  gxDatabaseError err = gxDBASE_NO_ERROR;
  if(pod) {
    err = pod->Close();
    if(err != gxDBASE_NO_ERROR) return err;
  }
  return err;
}

gxDatabaseError gxRDBMS::CreateTable(const gxString &tdef, 
				     const gxString &kdef)
{
  gxDatabaseError err = table.CreateTable(tdef);
  if(err != gxDBASE_NO_ERROR) {
    return err;
  }
  err = DefineKeys(kdef);
  if(err != gxDBASE_NO_ERROR) {
    return err;
  }
  return create_or_open(0);
}

gxDatabaseError gxRDBMS::CreateTable(const gxString &tdef, 
				     const gxString &kdef,
				     const gxString &dir)
{
  db_dir = dir;
  return CreateTable(tdef, kdef);
}

gxDatabaseError gxRDBMS::CreateTable(const gxString &tdef, 
				     const gxString &kdef,
				     const gxString &fdef, 
				     const gxString &fkdef)
{
  form_def = fdef;
  fkey_def = fkdef;
  return CreateTable(tdef, kdef);
}

gxDatabaseError gxRDBMS::CreateTable(const gxString &tdef, 
				     const gxString &kdef,
				     const gxString &fdef, 
				     const gxString &fkdef,
				     const gxString &dir)
{
  form_def = fdef;
  fkey_def = fkdef;
  db_dir = dir;
  return CreateTable(tdef, kdef);
}

gxDatabaseError gxRDBMS::Create(const gxString &tdef,
				const gxString &dir)
{
  db_dir = dir;
  return Create(tdef);
}

gxDatabaseError gxRDBMS::Create(const gxString &tdef)
{
  gxDatabaseError err = table.CreateTable(tdef);
  if(err != gxDBASE_NO_ERROR) {
    return err;
  }

  return create_or_open(0);
}

gxDatabaseError gxRDBMS::create_or_open(int rewrite_header)
{
  // Test the record def
  gxrdDatabaseRecord record;
  int i;
  gxDatabaseError err = record.CreateRecord(table.record_def);
  if(err != gxDBASE_NO_ERROR) {
    return err;
  }

  err = gen_file_names();
  if(err != gxDBASE_NO_ERROR) {
    return err;
  }

  FAU_t static_size = HeaderSize();
  int use_index = 1; // All tables will use an index file
  gxrdDatabaseKey key_type;

  if(pod) { // Close any open files
    delete pod; // Delete call will close any open files
    pod = 0;
  }

  pod = new POD;
  if(!pod) {
    return gxDBASE_MEM_ALLOC_ERROR;
  }
  
  if(!indexfile_names) {
    return gxDBASE_NULL_PTR;
  }

  if((num_sec_keys > 0) && (num_trees > 1)) { 
    // Table contains secondary index keys
    PODIndexFile *openindexfile = new PODIndexFile(num_trees);
    if(!openindexfile) {
      return gxDBASE_MEM_ALLOC_ERROR;
    }
    if(!futils_exists(datafile_name.c_str())) { 
      // The data file does not exist
      for(i = 0; i < num_trees; i++) {
	// Remove all the index files and recreate them
	if(futils_exists(indexfile_names[i].c_str())) {
	  futils_remove(indexfile_names[i].c_str());
	}
      }
    }
    int num_indexs = 0;
    int delete_all_indexs = 0;
    for(i = 0; i < num_trees; i++) {
      if(futils_exists(indexfile_names[i].c_str())) {
	num_indexs++;
      }
      else {
	if(num_indexs > 0) delete_all_indexs = 1;
      }
    }
    if((num_indexs > 0) && (delete_all_indexs)) {
      // One of the index files is missing so delete the existing 
      // index files.
      for(i = 0; i < num_trees; i++) { 
	// Remove all the index files and recreate them
	if(futils_exists(indexfile_names[i].c_str())) {
	  futils_remove(indexfile_names[i].c_str());
	}
      }      
    }
    for(i = 0; i < num_trees; i++) {
      err = openindexfile->Open(indexfile_names[i].c_str(), i,
				key_type, order, mode,
				num_trees, if_rev_letter);
      // Test for any errors
      if(err != gxDBASE_NO_ERROR) {
	delete openindexfile; // 09/12/2005 - Prevent memory leak
	return err;
      }
    }
    err = pod->Open(datafile_name.c_str(), openindexfile, mode, static_size, 
		    df_rev_letter);
    if(err != gxDBASE_NO_ERROR) {
      delete openindexfile; // 09/12/2005 - Prevent memory leak
      return err;
    }
#if defined (__PCLINT_CHECK__)
    // PC-lint 05/02/2005: Custodial pointer warning can be ignored
    delete openindexfile;
#endif
  }
  else {
    err = pod->Open(datafile_name.c_str(), indexfile_names[0].c_str(), 
		    key_type,
		    order, mode, use_index, 
		    static_size, num_trees, df_rev_letter, 
		    if_rev_letter);
  }

  // Test for any errors
  if(err != gxDBASE_NO_ERROR) {
    return err;
  }

  if(!pod->Exists()) {
    // Write the table header if a new file was created
    return WriteTableHeader(1, 1);
  }
  else if(rewrite_header) {
    // Rewrite the table header for all files
    return WriteTableHeader(1, 1);
  }
  else {
    // Read the table header from disk if the file already exists
    gxrdTableHeader table_header;
    gxRDBMSHeader file_id;
    err = ReadTableHeader(table_header, file_id);
    if(err != gxDBASE_NO_ERROR) {
      return err;
    }
    if(!table_header.TestFieldVars(table.table_header)) {
      // The table DEF has changed so rewrite the table header
      return WriteTableHeader(1, 1);
    }
    table.table_header = table_header;
  }

  return gxDBASE_NO_ERROR;
}

FAU_t gxRDBMS::Find(gxrdDatabaseRecord *r, __ULWORD__ field, int case_cmp,
		    int alpha_cmp, int test_tree)
{
  if(IsClosed()) return FAU_t(0);

  // PC-lint 05/02/2005: Possible use of null pointer
  if(!pod) return (FAU_t)0;

  if(!r) {
    pod->SetDataFileError(gxDBASE_NULL_PTR);
    return FAU_t(0);
  }

  gxDatabaseError err;
  if(table.record_def != r->RecordDef()) {
    err = r->CreateRecord(table.record_def);
    if(err != gxDBASE_NO_ERROR) {
      pod->SetDataFileError(err);
      return (FAU_t)0;
    }
  }
  
  // Search the index file for this entry
  if(UsingIndex()) {
    gxrdKeyType_t k = r->CreateKey(field, CaseCompare());
    if(k == (gxrdKeyType_t)0) {
      return pod->SetDataFileError(gxDBASE_BAD_KEY);
    }
    gxrdRecordHeader rh(table.table_name, r->NumFields());
    gxrdTableID_t tid = rh.table_id;
    gxrdKeyID_t kid = (gxrdKeyID_t)0;
    gxrdDatabaseKey key(k, kid, tid, dup);
    gxrdDatabaseKey compare_key;
    unsigned index_number = field;
    if(index_number == (__ULWORD__)-1) index_number = 0;
    if(index_number > (unsigned)num_trees) index_number = 0;
    if(!FindKey(key, compare_key, index_number, test_tree)) {
      return (FAU_t)0; // Could not find the key
    }
    return key.KeyID(); // Found the index file entry
  }

  // If not using index file search the data file
  if(test_tree) TestDatabase();
  gxBlockHeader blk;   // Block Header
  gxrdDatabaseRecord record(table.record_def);
  gxDatabase *f = pod->OpenDataFile();

  FAU_t gxdfileEOF = f->GetEOF();
  FAU_t block_address = (FAU_t)0;
  block_address = f->FindFirstBlock(block_address); 

  // No database blocks found in file
  if(block_address == (FAU_t)0) return (FAU_t)0; 

  while(1) { 
    if(FAU_t(block_address + f->BlockHeaderSize()) >= gxdfileEOF) 
      break;
    if(ReadBlockHeader(blk, block_address) != gxDBASE_NO_ERROR) {
      return (FAU_t)0;
    }
    if(f->TestBlockHeader(blk)) {
      if((__SBYTE__)blk.block_status == gxNormalBlock) {
	Read(&record, blk);
	if(pod->GetDataFileError() != gxDBASE_NO_ERROR) {
	  if(pod->GetDataFileError() != gxDBASE_BAD_TABLE_ID) {
	    return (FAU_t)0;
	  }
	}
	if(record.Compare(*r, field, case_cmp, alpha_cmp) == 0) {
	  // Found unique data member
	  if(field == (__ULWORD__)-1) return block_address;
 	  *r = record; // Synchronize the records during partial finds
	  return block_address;
	}
      }
      // Goto the next database block
      block_address = block_address + blk.block_length;
    }
    else {
      // Keep searching until a valid database block is found
      block_address++; 
    }
  }

  return (FAU_t)0;
}

int gxRDBMS::AddKey(DatabaseKeyB &key, DatabaseKeyB &compare_key,
		    unsigned index_number, int flushdb)
{
  if(IsClosed()) return 0;

  if(UsingIndex()) {
    // PC-lint 05/02/2005: Possible use of null pointer
    if(!pod) return 0;

    return pod->Index(index_number)->Insert(key, compare_key, flushdb);
  }
  return 1;
}

gxDatabaseError gxRDBMS::AddKey(gxrdDatabaseRecord *r, FAU block_address, 
				int flushdb, int test_tree)
{
  if(IsClosed()) return gxDBASE_FILE_NOT_OPEN_ERROR;

  // Add the entry to the Index file
  if(UsingIndex()) {

    // PC-lint 05/02/2005: Possible use of null pointer
    if(!pod) return gxDBASE_NULL_PTR;

    int pk = 0;
    if(primary_key == -1) {
      pk = -1;
    }
    else {
      pk = primary_key - 1;
    }
    gxrdKeyType_t k = r->CreateKey(pk, CaseCompare());
    if(k == (gxrdKeyType_t)0) {
      return pod->SetIndexFileError(gxDBASE_BAD_KEY);
    }
    gxrdRecordHeader rh(table.table_name, r->NumFields());
    gxrdTableID_t tid = rh.table_id;
    gxrdKeyID_t kid = block_address;
    gxrdDatabaseKey key(k, kid, tid, dup);
    gxrdDatabaseKey compare_key;
    int i;

    // Add the primary key
    if(!AddKey(key, compare_key, 0, flushdb)) {
      return pod->GetIndexFileError();
    }
    
    // Add any secondary keys
    if((num_sec_keys > 0) && (num_trees > 1)) {
      for(i = 0; i < num_sec_keys; i++) {
	if(!sec_key_fields) {
	  return pod->SetIndexFileError(gxDBASE_NULL_PTR, (i+1));
	}
	k = r->CreateKey((sec_key_fields[i]-1), CaseCompare());
	if(k == (gxrdKeyType_t)0) {
	  return pod->SetIndexFileError(gxDBASE_BAD_KEY, (i+1));
	}
	key.SetKey(k);
	key.SetKeyID(block_address);
	key.SetTableID(rh.table_id);
	if(!AddKey(key, compare_key, (i+1), flushdb)) {
	  gxDatabaseError err = pod->GetIndexFileError(i+1);
	  // Delete all associated keys
	  DeleteKey(r, block_address, flushdb, test_tree); 
	  return err;
	}
      }
    }
  }
  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxRDBMS::DeleteKey(gxrdDatabaseRecord *r, FAU &block_address, 
				   int flushdb, int test_tree)
{
  if(IsClosed()) return gxDBASE_FILE_NOT_OPEN_ERROR;

  // PC-lint 05/02/2005: Possible use of null pointer
  if(!pod) return gxDBASE_NULL_PTR;

  if(!r) return pod->SetDataFileError(gxDBASE_NULL_PTR);

  if(UsingIndex()) {
    int pk = 0;
    int i;
    int found_primary = 0;
    if(primary_key == -1) {
      pk = -1;
    }
    else {
      pk = primary_key - 1;
    }
    gxrdKeyType_t k = r->CreateKey(pk, CaseCompare());
    if(k == (gxrdKeyType_t)0) {
      return pod->SetDataFileError(gxDBASE_BAD_KEY);
    }
    gxrdRecordHeader rh(table.table_name, r->NumFields());
    gxrdTableID_t tid = rh.table_id;
    gxrdKeyID_t kid = block_address;
    gxrdDatabaseKey key(k, kid, tid, dup);
    gxrdDatabaseKey compare_key;
    if(FindKey(key, compare_key, 0, test_tree)) { // Found primary key
      block_address = key.KeyID();
      if(!DeleteKey(key, compare_key, 0, flushdb)) {
	return pod->GetIndexFileError();
      }
      found_primary = 1;
    }

    if(num_trees == 1) { // Did not find the primary key
      if(found_primary) return gxDBASE_NO_ERROR;
      return pod->SetIndexFileError(gxDBASE_RECORD_NOT_FOUND);
    }
   
    // Delete any secondary keys
    if((num_sec_keys > 0) && (num_trees > 1)) {
      for(i = 0; i < num_sec_keys; i++) {
	if(!sec_key_fields) {
	  return pod->SetIndexFileError(gxDBASE_NULL_PTR, (i+1));
	}
	k = r->CreateKey((sec_key_fields[i]-1), CaseCompare());
	if(k == (gxrdKeyType_t)0) {
	  return pod->SetIndexFileError(gxDBASE_BAD_KEY, (i+1));
	}
	key.SetKey(k);
	if(FindKey(key, compare_key, (i+1), test_tree)) {
	  block_address = key.KeyID();
	  if(!DeleteKey(key, compare_key, (i+1), flushdb)) {
	    pod->GetIndexFileError(i+1);
	  }
	}
      }
    }
  }

  return gxDBASE_NO_ERROR;
}

int gxRDBMS::FindKey(DatabaseKeyB &key, DatabaseKeyB &compare_key,
		     unsigned index_number, int test_tree)
{
  if(IsClosed()) return 0;

  if(UsingIndex()) {
    // PC-lint 05/02/2005: Possible use of null pointer
    if(!pod) return 0;

    return pod->Index(index_number)->Find(key, compare_key, test_tree);
  }
  return 0;
}

int gxRDBMS::DeleteKey(DatabaseKeyB &key, DatabaseKeyB &compare_key,
		       unsigned index_number, int flushdb)
{
  if(IsClosed()) return 0;

  if(UsingIndex()) {
    // PC-lint 05/02/2005: Possible use of null pointer
    if(!pod) return 0;

    return pod->Index(index_number)->Delete(key, compare_key, flushdb);
  }
  return 0;
}

FAU_t gxRDBMS::FindKey(gxrdDatabaseRecord *r, int test_tree)
{
  if(IsClosed()) return FAU_t(0);

  // PC-lint 05/02/2005: Possible use of null pointer
  if(!pod) return (FAU_t)0;

  if(!r) {
    pod->SetDataFileError(gxDBASE_NULL_PTR);
    return FAU_t(0);
  }

  // Check the primary key
  int pk = 0;
  int i;
  if(primary_key == -1) {
    pk = -1;
  }
  else {
    pk = primary_key - 1;
  }
  
  FAU_t block_address = Find(r, pk, compare_case, compare_alpha, test_tree);
  if(block_address != (FAU_t)0) {
    return block_address; // Found primary key
  }

  // Check all secondary keys
  if((num_sec_keys > 0) && (num_trees > 1)) {
    for(i = 0; i < num_sec_keys; i++) {
      if(!sec_key_fields) {
	pod->SetIndexFileError(gxDBASE_NULL_PTR, (i+1));
	return (FAU_t)0;
      }
      pod->SetIndexFileError(gxDBASE_NO_ERROR, i); // Reset previous error
      block_address = Find(r, (sec_key_fields[i]-1), 
			   compare_case, compare_alpha, test_tree);
      if(block_address != (FAU_t)0) {
	return block_address; // Found secondary key
       }
    }
  }
  return (FAU_t)0; // Did not find any matching keys
}

int gxRDBMS::AddRecord(gxrdDatabaseRecord *r, int flushdb, int test_tree)
{
  if(IsClosed()) return 0;

  // PC-lint 05/02/2005: Possible use of null pointer
  if(!pod) return 0;

  if(!r) {
    pod->SetDataFileError(gxDBASE_NULL_PTR);
    return 0;
  }

  FAU_t block_address = FindKey(r, test_tree);
  if(block_address != (FAU_t)0) {
    pod->SetDataFileError(gxDBASE_RECORD_EXISTS);
    return 0;
  }
  if(Write(r, flushdb, test_tree) != gxDBASE_NO_ERROR) return 0;
  return 1;
}

int gxRDBMS::FindRecord(gxrdDatabaseRecord *r, 
			int load_record, int test_tree)
{
  FAU_t block_address;
  return FindRecord(r, block_address, load_record, test_tree);
}

int gxRDBMS::FindRecord(gxrdDatabaseRecord *r, FAU_t &block_address,
			int load_record, int test_tree)
{
  block_address = (FAU_t)0; // Reset the block address

  if(IsClosed()) return 0;

  // PC-lint 05/02/2005: Possible use of null pointer
  if(!pod) return 0;

  if(!r) {
    pod->SetDataFileError(gxDBASE_NULL_PTR);
    return 0;
  }

  block_address = FindKey(r, test_tree);
  if(block_address == (FAU_t)0) {
    pod->SetDataFileError(gxDBASE_RECORD_NOT_FOUND);
    return 0;
  }
  if(load_record) ReadRecord(r, block_address); 
  return 1;
}

int gxRDBMS::ReadRecord(gxrdDatabaseRecord *r, FAU_t block_address)
{
  if(IsClosed()) return 0;

  // PC-lint 05/02/2005: Possible use of null pointer
  if(!pod) return 0;

  if(!r) {
    pod->SetDataFileError(gxDBASE_NULL_PTR);
    return 0;
  }

  if(Read(r, block_address) != gxDBASE_NO_ERROR) return 0;
  return 1;
}

int gxRDBMS::DeleteRecord(gxrdDatabaseRecord *r, int flushdb,
			  int test_tree)
{
  if(IsClosed()) return 0;

  // PC-lint 05/02/2005: Possible use of null pointer
  if(!pod) return 0;

  if(!r) {
    pod->SetDataFileError(gxDBASE_NULL_PTR);
    return 0;
  }
  if(table.record_def != r->RecordDef()) {
    pod->SetDataFileError(gxDBASE_BAD_RECORD);
    return 0;
  }
  gxDatabaseError err;
  gxDatabase *f = pod->OpenDataFile();
  FAU block_address = (FAU_t)0;
  if(UsingIndex()) {
    if(num_trees == 1) { 
      // There are no secondary keys so delete the primary key and return
      err = DeleteKey(r, block_address, flushdb, test_tree);
      if(err == gxDBASE_NO_ERROR) {
	return f->Delete(block_address+f->BlockHeaderSize());
      }
    }
    
    // Search all the indexes for the primary key  
    block_address = FindKey(r, test_tree);
    if(block_address == (FAU_t)0) {
      pod->SetDataFileError(gxDBASE_RECORD_NOT_FOUND);
      return 0;
    }
    // Load the record to test the primay and secondary keys
    if(Read(r, block_address) != gxDBASE_NO_ERROR) {
      return 0;
    }
    err = DeleteKey(r, block_address, flushdb, test_tree);
    if(err == gxDBASE_NO_ERROR) {
      return f->Delete(block_address+f->BlockHeaderSize());
    }
  }
  else { // Not using any indexes so search the data file
    int pk = 0;
    if(primary_key == -1) {
      pk = -1;
    }
    else {
      pk = primary_key - 1;
    }
    block_address = Find(r, pk, compare_case, compare_alpha, test_tree);
    if(block_address == (FAU_t)0) return 0; // Could not find data file entry
    return f->Delete(block_address+f->BlockHeaderSize());
  }

  return 0;
}

gxDatabaseError gxRDBMS::WriteTableHeader(int init, int flushdb)
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!pod) return gxDBASE_NULL_PTR;

  if(init) {
    gxDatabaseError err = init_table_header();
    if(err != gxDBASE_NO_ERROR) {
      return pod->SetDataFileError(err);
    }
  }

  return WriteTableHeader(pod->OpenDataFile(), table.table_header, 
			  flushdb);
}

gxDatabaseError gxRDBMS::ReadTableHeader(int test_db)
{
  gxRDBMSHeader file_id;

  // PC-lint 05/02/2005: Possible use of null pointer
  if(!pod) return gxDBASE_NULL_PTR;

  return ReadTableHeader(pod->OpenDataFile(), table.table_header, 
			 file_id, test_db);
}


gxDatabaseError gxRDBMS::ReadTableHeader(gxrdTableHeader &table_header,
					 gxRDBMSHeader &file_id,
					 int test_db)
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!pod) return gxDBASE_NULL_PTR;

  return ReadTableHeader(pod->OpenDataFile(), table_header, 
			 file_id, test_db);
}

gxDatabaseError gxRDBMS::WriteTableHeader(gxrdTableHeader &table_header, 
					  int flushdb)
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!pod) return gxDBASE_NULL_PTR;

  return WriteTableHeader(pod->OpenDataFile(), table_header, flushdb);
}

gxDatabaseError gxRDBMS::ReadTableHeader(gxDatabase *f,
					 gxrdTableHeader &table_header,
					 gxRDBMSHeader &file_id,
					 int test_db)
{
  if(!f) return gxDBASE_NULL_PTR;

  __ULWORD__ i;
  file_id.Clear();
  gxRDBMSHeader compare_file_id;
  gxINT32 num;

  if(test_db) f->TestFileHeader();
  f->Read(&file_id, sizeof(file_id), f->FileHeaderSize());
  if(f->GetDatabaseError() != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }
  if(!file_id.TestFileID(compare_file_id)) {
    return f->SetDatabaseError(gxDBASE_BAD_TABLE_HEADER);
  }
  f->Read(table_header.table_name, sizeof(table_header.table_name));
  if(f->GetDatabaseError() != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }
  f->Read(table_header.key_def, sizeof(table_header.key_def));
  if(f->GetDatabaseError() != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }
  f->Read(table_header.form_def, sizeof(table_header.form_def));
  if(f->GetDatabaseError() != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }
  f->Read(table_header.fkey_def, sizeof(table_header.fkey_def));
  if(f->GetDatabaseError() != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }
  f->Read(&num, sizeof(num));
  if(f->GetDatabaseError() != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }

  // Test the number of fields before allocating memory
  if((num < tableMIN_FIELDS) || (num > tableMAX_FIELDS)) {
    return f->SetDatabaseError(gxDBASE_BAD_TABLE_HEADER);
  }

  if(!table_header.AllocHeader(num)) {
    return f->SetDatabaseError(gxDBASE_MEM_ALLOC_ERROR);
  }
  for(i = 0; i < table_header.num_fields; i++) {
    f->Read(&table_header.fields[i], sizeof(gxrdTableHeaderField));
    if(f->GetDatabaseError() != gxDBASE_NO_ERROR) {
      return f->GetDatabaseError();
    }
  }

  f->Read(table_header.app_space, sizeof(table_header.app_space));
  if(f->GetDatabaseError() != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }

  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxRDBMS::WriteTableHeader(gxDatabase *f,
					  gxrdTableHeader &table_header, 
					  int flushdb)
{
  if(!f) return gxDBASE_NULL_PTR;

  gxRDBMSHeader file_id;
  __ULWORD__ i;

  f->Write(&file_id, file_id.SizeOf(), f->FileHeaderSize());
  if(f->GetDatabaseError() != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }
  f->Write(table_header.table_name, sizeof(table_header.table_name));
  if(f->GetDatabaseError() != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }
  f->Write(table_header.key_def, sizeof(table_header.key_def));
  if(f->GetDatabaseError() != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }
  f->Write(table_header.form_def, sizeof(table_header.form_def));
  if(f->GetDatabaseError() != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }
  f->Write(table_header.fkey_def, sizeof(table_header.fkey_def));
  if(f->GetDatabaseError() != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }
  f->Write(&table_header.num_fields, sizeof(table_header.num_fields));
  if(f->GetDatabaseError() != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }
  for(i = 0; i < table_header.num_fields; i++) {
    f->Write(&table_header.fields[i], sizeof(gxrdTableHeaderField));
    if(f->GetDatabaseError() != gxDBASE_NO_ERROR) {
      return f->GetDatabaseError();
    }
  }
  f->Write(table_header.app_space, sizeof(table_header.app_space));
  if(f->GetDatabaseError() != gxDBASE_NO_ERROR) {
    return f->GetDatabaseError();
  }

  if(flushdb) f->Flush();
  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxRDBMS::DefineTable(const gxString &tdef)
{
  return table.CreateTable(tdef);
}

gxDatabaseError gxRDBMS::DefineTable(const gxString &tdef, 
				     const gxString &kdef) 
{
  gxDatabaseError err = table.CreateTable(tdef);
  if(err != gxDBASE_NO_ERROR) {
    return err;
  }
  return DefineKeys(kdef);
}

gxDatabaseError gxRDBMS::DefineKeys(const gxString &kdef) 
{
  gxrdDatabaseRecord record;
  key_def.Clear(); // Clear the current key_def
  gxDatabaseError err = record.CreateRecord(table.record_def);
  if(err != gxDBASE_NO_ERROR) {
    return err;
  }

  if(kdef.IFind("PRI") == -1) {
    // No primary key defined
    return gxDBASE_BAD_KEY_DEF;
  }

  // Parse the record definition
  char words[MAXWORDS][MAXWORDLENGTH];
  int num_words, parse_err;
  parse_err = parse(kdef.c_str(), words, &num_words, ',');
  if(parse_err != 0) {
    return gxDBASE_BAD_KEY_DEF;
  }
  if(num_words < 1) {
    return gxDBASE_BAD_KEY_DEF;
  }
  
  gxString key_fields;
  gxString sbuf;
  int i, field_num, bt_node_order;
  int num_pri_keys = 0;

  // Allocate memory for the key fields array
  if(sec_key_fields) delete sec_key_fields;
  sec_key_fields = new int[record.NumFields()];
  if(!sec_key_fields) return gxDBASE_MEM_ALLOC_ERROR;
  for(i = 0; i < (int)record.NumFields(); i++) {
    sec_key_fields[i] = 0;
  }

  for(i = 0; i < num_words; i++) {
    if(*words[i]) {
      field_num = 0;
      sbuf = words[i];
      sbuf.TrimLeadingSpaces();
      sbuf.TrimTrailingSpaces();
      sbuf.ToUpper();
      if(sbuf.IFind("PRI") != -1) {
	if(sbuf.IFind("COL") == -1) {
	  delete[] sec_key_fields;
	  sec_key_fields = 0;
	  return gxDBASE_BAD_KEY_DEF;
	}
	if(num_pri_keys > 1) {
	  delete[] sec_key_fields;
	  sec_key_fields = 0;
	  return gxDBASE_BAD_KEY_DEF;
	}
	sbuf.DeleteBeforeIncluding("COL");
	sbuf.TrimLeadingSpaces();
	sbuf.TrimTrailingSpaces();
	if(sbuf.IFind("NONE") != -1) {
	  primary_key = -1;
	  num_pri_keys++;
	}
	else {
	  field_num = sbuf.Atoi();
	  if(field_num == 0) {
	    delete[] sec_key_fields;
	    sec_key_fields = 0;
	    return gxDBASE_BAD_KEY_DEF;
	  }
	  primary_key = field_num;
	  num_pri_keys++;
	}
      } 
      if(sbuf.IFind("SEC") != -1) {
	if(sbuf.IFind("COL") == -1) {
	  delete[] sec_key_fields;
	  sec_key_fields = 0;
	  return gxDBASE_BAD_KEY_DEF;
	}
	sbuf.DeleteBeforeIncluding("COL");
	sbuf.TrimLeadingSpaces();
	sbuf.TrimTrailingSpaces();
	field_num = sbuf.Atoi();
	if(field_num == 0) {
	  delete[] sec_key_fields;
	  sec_key_fields = 0;
	  return gxDBASE_BAD_KEY_DEF;
	}
	if(num_sec_keys > (int)record.NumFields()) {
	  delete[] sec_key_fields;
	  sec_key_fields = 0;
	  return gxDBASE_BAD_KEY_DEF;
	}
	sec_key_fields[num_sec_keys] = field_num;
	num_sec_keys++;
      } 

      if(sbuf.IFind("ATTRIB") != -1) {
	if(sbuf.IFind("NOCASE") != -1) {
	  compare_case = 0;
	}
	else if(sbuf.IFind("ANCOMPARE") != -1) {
	  compare_alpha = 1;
	}
	else if(sbuf.IFind("ALLOWDUP") != -1) {
	  dup = 1;
	}
	else if(sbuf.IFind("ORDER") != -1) {
	  sbuf.DeleteBeforeIncluding("=");
	  sbuf.TrimLeadingSpaces();
	  sbuf.TrimTrailingSpaces();
	  bt_node_order = sbuf.Atoi();
	  if(bt_node_order == 0) {
	    delete[] sec_key_fields;
	    sec_key_fields = 0;
	    return gxDBASE_BAD_KEY_DEF;
	  }
	  else {
	    order = bt_node_order;
	  }
	}
	else if(sbuf.IFind("RECLAIM") != -1) {
	  sbuf.DeleteBeforeIncluding("=");
	  sbuf.TrimLeadingSpaces();
	  sbuf.TrimTrailingSpaces();
	  if((sbuf == "NONE") || (sbuf == "none")) {
	    method = gxDBASE_RECLAIM_NONE;
	  }
	  else if((sbuf == "BEST") || (sbuf == "best")) {
	    method = gxDBASE_RECLAIM_BESTFIT;
	  }
	  else if((sbuf == "FIRST") || (sbuf == "first")) {
	    method = gxDBASE_RECLAIM_FIRSTFIT;
	  }
	  else {
	    delete[] sec_key_fields;
	    sec_key_fields = 0;
	    return gxDBASE_BAD_KEY_DEF;
	  }
	}
	else {
	  delete[] sec_key_fields;
	  sec_key_fields = 0;
	  return gxDBASE_BAD_KEY_DEF;
	}
      } 
    }
  }
  key_def = kdef; // Set key definition
  return gxDBASE_NO_ERROR;
}

int gxRDBMS::delete_record(FAU_t &block_address, int flushdb, int test_tree)
{
  if(IsClosed()) return 0;

  // PC-lint 05/02/2005: Possible use of null pointer
  if(!pod) return 0;

  if(block_address == (FAU_t)0) {
    pod->SetDataFileError(gxDBASE_BAD_OBJECT_ADDRESS);
    return 0;
  }
  gxDatabase *f = pod->OpenDataFile();
  return f->Delete(block_address+f->BlockHeaderSize());
}


int gxRDBMS::OverWriteRecord(gxrdDatabaseRecord *r, FAU_t &block_address,
			     int flushdb, int test_tree)
// Overwrite the record at the specified record address. NOTE: This
// function assumes that the record's primary or secondary key names
// have not changed. Returns true if successful or false if an error 
// occurs.
{
  if(IsClosed()) return 0;

  // PC-lint 05/02/2005: Possible use of null pointer
  if(!pod) return 0;

  if(!r) {
    pod->SetDataFileError(gxDBASE_NULL_PTR);
    return 0;
  }
  if(table.record_def != r->RecordDef()) {
    pod->SetDataFileError(gxDBASE_BAD_RECORD);
    return 0;
  }

  gxDatabase *f = pod->OpenDataFile();
  gxBlockHeader blk;
  if(ReadBlockHeader(blk, block_address) != gxDBASE_NO_ERROR) {
    return 0;
  }

  // Adjust the number of bytes allocated for the record space 
  // according to the data file revision letter.
  __ULWORD__ length;
  switch(f->GetRevLetter()) {
    case '\0' : case ' ' : case 'e' : case 'E' :
      length = blk.block_length - f->BlockHeaderSize();
      break;
    default:
      length = (blk.block_length-(f->BlockHeaderSize()+ sizeof(gxChecksum)));
      break;
  }

  gxrdRecordHeader rh;
  length -= rh.SizeOf(); // Account for the record header
  if(r->SizeOf() != length) {
    // The length of the record is greater or smaller than 
    // the number of bytes allocated on disk. In order to 
    // overwrite the existing disk record must be deleted 
    // and the new record reinserted.
    gxrdDatabaseRecord prev_record;
    if(Read(&prev_record, blk, block_address) != gxDBASE_NO_ERROR) {
      return 0;
    }
    FAU ba = block_address;
    if(DeleteKey(&prev_record, ba, flushdb, test_tree) != gxDBASE_NO_ERROR) {
      return 0;
    }
    if(!delete_record(block_address, flushdb, test_tree)) {
      return 0;
    }
    if(Write(r, block_address, flushdb, test_tree) != gxDBASE_NO_ERROR) {
      return 0;
    }
    return 1;
  }
  
  // Create a disk record that will be stored in a single
  // contiguous block of disk space
  MemoryBuffer disk_record(r->SizeOf()+ sizeof(gxrdRecordHeader));
  gxDatabaseError err = r->CreateDiskRecord(disk_record);
  if(err != gxDBASE_NO_ERROR) {
    pod->SetDataFileError(err);
    return 0;
  }

  // Overwrite the disk record
  if(f->Write(disk_record.m_buf(), disk_record.length(),
	      gxCurrAddress, flushdb) != gxDBASE_NO_ERROR) {
    return 0;
  }

  return 1;
}

gxDatabaseError gxRDBMS::change_or_add_record(gxrdDatabaseRecord *from, 
					      gxrdDatabaseRecord *to,
					      int add, FAU_t &block_address,
					      int flushdb, int test_tree)
{
  if(IsClosed()) return gxDBASE_FILE_NOT_OPEN_ERROR;

  // PC-lint 05/02/2005: Possible use of null pointer
  if(!pod) return gxDBASE_NULL_PTR;

  if((!from) || (!to)) {
    return pod->SetDataFileError(gxDBASE_NULL_PTR);
  }
  if((table.record_def != from->RecordDef()) || 
     (table.record_def != to->RecordDef())) {
    return pod->SetDataFileError(gxDBASE_BAD_RECORD);
  }

  if(block_address == (FAU_t)0) {
    if(!FindRecord(from, block_address, 1, test_tree)) {
      if(add) {
	return Write(from, flushdb, test_tree);
      }
      else {
	return pod->SetDataFileError(gxDBASE_RECORD_NOT_FOUND);
      }
    }
  }

  if(block_address == gxCurrAddress) {
    block_address = pod->OpenDataFile()->Tell();
    if(pod->GetDataFileError() != gxDBASE_NO_ERROR) {
      return pod->GetDataFileError();
    }
  }

  FAU_t org_ba = block_address;
  FAU ba = block_address;
  if(from->SizeOf() != to->SizeOf()) {
    // Delete, reinsert the record, and return
    if(DeleteKey(from, ba, flushdb, test_tree) != gxDBASE_NO_ERROR) {
      return pod->GetDataFileError();
    }
    if(!delete_record(block_address, flushdb, test_tree)) {
      return pod->GetDataFileError();
    }
    if(Write(to, block_address, flushdb, test_tree) != gxDBASE_NO_ERROR) {
      return pod->GetDataFileError();
    }
    return pod->SetDataFileError(gxDBASE_NO_ERROR);
  }

  // Overwrite the existing record. NOTE: The overwrite call will update
  // the block address if new block is allocated for this record.
  if(!OverWriteRecord(to, block_address, flushdb, test_tree)) {
    return pod->GetDataFileError();
  }
  
  if(org_ba != block_address) {
    // The record was deleted and reinserted. The entry key
    // was updated by the OverWriteRecord() call.
    return pod->SetDataFileError(gxDBASE_NO_ERROR);    
  }

  if(UsingIndex()) { // Test the entry key
    int pk = 0;
    if(primary_key == -1) {
      pk = -1;
    }
    else {
      pk = primary_key - 1;
    }
    gxrdKeyType_t ka = from->CreateKey(pk, CaseCompare());
    if(ka == (gxrdKeyType_t)0) {
      return pod->SetDataFileError(gxDBASE_BAD_KEY);
    }
    gxrdKeyType_t kb = to->CreateKey(pk, CaseCompare());
    if(kb == (gxrdKeyType_t)0) {
      return pod->SetDataFileError(gxDBASE_BAD_KEY);
    }

    // The entry key has not changed for this record
    if(ka == kb) return pod->SetDataFileError(gxDBASE_NO_ERROR);
    
    // Delete the entry key and reinsert it
    ba = block_address;
    if(DeleteKey(from, ba, flushdb, test_tree) !=
       gxDBASE_NO_ERROR) {
      return pod->GetDataFileError();
    }
    return AddKey(to, block_address, flushdb, test_tree); 
  }

  return pod->SetDataFileError(gxDBASE_NO_ERROR);
}

int gxRDBMS::ChangeRecord(gxrdDatabaseRecord *from, gxrdDatabaseRecord *to,
			  FAU_t &block_address, int flushdb, int test_tree)
// Public member function used to change this record. 
// Returns true if successful or false if the record 
// could not be changed.
{
  gxDatabaseError err = change_or_add_record(from, to, 0, block_address,
					     flushdb, test_tree);
  if(err != gxDBASE_NO_ERROR) return 0;
  return 1;
}

int gxRDBMS::ChangeRecord(gxrdDatabaseRecord *from, gxrdDatabaseRecord *to,
			  int flushdb, int test_tree)
{
  FAU_t block_address = (FAU_t)0;
  gxDatabaseError err = change_or_add_record(from, to, 0, block_address,
					     flushdb, test_tree);
  if(err != gxDBASE_NO_ERROR) return 0;
  return 1;
}

int gxRDBMS::ChangeOrAdd(gxrdDatabaseRecord *from, gxrdDatabaseRecord *to,
			 FAU_t &block_address, int flushdb, int test_tree)
{
  gxDatabaseError err = change_or_add_record(from, to, 1, block_address,
					     flushdb, test_tree);
  if(err != gxDBASE_NO_ERROR) return 0;
  return 1;
}

int gxRDBMS::ChangeOrAdd(gxrdDatabaseRecord *from, gxrdDatabaseRecord *to,
			 int flushdb, int test_tree)
{
  FAU_t block_address = (FAU_t)0;
  gxDatabaseError err =  change_or_add_record(from, to, 1, block_address,
					      flushdb, test_tree);
  if(err != gxDBASE_NO_ERROR) return 0;
  return 1;
}

int gxRDBMS::ChangeRecordField(const void *data, __ULWORD__ bytes, 
			       __ULWORD__ col_number, FAU_t &block_address, 
			       int flushdb, int test_tree)
{
  if(IsClosed()) return 0;

  // PC-lint 05/02/2005: Possible use of null pointer
  if(!pod) return 0;

  if(!data) {
    pod->SetDataFileError(gxDBASE_NULL_PTR);
    return 0;
  }
  if(block_address == (FAU_t)0) {
    pod->SetDataFileError(gxDBASE_RECORD_NOT_FOUND);
  }

  gxDatabase *f = pod->OpenDataFile();
  gxDatabaseError err;
  gxrdDatabaseRecord record;

  err = record.CreateRecord(table.record_def);
  if(err != gxDBASE_NO_ERROR) {
    return pod->SetDataFileError(err);
  }
  if(Read(&record, block_address) != gxDBASE_NO_ERROR) {
    return 0;
  }

  // Create a copy of the orginal record
  gxrdDatabaseRecord prev_record(record); 

  err = record.SetField(data, bytes, col_number);
  if(err != gxDBASE_NO_ERROR) {
    pod->SetDataFileError(err);
    return 0;
  }

  if(block_address == gxCurrAddress) {
    block_address = f->Tell();
    if(pod->GetDataFileError() != gxDBASE_NO_ERROR) {
      return 0;
    }
  }

  FAU ba = block_address;

  if(record.SizeOf() != prev_record.SizeOf()) {
    // The length of the record is greater or smaller than 
    // the number of bytes allocated on disk. In order to 
    // overwrite the existing disk record must be deleted 
    // and the new record reinserted.
    if(DeleteKey(&prev_record, ba, flushdb, test_tree) != gxDBASE_NO_ERROR) {
      return 0;
    }
    if(!delete_record(block_address, flushdb, test_tree)) {
      return 0;
    }
    if(Write(&record, block_address, flushdb, test_tree) != gxDBASE_NO_ERROR) {
      return 0;
    }
    return 1;
  }
  
  // Calculate the file offset for this record field
  FAU_t file_offset = block_address + f->BlockHeaderSize();
  gxrdRecordHeader rh;
  file_offset += rh.SizeOf(); // Account for the record header

  __ULWORD__ offset;
  err = record.FieldDataDiskOffset(offset, col_number);
  if(err != gxDBASE_NO_ERROR) {
    pod->SetDataFileError(err);
    return 0;
  }

  file_offset += offset;

  // Overwrite the record field
  if(f->Write(data, bytes, file_offset, flushdb) != 
     gxDBASE_NO_ERROR) {
    return 0;
  }

  if(UsingIndex()) { // Test the entry key
    int pk = 0;
    if(primary_key == -1) {
      pk = -1;
    }
    else {
      pk = primary_key - 1;
    }
    gxrdKeyType_t ka = record.CreateKey(pk, CaseCompare());
    if(ka == (gxrdKeyType_t)0) {
      return pod->SetDataFileError(gxDBASE_BAD_KEY);
    }
    gxrdKeyType_t kb = prev_record.CreateKey(pk, CaseCompare());
    if(kb == (gxrdKeyType_t)0) {
      return pod->SetDataFileError(gxDBASE_BAD_KEY);
    }

    // The entry key has not changed for this record
    if(ka == kb) {
      pod->SetDataFileError(gxDBASE_NO_ERROR);
      return 1;
    }

    // Delete the entry key and reinsert it
    ba = block_address;
    if(DeleteKey(&prev_record, ba, flushdb, test_tree) !=
       gxDBASE_NO_ERROR) {
      return pod->GetDataFileError();
    }
    return AddKey(&record, block_address, flushdb, test_tree); 
  }

  return 1;
}

gxDatabaseError gxRDBMS::GenFileNames(const gxString &tdef, 
				      const gxString &kdef,
				      const gxString &fdef, 
				      const gxString &fkdef)
{
  form_def = fdef;
  fkey_def = fkdef;
  return GenFileNames(tdef, kdef);
}

gxDatabaseError gxRDBMS::GenFileNames(const gxString &tdef, 
				      const gxString &kdef,
				      const gxString &fdef, 
				      const gxString &fkdef,
				      const gxString &dir)
{
  form_def = fdef;
  fkey_def = fkdef;
  db_dir = dir;
  return GenFileNames(tdef, kdef);
}

gxDatabaseError gxRDBMS::GenFileNames(const gxString &tdef, 
				      const gxString &kdef) 
{
  gxDatabaseError err = table.CreateTable(tdef);
  if(err != gxDBASE_NO_ERROR) {
    return err;
  }
  err = DefineKeys(kdef);
  if(err != gxDBASE_NO_ERROR) {
    return err;
  }
  return gen_file_names();
}

gxDatabaseError gxRDBMS::GenFileNames(const gxString &tdef, 
				      const gxString &kdef,
				      const gxString &dir) 
{
  db_dir = dir;
  return GenFileNames(tdef, kdef);
}

gxDatabaseError gxRDBMS::gen_file_names() 
{
  // Set the number of indexes used in this database
  if(primary_key == -1) {
    num_trees = 1;
  }
  else {
    num_trees = 1;
    num_trees += num_sec_keys;
  }

  gxString index_file;
  datafile_name.Clear();
  index_file.Clear();
  int i;
  
  if(!db_dir.is_null()) {
#if defined (__WIN32__)
    char path_sep = '\\';
    db_dir.ReplaceChar('/', '\\');
#else 
    char path_sep = '/';
    db_dir.ReplaceChar('\\', '/');
#endif
    datafile_name << db_dir;
    if(datafile_name[datafile_name.length()-1] != path_sep) {
      datafile_name << path_sep;
    }
    index_file << db_dir;
    if(index_file[index_file.length()-1] != path_sep) {
      index_file << path_sep;
    }
  }
  datafile_name << table.table_name.c_str();
  index_file << table.table_name.c_str();
  datafile_name << datafile_ext;
  index_file << indexfile_ext;
  
  if(indexfile_names) delete[] indexfile_names;
  indexfile_names = new gxString[num_trees];
  if(!indexfile_names) {
    return gxDBASE_MEM_ALLOC_ERROR;
  }
  if((num_sec_keys > 0) && (num_trees > 1)) {
    for(i = 0; i < num_trees; i++) {
      indexfile_names[i].Clear();
      indexfile_names[i] << index_file << (i+1);
    }
  }
  else {
    indexfile_names[0].Clear();
    indexfile_names[0] << index_file;
  }
  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxRDBMS::init_table_header()
{
  gxString tdef = table.table_def;
  table.table_header.Clear();
  gxDatabaseError err = table.CreateTable(tdef);
  if(err != gxDBASE_NO_ERROR) {
    return err;
  }
  table.table_header.SetKeyDef(key_def.c_str());
  if(!form_def.is_null()) {
    table.table_header.SetFormDef(form_def.c_str());
  }
  if(!fkey_def.is_null()) {
    table.table_header.SetFKeyDef(fkey_def.c_str());
  }
  return gxDBASE_NO_ERROR;
}

FAU_t gxRDBMS::HeaderSize()
{
  gxRDBMSHeader file_id;
  FAU_t static_size = \
    FAU_t(table.table_header.SizeOf() + file_id.SizeOf());
  return static_size;
}

gxDatabaseError gxRDBMS::WriteAppSpace(const void *buf, __ULWORD__ bytes,
				       int flushdb) 
{
  if(IsClosed()) return gxDBASE_FILE_NOT_OPEN_ERROR;

  // PC-lint 05/02/2005: Possible use of null pointer
  if(!pod) return gxDBASE_NULL_PTR;

  return WriteAppSpace(pod->OpenDataFile(), table.table_header, buf, bytes,
		       flushdb);
}

gxDatabaseError gxRDBMS::WriteAppSpace(gxDatabase *f, 
				       gxrdTableHeader &table_header,
				       const void *buf, __ULWORD__ bytes,
				       int flushdb)
{
  if(!f) return gxDBASE_NULL_PTR;
  if(bytes > sizeof(table_header.app_space)) {
    return f->SetDatabaseError(gxDBASE_BUFFER_OVERFLOW);
  }
  if((table_header.num_fields == ((__LWORD__)0)) || (!table_header.fields)) {
    return f->SetDatabaseError(gxDBASE_BAD_TABLE_HEADER);
  }
  if(table_header.num_fields != table.table_header.num_fields) {
    return f->SetDatabaseError(gxDBASE_BAD_TABLE_HEADER);
  }

  // Calculate the file offset
  gxrdTableHeaderField thd;
  gxRDBMSHeader file_id;
  
  FAU_t offset = f->FileHeaderSize();
  offset += file_id.SizeOf();
  offset += sizeof(table_header.table_name);
  offset += sizeof(table_header.key_def);
  offset += sizeof(table_header.form_def);
  offset += sizeof(table_header.fkey_def);
  offset += sizeof(table_header.num_fields);
  offset += (FAU_t)table_header.num_fields * thd.SizeOf();
  
  // Load the application data into the table header
  memmove(table_header.app_space, buf, bytes);

  // Write the application space data
  return f->Write(buf, bytes, offset, flushdb);
}

gxDatabaseError gxRDBMS::ReadAppSpace(void *buf, __ULWORD__ bytes,
				      int test_db)
{
  if(IsClosed()) return gxDBASE_FILE_NOT_OPEN_ERROR;

  // PC-lint 05/02/2005: Possible use of null pointer
  if(!pod) return gxDBASE_NULL_PTR;

  return ReadAppSpace(pod->OpenDataFile(), table.table_header, buf, bytes,
		      test_db);

}

gxDatabaseError gxRDBMS::ReadAppSpace(gxDatabase *f, 
				      gxrdTableHeader &table_header,
				      void *buf, __ULWORD__ bytes,
				      int test_db)
{
  if(!f) return gxDBASE_NULL_PTR;
  if(test_db) f->TestFileHeader();

  if(bytes > sizeof(table_header.app_space)) {
    return f->SetDatabaseError(gxDBASE_BUFFER_OVERFLOW);
  }
  if((table_header.num_fields == ((__LWORD__)0)) || (!table_header.fields)) {
    return f->SetDatabaseError(gxDBASE_BAD_TABLE_HEADER);
  }
  if(table_header.num_fields != table.table_header.num_fields) {
    return f->SetDatabaseError(gxDBASE_BAD_TABLE_HEADER);
  }

  // Calculate the file offset
  gxrdTableHeaderField thd;
  gxRDBMSHeader file_id;
  FAU_t offset = f->FileHeaderSize();
  offset += file_id.SizeOf();
  offset += sizeof(table_header.table_name);
  offset += sizeof(table_header.key_def);
  offset += sizeof(table_header.form_def);
  offset += sizeof(table_header.fkey_def);
  offset += sizeof(table_header.num_fields);
  offset += (FAU_t)table_header.num_fields * thd.SizeOf();

  // Write the application space data
  return f->Read(buf, bytes, offset);
}

// --------------------------------------------------------------
// BEGIN: Fault detection and recovery functions
// --------------------------------------------------------------
gxDatabaseError gxRDBMS::CheckDataFile(int &rebuild_df, int &rebuild_ix, 
				       gxString *mesg)
{
  rebuild_df = rebuild_ix = 0;

  if(mesg) mesg->Clear();

  if(!futils_exists(DataFileName().c_str())) {
    if(mesg) {
      *(mesg) << table.table_name << " data file does not exist"
	      << "\n";
      *(mesg) << "Warning: all associated index files will be recreated" 
	      << "\n";
    }
    return create_or_open(0);
  }

  // Ensure that the in memory buffers and the file data
  // stay in sync during multiple file access.
  if(pod) pod->TestDataFile();

  gxDatabaseDebugManager f;
  if(f.BlindOpen(datafile_name.c_str(), df_rev_letter) != gxDBASE_NO_ERROR) {
    if(pod) pod->SetDataFileError(f.GetDatabaseError());
    if(mesg) *(mesg) << f.status_message;
    return f.GetDatabaseError();
  }
  gxDatabaseError err = f.AnalyzeHeader(0, 0);
  if(err != gxDBASE_NO_ERROR) {
    if((err == gxDBASE_REV_MISMATCH) || (err == gxDBASE_VER_MISMATCH)) {
      // Ignore rev and ver warnings
      if(pod) pod->SetDataFileError(gxDBASE_NO_ERROR);
    }
    else {
      if(pod) pod->SetDataFileError(err);
      if(mesg) *(mesg) << f.status_message;
      f.Close();
      rebuild_df = rebuild_ix = 1;
      return err;
    }
  }

  if(mesg) {
    *(mesg) << "Analyzing gxRDBMS table header" << "\n";
  }
  gxRDBMSHeader file_id, compare_file_id;
  if(f.StaticArea() != HeaderSize()) {
    if(mesg) {
      *(mesg) << "Static data area size mismatch" << "\n"
	      << "Cannot read table header from file" << "\n";
    }
    rebuild_df = rebuild_ix = 1;
    return gxDBASE_BAD_TABLE_HEADER;
  }
  file_id.Clear();
  f.Read(&file_id, sizeof(file_id), f.FileHeaderSize());
  if(f.GetDatabaseError() != gxDBASE_NO_ERROR) {
    if(mesg) {
      *(mesg) << "Error reading gxRDBMS file ID string" << "\n" 
	      << f.DatabaseExceptionMessage() << "\n";
    }
    if(pod) pod->SetDataFileError(f.GetDatabaseError());
    return f.GetDatabaseError();
  }
  if(!file_id.TestHeader(compare_file_id)) {
    rebuild_df = rebuild_ix = 1;
    if(mesg) {
      *(mesg) << "Bad gxRDBMS table header" << "\n";
    }
    if(!file_id.TestFileID(compare_file_id)) {
      if(mesg) {
	*(mesg) << "Bad gxRDBMS file identification string" << "\n";
      }
    }
    if(!file_id.TestRevLetter(compare_file_id)) {
      if(mesg) {
	*(mesg) << "Revision letter mismatch" << "\n";
      }
    }
    if(!file_id.TestVersion(compare_file_id)) {
      if(mesg) {
	*(mesg) << "Version number mismatch" << "\n";
      }
    }
    if(pod) pod->SetDataFileError(gxDBASE_BAD_TABLE_HEADER);
    rebuild_df = rebuild_ix = 1;
    return gxDBASE_BAD_TABLE_HEADER;
  }

  gxrdTableHeader table_header;
  err = ReadTableHeader(&f, table_header, file_id, 0);
  if(err != gxDBASE_NO_ERROR) {
    if(mesg) {
      *(mesg) << "Error reading gxRDBMS table header" << "\n" 
	      << f.DatabaseExceptionMessage() << "\n";
    }
    if(pod) pod->SetDataFileError(err);
    return err;
  }

  // NOTE: The table_def and key_def should be set by a previous 
  // GenFileNames() call.
  if(table.table_def.is_null()) {
    if(pod) pod->SetDataFileError(gxDBASE_BAD_TABLE_DEF);
    return gxDBASE_BAD_TABLE_DEF;
  }
  if(key_def.is_null()) {
    if(pod) pod->SetDataFileError(gxDBASE_BAD_KEY_DEF);
    return gxDBASE_BAD_KEY_DEF;
  }

  err = init_table_header();
  if(err != gxDBASE_NO_ERROR) {
    if(pod) pod->SetDataFileError(err);
    return err;
  }

  if(!table_header.VerifyFieldTypes()) {
    if(mesg) {
      *(mesg) << "Bad gxRDBMS table header" << "\n"
	      << "The table header read from file is not valid" << "\n";
    }
    if(pod) pod->SetDataFileError(gxDBASE_BAD_TABLE);
    rebuild_df = rebuild_ix = 1;
    return gxDBASE_BAD_TABLE;
  }

  if(!table_header.TestFieldTypes(table.table_header)) {
    if(mesg) {
      *(mesg) << "Bad gxRDBMS field type" << "\n"
	      << "The table header read from file does not match DEF" 
	      << "\n";
    }
    if(pod) pod->SetDataFileError(gxDBASE_BAD_TABLE_FIELD);
    return gxDBASE_BAD_TABLE_FIELD;
  }

  if(!table_header.TestKeyDef(table.table_header)) {
    if(mesg) {
      *(mesg) << "The key DEF has changed" << "\n"
	      << "The indexing subsystem must be rebuilt" 
	      << "\n";
    }
    rebuild_ix = 1;
    if(pod) pod->SetDataFileError(gxDBASE_BAD_KEY_DEF);
  }

  if(!table_header.TestFKeyDef(table.table_header)) {
    if(mesg) {
      *(mesg) << "The foreign key DEF has changed" << "\n"
	      << "The indexing subsystem must be rebuilt" 
	      << "\n";
    }
    rebuild_ix = 1;
    if(pod) pod->SetDataFileError(gxDBASE_BAD_FKEY_DEF);
  }

  if(mesg) {
    *(mesg) << "Database file header checks good." << "\n" 
	    << "gxRDBMS table header checks good." << "\n";
  }

  f.Close();

  // Ensure the database is connected to this object
  if(!pod) return create_or_open(0); 

  return gxDBASE_NO_ERROR;
}

int gxRDBMS::VerifyDataFile(FAU_t &records) {
  int rebuild_idx = 0;
  int rebuild_df = 0;
  return VerifyDataFile(records, 1, rebuild_df, rebuild_idx);
}

int gxRDBMS::VerifyDataFile(FAU_t &records, int auto_fix, int &rebuild_df,
			    int &rebuild_ix)
{
  if(!pod) {
    // No database is open, so check the data file 
    if(CheckDataFile(rebuild_df, rebuild_ix) != gxDBASE_NO_ERROR) {
      return 0;
    }
  }
  
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!pod) return 0;

  rebuild_df = 0;
  rebuild_ix = 0;

  // Ensure that the in memory buffers and the file data
  // stay in sync during multiple file access.
  pod->TestDataFile();

  records = (FAU_t)0;
  if(!futils_exists(DataFileName().c_str())) {
    // The database does not exist, so create the database and recreate
    // all associated index files.
    return create_or_open(0);
  }
  gxrdDatabaseRecord record(table.record_def);
  gxrdDatabaseRecord compare_record(table.record_def);
  gxDatabase *f = pod->OpenDataFile();
  FAU_t gxdfileEOF = f->GetEOF();
  FAU_t block_address = (FAU_t)0;
  gxBlockHeader blk;
  
  // Search for the first database block in the datafile
  block_address = f->FindFirstBlock(block_address); 
  
  if(block_address == (FAU_t)0) { 
    if(f->NormalBlocks() == (FAU_t)0) { // No blocks found
      // Both the data and index files are empty
      // Tell the caller that the data and index files are empty
      pod->SetDataFileError(gxDBASE_DATAFILE_EMPTY);
      return 1;
    }
    else {
      // The data file is corrupt
      pod->SetDataFileError(gxDBASE_BAD_DATAFILE);
      return 0;
    }
  }
  
  // Walk through the data file to find all existing records
  while(1) { 
    if(FAU_t(block_address + f->BlockHeaderSize()) >= gxdfileEOF) 
      break;
    if(ReadBlockHeader(blk, block_address) != gxDBASE_NO_ERROR) {
      return (FAU_t)0;
    }
    if(f->TestBlockHeader(blk)) {
      if((__SBYTE__)blk.block_status == gxNormalBlock) {
	if(Read(&record, blk) != gxDBASE_NO_ERROR) {
	  if(pod->GetDataFileError() != gxDBASE_BAD_TABLE_ID) {
	    // Found a bad record in the data file.
	    if(pod->GetDataFileError() == gxDBASE_BAD_RECORD) {
	      if(!auto_fix) {
		rebuild_df = 0;
		rebuild_ix = 0;
		pod->SetDataFileError(gxDBASE_BAD_DATAFILE);
		return 0;
	      }
	      if(!f->Delete(block_address + f->BlockHeaderSize())) {
		rebuild_df = 0;
		rebuild_ix = 0;
		return 0;
	      }
	      else {
		rebuild_ix = 1;
		block_address = block_address + blk.block_length;
		continue;
	      }
	    }
	    return 0;
	  }
	  else { 
	    // This data file contains more than one record type
	    block_address = block_address + blk.block_length;
	    continue;
	  }
	}
	if(!verify_record(record, compare_record)) {
	  if(auto_fix) {
	    if(!f->Delete(block_address + f->BlockHeaderSize())) {
	      rebuild_df = 0;
	      rebuild_ix = 0;
	      return 0;
	    }
	    else {
	      rebuild_ix = 1;
	      block_address = block_address + blk.block_length;
	      continue;
	    }
	  }
	  else {
	    rebuild_df = 0;
	    rebuild_ix = 0;
	    pod->SetDataFileError(gxDBASE_BAD_DATAFILE);
	    return 0;
	  }
	}
	records++;
      }
      // Goto the next database block
      block_address = block_address + blk.block_length;
    }
    else { // The data file is corrupt
      rebuild_df = 0;
      rebuild_ix = 0;
      pod->SetDataFileError(gxDBASE_BAD_DATAFILE);
      return 0;
    }
  }
  return 1;
}

gxDatabaseError gxRDBMS::CheckIndex(__ULWORD__ index_number, 
				    int &rebuild, gxString *mesg)
{
  rebuild = 0;
  if(mesg) mesg->Clear();
  
  if(index_number > __ULWORD__(num_trees-1)) { // Stay in bounds
    if(mesg) {
      *(mesg) << "Specified index number is out of range" 
	      << "\n";
    }
    if(pod) pod->SetDataFileError(gxDBASE_BAD_INDEX_NUMBER); 
    return gxDBASE_BAD_INDEX_NUMBER;
  }
  if(!indexfile_names) {
    if(mesg) {
      *(mesg) << "No index files names were generated" 
	      << "\n";
    }
    if(pod) pod->SetDataFileError(gxDBASE_BAD_INDEX_NAME);
    return gxDBASE_BAD_INDEX_NAME;
  }

  // Ensure that the in memory buffers and the file data
  // stay in sync during multiple file access.
  if(pod) pod->TestIndexFile(index_number);

  gxDatabaseDebugManager f;
  if(f.BlindOpen(indexfile_names[index_number].c_str(), if_rev_letter) 
     != gxDBASE_NO_ERROR) {
    if(pod) pod->SetDataFileError(f.GetDatabaseError());
    if(mesg) *(mesg) << f.status_message;
    return f.GetDatabaseError();
  }
  gxDatabaseError err = f.AnalyzeHeader(0, 0);
  if(err != gxDBASE_NO_ERROR) {
    if((err == gxDBASE_REV_MISMATCH) || (err == gxDBASE_VER_MISMATCH)) {
      // Ignore rev and ver warnings
      if(pod) pod->SetDataFileError(gxDBASE_NO_ERROR);
    }
    else {
      if(pod) pod->SetDataFileError(err);
      if(mesg) *(mesg) << f.status_message;
      f.Close();
      rebuild = 1;
      return err;
    }
  }

  if(mesg) {
    *(mesg) << "Analyzing index file header" << "\n";
  }
  
  // The static data area must be large enough to hold at 
  // least on B-tree header.
  if(f.StaticArea() < FAU_t(sizeof(gxBtreeHeader))) {
    if(mesg) {
      *(mesg) << "Static data area size mismatch" << "\n"
	      << "Cannot read table header from file" << "\n";
    }
    rebuild = 1;
    return gxDBASE_BAD_INDEX_FILE;
  }
     
  gxBtreeHeader hdr;
  f.Read(&hdr, sizeof(hdr), f.FileHeaderSize());
  if(f.GetDatabaseError() != gxDBASE_NO_ERROR) {
    if(mesg) {
      *(mesg) << "Error reading index file header" << "\n" 
	      << f.DatabaseExceptionMessage() << "\n";
    }
    if(pod) pod->SetDataFileError(f.GetDatabaseError());
    return f.GetDatabaseError();
  }
  if(hdr.num_trees > (BtreeSize_t)num_trees) {
    if(mesg) {
      *(mesg) << "Bad index file header" << "\n";
    }
    if(pod) pod->SetDataFileError(gxDBASE_BAD_INDEX_FILE);
    rebuild = 1;
    return gxDBASE_BAD_INDEX_FILE;
  }

  rebuild = 0;
  if(mesg) {
    *(mesg) << "Database file header checks good." << "\n" 
	    << "Index file header checks good." << "\n";
  }

  f.Close();

  if(mesg) {
    *(mesg) << "Checking index file consistency" << "\n";
  }

  // Ensure the database is connected to this object
  if(!pod) {
    gxDatabaseError dberr2 = create_or_open(0); 
    if(dberr2 != gxDBASE_NO_ERROR) {
      return dberr2;
    }
  }

  // PC-lint 05/02/2005: Possible use of null pointer
  if(!pod) return gxDBASE_NULL_PTR;

  // Verify the B-tree consistency
  gxBtree *btx = GetDatabase()->Index(index_number);
  gxDatabase *f2 = pod->OpenDataFile();
  if(btx->NumKeys() == (BtreeSize_t)0) {
    if(f2->NormalBlocks() == (FAU_t)0) { // No blocks found
      if(mesg) {
	*(mesg) << "Database is empty" << "\n";
      }
      return gxDBASE_NO_ERROR;
    }
    else {
      if(mesg) {
	*(mesg) << "Index file failed consistency check" << "\n";
      }
      rebuild = 1;
      return pod->SetDataFileError(gxDBASE_BAD_INDEX_FILE);
    }
  }
  if(f2->NormalBlocks() != btx->NumKeys()) {
    if(mesg) {
      *(mesg) << "Index file failed consistency check" << "\n";
    }
    rebuild = 1;
    return pod->SetDataFileError(gxDBASE_BAD_INDEX_FILE);
  }

  return gxDBASE_NO_ERROR;
}

int gxRDBMS::verify_record(gxrdDatabaseRecord &a, gxrdDatabaseRecord &b)
{
  if(a == b) return 1; 
  if(a.RecordDef() != b.RecordDef()) return 0;
  if(a.TableName() != b.TableName()) return 0;
  if(a.NumFields() != b.NumFields()) return 0;
  __ULWORD__ i;
  for(i = 0; i < a.NumFields(); i++) {
    // Test the field types. NOTE: The number of byte used
    // for the field data may vary so the number of bytes
    // and field data cannot be tested here.
    if(a.FieldType(i) != b.FieldType(i)) return 0;
  }
  return 1;
}

int gxRDBMS::VerifyIndex(__ULWORD__ index_number, FAU_t &keys)
// Compares the data file to the index file.
// Returns true if data and index file match.
{
  if(IsClosed()) return 0;

  // PC-lint 05/02/2005: Possible use of null pointer
  if(!pod) return 0;

  if(index_number > __ULWORD__(num_trees-1)) { // Stay in bounds
    pod->SetDataFileError(gxDBASE_BAD_INDEX_NUMBER);
    return 0;
  }

  // Ensure that the in memory buffers and the file data
  // stay in sync during multiple file access.
  pod->TestIndexFile(index_number);
  pod->TestDataFile();
  
  gxrdDatabaseRecord record(table.record_def);
  gxrdDatabaseRecord compare_record(table.record_def);
  gxrdDatabaseKey key, compare_key;
  gxBtree *btx = GetDatabase()->Index(index_number);
  gxDatabase *f = pod->OpenDataFile();
  BtreeNode curr_node(btx->KeySize(), btx->NodeOrder());
  keys = (FAU_t)0; // Application key count

  // Walk through the tree starting at the first key
  if(btx->FindFirst(key)) {
    if(!ReadRecord(&record, key.KeyID())) {
      return 0;
    }
    if(!verify_record(record, compare_record)) {
      // The index file test has detected a bad data file
      pod->SetDataFileError(gxDBASE_BAD_DATAFILE);
    }
    keys++;
    while(btx->FindNext(key, compare_key, curr_node)) {
      if(!ReadRecord(&record, key.KeyID())) {
	return 0;
      }
      if(!verify_record(record, compare_record)) {
	// The index file test has detected a bad data file
	pod->SetDataFileError(gxDBASE_BAD_DATAFILE);
      }
      keys++;
    }
    // The number of keys does not equal the number of records
    if(keys != f->NormalBlocks()) return 0;
  }
  else { // No keys found so test the data file
    if(btx->NumKeys() == (BtreeSize_t)0) {
      if(f->NormalBlocks() == (FAU_t)0) { // No blocks found
	// Both the data and index files are empty
	// Tell the caller that the data and index files are empty
	pod->SetDataFileError(gxDBASE_DATAFILE_EMPTY);
	pod->SetIndexFileError(gxDBASE_INDEX_EMPTY, index_number);
	return 1;
      }
      else {
	pod->SetIndexFileError(gxDBASE_BAD_INDEX_FILE, index_number);
	return 0;
      }
    }
    else {
      // The B-tree header indicates that keys are stored but no
      // key where found.
      pod->SetIndexFileError(gxDBASE_BAD_INDEX_FILE, index_number);
      
      if(f->NormalBlocks() == (FAU_t)0) { // No blocks found
	// Signal to the caller that the data file may be bad as well
	// since no records were found.
	pod->SetDataFileError(gxDBASE_BAD_DATAFILE);
      }
      return 0;
    }
  }

  return 1; // Compare passed
}

gxDatabaseError gxRDBMS::insert_key(gxrdDatabaseRecord *r, FAU block_address, 
				    __ULWORD__ index_number)
{
  // PC-lint 05/02/2005: Possible use of null pointer
  if(!pod) return gxDBASE_NULL_PTR;

  int pk = 0;
  if(primary_key == -1) {
    pk = -1;
  }
  else {
    pk = index_number;
  }
  
  gxrdKeyType_t k = r->CreateKey(pk, CaseCompare());
  
  if(k == (gxrdKeyType_t)0) {
    return pod->SetIndexFileError(gxDBASE_BAD_KEY);
  }
  gxrdRecordHeader rh(table.table_name, r->NumFields());
  gxrdTableID_t tid = rh.table_id;
  gxrdKeyID_t kid = block_address;
  gxrdDatabaseKey key(k, kid, tid, dup);
  gxrdDatabaseKey compare_key;
  
  // Insert the key
  if(!pod->Index(index_number)->Insert(key, compare_key)) {
    return pod->GetIndexFileError();
  }
    
  return gxDBASE_NO_ERROR;
}

int gxRDBMS::RebuildIndex(__ULWORD__ index_number, 
			  FAU_t &records, FAU_t &keys) 
// Function used to rebuild a damaged index file. Returns the total number
// of members inserted into the new index file or zero if an error occurs.
{
  return RebuildIndex(index_number, order, num_trees, if_rev_letter,
		      records, keys);
}

int gxRDBMS::RebuildIndex(__ULWORD__ index_number, BtreeNodeOrder_t no, 
			  int ntrees, __SBYTE__ rev, 
			  FAU_t &records, FAU_t &keys)
{
  int reconnect = 0;
  int rv = rebuild_index(index_number, no, ntrees, rev, records, keys);
  if(rv != 1) {
    switch(rv) {
      case 0: // Bad pointer or index number
	reconnect = 1;
	break;
      case -1: // Cannot close specified index
	break;
      case -2: // Cannot overwrite specified index
	reconnect = 1;
	break;
      case -3: // Data file is corrupt
	reconnect = 1;
	break;
      case -4: // Data file record read failed
	reconnect = 1;
	break;
      case -5: // Key insertion failed
	reconnect = 1;
	break;
      case -6: // Data file is corrupt
	reconnect = 1;
	break;
      default:
	// Should never reach default case statement.
	// Included for compilers that require a default 
	// case.
	break;
    }
  }
  
  // Reconnect the index file to prevent any possible runtime
  // crashes if the caller makes any index file calls following
  // a rebuild failure.
  if(reconnect) create_or_open(0);

  return (rv == 1);
}

int gxRDBMS::rebuild_index(__ULWORD__ index_number, BtreeNodeOrder_t no, 
			   int ntrees, __SBYTE__ rev, 
			   FAU_t &records, FAU_t &keys)
{
  if(IsClosed()) return 0;

  // PC-lint 05/02/2005: Possible use of null pointer
  if(!pod) return 0;

  if(index_number > __ULWORD__(num_trees-1)) { // Stay in bounds
    pod->SetDataFileError(gxDBASE_BAD_INDEX_NUMBER);
    return 0;
  }
  if(!indexfile_names) {
    pod->SetDataFileError(gxDBASE_BAD_INDEX_NAME);
    return 0;
  }

  // Keep the database on-line during index file rebuild
  pod->DisableIndex(); 

  // Close and recreate the specified index file
  if(pod->CloseIndex(index_number) != gxDBASE_NO_ERROR) return -1;
  gxrdDatabaseKey key_type;  
  if(pod->CreateIndex(indexfile_names[index_number].c_str(), index_number, 
		      key_type, no, ntrees, rev) != gxDBASE_NO_ERROR) {
    return -2;
  }
  
  gxDatabase *f = pod->OpenDataFile();

  // Ensure that the in memory buffers and the file data
  // stay in sync during multiple file access.
  pod->TestDataFile();
  
  gxrdDatabaseRecord record(table.record_def);
  gxrdDatabaseKey key, compare_key;
  FAU_t gxdfileEOF = f->GetEOF();
  FAU_t block_address = (FAU_t)0;
  gxBlockHeader blk;
  keys = (FAU_t)0;
  records = (FAU_t)0;

  // Search for the first database block in the datafile
  block_address = f->FindFirstBlock(block_address); 

  if(block_address == (FAU_t)0) { 
    if(f->NormalBlocks() == (FAU_t)0) { // No blocks found
      // Both the data and index files are empty
      // Tell the caller that the data and index files are empty
      pod->SetDataFileError(gxDBASE_DATAFILE_EMPTY);
      pod->SetIndexFileError(gxDBASE_INDEX_EMPTY, index_number);
      return 1;
    }
    else {
      // The data file is corrupt
      pod->SetDataFileError(gxDBASE_BAD_DATAFILE);
      return -3;
    }
  }

  // Walk through the data file to find all existing records
  while(1) { 
    if(FAU_t(block_address + f->BlockHeaderSize()) >= gxdfileEOF) 
      break;
    if(ReadBlockHeader(blk, block_address) != gxDBASE_NO_ERROR) {
      return (FAU_t)0;
    }
    if(f->TestBlockHeader(blk)) {
      if((__SBYTE__)blk.block_status == gxNormalBlock) {
	if(Read(&record, blk) != gxDBASE_NO_ERROR) {
	  if(pod->GetDataFileError() != gxDBASE_BAD_TABLE_ID) {
	    return -4;
	  }
	  else { 
	    // This data file contains more than one record type
	    block_address = block_address + blk.block_length;
	    continue;
	  }
	}
	records++;
	// Insert the record into the tree
	if(insert_key(&record, block_address, index_number) != 
	   gxDBASE_NO_ERROR) {
	  return -5;
	}
	keys++;
      }
      // Goto the next database block
      block_address = block_address + blk.block_length;
    }
    else {
      // The data file is corrupt
      pod->SetDataFileError(gxDBASE_BAD_DATAFILE);
      return -6;
    }
  }

  // Flushing the index file buffers following batch insertion
  pod->FlushIndexFile(index_number);

  // Enable the index file
  pod->EnableIndex();
  return 1;
}

int gxRDBMS::RebuildDataFile(FAU_t &records, const char *tempfile)
{
  return RebuildDataFile(df_rev_letter, HeaderSize(), 0, 
			 records, tempfile);
}

int gxRDBMS::RebuildDataFile(int copy_static_data, FAU_t &records, 
			     const char *tempfile)
{
  return RebuildDataFile(df_rev_letter, HeaderSize(), copy_static_data, 
			 records, tempfile);
}


int gxRDBMS::RebuildDataFile(__SBYTE__ rev, FAU_t static_area, 
			     int copy_static_data,
			     FAU_t &records, const char *tempfile)
{
  int rv = rebuild_data_file(rev, static_area, copy_static_data,
			     records, tempfile);
  int reconnect = 0;
  if(rv != 1) {
    switch(rv) {
      case 0: // No database if open
	reconnect = 1;
	break;
      case -1: // Cannot close current database
	break;
      case -2: // Blind open failed
	reconnect = 1;
	break;
      case -3: // Rebuild failed
	reconnect = 1;
	break;
      case -4: // Delete current data file failed
	reconnect = 1;
	break;
      case -5: // Rename failed
	reconnect = 1;
	// NOTE: A reconnected after a rename fail will
	// cause an empty data file to be created.
	break;
      case -6: case -7:// Open rebuilt database failed
	if(create_or_open(1) == gxDBASE_NO_ERROR) {
	  return 1;
	}
	break;
      case -8: case -9: // Restore table header failed
	if(create_or_open(1) == gxDBASE_NO_ERROR) {
	  return 1;
	}
	break;
      case -10: // Reconnect failed
	reconnect = 1;
	break;
      default:
	// Should never reach default case statement.
	// Included for compilers that require a default 
	// case.
	break;
    }
  }
  
  // Reconnect the data file to prevent any possible runtime
  // crashes if the caller makes any data file calls following
  // a rebuild failure.
  if(reconnect) create_or_open(0);

  return (rv == 1);
}

int gxRDBMS::rebuild_data_file(__SBYTE__ rev, FAU_t static_area, 
			       int copy_static_data,
			       FAU_t &records, const char *tempfile)

{
  // Set the temp file name
  gxString fname;
  int ext = 1;
  int has_pod = 0;
  if(pod) has_pod = 1;
  if(!tempfile) { // Create a unique temp file name
    fname.Clear();
    fname << datafile_name << "_rebuild_temp_file" << datafile_ext
	  << ".00" << ext;
    while(futils_exists(fname.c_str())) {
      fname.Clear();
      fname << datafile_name << "_rebuild_temp_file" << datafile_ext
	    << ".00" << ++ext;
    }
  }
  else {
    fname = tempfile;
  }

  // Close the data file
  if(pod) {
    if(pod->CloseDataFile() != gxDBASE_NO_ERROR) {
      return -1;
    }
  }

  // Use the debug file manager to rebuild the data file
  gxDatabaseDebugManager f;
  if(f.BlindOpen(datafile_name.c_str(), df_rev_letter) != 
     gxDBASE_NO_ERROR) {
    if(pod) pod->SetDataFileError(f.GetDatabaseError());
    return -2;
  }

  int rv = f.Rebuild(fname.c_str(), &records, gxDatabase::gxVersion, 
		     rev, static_area, 0);
  gxDatabaseError err = f.GetDatabaseError();
  if(err != gxDBASE_NO_ERROR) {
    // Record the error. The file was rebuild but there is an 
    // error condition that may need to be evaluated
    if(pod) pod->SetDataFileError(err);
  }
  if(!rv) { // The rebuild failed
    f.Close();
    return -3;
  } 
  f.Close();

  if(futils_remove(datafile_name.c_str()) != 0) {
    if(pod) pod->SetDataFileError(gxDBASE_FILE_DELETE_ERROR);
    return -4;
  }
  if(futils_rename(fname.c_str(), datafile_name.c_str()) != 0) {
    if(pod) pod->SetDataFileError(gxDBASE_FILE_RENAME_ERROR);
    return -5;
  }

  if(!pod) {
    pod = new POD;
    if(!pod) return -6;
  }

  if(pod->OpenDataFile(datafile_name.c_str()) != gxDBASE_NO_ERROR) {
    return -7;
  }

  err = init_table_header();
  if(err != gxDBASE_NO_ERROR) {
    pod->SetDataFileError(err);
    return -8;
  }
  err = WriteTableHeader(pod->OpenDataFile(), table.table_header, 1);
  if(err != gxDBASE_NO_ERROR) {
    pod->SetDataFileError(err);
    return -9;
  }

  if(!has_pod) { // Must reconnect and remove all associated index files
    err = create_or_open(0);
    if(err != gxDBASE_NO_ERROR) {
      pod->SetDataFileError(err);
      return -10;
    } 
  }

  // Remove all temp files from this rebuild and any previous rebuilds
  // that failed.
  ext = 1;
  int num_prev_to_find = 10;
  while(num_prev_to_find--) {
    fname.Clear();
    fname << datafile_name << "_rebuild_temp_file" << datafile_ext
	  << ".00" << ext;
    if(futils_exists(fname.c_str())) break;
    ext++;
  }
  while(futils_exists(fname.c_str())) {
    if(futils_remove(fname.c_str()) != 0) break;
    fname.Clear();
    fname << datafile_name << "_rebuild_temp_file" << datafile_ext
	  << ".00" << ++ext;
  }
  
  return 1;
}

void gxRDBMS::Release()
// Reset all gxRDBMS pointers when multiple objects are 
// bound to any of the gxRDBMS pointers. A release call 
// will prevent the gxRDBMS destructor from deleting a 
// pointer if this gxRDBMS objects goes out of scope or 
// is no longer needed. NOTE: A release call should only 
// be invoked when more than one object is referencing 
// any of the gxRDBMS pointers constructed with this object. 
{
  // PC-lint 09/12/2005: Ignore memory leak warnings because
  // the caller is aware of another object bound to the same 
  // pointers. This object should not delete these variables 
  // to prevent data loss.
#if defined(__PCLINT_CHECK__)
  return;
#else
  pod = 0;
  sec_key_fields = 0;
  indexfile_names = 0;
#endif
}
// --------------------------------------------------------------
// END: Fault detection and recovery functions
// --------------------------------------------------------------

// --------------------------------------------------------------
// BEGIN: Search, and sort, functions
// --------------------------------------------------------------
void gxRDBMS::set_record_parms(gxrdDatabaseRecord *r,
			       __ULWORD__ field,
			       int case_cmp, int alpha_cmp)
{
  r->SetCompareField(field);
  if(!case_cmp) {
    r->ResetCaseCompare();
  }
  else {
    r->SetCaseCompare();
  }
  if(!alpha_cmp) {
    r->ResetAlphaCompare();
  }
  else {
    r->SetAlphaCompare();
  }
} 

gxDatabaseError gxRDBMS::SortOrderList(
				   gxBStree<gxrdDatabaseRecord> &virtual_list,
				       __ULWORD__ index_number, 
				       __ULWORD__ key_number,
				       int case_cmp, int alpha_cmp, 
				       FAU_t list_limit, FAU_t list_start, 
				       FAU_t list_end)
// Function used to build a sort ordered list. Returns gxDBASE_NO_ERROR if
// no errors occur. 
{
  gxString sbuf;
  virtual_list.Clear(); // Clear any existing list entries

  // Test for database error conditions before building the list
  if(IsClosed()) return gxDBASE_NO_DATABASE_OPEN;

  // PC-lint 05/02/2005: Possible use of null pointer
  if(!pod) return gxDBASE_NULL_PTR;

  if(index_number > __ULWORD__(NumTrees()-1)) {
    return pod->SetDataFileError(gxDBASE_BAD_INDEX_NUMBER);
  }

  // Test the range limits
  FAU_t num_records = pod->OpenDataFile()->NormalBlocks();
  if(num_records == (FAU_t)0) return gxDBASE_NO_ERROR;

  if((list_start > num_records) || (list_end > num_records)) {
    return pod->SetDataFileError(gxDBASE_BAD_RANGE_LIMIT);
  }
  if(list_end < list_start) {
    return pod->SetDataFileError(gxDBASE_BAD_RANGE_LIMIT);
  }
  if((list_end - list_start) > num_records) {
    return pod->SetDataFileError(gxDBASE_BAD_RANGE_LIMIT);
  }

  gxrdDatabaseKey key, compare_key;
  gxBtree *btx = GetDatabase()->Index(index_number);
  gxrdDatabaseRecord record, compare_record;
  gxDatabaseError err;
  FAU_t record_count = (FAU_t)0;
  FAU_t key_count = (FAU_t)0;
  int exists;
  __ULWORD__ field = 0;
  if(key_number == (__ULWORD__)-1) {
    field = (__ULWORD__)-1;
  }
  else {
    field = key_number - 1;
  }
  int pk = 0;
  if(primary_key == -1) {
    pk = -1;
  }
  else {
    pk = primary_key - 1;
  }
  err = record.CreateRecord(table.record_def);
  if(err != gxDBASE_NO_ERROR) {
    return pod->SetDataFileError(err);
  }
  if(key_number > record.NumFields()) {
    return pod->SetDataFileError(gxDBASE_BUFFER_OVERFLOW);
  }

  set_record_parms(&record, field, case_cmp, alpha_cmp);

  // Walk through the tree starting at the first key
  if(btx->FindFirst(key)) {
    if((list_start == (FAU_t)0) || (list_start == (FAU_t)1)) {
      if(!ReadRecord(&record, key.KeyID())) {
	err = GetDatabase()->GetDataFileError();
	if(err != gxDBASE_NO_ERROR) {
	  // Return the data file error first
	  return err;
	}
	return GetDatabase()->GetIndexFileError(index_number);
      }
      // Use the primary key to build the sort order list
      set_record_parms(&record, pk, compare_case, compare_alpha);
      if(!virtual_list.Insert(record, &exists)) {
	if(!exists) { // A fatal error occured during tree insert
	  return pod->SetDataFileError(gxDBASE_VIRTUAL_LIST_ERROR);
	}
      }
      set_record_parms(&record, field, case_cmp, alpha_cmp);
      record_count++;
    }
    key_count++;
    while(btx->FindNext(key, compare_key)) {
      if(record_count >= ((list_end-list_start)+1)) break;
      if((record_count >= list_limit) || (record_count >= list_end)) break; 
      if(key_count >= list_start) {
	if(!ReadRecord(&record, key.KeyID())) {
	  err = GetDatabase()->GetDataFileError();
	  if(err != gxDBASE_NO_ERROR) {
	    return err; // Return the data file error first
	  }
	  return GetDatabase()->GetIndexFileError(index_number);
	}

	// Use the primary key to build the sort order list
	set_record_parms(&record, pk, compare_case, compare_alpha);
	if(!virtual_list.Insert(record, &exists)) {
	  if(!exists) { // A fatal error occured during tree insert
	    return pod->SetDataFileError(gxDBASE_VIRTUAL_LIST_ERROR);
	  }
	}
	set_record_parms(&record, field, case_cmp, alpha_cmp);
	record_count++;
      }
      key_count++;
    }
  }
  return pod->SetDataFileError(gxDBASE_NO_ERROR);
}

gxDatabaseError gxRDBMS::Search(gxBStree<gxrdDatabaseRecord> &virtual_list,
				gxrdDatabaseRecord &compare_record,
				__ULWORD__ index_number,
				__ULWORD__ search_field,
				int case_cmp, int alpha_cmp, 
				int find_all_matches, FAU_t list_limit)
{
  gxString sbuf;
  virtual_list.Clear(); // Clear any existing list entries

  // Test for database error conditions before building the list
  if(IsClosed()) return gxDBASE_NO_DATABASE_OPEN;

  // PC-lint 05/02/2005: Possible use of null pointer
  if(!pod) return gxDBASE_NULL_PTR;

  if(index_number > __ULWORD__(NumTrees()-1)) {
    return pod->SetDataFileError(gxDBASE_BAD_INDEX_NUMBER);
  }

  // Test the range limits
  FAU_t num_records = pod->OpenDataFile()->NormalBlocks();
  if(num_records == (FAU_t)0) return gxDBASE_NO_ERROR;

  if(list_limit > num_records) {
    return pod->SetDataFileError(gxDBASE_BAD_RANGE_LIMIT);
  }

  gxrdDatabaseKey key, compare_key;
  gxBtree *btx = GetDatabase()->Index(index_number);
  gxrdDatabaseRecord record;
  int exists;
  gxDatabaseError err;
  FAU_t record_count = (FAU_t)0;
  __ULWORD__ field = 0;
  if(search_field == (__ULWORD__)-1) {
    field = (__ULWORD__)-1;
  }
  else {
    field = search_field - 1;
  }
  int pk = 0;
  if(primary_key == -1) {
    pk = -1;
  }
  else {
    pk = primary_key - 1;
  }
  err = record.CreateRecord(table.record_def);
  if(err != gxDBASE_NO_ERROR) {
    return pod->SetDataFileError(err);
  }
  if(search_field > record.NumFields()) {
    return gxDBASE_BUFFER_OVERFLOW;
  }

  set_record_parms(&record, field, case_cmp, alpha_cmp);
  set_record_parms(&compare_record, field, case_cmp, alpha_cmp);

  // Walk through the tree starting at the first key
  if(btx->FindFirst(key)) {
    if(!ReadRecord(&record, key.KeyID())) {
      err = GetDatabase()->GetDataFileError();
      if(err != gxDBASE_NO_ERROR) {
	return err; // Return the data file error first
      }
      return GetDatabase()->GetIndexFileError(index_number);
    }
    if(record.Compare(compare_record, field, case_cmp,
		      alpha_cmp, find_all_matches) == 0) {
      // Use the primary key to build the sort order list
      set_record_parms(&record, pk, compare_case, compare_alpha);
      if(!virtual_list.Insert(record, &exists)) {
	if(!exists) { // A fatal error occured during tree insert
	  return pod->SetDataFileError(gxDBASE_VIRTUAL_LIST_ERROR);
	}
      }
      set_record_parms(&record, field, case_cmp, alpha_cmp);
    }
    record_count++;
    while(btx->FindNext(key, compare_key)) {
      if(record_count >= list_limit) break; 
      if(!ReadRecord(&record, key.KeyID())) {
	err = GetDatabase()->GetDataFileError();
	if(err != gxDBASE_NO_ERROR) {
	  return err; // Return the data file error first
	}
	return GetDatabase()->GetIndexFileError(index_number);
      }
      if(record.Compare(compare_record, field, case_cmp,
			alpha_cmp, find_all_matches) == 0) {
	// Use the primary key to build the sort order list
	set_record_parms(&record, pk, compare_case, compare_alpha);
	if(!virtual_list.Insert(record, &exists)) {
	  if(!exists) { // A fatal error occured during tree insert
	    return pod->SetDataFileError(gxDBASE_VIRTUAL_LIST_ERROR);
	  }
	}
	set_record_parms(&record, field, case_cmp, alpha_cmp);
      }
      record_count++;
    }
  }
  return pod->SetDataFileError(gxDBASE_NO_ERROR);
}

gxDatabaseError gxRDBMS::Search(gxBStree<gxrdDatabaseRecord> &virtual_list,
				gxString &search_string,
				__ULWORD__ index_number,
				__ULWORD__ search_field, int case_cmp, 
				int alpha_cmp, int find_all_matches, 
				FAU_t list_limit)
{
  gxString sbuf;
  virtual_list.Clear(); // Clear any existing list entries

  // Test for database error conditions before building the list
  if(IsClosed()) return gxDBASE_NO_DATABASE_OPEN;

  // PC-lint 05/02/2005: Possible use of null pointer
  if(!pod) return gxDBASE_NULL_PTR;

  if(index_number > __ULWORD__(NumTrees()-1)) {
    return pod->SetDataFileError(gxDBASE_BAD_INDEX_NUMBER);
  }
  gxrdDatabaseRecord compare_record;
  gxDatabaseError err;

  __ULWORD__ field = 0;
  if(search_field == (__ULWORD__)-1) {
    return pod->SetDataFileError(gxDBASE_BUFFER_OVERFLOW);
  }
  else {
    field = search_field - 1;
  }

  err = compare_record.CreateRecord(table.record_def);
  if(err != gxDBASE_NO_ERROR) {
    return pod->SetDataFileError(err);
  }
  if(search_field > compare_record.NumFields()) {
    return pod->SetDataFileError(gxDBASE_BUFFER_OVERFLOW);
  }

  set_record_parms(&compare_record, field, case_cmp, alpha_cmp);

  // Type buffers
  gxINT16 i16buf;
  gxINT32 i32buf;
  gxFLOAT64 f64buf;
  CDate date_buf;
  char bool_buf = 0;
#if defined (__64_BIT_DATABASE_ENGINE__)
  gxINT64 i64buf;
#endif

  __ULWORD__ length;
  switch(compare_record.FieldType(field)) {
    case gxrdBOOL :
      sbuf = search_string; sbuf.ToUpper();
      if((sbuf == "Y") || (sbuf == "YES") ||
	 (sbuf == "TRUE") || (sbuf == "1") ||
	 (sbuf == "CHECKED")) {
	bool_buf = 1;
      }
      if((sbuf == "N") || (sbuf == "NO") ||
	 (sbuf == "FALSE") || (sbuf == "0") ||
	 (sbuf == "UNCHECKED")) {
	bool_buf = 0;
      }
      compare_record.SetField(&bool_buf, sizeof(bool_buf), field);
      break;
    case gxrdCHAR : case gxrdVCHAR :
      length = search_string.length();
      if(compare_record.FieldType(field) == gxrdCHAR) { 
	// Ensure fixed length strings stay inbounds
	if(length > compare_record.FieldLength(field)) {
	  length = compare_record.FieldLength(field);
	}
      }
      compare_record.SetField(search_string.GetSPtr(), length, field);
      break;
      
    case gxrdBINARY : 
      compare_record.SetField(search_string.GetSPtr(), search_string.length(),
			      field);
      break;
      
    case gxrdBLOB :
      compare_record.SetField(search_string.GetSPtr(), search_string.length(),
			      field);
      break;
      
    case gxrdINT16 :
      i16buf = (short)search_string.Atoi();
      compare_record.SetField(&i16buf, sizeof(i16buf), field);
      break;
      
    case gxrdINT32 :
      i32buf = search_string.Atol();
      compare_record.SetField(&i32buf, sizeof(i32buf), field);
      break;
      
    case gxrdINT64 :
#if defined (__64_BIT_DATABASE_ENGINE__)
      i64buf = search_string.Atoi64();
      compare_record.SetField(&i64buf, sizeof(i64buf), field);
#else
      f64buf = search_string.Atof();
      compare_record.SetField(&f64buf, sizeof(f64buf), field);
#endif
      break;
      
    case gxrdFLOAT : case gxrdCURRENCY :
      f64buf = search_string.Atof();
      compare_record.SetField(&f64buf, sizeof(f64buf), field);
      break;
      
    case gxrdDATE :
      date_buf = search_string.c_str();
      compare_record.SetField(&date_buf, sizeof(date_buf), field);
      break;
      
    default: // Unknown or Unhandled type 
      break;
  }

  return Search(virtual_list, compare_record, index_number, search_field,
		case_cmp, alpha_cmp, find_all_matches, list_limit);
}
// --------------------------------------------------------------
// END: Search and sort functions
// --------------------------------------------------------------

// --------------------------------------------------------------
// BEGIN: Standalone functions
// --------------------------------------------------------------
GXDLCODE_API void gxrdDefaultPIDName(gxString &fname)
{
  fname = "gxRDBMS.pid";
}

GXDLCODE_API int gxrdWritePID(const char *pid_file_name) 
{
  gxString fname;
  if(pid_file_name) {
    fname = pid_file_name;
  }
  else {
    gxrdDefaultPIDName(fname);
  }

  DiskFileB outfile;
  if(outfile.df_Create(fname.c_str()) != DiskFileB::df_NO_ERROR) {
    return 0;
  }
  return 1;
}

GXDLCODE_API int gxrdHasPIDFile(const char *pid_file_name) 
{
  gxString fname;
  if(pid_file_name) {
    fname = pid_file_name;
  }
  else {
    gxrdDefaultPIDName(fname);
  }
  return futils_exists(fname.c_str());
}

GXDLCODE_API int gxrdRemovePID(const char *pid_file_name)
{
  gxString fname;
  if(pid_file_name) {
    fname = pid_file_name;
  }
  else {
    gxrdDefaultPIDName(fname);
  }
  if(futils_exists(fname.c_str())) {
    if(futils_remove(fname.c_str()) != 0) {
      return 0;
    }
  }
  return 1;
}
// --------------------------------------------------------------
// END: Standalone functions
// --------------------------------------------------------------

#ifdef __BCC32__
#pragma warn .8057
#pragma warn .8080
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
