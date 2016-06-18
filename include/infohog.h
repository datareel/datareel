// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: infohog.h
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/17/1997  
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

InfoHog database application. InfoHog is a general-purpose platform 
interoperable database used store any type of binary data. InfoHog data 
is organized in a member array with member 0 representing a fixed length 
primary key. The key type must be defined by the application or derived 
class when InfoHog objects are constructed. All other array elements can 
be any built-in or user defined type known to the application or the 
derived class. The application or the derived class is responsible for 
type casting the members of the object array to the correct type. The 
member array is variable in length for both in-memory copies and disk 
copies of InfoHog objects. This feature allows applications or a derived 
class to add or subtract fields from database records without having to 
rebuild the database. Using InfoHog you can rapidly assemble any type of 
relational or object-oriented database.

Changes:
==============================================================
10/05/2001: Modified the InfoHog copy constructor to set the
member array to zero before copying to tell the AllocArray()
function that no memory has been allocated for the array.

02/08/2002: All InfoHog functions using non-persistent file 
address units have been modified to use FAU_t data types instead 
of FAU types. FAU_t types are native or built-in integer types 
and require no additional overhead to process. All persistent 
file address units still use the FAU integer type, which is a 
platform independent type allowing database files to be shared 
across multiple platforms.

05/07/2002: Fixed non-key change problem in the InfoHog<TYPE>::
ChangeObject() function. The ChangeObject() functions was not 
correctly changing non-key members for objects with the same 
key and different general purpose members.

05/08/2002: Fixed single general-purpose member change problem 
in InfoHog<TYPE>::ChangeObject() function. The ChangeObject() 
function was not correctly changing objects with only one general 
purpose member.
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_INFOHOG_HPP__
#define __GX_INFOHOG_HPP__

#include "gxdlcode.h"

#include "gpersist.h"
#include "membuf.h"

// Constants
const unsigned InfoHogMinMembers = 1;   // Minimum number of members
const unsigned InfoHogDefMembers = 9;   // Default number of members
const unsigned InfoHogMaxMembers = 255; // Maximum number of members
const unsigned InfoHogNameLength = 256; // Default name lengths

// Default number of keys per tree node
const BtreeNodeOrder_t InfoHogNodeOrder = 11; 

// Default Class ID for InfoHog objects
const gxClassID ClassInfoHogID = (gxClassID)-1; 
                     
// This infohog configuration will allow 255 Btree indexes per index file
// and 255 static data entries per data file.
const int InfoHogNumTrees = 255;   // Max number of Btrees per index file
const int InfoHogUseIndexFile = 1; // Used to enable the use of index files
const FAU_t InfoHogStaticArea = FAU_t(255 * 255); // Static data area

// Primary key type used by this database
template<class TYPE>
class InfoHogKey_t
{
public:
  InfoHogKey_t() { }
  ~InfoHogKey_t() { }

public: // Persistent data members
  // NOTE: Do not change the ordering of the data members in order to
  // maintain a uniform byte alignment between the in-memory copy of
  // the object and the copy stored on disk. If the member alignment
  // is changed all database files using this structure will have to
  // be rebuilt.
  TYPE object_name;     // Object name
  gxObjectID object_id; // Object data file address/identification number
  gxClassID class_id;   // Object class identification number
};

// Primary key class used by this database 
template<class TYPE>
class InfoHogKey : public DatabaseKeyB
{
public:
  InfoHogKey(int dup_names = 0) : DatabaseKeyB((char *)&key) {
    key.object_id = (gxObjectID)0;
    key.class_id = (gxClassID)0;
    allow_dup_names = dup_names;
  }

  InfoHogKey(TYPE &name, gxObjectID oid = (gxObjectID_t)0, 
	     gxClassID cid = (gxClassID_t)0, int dup_names = 0) : 
    DatabaseKeyB((char *)&key) {
    key.object_name = name;
    key.object_id = oid;
    key.class_id = cid;
    allow_dup_names = dup_names;
  }
  
public: // Base class interface
  size_t KeySize() { return sizeof(key); }
  int operator==(const DatabaseKeyB& k) const;
  int operator>(const DatabaseKeyB& k) const;

