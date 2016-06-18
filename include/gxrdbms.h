// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxrdbms.h
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

Relational database management system used with the DataReel 
library gxDatabase, gxBtree, and gxStream classes.
*/
// ----------------------------------------------------------- //  
#ifndef __GX_RDBMS_HPP__
#define __GX_RDBMS_HPP__

#include "gxdlcode.h"

#include "gxrdbhdr.h"
#include "gxlist.h"
#include "gxbstree.h"
#include "bstreei.h"

// Relational database manager class
class GXDLCODE_API gxRDBMS
{
public:
  gxRDBMS();
  virtual ~gxRDBMS();

private: // Disallow copying and assignment
  gxRDBMS(const gxRDBMS &ob) { }
  void operator=(const gxRDBMS &ob) { }

public: // User interface
  gxDatabaseError CreateTable(const gxString &tdef, const gxString &kdef);
  gxDatabaseError CreateTable(const gxString &tdef, const gxString &kdef,
 			      const gxString &dir);
  gxDatabaseError CreateTable(const gxString &tdef, const gxString &kdef,
  			      const gxString &fdef, const gxString &fkdef);
  gxDatabaseError CreateTable(const gxString &tdef, const gxString &kdef,
  			      const gxString &fdef, const gxString &fkdef,
			      const gxString &dir);
  int AddRecord(gxrdDatabaseRecord *r, int flushdb = 0, int test_tree = 0);
  int FindRecord(gxrdDatabaseRecord *r, FAU_t &block_address,
		 int load_record = 1, int test_tree = 0);
  int FindRecord(gxrdDatabaseRecord *r, int load_record = 1, 
		 int test_tree = 0);
  int DeleteRecord(gxrdDatabaseRecord *r, int flushdb = 0,
		   int test_tree = 0);
  int ReadRecord(gxrdDatabaseRecord *r, 
		 FAU_t block_address = gxCurrAddress);

public: // File and setup functions
  __SBYTE__ DataFileRev() const {return  df_rev_letter; }
  __SBYTE__ IndexFileRev() const {return  if_rev_letter; }
  void SetDataFileRev(__SBYTE__ rev) { df_rev_letter = rev; }
  void SetIndexFileRev(__SBYTE__ rev) { if_rev_letter = rev; }
  const gxString& DataFileExt() const { return datafile_ext; }
  const gxString& IndexFileExt() const { return indexfile_ext; }
  void SetDataFileExt(const UString &ext) { datafile_ext = ext; }
  void SetIndexFileExt(const UString &ext) { indexfile_ext = ext; }
  gxDatabaseAccessMode Mode() const { return mode; }
  void SetMode(gxDatabaseAccessMode m) { mode = m; }
  gxDatabaseReclaimMethod Method() const { return method; }
  void SetMethod(gxDatabaseReclaimMethod m) { method = m; }
  const gxString& DataFileName() const { return datafile_name; }
  gxString *IndexFileNames() const { return indexfile_names; }

public: // Low-level table and record functions
  gxDatabaseError Create(const gxString &tdef);
  gxDatabaseError Create(const gxString &tdef, const gxString &dir);
  gxDatabaseError Write(gxrdDatabaseRecord *r, FAU_t &block_address,
			int flushdb = 0, int test_tree = 0);
  gxDatabaseError Write(gxrdDatabaseRecord *r, 
			int flushdb = 0, int test_tree = 0);
  gxDatabaseError Read(gxrdDatabaseRecord *r, 
		       FAU_t block_address = gxCurrAddress);
  gxDatabaseError Read(gxrdDatabaseRecord *r, gxBlockHeader &blk, 
		       FAU_t block_address = gxCurrAddress);
  gxDatabaseError ReadBlockHeader(gxBlockHeader &blk, 
				  FAU_t block_address = gxCurrAddress);
  FAU_t Find(gxrdDatabaseRecord *r, __ULWORD__ field, int case_cmp,
	     int alpha_cmp, int test_tree);

public: // Index key functions
  int AddKey(DatabaseKeyB &key, DatabaseKeyB &compare_key,
	     unsigned index_number = 0, int flushdb = 0);
  gxDatabaseError AddKey(gxrdDatabaseRecord *r, FAU block_address, 
			 int flushdb = 0, int test_tree = 0);
  int FindKey(DatabaseKeyB &key, DatabaseKeyB &compare_key,
	      unsigned index_number = 0, int test_tree = 0);
  FAU_t FindKey(gxrdDatabaseRecord *r, int test_tree = 0);
  int DeleteKey(DatabaseKeyB &key, DatabaseKeyB &compare_key,
		unsigned index_number = 0, int flushdb = 0);
  gxDatabaseError DeleteKey(gxrdDatabaseRecord *r, FAU &block_address, 
			    int flushdb = 0, int test_tree = 0);
  int NumSecKeys() const { return num_sec_keys; }
  int *SecKeyFields() const { return sec_key_fields; }
  int PrimaryKey() const { return primary_key; }
  int NoCaseCompare() const { return compare_case == 0; }
  int CaseCompare() const { return compare_case == 1; }
  int NoAlphaCompare() const { return compare_alpha == 0; }
  int AlphaCompare() const { return compare_alpha == 1; }
  int AllowDup() const { return dup == 1; }
  int NumTrees() const { return num_trees; }
  BtreeNodeOrder_t Order() const { return order; }

public: // Database panel functions
  int OverWriteRecord(gxrdDatabaseRecord *r, FAU_t &block_address,
		      int flushdb = 0, int test_tree = 0);
  int ChangeRecord(gxrdDatabaseRecord *from, gxrdDatabaseRecord *to,
		   int flushdb = 0, int test_tree = 0);
  int ChangeRecord(gxrdDatabaseRecord *from, gxrdDatabaseRecord *to,
		  FAU_t &block_address, int flushdb = 0, int test_tree = 0);
  int ChangeOrAdd(gxrdDatabaseRecord *from, gxrdDatabaseRecord *to,
		  int flushdb = 0, int test_tree = 0);
  int ChangeOrAdd(gxrdDatabaseRecord *from, gxrdDatabaseRecord *to,
		  FAU_t &block_address, int flushdb = 0, int test_tree = 0);

public: // Database grid functions
  int ChangeRecordField(const void *data, __ULWORD__ bytes, __ULWORD__ 
			col_number, FAU_t &block_address, 
			int flushdb = 0, int test_tree = 0);

public: // Search and sort functions
  gxDatabaseError SortOrderList(gxBStree<gxrdDatabaseRecord> &virtual_list,
				FAU_t list_limit, FAU_t list_start, 
				FAU_t list_end) {
    return SortOrderList(virtual_list, (primary_key-1), primary_key, 
			 compare_case, compare_alpha, list_limit,
			 list_start, list_end);
  }
  gxDatabaseError SortOrderList(gxBStree<gxrdDatabaseRecord> &virtual_list,
				__ULWORD__ index_number, 
				__ULWORD__ key_number,
				int case_cmp, int alpha_cmp, 
				FAU_t list_limit, FAU_t list_start, 
				FAU_t list_end);
  gxDatabaseError Search(gxBStree<gxrdDatabaseRecord> &virtual_list,
			 gxrdDatabaseRecord &compare_record, 
			 int find_all_matches, FAU_t list_limit) {
    return Search(virtual_list, compare_record, (primary_key-1), primary_key,
		  compare_case, compare_alpha, find_all_matches, list_limit);
  }
  gxDatabaseError Search(gxBStree<gxrdDatabaseRecord> &virtual_list,
			 gxrdDatabaseRecord &compare_record,
			 __ULWORD__ index_number, __ULWORD__ search_field,
			 int case_cmp, int alpha_cmp, int find_all_matches,
			 FAU_t list_limit);
  gxDatabaseError Search(gxBStree<gxrdDatabaseRecord> &virtual_list,
			 gxString &search_string,
			 int find_all_matches, FAU_t list_limit) {
    return Search(virtual_list, search_string, (primary_key-1), primary_key,
		  compare_case, compare_alpha, find_all_matches, list_limit);
  }
  gxDatabaseError Search(gxBStree<gxrdDatabaseRecord> &virtual_list,
			 gxString &search_string, __ULWORD__ index_number,
			 __ULWORD__ search_field, int case_cmp, 
			 int alpha_cmp, int find_all_matches, 
			 FAU_t list_limit);

public: // Fault detection and recovery functions
  int IsOpen() const { return pod != 0; }
  int IsClosed() const { return pod == 0; }
  FAU_t NumRecords() {
    return pod->OpenDataFile()->NormalBlocks();
  }
  FAU_t NumKeys(__ULWORD__ index_number = 0) {
    return GetDatabase()->Index(index_number)->NumKeys();
  }
  int IsEmpty() const { 
    return (pod->OpenDataFile()->NormalBlocks() == (FAU_t)0);
  }
  void Release();
  gxDatabaseError GenFileNames(const gxString &tdef, 
			       const gxString &kdef);
  gxDatabaseError GenFileNames(const gxString &tdef, 
			       const gxString &kdef,
			       const gxString &dir);
  gxDatabaseError GenFileNames(const gxString &tdef, 
			       const gxString &kdef,
			       const gxString &fdef, 
			       const gxString &fkdef);
  gxDatabaseError GenFileNames(const gxString &tdef, 
			       const gxString &kdef,
			       const gxString &fdef, 
			       const gxString &fkdef,
			       const gxString &dir);
  gxDatabaseError CheckIndex(__ULWORD__ index_number, int &rebuild, 
			     gxString *mesg = 0);
  int VerifyIndex(__ULWORD__ index_number, FAU_t &keys);
  int RebuildIndex(__ULWORD__ index_number, FAU_t &records, FAU_t &keys); 
  int RebuildIndex(__ULWORD__ index_number, BtreeNodeOrder_t no, int ntrees,
		   __SBYTE__ rev, FAU_t &records, FAU_t &keys); 
  gxDatabaseError CheckDataFile(int &rebuild_df, int &rebuild_ix, 
				gxString *mesg = 0);
  int VerifyDataFile(FAU_t &records, int auto_fix, int &rebuild_df,
		     int &rebuild_ix);
  int VerifyDataFile(FAU_t &records);
  int RebuildDataFile(__SBYTE__ rev, FAU_t static_area, int copy_static_data,
		      FAU_t &records, const char *tempfile = 0);
  int RebuildDataFile(FAU_t &records, const char *tempfile = 0);
  int RebuildDataFile(int copy_static_data, FAU_t &records, 
		      const char *tempfile = 0);

public: // Def functions
  const gxString& RecordDef() const { return table.record_def; }
  const gxString& TableDef() const { return table.table_def; }
  const gxString& KeyDef() const { return key_def; }
  const gxString& FormDef() const { return form_def; }
  const gxString& FKeyDef() const { return fkey_def; }
  const gxrdDatabaseTable& Table() const { return table; }
  void SetRecordDef(const gxString &rdef) { table.record_def = rdef; }
  void SetTableDef(const gxString &tdef) { table.table_def = tdef; }
  void SetKeyDef(const gxString &kdef) { key_def = kdef; }
  void SetFormDef(const gxString &fdef) { form_def = fdef; }
  void SetFKeyDef(const gxString &fkdef) { fkey_def = fkdef; }
  void SetTable(const gxrdDatabaseTable &t) { table = t; }

public: // Table header functions
  FAU_t HeaderSize();
  gxDatabaseError WriteAppSpace(const void *buf, __ULWORD__ bytes,
				int flushdb = 0); 
  gxDatabaseError WriteAppSpace(gxDatabase *f, 
				gxrdTableHeader &table_header,
				const void *buf, __ULWORD__ bytes,
				int flushdb = 0);
  gxDatabaseError ReadAppSpace(void *buf, __ULWORD__ bytes,
			       int test_db = 0);
  gxDatabaseError ReadAppSpace(gxDatabase *f, 
			       gxrdTableHeader &table_header,
			       void *buf, __ULWORD__ bytes,
			       int test_db = 0);
  gxDatabaseError WriteTableHeader(int init, int flushdb = 0);
  gxDatabaseError WriteTableHeader(gxrdTableHeader &table_header, 
				   int flushdb = 0);
  gxDatabaseError WriteTableHeader(gxDatabase *f,
				   gxrdTableHeader &table_header, 
				   int flushdb = 0);
  gxDatabaseError ReadTableHeader(int test_db = 0);
  gxDatabaseError ReadTableHeader(gxrdTableHeader &table_header,
				  gxRDBMSHeader &file_id,
				  int test_db = 0);
  gxDatabaseError ReadTableHeader(gxDatabase *f, 
				  gxrdTableHeader &table_header,
				  gxRDBMSHeader &file_id,
				  int test_db = 0);
  gxDatabaseError DefineKeys(const gxString &kdef); 
  gxDatabaseError DefineTable(const gxString &tdef, 
			      const gxString &kdef);
  gxDatabaseError DefineTable(const gxString &tdef);
 
protected: // Internal processing functions
  gxDatabaseError create_or_open(int rewrite_header);
  gxDatabaseError gen_file_names();
  gxDatabaseError init_table_header();
  gxDatabaseError insert_key(gxrdDatabaseRecord *r, FAU block_address, 
			     __ULWORD__ index_number);
  int rebuild_index(__ULWORD__ index_number, BtreeNodeOrder_t no, int ntrees,
		   __SBYTE__ rev, FAU_t &records, FAU_t &keys);
  int rebuild_data_file(__SBYTE__ rev, FAU_t static_area, int copy_static_data,
			FAU_t &records, const char *tempfile);
  int verify_record(gxrdDatabaseRecord &a, gxrdDatabaseRecord &b);
  gxDatabaseError change_or_add_record(gxrdDatabaseRecord *from, 
				       gxrdDatabaseRecord *to,
				       int add, FAU_t &block_address, 
				       int flushdb, int test_tree);
  int delete_record(FAU_t &block_address, int flushdb, int test_tree);
  void set_record_parms(gxrdDatabaseRecord *r, __ULWORD__ field, 
			int case_cmp, int alpha_cmp);

public: // POD functions
  gxDatabaseError Close();
  void Flush() { if(pod) pod->Flush(); }
  int TestDatabase() const { return pod->TestDatabase(); }
  POD *GetDatabase() const { return pod; }
  gxBtree *GetBtree(__ULWORD__ index_number = 0) const {
    return pod->Index(index_number);
  }
  gxDatabase *GetDataFile() const {
    return pod->OpenDataFile();
  }
  int UsingIndex() const { return pod->UsingIndex() == 1; }

protected: 
  POD *pod; // Pointer to this objects POD manager
  gxrdDatabaseTable table; // Database table
  gxString db_dir;     // Absolute path directory name
  gxString key_def;    // Database key definition
  gxString form_def;   // Form definition
  gxString fkey_def;   // Foreign key definition
  int num_trees;       // Total number of B-tree index files used
  int num_sec_keys;    // Total number of secondary keys defined
  int *sec_key_fields; // Secondary key fields
  int primary_key;     // Primary key field
  int compare_case;    // True if comparing string case
  int compare_alpha;   // True if comparing alphanumeric strings
  int dup;             // Variable used to allow duplicate names
  gxString datafile_name;    // Name of data file used to store tables
  gxString *indexfile_names; // Array of index file names
  gxDatabaseAccessMode mode; // Variable used to toggle read/write file access
  BtreeNodeOrder_t order;    // Variable used to set the B-tree order
  __SBYTE__ df_rev_letter;   // Variable used to set the data file rev letter
  __SBYTE__ if_rev_letter;   // Variable used to set the index file rev letter
  gxString datafile_ext;     // Data file dot extension
  gxString indexfile_ext;    // Index file dot extension

  // Database deleted/removed block reclamation method
  gxDatabaseReclaimMethod method;
};

// Standalone functions
// --------------------------------------------------------------
GXDLCODE_API void gxrdDefaultPIDName(gxString &fname);
GXDLCODE_API int gxrdWritePID(const char *pid_file_name = 0);
GXDLCODE_API int gxrdHasPIDFile(const char *pid_file_name = 0);
GXDLCODE_API int gxrdRemovePID(const char *pid_file_name = 0);

#endif // __GX_RDBMS_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
