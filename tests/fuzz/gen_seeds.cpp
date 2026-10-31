// gen_seeds.cpp - seed files for fuzz_db: a small data file, its index
// and a journal holding two committed transactions (copied before the
// journal is checkpointed). Usage: gen_seeds DIR
#include "gxdlcode.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include "gxbtree.h"
#include "gxdjrnl.h"

using namespace datareel;
namespace fs = std::filesystem;

struct SdKeyData { char k[16]; FAU addr; };
class SdKey : public DatabaseKeyB {
public:
  SdKey() : DatabaseKeyB((char *)&d) { memset(&d, 0, sizeof(d)); }
  size_t KeySize() { return sizeof(d); }
  int CompareKey(const DatabaseKeyB &o) const {
    return memcmp(d.k, ((const SdKeyData *)o.db_key)->k, 16);
  }
  int operator==(const DatabaseKeyB &o) const { return CompareKey(o) == 0; }
  int operator>(const DatabaseKeyB &o) const { return CompareKey(o) > 0; }
  SdKeyData d;
};

int main(int argc, char **argv)
{
  std::string dir = argc > 1 ? argv[1] : ".";
  fs::create_directories(dir + "/db");
  std::string gxd = dir + "/db/seed.gxd", btx_name = dir + "/db/seed.btx";
  gxdJournal j;
  j.Open((dir + "/db/seed.jnl").c_str());
  j.SetWriteMode(gxdJOURNAL_WRITE_BACK);
  gxDatabase db;
  SdKey type;
  gxBtree btx(type, 16);
  db.Create(gxd.c_str());
  btx.Create(btx_name.c_str());
  j.Attach(&db);
  j.Attach(&btx);
  for(int b = 0; b < 2; b++) {
    j.Begin();
    for(int i = 0; i < 150; i++) {
      char rec[64];
      int id = b * 150 + i;
      snprintf(rec, sizeof(rec), "%016d record %d", id, id);
      FAU_t a = db.Alloc(sizeof(rec));
      db.Write(rec, sizeof(rec), a);
      SdKey key, cmp;
      memcpy(key.d.k, rec, 16);
      key.d.addr = a;
      btx.Insert(key, cmp);
    }
    j.Commit(1);
  }
  fs::create_directories(dir + "/jnl");
  fs::copy_file(dir + "/db/seed.jnl", dir + "/jnl/seed.jnl",
		fs::copy_options::overwrite_existing);
  j.Close();
  btx.Close();
  db.Close();
  fs::create_directories(dir + "/gxd");
  fs::create_directories(dir + "/btx");
  fs::copy_file(gxd, dir + "/gxd/seed.gxd", fs::copy_options::overwrite_existing);
  fs::copy_file(btx_name, dir + "/btx/seed.btx", fs::copy_options::overwrite_existing);
  printf("seeds in %s/{gxd,btx,jnl}\n", dir.c_str());
  return 0;
}
