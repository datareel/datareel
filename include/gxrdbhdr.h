// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxrdbhdr.h
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

Type definitions, constants, and data structures used the 
DataReel relational database management system.

Changes:
==============================================================
10/01/2003: Added the gxrdDatabaseRecord::exclude_fields public 
data member to the gxrdDatabaseRecord class. This modification 
was made to allow specified fields to be excluded when disk 
records are created and loaded.
==============================================================
*/
// ----------------------------------------------------------- //  
#ifndef __GX_RDBMS_HEADERS_HPP__
#define __GX_RDBMS_HEADERS_HPP__

// --------------------------------------------------------------
// Include Files
// --------------------------------------------------------------
#include "gxdlcode.h"

#include "pod.h"
#include "gxstring.h"
#include "membuf.h"
#include "gxint16.h"
#include "cdate.h"
#include "gxfloat.h"
// --------------------------------------------------------------

// --------------------------------------------------------------
// Type Definitions
// --------------------------------------------------------------
#if defined (__64_BIT_DATABASE_ENGINE__)
typedef gxINT64 gxrdKeyID;       // Persistent RDBMS key ID type
typedef __LLWORD__ gxrdKeyID_t;  // Native RDBMS key ID type 

#else // Use the 32-bit version by default
typedef gxINT32 gxrdKeyID;     // Persistent RDBMS key ID type 
typedef __LWORD__ gxrdKeyID_t; // Native RDBMS key ID type 
#endif

typedef gxUINT32 gxrdTableID;     // Persistent RDBMS table ID type 
typedef __ULWORD__ gxrdTableID_t; // Native RDBMS table ID type 

typedef gxUINT32 gxrdKeyType;     // Persistent RDBMS Btree key type 
typedef __ULWORD__ gxrdKeyType_t; // Native RDBMS Btree key type 
// --------------------------------------------------------------

// --------------------------------------------------------------
// Constants
// --------------------------------------------------------------
// Table Constants
const int tableFIELD_NAME_LEN = 255; // Max number of bytes for a field name

// Maximum number bytes allowed for table/file names
const int tableNAME_LEN = 255; 

// Minimum number of bytes allocated for persistent definitions
const int tableDEF_LEN = 2048;

const int tableAPPLICATION_SPACE = 64*255; // Scratch pad area for application
const int tableMIN_FIELDS = 1; // Every table must have at least one field
const int tableMAX_FIELDS = 255; // Allow up to 255 fields per table

// Record Constants
const int recordMIN_FIELDS = tableMIN_FIELDS;
const int recordMAX_FIELDS = tableMAX_FIELDS;

// gxRDBMS Constants
const int gxrdFILE_ID_STR_LEN = 8; // Number of bytes used for signature string

// B-tree constants
const int gxrdDEFAULT_ORDER = 64; // Default B-tree node order
const int gxrdMAX_TREES = tableMAX_FIELDS; // Max number of B-trees per file
// --------------------------------------------------------------

// --------------------------------------------------------------
// Format specifies used by applications to modify field types
// --------------------------------------------------------------
// Common string modifiers
const char gxrdMultiLine = 'M'; // Multi-line text
const char gxrdPassword = 'P';  // Password string
const char gxrdURL = 'U';       // Uniform resource locator

// Common BLOB modifiers
const char gxrdASCII = 'A'; // ASCII text file
const char gxrdDOC = 'D';   // Docutment text file
const char gxrdHTML = 'H';  // HTML text file
const char gxrdImage = 'I'; // Image text file
const char gxrdMM = 'm';    // Multimedia file
const char gxrdXML = 'X';   // XML text file
const char gxrdComp = 'Z';  // Compressed file

// Common boolean modifiers
const char gxrdCheckBox = 'C';  // Checked or unchecked
const char gxrdTrueFalse = 'T'; // True or false
const char gxrdYesNo = 'Y';     // Yes of no

// Common currency modifiers
const char gxrdDollar = 'D'; // Dollar symbol
// --------------------------------------------------------------

// --------------------------------------------------------------
// Enumerations
// --------------------------------------------------------------
enum gxrdFIELD_TYPES {
  gxrdINVALID_TYPE = 0, // Invalid data type specified 

  // Built-in relational field types
  gxrdCHAR,  // Fixed length character string type
  gxrdINT16, // 16-bit integer type
  gxrdINT32, // 32-bit integer type
  gxrdINT64, // 64-bit integer type
  gxrdFLOAT, // Floating point type

