# The DataReel 7 object database

DataReel's database is an embedded library: your program opens files
directly, there is no server and no SQL. It suits programs that store
their own records locally and need them to survive crashes and
upgrades: configuration and state stores, message and event logs,
catalogs, caches, and data exchanged between programs as files. File
formats have not changed since 2001.

Pick a layer:

| Need | Use | Header |
|------|-----|--------|
| Store and reuse variable size blocks by address | `gxDatabase` | `gxdbase.h` |
| Sorted index of fixed-size keys on disk | `gxBtree` with your `DatabaseKeyB` class | `gxbtree.h` |
| A data file and its indexes opened together | `POD` | `pod.h` |
| Objects with a key and variable length members | `InfoHog<KeyType>` | `infohog.h` |
| Tables with typed columns, keys, search and sort | `gxRDBMS` | `gxrdbms.h` |
| Faster repeated reads | `gxDatabaseCache`, `BtreeCache` | `dbfcache.h`, `btcache.h` |
| Check and repair files | `db_debug`, `ix_debug` utilities | `utils/` |

## Concepts

- A database file (`.gxd` by convention) is a header followed by
  blocks. `Alloc(n)` returns the **object address** (`FAU_t`) of a
  block that holds at least n bytes, reusing deleted blocks first.
  `Write(buf, n, addr)` and `Read(buf, n, addr)` use that address.
  Addresses stay valid until the object is deleted, so indexes store
  them.
- Each block has a header with a status (normal, deleted, removed)
  and, in revision A and later files, an optional CRC
  (`WriteObjectChecksum`, `ReadObjectChecksum`). New files are revision
  E, which also supports file and record locks.
- Records are raw bytes. Use fixed-size types (`char[N]`, `gxINT32`,
  `gxUINT32`, `gxINT64`, `gxFLOAT64`, `CDate`, `FAU`), never pointers,
  `std::string` or virtual functions. With
  `DATAREEL_NATIVE_TYPES=OFF` the gx types are byte-order-portable
  classes and files can move between Linux and Windows.
- `gxDatabase`, `gxBtree`, `POD` and `gxRDBMS` are not copyable. Use
  one object per file per thread, or guard it with a `std::mutex`.
  `LockFile()`/`LockRecord()` coordinate several processes.
- Error codes are `gxDatabaseError`; `gxDBASE_NO_ERROR` is 0.
  `DatabaseExceptionMessage()` (member) or
  `gxDatabaseExceptionMessage(err)` gives the text.

## Records in a block file

<!-- sample: docs/ai/samples/record_store.cpp -->
```cpp
// docs/ai/samples/record_store.cpp
// DataReel 7 sample: store, read, walk and delete fixed-layout records
// in a gxDatabase block file.
// Usage: record_store [file.gxd]
#include <cstring>
#include <iostream>
#include "gxdbase.h"
#include "gxfloat.h"

using namespace datareel;

// Records are written as raw bytes, so use fixed-size members: char
// arrays and the gx integer and float types. No pointers, std::string
// or virtual functions.
struct Part
{
  char name[32];
  gxINT32 quantity;
  gxFLOAT64 price;
};

static Part make_part(const char *name, long qty, double price)
{
  Part p;
  memset(&p, 0, sizeof(p)); // Zero the padding too
  strncpy(p.name, name, sizeof(p.name) - 1);
  p.quantity = (gxINT32)qty;
  p.price = price;
  return p;
}

int main(int argc, char **argv)
{
  const char *fname = argc > 1 ? argv[1] : "parts.gxd";
  gxDatabase db;
  // Open() and Create() return gxDBASE_NO_ERROR (0) on success
  if(db.Open(fname) != gxDBASE_NO_ERROR && 
     db.Create(fname) != gxDBASE_NO_ERROR) {
    std::cerr << fname << ": " << db.DatabaseExceptionMessage() << "\n";
    return 1;
  }

  // Alloc() returns the address of a new (or reused) block, 0 on error
  Part parts[] = { make_part("bolt", 500, 0.10), make_part("nut", 800, 0.05),
		   make_part("washer", 1200, 0.02) };
  FAU_t nut_addr = 0;
  for(const Part &p : parts) {
    FAU_t addr = db.Alloc(sizeof(Part));
    if(addr == (FAU_t)0 || db.Write(&p, sizeof(Part), addr) != gxDBASE_NO_ERROR) {
      std::cerr << "Write failed: " << db.DatabaseExceptionMessage() << "\n";
      return 1;
    }
    db.WriteObjectChecksum(addr); // Optional CRC for this record
    if(strcmp(p.name, "nut") == 0) nut_addr = addr;
  }

  // Read one record back by its address
  Part p;
  if(db.Read(&p, sizeof(Part), nut_addr) != gxDBASE_NO_ERROR) return 1;
  std::cout << "At " << (long long)nut_addr << ": " << p.name << "\n";

  // Delete it. The block goes on the free list and the next Alloc()
  // of the same size or smaller reuses it.
  db.Delete(nut_addr);

  // Walk every record that is not deleted
  for(FAU_t addr = db.FindFirstObject(); addr != (FAU_t)0; 
      addr = db.FindNextObject(addr)) {
    if(db.ObjectLength(addr) != sizeof(Part)) continue; // Not a Part
    if(db.Read(&p, sizeof(Part), addr) != gxDBASE_NO_ERROR) break;
    std::cout << (long long)addr << " " << p.name << " qty " 
	      << (long)p.quantity << " price " << (double)p.price << "\n";
  }
  db.Close();
  return 0;
}
```
<!-- end sample -->

