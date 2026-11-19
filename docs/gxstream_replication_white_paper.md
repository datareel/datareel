# gxStream and Replication on Port 2073 (White Paper)

**Release:** DataReel 7.4 (the 7.4.1 branch), 11/18/2026. **Audience:** network and security reviewers, firewall administrators, and developers who write gxStream clients and servers or run DataReel replication. Everything in this paper was written from the DataReel 7.4 source: `include/gxstypes.h`, `include/gxheader.h`, `include/gxstream.h`, `src/gxstream.cpp`, `include/gxstls.h`, `src/gxstls.cpp`, `include/gxdbserver.h`, `src/gxdbserver.cpp`, `include/gxdbmux.h`, `src/gxdbmux.cpp`, `include/gxdbobject.h`, `src/gxdbobject.cpp`, `include/gxobjrepl.h` and `src/gxobjrepl.cpp`. When this paper and the code disagree, the code wins.

The exact wire format, with every field, is in the [protocol specification](docs/rfc/draft-gaer-datareel-gxstream-00.html) (`docs/rfc/draft-gaer-datareel-gxstream-00.txt`, an Internet-Draft for the Independent Submission stream). The security design of the library as a whole is in the [DataReel security white paper](docs/security_white_paper.md).

## 1. Port 2073

TCP and UDP port 2073 are registered with IANA for DataReel under the service name `gxs-data-port`, "DataReel Database Socket". The registration was made in the 1990s, when a protocol description was not required with it; this paper and the specification are that description. The DataReel source has carried the services file entries since its early releases (`include/gxstypes.h`):

```
gxs-data-port   2073/tcp   DataReel Database Socket
gxs-data-port   2073/udp   DataReel Database Socket
```

and the port number as constants (`gxsDATA_PORT`, `gxSOCKET_DBS_PORT`, `gxSOCKET_DEFAULT_PORT`, all 2073).

What uses it in DataReel 7.4:

| Use | Port 2073 | Where |
|---|---|---|
| gxStream clients | the default of `gxStream::Connect()` | `gxstream.h` |
| The database object server | the default `listen_port` | `gxdbserver.h` |
| Remote database objects (object service) | carried by the database object server | `gxdbobject.h` |
| Vector searches (7.4) | carried by the object service | `gxdbobject.h` |
| 6.x style clients (legacy add/request blocks) | carried by the object service | `gxdbobject.h` |
| Object replication (gxObjRepl) | the default port of the copy and of the primary's copy address (7.4.1) | `gxobjrepl.h` |

UDP is not used by DataReel 7. The UDP registration is for the legacy DataReel 6.x class gxDatagram, which sends the same blocks over UDP (one datagram for the header and one for the body) in cleartext and with no peer authentication. DataReel 6.x gxStream can also run over plain TCP when a server explicitly opts out of client authentication. Both are legacy transports, described in the specification for completeness and not recommended.

**Why a registered port.** gxStream is a long-lived service protocol between hosts that are configured once and then left alone: a database server, its clients and a replication pair. A fixed, registered port lets the services files, firewall rules, SELinux port labels and network monitoring name the traffic as DataReel database traffic, lets clients connect without being told a port, and keeps DataReel from colliding with another program's choice of an unregistered port. gxStream has been DataReel's database block protocol since 1999, and 2073 its port since the registration.

## 2. The layers

```
 +-------------------------------------------------------------+
 | Services: object service (op 1-8), vector search, legacy    |
 | add/request blocks, object replication frames               |
 +-------------------------------------------------------------+
 | Multiplex (optional): channels with credit flow control     |
 +-------------------------------------------------------------+
 | gxStream blocks: 32-byte header + body                      |
 +-------------------------------------------------------------+
 | TLS 1.2 or 1.3, certificates on both sides                  |
 +-------------------------------------------------------------+
 | TCP, port 2073                                              |
 +-------------------------------------------------------------+
```

