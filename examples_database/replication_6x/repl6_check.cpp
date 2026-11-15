// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: repl6_check.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/15/2026
// Date Last Modified: 11/15/2026
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

DataReel 6.4 replication example, the checker: reads a copy of the
database repl6_writer writes (read only, DataReel 6.x) and checks it
is a whole commit:

  repl6_check FOLDER [MIN_RECORDS]

- the header and every record read without errors;
- the records are exactly the header's count, in order (seq 0, 1, ...),
  each with a good checksum;
- the running checksum of the records equals the header's.
On a live copy the checker holds the shared lock of the copy's apply
journal (FOLDER/.gxrepl/apply.jnl, gxdJournalReadLock), the lock the
DataReel 7.x gxrepl holds exclusively while it applies a commit: the
DataReel 6.x lock and the 7.x lock are the same file lock.
A copy taken in the middle of a commit (records without the header
that counts them, or a header without its records) fails. Prints
"WHOLE <records> <commits>" and returns 0 if the copy is whole and
holds at least MIN_RECORDS records, 1 otherwise.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gxdbase.h"
#include "gxdjrnl.h"
#include "futils.h"
#include "gxstring.h"
#include "repl6_records.h"

int main(int argc, char **argv)
{
  if(argc < 2) {
    printf("Usage: %s FOLDER [MIN_RECORDS]\n", argv[0]);
    return 2;
  }
  gxString dname = argv[1];
  dname << "/data.gxd";
  unsigned long min_records = argc > 2 ? strtoul(argv[2], 0, 10) : 0;

  gxString aname = argv[1];
  aname << "/.gxrepl/apply.jnl";
  gxdJournalReadLock rl;
  if(futils_exists(aname.c_str()) && rl.Lock(aname.c_str()) != gxDBASE_NO_ERROR) {
    printf("TORN cannot lock %s\n", aname.c_str());
    return 1;
  }
  gxDatabase db;
  if(db.Open(dname.c_str(), gxDBASE_READONLY) != gxDBASE_NO_ERROR) {
    printf("TORN cannot open %s: %s\n", dname.c_str(), db.DatabaseExceptionMessage());
    return 1;
  }
  ReplHeader hdr;
  FAU_t a = db.FindFirstObject();
  if(a == (FAU_t)0 || db.Read(&hdr, sizeof(hdr), a) != gxDBASE_NO_ERROR ||
     memcmp(hdr.magic, REPL6_MAGIC, sizeof(hdr.magic)) != 0) {
    printf("TORN no header\n");
    return 1;
  }
  unsigned long n = 0, crc = 0;
  // Walk the blocks in file order: the next block starts where this one
  // ends (BlockLength() is the whole block, header included)
  const FAU_t bh = (FAU_t)db.BlockHeaderSize();
  FAU_t eof = (FAU_t)db.GetEOF();
  for(;;) {
    __ULWORD__ len = db.BlockLength(a);
    if(len == 0) break;
    FAU_t next = a - bh + (FAU_t)len;
    if(next >= eof) break;
    a = db.FindFirstObject(next);
    if(a == (FAU_t)0) break;
    ReplRecord rec;
    if(db.Read(&rec, sizeof(rec), a) != gxDBASE_NO_ERROR) {
      printf("TORN record %lu cannot be read\n", n);
      return 1;
    }
    if(rec.seq != n || rec.crc != RecordSelfCRC(rec)) {
      printf("TORN record %lu: seq %lu, checksum %s\n", n, rec.seq,
	     rec.crc == RecordSelfCRC(rec) ? "good" : "bad");
      return 1;
    }
    crc = RecordCRC(crc, rec);
    n++;
  }
  if(n != hdr.count || crc != hdr.crc) {
    printf("TORN %lu records, the header says %lu (checksum %s)\n", n, hdr.count,
	   crc == hdr.crc ? "equal" : "different");
    return 1;
  }
  printf("WHOLE %lu %lu\n", hdr.count, hdr.commits);
  db.Close();
  return n >= min_records ? 0 : 1;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
