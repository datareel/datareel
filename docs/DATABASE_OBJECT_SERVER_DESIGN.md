# DataReel database object server (port 2073): design, phase 2

Status (11/09/2026): phase 1 (gxrepl protocol 2, databases only, safe reads, power-off fixes) is
done. Phase 2 (steps 1 to 6) is built and tested on branch gxrepl-phase2:
1. gxDatabaseServer (`gxdbserver.h`)
2. the multiplex (`gxdbmux.h`)
3. replication as a service on channels: protocol 3, durable ACK, retention floor
4. the object service (`gxdbobject.h`) and examples (`utils/dbserver`)
5. the power-loss simulator (`utils/gxrepl/powerloss_shim.c`) and its tests, which found and fixed
   two power-loss gaps (section 12)
6. these docs: protocol 3 and the object service on the wire (section 11)

The suite (`utils/gxrepl/test_gxrepl.py`, 35 tests) passes, and the whole suite is clean under
ThreadSanitizer. See `utils/gxrepl/README.md`. Section 10 still has open questions for dougg.

## 1. What exists today

| Piece | Where | What it does |
|---|---|---|
| Block protocol | `gxheader.h` | Every gxStream block has a header with a status: `A` add object, `C` change, `D` delete, `R` request object, `S` send data, `W` ack, `L` close, `K` kill, `T` restart |
| gxStream | `gxstream.h/.cpp` | Sends and receives blocks over TLS (`gxsTLSConnection`). A block length is checked against `SetMaxBlockSize()` before memory is allocated |
| gxStreamServer | `gxstream.h` | TLS listener with mutual TLS required by default (client CA, CN and fingerprint allow list) |
| gxrepl | `gxrepl.h/.cpp`, `utils/gxrepl` | Replication: each message is an `S` block (op, flags, length, body). Handshakes run off the accept loop (`handshake_timeout`, `max_handshakes`). One request at a time per connection |
| Example | datareel `examples/tls/stream_server.cpp` | Single-threaded server for one `gxDatabase`: add and request only |

There is no reusable object-server class, no multi-database session, no multiplexing and no flow
control.

## 2. Goal

There will be one server on 2073: the **DataReel database object server**.
- **Object access** (the port's primary use) and **replication** run as services of that server.
- They share one listener, one TLS context, the allow list, the handshake threads and the limits.
- **One connection carries many databases at once.** Each database gets its own channel, so one
  large or slow database never holds up the others.
- **Crash and power-off safety on both sides.**

## 3. Library layout (vendored DataReel 7.3.x)

| New file | Content |
|---|---|
| `include/gxdbserver.h`, `src/gxdbserver.cpp` | `gxDatabaseServer`: the listener, handshake thread, sessions, service registry, scheduler |
| `include/gxdbmux.h`, `src/gxdbmux.cpp` | The multiplex: channel table, frames, credits, per-channel queues, the I/O thread |
| `include/gxdbobject.h`, `src/gxdbobject.cpp` | Object service: add, change, delete, request on a named database |
| `gxrepl.h/.cpp` | The replication service on the same channels (`gxReplServer` becomes a service) |
| `utils/gxrepl` | Thin front end: `gxrepl serve` starts `gxDatabaseServer` with both services |
| `utils/dbserver/` | `object_server.cpp` and `object_client.cpp`, in DataReel example style |

OSA supplies only configs and `automate/` scripts. No OSA logic goes into the library.

## 4. Wire protocol 3: the multiplex

Every frame is still one gxStream block, so the framing and its size checks stay the same.
- Status `S` blocks carry mux frames.
- `A`/`C`/`D`/`R` keep working, unchanged, for single-database legacy clients (the
  `stream_server.cpp` style) on a session that never opened a channel.

Mux frame body: `u8 'M', u8 kind, u32 channel, u64 seq, u32 flags, payload` (exact layout in section 11.3).

| kind | Direction | Meaning |
|---|---|---|
| `HELLO` | both | protocol 3, node, FAU size, page size, byte order, zlib, max frame |
| `OPEN` | client → server | `channel, service (object\|repl), database or set name, mode (read\|write\|push\|pull)` |
| `OPENED` / `REFUSED` | server → client | the channel is ready (with initial credits) or refused, with the reason (that channel only) |
| `DATA` | both | a service message on a channel (an object op, or a replication op: streams, readlog, diff, apply...) |
| `CREDIT` | both | `channel, bytes`: the receiver allows this many more bytes on the channel |
| `ACK` | receiver → sender | `channel, seq, applied_lsn`: durable ack (section 6) |
| `ERROR` | both | `channel, code, text`: closes that channel only |
| `CLOSE` | both | closes a channel (channel 0: the whole connection) |

- **Per channel:** order (by `seq`), commit atomicity, flow control and errors are all per channel.
  An error or refusal closes only its channel. The connection and the other channels go on.
- **Sender scheduling:** deficit round-robin by bytes over the channels that have data and credit.
  The quantum is 256 KB, and a frame is at most `max_message`. A database with a large backlog
  sends one quantum a turn, so a small database's commit goes out within one round.
- **Commit framing:** a replication commit can span several frames, with `flags` bit 0 meaning
  "last frame of the commit". The receiver applies a commit only when its last frame has arrived
  (section 5).

## 5. Receiver: one reader, one queue and one applier per database (dougg)

```
socket -> reader thread (demux only) -> channel queue (bounded) -> applier thread -> database journal -> files
                                     -> channel queue             -> applier thread -> ...
```

- **Reader thread, one per connection:** it only reads frames and appends each one to its
  channel's queue. It never applies anything, so a slow apply never blocks the socket.
- **Bounded queue per channel** (for example 16 MB):
  - The receiver grants credits only for free queue space.
  - When a queue fills, that channel's credits stop. Only that database's sender waits; the others
    keep flowing.
- **Applier per database:**
  - It drains its own queue in `seq` order.
  - It takes a commit only once all of the commit's frames are in the queue.
  - It applies the commit whole, through that database's own apply journal (`ApplyPages`: journal
    synced, pages written, files synced, journal cleared).
  - All databases apply in parallel and in real time.