  // NOTE: This comparison function is only used if the 
  // __USE_SINGLE_COMPARE__ preprocessor directive is 
  // defined when the program is compiled.
  int CompareKey(const DatabaseKeyB& key) const;

public:
  TYPE &ObjectName() { return key.object_name; }
  gxObjectID ObjectID() const { return key.object_id; }
  gxClassID ClassID() const { return key.class_id; }
  void SetObjectName(const TYPE &name) { key.object_name = name; }
  void SetObjectName(TYPE &name) { key.object_name = name; }
  void SetObjectID(gxObjectID oid) { key.object_id = oid; }
  void SetClassID(gxClassID cid) { key.class_id = cid; }
  void AllowDuplicates() { allow_dup_names = 1; }
  void DisallowDuplicates() { allow_dup_names = 0; }

private: // Non-persistent data members
  int allow_dup_names;

private: // Persistent data members
  InfoHogKey_t<TYPE> key;
};

template<class TYPE>
class InfoHog : public gxPersistent
{
public:
  InfoHog(POD *pod, unsigned members = InfoHogDefMembers);
  InfoHog(const POD *pod, unsigned members = InfoHogDefMembers);
  InfoHog(POD *pod, const TYPE &key, unsigned members = InfoHogDefMembers);
  InfoHog(const POD *pod, const TYPE &key, 
	  unsigned members = InfoHogDefMembers);
  InfoHog();
  InfoHog(const InfoHog<TYPE> &ob) { 
    ih_members = 0;
    Copy(ob); 
  }
  void operator=(const InfoHog<TYPE> &ob) { Copy(ob); }
  virtual ~InfoHog();
  
public:
  int SetMember(const void *ob, unsigned len, unsigned index);
  int Copy(const InfoHog<TYPE> &ob, int copy_pod_vars = 1);
  MemoryBuffer *GetMemberArray() { return ih_members; }
  unsigned char *GetMember(unsigned index);
  unsigned GetMemberLen(unsigned index);
  unsigned GetNumMembers() { return num_members; }
  gxObjectID GetObjectID() { return objectaddress; }
  void SetObjectID(gxObjectID oid) { objectaddress = oid; }

public: // Run time type information functions
  gxClassID GetClassID() { return class_id; }
  void SetClassID(gxClassID cid) { class_id = cid; }

public: // Database utility functions
  int CompareIndex(unsigned index_number);
  int RebuildIndexFile(const char *fname, unsigned index_number,
		       int num_trees, BtreeNodeOrder_t node_order);
    
public: // Database panel functions
  int OverWriteObject(FAU_t object_address);
  int OverWriteObject() { return OverWriteObject(objectaddress); }
  int ChangeObject(InfoHog<TYPE> &ob, int find_object = 1);

public: // Database grid functions
  int ChangeKeyMember(TYPE &k, int find_object = 1);
  int ChangeMember(const void *ob, unsigned len, unsigned index,
		   int find_object = 1);

public: // Overloaded operators
  virtual int operator==(const InfoHog<TYPE> &ob) const;
  virtual int operator!=(const InfoHog<TYPE> &ob) const;
  virtual int operator<(const InfoHog<TYPE> &ob) const;
  virtual int operator>(const InfoHog<TYPE> &ob)  const;
  
private: // Base class interface
  gxDatabaseError Write();
  gxDatabaseError Read(FAU Address);
  int Find();
  int Delete();
  __UWORD__ ObjectLength();
  
protected:
  int AllocArray(unsigned num);
  void DeleteArray();

protected: // Non-persistent data members
  gxClassID class_id;   // Optional class ID use to type InfoHog objects
  unsigned num_members; // Number of members this object is using

protected: // Persistent data members
  // NOTE: Do not change the ordering of the data members in order to
  // maintain a uniform byte alignment between the in-memory copy of
  // the object and the copy stored on disk. If the member alignment
  // is changed all database files using this structure will have to
  // be rebuilt.
  MemoryBuffer *ih_members; // Infohog data member array
};

template<class TYPE>
int InfoHogKey<TYPE>::operator==(const DatabaseKeyB& k) const
{
  const InfoHogKey<TYPE> *kptr = (const InfoHogKey<TYPE> *)(&k);
  // NOTE: Duplicate names will not be allowed if the object ID is ignored
  if(!allow_dup_names) {  
    return key.object_name == kptr->key.object_name;
  }
  else { // Allow duplicate names by comparing the object ID  
    return ((key.object_name == kptr->key.object_name) && \
	    (key.object_id == kptr->key.object_id));
  }
}

