DataReel 6.4 example: replicating a DataReel 6.x database with gxrepl
=====================================================================

DataReel 6.4 has no replication code of its own. A DataReel 6.x
program's database folder is replicated by the DataReel 7.x gxrepl
program, running next to the 6.x program as a separate process. The
6.x program is not changed and does not link any 7.x code.

  repl6_writer      a plain 6.x program: writes FOLDER/data.gxd, each
                    commit (records + the header that counts them) in
                    one gxdJournal transaction with the write lock on
  repl6_check       a plain 6.x program: reads a copy and checks it is
                    one whole commit (no half commit, nothing missing)
  run_repl6_cross.sh  the test: gxrepl copies the writer's folder to
                    another folder, while the writer is quiet and while
                    it writes, and the checker reads the copy

Build (Linux; gxrepl is POSIX only):

  # the 6.x programs (6.x makefiles, or the 6.x CMake build)
  cd examples_database/replication_6x && make
  # gxrepl, from a DataReel 7.4 or later source tree, with the 6.x
  # CMake build (or build it in the 7.x tree itself)
  cmake -S datareel-6.4 -B build -DDATAREEL7_FEATURES_DIR=/path/to/datareel-7.4
  cmake --build build -j2            # gxrepl is in build/dr7inner/tools

  ./run_repl6_cross.sh build/dr7inner/tools . [WORK_FOLDER]

What the test checks:
- a database written by a 6.x program, copied while the program is
  quiet: gxrepl once reports every set the same, the copy is
  byte-identical, the checker finds the copy whole;
- a 6.x database written without a journal: the same;
- the writer's journal (data.jnl) is never copied;
- copied live while the 6.x program writes 300 more commits (gxrepl
  run): every sample of the copy the checker reads is a whole commit
  and the copy never goes back; the copy catches up, and after the run
  it is byte-identical to the primary.

How it works, and what a 6.x program must do
--------------------------------------------
gxrepl copies a folder without a ship log by a scan: changed files are
compared in 64 KB pieces and only the pieces that differ are sent. A
live writer's files are read under the shared lock of the journal in
the same folder (gxdJournalReadLock on the folder's .jnl file), and
the copy applies each update through its own journal
(<root>/.gxrepl/apply.jnl), so the copy is always one whole commit.

The DataReel 6.3/6.4 journal (gxdJournal, gxdJournalReadLock) uses the
same lock as the 7.x journal: a shared or exclusive flock() on the
journal file (LockFileEx on Windows). So:

- A 6.x program that writes through gxdJournal with SetWriteLock(1)
  holds its journal file locked while it changes the database files,
  and gxrepl waits for it: copies taken while it writes are whole.
  Keep the journal in the same folder as the database files.
- A 6.x program that writes without a journal can be copied while it is
  quiet (between runs, or a folder it no longer writes). Copies taken
  while it writes may hold half a write: gxrepl's next pass corrects
  them, but a node must not take over from such a copy.
- A 6.x program reading a live copy holds the shared lock of the
  copy's apply journal (repl6_check does: gxdJournalReadLock on
  <copy>/.gxrepl/apply.jnl), the lock gxrepl holds while it applies.

Commit blocks, derived indexes (vector indexes and B-trees the copy
rebuilds itself), failover and failback, archived folders and the
monitoring are as in the DataReel 7.x replication administrator's
guide (DataReel 7.4: docs/replication_admin_guide.md). The ship log
(the commit-by-commit replication of a writer that calls
gxdJournal::EnableShipLog()) needs the 7.x journal: a 6.x program's
folders always go by the scan described above.

gxrepl configs used by the test (one set per database folder):

  # primary.conf (the 6.x program's node, client, pushes)
  node = primary
  role = client
  connect = 127.0.0.1:PORT
  server_name = copy
  cert = pki/primary.pem
  key = pki/primary.key
  ca = pki/ca.pem
  [set db]
  root = /path/to/the/6.x/database/folder
  direction = push
  scan_interval = 1

  # copy.conf (the other node, server)
  node = copy
  role = server
  listen = 127.0.0.1:PORT
  allow_cn = primary
  cert = pki/copy.pem
  key = pki/copy.key
  ca = pki/ca.pem
  [set db]
  root = /path/to/the/copy
  accept = target