- **Notify readers:**
  - After each applied commit, the applier bumps a per-database applied-generation counter and
    signals local readers: object-access clients and GUIs subscribed to that database.
  - Readers re-read under `gxdJournalReadLock` and never see a half-applied commit.
- **Status:** per channel, the queue depth (frames and bytes), the credits, the applied LSN, the
  lag behind the sender's LSN, and the last error.

## 6. Crash and power-off safety (both sides)

Sender:
- The ship log record is fdatasynced before the commit's pages reach the files. Done in phase 1:
  `gxdJournal` sets the ship log's sync from its own sync level. A new segment's folder is synced.
- The sender keeps records until the receiver's **durable ACK** for that channel. Phase 1 keeps them
  by age and size (7 days or 2 GB) and falls back to a whole-file verify. Phase 2 adds a retention
  floor at the lowest ACKed LSN of any active copy, capped by a disk limit.
- After a reconnect, the sender resends from the copy's ACKed LSN (`OPEN` returns it). Frames in
  flight are never trusted: unACKed frames simply come again.

Receiver:
- The apply journal is synced before pages are written, and the files are synced before the
  journal is cleared (`ApplyPages`/`Recover`).
- The applied LSN per database is written with a temporary file, fdatasync, a rename and a folder
  sync (phase 1 added the folder sync).
- The `ACK` goes out only after that. The in-memory queues are never trusted after a restart.
- `Recover` runs at start, before a channel opens.
- Files and folders an apply creates have their folders synced (phase 1).

Both:
- Torn tails are cut by checksum: ship log records (rec_check), journal commits (jcheck).
- Status and lock files are written with a temporary file, a rename and a folder sync.

Gaps found after phase 1, and where they stand (11/09/2026):
1. **Open.** An `ApplyFiles` batch over 1000 files is split into several journal transactions, so it
   is not atomic across the parts. Only a first copy or a whole-set verify sends that many files at
   once; each part is atomic, and a crash between parts leaves files that the next pass sends again.
2. **Open.** A database read in several parts without a ship log can leave the copy holding a mixed
   version until the retry completes (its mtime is 0 so the next scan sends it again). Live
   databases should have a ship log; the others are published by a swap, which is read safely.
3. **Fixed (step 5).** A delete now syncs the folders it removed files from.
4. **Fixed (step 3).** Retention follows the copies' durable ACKs (the floor in `acked/`), capped
   by the disk limit.