template<class TYPE>
int InfoHogKey<TYPE>::operator>(const DatabaseKeyB& k) const
{
  const InfoHogKey<TYPE> *kptr = (const InfoHogKey<TYPE> *)(&k);
  // NOTE: Duplicate names will not be allowed if the object ID is ignored
  if(!allow_dup_names) { 
    return key.object_name > kptr->key.object_name;
  }
  else { // Allow duplicate names by comparing the object ID  
    return ((key.object_name > kptr->key.object_name) && \
	    (key.object_id > kptr->key.object_id));
  }
}

template<class TYPE>
int InfoHogKey<TYPE>::CompareKey(const DatabaseKeyB& k) const
{
  const InfoHogKey<TYPE> *kptr = (const InfoHogKey<TYPE> *)(&k);
  int rv = -1;
  if(key.object_name > kptr->key.object_name) rv = 1;
  if(key.object_name == kptr->key.object_name) rv = 0;  

  // NOTE: Duplicate names will not be allowed if the object ID is ignored
  if(!allow_dup_names) { 
    return rv;
  }
  else { // Allow duplicate names by comparing the object ID  
    if(rv == 0) {
      if(key.object_id > kptr->key.object_id) return 1;
      if(key.object_id < kptr->key.object_id) return -1;
    }
  }
  return rv;
}

template<class TYPE>
InfoHog<TYPE>::InfoHog() 
{ 
  ih_members = 0; 
  num_members = 0; 
  class_id = ClassInfoHogID;
}

template<class TYPE>
InfoHog<TYPE>::InfoHog(POD *pod, unsigned members) :
  gxPersistent(pod) 
{ 
  ih_members = 0; // Initialize the array before allocating memory
  AllocArray(members);
  class_id = ClassInfoHogID;
}

template<class TYPE>
InfoHog<TYPE>::InfoHog(const POD *pod, unsigned members) : 
  gxPersistent(pod)
{
  ih_members = 0; // Initialize the array before allocating memory
  AllocArray(members);
  class_id = ClassInfoHogID;
}

template<class TYPE>
InfoHog<TYPE>::InfoHog(POD *pod, const TYPE &key, unsigned members) :
  gxPersistent(pod)
{
  ih_members = 0; // Initialize the array before allocating memory
  AllocArray(members);
  class_id = ClassInfoHogID;
  SetMember(&key, sizeof(key), 0); // Set the key name
}

template<class TYPE>
InfoHog<TYPE>::InfoHog(const POD *pod, const TYPE &key, unsigned members) :
  gxPersistent(pod)
{
  ih_members = 0; // Initialize the array before allocating memory
  AllocArray(members);
  class_id = ClassInfoHogID;
  SetMember(&key, sizeof(key), 0); // Set the key name
}

template<class TYPE>
InfoHog<TYPE>::~InfoHog() 
{
  DeleteArray();
}

template<class TYPE>
unsigned char *InfoHog<TYPE>::GetMember(unsigned index) 
// Returns a pointer to the specified member of the object array.
// If the index is out of bounds or no memory has been allocated
// for the member array this function will return a null value.
{ 
  if(index > num_members) return 0;
  if(!ih_members) return 0;
  return ih_members[index].m_buf(); 
}

template<class TYPE>
unsigned InfoHog<TYPE>::GetMemberLen(unsigned index) 
// Returns the length of the specified member in the object array.
// If the index is out of bounds or no memory has been allocated
// for the member array this function will return zero.
{ 
  if(index > num_members) return 0;
  if(!ih_members) return 0;
  return ih_members[index].length();
}

template<class TYPE>
int InfoHog<TYPE>::SetMember(const void *ob, unsigned len, unsigned index) 
// Sets a member of the object array. NOTE: The primary key will always
// be index number 0. Returns true if the member was set or false if
// an error occurred.
{
  if(!ih_members) return 0;
  if(index > num_members) return 0;
  if(!ih_members[index].Load(ob, len)) return 0;
  return 1;
}