  // Extended relational field types
  gxrdBOOL,   // Boolean type 
  gxrdVCHAR,  // Variable length character string type
  gxrdBINARY, // User defined type
  gxrdBLOB,   // Large binary object type

  // Application specific field types
  gxrdCURRENCY, // Currency data type
  gxrdDATE      // Date data type
};
// --------------------------------------------------------------

// --------------------------------------------------------------
// Data Structures
// --------------------------------------------------------------
class GXDLCODE_API gxRDBMSHeader
{
public:
  gxRDBMSHeader();
  ~gxRDBMSHeader() { }
  gxRDBMSHeader(const gxRDBMSHeader &ob) { Copy(ob); }
  void operator=(const gxRDBMSHeader &ob) { Copy(ob); }

public:
  const char *FileID() { return (const char *)file_id; }
  char RevLetter() { return file_id[gxrdFILE_ID_STR_LEN-1]; }
  __ULWORD__ Version() { return (__ULWORD__)file_version; }
  int TestHeader(const gxRDBMSHeader &ob);
  int TestFileID(const gxRDBMSHeader &ob);
  int TestVersion(const gxRDBMSHeader &ob);
  int TestRevLetter(const gxRDBMSHeader &ob);
  void Copy(const gxRDBMSHeader &ob);
  void Clear();
  __ULWORD__ SizeOf() { return (sizeof(file_id)+sizeof(file_version)); }

private:
  char file_id[gxrdFILE_ID_STR_LEN];
  gxUINT32 file_version;
};

struct GXDLCODE_API gxrdRecordHeader
{
  gxrdRecordHeader() { Clear(); }
  gxrdRecordHeader(const gxString &table_name, gxUINT32 nfields) {
    SetTableID(table_name);
    num_fields = nfields;
  }
  ~gxrdRecordHeader() { }
  gxrdRecordHeader(const gxrdRecordHeader &ob) { Clear(); Copy(ob);  }
  void operator=(const gxrdRecordHeader &ob) { Copy(ob); }

  __ULWORD__ SizeOf() {
    return (sizeof(table_id) + sizeof(num_fields));
  }
  void Clear() {
    table_id = num_fields = (gxUINT32)0;
  }
  void Copy(const gxrdRecordHeader &ob);
  void SetTableID(const gxString &table_name);
  gxUINT32 GetCRC(const gxString &table_name);
  int TestTableID(const gxString &table_name) {
    return table_id == GetCRC(table_name);
  }

  gxUINT32 table_id;   // Unique table ID created with CRC32 of table name
  gxUINT32 num_fields; // Number of field this contained in this record  
};

struct GXDLCODE_API gxrdTableHeaderField
{
  gxrdTableHeaderField() { Clear(); }
  ~gxrdTableHeaderField() { }
  gxrdTableHeaderField(const gxrdTableHeaderField &ob) { 
    Clear();
    Copy(ob); 
  }
  void operator=(const gxrdTableHeaderField &ob) { Copy(ob); }

  void Clear();
  void Copy(const gxrdTableHeaderField &ob);
  __ULWORD__ SizeOf();

  int VerifyFieldType();
  int TestFieldType(const gxrdTableHeaderField &ob, int strict_test = 0);
  int TestFieldVars(const gxrdTableHeaderField &ob);

  // Persistent field information when printing or displaying a table
  char field_type_id;     // Data type for this field
  char format_specifier;  // Optional field modifier, 0 if not set
  gxUINT32 column_length; // Num of chars required to display the field name 
  gxUINT32 array_length;  // Optional array len of fixed types, 0 if not set 
  char field_name[tableFIELD_NAME_LEN]; // Name of this field
};

struct GXDLCODE_API gxrdTableHeader 
{
  gxrdTableHeader();
  gxrdTableHeader(__ULWORD__ nfields);
  ~gxrdTableHeader() { DestroyHeader(); }
  gxrdTableHeader(const gxrdTableHeader &ob);
  void operator=(const gxrdTableHeader &ob) { Copy(ob); }

