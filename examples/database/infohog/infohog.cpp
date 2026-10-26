// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: infohog.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 10/26/2026
// Date Last Modified: 10/26/2026
// Copyright (c) 2001-2026 DataReel Software Development
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

InfoHog object database demo. Each record is an InfoHog object: a
fixed-length key in member 0 and any number of variable-length
members after it. Records are stored in a gxDatabase data file and
indexed by key in a gxBtree index file, both opened through POD.
The demo adds contacts, finds one by key, changes a member, deletes
a record, lists the index in key order and reopens the files to show
the data persisted.

Usage: infohog [data_file] [index_file]
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <cstring>
#include <iostream>
#include <string>
#include "infohog.h"

// Fixed-length key. InfoHog keys are stored in the index as raw
// bytes, so they must not hold pointers (no std::string).
struct ContactName
{
  ContactName() { memset(name, 0, sizeof(name)); }
  ContactName(const char *s) {
    memset(name, 0, sizeof(name));
    strncpy(name, s, sizeof(name) - 1);
  }
  int operator==(const ContactName &o) const { return strcmp(name, o.name) == 0; }
  int operator<(const ContactName &o) const { return strcmp(name, o.name) < 0; }
  int operator>(const ContactName &o) const { return strcmp(name, o.name) > 0; }

  char name[64];
};

typedef InfoHog<ContactName> Contact;
typedef InfoHogKey<ContactName> ContactKey;

// Member numbers. Member 0 is always the key.
enum { NAME = 0, EMAIL = 1, PHONE = 2, NOTES = 3, NUM_MEMBERS = 4 };

static void set_text(Contact &c, unsigned member, const std::string &s)
{
  c.SetMember(s.c_str(), (unsigned)s.size() + 1, member); // Keep the null
}

static std::string get_text(Contact &c, unsigned member)
{
  const char *p = (const char *)c.GetMember(member);
  unsigned len = c.GetMemberLen(member);
  if(!p || len == 0) return std::string();
  return std::string(p, strnlen(p, len));
}

static int open_pod(POD &pod, const char *data_file, const char *index_file)
{
  ContactKey key_type;
  gxDatabaseError err = pod.Open(data_file, index_file, key_type,
				 InfoHogNodeOrder, gxDBASE_READWRITE,
				 InfoHogUseIndexFile, InfoHogStaticArea,
				 InfoHogNumTrees);
  if(err != gxDBASE_NO_ERROR) {
    std::cerr << "Cannot open the database: " 
	      << gxDatabaseExceptionMessage(err) << "\n";
    return -1;
  }
  return 0;
}

static int add_contact(POD &pod, const char *name, const char *email,
		       const char *phone, const char *notes)
{
  ContactName key(name);
  Contact c(&pod, key, NUM_MEMBERS);
  set_text(c, EMAIL, email);
  set_text(c, PHONE, phone);
  set_text(c, NOTES, notes);
  if(c.FindObject()) { // Keys are unique
    std::cout << "Not added, " << name << " is already in the database\n";
    return -1;
  }
  if(!c.WriteObject(0)) {
    std::cout << "Could not add " << name << "\n";
    return -1;
  }
  std::cout << "Added " << name << " at address " 
	    << (long long)c.ObjectAddress() << "\n";
  return 0;
}

static int find_contact(POD &pod, const char *name, Contact &c)
{
  ContactName key(name);
  Contact probe(&pod, key, NUM_MEMBERS);
  if(!probe.FindObject()) return -1;
  if(!c.ReadObject(probe.ObjectAddress())) return -1;
  return 0;
}

static void list_contacts(POD &pod)
{
  ContactKey key, compare_key;
  gxBtree *btx = pod.Index();
  std::cout << "Contacts in key order:\n";
  if(!btx->FindFirst(key)) {
    std::cout << "  (none)\n";
    return;
  }
  do {
    Contact c(&pod, NUM_MEMBERS);
    if(c.ReadObject(key.ObjectID())) {
      std::cout << "  " << key.ObjectName().name << " <" 
		<< get_text(c, EMAIL) << "> " << get_text(c, PHONE) << "\n";
    }
  } while(btx->FindNext(key, compare_key));
}

int main(int argc, char **argv)
{
  const char *data_file = argc > 1 ? argv[1] : "contacts.ihd";
  const char *index_file = argc > 2 ? argv[2] : "contacts.ihx";

  POD pod;
  if(open_pod(pod, data_file, index_file) != 0) return 1;

  add_contact(pod, "Carol Smith", "carol@example.com", "555-0103", "Sales");
  add_contact(pod, "Alice Jones", "alice@example.com", "555-0101", "Engineering");
  add_contact(pod, "Bob Brown", "bob@example.com", "555-0102", "Support");
  add_contact(pod, "Alice Jones", "alice2@example.com", "555-0199", "Duplicate");
  list_contacts(pod);

  Contact c(&pod, NUM_MEMBERS);
  if(find_contact(pod, "Bob Brown", c) != 0) {
    std::cerr << "Bob Brown not found\n";
    return 1;
  }
  std::cout << "Found Bob Brown: notes = " << get_text(c, NOTES) << "\n";

  // Change one member in place. The record grows, so InfoHog moves it.
  std::string notes = "Support team lead since 2026";
  if(!c.ChangeMember(notes.c_str(), (unsigned)notes.size() + 1, NOTES)) {
    std::cerr << "ChangeMember failed\n";
    return 1;
  }
  std::cout << "Changed Bob Brown's notes\n";

  ContactName gone("Carol Smith");
  Contact del(&pod, gone, NUM_MEMBERS);
  if(del.FindObject() && del.DeleteObject()) {
    std::cout << "Deleted Carol Smith\n";
  }
  list_contacts(pod);
  pod.Close();

  // Reopen the files to show the changes were saved
  std::cout << "Reopening the database\n";
  POD pod2;
  if(open_pod(pod2, data_file, index_file) != 0) return 1;
  Contact again(&pod2, NUM_MEMBERS);
  if(find_contact(pod2, "Bob Brown", again) != 0) {
    std::cerr << "Bob Brown not found after reopening\n";
    return 1;
  }
  std::cout << "Bob Brown's notes: " << get_text(again, NOTES) << "\n";
  list_contacts(pod2);
  pod2.Close();
  return get_text(again, NOTES) == notes ? 0 : 1;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