template<class TYPE>
int InfoHog<TYPE>::AllocArray(unsigned num)
// Allocate memory for the object array. The length of the
// member array will be equal to the number of array elements
// multiplied by the size of a MemoryBuffer object. NOTE: This
// function will not allocate memory for the array if the 
// current object array is large enough to hold the number
// of elements requested. Returns true if allocation is
// successful or false if a memory allocation error occurs.
{
  // Check the object array boundaries
  if(num == 0) num = InfoHogMinMembers;
  if(num > InfoHogMaxMembers) num = InfoHogMaxMembers;

  if(ih_members) { // Memory has been allocated for the array
    if(num_members >= num) { // Do not re-allocate
      num_members = num; 
      return 1;
    }
    else {
      DeleteArray(); // Free existing memory in use
    }
  }

  MemoryBuffer *buf = new MemoryBuffer[num];
  if(!buf) { 
    num_members = 0;
    ih_members = 0;
    return 0; // An memory allocation error occurred
  }

  ih_members = buf;
  num_members = num; // Set the length of the array
  return 1;
}

template<class TYPE>
void InfoHog<TYPE>::DeleteArray()
// Free the memory used for the object array and reset the
// array variables.
{
  if(ih_members) delete[] ih_members;
  ih_members = 0;
  num_members = 0;
}

template<class TYPE>
__UWORD__ InfoHog<TYPE>::ObjectLength()
// Calculates the length of this object based on total number of 
// InfoHog data members: 
// [num_members][len_record*num_members][member_len*num_members] 
{
  __UWORD__ len = sizeof(gxUINT32); // Persistent number of members record
  len += (num_members * sizeof(gxUINT32)); // Persistent length record
  
  for(unsigned i = 0; i  < num_members; i++) {
    len += ih_members[i].length(); // Length of the data member 
  }
  return len;
}

template<class TYPE>
gxDatabaseError InfoHog<TYPE>::Write()
{
  gxObjectHeader oh;

  // Allocate a block in the data file for this object's data
  objectaddress = pod->OpenDataFile()->Alloc(sizeof(gxObjectHeader) + 
					     ObjectLength());
  
  // Check for any allocation errors
  if(objectaddress == (FAU_t)0) {
    return pod->GetDataFileError();
  }

  oh.ClassID = GetClassID();
  oh.ObjectID = objectaddress;

  // Write the object header to the datafile
  if(WriteObjectHeader(oh) != gxDBASE_NO_ERROR) {
    return pod->GetDataFileError();
  }
  
  // Record the number of members
  gxUINT32 num = (__ULWORD__)num_members;
  if(pod->OpenDataFile()->Write(&num, sizeof(gxUINT32)) != gxDBASE_NO_ERROR) {
    return pod->GetDataFileError();
  }

  // Record the length of each object and write object 
  for(unsigned i = 0; i  < num_members; i++) {
    gxUINT32 len = (__ULWORD__)ih_members[i].length();
    if(pod->OpenDataFile()->Write(&len, sizeof(gxUINT32)) != 
       gxDBASE_NO_ERROR) {
      return pod->GetDataFileError();
    }
    if(len > (__ULWORD__)0) {
      if(pod->OpenDataFile()->Write(ih_members[i].m_buf(), len) != 
	 gxDBASE_NO_ERROR) {
	return pod->GetDataFileError();
      }   
    }
  }

  // Add the entry to the Index file
  if(UsingIndex()) {
    InfoHogKey<TYPE> key(*((TYPE *)ih_members[0].m_buf()), 
			   oh.ObjectID, oh.ClassID);
    InfoHogKey<TYPE> compare_key;
    if(!AddKey(key, compare_key)) {
      return pod->GetIndexFileError();
    }
  }

  return gxDBASE_NO_ERROR;
}

