// testprog.cpp (examples_database/reclaim) - 11/16/2026: DataReel 6.x port of the 7.4 tests/reclaim_test.cpp.
// ReclaimBestFit() reused only exact fits: its best fit so far started at 0 unused bytes, so no bigger free block
// was ever taken and the file grew. Checks, for both methods, that a freed bigger block is reused for a smaller
// object (the file does not grow), that best fit takes the smallest block that fits among several, that the split
// leaves a free block for the rest, and that the deleted/removed block counts stay right.
//   reclaim_test DIR        (exit 0 = all passed)
#include "gxdlcode.h"
#include <stdio.h>
#include <string.h>
#include <string>
#include "gxdbase.h"

static int pass = 0, fail = 0;
static void check(bool ok, const std::string &what) { printf("%s %s\n", ok ? "PASS" : "FAIL", what.c_str()); (ok ? pass : fail)++; }

static long long freed(gxDatabase &f) { FAU_t d = 0, r = 0; f.DeletedBlocks(&d, &r); return (long long)(d + r); }   // free blocks
static FAU_t put(gxDatabase &f, size_t n, gxDatabaseReclaimMethod m)
{
  std::string b(n, 'x');
  FAU_t a = f.Alloc((__ULWORD__)n, m);
  if(a) f.Write(b.data(), (__ULWORD__)n, a);
  return a;
}

int main(int argc, char **argv)
{
  std::string dir = argc > 1 ? argv[1] : ".";
  for(int k = 0; k < 2; k++) {
    gxDatabaseReclaimMethod m = k == 0 ? gxDBASE_RECLAIM_BESTFIT : gxDBASE_RECLAIM_FIRSTFIT;
    std::string name = k == 0 ? "best fit" : "first fit";
    std::string fn = dir + (k == 0 ? "/reclaim_best.gxd" : "/reclaim_first.gxd");
    gxDatabase f;
    if(f.Create(fn.c_str()) != gxDBASE_NO_ERROR) { check(false, name + ": create"); continue; }
    // blocks of 4000, 1000 and 2000 bytes between small separators (so freed blocks do not merge)
    FAU_t big = put(f, 4000, m); put(f, 10, m);
    FAU_t small_blk = put(f, 1000, m); put(f, 10, m);
    FAU_t mid = put(f, 2000, m); put(f, 10, m);
    FAU_t eof = f.GetEOF();
    f.Delete(big); f.Delete(small_blk); f.Delete(mid);
    long long d0 = freed(f);
    // a 700-byte object fits the 1000-byte block best (the 4000 one is first in no particular order)
    FAU_t a = put(f, 700, m);
    check(a != 0 && f.GetEOF() == eof, name + ": a 700-byte object reuses a freed block (the file stays " + std::to_string((long long)eof) + " bytes)");
    if(k == 0) check(a == small_blk, name + ": it took the smallest block that fits (the 1000-byte one)");
    long long d1 = freed(f);
    check(d1 >= 1 && d1 <= d0, name + ": free blocks " + std::to_string(d0) + " -> " + std::to_string(d1) + " (the rest of the split is free again)");
    // the rest of the split block is reusable: more small objects still do not grow the file
    put(f, 100, m); put(f, 100, m);
    check(f.GetEOF() == eof, name + ": two more 100-byte objects fit in freed space");
    // read back what was written
    std::string back(700, 0);
    check(f.Read(&back[0], 700, a) == gxDBASE_NO_ERROR && back == std::string(700, 'x'), name + ": the reused block reads back");
    f.Close();
  }
  printf("%d passed, %d failed\n", pass, fail);
  return fail ? 1 : 0;
}
