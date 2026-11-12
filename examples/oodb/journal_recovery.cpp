// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: journal_recovery.cpp
// Compiler Used: GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/12/2026
// Date Last Modified: 11/12/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- //
// ------------- Program Description and Details ------------- //
// ----------------------------------------------------------- //
/*
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library. If not, see
<https://www.gnu.org/licenses/>.

gxdJournal crash and recovery.

A transaction groups several writes so that after a crash either all
of them are in the database file or none are. This program proves it
the hard way, by killing a writer in the middle of its work:

 1. A child process (fork) opens a journal, creates a database file,
    attaches the file to the journal and writes batches of records.
    Each batch is one transaction: Begin(), several Alloc() and
    Write() calls, Commit(). After each commit the child writes the
    number of committed batches to a progress file.
 2. The parent waits until a few batches are committed, waits a
    little longer so the child is somewhere inside a transaction, and
    kills it with SIGKILL. Nothing in the child gets to clean up:
    this is what a crash or "kill -9" looks like to the files.
 3. The parent calls gxdJournal::Recover(), which writes every
    committed transaction left in the journal to the database file
    and drops the one that was not committed.
 4. The parent opens the database and checks it: every record belongs
    to a whole batch, no batch is partly there, and at least every
    batch the child reported as committed is present. (One more batch
    may be there: the child can be killed after a Commit() returned
    and before it wrote the progress file.)
 5. It also shows Rollback(): a transaction that is started and then
    rolled back leaves no trace.

Each record is 64 bytes: the batch number, the record number in the
batch, the number of records in a batch and some text. The database
is consistent when every batch b holds records 0 to BATCH-1, each
exactly once.

The program uses fork() and kill(), so it runs on Linux and other
POSIX systems.

Usage: journal_recovery [rounds]

  rounds  How many times to crash and recover (default 5)

Files written in the current directory: journal_recovery.gxd,
journal_recovery.jnl and journal_recovery.progress. Exits with 0 when
every round recovers to a consistent database.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "gxdbase.h"
#include "gxdjrnl.h"

using namespace datareel;

const char *DB_FILE = "journal_recovery.gxd";
const char *JNL_FILE = "journal_recovery.jnl";
const char *PROGRESS_FILE = "journal_recovery.progress";
const int BATCH = 25; // Records per transaction

struct Record {
  std::int32_t batch;     // Which transaction wrote it
  std::int32_t number;    // 0 .. BATCH-1 inside the transaction
  std::int32_t batch_size;
  char text[52];
};

static void write_progress(long committed)
{
  // Write a new file and rename it over the old one, so the parent
  // never reads half a number
  std::string tmp = std::string(PROGRESS_FILE) + ".tmp";
  FILE *fp = fopen(tmp.c_str(), "w");
  if(!fp) return;
  fprintf(fp, "%ld\n", committed);
  fclose(fp);
  rename(tmp.c_str(), PROGRESS_FILE);
}

static long read_progress()
{
  FILE *fp = fopen(PROGRESS_FILE, "r");
  if(!fp) return 0;
  long n = 0;
  if(fscanf(fp, "%ld", &n) != 1) n = 0;
  fclose(fp);
  return n;
}

// The child: write batches until killed. It never returns.
static void writer()
{
  gxdJournal j;
  // Open the journal before the database file. Open() recovers any
  // committed transactions an earlier crash left in it.
  if(j.Open(JNL_FILE) != gxDBASE_NO_ERROR) _exit(2);
  gxDatabase db;
  if(db.Create(DB_FILE) != gxDBASE_NO_ERROR) _exit(3);
  // Make the new file's header durable before the journal takes over:
  // the journal protects changes, so the starting point must be on disk.
  db.Sync(1);
  if(j.Attach(&db) != gxDBASE_NO_ERROR) _exit(4);
  for(long b = 0;; b++) {
    j.Begin();
    for(int i = 0; i < BATCH; i++) {
      Record r;
      memset(&r, 0, sizeof(r));
      r.batch = (std::int32_t)b;
      r.number = i;
      r.batch_size = BATCH;
      snprintf(r.text, sizeof(r.text), "batch %ld record %d", b, i);
      FAU_t addr = db.Alloc(sizeof(r));
      if(addr == (FAU_t)0 || db.Write(&r, sizeof(r), addr) != gxDBASE_NO_ERROR) _exit(5);
      // Slow the writer down a little so the parent can kill it in the
      // middle of a transaction, not only between two
      if(i == BATCH / 2) usleep(2000);
    }
    // Commit(1): the transaction is in the journal and synced to disk
    // when Commit() returns
    if(j.Commit(1) != gxDBASE_NO_ERROR) _exit(6);
    write_progress(b + 1);
  }
}

// Check the recovered database. Returns the number of whole batches,
// -1 if the file is not consistent.
static long check_database(long committed, std::string &why)
{
  gxDatabase db;
  if(db.Open(DB_FILE, gxDBASE_READONLY) != gxDBASE_NO_ERROR) {
    why = "cannot open the database";
    return -1;
  }
  std::map<int, std::vector<int>> batches; // batch -> how often each number is there
  for(FAU_t addr = db.FindFirstObject(); addr != (FAU_t)0;
      addr = db.FindNextObject(addr)) {
    Record r;
    if(db.ObjectLength(addr) != sizeof(r) ||
       db.Read(&r, sizeof(r), addr) != gxDBASE_NO_ERROR ||
       r.batch_size != BATCH || r.number < 0 || r.number >= BATCH) {
      why = "a record that no batch wrote";
      return -1;
    }
    std::vector<int> &seen = batches[r.batch];
    seen.resize(BATCH);
    seen[r.number]++;
  }
  db.Close();
  long expect = 0;
  for(auto &b : batches) {
    if(b.first != expect) { why = "a missing batch in the middle"; return -1; }
    for(int n : b.second) if(n != 1) { why = "a batch that is only partly there"; return -1; }
    expect++;
  }
  if(expect < committed) { why = "a committed batch was lost"; return -1; }
  if(expect > committed + 1) { why = "more batches than were committed"; return -1; }
  return expect;
}

// Rollback(): start a transaction, write to it, roll it back
static bool rollback_demo()
{
  gxdJournal j;
  gxDatabase db;
  if(j.Open(JNL_FILE) != gxDBASE_NO_ERROR || db.Open(DB_FILE) != gxDBASE_NO_ERROR) return false;
  j.Attach(&db);
  long before = 0;
  for(FAU_t a = db.FindFirstObject(); a != (FAU_t)0; a = db.FindNextObject(a)) before++;
  j.Begin();
  Record r;
  memset(&r, 0, sizeof(r));
  r.batch = -1; // Would fail the consistency check if it survived
  FAU_t addr = db.Alloc(sizeof(r));
  db.Write(&r, sizeof(r), addr);
  j.Rollback();
  long after = 0;
  for(FAU_t a = db.FindFirstObject(); a != (FAU_t)0; a = db.FindNextObject(a)) after++;
  j.Close(); // Close the journal before the files it protects
  db.Close();
  std::cout << "Rollback: " << before << " records before, " << after
	    << " after a rolled-back insert\n";
  return before == after;
}

int main(int argc, char **argv)
{
  int rounds = argc > 1 ? atoi(argv[1]) : 5;
  if(rounds < 1) rounds = 1;
  int failures = 0;
  srand(12345);
  for(int round = 0; round < rounds; round++) {
    remove(DB_FILE);
    remove(JNL_FILE);
    remove(PROGRESS_FILE);

    pid_t pid = fork();
    if(pid < 0) { perror("fork"); return 1; }
    if(pid == 0) writer(); // The child never comes back

    // Wait for at least 3 committed batches (up to 30 seconds)
    for(int i = 0; i < 3000 && read_progress() < 3; i++) usleep(10000);
    // Then a random short wait, so the kill lands anywhere in a batch
    usleep(1000 + rand() % 20000);
    kill(pid, SIGKILL);
    int status = 0;
    waitpid(pid, &status, 0);
    long committed = read_progress();

    // Recover before anything opens the database file
    unsigned long recovered = 0;
    gxDatabaseError err = gxdJournal::Recover(JNL_FILE, &recovered);
    std::string why;
    long batches = err == gxDBASE_NO_ERROR ? check_database(committed, why) : -1;
    if(err != gxDBASE_NO_ERROR) why = gxDatabaseExceptionMessage(err);
    std::cout << "Round " << round + 1 << ": killed after " << committed
	      << " committed batches; Recover() wrote " << recovered
	      << " transactions from the journal; database holds "
	      << (batches < 0 ? 0 : batches) << " whole batches"
	      << (batches == committed + 1 ? " (the last commit beat its progress note)" : "")
	      << (batches < 0 ? " - NOT CONSISTENT: " + why : std::string(" - consistent"))
	      << "\n";
    if(batches < 0) failures++;
  }
  if(!rollback_demo()) failures++;
  std::cout << (failures == 0 ? "Every crash recovered to a consistent database" :
		"Some rounds failed") << "\n";
  return failures == 0 ? 0 : 1;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
