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
version 3 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library. If not, see
<https://www.gnu.org/licenses/>.

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
It then writes the pages to the database files (without a sync), so
other processes see the batch as soon as Commit() returns. A
checkpoint syncs the files and empties the journal; it runs when the
journal passes the checkpoint size (default 8 MB), on Checkpoint() and
on Close(), and Close() truncates the journal file. See SetWriteMode()
for the write-back mode, which keeps committed pages in memory until a
checkpoint.

Other processes that read the files while a writer commits see every
batch once Commit() returns, but a reader that runs during a commit's
page writes can see part of the batch. Use the application's own
change counter, or SetWriteLock(1) in the writer and gxdJournalReadLock
in the readers.

If the program or the computer stops, the next Open() or Recover()
writes every committed transaction in the journal to the files and
ignores a transaction that was not committed. Commit(0) skips the
sync: the transaction survives a program crash but may be lost (as a
whole) if the computer loses power.

Rules:
- Make every change to the attached files inside a transaction, and
  only from this process while the journal is open.
- An open transaction's pages are held in memory until it commits.
  In write-back mode committed pages stay in memory until the next
  checkpoint, which runs when the journal passes the checkpoint size
  or a file has more committed pages than its page cache size.
- In write-through mode Commit(0) writes the pages to the files before
  the journal is synced: a power loss right after it can leave part
  of that batch in the files. Commit(1) never does.
- Rollback() drops the open transaction's changes and reloads the
  attached gxDatabase and gxBtree headers; an attached BtreeCache is
  emptied.

Changes:
==============================================================
10/31/2026: New in DataReel 7.2.
11/07/2026: DataReel 7.3 replication. EnableShipLog() also appended
each commit to a ship log (gxdrepl.h) that gxrepl sent to another
node, and ApplyPages() wrote a batch of received pages to the files
as one transaction through this journal's recovery code.
11/16/2026: DataReel 7.4 drops the gxrepl page-shipping replication
(7.4 replication is gxObjRepl, gxobjrepl.h): EnableShipLog(),
DisableShipLog(), ShipLog(), EnableObjectNotes(), ApplyPages() and the
"<journal>.recovered" marker are gone. Recover()'s "mark" argument is
kept and ignored.
==============================================================
*/
// ----------------------------------------------------------- //
#ifndef __GX_DATABASE_JOURNAL_HPP__
#define __GX_DATABASE_JOURNAL_HPP__

#include "gxdlcode.h"

#include <string>
#include <vector>
#include <map>
#include "gxdbase.h"

namespace datareel {

class gxBtree;     // Forward class declarations
class BtreeCache;
struct gxdPage;


const size_t gxdDefaultCheckpointSize = 8 * 1024 * 1024;
const size_t gxdDefaultWriteBackCheckpointSize = 64 * 1024 * 1024;

// When committed pages reach the database files, see SetWriteMode()
enum gxdJournalWriteMode {
  gxdJOURNAL_WRITE_THROUGH, // At each commit (the default)
  gxdJOURNAL_WRITE_BACK     // At each checkpoint
};

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
  // 11/16/2026: mark is ignored (7.4 dropped the ship log and its "<jname>.recovered" marker).
  static gxDatabaseError Recover(const char *jname,
				 unsigned long *recovered = 0, int mark = 1);
  // 11/16/2026: kept for callers (gxObjRepl): the reason of this thread's last Recover() failure, or ""
  static const char *RecoverError();

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
  // 11/16/2026: Write-back mode: write the committed pages held in
  // memory to their files now, without a sync and without emptying the
  // journal (the next checkpoint syncs the files and empties it). Not
  // inside a transaction. With Commit(0) and a sync of the journal file
  // made in between (by another descriptor, so another thread may use
  // the journal meanwhile), it is the write-through commit split in
  // three: the files are changed only after their batch is durable.
  gxDatabaseError WriteCommitted();
  // 11/16/2026: A checkpoint in two steps, for a program that syncs the
  // files itself in between (with other descriptors, without holding
  // its own lock on the journal, so other threads read and commit
  // meanwhile). CheckpointBegin() (not inside a transaction) writes the
  // committed pages to the files and passes back the path of every
  // attached file and the commit count; the caller syncs each of those
  // files (fsync) and calls CheckpointEnd() with the count. If no commit
  // came in between, the files hold every page in the journal, which is
  // emptied without syncing them again; otherwise CheckpointEnd() does
  // nothing and returns gxDBASE_FILE_NOT_READY (try again later).
  gxDatabaseError CheckpointBegin(std::vector<std::string> &paths,
				  unsigned long &commit_count);
  gxDatabaseError CheckpointEnd(unsigned long commit_count);
  int InTransaction() const { return in_txn; }
  // Checkpoint when the journal passes this size. 0 (the default) means
  // 8 MB in write-through mode and 64 MB in write-back mode.
  void SetCheckpointSize(size_t bytes) { ckpt_size = bytes; }

  // WRITE_THROUGH (the default) writes a commit's pages to the database
  // files right after they are in the journal, without syncing the
  // files, so other processes reading the files see each batch when
  // Commit() returns. A checkpoint then only syncs the files and empties
  // the journal. WRITE_BACK keeps committed pages in memory until a
  // checkpoint writes them: fewer writes for a program that is the only
  // user of the files, but other processes do not see committed data
  // until the checkpoint.
  void SetWriteMode(gxdJournalWriteMode mode) { write_mode = mode; }

  // Hold an exclusive lock on the journal file while a commit writes its
  // pages to the database files. Readers in other processes that take a
  // gxdJournalReadLock never see a batch half written. Off by default.
  // 7.4.1: in write-back mode too (WriteCommitted() and checkpoints);
  // Recover() always holds it while it replays
  void SetWriteLock(int lock) { write_lock = lock; }

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
    int written; // Pages written to the file since the last checkpoint
  };
  gxDatabaseError WriteHeader();
  size_t CheckpointSize() const;

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
  gxdJournalWriteMode write_mode;
  int write_lock;
  unsigned long recovered;
  FAU_t jsize;        // End of the journal's transactions
  FAU_t jalloc;       // Journal file size (space reserved ahead)
  unsigned long seq;  // Number of the last commit
  unsigned long salt;
  size_t ckpt_size;
  gxDatabaseError jerror;
  std::vector<unsigned char> out; // Commit record buffer, kept between commits
};

class GXDLCODE_API gxdJournalReadLock
// 10/31/2026: Shared lock on a journal file for a process that reads
// the database files while another process writes them through a
// gxdJournal with SetWriteLock(1). While the lock is held no commit is
// writing pages, so the files hold whole batches only:
//
//   gxdJournalReadLock rl;
//   rl.Lock("orders.jnl");  // Waits for a commit's page writes to end
//   ... re-read the headers, read records ...
//   rl.Unlock();
//
// Keep the lock short: the writer's next commit waits for it.
{
public:
  gxdJournalReadLock() { jfp = 0; }
  ~gxdJournalReadLock() { Unlock(); }

private:
  gxdJournalReadLock(const gxdJournalReadLock &) { }
  void operator=(const gxdJournalReadLock &) { }

public:
  gxDatabaseError Lock(const char *jname);
  void Unlock();

private:
  gxdFPTR *jfp;
};

} // namespace datareel

#endif // __GX_DATABASE_JOURNAL_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