- **TCP:** IPv6 and IPv4; a server with no bind address listens on every interface.
- **TLS:** always. gxStream refuses to send or accept a block before the TLS handshake has finished. The server is authenticated by its certificate and host name; the client by its certificate, which must chain to the server's client CA and be named in its allow list (SHA-256 fingerprint or subject common name). `gxStreamServer` requires this by default.
- **gxStream blocks:** the unit of the protocol. Each block is a 32-byte header followed by `block_length` bytes of body.
- **Multiplex:** an optional layer that runs many independent channels (one database each) over one connection.
- **Services:** what the blocks mean: requests to database objects, vector searches, and replication.

## 3. The gxStream block

Every block starts with the same header, the one `gxDatabase` writes before every block in a database file. On the wire it is four 8-byte little endian words, the same on every platform since DataReel 7.4:

```
  0               8               16              24              32
 +---------------+---------------+---------------+---------------+
 |  check word   |    length     |    status     |    result     |
 +---------------+---------------+---------------+---------------+
```

| Word | Contents |
|---|---|
| check word | `0xFEFE`; anything else is a synchronization error |
| length | the number of body bytes that follow (0 for a header-only block) |
| status | byte 0: the block's data status, `V` (a block from a remote device); byte 1: the control code (below); the other bytes are 0 |
| result | a 64-bit result value carried in a header-only acknowledgment (a file address, for example); otherwise 0 |

The high four bytes of the check word and the status words are always 0. A DataReel peer older than 7.4 beta 5 on 64-bit Windows, or a 32-bit build, sent 4-byte words (a 24 or 16 byte header); a 7.4 receiver sees non-zero bytes there and refuses the connection instead of misreading it.

Control codes (status byte 1) used on the wire in 7.4:

| Code | Name | Meaning |
|---|---|---|
| `S` | send block | a body of data: a reply, a multiplex frame or a replication frame |
| `R` | request block | a request; the body names what is wanted (an 8-byte object address for the legacy object service) |
| `A` | add block | add the body as a new object; the reply is a result block with its address |
| `C` | change block | change an object; the request block is followed by an `S` block with the new contents |
| `D` | delete block | delete the object the body names |
| `W` | acknowledge | a header-only acknowledgment; the result word carries the result |
| `L` | close connection | the client is done |
| `K` | kill server | ask the server to shut down |
| `T` | restart server | ask the server to restart |

A receiver checks the length against its maximum block size (64 MB by default, 17 MB on the database object server) before it allocates anything, and refuses a header with a wrong check word or an oversize length. The `K` and `T` codes are defined by the protocol; the 7.4 database object server does not act on them (a session that sends one is closed), and programs that offer them must restrict them to administrators.

## 4. The database object server

`gxDatabaseServer` is one TLS listener on port 2073 shared by several services:

1. The accept loop takes the TCP connection only. The TLS handshake runs in its own thread with a 10 second limit, at most 16 at once, so a scanner or a silent connection cannot keep real clients out.
2. A client whose certificate fails the CA or allow list check is closed. At most `max_clients` (4 by default) authenticated sessions run at once.
3. The server reads the session's first block and gives the session to the first service that claims it:
   - an `S` block whose body is a multiplex HELLO frame goes to the multiplex (section 5);
   - an `A` or `R` block goes to the legacy object service, when the server names a legacy database;
   - anything else closes the session.

## 5. The multiplex

A multiplex session carries numbered channels over one connection. Each frame is one `S` block whose body starts with an 18-byte frame header:

```
 byte 0   'M' (0x4D), the multiplex mark
 byte 1   kind
 bytes 2-5    channel (u32)
 bytes 6-13   sequence number (u64)
 bytes 14-17  flags (u32; bit 0 = the last frame of a message)
 then the payload
```

| Kind | Payload | Meaning |
|---|---|---|
| 1 HELLO | str "GXDBMUX", u32 version (1) | first frame of each side, on channel 0 |
| 2 OPEN | str service, str name, str mode, u64 credit | open a channel to a database (`object`, the database name, `read` or `write`) |
| 3 OPENED | u64 credit, bytes extra | the channel is open |
| 4 REFUSED | str reason | the channel was not opened |
| 5 DATA | service data | part of a message |
| 6 CREDIT | u64 bytes | the receiver has room for this many more bytes |
| 7 ACK | u64 seq, u64 lsn | a durable apply up to seq |
| 8 ERROR | str reason | closes the channel |
| 9 CLOSE | none | closes the channel; on channel 0, the connection |