  void Copy(const gxrdTableHeader &ob); 
  void Clear();
  __ULWORD__ SizeOf();
  int AllocHeader(__ULWORD__ nfields);
  void DestroyHeader();
  int GetField(gxrdTableHeaderField *fh, __ULWORD__ nfield);
  int SetField(char field_type_id, char format_specifier, char *field_name, 
	       gxUINT32 column_length, gxUINT32 array_length, 
	       __ULWORD__ nfield);
  void SetTableName(const char *s);
  void SetKeyDef(const char *s);
  void SetFormDef(const char *s);
  void SetFKeyDef(const char *s);
  int TestFieldTypes(const gxrdTableHeader &ob);
  int VerifyFieldTypes();
  int TestTableName(const gxrdTableHeader &ob);
  int TestKeyDef(const gxrdTableHeader &ob);
  int TestFormDef(const gxrdTableHeader &ob);
  int TestFKeyDef(const gxrdTableHeader &ob);
  int TestFieldVars(const gxrdTableHeader &ob);
  gxString TableDef() const;

  // Table infomation null terminated strings.
  char table_name[tableNAME_LEN]; // Name of this table
  char key_def[tableDEF_LEN];     // Key definition
  char form_def[tableDEF_LEN];    // Form definition
  char fkey_def[tableDEF_LEN];    // Foreign key definition

  // Table fields
  gxUINT32 num_fields; // Number of fields in this header
  gxrdTableHeaderField *fields; // Array of header fields

  // Application specific static storage area
  char app_space[tableAPPLICATION_SPACE];
};

// Structure used during print, export, and import functions
// to exclude record fields.
struct GXDLCODE_API gxrdExcludeFields
{
  gxrdExcludeFields() { fields = 0; num_fields = 0; }
  gxrdExcludeFields(gxUINT32 nfields) {
    fields = 0; num_fields = 0;
    SetNumFields(nfields);
  }
  ~gxrdExcludeFields() { Clear(); }
  gxrdExcludeFields(const gxrdExcludeFields &ob) {
    fields = 0; num_fields = 0; Copy(ob);
  }
  void operator=(const gxrdExcludeFields &ob) {
    if(this != &ob) Copy(ob);
  }

  int Exclude(gxUINT32 field);
  void SetNumFields(gxUINT32 nfields);
  void Clear();
  __ULWORD__ SizeOf();
  void Copy(const gxrdExcludeFields &ob);

  gxUINT32 num_fields; // Number of field to exclude
  gxUINT32 *fields;    // Zero-based array of field numbers
};
 
class GXDLCODE_API gxrdDatabaseRecord
{ 
public:
  gxrdDatabaseRecord(__ULWORD__ cmp_field = (__ULWORD__)-1, 
		     int case_cmp = 1, int alpha_cmp = 0) { 
    db_record = 0; num_fields = 0; Clear(); 
    compare_field = cmp_field; // Primary key 
    compare_case = case_cmp;   // Compare string case
    compare_alpha = alpha_cmp; // Perform alphanumeric compare
  }
  gxrdDatabaseRecord(const gxString &def, 
		     __ULWORD__ cmp_field = (__ULWORD__)-1, 
		     int case_cmp = 1, int alpha_cmp = 0) {
    // NOTE: Use this constructor with known good record definitions.
    db_record = 0; num_fields = 0; Clear();
    CreateRecord(def, cmp_field, case_cmp, alpha_cmp);
  }
  ~gxrdDatabaseRecord() { DeleteArray(); }
  gxrdDatabaseRecord(const gxrdDatabaseRecord &ob) { 
    db_record = 0; num_fields = 0; Clear();
    Copy(ob); 
  }
  void operator=(const gxrdDatabaseRecord &ob) { Copy(ob); }

public: // Record functions
  gxDatabaseError SetField(const void *data, __ULWORD__ bytes, 
			   __ULWORD__ col_number);
  gxDatabaseError GetField(void *data, __ULWORD__ bytes, 
			   __ULWORD__ col_number);
  void *GetField(__ULWORD__ col_number);
  void *GetField(__ULWORD__ col_number, __ULWORD__ &bytes);
  void *GetField(__ULWORD__ col_number, __ULWORD__ &bytes,
		 gxDatabaseError &err);
  gxDatabaseError FieldDiskOffset(__ULWORD__ &offset, 
				  __ULWORD__ col_number);
  gxDatabaseError FieldDataDiskOffset(__ULWORD__ &offset, 
				      __ULWORD__ col_number);
  gxDatabaseError CreateRecord(const gxString &def, int clear_rec = 1);
  gxDatabaseError CreateRecord(const gxString &def, __ULWORD__ cmp_field, 
			       int case_cmp, int alpha_cmp);
  gxDatabaseError CreateDiskRecord(MemoryBuffer &disk_record);
  gxDatabaseError LoadDiskRecord(const MemoryBuffer &disk_record);
 
