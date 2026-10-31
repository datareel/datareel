// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxdjrnl.h
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 10/31/2026
// Date Last Modified: 10/31/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- //
// ---------- Include File Description and Details  ---------- //
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

The gxdJournal class makes a group of writes to one or more gxDatabase
files (a data file and its index files, for example) atomic: after a
program crash or a power loss either all of the writes in a
transaction are in the files or none are. The journal is a write-ahead
log kept in a separate file, so the database file formats do not
change and files written with a journal open without one.

  gxdJournal j;
  j.Open("orders.jnl");     // Replays committed transactions
  db.Open("orders.gxd");    // Open the files after the journal
  btx.Open("orders.btx");
  j.Attach(&db);
  j.Attach(&btx);           // A gxBtree, gxDatabase or BtreeCache
  j.Begin();
  ... inserts, updates and deletes ...
  j.Commit();               // Durable: on disk when it returns
  ...
  j.Close();                // Checkpoint, before closing the files
  btx.Close();
  db.Close();

While a transaction is open the attached files' writes stay in each
file's page cache (gxdpcache.h). Commit() appends the changed pages
and a commit record to the journal and syncs it: one sync per commit.
The pages stay in memory until a checkpoint writes them to the
database files, syncs the files and empties the journal. A checkpoint
runs when the journal passes the checkpoint size (default 64 MB), on
Checkpoint() and on Close().

If the program or the computer stops, the next Open() or Recover()
writes every committed transaction in the journal to the files and
ignores a transaction that was not committed. Commit(0) skips the
sync: the transaction survives a program crash but may be lost (as a
whole) if the computer loses power.

Rules:
- Make every change to the attached files inside a transaction, and
  only from this process while the journal is open.
- An open transaction's pages are held in memory until it commits.
  Committed pages stay in memory until the next checkpoint, which
  runs when the journal passes the checkpoint size or a file has more
  committed pages than its page cache size.
- Rollback() drops the open transaction's changes and reloads the
  attached gxDatabase and gxBtree headers; an attached BtreeCache is
  emptied.

Changes:
==============================================================
10/31/2026: New in DataReel 7.2.
==============================================================
*/
// ----------------------------------------------------------- //
#ifndef __GX_DATABASE_JOURNAL_HPP__
#define __GX_DATABASE_JOURNAL_HPP__

#include "gxdlcode.h"

#include <string>
#include <vector>
#include "gxdbase.h"

namespace datareel {

class gxBtree;     // Forward class declarations
class BtreeCache;

const size_t gxdDefaultCheckpointSize = 64 * 1024 * 1024;

// Journal sync levels, see SetSyncLevel()
enum gxdJournalSyncLevel {
  gxdJOURNAL_SYNC_OFF,    // Only Commit(1) syncs
  gxdJOURNAL_SYNC_NORMAL  // Checkpoints also sync (the default)
};

class GXDLCODE_API gxdJournal
{
public:
  gxdJournal();
  ~gxdJournal();

private:
  gxdJournal(const gxdJournal &) { }
  void operator=(const gxdJournal &) { }

public:
  // Write the committed transactions in the journal file "jname" to
  // their database files and empty the journal. Call it before the
  // database files are opened. Sets "recovered" to the number of
  // transactions written.
  static gxDatabaseError Recover(const char *jname,
				 unsigned long *recovered = 0);

  gxDatabaseError Open(const char *jname); // Recover, then open
  gxDatabaseError Close(); // Rollback or checkpoint, then close

  // Add a file to the journal. Turns on the file's page cache if it
  // is not on. Attach every file before Begin().
  gxDatabaseError Attach(gxDatabase *f,
			 size_t cache_bytes = gxdDefaultPageCacheSize);
  gxDatabaseError Attach(gxBtree *btx,
			 size_t cache_bytes = gxdDefaultPageCacheSize);
  gxDatabaseError Attach(BtreeCache *cache);
  gxDatabaseError Detach(); // Checkpoint and remove every file

  gxDatabaseError Begin();
  gxDatabaseError Commit(int durable = 1);
  gxDatabaseError Rollback();
  gxDatabaseError Checkpoint();
  int InTransaction() const { return in_txn; }
  void SetCheckpointSize(size_t bytes) { ckpt_size = bytes; }

  // NORMAL (the default) syncs the journal and the database files at
  // each checkpoint, so a power loss can lose the last Commit(0)
  // transactions but never leaves the files half updated. OFF skips
  // those syncs while the journal holds only Commit(0) transactions:
  // the files then survive a program crash but not a power loss, like
  // SQLite's synchronous=OFF. Commit(1) always syncs.
  void SetSyncLevel(gxdJournalSyncLevel level) { sync_level = level; }

  unsigned long Recovered() const { return recovered; } // Set by Open()
  gxDatabaseError GetJournalError() const { return jerror; }

public: // Statistics
  unsigned long commits;
  unsigned long checkpoints;
  unsigned long long pages_logged;
  unsigned long long pages_written;

private:
  struct JFile {
    gxDatabase *f;
    gxBtree *btx;
    std::string path;
  };
  gxDatabaseError WriteHeader();

private:
  gxdFPTR *jfp;
  std::string jname;
  std::vector<JFile> files;
  std::vector<BtreeCache *> caches;
  int in_txn;
  int header_written; // The journal holds a header and transactions
  int unsynced;       // Commit(0) transactions not synced yet
  int durable_logged; // A Commit(1) transaction is in the journal
  gxdJournalSyncLevel sync_level;
  unsigned long recovered;
  FAU_t jsize;        // End of the journal's transactions
  FAU_t jalloc;       // Journal file size (space reserved ahead)
  unsigned long seq;  // Number of the last commit
  unsigned long salt;
  size_t ckpt_size;
  gxDatabaseError jerror;
};

} // namespace datareel

#endif // __GX_DATABASE_JOURNAL_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
