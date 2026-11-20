# DataReel 7.4 known issues

This file lists the known limits, untested areas and flaky tests of the
7.4 line. Fixes for 7.4 go on the 7.4.1 branch. Each entry says what
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
- **Commit size.** One gxObjRepl commit holds at most 60 MB of changes,
  and one object at most 32 MB. Split larger batches into several
  commits.

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
