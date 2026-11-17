// testprog.cpp (examples_database/journal_salt) - 11/16/2026: DataReel 6.x port of the 7.4 tests/journal_salt_test.cpp.
// the same second. The salt was time(0), a counter and the journal object's address, all equal in such children; a
// recovery then took the previous session's records (left after a cleared header) for its own and replayed old
// transactions (found by the gxObjRepl property test: a copy went back from applied 4 to 3).
//   journal_salt_test DIR      (exit 0 = all salts distinct)
#include "gxdlcode.h"
#include <stdio.h>
#include <stdint.h>
#include <string>
#include <set>
#include <unistd.h>
#include <sys/wait.h>
#include "gxdbase.h"
#include "gxdjrnl.h"

int main(int argc, char **argv)
{
  std::string dir = argc > 1 ? argv[1] : ".";
  const int N = 20;
  gxdJournal *j = new gxdJournal;                 // the same address in every child (forked after this)
  for(int i = 0; i < N; i++) {
    if(fork() == 0) {
      std::string db = dir + "/d" + std::to_string(i) + ".gxd", jn = dir + "/j" + std::to_string(i) + ".jnl";
      gxDatabase f;
      if(j->Open(jn.c_str()) != gxDBASE_NO_ERROR || f.Create(db.c_str()) != gxDBASE_NO_ERROR || j->Attach(&f) != gxDBASE_NO_ERROR) _exit(2);
      j->Begin(); f.Alloc(16); j->Commit(0);      // writes the journal header (with its salt); Commit(0) keeps it in the journal
      _exit(0);                                    // no Close(): the header stays as written
    }
  }
  int bad = 0;
  for(int i = 0; i < N; i++) { int st = 0; wait(&st); if(!WIFEXITED(st) || WEXITSTATUS(st) != 0) bad++; }
  std::set<uint32_t> salts;
  for(int i = 0; i < N; i++) {
    std::string jn = dir + "/j" + std::to_string(i) + ".jnl";
    FILE *f = fopen(jn.c_str(), "rb"); unsigned char h[16] = {0};
    if(!f || fread(h, 1, 16, f) != 16) { bad++; if(f) fclose(f); continue; }
    fclose(f);
    uint32_t s = (uint32_t)h[12] | ((uint32_t)h[13] << 8) | ((uint32_t)h[14] << 16) | ((uint32_t)h[15] << 24);
    salts.insert(s);
  }
  printf("%d children, %zu distinct salts, %d errors\n", N, salts.size(), bad);
  printf("%s journal salts\n", (salts.size() == (size_t)N && !bad) ? "PASS" : "FAIL");
  return (salts.size() == (size_t)N && !bad) ? 0 : 1;
}
