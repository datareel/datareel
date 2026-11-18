# Troubleshooting and support for DataReel 7

AI-assisted troubleshooting follows the rules in `AGENTS.md`: written
management approval and approved enterprise AI models only ("Before you
use AI assistance"), a person in the loop for every step (section 0),
read-only checks first, and backups of every database file before any
change (section 1). The agent assists; the people decide and run the
changes, and problems the agent cannot solve go to DataReel through the
support summary below.

## Read-only checks

Run these first and keep their output for the support summary. Run the
file tools on copies of the files, with the programs that use them
stopped.

```sh
cat version.txt                                   # DataReel version
grep -E 'DATAREEL_(64BIT_DATABASE|NATIVE_TYPES|SSL)|BUILD_SHARED_LIBS' cmake-build/CMakeCache.txt
tests/run_tests.sh cmake-build                    # library self-test
db_debug copy/file.gxd                            # data file analysis
ix_debug copy/file.btx                            # B-tree index check
dr_digest copy/file.gxd                           # digest to compare two copies
```

## Problems by area

### Building and linking

| Symptom | Check |
|---------|-------|
| "You must define a target platform and/or compiler" | The program compiles DataReel headers without the `DataReel::datareel` target (`find_package(DataReel 7 REQUIRED)`, `target_link_libraries`). |
| Undefined classes after moving from 6.x | The class was removed in 7.0 or 7.1; see `porting-from-6x.md`. |
| `find_package(DataReel)` fails | The library was not installed (`cmake --install`), or `CMAKE_PREFIX_PATH` does not include the install prefix. |
| OpenSSL not found (Linux) | `openssl-devel` is not installed. |
| A test in `run_tests.sh` fails | Stop. Keep the full output for the support summary; do not change the test. |

### Database files

| Symptom | Check |
|---------|-------|
| A file opens with an error or reads wrong data after a rebuild | The build's `DATAREEL_64BIT_DATABASE` and `DATAREEL_NATIVE_TYPES` must match the build that wrote the file (`building.md`). |
| Records differ between Linux and Windows | The record holds native gx types or pointers; use fixed-width types (`object-database.md`). |
| Keys missing or a batch half written after a crash | The program wrote without a `gxdJournal`, or with `Flush()` instead of `Sync()`. |
| `db_debug` reports damaged blocks or a CRC error | Stop and escalate (below). Restore from backup or repair a copy only with the person's OK. |
| An index does not match its data file | Rebuild the index from the data file on a copy, then compare. |
| A file from a 7.4 beta older than beta 5 | It opens as it is; `gxd_upgrade` converts it to the new layout (`RELEASE_NOTES_7.4.txt`). |

### TLS and network

| Symptom | Check |
|---------|-------|
| Certificate verification fails | The server's chain against the system store, or `SetCAFile()` for a private CA; the host name in the certificate. `openssl s_client -connect host:port -servername host` shows the chain. Never turn verification off outside tests. |
| A plain text server (HTTP, SMTP, POP3, FTP) cannot be reached | DataReel 7 is TLS only; the server must offer TLS (or STARTTLS where the class supports it). |
| Windows and Linux behave differently | The TLS backend: Schannel (Windows default) or OpenSSL (`DATAREEL_SSL`). |
| A call returns -1 | Read `Error()` on the same object right after the call. |

## Stop and escalate at once

- Damaged files, CRC or checksum errors, or a journal that does not
  recover.
- Any sign of data loss or of two programs writing the same files
  without a lock.
- A possible security problem in DataReel (a crash on damaged input, a
  length check that can be passed, TLS accepting a certificate it should
  refuse). Report it privately to DataReel, not in a public issue.
- Anything that needs a change to the on-disk or wire format.

## Support summary

When a problem is not solved, give the person a summary they can send to
their team or to DataReel (https://datareel.com/contact.php):

1. DataReel version, OS, compiler and build options (32/64-bit engine,
   native types, TLS backend, static or shared).
2. The `tests/run_tests.sh` result on that build.
3. `db_debug` and `ix_debug` output from copies of the files involved.
4. The smallest program or steps that show the problem, the error text
   and the log lines around it, with times.
5. What happened, when, what changed before it, and everything already
   tried with its result.

Leave out passwords, private keys and customer data, and remove host
names, addresses and other site data when the person asks for that.
Projects maintained by DataReel Software Development report DataReel
issues through DataReel's internal handoff process instead of patching
their own DataReel copy.
