// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxrdbhdr.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 08/09/2001
// Date Last Modified: 05/03/2016
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

Type definitions, constants, and data structures used the 
DataReel relational database management system.

Changes:
==============================================================
10/01/2003: Modified the gxrdDatabaseRecord::Copy() function to 
copy the gxrdDatabaseRecord::exclude_fields public data member.

10/01/2003: Modified the gxrdDatabaseRecord::CreateDiskRecord() 
and gxrdDatabaseRecord::LoadDiskRecord() functions to check for 
exclude field that will not be copied to or from the disk record.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "gxrdbhdr.h"

#ifdef __BCC32__
#pragma warn -8071
#pragma warn -8080
#endif

// Must match the gxrdFIELD_TYPES enumeration
const int gxrdNUM_FIELD_TYPES = 12;
const char *gxrdFIELD_TYPES_STRINGS[gxrdNUM_FIELD_TYPES] = {
  "Invalid type", // gxrdINVALID_TYPE
  "CHAR",         // gxrdCHAR 
  "INT16",        // gxrdINT16 
  "INT32",        // gxrdINT32
  "INT64",        // gxrdINT64
  "FLOAT",        // gxrdFLOAT
  "BOOL",         // gxrdBOOL
  "VCHAR",        // gxrdVCHAR
  "BINARY",       // gxrdBINARY
  "BLOB",         // gxrdBLOB
  "CURRENCY",     // gxrdCURRENCY
  "DATE"          // gxrdDATE
};

gxRDBMSHeader::gxRDBMSHeader() 
{
  const char *gxrdbms_id_string = "GXRDBMS";
  const char gxrdbms_rev_letter = '\0';
  file_version = (gxUINT32)1027;
  memmove(file_id, gxrdbms_id_string, (gxrdFILE_ID_STR_LEN-1));
  file_id[gxrdFILE_ID_STR_LEN-1] = gxrdbms_rev_letter;
}

int gxRDBMSHeader::TestFileID(const gxRDBMSHeader &ob)
{
  if(memcmp(file_id, ob.file_id, (gxrdFILE_ID_STR_LEN-1)) != 0) return 0;
  return 1;
}

int gxRDBMSHeader::TestVersion(const gxRDBMSHeader &ob)
{
  if(file_version != ob.file_version) return 0;
  return 1;
}

int gxRDBMSHeader::TestRevLetter(const gxRDBMSHeader &ob)
{
  if(file_id[gxrdFILE_ID_STR_LEN-1] != ob.file_id[gxrdFILE_ID_STR_LEN-1]) {
    return 0;
  }
  return 1;
}

int gxRDBMSHeader::TestHeader(const gxRDBMSHeader &ob)
{
  if(memcmp(file_id, ob.file_id, gxrdFILE_ID_STR_LEN) != 0) return 0;
  if(file_version != ob.file_version) return 0;
  return 1;
}

void gxRDBMSHeader::Copy(const gxRDBMSHeader &ob)
{
  if(this == &ob) return; // Prevent self assignment
  memmove(file_id, ob.file_id, gxrdFILE_ID_STR_LEN);
  file_version = ob.file_version;
}

void gxRDBMSHeader::Clear()
{
  memset(file_id, 0, gxrdFILE_ID_STR_LEN);
  file_version = 0;
}

void gxrdRecordHeader::Copy(const gxrdRecordHeader &ob) 
{
  if(this == &ob) return; // Prevent self assignment
  table_id = ob.table_id;
  num_fields = ob.num_fields;
}

void gxrdRecordHeader::SetTableID(const gxString &table_name) 
{
  if(table_name.is_null()) table_id = (gxUINT32)0;
  table_id = calcCRC32(table_name.c_str(), table_name.length());
}

gxUINT32 gxrdRecordHeader::GetCRC(const gxString &table_name) 
{
  if(table_name.is_null()) return (gxUINT32)0;
  return calcCRC32(table_name.c_str(), table_name.length());
}

void gxrdTableHeaderField::Clear() 
{
  field_type_id = 0;
  format_specifier = 0;
  column_length = (gxUINT32)0;
  array_length = (gxUINT32)0;
  memset(field_name, 0, tableFIELD_NAME_LEN);
}

void gxrdTableHeaderField::Copy(const gxrdTableHeaderField &ob)
{
  if(this == &ob) return; // Prevent self assignment
  field_type_id = ob.field_type_id;
  format_specifier = ob.format_specifier;
  column_length = ob.column_length;
  array_length = ob.array_length;
  memmove(field_name, ob.field_name, tableFIELD_NAME_LEN);
}

__ULWORD__ gxrdTableHeaderField::SizeOf()
{
  __ULWORD__ size = (sizeof(field_type_id)+sizeof(format_specifier));
  size += sizeof(column_length);
  size += sizeof(array_length);
  size += sizeof(field_name);
  return size;
}

int gxrdTableHeaderField::VerifyFieldType() 
{
  int type = (int)field_type_id;

  // NOTE: All relational field types must be account for in
  // this function.
  switch(type) {
    case gxrdCHAR : case gxrdVCHAR : case gxrdBOOL :
      break;
    case gxrdINT16 :
      break;
    case gxrdINT32 :
      break;
    case gxrdINT64 :
      break;
    case gxrdFLOAT : case gxrdCURRENCY :
      break;
    case gxrdBINARY : case gxrdBLOB :
      break;
    case gxrdDATE :
      break;
    default :
      return 0;
  }
  return 1;
}

int gxrdTableHeaderField::TestFieldType(const gxrdTableHeaderField &ob,
					int strict_test) 
{
  if(strict_test) {
    return field_type_id == ob.field_type_id;
  }
  if(field_type_id != ob.field_type_id) {
    int type1 = (int)field_type_id;
    int type2 = (int)ob.field_type_id;

    // NOTE: All relational field types must be account for in
    // this function.
    switch(type1) {
      case gxrdBLOB :
	if(type2 != gxrdBLOB) return 0; // Cannot cast a BLOB 
	break;
      case gxrdBOOL :
	if(type2 == gxrdBLOB) return 0; 
	break; // All other types can be cast to a bool type
      case gxrdCHAR : case gxrdVCHAR : 
	if((type2 == gxrdCHAR) || (type2 == gxrdVCHAR)) {
	  break; // Casting one char type to another
	}
	return 0;
      case gxrdINT16 :
	if(type2 != gxrdINT16) return 0;
	break;
      case gxrdINT32 :
	if((type2 == gxrdINT16) || (type2 == gxrdINT32)) {
	  break; // Allow INT16 types to be promoted
	}
	return 0;
      case gxrdINT64 : 
	if((type2 == gxrdINT16) || (type2 == gxrdINT32) || 
	   (type2 == gxrdINT64)) {
	  break; // Allow INT16 and INT32 types to be promoted
	}
	return 0;
      case gxrdFLOAT : case gxrdCURRENCY :
	if((type2 == gxrdFLOAT) || (type2 == gxrdCURRENCY)) {
	  break; // Casting one float type to another
	}
	return 0;
      case gxrdBINARY : 
	if(type2 != gxrdBINARY) return 0; // Cannot cast user-defined type
	break;
      case gxrdDATE :
	if(type2 != gxrdDATE) return 0; // Cannot cast date type
	break;
      default :
	return 0;
    }
  }
  return 1;
}

int gxrdTableHeaderField::TestFieldVars(const gxrdTableHeaderField &ob)
{
  if(format_specifier != ob.format_specifier) return 0;
  if(array_length != ob.array_length) return 0;
  if(column_length != ob.column_length) return 0;
  
  gxString sbuf1, sbuf2;
  if(!test_for_null(field_name, tableFIELD_NAME_LEN)) {
    sbuf1.InsertAt(0, field_name, tableFIELD_NAME_LEN);
  }
  else {
    sbuf1 << field_name;
  }
  if(!test_for_null(ob.field_name, tableFIELD_NAME_LEN)) {
    sbuf2.InsertAt(0, ob.field_name, tableFIELD_NAME_LEN);
  }
  else {
    sbuf2 << ob.field_name;
  }
  return sbuf1 == sbuf2;
}

gxrdTableHeader::gxrdTableHeader() 
{
  fields = 0;
  num_fields = (gxUINT32)0;
  Clear();
}

gxrdTableHeader::gxrdTableHeader(const gxrdTableHeader &ob)
{
  fields = 0;
  num_fields = (gxUINT32)0;
  Clear();
  Copy(ob); 
}