Useful calls: `ObjectLength(addr)`, `UnDelete(addr)`,
`Delete(addr, 1)` (remove permanently), `TotalBlocks()`,
`DeletedBlocks()`, `Flush()`, `Open(name, gxDBASE_READONLY)`.
`FindFirstObject()`, `FindNextObject(addr)`, `FindPrevObject(addr)`
walk the objects in file order, skipping deleted ones.

## Records with an index

A key class derives from `DatabaseKeyB`, keeps its data in one
fixed-size struct, passes that struct's address to the base class,
and implements `KeySize()` and the compare functions. Store the
record's address in the key so a lookup leads to the record. `POD`
opens (or creates) the data file and the index file together; it
needs the key class only as a type sample.

<!-- sample: docs/ai/samples/indexed_records.cpp -->
```cpp
// docs/ai/samples/indexed_records.cpp
// DataReel 7 sample: a data file plus a B-tree index (POD) with a
// custom key class. Records are found by name and listed in key order.
// Usage: indexed_records [data.gxd index.btx]
#include <cstring>
#include <iostream>
#include "pod.h"

using namespace datareel;

struct Customer
{
  char name[48];
  char email[64];
  gxINT32 orders;
};

// The index entry: a fixed-size key plus the record's address. Only
// the name is compared, so a lookup by name returns the address.
struct CustomerKeyData
{
  char name[48];
  FAU address;
};

class CustomerKey : public DatabaseKeyB
{
public:
  CustomerKey(const char *name = "", FAU_t addr = 0) : DatabaseKeyB(&data) {
    memset(&data, 0, sizeof(data));
    strncpy(data.name, name, sizeof(data.name) - 1);
    data.address = addr;
  }
  FAU_t Address() const { return (FAU_t)data.address; }
  const char *Name() const { return data.name; }

public: // DatabaseKeyB interface
  size_t KeySize() override { return sizeof(data); }
  // The CMake build defines __USE_SINGLE_COMPARE__, which uses
  // CompareKey(). Defining the dual compare functions as well keeps the
  // class working with either setting.
  int CompareKey(const DatabaseKeyB &key) const {
    return strcmp(data.name, ((const CustomerKeyData *)key.db_key)->name);
  }
  int operator==(const DatabaseKeyB &key) const { return CompareKey(key) == 0; }
  int operator>(const DatabaseKeyB &key) const { return CompareKey(key) > 0; }

private:
  CustomerKeyData data;
};

const BtreeNodeOrder_t ORDER = 31; // Keys per node + 1

static int add(POD &pod, const char *name, const char *email, long orders)
{
  CustomerKey key(name), found;
  if(pod.Index()->Find(key, found)) return -1; // Names are unique
  Customer c;
  memset(&c, 0, sizeof(c));
  strncpy(c.name, name, sizeof(c.name) - 1);
  strncpy(c.email, email, sizeof(c.email) - 1);
  c.orders = (gxINT32)orders;
  gxDatabase *db = pod.OpenDataFile();
  FAU_t addr = db->Alloc(sizeof(c));
  if(addr == (FAU_t)0 || db->Write(&c, sizeof(c), addr) != gxDBASE_NO_ERROR)
    return -1;
  CustomerKey entry(name, addr), compare;
  return pod.Index()->Insert(entry, compare) ? 0 : -1;
}

int main(int argc, char **argv)
{
  const char *dfile = argc > 2 ? argv[1] : "customers.gxd";
  const char *ifile = argc > 2 ? argv[2] : "customers.btx";
  CustomerKey key_type;
  POD pod;
  // Opens both files, creating them if they do not exist
  gxDatabaseError err = pod.Open(dfile, ifile, key_type, ORDER);
  if(err != gxDBASE_NO_ERROR) {
    std::cerr << gxDatabaseExceptionMessage(err) << "\n";
    return 1;
  }
  add(pod, "Wilson Supply", "orders@wilson.example", 12);
  add(pod, "Acme Tools", "buy@acme.example", 40);
  add(pod, "Baker Hardware", "info@baker.example", 7);

  // Look up one record by name. Find() fills in the stored key,
  // including the record address.
  CustomerKey key("Acme Tools"), compare;
  Customer c;
  if(pod.Index()->Find(key, compare) &&
     pod.OpenDataFile()->Read(&c, sizeof(c), key.Address()) == gxDBASE_NO_ERROR) {
    std::cout << "Found " << c.name << " <" << c.email << ">\n";
  }

  // List every record in name order
  CustomerKey k;
  if(pod.Index()->FindFirst(k)) {
    do {
      pod.OpenDataFile()->Read(&c, sizeof(c), k.Address());
      std::cout << "  " << c.name << ", " << (long)c.orders << " orders\n";
    } while(pod.Index()->FindNext(k, compare));
  }

  // Delete: remove the index entry, then the record
  CustomerKey gone("Baker Hardware");
  if(pod.Index()->Find(gone, compare)) {
    FAU_t addr = gone.Address();
    pod.Index()->Delete(gone, compare);
    pod.OpenDataFile()->Delete(addr);
  }
  pod.Close();
  return 0;
}
```
<!-- end sample -->