  void Clear();
  void Copy(const gxrdDatabaseRecord &ob);
  __ULWORD__ SizeOf() const;
  __ULWORD__ SizeOfBuffer() const;
  __ULWORD__ SizeOfField(__ULWORD__ col_number) const;
  __ULWORD__ NumFields() const { return num_fields; }
  MemoryBuffer *GetRecord() const { return db_record; }
  const gxString& TableName() const { return table_name; }
  const gxString& RecordDef() const { return record_def; }
  int CompareField() const { return compare_field; }
  int NoCaseCompare() const { return compare_case == 0; }
  int CaseCompare() const { return compare_case == 1; }
  int NoAlphaCompare() const { return compare_alpha == 0; }
  int AlphaCompare() const { return compare_alpha == 1; }
  int Compare(const gxrdDatabaseRecord &ob, __ULWORD__ field,
	      int case_cmp, int aplha_cmp, int find_match = 0) const;
  int TestRecord(const gxrdDatabaseRecord &ob) const {
    return record_def == ob.record_def; 
  }
  void GetRecordTypeSize(gxrdFIELD_TYPES type, __ULWORD__ &size,
			 __ULWORD__ &overhead);

  // NOTE: These functions will change default behavior of the 
  // overloaded comparison operators
  void SetCompareField(int cmp_field) { compare_field = cmp_field; }
  void ResetCompareField() { compare_field = -1; }
  void SetCaseCompare() { compare_case = 1; }
  void SetAlphaCompare() { compare_alpha = 1; }
  void ResetCaseCompare() { compare_case = 0; }
  void ResetAlphaCompare() { compare_alpha = 0; }

public: // Field functions
  __ULWORD__ FieldLength(__ULWORD__ col_number);
  int FieldType(__ULWORD__ col_number);
  int FieldHasNull(__ULWORD__ col_number);
  void *FieldData(__ULWORD__ col_number);
  void *FieldData(__ULWORD__ col_number, char &field_type_id,
		  gxUINT32 &num_bytes);

public: // Key functions
  gxrdKeyType_t CreateKey(__ULWORD__ field, int case_cmp) const;

public: // Overloaded operators required for tree and list structures
  friend int operator==(const gxrdDatabaseRecord &a,
			const gxrdDatabaseRecord &b) {
    return (a.Compare(b, a.compare_field, 
		      a.compare_case, a.compare_alpha) == 0);
  }
  friend int operator!=(const gxrdDatabaseRecord &a, 
			const gxrdDatabaseRecord &b) {
    return (a.Compare(b, a.compare_field, 
		      a.compare_case, a.compare_alpha) != 0);

  }
  friend int operator<(const gxrdDatabaseRecord &a, 
		       const gxrdDatabaseRecord &b) {
    return (a.Compare(b, a.compare_field, 
		      a.compare_case, a.compare_alpha) < 0);
  }
  friend int operator>(const gxrdDatabaseRecord &a,
		       const gxrdDatabaseRecord &b) {
    return (a.Compare(b, a.compare_field, 
		      a.compare_case, a.compare_alpha) > 0);
  }

private: // Internal processing functions
  gxDatabaseError InitRecord(const gxString &def);
  int AllocArray(__ULWORD__ nfields);
  void DeleteArray();

private:
  gxString table_name;   // Associated table name
  gxString record_def;   // Record sting definition 
  __ULWORD__ num_fields; // Number of record fields
  int compare_field;     // Field(s) used for comparison
  int compare_case;      // True if comparing string case
  int compare_alpha;     // True if comparing alphanumeric strings

  // The record fields will be stored in an array of non-contiguous memory. 
  // This allows each field to be resized with having to resize the entire 
  // array.
  MemoryBuffer *db_record; // Array of record fields

public:
  // Public memeber used to allow specified fields to be excluded
  // when disk records are created and loaded. 
  gxrdExcludeFields exclude_fields;
};