template<class TYPE>
gxDatabaseError InfoHog<TYPE>::Read(FAU object_address)
{
  gxObjectHeader oh;
  
  // Optimize seeks during intervening reads
  pod->OpenDataFile()->SeekTo(object_address);
  if(pod->GetDataFileError() != gxDBASE_NO_ERROR) {
    return pod->GetDataFileError();
  }
  
  if(ReadObjectHeader(oh, object_address) != gxDBASE_NO_ERROR) {
    return pod->GetDataFileError();
  }
  
  // Incorrect object type
  if(oh.ClassID != GetClassID()) {
    return pod->SetDataFileError(gxDBASE_BAD_CLASS_ID);
  }
  
  // Read the only the name portion of the database key   
  gxUINT32 num = (__ULWORD__)0;
  if(pod->OpenDataFile()->Read(&num, sizeof(gxUINT32)) != gxDBASE_NO_ERROR) {
    return pod->GetDataFileError();
  }
  
  // Ensure the correct amount of memory is allocated for the member array
  if(!AllocArray((__ULWORD__)num)) {
    // Error - 
  }
  
  // Read the InfoHog members
  for(unsigned i = 0; i  < num_members; i++) {
    gxUINT32 len = (__ULWORD__)0;
    if(pod->OpenDataFile()->Read(&len, sizeof(gxUINT32)) != gxDBASE_NO_ERROR) {
      return pod->GetDataFileError();
    }
    if(len > (__ULWORD__)0) {
      ih_members[i].resize((__ULWORD__)len, 0);
      if(pod->OpenDataFile()->Read(ih_members[i].m_buf(), len) != 
	 gxDBASE_NO_ERROR) {
	return pod->GetDataFileError();
      }   
    }
  }

  objectaddress = object_address;

  return gxDBASE_NO_ERROR;
}

template<class TYPE>
int InfoHog<TYPE>::Find()
{
  // Search the index file for this entry
  if(UsingIndex()) {
    InfoHogKey<TYPE> key(*((TYPE *)ih_members[0].m_buf()));
    InfoHogKey<TYPE> compare_key;
    if(!FindKey(key, compare_key)) return 0;
    objectaddress = key.ObjectID();
    return 1; // Found the index file entry
  }

  // If not using index file search the data file
  InfoHog<TYPE> infohog;
  FAU_t oa;          // Object Address
  gxBlockHeader blk; // Block Header
  gxObjectHeader oh; // Object Header
  
  FAU_t gxdfileEOF = pod->OpenDataFile()->GetEOF();
  FAU_t addr = (FAU_t)0;
  addr = pod->OpenDataFile()->FindFirstBlock(addr); // Search the entire file

  if(addr == (FAU_t)0) return 0; // No database blocks found in file

  infohog.Copy(*(this), 0); // Make a tempory copy of the object

  while(1) { 
    if(FAU_t(addr + pod->OpenDataFile()->BlockHeaderSize()) >= gxdfileEOF) 
      break;
    if(pod->OpenDataFile()->Read(&blk, sizeof(gxBlockHeader), addr) !=
       gxDBASE_NO_ERROR) {
      return 0;
    }
    if(blk.block_check_word == gxCheckWord) {
      if((__SBYTE__)blk.block_status == gxNormalBlock) {
	oa = addr + pod->OpenDataFile()->BlockHeaderSize();
	if(ReadObjectHeader(oh, oa) != gxDBASE_NO_ERROR) {
	  return 0;
	}
	if(oh.ClassID == GetClassID()) { 
	  if(Read(oa) != gxDBASE_NO_ERROR) {
	    return 0;
	  }
	  if(ih_members[0] == infohog.ih_members[0]) {
	    objectaddress = oa;
	    return 1; // Found unique data member
	  }
	}
      }
      addr = addr + blk.block_length; // Goto the next database block
    }
    else {
      addr++; // Keep searching until a valid database block is found
    }
  }

  this->Copy(infohog, 0); // Reset this object's data
  return 0; // Could not find 
}

template<class TYPE>
int InfoHog<TYPE>::Copy(const InfoHog<TYPE> &ob, int copy_pod_vars)
// Copy the contents of the specified object into this object. If 
// the "copy_pod_vars" variable is true the POD and data file address 
// pointers plus all other POD variables will be copied as well as 
// the object's data members. NOTE: This function assumes that memory
// has been allocated for the "ob" object array.
{
  // Ensure that memory is allocated for the member array
  if(!AllocArray(ob.num_members)) return 0;

  // Copy the object array. NOTE: Not using a memmove to ensure that
  // each element of the member array is unique to this object
  for(unsigned i = 0; i < num_members; i++) {
    ih_members[i] = ob.ih_members[i];
  }

  if(copy_pod_vars) { // Copy the POD variables
    class_id = ob.class_id;
    pod = ob.pod;
    objectaddress = ob.objectaddress;
  }
  return 1;
}

template<class TYPE>
int InfoHog<TYPE>::operator==(const InfoHog<TYPE> &ob) const
{
  if(num_members != ob.num_members) return 0;
  for(unsigned i = 0; i  < num_members; i++) {
    if(ih_members[i] != ob.ih_members[i]) return 0;
  }
  return 1;
}