Notes:

- `Find(key, compare)` fills `key` with the stored key when it matches,
  so compare only the fields you search by and read the rest (the
  address) back from `key`.
- `Insert()` does not reject duplicates on its own; `Find()` first if
  keys must be unique.
- The node order (keys per node + 1) is fixed when the index is
  created; reopen with the same order and key size.
- `FindFirst`/`FindNext`/`FindPrev`/`FindLast` iterate in key order.
  `PartialFind()` finds the first key equal to or greater than a
  value, for prefix and range scans.
- An index file can hold several trees, and a `POD` can have several
  index files (`POD::Open` with an array of file names). When an
  index file did not exist and `POD::Open` created it empty,
  `pod.RebuildIndex()` returns true and the program must insert the
  keys again from the data file.
- `gxBtree` can be used alone: `gxBtree btx(key_type, order);
  btx.Create("file.btx")` or `btx.Open("file.btx")`.

## Persistent objects (InfoHog)

`InfoHog<Key>` stores an object made of a key (member 0) plus any
number of variable length members, with the key in a B-tree index.
Good for documents, contacts or settings whose fields change size.

<!-- sample: examples/database/infohog/infohog.cpp from="#include" -->
```cpp
// examples/database/infohog/infohog.cpp (license header omitted)
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
```
<!-- end sample -->

## Relational tables (gxRDBMS)

`gxRDBMS` creates a table from a text definition: a table name and
columns written `Name{display width}TYPE[size]`, plus a key
definition. The types are `INT16`, `INT32`, `INT64`, `FLOAT`,
`BOOL`, `CHAR[n]`, `VCHAR[n]`, `BINARY`, `BLOB`, `CURRENCY` and `DATE`. Records are `gxrdDatabaseRecord` objects whose
fields are set and read by zero-based column number; `Search()`
and `SortOrderList()` fill a `std::set<gxrdDatabaseRecord>`. It has no
SQL and no joins; use SQLite when you need those.

