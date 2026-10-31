// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: testprog.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 10/31/2026
// Date Last Modified: 10/31/2026
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

6.3 tests for damaged database files (from the DataReel 7.3 security
scan):

  zero_length  A block header with a length of zero: the block walks
               (TotalBlocks()) used to loop forever.
  huge_length  A block length past the end of the file: the address
               could wrap back to an earlier block.
  tiny_length  Block lengths shorter than a block header: the walks
               used to read the same bytes again and again.
  checksum     A damaged block length: the checksum functions used to
               allocate and read up to 4 GB.
  wrong_file   A file that is not a database file: Open() used to keep
               the file open, and closing the object wrote a database
               header into it.
  closed_crc   WriteObjectChecksum() on a closed file returns 0 (the
               return value used to be uninitialised).

The programs run with a 1 GB address space limit and a 60 second alarm
on UNIX, so a huge allocation or an endless loop fails the test.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "gxdbase.h"

#if defined (__UNIX__)
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <sys/resource.h>
#endif

static int failures = 0;

static void check(int ok, const char *what)
{
  printf("%s %s\n", ok ? "PASS" : "FAIL", what);
  if(!ok) failures++;
}

static double seconds()
{
  return (double)clock() / CLOCKS_PER_SEC;
}

const int NUM_OBJECTS = 200; // About 24 KB with rev 'C' blocks
const __ULWORD__ OBJECT_SIZE = 100;

static int make_db(const char *fname, FAU_t *objects)
// A rev 'C' file (block walks and object checksums), returns 1 if
// successful
{
  gxDatabase f;
  if(f.Create(fname, (FAU_t)0, 'C') != gxDBASE_NO_ERROR) return 0;
  char buf[OBJECT_SIZE];
  for(int i = 0; i < NUM_OBJECTS; i++) {
    memset(buf, 'a' + (i % 26), sizeof(buf));
    FAU_t a = f.Alloc(OBJECT_SIZE);
    if(!a || f.Write(buf, OBJECT_SIZE, a) != gxDBASE_NO_ERROR) return 0;
    f.WriteObjectChecksum(a);
    objects[i] = a;
  }
  return f.Close() == gxDBASE_NO_ERROR;
}

static int set_length(const char *fname, FAU_t object, gxUINT32 len)
{
  gxDatabase f;
  if(f.Open(fname) != gxDBASE_NO_ERROR) return 0;
  gxBlockHeader blk;
  FAU_t a = object - f.BlockHeaderSize();
  if(f.Read(&blk, sizeof(blk), a) != gxDBASE_NO_ERROR) return 0;
  blk.block_length = len;
  return f.Write(&blk, sizeof(blk), a) == gxDBASE_NO_ERROR;
}

static long open_files()
// Number of open file descriptors, or -1 if unknown
{
#if defined (__LINUX__)
  DIR *d = opendir("/proc/self/fd");
  if(!d) return -1;
  long n = 0;
  while(readdir(d)) n++;
  closedir(d);
  return n;
#else
  return -1;
#endif
}

static int same_file(const char *a, const char *b)
{
  FILE *fa = fopen(a, "rb"), *fb = fopen(b, "rb");
  int same = (fa && fb);
  while(same) {
    int ca = fgetc(fa), cb = fgetc(fb);
    if(ca != cb) same = 0;
    if(ca == EOF || cb == EOF) break;
  }
  if(fa) fclose(fa);
  if(fb) fclose(fb);
  return same;
}