(str: a 4-byte length and the bytes; bytes: an 8-byte length and the bytes.)

- **Flow control per channel.** Each side opens with the size of its receive queue as credit (16 MB by default). A sender sends DATA only while it has credit, and the receiver returns credit as its consumer takes data. A peer that sends past its credit has the channel closed with an ERROR. So one slow database never blocks the others.
- **Fair sending.** Control frames go first; data frames go by deficit round robin (256 KB a turn), and no data frame is larger than 1 MB, so a large transfer cannot starve a small one.
- **Errors are per channel.** A refused open, an ERROR or a CLOSE ends that channel only. A bad block or a frame that is not a multiplex frame ends the connection.

## 6. The object service

On an open `object` channel each message (DATA frames up to the one with the last-frame flag) is one request: a 1-byte operation and its arguments. The reply is one message: a status byte (0 ok, 1 error), then the result or an error string.

| Op | Request | Reply |
|---|---|---|
| 1 ADD | bytes data | u64 address |
| 2 REQUEST | u64 address | bytes data |
| 3 CHANGE | u64 address, bytes data (the same length as the object) | nothing |
| 4 DELETE | u64 address | nothing |
| 5 FIRST | nothing | u64 address of the first object (0: none) |
| 6 NEXT | u64 address | u64 address of the next object (0: none) |
| 7 VECTOR_SEARCH | version, k, ef, dim, filters, query floats | nearest neighbors with distances and tags |
| 8 VECTOR_INFO | nothing | the vector index's dimension, type, metric and counts |

A database opened `write` changes through its journal, one transaction per request. A database that is a replicated copy is read-only, and every read holds the copy's apply lock so it never sees half of a commit.

**Legacy clients.** A DataReel 6.x style client that sends an `A` block (add) gets a `W` result block with the new object's address in the result word; an `R` block with an 8-byte address gets an `S` block with the object; `L` ends the session. These work only on a server that names a legacy database, and over TLS with a client certificate like every other client.

## 7. Object replication (gxObjRepl)

`gxObjRepl` keeps a copy of a primary's object databases on a second node. Only objects travel, never file pages: each node keeps its own file addresses, free space and indexes, so the two nodes can be different platforms (Linux and Windows, in either direction).

### 7.1 The stream

The primary connects to the copy over gxStream with mutual TLS: the copy listens with `gxStreamServer` and accepts only a primary whose certificate chains to the configured CA and has the configured common name. Each frame is one `S` block; the copy answers each with one `S` block (the hello reply, or an 8-byte acknowledgment). All numbers are little endian; strings are a 2-byte length and the bytes.

```
 primary                                   copy
    | -- TLS handshake (certificates both ways) -->|
    | -- hello: magic, epoch, link map, name,      |
    |    role, last seq, running digest ---------->|  checks roles, epoch, history
    |<-- reply: ok, applied seq, epoch, in full    |
    |    send, name, role, applied digest, why ----|
    | -- F_EPOCH epoch --------------------------->|  saves the epoch (fencing)
    |<-- ack: applied seq -------------------------|
    | -- F_COMMIT seq, epoch, ops ---------------->|  applies in one transaction
    |<-- ack: seq ---------------------------------|  after it is durable
    |        ... one commit at a time ...          |
```

