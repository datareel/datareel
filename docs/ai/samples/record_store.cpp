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