gxString gxrdTableHeader::TableDef() const
// Function used to return a copy of the table definition
// store in the table header.
{
  gxString def;
  __ULWORD__ i, type;
  if((!fields) || (!num_fields)) {
    // This header has not been initialized
    return def;
  }
  def << table_name << ",";
  for(i = 0; i < num_fields; i++) {
    type = (int)fields[i].field_type_id;
    def << fields[i].field_name << "{" << fields[i].column_length << "}" 
	<< gxrdFieldTypeString((gxrdFIELD_TYPES)type);
    if(fields[i].array_length > (gxUINT32)0) {
      def << "[" << fields[i].array_length << "]";
    }
    if(fields[i].format_specifier != 0) {
      if(isalpha(fields[i].format_specifier)) {
	def << "." << fields[i].format_specifier;
      }
      else { 
	def << "." << (int)fields[i].format_specifier;
      }
    }
    if(i < (num_fields-1)) def << ",";
  }
  return def;
}

void gxrdTableHeader::SetTableName(const char *s)
{
  strncpy(table_name, s, tableNAME_LEN);
  table_name[tableNAME_LEN-1] = 0;
}

void gxrdTableHeader::SetKeyDef(const char *s)
{
  strncpy(key_def, s, tableDEF_LEN);
  key_def[tableDEF_LEN-1] = 0;
}

void gxrdTableHeader::SetFormDef(const char *s)
{
  strncpy(form_def, s, tableDEF_LEN);
  form_def[tableDEF_LEN-1] = 0;
}

void gxrdTableHeader::SetFKeyDef(const char *s)
{
  strncpy(fkey_def, s, tableDEF_LEN);
  fkey_def[tableDEF_LEN-1] = 0;
}

gxrdTableHeader::gxrdTableHeader(__ULWORD__ nfields) 
{
  fields = 0;
  num_fields = (gxUINT32)0;
  Clear();
  AllocHeader(nfields);
}

void gxrdTableHeader::Copy(const gxrdTableHeader &ob)
{
  if(this == &ob) return; // Prevent self assignment
  memmove(table_name, ob.table_name, tableNAME_LEN);
  memmove(key_def, ob.key_def, tableDEF_LEN);  
  memmove(form_def, ob.form_def, tableDEF_LEN);  
  memmove(fkey_def, ob.fkey_def, tableDEF_LEN);
  // 12/19/2005: Corrected statement with no effect
  memmove(app_space, ob.app_space, tableAPPLICATION_SPACE);
  DestroyHeader();
  num_fields = ob.num_fields;
  AllocHeader(num_fields);
  if(!fields) {
    num_fields = (gxUINT32)0;
    return;
  }
  for(__ULWORD__ i = 0; i < num_fields; i++) fields[i] = ob.fields[i];
}

int gxrdTableHeader::VerifyFieldTypes()
{
  if(num_fields == (__ULWORD__)0) return 0;
  if(!fields) return 0; // Object is not initialized

  for(__ULWORD__ i = 0; i < num_fields; i++) {
    if(!fields[i].VerifyFieldType()) return 0;
  }
  return 1;
}

int gxrdTableHeader::TestFieldTypes(const gxrdTableHeader &ob)
{
  if(num_fields != ob.num_fields) return 0;
  if(fields) {
    if(!ob.fields) return 0; // Object is not initialized
    for(__ULWORD__ i = 0; i < num_fields; i++) {
      if(!fields[i].TestFieldType(ob.fields[i])) return 0;
    }
  }
  if(ob.fields) { 
    if(!fields) return 0; // Object is not initialized
  }
  return 1;
}

int gxrdTableHeader::TestFieldVars(const gxrdTableHeader &ob) 
{
  if(num_fields != ob.num_fields) return 0;
  if(fields) {
    if(!ob.fields) return 0; // Object is not initialized
    for(__ULWORD__ i = 0; i < num_fields; i++) {
      if(!fields[i].TestFieldVars(ob.fields[i])) return 0;
    }
  }
  if(ob.fields) { 
    if(!fields) return 0; // Object is not initialized
  }
  return 1;
}

int gxrdTableHeader::TestTableName(const gxrdTableHeader &ob)
{
  gxString sbuf1, sbuf2;
  if(!test_for_null(table_name, tableNAME_LEN)) {
    sbuf1.InsertAt(0, table_name, tableNAME_LEN);
  }
  else {
    sbuf1 << table_name;
  }
  if(!test_for_null(ob.table_name, tableNAME_LEN)) {
    sbuf2.InsertAt(0, ob.table_name, tableNAME_LEN);
  }
  else {
    sbuf2 << ob.table_name;
  }
  return sbuf1 == sbuf2;
}

int gxrdTableHeader::TestKeyDef(const gxrdTableHeader &ob)
{
  gxString sbuf1, sbuf2;
  if(!test_for_null(key_def, tableDEF_LEN)) {
    sbuf1.InsertAt(0, key_def, tableDEF_LEN);
  }
  else {
    sbuf1 << key_def;
  }
  if(!test_for_null(ob.key_def, tableDEF_LEN)) {
    sbuf2.InsertAt(0, ob.key_def, tableDEF_LEN);
  }
  else {
    sbuf2 << ob.key_def;
  }
  return sbuf1 == sbuf2;
}

int gxrdTableHeader::TestFormDef(const gxrdTableHeader &ob)
{
  gxString sbuf1, sbuf2;
  if(!test_for_null(form_def, tableDEF_LEN)) {
    sbuf1.InsertAt(0, form_def, tableDEF_LEN);
  }
  else {
    sbuf1 << form_def;
  }
  if(!test_for_null(ob.form_def, tableDEF_LEN)) {
    sbuf2.InsertAt(0, ob.form_def, tableDEF_LEN);
  }
  else {
    sbuf2 << ob.form_def;
  }
  return sbuf1 == sbuf2;
}

int gxrdTableHeader::TestFKeyDef(const gxrdTableHeader &ob)
{
  gxString sbuf1, sbuf2;
  if(!test_for_null(fkey_def, tableDEF_LEN)) {
    sbuf1.InsertAt(0, fkey_def, tableDEF_LEN);
  }
  else {
    sbuf1 << fkey_def;
  }
  if(!test_for_null(ob.fkey_def, tableDEF_LEN)) {
    sbuf2.InsertAt(0, ob.fkey_def, tableDEF_LEN);
  }
  else {
    sbuf2 << ob.fkey_def;
  }
  return sbuf1 == sbuf2;
}

void gxrdTableHeader::Clear()
{
  memset(table_name, 0, tableNAME_LEN);
  memset(key_def, 0, tableDEF_LEN);
  memset(form_def, 0, tableDEF_LEN);
  memset(fkey_def, 0, tableDEF_LEN);
  memset(app_space, 0, tableAPPLICATION_SPACE);
  for(__ULWORD__ i = 0; i < num_fields; i++) {
    if(!fields) {
      num_fields = (gxUINT32)0;
      break;
    }
    fields[i].Clear();
  }
}

int gxrdTableHeader::AllocHeader(__ULWORD__ nfields) 
{
  DestroyHeader();

  // Test the number of fields before allocating memory
  if(nfields <= (gxUINT32)0) nfields = tableMIN_FIELDS;
  if(nfields > (__ULWORD__)tableMAX_FIELDS) nfields = tableMAX_FIELDS;

  num_fields = (gxUINT32)nfields;
  fields = new gxrdTableHeaderField[(int)num_fields];
  if(!fields) return 0;
  memset(app_space, 0, tableAPPLICATION_SPACE);
  return 1;
}

void gxrdTableHeader::DestroyHeader() 
{
  if(fields) delete[] fields;
  fields = 0;
  num_fields = (gxUINT32)0;
}

int gxrdTableHeader::GetField(gxrdTableHeaderField *fh, __ULWORD__ nfield) 
{
  if(!fields) return 0;
  if(nfield > (num_fields-1)) return 0;
  fh->field_type_id = fields[nfield].field_type_id;
  fh->format_specifier = fields[nfield].format_specifier;
  fh->column_length = fields[nfield].column_length;
  fh->array_length = fields[nfield].array_length;
  strncpy(fh->field_name, fields[nfield].field_name, tableFIELD_NAME_LEN);
  fh->field_name[tableFIELD_NAME_LEN-1] = 0;
  return 1;
}

int gxrdTableHeader::SetField(char field_type_id, char format_specifier, 
			      char *field_name, gxUINT32 column_length, 
			      gxUINT32 array_length, __ULWORD__ nfield) 
{
  if(!fields) return 0;
  if(nfield > (num_fields-1)) return 0;
  fields[nfield].Clear();
  fields[nfield].field_type_id = field_type_id;
  fields[nfield].format_specifier = format_specifier;
  fields[nfield].column_length = column_length;
  fields[nfield].array_length = array_length;
  strncpy(fields[nfield].field_name, field_name, tableFIELD_NAME_LEN);
  fields[nfield].field_name[tableFIELD_NAME_LEN-1] = 0;
  return 1;
}