int main()
{
#if defined (__UNIX__)
  struct rlimit rl;
  rl.rlim_cur = rl.rlim_max = 1024UL * 1024UL * 1024UL;
  setrlimit(RLIMIT_AS, &rl);
  alarm(60);
#endif
  FAU_t objects[NUM_OBJECTS];
  const char *fname = "damaged.gxd";
  int created = make_db(fname, objects);
  check(created, "create the test file");
  if(!created) return 1;

  { // Undamaged file: every block counted
    gxDatabase f;
    f.Open(fname, gxDBASE_READONLY);
    check(f.TotalBlocks() == (FAU_t)NUM_OBJECTS, "undamaged file: TotalBlocks() counts every block");
  }

  // zero_length
  make_db(fname, objects);
  set_length(fname, objects[10], 0);
  {
    gxDatabase f;
    f.Open(fname, gxDBASE_READONLY);
    double t0 = seconds();
    FAU_t n = f.TotalBlocks();
    double t = seconds() - t0;
    check(n >= (FAU_t)(NUM_OBJECTS - 1) && n <= (FAU_t)NUM_OBJECTS && t < 2.0,
	  "zero_length: TotalBlocks() ends and skips the damaged block");
    FAU_t a = f.FindFirstObject();
    int walked = 0;
    while(a && walked <= NUM_OBJECTS * 2) {
      walked++;
      a = f.FindFirstObject(a);
    }
    check(walked <= NUM_OBJECTS, "zero_length: an object walk ends");
  }

  // huge_length (from the 7.3 security scan test)
  make_db(fname, objects);
  set_length(fname, objects[10], 0xFFFFFFFF);
  {
    gxDatabase f;
    f.Open(fname, gxDBASE_READONLY);
    double t0 = seconds();
    FAU_t n = f.TotalBlocks();
    double t = seconds() - t0;
    check(t < 2.0 && n <= (FAU_t)NUM_OBJECTS, "huge_length: TotalBlocks() ends on a length past the end of the file");
  }

  // tiny_length
  make_db(fname, objects);
  for(int i = 0; i < NUM_OBJECTS; i += 2) set_length(fname, objects[i], (gxUINT32)(1 + (i % 7)));
  {
    gxDatabase f;
    f.Open(fname, gxDBASE_READONLY);
    double t0 = seconds();
    FAU_t n = f.TotalBlocks();
    double t = seconds() - t0;
    printf("     tiny_length: %ld blocks in %.3f s\n", (long)n, t);
    check(t < 2.0 && n <= (FAU_t)NUM_OBJECTS, "tiny_length: TotalBlocks() takes time in proportion to the file");
  }

  // checksum
  make_db(fname, objects);
  set_length(fname, objects[3], 0x7ffffff0);
  set_length(fname, objects[4], 20); // Shorter than a header and a checksum
  // All bits set but the low ones: with native types on 64-bit systems
  // the length is 64 bits (found by fuzzing 6.3)
  set_length(fname, objects[6], (gxUINT32)0 - (gxUINT32)90);
  {
    gxDatabase f;
    f.Open(fname, gxDBASE_READWRITE);
    __ULWORD__ ocrc = 0, ccrc = 0;
    check(f.ReadObjectChecksum(objects[3], &ocrc, &ccrc) == 0, "checksum: ReadObjectChecksum() refuses a block longer than the file");
    check(f.WriteObjectChecksum(objects[3]) == 0, "checksum: WriteObjectChecksum() refuses a block longer than the file");
    check(f.ReadObjectChecksum(objects[4], &ocrc, &ccrc) == 0, "checksum: a block too short for a checksum is refused");
    check(f.ReadObjectChecksum(objects[6], &ocrc, &ccrc) == 0, "checksum: a length that wraps around is refused");
    check(f.ReadObjectChecksum(objects[5], &ocrc, &ccrc) == 1, "checksum: an undamaged block still checks");
  }

  // wrong_file
  {
    const char *other = "not_a_database.txt";
    const char *copy = "not_a_database.orig";
    FILE *fp = fopen(other, "wb");
    for(int i = 0; i < 4096; i++) fputs("text of another program\n", fp);
    fclose(fp);
    fp = fopen(copy, "wb");
    for(int i = 0; i < 4096; i++) fputs("text of another program\n", fp);
    fclose(fp);
    fp = fopen("short.txt", "wb");
    fputs("short", fp);
    fclose(fp);
    long before = open_files();
    int refused = 1;
    for(int i = 0; i < 300; i++) {
      gxDatabase f;
      if(f.Open(other, gxDBASE_READWRITE) != gxDBASE_WRONG_FILE_TYPE) refused = 0;
      gxDatabase g;
      if(g.Open("short.txt", gxDBASE_READONLY) == gxDBASE_NO_ERROR) refused = 0;
      if(f.IsOpen() || g.IsOpen()) refused = 0;
    }
    gxDatabase reuse;
    for(int i = 0; i < 300; i++) reuse.Open(other, gxDBASE_READONLY);
    long after = open_files();
    check(refused, "wrong_file: Open() refuses a file that is not a database file and leaves it closed");
    check(before < 0 || before == after, "wrong_file: no file handle is left open");
    check(same_file(other, copy), "wrong_file: the other program's file is not changed");
    remove(other);
    remove(copy);
    remove("short.txt");
  }

  // closed_crc
  {
    gxDatabase f;
    check(f.WriteObjectChecksum((FAU_t)1000) == 0, "closed_crc: WriteObjectChecksum() on a closed file returns 0");
  }

  remove(fname);
  if(failures) {
    printf("%d damaged file tests failed\n", failures);
    return 1;
  }
  printf("All damaged file tests passed\n");
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