template<class TYPE>
int InfoHog<TYPE>::operator!=(const InfoHog<TYPE> &ob) const
{
  if(num_members != ob.num_members) return 1;
  for(unsigned i = 0; i  < num_members; i++) {
    if(ih_members[i] != ob.ih_members[i]) return 1;
  }
  return 0;
}

template<class TYPE>
int InfoHog<TYPE>::operator>(const InfoHog<TYPE> &ob) const
{
  if(ih_members[0] > ob.ih_members[0]) return 1;
  return 0;
}

template<class TYPE>
int InfoHog<TYPE>::operator<(const InfoHog<TYPE> &ob) const
{
  if(ih_members[0] < ob.ih_members[0]) return 1;
  return 0;
}
  
template<class TYPE>
int InfoHog<TYPE>::Delete()
{
  if(UsingIndex()) {
    InfoHogKey<TYPE> key(*((TYPE *)ih_members[0].m_buf()));
    InfoHogKey<TYPE> compare_key;
    if(!FindKey(key, compare_key)) return 0; // Could not find the key
    objectaddress = key.ObjectID();
    if(!DeleteObject(objectaddress)) return 0; // Could not delete the object
    if(!DeleteKey(key, compare_key)) return 0; // Could not delete the key
    return 1; // The index and data file entry was deleted
  }

  // If not using index file search the data file
  if(!Find()) return 0; // Could not find the data file entry
  if(DeleteObject(objectaddress)) return 0; // Could not delete the object

  // The object was deleted from the data file
  return 1;
}

template<class TYPE>
int InfoHog<TYPE>::CompareIndex(unsigned index_number)
// Compares the data file to the index file.
// Returns true if data and index file match.
{
  if(!UsingIndex()) return 0;

  // Ensure that the in memory buffers and the file data
  // stay in sync during multiple file access.
  pod->Index(index_number)->TestTree();

  InfoHog<TYPE> infohog(pod);
  InfoHogKey<TYPE> key, compare_key;
  
  FAU_t oa;          // Object Address
  gxBlockHeader blk; // Block Header
  gxObjectHeader oh; // Object Header
  
  int objects = 0; // Keeps track of good database blocks
  int matches = 0; // Keep track of matches
  
  FAU_t gxdfileEOF = pod->OpenDataFile()->GetEOF();
  FAU_t addr = (FAU_t)0;
  addr = pod->OpenDataFile()->FindFirstBlock(addr); // Search the entire file

  if(addr == (FAU_t)0) return 0; // No database blocks found in file

  while(1) { 
    if(FAU_t(addr + pod->OpenDataFile()->BlockHeaderSize()) >= gxdfileEOF) 
      break;
    pod->OpenDataFile()->Read(&blk, sizeof(gxBlockHeader), addr);
    if(blk.block_check_word == gxCheckWord) {
      if((__SBYTE__)blk.block_status == gxNormalBlock) {
	oa = addr + pod->OpenDataFile()->BlockHeaderSize();
	ReadObjectHeader(oh, oa);

	if(oh.ClassID == GetClassID()) { 
	  objects++; // Increment the object count
	  infohog.Read(oa);
	  key.SetObjectName(*((TYPE *)infohog.ih_members[0].m_buf()));
	  key.SetObjectID(oa);
	  key.SetClassID(oh.ClassID);
	  if(FindKey(key, compare_key, index_number)) {
	    matches++; // Index and data file match
	  }
	}
      }
      addr = addr + blk.block_length; // Goto the next database block
    }
    else {
      addr++; // Keep searching until a valid database block is found
    }
  }
  return objects == matches;
}

template<class TYPE>
int InfoHog<TYPE>::RebuildIndexFile(const char *fname, unsigned index_number,
				    int num_trees, BtreeNodeOrder_t node_order)
