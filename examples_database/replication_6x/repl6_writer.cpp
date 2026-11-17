// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: repl6_writer.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/15/2026
// Date Last Modified: 11/17/2026
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

DataReel 6.4 replication example: a DataReel 6.x program writing its
records as the primary of a DataReel 7.x object replication pair
(gxv7ObjRepl, dr7features.h). The copy can be repl6_check, a DataReel
7.4 program or the 7.4 objrepl tool, on Linux or Windows.

  repl6_writer primary ROOT CONF RECORDS   write RECORDS station readings
                                       (adds, every 7th updated, every
                                       11th removed), commit them 25 at
                                       a time, send them to the copy and
                                       wait up to 120 s for the copy to
                                       have them all. Exit 0 (drained),
                                       2 (not drained), 1 (an error)
  repl6_writer digest ROOT             every database: ids and content
                                       hashes (the line "objrepl digest"
                                       prints)
  repl6_writer status ROOT             the node's status line
  repl6_writer engine                  the 7.x library's database engine
                                       (exit 0 for 64-bit, 1 for 32-bit)

CONF is an objrepl.conf file, one "key value" line each: the primary
needs copy (HOST:PORT), ca, cert and key. See README.txt.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dr7features.h"
#include "repl6_records.h"

// digest ROOT, status ROOT, engine: the same in both programs (-1: none of them)
static int folder_cmd(int argc, char **argv)
{
  gxString line;
  if(argc == 2 && strcmp(argv[1], "engine") == 0) {
    printf("engine %s\n", gxv7ObjRepl::Engine64() ? "64-bit" : "32-bit");
    return gxv7ObjRepl::Engine64() ? 0 : 1;
  }
  if(argc == 3 && strcmp(argv[1], "digest") == 0) {
    int rc = gxv7ObjRepl::Digest(argv[2], line);
    printf("%s\n", line.c_str());
    return rc == 0 ? 0 : 1;
  }
  if(argc == 3 && strcmp(argv[1], "status") == 0) {
    int rc = gxv7ObjRepl::Status(argv[2], line);
    printf("%s\n", line.c_str());
    return rc == 0 ? 0 : 1;
  }
  return -1;
}

static int primary(const char *root, const char *conf, long records)
{
  gxv7ObjRepl p;
  if(p.LoadConfig(conf) != 0 || p.Open(root, gxv7ObjRepl::PRIMARY) != 0 ||
     p.OpenDatabase(REPL6_DB) != 0) {
    printf("repl6_writer: %s\n", p.Error().c_str());
    return 1;
  }
  unsigned long long *ids = new unsigned long long[records > 0 ? records : 1];
  long adds = 0, updates = 0, removes = 0;
  Reading r;
  for(long i = 0; i < records; ) {
    if(p.Begin() != 0) { printf("repl6_writer: Begin: %s\n", p.Error().c_str()); delete[] ids; return 1; }
    for(int k = 0; k < 25 && i < records; k++, i++) {
      ids[i] = p.NewId();
      make_reading(r, i + 1, 0);
      if(ids[i] == 0 || p.Put(REPL6_DB, ids[i], &r, sizeof(r)) != 0) {
	printf("repl6_writer: Put: %s\n", p.Error().c_str()); delete[] ids; return 1;
      }
      adds++;
      if(i % 7 == 6) { // an update of an earlier record
	make_reading(r, i - 5, 1);
	if(p.Put(REPL6_DB, ids[i - 6], &r, sizeof(r)) != 0) { printf("repl6_writer: update: %s\n", p.Error().c_str()); delete[] ids; return 1; }
	updates++;
      }
      if(i % 11 == 10 && (i - 3) % 7 != 0) { // a remove (the id stays as a
					      // tombstone), never of one updated later
	if(p.Remove(REPL6_DB, ids[i - 3]) != 0) { printf("repl6_writer: Remove: %s\n", p.Error().c_str()); delete[] ids; return 1; }
	removes++;
      }
    }
    if(p.Commit() != 0) { printf("repl6_writer: Commit: %s\n", p.Error().c_str()); delete[] ids; return 1; }
    p.Pump(); // The copy may be away: Drain() below waits for it
  }
  delete[] ids;
  int rc = p.Drain(120);
  printf("repl6_writer: %ld adds, %ld updates, %ld removes, last_seq %llu, copy has %llu (%s)\n",
	 adds, updates, removes, p.LastSeq(), p.CopyApplied(), rc == 0 ? "drained" : "not drained");
  if(rc != 0) printf("repl6_writer: %s\n", p.Error().c_str());
  p.Close();
  return rc == 0 ? 0 : 2;
}

int main(int argc, char **argv)
{
  int rc = folder_cmd(argc, argv);
  if(rc >= 0) return rc;
  if(argc == 5 && strcmp(argv[1], "primary") == 0) return primary(argv[2], argv[3], atol(argv[4]));
  fprintf(stderr, "usage: repl6_writer primary ROOT CONF RECORDS | digest ROOT | status ROOT | engine\n");
  return 64;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