5. **Fixed (step 5).** The power-loss tests (section 12).
6. **Fixed (step 5, found by the power-loss test).** A new database or journal file's name was not
   durable: commits synced the journal file but never its folder, so a power cut soon after a
   database was made could lose the file, commits and all. `gxdFPTRCreate()` now syncs the folder
   after every create (POSIX).
7. **Fixed (step 5, found by the power-loss test).** A crash in the middle of a floor write left
   `acked/<copy>.gxrtmp`, which the ship-log purge read as another copy's floor. Purge skips it now.

## 7. Object service

- `OPEN service=object database=NAME mode=read|write`. The server config lists exported databases:
  `[db NAME] path = ... access = read|write`.
- A copy that replication writes is always read-only for object clients. A write `OPEN` on it is
  `REFUSED`.
- `DATA` ops map to the `gxDatabase` calls behind A/C/D/R (Alloc+Write, Write, Delete, ObjectLength+Read).
- Writes go through the database's `gxdJournal`. With a ship log on, a remote write replicates like
  a local one.

## 8. Tests (phase 2)

Mux and queues:
- Several databases replicating at once over one connection, with interleaved commits. Each copy
  matches its own commit at every check (`gxrepl_testwriter check` after each applied generation).
- One database refused (unsafe path, a `writer_lock` held, an unknown set) while the others continue.
- A large, slow database (artificial apply delay) never blocks a small one. The small database's lag
  stays under one round.
- Queue full: one channel's credits stop. The others keep their rate, and the full one catches up
  after the delay.

Object service:
- Add, request, change, delete over TLS.
- Read-only refusal on a replicated copy.
- Object and replication clients on one server.

Crash and power-off:
- kill -9 at random points on both sides during a multi-database apply. Each copy is always a whole
  commit and catches up with no gap or duplicate.
- **Power-loss shim** (built in step 5 as an `LD_PRELOAD` library, section 12): it keeps a "durable
  image" updated only by fsync/fdatasync, including folder entries on a folder sync. At a random
  moment the test kills one or both sides, rebuilds the folders from the durable images only,
  restarts, and checks:
  - each copy is a whole commit
  - the sender's log is not behind its files
  - catch-up ends with no gap or duplicate
- Torn ship-log tail and torn journal tail (phase 1 has the ship-log one).

## 9. Estimate

| Part | Work |
|---|---|
| `gxDatabaseServer` (listener, sessions, handshake reuse, service registry) | 0.5 day |
| Mux, scheduler, credits, per-channel queues, appliers, status | 1 to 1.5 days |
| Replication as a service on channels (protocol 3), durable ACK, retention floor | 1 day |
| Object service and examples | 0.5 day |
| Power-loss shim and the tests above | 1 day |
| Docs, release notes, both vendored copies | 0.5 day |

About 4 to 5 days in total, delivered in that order, with the suite green at each step.

## 10. Open questions

- Should legacy `A/C/D/R` clients without `OPEN` stay allowed (one default database per server
  config), or be refused?
- Failback direction: switch a set to pull automatically while DRCM says AWS is active (the failback
  flag), or as a manual `gxrepl once --pull`?
- Retention disk cap per stream when a copy is down for days (today 2 GB).

## 11. Language-neutral spec of what is on disk and on the wire (for ports, e.g. DataReel 6.3)

These are the formats of DataReel 7.3.1 with replication protocol 3 (phase 2, 11/09/2026). All
integers are in the **native byte order** of the writer. The hello refuses a peer with another byte
order, address size or page size. Strings (`str`) are `u32 length` followed by that many bytes, with
no terminator. `bytes` is `u64 length` followed by the bytes. Paths are relative to a set root,
`/`-separated, and never contain `.`, `..` or empty components.

Protocol 3 replaced protocol 2 on 11/08/2026: a protocol 2 peer's HELLO is refused (the protocol
number differs), so both nodes of a pair are updated together.

### 11.1 gxStream block (the transport, TLS only)

Each block is a header and then `block_length` bytes. The header is sent as the C struct
`gxBlockHeader`:

| Field | Type | Value |
|---|---|---|
| block_check_word | u32 | `0xFEFE` |
| block_length | u32 | bytes that follow the header |
| block_status | u32 | bits 8-15: the net status (`'A'` add, `'C'` change, `'D'` delete, `'R'` request, `'S'` send, `'W'` ack, `'L'` close, `'K'` kill, `'T'` restart); bits 0-7: the file status (`'V'` remote device block) |
| (padding) | 4 bytes | only in the 64-bit engine (FAU is 8 bytes): zero |
| block_nd_fptr | FAU (u32 or u64) | 0 on the wire |

The header is 16 bytes in the 32-bit engine and 24 bytes in the 64-bit one. A receiver checks
`block_length` against its limit before it allocates any memory. Blocks are never sent unencrypted.

### 11.2 Sessions: what the first block decides

The server (`gxDatabaseServer`) reads one block after the TLS handshake and gives the connection to
the first service that claims it:

| First block | Session |
|---|---|
| `'S'`, body starts with `'M'` (0x4D) | Multiplex session (11.3): replication and objects, many channels |
| `'S'`, body is a replication message with op 1 (HELLO) | Plain replication session: one request at a time, no channels (tools and tests use it) |
| `'A'` or `'R'` | Legacy object client (11.6), only if the config names a `legacy_db` |
| anything else | Closed at once |

A replication message never starts with 0x4D (op codes are 1 to 101), so the two `'S'` kinds cannot
be mixed up.

### 11.3 Multiplex frame (gxdbmux.h)

Each frame is one gxStream block (11.1) with net status `'S'` and file status `'V'`. Its body:

| Offset | Field | Type | Value |
|---|---|---|---|
| 0 | mark | u8 | `'M'` |
| 1 | kind | u8 | table below |
| 2 | channel | u32 | 0 = the connection; 1 and up = channels |
| 6 | seq | u64 | DATA: counts 1, 2, 3... per channel and direction; 0 for the rest |
| 14 | flags | u32 | bit 0 (`LAST`): the last DATA frame of a message |
| 18 | payload | | by kind |

| kind | Name | Payload | Rules |
|---|---|---|---|
| 1 | HELLO | `str "GXDBMUX", u32 version (1)` | Channel 0. The client sends it first and the server answers with its own. Another magic or version ends the connection |
| 2 | OPEN | `str service, str name, str mode, u64 credit` | The opener picks a free channel id. `credit` is the opener's receive queue size: the accepter may send that many DATA bytes |
| 3 | OPENED | `u64 credit, bytes extra` | The channel is open. `credit` is the accepter's queue size. `extra` is service data (empty today) |
| 4 | REFUSED | `str reason` | Closes that channel only |
| 5 | DATA | service bytes | Counted against the credit. A message is DATA frames up to the one with `LAST`, joined. A frame's payload is at most 1 MB |
| 6 | CREDIT | `u64 bytes` | The receiver took that many bytes out of its queue: the sender may send that many more. Sent each time a quarter of the queue is taken, or when the queue empties |
| 7 | ACK | `u64 seq, u64 lsn` | Durable apply (11.4) |
| 8 | ERROR | `str reason` | Closes that channel only. A peer that sends past its credit gets `ERROR "over credit"` |
| 9 | CLOSE | (none) | Channel 0: the connection. Otherwise that channel |

Rules:
- A frame for a channel that is closed is dropped (late frames are normal).
- Each side owns one I/O thread per connection and sends control frames (HELLO, OPEN, OPENED,
  REFUSED, CREDIT, ACK, ERROR, CLOSE) before DATA.
- DATA from different channels is sent by deficit round-robin, 256 KB a turn. That is not part of
  the wire format, but it is what keeps a large set from holding up a small one; a port should do
  the same.
- The default queue (and so the initial credit) is 16 MB per channel. A receiver may grant less,
  never more than it can hold.

### 11.4 Replication on the multiplex (service `repl`)

1. The client sends HELLO (11.3) and waits for the server's.
2. It sends `OPEN channel 1, "repl", name "", mode "control"`. The server accepts one control channel
   per connection.
3. On channel 1 it sends the replication HELLO (11.5, op 1). Only HELLO is allowed on channel 1, and
   HELLO is not allowed anywhere else.