__ULWORD__ gxrdTableHeader::SizeOf() 
{
  gxrdTableHeaderField thf;
  __ULWORD__ size = sizeof(table_name);
  size += sizeof(key_def);
  size += sizeof(form_def);
  size += sizeof(fkey_def);
  size += sizeof(num_fields);
  size += (num_fields * thf.SizeOf());
  size += sizeof(app_space);
  return size;
}

void gxrdDatabaseTable::Copy(const gxrdDatabaseTable &ob)
{
  if(this == &ob) return; // Prevent self assignment
  table_name = ob.table_name;
  record_def = ob.record_def;
  table_def = ob.table_def;
  table_header = ob.table_header;
}

__ULWORD__ gxrdDatabaseTable::SizeOf()
{
  __ULWORD__ size;
  size = table_name.length();
  size += record_def.length();
  size += table_def.length();
  size += table_header.SizeOf();
  return size;
}

void gxrdDatabaseTable::Clear()
{
  table_name.Clear();
  record_def.Clear();
  table_def.Clear();
  table_header.Clear();
}

gxDatabaseError gxrdDatabaseTable::CreateTable(const gxString &tdef)
{
  // Parse the table definition
  char words[MAXWORDS][MAXWORDLENGTH];
  int num_words, parse_err;
  parse_err = parse(tdef.c_str(), words, &num_words, ',');
  if(parse_err != 0) {
    return gxDBASE_BAD_TABLE_DEF;
  }
 if(num_words < 2) {
   return gxDBASE_BAD_TABLE_DEF;
  }
  
 // Set the table name
 if(*words[0]) { 
   table_name = words[0];
 }
 else {
   // Bad table name
   return gxDBASE_BAD_TABLE_DEF;
 }
 
 // Test table name for illegal file name characters
 int i;
 table_name.ConvertEscapeSeq('%');
 table_name.FilterChar('\t');
 table_name.TrimLeadingSpaces();
 table_name.TrimTrailingSpaces();
 for(i = 0; i < (int)table_name.length(); i++) {
   // Include both UNIX and WIN32 platforms
   if(table_name[i] == '/') return gxDBASE_ILLEGAL_CHAR;
   if(table_name[i] == '\\') return gxDBASE_ILLEGAL_CHAR;
   if(table_name[i] == '*') return gxDBASE_ILLEGAL_CHAR;
   if(table_name[i] == '|') return gxDBASE_ILLEGAL_CHAR;
   if(table_name[i] == ':') return gxDBASE_ILLEGAL_CHAR;
   if(table_name[i] == '\"') return gxDBASE_ILLEGAL_CHAR;
   if(table_name[i] == '<') return gxDBASE_ILLEGAL_CHAR;
   if(table_name[i] == '>') return gxDBASE_ILLEGAL_CHAR;
   if(table_name[i] == '?') return gxDBASE_ILLEGAL_CHAR;
 }
 
 // Initialize the persistent table header
 table_header.SetTableName(table_name.c_str());
 int num_fields = num_words - 1;
 table_header.AllocHeader(num_fields);
 
 gxString sbuf, intbuf, array_len, format_attrib;
 char field_type_id;
 char format_specifier;
 gxString field_type;
 gxString field_name;
 gxUINT32 column_length;
 __ULWORD__ alen;
 int j, found_field_type;
 record_def.Clear();
 record_def << table_name << ",";
 
 for(i = 1; i < num_words; i++) {
   if(*words[i]) {
     sbuf = words[i];
     intbuf.Clear();
     array_len.Clear();
     alen = (__ULWORD__)0;
     format_attrib.Clear();
     found_field_type = 0;
     format_specifier = 0;
     sbuf.TrimLeadingSpaces();
     sbuf.TrimTrailingSpaces();
     field_name = sbuf;
     if((field_name.Find("{") == -1) || (field_name.Find("}") == -1)) {
       return gxDBASE_BAD_COL_LENGTH;
     }
     field_name.DeleteAfterLast("}");
     intbuf = field_name;
     intbuf.DeleteBeforeIncluding("{");
     intbuf.DeleteAfterIncluding("}");
     intbuf.TrimLeadingSpaces();
     intbuf.TrimTrailingSpaces();
     field_name.DeleteAfterIncluding("{");
     field_name.TrimLeadingSpaces();
     field_name.TrimTrailingSpaces();
     
     column_length = (gxUINT32)intbuf.Atoi();
     // 07/13/2003: NOTE: A 0 column length is used to tell the GUI
     // to hide this field from input and display functions
     field_type = sbuf;
     field_type.DeleteBeforeIncluding("}");
     
     if(field_type.Find(".") != -1) {
       format_attrib = field_type;
       format_attrib.DeleteBeforeIncluding(".");
       format_attrib.TrimLeadingSpaces();
       format_attrib.TrimTrailingSpaces();
       if(!format_attrib.is_null()) {
	 format_specifier = format_attrib.Atoi();
	 if(format_specifier == 0) {
	   // This is not an integer modifier
	   format_specifier = format_attrib[0];
	 }
       }
       field_type.DeleteAfterIncluding(".");
     }
     field_type.TrimLeadingSpaces();
     field_type.TrimTrailingSpaces();
     
     if(field_type.Find("[") != -1) {
       array_len = field_type;
       array_len.DeleteBeforeIncluding("[");
       array_len.DeleteAfterIncluding("]");
       array_len.TrimLeadingSpaces();
       array_len.TrimTrailingSpaces();
       field_type.DeleteAfterIncluding("[");
       field_type.TrimTrailingSpaces();
     }
     for(j = 0; j < gxrdNUM_FIELD_TYPES; j++) {
       if(field_type == gxrdFIELD_TYPES_STRINGS[j]) {
	 found_field_type = 1;
	 break;
       }
     }
     if(!found_field_type) {
       return gxDBASE_BAD_FIELD_TYPE;
     }
     
     // Set the field type ID
     field_type_id = (char)j;
     
     // Set the record definition for this table
     record_def << field_type;
     if(!array_len.is_null()) {
       alen = array_len.Atoi();
       if(alen > 0) {
	 record_def << "[" << alen << "]";
       }
     }
     if(i < (num_words-1)) record_def << ",";
     
     // Set the table header field
     table_header.SetField(field_type_id, format_specifier, 
			   field_name.c_str(), column_length, alen, (i-1));
     
   }
   else {
     return gxDBASE_BAD_FIELD_TYPE;
   }
 }
 
 // Set the table definition after all fields have been verified
 table_def.Clear();
 table_def << tdef;
 
 return gxDBASE_NO_ERROR;
}

void gxrdDatabaseKey_t::Copy(const gxrdDatabaseKey_t &ob)
{
  if(this == &ob) return; // Prevent self assignment
  k = ob.k;
  tid = ob.tid;
  kid = ob.kid;
}

int gxrdDatabaseKey::operator==(const DatabaseKeyB& k) const
{
  const gxrdDatabaseKey *kptr = (const gxrdDatabaseKey *)(&k);
  // NOTE: Duplicate names will not be allowed if the object ID is ignored
  if(!allow_dup_names) {  
    return key.k == kptr->key.k;
  }
  else { // Allow duplicate names by comparing the object ID  
    return ((key.k == kptr->key.k) && \
	    (key.kid == kptr->key.kid));
  }
}

int gxrdDatabaseKey::operator>(const DatabaseKeyB& k) const
{
  const gxrdDatabaseKey *kptr = (const gxrdDatabaseKey *)(&k);
  // NOTE: Duplicate names will not be allowed if the object ID is ignored
  if(!allow_dup_names) { 
    return key.k > kptr->key.k;
  }
  else { // Allow duplicate names by comparing the object ID  
    return ((key.k > kptr->key.k) && \
	    (key.kid > kptr->key.kid));
  }
}

int gxrdDatabaseKey::CompareKey(const DatabaseKeyB& k) const
{
  const gxrdDatabaseKey *kptr = (const gxrdDatabaseKey *)(&k);
  int rv = -1;
  if(key.k > kptr->key.k) rv = 1;
  if(key.k == kptr->key.k) rv = 0;  

  // NOTE: Duplicate names will not be allowed if the object ID is ignored
  if(!allow_dup_names) { 
    return rv;
  }
  else { // Allow duplicate names by comparing the object ID  
    if(rv == 0) {
      if(key.kid > kptr->key.kid) return 1;
      if(key.kid < kptr->key.kid) return -1;
    }
  }
  return rv;
}

void gxrdDatabaseKey::Copy(const gxrdDatabaseKey &ob)
{
  if(this == &ob) return; // Prevent self assignment
  key = ob.key;
  allow_dup_names = ob.allow_dup_names;
}

