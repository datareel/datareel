# DataReel 7.5 known issues

This file lists the known limits, untested areas and flaky tests of the
7.4 and 7.5 line. 7.4.1 gets no more fixes: fixes go into 7.5. Each entry says what
happens and what to do about it today.

## Limits

- **A gxObjRepl root has one writing process.** `gxObjRepl::Open()`
  takes an exclusive lock on `ROOT/objrepl.lock`, so a second process
  cannot open the same root as a node. Since 7.4.1 other processes read
  a running node with `gxObjReplReader` (gxobjrepl.h): whole commits
  only, at most a few commits behind the node, and the 64-bit engine
  only. A reader call can be refused while the node creates a new
  database file or a copy takes a full send; try it again.
- **Reader vector searches load the index.** `gxObjReplReader::Nearest()`
  keeps a vector index in memory and reads it again after the node
  commits, so frequent searches on a busy node with a large index read
  the whole index often. On Windows the loaded index file stays open
  until `ReleaseVectors()`, which can make a copy's full send fail
  while it is held.
- **An incremental rejoin holds the new primary's lock** (7.5) while it sends the changed
  objects, so the primary's writers wait for it. The change set is capped by `rejoin-max-share`
  (default 50% of the objects); past that a full send, which does not hold the lock, goes instead.
- **Commit size.** One gxObjRepl commit holds at most 60 MB of changes,
  and one object at most 32 MB. Split larger batches into several
  commits.
- **A copy's index builds are slow on a slow disk.** A copy keeps its
  B-tree and vector indexes up to date as it applies each commit. On a
  slow cloud disk (AWS EBS, 11/19/2026) that was the bottleneck: about
  1 commit a minute with indexes against about 6 with no index spec
  (ids only). On a slow disk, give a copy no index spec when nothing
  reads its indexes there.
- **Full key-order scans are slower than SQLite.** The data and the
  index are separate files, so a scan in key order reads the data in
  random order: about 652k records a second against SQLite's 7.4M
  (1M records, the journal on, SQLite in WAL mode). Read in id order
  (`IdsAfter()`, Python `ids(after=, limit=)`), or by key range
  (`FindRange()`), where a scan of the whole store is not needed.

## Untested areas

- **Serial ports on Windows:**
  - Ctrl+C and Ctrl+Break during a blocking read have not been tested
    (they need an interactive console).
  - Port names COM10 and up, which get the `\\.\` prefix automatically,
    have not been tested on a real port.
  - XON/XOFF flow control has no automated test, because it needs two
    connected COM ports.
- **Benchmarks:** LMDB and RocksDB have not been run against the
  DataReel OODB benchmark.
- **Fuzzing:** the libFuzzer targets have had short runs only.

## Flaky tests

These pass when run again on their own. If one fails in a full run on
a loaded machine, rerun that test by itself before treating it as a
bug.

- `utils/objrepl/test_objrepl_iofaults.sh`: the "primary fsync EIO
  once" case, under heavy load.
- `utils/objrepl/test_objrepl_review.sh` case 3 "idle", at a load of 20 (it failed once in 46 s on
  11/21/2026 and passed on its own right after).
- `imap_container` in `tests/run_tests.sh`: the Dovecot test container
  sometimes starts too slowly.
- Windows only: `test_objrepl_check.sh` "stopping clean" (the status
  file read empty once in a full Windows pass on 11/20/2026; three runs
  on its own passed).
- 32-bit builds only: `stream_blocks` and
  `stream_client_identity_logged` in `tests/run_tests.sh`, under load.

## Test harness notes

- `gxdbmux_test` run by itself needs `GXDBMUX_PKI=DIR`, where DIR is a
  folder made by
  `utils/objrepl/objrepl_certs.sh DIR --copy-host 127.0.0.1 --apply`.
  `tests/run_tests.sh` sets it for you.
