DataReel example programs: object database patterns and utilities
===================================================================

These examples (added 11/12/2026) show patterns built from the
library as it is in 7.3. Each one is a single, heavily commented
source file; the comment at the top says what it does, step by step.
They are built with the other examples (examples/CMakeLists.txt) and
run by tests/run_tests.sh. Build them with:

  cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release
  cmake --build cmake-build -j2

The programs are then in cmake-build/examples. Run each one in an
empty folder: they write their files in the current directory.

Two of them use gxStream, which always uses TLS, so they need a
certificate and key for localhost. A self-signed one is enough for a
test; the client then checks the server with --ca cert.pem:

  openssl req -x509 -newkey rsa:2048 -nodes -keyout key.pem -out cert.pem \
    -days 30 -subj "/CN=localhost" \
    -addext "subjectAltName=DNS:localhost,IP:127.0.0.1"


Typed objects (examples/oodb/typed_objects.cpp)
-----------------------------------------------
A C++ class (TradeEvent) packs itself into a block with a type header
(type code, version, size). A client sends the objects over gxStream
to a server thread that stores them in a gxDatabase file, reads each
one back by address, then the program exports the file to plain text
(a key=value line per object under a comment that says it in words),
imports the text into a second database and compares every field.

  typed_objects cert.pem key.pem --ca cert.pem

Output: "sent id 1001, stored at 128, read back OK" for each object,
the export and import counts, the first lines of typed_objects.txt and
"All objects match".


Journal crash and recovery (examples/oodb/journal_recovery.cpp)
---------------------------------------------------------------
A child process writes batches of records, one gxdJournal transaction
per batch. The parent kills it with SIGKILL in the middle of a batch,
runs gxdJournal::Recover() and checks that the database holds only
whole batches and every committed one. It also shows Rollback().
POSIX only (fork and kill), so it is not built on Windows.

  journal_recovery [rounds]      (default 5)

Output: one line per round, for example "killed after 6 committed
batches; Recover() wrote 7 transactions from the journal; database
holds 7 whole batches (the last commit beat its progress note) -
consistent", then the rollback line.


US trading sessions (examples/general/trading_sessions.cpp)
-----------------------------------------------------------
CDate (the 4-byte date to keep in records) and the systime.h time
functions, with the calendar math CDate does not do: which session a
time in New York is in (pre-market 4:00-9:30, regular 9:30-16:00,
post-market 16:00-20:00 ET, closed otherwise), weekends, the 2026
NYSE holidays and early closes, the trading days in a month and the
next trading day.

  trading_sessions                 (samples, a month, and now)
  trading_sessions 11/27/2026 13:05

Output: a table of sample times with their sessions and "ok", the
trading days in November 2026, next-trading-day lines and the current
session in New York.


String, base 64 and CRC-32 utilities (examples/general/string_utils.cpp)
------------------------------------------------------------------------
strutil.h (trim, split, case, find, replace, format, number
conversion, fixed-field copies, natural ordering), gxs_b64.h (base 64
and HTTP basic authentication) and gxcrc32.h (buffer and file
checksums). Every line is checked against a known answer.

  string_utils [file]

Output: one line per function with its result, and the file's CRC-32
computed two ways when a file is given.


Day-organized event database (examples/oodb/day_events.cpp)
-----------------------------------------------------------
One gxDatabase file per day in YYYY/MM/DD folders. Events are
appended in time order to their day's file; a time-range query opens
only the days in the range, skips days with no file and stops reading
a day once it passes the end time.

  day_events [folder]              (default ./day_events, rebuilt)

Output: the day files written, the query range, the events it found
(from 3 day files; the gap day has none) and "The query matches the
events written".


Replication (examples/oodb/replication.cpp)
-------------------------------------------
Database replication with gxrepl (utils/gxrepl/README.md and
docs/replication_admin_guide.md), object by object (DataReel 7.4).
A primary writes records and keeps two indexes made from them, a
B-tree of names and a vector index (replication_records.h). gxrepl
copies the committed pages of the records to a copy and never sends
the indexes: the copy keeps its own, updated object by object from the
records each commit changed (derived_apply = incremental), with the
example's extractor "example_records" built into the copy's gxrepl
(replication_gxrepl, from replication_gxrepl.cpp and gxrepl's own
source). When the copy cannot be sure of an index (a damaged stamp, a
gap in the ship log, an extractor error) it rebuilds that folder's
indexes in full on its own (replication rebuild), says so in its log,
and goes back to object by object. A full rebuild on every change is
still a set's own choice: derived_apply = rebuild.

  examples/oodb/replication_walkthrough.sh BUILD/examples

The walkthrough (POSIX, the openssl command for throwaway
certificates) runs a primary and a copy on throwaway folders over
127.0.0.1: writes, then the derived status, a simulated fault (the
copy's vector index stamp damaged), the log lines of the rebuild, the
way back to object by object, and the two indexes' digests, the same
on both nodes. It exits 0 when every check passes.

  replication write DIR [--commits N] [--per N] [--sleep MS] [--seed N]
  replication rebuild DIR [LSN]
  replication digest DIR

gxrepl, and so replication.cpp and its walkthrough, run on Linux and
other POSIX systems only. On Windows see the folder copy example below.


Folder copy (examples/oodb/replication_copy.cpp, Windows and Linux)
--------------------------------------------------------------------
The Windows replication example (DataReel 7.4). Live replication with
gxrepl is POSIX only; on Windows, and anywhere a copy minutes old is
fine, a database folder is copied with a plain file copy while its
writer is stopped, and checked with digests. The records and their two
indexes are the replication example's (replication_records.h). The
writer changes all three files in one gxdJournal transaction per commit
and closes the journal (a checkpoint) when it stops, so the files hold
every commit. The digest prints the records (address and bytes), the
B-tree's keys in order (the example's own key walk) and the vector
index's live nodes; it is equal on both folders. A copy that loses its
indexes makes them again from its own records (rebuild).

  examples\oodb\replication_copy_walkthrough.cmd BUILD\examples\Release BUILD\utils\Release

The walkthrough writes, stops the writer, copies the folder with
robocopy /MIR (the journal left out) and compares the digests; writes
more, copies the changed files and compares again; then deletes the
copy's two indexes, rebuilds them and compares again. vix_rebuild
--digest checks both vector indexes as well. It exits 0 when every
check passes; KEEP=1 keeps its work folder. tests/run_tests_win.sh runs
it, and tests/run_tests.sh runs the same steps on Linux with cp.

  replication_copy write DIR [--commits N] [--per N] [--seed N]
  replication_copy digest DIR
  replication_copy rebuild DIR
