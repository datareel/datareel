// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: testprog.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/16/2026
// Date Last Modified: 11/16/2026
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

6.4 test: database files written with the other on-disk layout are
refused with gxDBASE_FILE_LAYOUT_ERROR. With native integer types a
gxUINT32 is 8 bytes on 64-bit Linux and 4 bytes on Windows and in
32-bit builds, so the headers differ and such a file used to open and
read as empty.

  same_layout   A file made by this build, with and without objects,
                and its B-tree index, open as before.
  other_sig     A header with the signature where the other layout
                puts it (the 32-bit engine's difference) is refused.
  other_block   A file whose first block header has the other layout
                (the 64-bit engine's difference) is refused.
  not_database  A file that is not a database file still gives
                gxDBASE_WRONG_FILE_TYPE.

Usage: testprog [check DIR]
  With no arguments the tests above run in the current directory.
  "check DIR" opens DIR/t.gxd and DIR/t.btx, made on another platform
  and prints what Open() says.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <string.h>
#include "gxdbase.h"
#include "gxbtree.h"

static int failures = 0;

static void check(int ok, const char *what)
{
  printf("%s %s\n", ok ? "PASS" : "FAIL", what);
  if(!ok) failures++;
}

class IntKey : public DatabaseKeyB {
public:
  IntKey(gxINT32 v = 0) : DatabaseKeyB((char *)&key) { key = v; }
  size_t KeySize() { return sizeof(key); }
  int operator==(const DatabaseKeyB &k) const { return key == *((gxINT32 *)k.db_key); }
  int operator>(const DatabaseKeyB &k) const { return key > *((gxINT32 *)k.db_key); }
  int CompareKey(const DatabaseKeyB &k) const {
    gxINT32 o = *((gxINT32 *)k.db_key);
    return key < o ? -1 : (key > o ? 1 : 0);
  }
  gxINT32 key;
};

static long file_size(const char *fname)
{
  FILE *f = fopen(fname, "rb");
  if(!f) return -1;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fclose(f);
  return n;
}

static int read_file(const char *fname, unsigned char *buf, long n)
{
  FILE *f = fopen(fname, "rb");
  if(!f) return -1;
  long got = (long)fread(buf, 1, (size_t)n, f);
  fclose(f);
  return got == n ? 0 : -1;
}

static int write_file(const char *fname, const unsigned char *buf, long n)
{
  FILE *f = fopen(fname, "wb");
  if(!f) return -1;
  long put = (long)fwrite(buf, 1, (size_t)n, f);
  fclose(f);
  return put == n ? 0 : -1;
}

static gxDatabaseError open_ro(const char *fname)
{
  gxDatabase db;
  gxDatabaseError err = db.Open(fname, gxDBASE_READONLY);
  if(err == gxDBASE_NO_ERROR) db.Close();
  return err;
}

static int run_tests()
{
  const char *text = "an object written by this build";
  { // same_layout
    gxDatabase db;
    if(db.Create("t.gxd") != gxDBASE_NO_ERROR) { printf("create failed\n"); return 1; }
    FAU_t a = db.Alloc((__ULWORD__)strlen(text) + 1);
    db.Write(text, (__ULWORD__)strlen(text) + 1, a);
    db.Close();
    gxDatabase e;
    e.Create("empty.gxd");
    e.Close();
    IntKey k;
    gxBtree bt(k, 7);
    bt.Create("t.btx");
    for(int i = 0; i < 20; i++) { IntKey ik(i), c; bt.Insert(ik, c); }
    bt.Close();
  }
  check(open_ro("t.gxd") == gxDBASE_NO_ERROR, "same_layout: data file opens");
  check(open_ro("empty.gxd") == gxDBASE_NO_ERROR,
	"same_layout: file with no objects opens");
  {
    IntKey k, c;
    gxBtree bt(k, 7);
    int ok = bt.Open("t.btx", gxDBASE_READONLY) == gxDBASE_NO_ERROR;
    int n = 0;
    if(ok && bt.FindFirst(k)) { do { n++; } while(bt.FindNext(k, c)); }
    bt.Close();
    check(ok && n == 20, "same_layout: B-tree index opens with its 20 keys");
  }

  long n = file_size("t.gxd");
  unsigned char *buf = new unsigned char[n + 64];
  memset(buf, 0, n + 64);
  if(read_file("t.gxd", buf, n) != 0) { printf("read failed\n"); return 1; }

  { // other_sig: the signature where the other layout puts it
    unsigned char *o = new unsigned char[n + 64];
    memcpy(o, buf, n);
    size_t here = 4 * sizeof(FAU), other = (sizeof(FAU) == 8) ? 16 : 32;
    unsigned char sig[gxSignatureSize];
    memcpy(sig, buf + here, gxSignatureSize);
    memset(o + 16, 0, 32);
    memcpy(o + other, sig, gxSignatureSize);
    write_file("other_sig.gxd", o, n);
    delete[] o;
    gxDatabaseError err = open_ro("other_sig.gxd");
    printf("  other_sig: %s\n", gxDatabaseExceptionMessage(err));
    check(err == gxDBASE_FILE_LAYOUT_ERROR,
	  "other_sig: signature at the other layout's offset is refused");
  }

#if defined (__64_BIT_DATABASE_ENGINE__)
  { // other_block: the 4 bytes after the first block's check word
    gxDatabase db;
    db.Open("t.gxd", gxDBASE_READONLY);
    FAU_t hs = db.GetHeapStart();
    db.Close();
    unsigned char *o = new unsigned char[n + 64];
    memcpy(o, buf, n);
    if(sizeof(gxUINT32) == 8) { // 4-byte longs: the block length follows
      o[hs + 4] = 0x40; o[hs + 5] = o[hs + 6] = o[hs + 7] = 0;
    }
    else {                      // 8-byte longs: 4 zero bytes follow
      o[hs + 4] = o[hs + 5] = o[hs + 6] = o[hs + 7] = 0;
    }
    write_file("other_block.gxd", o, n);
    delete[] o;
    gxDatabaseError err = open_ro("other_block.gxd");
    printf("  other_block: %s\n", gxDatabaseExceptionMessage(err));
    check(err == gxDBASE_FILE_LAYOUT_ERROR,
	  "other_block: first block in the other layout is refused");
  }
#else
  printf("  other_block: 32-bit engine, the signature offset tells\n");
#endif

  { // not_database
    const char *junk = "this is not a database file, just some text....."
      "..................................................";
    write_file("junk.gxd", (const unsigned char *)junk, (long)strlen(junk));
    check(open_ro("junk.gxd") == gxDBASE_WRONG_FILE_TYPE,
	  "not_database: still gxDBASE_WRONG_FILE_TYPE");
  }
  delete[] buf;

  printf("%d failure(s)\n", failures);
  return failures ? 1 : 0;
}

static int check_dir(const char *dir)
// Opens files made on another platform and prints what happens
{
  char p[1024];
  snprintf(p, sizeof(p), "%s/t.gxd", dir);
  gxDatabaseError e1 = open_ro(p);
  printf("%s: %s\n", p, gxDatabaseExceptionMessage(e1));
  snprintf(p, sizeof(p), "%s/t.btx", dir);
  IntKey k;
  gxBtree bt(k, 7);
  gxDatabaseError e2 = bt.Open(p, gxDBASE_READONLY);
  printf("%s: %s\n", p, gxDatabaseExceptionMessage(e2));
  bt.Close();
  if(e1 == gxDBASE_FILE_LAYOUT_ERROR && e2 == gxDBASE_FILE_LAYOUT_ERROR) return 3;
  return (e1 == gxDBASE_NO_ERROR && e2 == gxDBASE_NO_ERROR) ? 0 : 1;
}

int main(int argc, char **argv)
{
  if(argc == 3 && strcmp(argv[1], "check") == 0) return check_dir(argv[2]);
  return run_tests();
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