__ULWORD__ gxrdDatabaseRecord::SizeOf() const
// Public member function that returns the total number of bytes
// required to write this record to disk. The actual number of
// bytes allocated for this record in memory may be larger for
// variable length records that grow and shrink in size.
{ 
  if((!db_record) || (!num_fields)) return (__ULWORD__)0;

  __ULWORD__ total_size = 0;
  __ULWORD__ i;
  __ULWORD__ field_type_size = sizeof(char);
  gxUINT32 num_bytes;

  for(i = 0; i < num_fields; i++) {
    total_size += (sizeof(num_bytes)+field_type_size);
    memmove(&num_bytes, (db_record[i].m_buf()+field_type_size), 
	    sizeof(num_bytes));
    total_size += num_bytes;
  }
  return total_size;
}  

__ULWORD__ gxrdDatabaseRecord::SizeOfBuffer() const 
{ 
  if((!db_record) || (!num_fields)) return (__ULWORD__)0;

  __ULWORD__ total_size = 0;
  __ULWORD__ i;

  for(i = 0; i < num_fields; i++) {
    total_size += db_record[i].length();
  }
  return total_size;
}  

__ULWORD__ gxrdDatabaseRecord::SizeOfField(__ULWORD__ col_number) const
{
  if(!db_record) return (__ULWORD__)0;
  if(col_number > (num_fields-1)) return (__ULWORD__)0;

  __ULWORD__ total_size = 0;
  __ULWORD__ field_type_size = sizeof(char);
  gxUINT32 num_bytes;

  total_size += (sizeof(num_bytes)+field_type_size);
  memmove(&num_bytes, (db_record[col_number].m_buf()+field_type_size), 
	  sizeof(num_bytes));
  return (total_size + num_bytes);
}
 
void gxrdDatabaseRecord::Clear() 
{
  table_name.Clear();
  record_def.Clear();

  // Default comparison settings 
  compare_field = -1; // Compare all fields
  compare_case = 1;   // Compare string case
  compare_alpha = 0;  // Do not compare alphanumerics

  // NOTE: Do not clear the number of field to allow
  // memory already allocated for this record to be
  // reused if possible.
}

__ULWORD__ gxrdDatabaseRecord::FieldLength(__ULWORD__ col_number)
{
  char field_type_id;
  gxUINT32 num_bytes;
  FieldData(col_number, field_type_id, num_bytes);
  return (__ULWORD__)num_bytes;
}

int gxrdDatabaseRecord::FieldType(__ULWORD__ col_number)
{
  char field_type_id;
  gxUINT32 num_bytes;
  FieldData(col_number, field_type_id, num_bytes);
  return (int)field_type_id;
}

int gxrdDatabaseRecord::FieldHasNull(__ULWORD__ col_number)
{
  __ULWORD__ i;
  __ULWORD__ npos = 0;
  char field_type_id;
  gxUINT32 num_bytes;
  char *p = (char *)FieldData(col_number, field_type_id, num_bytes);
  if(!p) return 0;
  for(i = 0; i < num_bytes; i++, p++) {
    if(*p == 0) {
      npos = i;
      if(i == 0) npos++; // The first char is null
      break;
    }
  }
  return npos != 0;
}

void *gxrdDatabaseRecord::FieldData(__ULWORD__ col_number, char &field_type_id,
				    gxUINT32 &num_bytes)
{
  if(!db_record) return (void *)0;
  if(col_number > (num_fields-1)) return (void *)0;
  __ULWORD__ offset = 0;
  memmove(&field_type_id, (db_record[col_number].m_buf()+offset), 
	  sizeof(field_type_id));
  offset += sizeof(field_type_id);
  memmove(&num_bytes, (db_record[col_number].m_buf()+offset), 
	  sizeof(num_bytes));
  offset += sizeof(num_bytes);
  return (void *)(db_record[col_number].m_buf()+offset);
}

void *gxrdDatabaseRecord::FieldData(__ULWORD__ col_number)
{
  char field_type_id;
  gxUINT32 num_bytes;
  return FieldData(col_number, field_type_id, num_bytes);
}