// Function used to rebuild a damaged index file. Returns the total number
// of members inserted into the new index file or zero if an error occurs.
{
  if(!UsingIndex()) return 0;
  InfoHogKey<TYPE> key, compare_key;
  gxBtree btx(key, node_order);
  if(btx.Create(fname, num_trees) != gxDBASE_NO_ERROR) return 0;
  
  InfoHog<TYPE> infohog(pod);
 
  FAU_t oa;          // Object Address
  gxBlockHeader blk; // Block Header
  gxObjectHeader oh; // Object Header
  
  int objects = 0; // Keeps track of good database blocks
  int inserts = 0; // Keep track of inserts
  
  FAU_t gxdfileEOF = pod->OpenDataFile()->GetEOF();
  FAU_t addr = (FAU_t)0;
  addr = pod->OpenDataFile()->FindFirstBlock(addr); // Search the entire file

  if(addr == (FAU_t)0) return 0; // No database blocks found in file

  while(1) { 
    if(FAU_t(addr + pod->OpenDataFile()->BlockHeaderSize()) >= gxdfileEOF) 
      break;
    pod->OpenDataFile()->Read(&blk, sizeof(gxBlockHeader), addr);
    if(blk.block_check_word == gxCheckWord) {
      if((__SBYTE__)blk.block_status == gxNormalBlock) {
	oa = addr + pod->OpenDataFile()->BlockHeaderSize();
	ReadObjectHeader(oh, oa);
	if(oh.ClassID == GetClassID()) { 
	  objects++; // Increment the object count
	  infohog.Read(oa);
	  key.SetObjectName(*((TYPE *)infohog.ih_members[0].m_buf()));	
	  key.SetObjectID(oa);
	  key.SetClassID(oh.ClassID);

	  // Could not add the key
	  if(!btx.Insert(key, compare_key)) return 0;
	  inserts++; // Index and data file match
	}
      }
      addr = addr + blk.block_length; // Goto the next database block
    }
    else {
      addr++; // Keep searching until a valid database block is found
    }
  }
  return objects == inserts;
}

template<class TYPE>
int InfoHog<TYPE>::OverWriteObject(FAU_t object_address)
// Overwrite the object at the specified object address. NOTE: This
// function assumes that objects are fixed in length and will not
// overwrite the object's key name. Returns true if successful or 
// false if an error occurs.
{
  FAU_t member_address = object_address + sizeof(gxObjectHeader);
  
  // Seek to the first general-purpose member address
  // Skip over num_members record, key length record,
  // and the data key
  member_address += (sizeof(gxUINT32) * 2);
  member_address += ih_members[0].length();
  pod->OpenDataFile()->SeekTo(member_address);
  if(pod->GetDataFileError() != gxDBASE_NO_ERROR) return 0;

  for(unsigned i = 1; i  < num_members-1; i++) {
    member_address += sizeof(gxUINT32); // Skip length record
    pod->OpenDataFile()->SeekTo(member_address);
    if(pod->GetDataFileError() != gxDBASE_NO_ERROR) return 0;
    gxUINT32 len = (__ULWORD__)ih_members[i].length();
    if(len > (__ULWORD__)0) {
      if(pod->OpenDataFile()->Write(ih_members[i].m_buf(), len) != 
	 gxDBASE_NO_ERROR) {
	return 0;
      }
      member_address += len;   
    }
  }
  return 1;
}

template<class TYPE>
int InfoHog<TYPE>::ChangeObject(InfoHog<TYPE> &ob, int find_object)
// Public member function used to change this object. If the "find_object" 
// variable is true this function will obtain the object's data file address 
// and load the object. Returns true if successful or false if the object 
// could not be changed.
{
  if(find_object) { // Obtain the objects datafile address and load the object
    if(!Find()) return 0; // Could not find the data file entry
    if(!ReadObject()) return 0;
  }
  if(*(this) == ob) return 1; // Nothing to change

  if((this->ObjectLength() != ob.ObjectLength()) || 
     (this->num_members != ob.num_members)) { // Must reallocate this object
  if(!DeleteObject()) return 0; // Could not remove this entry
    this->Copy(ob); // Copy the new object and write it to the file
    return WriteObject();
  }

  // Test key member
  if(this->ih_members[0] != ob.ih_members[0]) {
    if(!ChangeMember(ob.ih_members[0].m_buf(), ob.ih_members[0].length(), 0)) {
      return 0;
    }
  }

  FAU_t member_address = objectaddress + sizeof(gxObjectHeader);
  
  // Seek to the first general-purpose member address
  // Skip over num_members record, key length record,
  // and the data key
  member_address += (sizeof(gxUINT32) * 2);
  member_address += ih_members[0].length();
  pod->OpenDataFile()->SeekTo(member_address);
  if(pod->GetDataFileError() != gxDBASE_NO_ERROR) return 0;
  for(unsigned i = 1; i <= num_members-1; i++) {
    member_address += sizeof(gxUINT32); // Skip length record
    pod->OpenDataFile()->SeekTo(member_address);
    if(pod->GetDataFileError() != gxDBASE_NO_ERROR) return 0;
    gxUINT32 len = (__ULWORD__)ob.ih_members[i].length();
    if((len > (__ULWORD__)0) && (this->ih_members[i] != ob.ih_members[i])) {
      SetMember(ob.ih_members[i].m_buf(), len, i); // Set the member
      if(pod->OpenDataFile()->Write(ih_members[i].m_buf(), len) != 
	 gxDBASE_NO_ERROR) {
	return 0;
      }
      member_address += len;   
    }
  }
  return 1;
}