<!-- sample: examples/database/rdbms/rdbms.cpp from="#include" -->
```cpp
// examples/database/rdbms/rdbms.cpp (license header omitted)
#include "gxdlcode.h"

#include <cstring>
#include <iostream>
#include <set>
#include <string>
#include "gxrdbms.h"

static void print_record(gxrdDatabaseRecord &r)
{
  gxINT32 id = 0;
  gxFLOAT64 salary = 0;
  CDate hired;
  r.GetField(&id, sizeof(id), 0);
  r.GetField(&salary, sizeof(salary), 3);
  r.GetField(&hired, sizeof(hired), 4);
  std::cout << "  " << (long)id << "  " << (const char *)r.GetField(1)
	    << ", " << (const char *)r.GetField(2) << ", "
	    << (double)salary << ", hired " << hired.str() << "\n";
}

static int add_employee(gxRDBMS &db, long id, const char *name,
			const char *dept, double salary, const char *hired)
{
  gxrdDatabaseRecord rec(db.RecordDef());
  gxINT32 i = (gxINT32)id;
  gxFLOAT64 s = salary;
  CDate d(hired);
  rec.SetField(&i, sizeof(i), 0);
  rec.SetField(name, (__ULWORD__)strlen(name) + 1, 1); // Variable length
  rec.SetField(dept, (__ULWORD__)strlen(dept) + 1, 2);
  rec.SetField(&s, sizeof(s), 3);
  rec.SetField(&d, sizeof(d), 4);
  if(!db.AddRecord(&rec)) {
    std::cout << "Not added: " << name << " ("
	      << db.GetDatabase()->DataFileExceptionMessage() << ")\n";
    return -1;
  }
  return 0;
}

int main(int argc, char **argv)
{
  // Columns: ID, Name (primary key), Department (secondary key),
  // Salary, Hire date. {n} is the display width and [n] the size.
  std::string tdef = "employees, ID{6}INT32, Name{30}VCHAR[64], "
    "Department{20}CHAR[32], Salary{10}FLOAT, Hired{10}DATE";
  // ALLOWDUP lets several records share a department key value
  std::string kdef = "PRI: COL 2, SEC: COL 3, ATTRIB: NOCASE, ATTRIB: ALLOWDUP";

  gxRDBMS db;
  gxDatabaseError err = argc > 1 ? db.CreateTable(tdef, kdef, argv[1]) : 
    db.CreateTable(tdef, kdef);
  if(err != gxDBASE_NO_ERROR) {
    std::cerr << gxDatabaseExceptionMessage(err) << "\n";
    return 1;
  }
  std::cout << "Record definition: " << db.RecordDef() << "\n";

  if(db.IsEmpty()) {
    add_employee(db, 101, "Alice Jones", "Engineering", 125000.0, "3/14/2019");
    add_employee(db, 102, "Bob Brown", "Support", 72000.0, "6/1/2021");
    add_employee(db, 103, "Carol Smith", "Sales", 88000.0, "11/20/2017");
    add_employee(db, 104, "Dave Lee", "Engineering", 118000.0, "1/9/2023");
    add_employee(db, 105, "Erin Diaz", "Support", 69000.0, "8/30/2024");
  }
  std::cout << db.NumRecords() << " records\n";

  // Find by primary key. Only the key field is needed and NOCASE
  // makes the match ignore case.
  gxrdDatabaseRecord found(db.RecordDef());
  found.SetField("BOB BROWN", 10, 1);
  FAU_t addr;
  if(!db.FindRecord(&found, addr)) {
    std::cerr << "Bob Brown not found\n";
    return 1;
  }
  std::cout << "Found:\n";
  print_record(found);

  // Change the salary
  gxrdDatabaseRecord changed(found);
  gxFLOAT64 raise = 76000.0;
  changed.SetField(&raise, sizeof(raise), 3);
  if(!db.ChangeRecord(&found, &changed)) {
    std::cerr << "Change failed\n";
    return 1;
  }
  std::cout << "Changed Bob Brown's salary\n";

  // Search the department index (one-based field 3) for Engineering
  std::set<gxrdDatabaseRecord> matches;
  std::string dept = "engineering";
  // Arguments: index 1 (the secondary key), one-based field 3, no case
  // compare, no alphanumeric compare, exact match, at most every record
  err = db.Search(matches, dept, 1, 3, 0, 0, 0, db.NumRecords());
  if(err != gxDBASE_NO_ERROR) {
    std::cerr << "Search failed: " << gxDatabaseExceptionMessage(err) << "\n";
    return 1;
  }
  std::cout << "Engineering (" << matches.size() << "):\n";
  for(const auto &m : matches) {
    gxrdDatabaseRecord r(m);
    print_record(r);
  }

  // List the whole table in primary key order
  std::set<gxrdDatabaseRecord> all;
  FAU_t n = db.NumRecords();
  db.SortOrderList(all, n, 1, n); // Limit, first and last record
  std::cout << "All employees:\n";
  for(const auto &m : all) {
    gxrdDatabaseRecord r(m);
    print_record(r);
  }
  db.Close();
  return matches.size() == 2 ? 0 : 1;
}
```
<!-- end sample -->

## Checking and repairing files

- `db_debug file.gxd` analyzes a database file, reports damaged
  blocks and can rebuild a damaged file.
- `ix_debug file.btx` checks a B-tree index file.
- `examples/database/defrag` shows first-fit and best-fit reuse of
  deleted blocks.
- The library checks lengths and counts it reads from files, so a
  damaged file returns an error instead of crashing (see
  `tests/damaged_files.cpp`).

## Mistakes to avoid

- Putting `std::string` or pointers in a record or key: the file then
  holds memory addresses. Use `char[N]` or an InfoHog member.
- Opening files with a different `DATAREEL_64BIT_DATABASE` or
  `DATAREEL_NATIVE_TYPES` setting than the one that created them.
- Forgetting to `memset` records and keys: padding bytes then hold
  random memory and identical records differ on disk.
- Deleting a record but not its index entry (or the reverse).
- Implementing only `operator==`/`operator>` in a key class: the CMake
  build uses `CompareKey()`.