4. It sends `OPEN channel N, "repl", <set>, push|pull` for each set, with N = 2, 3... The server
   refuses a set it does not have, `push` to a set it is the source of, or `pull` from a set it is
   the copy of. A refused set does not stop the others.
5. Each set channel carries requests and replies (11.5) for its own set only, in order. The server
   serves each channel on its own thread, so sets apply in parallel.
6. After an APPLYLOG is applied durably (the pages are synced and the applied position is written,
   11.9), the server sends the OK reply and then `ACK seq lsn` on that channel: seq is the request's
   last frame and lsn the applied lsn. The client writes the retention floor (11.7) from the OK
   reply's applied lsn, which the server only sends once the apply is durable. The ACK carries the
   same lsn for the channel status.

### 11.5 Replication message (a DATA message on a repl channel, or an `'S'` block on a plain session)

| Field | Type | Value |
|---|---|---|
| op | u8 | operation (table below); `100` OK, `101` error (body: `str` message) |
| flags | u8 | bit 0: the body is zlib-compressed (only if both sides said zlib in the hello) |
| raw_length | u32 | length of the uncompressed body; refused above `4 * max_message + 1 MB` |
| body | bytes (raw) | the uncompressed body, or a zlib stream that inflates to `raw_length` bytes |

Every request gets exactly one reply message (OK or error), in order, on the same channel (or the
same plain session). On a plain session a `'L'` block closes the session.

| op | Name | Request body | OK reply body |
|---|---|---|---|
| 1 | HELLO | `str "GXREPL", u32 protocol (3), str node, u32 sizeof(FAU) (4\|8), u32 page size (4096), u32 0x01020304, u32 zlib (0\|1)` | `u32 protocol, str node, u32 n, n x (str set, str accept), u32 zlib` |
| 10 | STREAMS | `str set` | `u32 n, n x stream` |
| 11 | READLOG | `str set, str stream, u64 from_lsn, u64 max_bytes` | `u32 status (0 ok, 1 gone), u64 next_lsn, bytes records` |
| 12 / 21 | SMANIFEST / TMANIFEST | `str set` | `u32 n, n x file` |
| 13 | DIFF | `str set, diffreq` | `u32 status (0 ok, 1 no file, 2 apply log first, 3 changed while read), u64 next, u32 more, u64 pieces, u64 sent, update` |
| 20 | STATE | `str set` | `u32 n, n x streamstate` |
| 22 | HASHES | `str set, str path, u32 piece` | `u32 exists, u32 n, n x u64 hash` |
| 23 | APPLYLOG | `str set, str stream, str log_id, bytes records` | `u64 applied_lsn` |
| 24 | APPLYFILES | `str set, u32 n, n x update` | `u32 n, n x u32 result (0 applied, 1 stale)` |
| 25 | DELETE | `str set, u32 n, n x str path` | (empty) |
| 26 | SETSTATE | `str set, streamstate` | (empty) |

Records:
- `file` = `str path, u64 size, i64 mtime_ns, u32 mode, u8 covered`
- `stream` = `str name, str log_id, str journal, u64 first_lsn, u64 next_lsn, u32 n, n x str file`
- `streamstate` = `str name, str log_id, u64 lsn`
- `diffreq` = `str path, u32 piece, u64 start, u64 max_bytes, streamstate copy, u32 n, n x u64 hash`
- `update` = `str path, u64 size, i64 mtime_ns, u32 mode, u8 last, u32 n, n x (u64 offset, bytes data), str stream, str log_id, u64 lsn, bytes records`

Piece hash: `gxdReplHash(data, n, 0)` over each 64 KB piece. It's a 64-bit multiply-xor hash (the
reference is in `gxdrepl.cpp`, constants `0x9E3779B97F4A7C15`, `0xC2B2AE3D27D4EB4F` and the
four lane seeds). A port must reproduce it exactly.