- **The hello** carries the magic number `0x3550524F` ("ORP5"), the primary's epoch, its link map (which 8-byte fields of which objects are links to other objects), its name and role, its last sequence number and its running digest. The copy refuses a primary with an older epoch, a peer that is not a primary, an unexpected name, a primary that is behind the copy, or a link map that differs from the copy's own (when the copy has one configured). A refusal is a reply with ok = 0 and the reason, then the copy closes the connection and writes an alarm.
- **History check.** The primary checks the copy's reply: the copy must be a copy, at a sequence the primary has committed, with the same running digest the primary had at that sequence. A copy whose history differs, or that is past the primary, is refused with an alarm.
- **F_EPOCH** comes next. The copy saves the primary's epoch when it applies it, so from then on it refuses any older primary. The copy refuses data frames that come before F_EPOCH.
- **F_COMMIT** carries one commit: its sequence number, its epoch and its operations (add, update, remove, purge), each naming a database and a 64-bit object id; adds and updates carry the object's bytes with its links as ids. The copy applies a commit only if it is the next one (a resend of one it has is acknowledged again; a gap is refused), in one journal transaction, and acknowledges it once that transaction is durable.

### 7.2 The full send

When the copy is new, was told to rejoin, or is further behind than the primary's outbox reaches, the primary sends everything instead: F_SNAP_BEGIN (the base sequence), F_SNAP_BATCH frames of objects and tombstones, F_SNAP_DLOG frames with the primary's digest log, and F_SNAP_END with the base's digest, the number of records sent and a hash chained over them. The copy ends the full send only if the count and the hash match what it received; otherwise it waits for the primary to send it again. The copy's old data is set aside, not deleted, until the full send is whole. Commits made while the full send was under way follow from the outbox.

### 7.3 Failover

- **Takeover:** a copy becomes the primary at the next epoch. Object ids carry the epoch in their high bits, so the new primary never reuses an id the old one handed out.
- **Fencing:** every copy that has heard from the new primary refuses the old one (its epoch is older).
- **Rejoin:** the old primary becomes a copy waiting for a full send, and records how many of its own commits never reached the new primary.

Replication is asynchronous: a commit is durable on the primary when `Commit()` returns, and on the copy once it is acknowledged. If the primary is lost, the commits the copy did not acknowledge are lost with it, and the status shows how many.

### 7.4 The port

Replication is a gxStream object stream, so it uses the gxStream port. Since 7.4.1 the copy listens on port 2073 and the primary connects to port 2073 unless the configuration names another port (`listen [ADDR:]PORT`, `copy HOST:PORT`). In 7.4 the port had no default. A host that runs a copy and another gxStream service on the same address gives one of them another port.

## 8. Compatibility

- **Byte order and layout:** the block header is canonical little endian on every platform, with the same 32 bytes a 64-bit Linux build has always sent. DataReel 6.4 and 7.4 interoperate: the 6.4 legacy release sends the same header, and `utils/objrepl/xver` tests 6.4 and 7.4 peers against each other in both directions, as server and as client.
- **Old peers are refused, not misread:** a 7.4 receiver detects a 4-byte-word header and closes the connection with a clear error (`PeerOldHeader()`), and replication writes an alarm.
- **Versioning:** the multiplex HELLO carries a version (1), replication's hello a magic number for the replication state format, and vector requests their own version (1). A peer that does not match is refused.

## 9. Security

The design choices that matter for a reviewer of port 2073 (the [security white paper](docs/security_white_paper.md) has the details):

- TLS 1.2 or 1.3 on every connection, and no cleartext fallback: the block functions refuse to run on a connection that is not secure.
- Mutual TLS by default for gxStream servers: a client CA (not the system store) and an allow list of fingerprints or common names. Opting out needs an explicit call.
- Every length from the peer is checked before it is used: block lengths against the maximum block size, multiplex fields against the frame, replication fields against the frame and against object and commit size limits (32 MB per object, 60 MB per commit).
- Bounded resources: handshake threads and their timeout, a session limit, per-channel credit, a limit on concurrent vector searches.
- Replication refuses older epochs, gaps, reordering, changed histories and incomplete full sends, and writes an alarm for each.

Limits: a trusted certificate is trusted with everything its service allows (a `write` client can change any object of its database; a replication peer the copy accepts can raise the copy's epoch or replace its data). The replication digests are FNV-1a hashes that catch mistakes, not forgeries; the mutual TLS check is what keeps forged streams out. Anyone who can reach the port can use up handshake slots for their timeout, so firewall port 2073 to the hosts that need it.
