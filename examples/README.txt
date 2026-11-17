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


Vector index basics (examples/oodb/vector_basics.cpp)
-----------------------------------------------------
The vector index of DataReel 7.4 (gxvindex.h): 4,000 clustered vectors
in float32, float16 and int8 files (their sizes), Exact() against the
graph Search() (recall and time for three search widths), the l2,
cosine and ip metrics, tags and a gxvFilter (a category in a range of
days), Update, Delete, Repair() and Verify(), a gxdJournal transaction
rolled back and Reload(), and the phase D format fields with the
logical digest (the same nodes inserted in another order: another
graph, the same digest).

  vector_basics [WORK_FOLDER]

Records with vector members (examples/oodb/vector_records.cpp)
---------------------------------------------------------------
Fixed records that carry their own vectors (gxvmember.h): each record
and its index node in one gxdJournal transaction, the node id the
record's address; a search, then the records read back by address;
Verify() with a resolver that reads every node's record; and the index
made again from the records alone (gxvRebuildFromMembers, as
vix_rebuild and a replicated copy do), with the same logical digest.

  vector_records [WORK_FOLDER]

A vector set across day files (examples/oodb/vector_days.cpp)
---------------------------------------------------------------
One .vix per day in YYYY/MM/DD folders, searched as one through a
.vixset manifest (gxvset.h): the manifest's lines, a search with 1 and
4 threads (the same results), a day range that skips the other files
from the manifest alone, an archived day skipped and counted (an error
with strict), and a live day whose writer attaches its own open index.

  vector_days [WORK_FOLDER]

Vector searches through the object server
(examples/oodb/vector_search_service.cpp, POSIX)
---------------------------------------------------------------
The database object server's VECTOR_INFO and VECTOR_SEARCH (gxdbobject.h
ops 8 and 7) over gxStream with mutual TLS: make builds three day files
and their manifest, serve answers database "notes" with "vectors =",
search asks for the index's description and the 5 nearest, of every
day or of a day range (the server skips the other files). The program's
comment gives the request and reply layouts.

  examples/oodb/vector_search_walkthrough.sh BUILD/examples

Replication
-----------
11/16/2026: DataReel 7.4 replication is gxObjRepl (gxobjrepl.h). The
gxrepl page-shipping example (replication.cpp, its walkthrough and its
extractor) went with gxrepl; the gxObjRepl examples come with it.


Folder copy (examples/oodb/replication_copy.cpp, Windows and Linux)
--------------------------------------------------------------------
A database folder copied with a plain file copy while its writer is
stopped, and checked with digests (DataReel 7.4), on Windows and
anywhere a copy minutes old is fine. The records and their two indexes
are in replication_records.h. The
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