Rules both sides enforce:
- A path must be a database file (`.gxd`, `.btx`, or the set's `include` patterns).
- It must not be hidden, excluded or databento, and must not lead through a symlink out of the set.

### 11.6 Objects on the multiplex (service `object`) and legacy object clients

`OPEN channel N, "object", <database>, read|write`, after the multiplex HELLO (no replication HELLO
is needed). The server refuses:
- a database its config does not name (`[db NAME]`);
- `write` on a database whose `access` is `read`;
- `write` on a database inside a set this server receives (a replicated copy).

Each DATA message is one request: `u8 op`, then its body. Each reply is `u8 status` (0 ok, 1 error),
then the result, or `str error`.

| op | Name | Request body | Result |
|---|---|---|---|
| 1 | ADD | `bytes data` | `u64 address` |
| 2 | REQUEST | `u64 address` | `bytes data` |
| 3 | CHANGE | `u64 address, bytes data` (the object's own length) | (none) |
| 4 | DELETE | `u64 address` | (none) |
| 5 | FIRST | (none) | `u64 address` (0: none) |
| 6 | NEXT | `u64 address` | `u64 next address` (0: none) |

How the server handles requests:
- Each change (ADD, CHANGE, DELETE) is one `gxdJournal` transaction, committed durably before the
  reply. An answered change survives a power cut (`test_power_loss_object_service`).
- A read of a replicated copy holds `gxdJournalReadLock` on the set's `apply.jnl`, so it never sees
  half of a commit.

A legacy object client (the 6.x gxStream style) uses no channels. It talks to the config's
`legacy_db`:
- an `'A'` block with the object's bytes gets `gxStream::WriteResultBlock(address)`;
- an `'R'` block with an 8-byte address gets one block with the object's bytes (empty if there is
  none);
- `'L'` ends the session, and any other block closes it.

### 11.7 Ship log (sender, `<set root>/.gxrepl/log/<stream>/`)

| File | Content |
|---|---|
| `log.id` | 16 hex digits and a newline: a new id when the folder is made |
| `writer.lock` | flock held by the one writer |
| `journal` | the journal's path, relative to the root |
| `files` | the journal's database files, one relative path per line |
| `<first lsn as 16 hex>.gxl` | segments |
| `acked/<copy>` | the retention floor (protocol 3): one line, the decimal lsn that copy has applied durably. `<copy>` is the copy's node name with every character other than letters, digits, `-`, `_` and `.` made `_` |

These small files are written with a temporary file (`<name>.gxrtmp`), fdatasync, a rename and a
folder sync. A reader skips names that start with `.` or end in `.gxrtmp` (a write a crash stopped).

Purge: when a segment fills, the writer removes old segments, oldest first, and never the one in
use. It stops at the first segment that must stay. A segment goes only if both hold:
- it is older than `keep_days`;
- every record in it is at or below the lowest floor in `acked/` (or `acked/` is empty).

The size cap (`max_bytes`) still removes the oldest segments. A copy that then needs a removed record
sees its position below the log's first lsn and is verified whole (hash compare) instead.

Segment layout:
- **Segment header**, 32 bytes: `char[8] "GXRSEG01"`, `char[16]` log id (hex), `u64` first lsn.
- **Record**: `u32 magic 0x52525847 ("GXRR")`, `u32 type (1 commit, 2 open, 3 gap)`, `u64 lsn`,
  `u64 time_us`, `u32 payload length`, `u32 check`, then the payload.
  - `check = (h ^ (h >> 32))` as u32, where `h = gxdReplHash(payload, len, gxdReplHash(header bytes 4..27, 24, 0x5348))`.
- **Commit payload**: `u32 nfiles`, then per file: `str name`, `u64 size after the commit`,
  `u32 npages`, and per page `u64 page number` + 4096 bytes.
- **Order**: lsn starts at 1 and counts up by one for the life of a log id.
- **Durability**: with a journal at sync level NORMAL, each record is fdatasynced before the commit's
  pages are written to the files. A new segment's folder is synced.
- **Torn tail**: the writer's Open() reads the last segment and cuts it back to the end of the last
  record whose magic, length and check are valid.

### 11.8 Apply journal (receiver, `<set root>/.gxrepl/apply.jnl`) and database journals

Both use the gxdJournal file format (`gxdjrnl.cpp`). It's only read back on the machine that wrote it.

| Part | Layout |
|---|---|
| Header | `char[8] "GXDWAL01"`, `u32 version 1`, `u32 salt`, `u32 nfiles`, `u32 page size`, per file `str full path`, `u32 checksum` |
| Page record | `u32 0x4D524657 ("WFRM")`, `u32 salt`, `u32 file index`, `u32 commit number`, `u64 page number`, `u32 checksum`, one page |
| Commit record | `u32 0x544D4357 ("WCMT")`, `u32 salt`, `u32 commit number`, `u32 nfiles`, `u64 size per file`, `u32 checksum` |

How an apply and a recovery work:
- **Apply** (`ApplyPages`): write the records, sync, write the header last. Then `Recover`: replay the
  committed pages, sync every file, and clear the header with a sync.
- **Recovery** stops at the first invalid record. A transaction without its commit record is ignored.

### 11.9 Applied position (receiver, `<set root>/.gxrepl/state/<stream>.state`)

- One line: `<log id> <applied lsn>\n`.
- It's written only after the pages are durable: a temporary file `.gxrtmp`, fdatasync, a rename and
  a folder sync. Only then is the request answered.
- After a restart, the client asks the copy for this position and sends from `lsn + 1`.
- A different log id means a new log on the source, so the copy is verified (hash compare) first.

### 11.10 Status file (both, `status_file`)

JSON, written atomically (a temporary file, fdatasync, a rename and a folder sync). Client fields:

- top level: `node, role, time, connected, peer, since, via, note`
- `sets.<name>`: `direction, ok, last_ok, last_scan, last_verify, last_full_verify, records, files_sent,
  pieces_sent, bytes_sent, deletes, last_error`
- `streams.<name>`: `source, copy, lag`

Server fields: `node, role ("server"), time, port, applied_records, applied_pages`, `clients` (one
string per connection), `sets.<name>: accept, root, last`, and `channels` (one per open set
channel: `set, mode, state, queue_frames, queue_bytes, frames, error`).

## 12. Power-loss tests (step 5)

`utils/gxrepl/powerloss_shim.c` is a test-only `LD_PRELOAD` library. It is never linked into a
program. The test compiles it and loads it into the writer, the gxrepl server and the gxrepl client.
It replaces `fsync` and `fdatasync`. After each successful call on a watched folder (`PL_ROOTS`):
- a file's bytes at that moment go to `PL_DURABLE/inodes/<dev>_<ino>`
- a folder's entries (type, inode, name) go to `PL_DURABLE/dirs/<dev>_<ino>`

Folders are kept by inode, so a renamed folder keeps its entries. Saves are a temporary file and a
rename under a lock, so processes and threads can share one image. `PL_TRACE` logs every call seen.

A cut, in the test: SIGKILL every process on the side being cut. Then the folder is rebuilt from the
image only:
- A name exists only if its folder was synced after the name was made.
- A file has the bytes of its last sync; a name whose bytes were never synced is an empty file.
- In "mix" cycles each file keeps its unsynced bytes or not, at random, because the page cache may
  have written some of them back.

The model is stricter than ext4 or XFS, which commit some metadata on their own. That is the point:
it only lets through what POSIX promises.

| Test | What it shows |
|---|---|
| `test_power_loss_shim_model` | The model itself: unsynced bytes lost, a rename undone without a folder sync, a name lost without one, the safe sync-rename-sync kept |
| `test_power_loss_both_nodes` | 6 cuts of the writer, the client and the server at once (3 with mix), each during live commits and the copy of a 1-5 MB file in parts. After each cut: the source's floor is not ahead of the copy's durable position; the writer recovers; the copy catches up to the same files, `gxrepl_testwriter check` passes on both sides, and the lag is 0 |
| `test_power_loss_source_node` | 4 cuts of the writer's node only (the server keeps running) |
| `test_power_loss_copy_node` | 4 cuts of the server's node only (the client reconnects) |
| `test_power_loss_object_service` | 5 cuts of the server while a client adds and changes objects. Every object the server answered is there with its last answered bytes, and the database walks end to end |

What the tests found (both fixed, section 6 items 6 and 7): a new database file's name was not
synced, and a stopped floor write was read as a floor.

Not covered by the shim: `msync`, `sync_file_range`, `O_SYNC`/`O_DSYNC` writes and `syncfs`.
DataReel uses none of these (only `fsync` and `fdatasync`, `gxdFPTRSync`). Windows has no shim:
NTFS journals its own metadata, and `FlushFileBuffers` is the only call.

## 13. DataReel 6.3 port: legacy DataReel classes, not the C++ standard library (dougg, 11/08)

The 6.3 port must follow the 6.x design: DataReel's own classes and C-style code, and no
`std::` containers, strings, threads, atomics, chrono or filesystem. The formats in section 11 stay
byte-identical, so a 6.3 node and a 7.3 node can replicate with each other. The hello still refuses
a peer with another address size, page size or byte order.

### 13.1 What 6.3 already has (branch `6.3-oodb-backport`, head 36cdebf)

| Piece | State in 6.3 |
|---|---|
| gxDatabase / gxBtree file formats | The same as 7.3 (6.2-compatible) |
| gxdJournal (`GXDWAL01`, `WFRM`/`WCMT` records) and `gxdJournalReadLock` | Present: the same on-disk format as section 11.8 |
| gxdPageCache | Present |
| gxStream / gxSocket | Present, **without TLS**: TLS is only on WIP branch `6.3-wip-net-tls` (1dff199), not in the build |
| gxSSL, gxThread, gxMutex, gxCondition, gxString, gxList, MemoryBuffer, gxConfig, gxdFPTR | Present |
| Ship log, `ApplyPages`, gxrepl, the object server | **Missing** |

Finding: the backported `gxdjrnl.cpp` and `gxdpcache.cpp` on 6.3 still use the standard library
(`std::map`: 19 uses in gxdjrnl.cpp; `<algorithm>`: 9 in gxdpcache.cpp). They don't match the
6.3 design yet either, and should be converted along with the port.

### 13.2 Mapping for the port

| 7.3 code uses | 6.3 port uses |
|---|---|
| `std::string` | `gxString` |
| `std::vector<unsigned char>`, message buffers | `MemoryBuffer` |
| `std::vector<T>`, `std::list<T>` | `gxList` / `gxListNode` (or fixed C arrays where the size is known) |
| `std::map` / `std::set` (path tables, stream states) | a sorted `gxList` searched in order, or an in-memory `gxBtree` for large tables (the manifest of 100k+ files) |
| `std::thread` | `gxThread` (POSIX and Win32) |
| `std::mutex`, `std::lock_guard` | `gxMutex` with explicit lock and unlock |
| `std::condition_variable` (phase 2 queues) | `gxCondition` |
| `std::atomic<int>` counters | an int guarded by a `gxMutex` |
| `std::unique_ptr`, `std::shared_ptr` | owned raw pointers with an explicit delete (the 6.x style) |
| `std::function`, lambdas | static functions or small classes |
| `std::chrono`, `time()` | `time()` and `gettimeofday()` (Win32: `GetSystemTimeAsFileTime`) |
| `std::filesystem` | `gxdFPTR` calls, `opendir`/`readdir` (Win32: `FindFirstFile`) |
| exceptions | none: return codes (`gxDatabaseError`, `gxSocketError`) as in 6.x |
| config parsing (`gxReplConfig::Load`) | `gxConfig` |
| TLS (`gxsTLSConnection`, `gxStreamServer`) | `gxSSL` over `gxSocket`, from `6.3-wip-net-tls` finished first; mutual TLS (client CA, CN and fingerprint allow list) has to be added there |
| `flock` (writer lock, read lock) | `gxdFPTRLock` (fcntl or flock on POSIX, `LockFileEx` on Win32) |
| `fdatasync`, folder fsync | `gxdFPTRSync`; folder sync on POSIX only (NTFS journals its metadata, so it isn't needed on Win32) |
| zlib | optional, the same as 7.3 (`DATAREEL_HAVE_ZLIB`) |
| `gxdReplHash`, record check | the same C code: no std, no change |

### 13.3 Order of work for the 6.3 port (after 7.3 replication is proven)

1. Finish TLS on 6.3's gxStream (`6.3-wip-net-tls`), with mutual TLS.
2. Convert the backported journal and page cache to DataReel classes.
3. Port the ship log (`gxdShipLog`, its reader, `EnableShipLog`, `ApplyPages`).
4. Port gxrepl protocol 2 (databases only, safe reads, power-off rules), then phase 2 when 7.3 has it.
5. Interop tests: a 6.3 node against a 7.3 node in both directions, 32-bit and 64-bit engine, Linux
   and Windows (MSVC). The formats are byte-identical to section 11.