gxDatabaseError gxrdDatabaseRecord::FieldDataDiskOffset(__ULWORD__ &offset, 
							__ULWORD__ col_number)
{
  gxDatabaseError err = FieldDiskOffset(offset, col_number);
  if(err != gxDBASE_NO_ERROR) {
    return err;
  }
  offset += 1; // Field type ID
  offset += sizeof(gxUINT32); // Num bytes

  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxrdDatabaseRecord::FieldDiskOffset(__ULWORD__ &offset, 
						    __ULWORD__ col_number)
{
  if(!db_record) return gxDBASE_NULL_PTR;
  if(col_number > (num_fields-1)) return gxDBASE_BUFFER_OVERFLOW;
  gxUINT32 num_bytes;
  offset = 0;

  for(__ULWORD__ i = 0; i < col_number; i++) {
    offset += 1; // Field type ID
    memmove(&num_bytes, (db_record[i].m_buf()+1), 
	    sizeof(num_bytes));
    offset += sizeof(num_bytes);
    offset += num_bytes;
  }

  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxrdDatabaseRecord::GetField(void *data, 
					     __ULWORD__ bytes, 
					     __ULWORD__ col_number) 
{
  if(!db_record) return gxDBASE_NULL_PTR;
  if(col_number > (num_fields-1)) return gxDBASE_BUFFER_OVERFLOW;

  gxUINT32 num_bytes;
  __ULWORD__ offset = 1; // One byte field ID

  memmove(&num_bytes, (db_record[col_number].m_buf()+offset), 
	  sizeof(num_bytes));
  offset += sizeof(num_bytes);

  // Check to ensure that the number of bytes requested is not
  // greater than the number of bytes allocated for this field.
  if(bytes > num_bytes) {
    return gxDBASE_BAD_FIELD_LENGTH;
  }

  // Copy field data to the buffer
  memmove(data, (db_record[col_number].m_buf()+offset), bytes);

  return gxDBASE_NO_ERROR;
}

void *gxrdDatabaseRecord::GetField(__ULWORD__ col_number)
{ 
  __ULWORD__ bytes;
  gxDatabaseError err;
  return GetField(col_number, bytes, err);
}

void *gxrdDatabaseRecord::GetField(__ULWORD__ col_number, __ULWORD__ &bytes)
{ 
  gxDatabaseError err;
  return GetField(col_number, bytes, err);
}

void *gxrdDatabaseRecord::GetField(__ULWORD__ col_number, __ULWORD__ &bytes, 
				   gxDatabaseError &err) 
{
  if(!db_record) {
    err = gxDBASE_NULL_PTR;
    return (void *)0;
  }
  if(col_number > (num_fields-1)) {
    err = gxDBASE_BUFFER_OVERFLOW;
    return (void *)0;
  }

  gxUINT32 num_bytes;
  __ULWORD__ offset = 1; // One byte field ID

  memmove(&num_bytes, (db_record[col_number].m_buf()+offset), 
	  sizeof(num_bytes));
  offset += sizeof(num_bytes);

  // Pass the field length back to the caller
  bytes = num_bytes;

  return (void *)(db_record[col_number].m_buf()+offset);
}

gxDatabaseError gxrdDatabaseRecord::SetField(const void *data, 
					     __ULWORD__ bytes, 
					     __ULWORD__ col_number) 
{
  if(!db_record) return gxDBASE_NULL_PTR;
  if(col_number > (num_fields-1)) return gxDBASE_BUFFER_OVERFLOW;
  gxUINT32 num_bytes, nsize;
  __ULWORD__ offset = 1; // One byte field ID
  memmove(&num_bytes, (db_record[col_number].m_buf()+offset), 
	  sizeof(num_bytes));
  offset += sizeof(num_bytes);

  if(num_bytes != bytes) { // This record must be resized
    nsize = bytes; // Change the byte size;
    // Reset the byte count stored with the record  
    memmove((db_record[col_number].m_buf()+1), &nsize, sizeof(nsize));
    if(num_bytes < bytes) { // Must resize this field
      db_record[col_number].resize(bytes+(sizeof(num_bytes)+1));
    }
  }

  memmove((db_record[col_number].m_buf()+offset), data, bytes);
  return gxDBASE_NO_ERROR;
}

void gxrdDatabaseRecord::GetRecordTypeSize(gxrdFIELD_TYPES type, 
					   __ULWORD__ &size,
					   __ULWORD__ &overhead) 
{
  // Set the overhead for every record type
  overhead = sizeof(char); // Type ID
  overhead += sizeof(gxUINT32); // Number of bytes allocated for type

  // NOTE: All relational field types must be account for in
  // this function.
  switch(type) {
    case gxrdBOOL : case gxrdCHAR : case gxrdVCHAR : case gxrdBINARY : 
    case gxrdBLOB :  
      size = 1; // Single byte types
      break;
    case gxrdINT16 :
      size = sizeof(gxINT16);
      break;
    case gxrdINT32 :
      size = sizeof(gxINT32);
      break;
    case gxrdINT64 :
      size = 8;
      break;
    case gxrdFLOAT :
      size = sizeof(gxFLOAT64);
      break;
    case gxrdCURRENCY :
      size = sizeof(gxFLOAT64);
      break;
    case gxrdDATE :
      size = sizeof(CDate);
      break;
    default:
      size = 1; // Allocate a single byte for unknown types
      break;
  }
}

int gxrdDatabaseRecord::AllocArray(__ULWORD__ nfields)
// Allocate memory for the record array. The length of the
// member array will be equal to the number of array elements
// multiplied by the size of a MemoryBuffer object. NOTE: This
// function will not allocate memory for the array if the 
// current object array is large enough to hold the number
// of elements requested. Returns true if allocation is
// successful or false if a memory allocation error occurs.
{
  // Check the object array boundaries
  if(nfields <= 0) nfields = recordMIN_FIELDS;
  if(nfields > (__ULWORD__)recordMAX_FIELDS) nfields = recordMAX_FIELDS;

  if(db_record) { // Memory has been allocated for the array
    if(num_fields >= nfields) { // Do not re-allocate
      num_fields = nfields; 
      return 1;
    }
    else {
      DeleteArray(); // Free existing memory in use
    }
  }

  MemoryBuffer *buf = new MemoryBuffer[nfields];
  if(!buf) { 
    num_fields = 0;
    db_record = 0;
    return 0; // An memory allocation error occurred
  }

  db_record = buf;
  num_fields = nfields; // Set the length of the array
  return 1;
}

void gxrdDatabaseRecord::DeleteArray() 
// Free the memory used for the record array and reset the
// array variables.
{
  if(db_record) delete[] db_record;
  db_record = 0;
  num_fields = 0;
}

gxDatabaseError gxrdDatabaseRecord::InitRecord(const gxString &def)
{
  char words[MAXWORDS][MAXWORDLENGTH];
  int num_words, parse_err;
  parse_err = parse(def.c_str(), words, &num_words, ',');
  if(parse_err != 0) {
    return gxDBASE_BAD_RECORD_DEF;
  }
  gxString sbuf;
  gxString array_len;
  gxString field_type;
  int i, j;
  int found_field_type;
  __ULWORD__ size = 0;
  __ULWORD__ overhead = 0;
  __ULWORD__ offset = 0;

  __ULWORD__ field_len = 0;

  gxUINT32 num_bytes;
  char field_type_id;

  for(i = 1; i < num_words; i++) {
    if(*words[i]) {
      array_len.Clear(); // Clear the attribute string
      field_len = 0;     // Reset the field size
      offset = 0;        // Reset the offset
      found_field_type = 0;      
      sbuf = words[i];
      sbuf.TrimLeadingSpaces();
      sbuf.TrimTrailingSpaces();
      field_type = sbuf;
      if(field_type.Find("[") != -1) {
	array_len = sbuf;
	array_len.DeleteBeforeIncluding("[");
	array_len.DeleteAfterIncluding("]");
	array_len.TrimLeadingSpaces();
	array_len.TrimTrailingSpaces();
	field_type.DeleteAfterIncluding("[");
	field_type.TrimTrailingSpaces();
      }
      
      for(j = 0; j < gxrdNUM_FIELD_TYPES; j++) {
	if(field_type == gxrdFIELD_TYPES_STRINGS[j]) {
	  found_field_type = 1;
	  break;
	}
      }
      if(!found_field_type) {
	Clear();
	return gxDBASE_BAD_FIELD_TYPE;
      }

      gxrdFIELD_TYPES type = (gxrdFIELD_TYPES)j;
      field_type_id = (char)j;
      GetRecordTypeSize(type, size, overhead);

      // Set the number of bytes allocated for this data type
      if(!array_len.is_null()) {
	num_bytes = (size * array_len.Atoi());
	if(num_bytes < size) num_bytes = size;
      }
      else {
	num_bytes = size;
      }
      field_len = num_bytes + overhead;

      // Setup the memory buffer and set the type ID and data length
      if(!db_record) return gxDBASE_NULL_PTR;
      db_record[(i-1)].resize(field_len, 0);
      db_record[(i-1)].MemSet(0, 0, field_len);
      offset = 0;
      memmove((db_record[(i-1)].m_buf()+offset), &field_type_id, 
	       sizeof(field_type_id));
      offset += sizeof(field_type_id);
      memmove((db_record[(i-1)].m_buf()+offset), &num_bytes, 
	      sizeof(num_bytes));
    }
    else {
      return gxDBASE_BAD_FIELD_TYPE;
    }
  }

  return gxDBASE_NO_ERROR;
}

void gxrdDatabaseRecord::Copy(const gxrdDatabaseRecord &ob)
{
  if(this == &ob) return; // Prevent self assignment

  table_name = ob.table_name;
  record_def = ob.record_def;
  num_fields = ob.num_fields;
  compare_field = ob.compare_field;
  compare_alpha = ob.compare_alpha;

  // The object does not have any record space
  if(!ob.db_record) {
    DeleteArray(); // Free any memory this record is holding
    return;
  }
  
  // Alloc space for record field array
  AllocArray(num_fields);
  
  if(!db_record) return;

  // Copy the record field array
  for(__ULWORD__ i = 0; i < num_fields; i++) {
    db_record[i] = ob.db_record[i];
  }

  // Copy the exclude fields
  exclude_fields = ob.exclude_fields;
}

gxrdKeyType_t gxrdDatabaseRecord::CreateKey(__ULWORD__ field, 
					    int case_cmp) const
{
  if(!db_record) return (gxrdKeyType_t)0;

  int compare_all = 0;
  if(field == (__ULWORD__)-1) compare_all = 1;
  if(field > num_fields) compare_all = 1;

  char field_type_id;
  gxUINT32 num_bytes;
  __ULWORD__ offset;
  __ULWORD__ i, j, npos;
  char *p;
  int type;

  // Type buffers
  gxString sbuf;
  MemoryBuffer bin_buf;
  gxINT16 i16buf;
  gxINT32 i32buf;
  gxFLOAT64 f64buf;
  CDate date_buf;
  char bool_buf;

#if defined (__64_BIT_DATABASE_ENGINE__)
  gxINT64 i64buf;
#else
  gxINT32 low_word, high_word;
#endif

  if(compare_all) {
    i = 0;
  }
  else {
    i = field;
  }
  __ULWORD__ total_size = this->SizeOf();
  MemoryBuffer buf(total_size);
  buf.MemSet(0, 0, total_size);
  __ULWORD__ buf_size = 0;
  __ULWORD__ buf_offset = 0;

  for(; i < num_fields; i++) {
    offset = 0;
    npos = 0;
    memmove(&field_type_id, (db_record[i].m_buf()+offset), 
	    sizeof(field_type_id));
    offset += sizeof(field_type_id);
    memmove(&num_bytes, (db_record[i].m_buf()+offset), 
	    sizeof(num_bytes));
    offset += sizeof(num_bytes);
    type = (int)field_type_id;

    // Move to the field type
    p = (char *)(db_record[i].m_buf()+offset);

    // NOTE: All relational field types must be account for in
    // this function.
    switch(type) {
      case gxrdBOOL :
	if(num_bytes < 1) return (gxrdKeyType_t)0;
	buf_size += 1;
	if(buf.length() < buf_size) buf.resize(buf_size);
	memmove(&bool_buf, p, 1);
	memmove((buf.m_buf()+buf_offset), &bool_buf, 1);
	buf_offset += 1;
	break;

      case gxrdCHAR : case gxrdVCHAR :
	if(num_bytes < sizeof(char)) return (gxrdKeyType_t)0;
	sbuf.Clear();
	for(j = 0; j < num_bytes; j++, p++) {
	  if(*p == 0) {
	    npos = j;
	    if(j == 0) npos++; // The first char is null
	    break;
	  }
	}
	if(npos == 0) { // This is not a null terminated string
	  sbuf.InsertAt(0, (char *)(db_record[i].m_buf()+offset), 
			num_bytes);
	}
	else { // This is null terminated string
	  sbuf = (char *)(db_record[i].m_buf()+offset);
	}
	// Do not compare string case
	if(!case_cmp) sbuf.ToUpper();
	buf_size += sbuf.length();
	if(buf.length() < buf_size) buf.resize(buf_size);
	memmove((buf.m_buf()+buf_offset), sbuf.GetSPtr(), sbuf.length());
	buf_offset += sbuf.length();
	break;
	
      case gxrdBLOB : // BLOB types cannot be indexed
	if(num_bytes < sizeof(char)) return (gxrdKeyType_t)0;
	buf_offset += num_bytes; // Continue to the next field
	break;

      case gxrdBINARY : // User defined types and small binary objects
	if(num_bytes < sizeof(char)) return (gxrdKeyType_t)0;
	bin_buf.Clear();
	bin_buf.InsertAt(0, (char *)(db_record[i].m_buf()+offset), 
			 num_bytes);
	buf_size += bin_buf.length();
	if(buf.length() < buf_size) buf.resize(buf_size);
	memmove((buf.m_buf()+buf_offset), bin_buf.m_buf(), bin_buf.length());
	buf_offset += bin_buf.length();
	break;

      case gxrdINT16 :
	if(num_bytes < sizeof(gxINT16)) return (gxrdKeyType_t)0;
	buf_size += sizeof(gxINT16);
	if(buf.length() < buf_size) buf.resize(buf_size);
	memmove(&i16buf, p, sizeof(gxINT16));
	memmove((buf.m_buf()+buf_offset), &i16buf, sizeof(gxINT16));
	buf_offset += sizeof(gxINT16);
	break;

      case gxrdINT32 :
	if(num_bytes < sizeof(gxINT32)) return (gxrdKeyType_t)0;
	buf_size += sizeof(gxINT32);
	if(buf.length() < buf_size) buf.resize(buf_size);
	memmove(&i32buf, p, sizeof(gxINT32));
	memmove((buf.m_buf()+buf_offset), &i32buf, sizeof(gxINT32));
	buf_offset += sizeof(gxINT32);
	break;

      case gxrdINT64 :
	if(num_bytes < 8) return (gxrdKeyType_t)0;
	buf_size += 8;
	if(buf.length() < buf_size) buf.resize(buf_size);
#if defined (__64_BIT_DATABASE_ENGINE__)
	memmove(&i64buf, p, sizeof(gxINT64));
	memmove((buf.m_buf()+buf_offset), &i64buf, sizeof(gxINT64));
	buf_offset += sizeof(gxINT64);
#else
	memmove(&low_word, p, sizeof(gxINT32));
	memmove((buf.m_buf()+buf_offset), &low_word, sizeof(gxINT32));
	buf_offset += sizeof(gxINT32);
	memmove(&high_word, (p+sizeof(gxINT32)), sizeof(gxINT32));
	memmove((buf.m_buf()+buf_offset), &high_word, sizeof(gxINT32));
	buf_offset += sizeof(gxINT32);
#endif
	break;

      case gxrdFLOAT : case gxrdCURRENCY :
	if(num_bytes < sizeof(gxFLOAT64)) return (gxrdKeyType_t)0;
	buf_size += sizeof(gxFLOAT64);
	if(buf.length() < buf_size) buf.resize(buf_size);
	memmove(&f64buf, p, sizeof(gxFLOAT64));
	memmove((buf.m_buf()+buf_offset), &f64buf, sizeof(gxFLOAT64));
	buf_offset += sizeof(gxFLOAT64);
	break;

      case gxrdDATE :
	if(num_bytes < sizeof(CDate)) return (gxrdKeyType_t)0;
	buf_size += sizeof(CDate);
	if(buf.length() < buf_size) buf.resize(buf_size);
	memmove(&date_buf, p, sizeof(CDate));
	memmove((buf.m_buf()+buf_offset), &date_buf, sizeof(CDate));
	buf_offset += sizeof(CDate);
	break;

      default:
	return (gxrdKeyType_t)0;
    }
    if(!compare_all) break;
  }
  return calcCRC32((char *)buf.m_buf(), buf_size);
}

int gxrdDatabaseRecord::Compare(const gxrdDatabaseRecord &ob, 
				__ULWORD__ field, int case_cmp, 
				int alpha_cmp, int find_match) const
{
  int result = -1;
  gxString result_buf;
  if((!db_record) || (!ob.db_record)) return result;
  if(table_name != ob.table_name) return result;
  if(record_def != ob.record_def) return result;
  if(num_fields != ob.num_fields) return result;

  int compare_all = 0;
  if(field == (__ULWORD__)-1) compare_all = 1;
  if((field > num_fields) || (field > ob.num_fields)) compare_all = 1;

  char field_type_id1, field_type_id2;
  gxUINT32 num_bytes1, num_bytes2 ;
  __ULWORD__ offset1, offset2;
  __ULWORD__ i, j, npos1, npos2;
  char *p1, *p2;
  int type;
  
  gxString sbuf1, sbuf2;
  MemoryBuffer bin_buf1, bin_buf2;
  gxINT16 i16buf1, i16buf2;
  gxINT32 i32buf1, i32buf2;
  gxFLOAT64 f64buf1, f64buf2;
  CDate date_buf1, date_buf2;
  char bool_buf1, bool_buf2;
  
#if defined (__64_BIT_DATABASE_ENGINE__)
  gxINT64 i64buf1, i64buf2;
#else
  gxINT32 low_word1, low_word2, high_word1, high_word2;
#endif

  if(compare_all) {
    i = 0;
  }
  else {
    i = field;
  }

  for(; i < num_fields; i++) {
    offset1 = offset2 = 0;
    npos1 = npos2 = 0;
    memmove(&field_type_id1, (db_record[i].m_buf()+offset1), 
	    sizeof(field_type_id1));
    offset1 += sizeof(field_type_id1);
    memmove(&num_bytes1, (db_record[i].m_buf()+offset1), 
	    sizeof(num_bytes1));
    offset1 += sizeof(num_bytes1);

    memmove(&field_type_id2, (ob.db_record[i].m_buf()+offset2), 
	    sizeof(field_type_id2));
    offset2 += sizeof(field_type_id2);
    memmove(&num_bytes2, (ob.db_record[i].m_buf()+offset2), 
	    sizeof(num_bytes2));
    offset2 += sizeof(num_bytes2);

    // Two different types specified
    if(field_type_id1 != field_type_id2) return -1;
    type = (int)field_type_id1;

    // Move to the field type
    p1 = (char *)(db_record[i].m_buf()+offset1);
    p2 = (char *)(ob.db_record[i].m_buf()+offset2);

    // NOTE: All relational field types must be account for in
    // this function.
    switch(type) {
      case gxrdBOOL :
	if((num_bytes1 < 1) || (num_bytes2 < 1)) {
	  return -1;
	}
	memmove(&bool_buf1, p1, 1);
	memmove(&bool_buf2, p2, 1);
	if(bool_buf1 == bool_buf2) result = 0;
	if(bool_buf1 < bool_buf2) result = -1;
	if(bool_buf1 > bool_buf2) result = 1;
	break;

      case gxrdCHAR : case gxrdVCHAR :
	if((num_bytes1 < sizeof(char)) || (num_bytes2 < sizeof(char))) {
	  return -1;
	}
	sbuf1.Clear(); sbuf2.Clear();
	for(j = 0; j < num_bytes1; j++, p1++) {
	  if(*p1 == 0) {
	    npos1 = j;
	    if(j == 0) npos1++; // The first char is null
	    break;
	  }
	}
	for(j = 0; j < num_bytes2; j++, p2++) {
	  if(*p2 == 0) {
	    npos2 = j;
	    if(j == 0) npos2++; // The first char is null
	    break;
	  }
	}
	if((npos1 == 0) || (npos2 == 0)) {
	  sbuf1.InsertAt(0, (char *)(db_record[i].m_buf()+offset1), 
			 num_bytes1);
	  sbuf2.InsertAt(0, (char *)(ob.db_record[i].m_buf()+offset2), 
			 num_bytes2);
	}
	else {
	  sbuf1 = (char *)(db_record[i].m_buf()+offset1);
	  sbuf2 = (char *)(ob.db_record[i].m_buf()+offset2);
	}
	if(!case_cmp) {
	  // Disregard the string case 
	  sbuf1.ToUpper();
	  sbuf2.ToUpper();
	}
	if(find_match) { // Search for matching string
	  if(sbuf1.Find(sbuf2) != -1) {
	    result = 0;
	    break; 
	  }
	}
	if(!alpha_cmp) { 
	  // Perform alphanumeric compare for strings with numeric content
	  result = StringCompare(sbuf1, sbuf2);
	  break;
	}
	else {
	  result = StringANCompare(sbuf1.c_str(), sbuf2.c_str());
	}
	break;

      case gxrdBLOB : // BLOB types cannot be indexed
	result = -1;
	break;
	
      case gxrdBINARY : // User defined types and small binary objects
	if((num_bytes1 < sizeof(char)) || (num_bytes2 < sizeof(char))) {
	  return -1;
	}
	bin_buf1.Clear(); bin_buf2.Clear();
	bin_buf1.InsertAt(0, (char *)(db_record[i].m_buf()+offset1), 
			  num_bytes1);
	bin_buf2.InsertAt(0, (char *)(ob.db_record[i].m_buf()+offset2), 
			  num_bytes2);

	if(find_match) { // Search for matching byte pattern
	  if(bin_buf1.Find(bin_buf2) != __MEMBUF_NO_MATCH__) {
	    result = 0;
	    break; 
	  }
	}
	result = BufferCompare(bin_buf1, bin_buf2);
	break;

      case gxrdINT16 :
	if((num_bytes1 < sizeof(gxINT16)) || (num_bytes2 < sizeof(gxINT16))) {
	  return -1;
	}
	if(find_match) { // Search for matching byte pattern
	  bin_buf1.Clear(); bin_buf2.Clear();
	  bin_buf1.InsertAt(0, p1, sizeof(gxINT16));
	  bin_buf2.InsertAt(0, p2, sizeof(gxINT16));
	  if(bin_buf1.Find(bin_buf2) !=  __MEMBUF_NO_MATCH__) {
	    result = 0;
	    break; 
	  }
	}
	memmove(&i16buf1, p1, sizeof(gxINT16));
	memmove(&i16buf2, p2, sizeof(gxINT16));
	if(i16buf1 == i16buf2) result = 0;
	if(i16buf1 < i16buf2) result = -1;
	if(i16buf1 > i16buf2) result = 1;
	break;

      case gxrdINT32 :
	if((num_bytes1 < sizeof(gxINT32)) || (num_bytes2 < sizeof(gxINT32))) {
	  return -1;
	}
	if(find_match) { // Search for matching byte pattern
	  bin_buf1.Clear(); bin_buf2.Clear();
	  bin_buf1.InsertAt(0, p1, sizeof(gxINT32));
	  bin_buf2.InsertAt(0, p2, sizeof(gxINT32));
	  if(bin_buf1.Find(bin_buf2) !=  __MEMBUF_NO_MATCH__) {
	    result = 0;
	    break; 
	  }
	}
	memmove(&i32buf1, p1, sizeof(gxINT32));
	memmove(&i32buf2, p2, sizeof(gxINT32));
	if(i32buf1 == i32buf2) result = 0;
	if(i32buf1 < i32buf2) result = -1;
	if(i32buf1 > i32buf2) result = 1;
	break;

      case gxrdINT64 :
	if((num_bytes1 < 8) || (num_bytes2 < 8)) {
	  return -1;
	}
	if(find_match) { // Search for matching byte pattern
	  bin_buf1.Clear(); bin_buf2.Clear();
	  bin_buf1.InsertAt(0, p1, 8);
	  bin_buf2.InsertAt(0, p2, 8);
	  if(bin_buf1.Find(bin_buf2) !=  __MEMBUF_NO_MATCH__) {
	    result = 0;
	    break; 
	  }
	}
#if defined (__64_BIT_DATABASE_ENGINE__)
	memmove(&i64buf1, p1, sizeof(gxINT64));
	memmove(&i64buf2, p2, sizeof(gxINT64));
	if(i64buf1 == i64buf2) result = 0;
	if(i64buf1 < i64buf2) result = -1;
	if(i64buf1 > i64buf2) result = 1;
#else
	memmove(&low_word1, p1, sizeof(gxINT32));
	memmove(&low_word2, p2, sizeof(gxINT32));
	memmove(&high_word1, (p1+sizeof(gxINT32)), sizeof(gxINT32));
	memmove(&high_word2, (p2+sizeof(gxINT32)), sizeof(gxINT32));
	if(low_word1 == low_word2) {
	  if(high_word1 == high_word2) result = 0;
	  if(high_word1 < high_word2) result = -1;
	  if(high_word1 > high_word2) result = 1;
	}
	if(low_word1 < low_word2) {
	  if(high_word1 == high_word2) result = -1;
	  if(high_word1 < high_word2) result = -1;
	  if(high_word1 > high_word2) result = 1;
	}
	if(low_word1 > low_word2) {
	  if(high_word1 == high_word2) result = 1;
	  if(high_word1 < high_word2) result = -1;
	  if(high_word1 > high_word2) result = 1;
	}
#endif
	break;

      case gxrdFLOAT : case gxrdCURRENCY :
	if((num_bytes1 < sizeof(gxFLOAT64)) || 
	   (num_bytes2 < sizeof(gxFLOAT64))) {
	  return -1;
	}
	if(find_match) { // Search for matching byte pattern
	  bin_buf1.Clear(); bin_buf2.Clear();
	  bin_buf1.InsertAt(0, p1, sizeof(gxFLOAT64));
	  bin_buf2.InsertAt(0, p2, sizeof(gxFLOAT64));
	  if(bin_buf1.Find(bin_buf2) !=  __MEMBUF_NO_MATCH__) {
	    result = 0;
	    break; 
	  }
	}
	memmove(&f64buf1, p1, sizeof(gxFLOAT64));
	memmove(&f64buf2, p2, sizeof(gxFLOAT64));
	if(f64buf1 == f64buf2) result = 0;
	if(f64buf1 < f64buf2) result = -1;
	if(f64buf1 > f64buf2) result = 1;
	break;

      case gxrdDATE :
	if((num_bytes1 < sizeof(CDate)) || (num_bytes2 < sizeof(CDate))) {
	  return -1;
	}
	if(find_match) { // Search for matching byte pattern
	  bin_buf1.Clear(); bin_buf2.Clear();
	  bin_buf1.InsertAt(0, p1, sizeof(CDate));
	  bin_buf2.InsertAt(0, p2, sizeof(CDate));
	  if(bin_buf1.Find(bin_buf2) !=  __MEMBUF_NO_MATCH__) {
	    result = 0;
	    break; 
	  }
	}
	memmove(&date_buf1, p1, sizeof(CDate));
	memmove(&date_buf2, p2, sizeof(CDate));
	if(date_buf1 == date_buf2) result = 0;
	if(date_buf1 < date_buf2) result = -1;
	if(date_buf1 > date_buf2) result = 1;
	break;

      default:
	return -1;
    }
    if(compare_all) { // Record each result
      result_buf << result << " ";
    }
    else { // Compare a single field and exit
      return result;
    }
  }

  // A record field is less then the total weight
  if(result_buf.Find("-1")) return -1;
  
  // A record field is greater then the total weight
  if(result_buf.Find("1")) return -1;

  return 0; // All fields are equal 
}

gxDatabaseError gxrdDatabaseRecord::CreateRecord(const gxString &def, 
						 __ULWORD__ cmp_field, 
						 int case_cmp, int alpha_cmp)
{
  // Reset the current record before creating a new one
  Clear();

  // Set compare criteria
  compare_field = cmp_field; 
  compare_case = case_cmp;
  compare_alpha = alpha_cmp;

  // Create the record and return
  return CreateRecord(def, 0);
}

gxDatabaseError gxrdDatabaseRecord::CreateRecord(const gxString &def, int clear_rec) 
{
  // Reset the current record before creating a new one
  if(clear_rec) Clear();

  // Parse the record definition
  char words[MAXWORDS][MAXWORDLENGTH];
  int num_words, parse_err;
  parse_err = parse(def.c_str(), words, &num_words, ',');
  if(parse_err != 0) {
    if(clear_rec) Clear();
    return gxDBASE_BAD_RECORD_DEF;
  }
  if(num_words < 2) {
    if(clear_rec) Clear();
    return gxDBASE_BAD_RECORD_DEF;
  }
  
  // Set the table name
  if(*words[0]) { 
    table_name = words[0];
  }
  else {
    if(clear_rec) Clear();
    return gxDBASE_BAD_RECORD_DEF;
  }
  
  gxString sbuf;
  gxString array_len;
  gxString field_type;
  int found_field_type;
  int i, j;
  __ULWORD__ size = 0;
  __ULWORD__ overhead = 0;
  
  num_fields = num_words - 1;

  for(i = 1; i < num_words; i++) {
    if(*words[i]) {
      array_len.Clear(); // Clear the attribute string
      found_field_type = 0;
      sbuf = words[i];
      sbuf.TrimLeadingSpaces();
      sbuf.TrimTrailingSpaces();
      field_type = sbuf;
      if(field_type.Find("[") != -1) {
	array_len = sbuf;
	array_len.DeleteBeforeIncluding("[");
	array_len.DeleteAfterIncluding("]");
	array_len.TrimLeadingSpaces();
	array_len.TrimTrailingSpaces();
	field_type.DeleteAfterIncluding("[");
	field_type.TrimTrailingSpaces();
      }
      for(j = 0; j < gxrdNUM_FIELD_TYPES; j++) {
	if(field_type == gxrdFIELD_TYPES_STRINGS[j]) {
	  found_field_type = 1;
	  break;
	}
      }
      if(!found_field_type) {
	if(clear_rec) Clear();
	return gxDBASE_BAD_FIELD_TYPE;
      }

      gxrdFIELD_TYPES type = (gxrdFIELD_TYPES)j;
      GetRecordTypeSize(type, size, overhead);

      // Set the number of bytes allocated for this data type
      gxUINT32 num_bytes = (gxUINT32)0;

      if(!array_len.is_null()) {
	num_bytes = (size * array_len.Atoi());
	if(num_bytes < size) num_bytes = size;
      }
      else {
	num_bytes = size;
      }
    }
    else {
      if(clear_rec) Clear();
      return gxDBASE_BAD_FIELD_TYPE;
    }
  }
  
  if(!AllocArray(num_fields)) {
    if(clear_rec) Clear();
    return gxDBASE_MEM_ALLOC_ERROR;
  }

  // Initialize the record
  gxDatabaseError err = InitRecord(def);
  if(err != gxDBASE_NO_ERROR) {
    if(clear_rec) Clear();
    return err;
  }

  // Set the record definition
  record_def = def;

  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxrdDatabaseRecord::CreateDiskRecord(MemoryBuffer &disk_record)
{
  if(!db_record) return gxDBASE_NULL_PTR;

  // Record header stored with each record
  gxrdRecordHeader rh(table_name, num_fields);

  // Allocate space in the data file for the record and 
  // the record header.
  __ULWORD__ length = (this->SizeOf() + rh.SizeOf());

  // Resize the memory buffer if required
  if(disk_record.length() < length) {
    disk_record.resize(length, 0);
  }

  // Clear the disk record
  disk_record.MemSet(0, 0, length);

  // Load the record header
  memmove(disk_record.m_buf(), &rh, rh.SizeOf());

  char field_type_id;
  gxUINT32 num_bytes;
  __ULWORD__ field_len = 0;
  __ULWORD__ buf_offset = rh.SizeOf(); // Set the buffer offset
  __ULWORD__ rec_offset = 0;

  for(__ULWORD__ i = 0; i < num_fields; i++) {
    rec_offset = 0; // Reset the record offset
    memmove(&field_type_id, (db_record[i].m_buf()+rec_offset),
	    sizeof(field_type_id));
    rec_offset += sizeof(field_type_id);
    memmove(&num_bytes, (db_record[i].m_buf()+rec_offset),
	    sizeof(num_bytes));
    rec_offset += sizeof(num_bytes);

    memmove((disk_record.m_buf()+buf_offset), &field_type_id,
	    sizeof(field_type_id));
    buf_offset += sizeof(field_type_id);

    // Check for exclude fields
    if(exclude_fields.Exclude(i)) {
      num_bytes = (gxUINT32)0;
    }
    
    memmove((disk_record.m_buf()+buf_offset), &num_bytes, sizeof(num_bytes));
    buf_offset += sizeof(num_bytes);

    // Ensure that at least one byte has been allocated
    if(num_bytes == (gxUINT32)0) continue;

    // Test the record length to ensure that size of the record
    // stored in memory is not larger than the original record size.
    field_len += ((sizeof(field_type_id)+sizeof(num_bytes))+num_bytes);

    if(disk_record.length() < field_len) {
      disk_record.resize(field_len);
    }
    memmove((disk_record.m_buf()+buf_offset), 
	    (db_record[i].m_buf()+rec_offset), num_bytes);

    buf_offset += num_bytes; // Position the buffer offset to the next field
  }

  return gxDBASE_NO_ERROR;
}

gxDatabaseError gxrdDatabaseRecord::LoadDiskRecord(\
					   const MemoryBuffer &disk_record)
{
  if(!db_record) return gxDBASE_NULL_PTR;

  // Record header stored with each record
  gxrdRecordHeader rh;

  // Read the record header
  memmove(&rh, disk_record.m_buf(), rh.SizeOf());
  if(!rh.TestTableID(table_name)) {
    return gxDBASE_BAD_TABLE_ID;
  }

  char field_type_id;
  gxUINT32 num_bytes;
  __ULWORD__ byte_count = rh.SizeOf();
  __ULWORD__ field_len = 0;
  __ULWORD__ buf_offset = rh.SizeOf(); // Set the buffer offset
  __ULWORD__ rec_offset = 0;

  for(__ULWORD__ i = 0; i < rh.num_fields; i++) {
    byte_count += sizeof(field_type_id);
    if(byte_count > disk_record.length()) {
      return gxDBASE_BAD_RECORD;
    }
    memmove(&field_type_id, (disk_record.m_buf()+buf_offset), 
	    sizeof(field_type_id));
    buf_offset += sizeof(field_type_id);
    byte_count += sizeof(num_bytes);
    if(byte_count > disk_record.length()) {
      return gxDBASE_BAD_RECORD;
    }
    memmove(&num_bytes, (disk_record.m_buf()+buf_offset), 
	    sizeof(num_bytes));
    buf_offset += sizeof(num_bytes);

    // Test the record length to ensure that size of the record
    // stored on disk is not larger than the in-memory copy. If so
    // resize the in-memory copy to hold the field type and length
    // variables.
    field_len = (sizeof(field_type_id) + sizeof(num_bytes));
    if(db_record[i].length() < field_len) {
      db_record[i].resize(field_len);
    }

    rec_offset = 0; // Reset the record offset
    memmove((db_record[i].m_buf()+rec_offset), &field_type_id, 
	    sizeof(field_type_id));
    rec_offset += sizeof(field_type_id);

    // Check for exclude fields
    if(exclude_fields.Exclude(i)) {
      num_bytes = (gxUINT32)0;
    }

    memmove((db_record[i].m_buf()+rec_offset), &num_bytes, 
	    sizeof(num_bytes));
    rec_offset += sizeof(num_bytes);

    // Ensure that the number of bytes do not excced the length of
    // the memory buffer.
    byte_count += num_bytes;
    if(byte_count > disk_record.length()) {
      return gxDBASE_BAD_RECORD;
    }

    // Ensure that at least one byte has been allocated
    if(num_bytes == (gxUINT32)0) continue;

    // Test the record length to ensure that size of the record
    // stored on disk is not larger than the in-memory copy. If so
    // resize the in-memory copy to hold the field data.
    field_len += num_bytes;
    if(db_record[i].length() < field_len) {
      db_record[i].resize(field_len);
    }
    memmove((db_record[i].m_buf()+rec_offset), 
	    (disk_record.m_buf()+buf_offset), num_bytes);
    buf_offset += num_bytes; // Position the buffer offset to the next field
  }
  
  return gxDBASE_NO_ERROR;
}

int gxrdExcludeFields::Exclude(gxUINT32 field)
// Function used to check for fields to exclude from 
// a table. Returns true if the field is to be excluded.
// NOTE: The field variable represents a zero-based field 
// number.  
{
  int exclude = 0;
  int i;

  if((num_fields > (gxUINT32)0) && (fields)) {
    for(i = 0; i < num_fields; i++) {
      if(fields[i] == field) {
	exclude = 1;
	break;
      }
    }
  }
  return exclude;
}

void gxrdExcludeFields::SetNumFields(gxUINT32 nfields) 
{
  if(nfields < tableMIN_FIELDS) nfields = tableMIN_FIELDS;
  if(nfields > tableMAX_FIELDS) nfields = tableMAX_FIELDS;
  if(fields) delete fields;
  num_fields = nfields;
  fields = new gxUINT32[num_fields];
  if(!fields) {
    num_fields = 0;
    return;
  }
  for(int i = 0; i < num_fields; i++) {
    // Reset all the field numbers to a default value
    fields[i] = (gxUINT32)0xffffffff;
  }
}

void gxrdExcludeFields::Clear() 
{
  if(fields) delete[] fields;
  fields = 0; 
  num_fields = 0;
}

__ULWORD__ gxrdExcludeFields::SizeOf() 
{
  __ULWORD__ len = sizeof(num_fields);
  return (len + __ULWORD__(sizeof(num_fields)*num_fields));
}

void gxrdExcludeFields::Copy(const gxrdExcludeFields &ob) 
{
  if(fields) delete[] fields;
  num_fields = ob.num_fields;
  if(ob.fields) {
    fields = new gxUINT32[num_fields];
    memmove(fields, ob.fields, num_fields);
  }
}

GXDLCODE_API const char *gxrdFieldTypeString(gxrdFIELD_TYPES t)
{
  int error = (int)t;
  if(error > (gxrdNUM_FIELD_TYPES-1)) error = gxrdINVALID_TYPE;
  return gxrdFIELD_TYPES_STRINGS[error];
}

#ifdef __BCC32__
#pragma warn .8071
#pragma warn .8080
#endif
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
