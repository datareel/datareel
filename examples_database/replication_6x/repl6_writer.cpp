// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: repl6_writer.cpp
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

DataReel 6.4 replication example, the writer: a plain DataReel 6.x
program (no DataReel 7.x code) writing a database that the DataReel
7.x gxrepl copies to another node (see README.txt).

  repl6_writer FOLDER COMMITS [RECORDS] [DELAY_MS] [plain]

Writes FOLDER/data.gxd. Each commit appends RECORDS records and
rewrites the header record (count and a running checksum of every
record) in one gxdJournal transaction (FOLDER/data.jnl), with the
journal's write lock on (SetWriteLock(1)): while the writer changes
the database file it holds the journal file locked, and gxrepl reads
a live folder under the shared journal lock (gxdJournalReadLock), so
it never copies half a commit. "plain" writes without a journal
(for a folder that is not written while gxrepl copies it).
repl6_check reads a copy and checks it is whole.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gxdbase.h"
#include "gxdjrnl.h"
#include "gxcrc32.h"
#include "gxstring.h"
#include "repl6_records.h"

#if defined (__UNIX__)
#include <unistd.h>
#else
#include <windows.h>
#endif

static void sleep_ms(int ms)
{
#if defined (__UNIX__)
  usleep((useconds_t)ms * 1000);
#else
  Sleep(ms);
#endif
}

int main(int argc, char **argv)
{
  if(argc < 3) {
    printf("Usage: %s FOLDER COMMITS [RECORDS] [DELAY_MS] [plain]\n", argv[0]);
    return 2;
  }
  gxString dir = argv[1];
  int commits = atoi(argv[2]);
  int per = argc > 3 ? atoi(argv[3]) : 10;
  int delay = argc > 4 ? atoi(argv[4]) : 0;
  int plain = argc > 5 && strcmp(argv[5], "plain") == 0;
  gxString dname = dir; dname << "/data.gxd";
  gxString jname = dir; jname << "/data.jnl";

  gxdJournal j;
  if(!plain) {
    j.SetWriteLock(1);
    if(j.Open(jname.c_str()) != gxDBASE_NO_ERROR) {
      printf("Cannot open the journal %s\n", jname.c_str()); return 1;
    }
  }
  gxDatabase db;
  int fresh = 0;
  if(db.Open(dname.c_str()) != gxDBASE_NO_ERROR) {
    if(db.Create(dname.c_str()) != gxDBASE_NO_ERROR) {
      printf("Cannot create %s\n", dname.c_str()); return 1;
    }
    fresh = 1;
  }
  if(!plain && j.Attach(&db) != gxDBASE_NO_ERROR) {
    printf("Cannot attach %s to the journal\n", dname.c_str()); return 1;
  }

  ReplHeader hdr;
  FAU_t hdr_addr;
  if(fresh) {
    memset(&hdr, 0, sizeof(hdr));
    memcpy(hdr.magic, REPL6_MAGIC, sizeof(hdr.magic));
    if(!plain) j.Begin();
    hdr_addr = db.Alloc(sizeof(hdr));
    db.Write(&hdr, sizeof(hdr), hdr_addr);
    if(!plain && j.Commit() != gxDBASE_NO_ERROR) { printf("Commit failed\n"); return 1; }
  }
  else {
    hdr_addr = db.FindFirstObject();
    if(hdr_addr == (FAU_t)0 || db.Read(&hdr, sizeof(hdr), hdr_addr) != gxDBASE_NO_ERROR ||
       memcmp(hdr.magic, REPL6_MAGIC, sizeof(hdr.magic)) != 0) {
      printf("%s is not a repl6 database\n", dname.c_str()); return 1;
    }
  }

  for(int c = 0; c < commits; c++) {
    if(!plain && j.Begin() != gxDBASE_NO_ERROR) { printf("Begin failed\n"); return 1; }
    for(int r = 0; r < per; r++) {
      ReplRecord rec;
      MakeRecord(rec, hdr.count);
      FAU_t a = db.Alloc(sizeof(rec));
      if(a == (FAU_t)0 || db.Write(&rec, sizeof(rec), a) != gxDBASE_NO_ERROR) {
	printf("Write failed\n"); return 1;
      }
      hdr.crc = RecordCRC(hdr.crc, rec);
      hdr.count++;
    }
    hdr.commits++;
    if(db.Write(&hdr, sizeof(hdr), hdr_addr) != gxDBASE_NO_ERROR) { printf("Header write failed\n"); return 1; }
    if(!plain) {
      if(j.Commit() != gxDBASE_NO_ERROR) { printf("Commit failed\n"); return 1; }
    }
    else db.Flush();
    if(delay > 0) sleep_ms(delay);
  }
  printf("%s: %lu records in %lu commits\n", dname.c_str(),
	 (unsigned long)hdr.count, (unsigned long)hdr.commits);
  if(!plain) j.Close();
  db.Close();
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