class GXDLCODE_API gxrdDatabaseTable 
{
public:
  gxrdDatabaseTable() { }
  ~gxrdDatabaseTable () { }
  gxrdDatabaseTable(const gxrdDatabaseTable &ob) { Copy(ob); }
  void operator=(const gxrdDatabaseTable &ob) { Copy(ob); }

public:
  gxDatabaseError CreateTable(const gxString &tdef);
  void Copy(const gxrdDatabaseTable &ob);
  __ULWORD__ SizeOf();
  void Clear();

public:
  gxString table_name; // Name of this table
  gxString record_def; // Record definition
  gxString table_def;  // Table definition
  gxrdTableHeader table_header; // Persistent table header
};

// Primary key type used by all gxRDBMS files
struct GXDLCODE_API gxrdDatabaseKey_t
{
  gxrdDatabaseKey_t() { Clear(); }
  ~gxrdDatabaseKey_t() { }
  gxrdDatabaseKey_t(const gxrdDatabaseKey_t &ob) { Clear(); Copy(ob); }
  gxrdDatabaseKey_t& operator=(const gxrdDatabaseKey_t &ob) {
    Copy(ob);
    return *this;
  }

  void Copy(const gxrdDatabaseKey_t &ob);
  void Clear() {
    k = (gxrdKeyType_t)0;
    tid = (gxrdTableID_t)0;
    kid = (gxrdKeyID_t)0;
  }
  __ULWORD__ SizeOf() {
    return (sizeof(k)+(sizeof(tid)+sizeof(kid)));
  }

  // Persistent data members
  // NOTE: Do not change the ordering of the data members in order to
  // maintain a uniform byte alignment between the in-memory copy of
  // the object and the copy stored on disk. If the member alignment
  // is changed all database files using this structure will have to
  // be rebuilt.
  gxrdKeyType k;   // gxRDBMS key
  gxrdTableID tid; // Key table identification number
  gxrdKeyID kid;   // Key data file address/identification number
};

// Primary key class used by this database 
class GXDLCODE_API gxrdDatabaseKey : public DatabaseKeyB
{
public:
  gxrdDatabaseKey(int dup_names = 0) : DatabaseKeyB((char *)&key) {
    key.kid = (gxrdKeyID_t)0;
    key.tid = (gxrdTableID_t)0;
    allow_dup_names = dup_names;
  }
  gxrdDatabaseKey(gxrdKeyType_t &k, gxrdKeyID_t kid = (gxrdKeyID_t)0, 
		  gxrdTableID_t tid = (gxrdTableID_t)0, int dup_names = 0) : 
    DatabaseKeyB((char *)&key) {
    key.k = k;
    key.tid = tid;
    key.kid = kid;
    allow_dup_names = dup_names;
  }
  gxrdDatabaseKey& operator=(const gxrdDatabaseKey &ob) {
    Copy(ob);
    return *this;
  }

public: // Base class interface
  size_t KeySize() { return sizeof(key); }
  int operator==(const DatabaseKeyB& k) const;
  int operator>(const DatabaseKeyB& k) const;

  // NOTE: This comparison function is only used if the 
  // __USE_SINGLE_COMPARE__ preprocessor directive is 
  // defined when the program is compiled.
  int CompareKey(const DatabaseKeyB& k) const;

public:
  gxrdKeyType &Key() { return key.k; }
  gxrdKeyID KeyID() const { return key.kid; }
  gxrdTableID TableID() const { return key.tid; }
  void SetKey(const gxrdKeyType_t &k) { key.k = k; }
  void SetKey(gxrdKeyType_t &k) { key.k = k; }
  void SetKeyID(gxrdKeyID_t kid) { key.kid = kid; }
  void SetTableID(gxrdTableID_t tid) { key.tid = tid; }
  void AllowDuplicates() { allow_dup_names = 1; }
  void DisallowDuplicates() { allow_dup_names = 0; }
  void Copy(const gxrdDatabaseKey &ob);
  __ULWORD__ SizeOf() { return key.SizeOf(); }
  void Clear() { key.Clear(); }

private: // Non-persistent data members
  int allow_dup_names;

private: // Persistent data members
  gxrdDatabaseKey_t key;
};
// --------------------------------------------------------------

// --------------------------------------------------------------
// Standalone functions
// --------------------------------------------------------------
GXDLCODE_API const char *gxrdFieldTypeString(gxrdFIELD_TYPES t);
// --------------------------------------------------------------

#endif // __GX_RDBMS_HEADERS_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //




