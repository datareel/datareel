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