template<class TYPE>
int InfoHog<TYPE>::ChangeMember(const void *ob, unsigned len, unsigned index,
				int find_object)
// Public member function used to change the object array member at the 
// specified array index. If the "find_object" variable is true this function 
// will obtain the object's data file address and load the object. Returns 
// true if successful or false if the object could not be changed.
{
  if(index == 0) {
    TYPE key(*((TYPE *)ob));
    return ChangeKeyMember(key, find_object);
  }

  if(find_object) { // Obtain the objects data file address and load the object
    if(!Find()) return 0; // Could not find the data file entry
    if(!ReadObject()) return 0;
  }

  if(len != ih_members[index].length()) { // Must reallocate this object
    if(!DeleteObject()) return 0; // Could not remove this entry
    SetMember(ob, len, index); // Set the member
    return WriteObject();
  }

  // Position the file stream to member being changed
  FAU_t member_address = objectaddress + \
    (sizeof(gxObjectHeader) + sizeof(gxINT32));
  for(unsigned i = 0; i < index; i++) {
    member_address += sizeof(gxUINT32); // Skip length record
    member_address += ih_members[i].length();
  }
  pod->OpenDataFile()->SeekTo(member_address);
  if(pod->GetDataFileError() != gxDBASE_NO_ERROR) return 0;
  SetMember(ob, len, index); // Set the member
  member_address += sizeof(gxUINT32); // Skip length record
  pod->OpenDataFile()->SeekTo(member_address);
  if(pod->GetDataFileError() != gxDBASE_NO_ERROR) return 0;
  if(len > (__ULWORD__)0) {
    if(pod->OpenDataFile()->Write(ih_members[index].m_buf(), len) != 
       gxDBASE_NO_ERROR) {
      return 0;
    }
  }
  return 1;
}

template<class TYPE>
int InfoHog<TYPE>::ChangeKeyMember(TYPE &k, int find_object)
// Public member function used to change this object's key name. 
// If the "find_object" variable is true this function will obtain 
// the object's data file address and load the object. Returns true 
// if successful or false if the object could not be changed.
{
  if(find_object) { // Obtain the objects datafile address and load the object
    if(!Find()) return 0; // Could not find the data file entry
    if(!ReadObject()) return 0;
  }
  if(UsingIndex()) { // Update the index file and maintain sort order
    InfoHogKey<TYPE> old_key(*((TYPE *)ih_members[0].m_buf()), 
			     objectaddress, GetClassID());
    InfoHogKey<TYPE> key(k, objectaddress, GetClassID());
    InfoHogKey<TYPE> compare_key;
    if(!DeleteKey(old_key, compare_key)) return 0;
    if(!AddKey(key, compare_key)) return 0;
  }
  
  // Update this member
  SetMember(&k, sizeof(k), 0); // Set the key name
    
  FAU_t member_address = objectaddress + \
    ((sizeof(gxObjectHeader) + (sizeof(gxUINT32)*2)));
  
  // Skip over the object header and the num_members record
  // Skip over the length record assuming that all keys are fixed in length
  pod->OpenDataFile()->SeekTo(member_address);
  if(pod->GetDataFileError() != gxDBASE_NO_ERROR) {
    return 0;
  }
 
  // Write the key member and check for errors
  gxUINT32 len = (__ULWORD__)ih_members[0].length();
  if(len > (__ULWORD__)0) {
    if(pod->OpenDataFile()->Write(ih_members[0].m_buf(), len) != 
       gxDBASE_NO_ERROR) {
      return 0;
    }
  }

  // The object was updated
  return 1;
}

#endif // __GX_INFOHOG_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
