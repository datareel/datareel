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
This example builds replication by hand from gxDatabase and
gxStream, in two processes over localhost, to show how it works. For
real use, the library has replication built in since 7.3.1: the
gxrepl tool (utils/gxrepl/README.md) and its classes (gxdrepl.h,
gxrepl.h). The primary
makes each change (insert, update, delete) to its own database file
and ships it over gxStream; the replica applies it to its file in the
same order, so records land at the same addresses. At the end the
primary compares record counts and CRCs, and the two files are byte
for byte the same.

  replication demo cert.pem key.pem --ca cert.pem      (POSIX: forks the replica)

or in two terminals (any system):

  replication replica 0 cert.pem key.pem     (prints the port it got)
  replication primary PORT --ca cert.pem

Output: the replica's port, the primary's steps (20 inserts, 5
updates, 4 deletes, 3 inserts that reuse deleted blocks at the same
address on both sides), the two checksums and "The replica matches
the primary". primary.gxd and replica.gxd compare equal with cmp.
