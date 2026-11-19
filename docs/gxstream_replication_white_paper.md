# gxStream and Replication on Port 2073, DataReel 6.4 (White Paper)

**Release:** DataReel 6.4 legacy release (6.4.1 and the 6.4.2 branch), 11/18/2026. **Audience:** network and security reviewers, firewall administrators, and developers who maintain DataReel 6.x gxStream and gxDatagram programs or replicate 6.x programs' records. This paper was written from the 6.4 source: `include/gxstypes.h`, `include/gxheader.h`, `include/gxstream.h`, `src/gxstream.cpp`, `include/gxdatagm.h`, `src/gxdatagm.cpp`, `include/gxsocket.h`, `src/gxsocket.cpp` and `dr7features/`. When this paper and the code disagree, the code wins.

The exact wire format is in the [protocol specification](rfc/draft-gaer-datareel-gxstream-00.html) (an Internet-Draft for the Independent Submission stream), which covers DataReel 7.4 and the DataReel 6.x legacy transports. The security design of 6.4 is in the [DataReel 6.4 security white paper](security_white_paper.html). DataReel 7.4 has its own version of this paper (https://datareel.com/clib7/gxstream_replication.php), with the database object server, the multiplex and replication in full.

## 1. Port 2073

TCP and UDP port 2073 are registered with IANA for DataReel under the service name `gxs-data-port`, "DataReel Database Socket". The registration was made in the 1990s, when a protocol description was not required with it; these papers and the specification are that description. The services file entries are in `include/gxstypes.h`:

```
gxs-data-port   2073/tcp   DataReel Database Socket
gxs-data-port   2073/udp   DataReel Database Socket
```

The port is `gxsDATA_PORT`, `gxSOCKET_DBS_PORT` and `gxSOCKET_DEFAULT_PORT` (all 2073), and every 6.x `gxSocket` starts with port 2073 until the program sets another.

| Use in DataReel 6.4 | Transport |
|---|---|
| `gxStream` database block clients and servers | TCP, with TLS and client certificates (or plain TCP when a server opts out) |
| `gxDatagram` database block clients and servers | UDP, plain text |
| `gxv7ObjRepl` replication (dr7features, the 7.4 code) | TCP, mutual TLS; port 2073 by default with a DataReel 7.4.1 or later tree |
| `gxv7ObjectClient` and the `vector_server` tool (dr7features, the 7.4 object server) | TCP, mutual TLS |

The UDP registration is for `gxDatagram`. DataReel 7 has no UDP transport.

## 2. The gxStream block

A block is a 32-byte header followed by `block_length` bytes of body. Since 6.4 every platform sends the header in one canonical form, four 8-byte little endian words, the same bytes DataReel 7.4 sends:

| Word | Contents |
|---|---|
| check word | `0xFEFE`; anything else is a synchronization error |
| length | the number of body bytes that follow |
| status | byte 0: the data status (`V`, a block from a remote device); byte 1: the control code; the rest 0 |
| result | a 64-bit result carried by a header-only acknowledgment (a file address); otherwise 0 |

A receiver refuses a header whose reserved bytes are not zero: that is a peer before 6.4 on 64-bit Windows or a 32-bit build, which sent 4-byte words. `PeerOldHeader()` reports it; upgrade both ends together.

Control codes: `A` add, `C` change, `D` delete, `R` request, `S` send data, `W` acknowledge (with a result), `L` close the connection, `K` kill the server, `T` restart the server. A server program must act on `K` and `T` only for clients it trusts to administer it.

The exchanges are the ones `gxStream` has had since 1999: `RequestBlock()` sends an `R` block and reads the reply block; `AddBlock()`, `ChangeBlock()` and `DeleteBlock()` send `A`, `C` (then the new contents in an `S` block) and `D` blocks; `WriteResultBlock64()` and `ReadResultBlock64()` carry a 64-bit result (6.3.1); `CloseConnection()` sends `L`. A receiver checks every length against `SetMaxBlockSize()` (64 MB by default) and against the caller's buffer before it reads.

## 3. gxStream over TCP

- **Server:** `StreamServer()` listens and `Accept()` or `AcceptClient()` takes a client. Since 6.3.1 they refuse to run unless the server authenticates its clients: `SetServerCert()` (TLS), `SetClientCAFile()` (the only CAs trusted for clients) and at least one `AllowClientFingerprint()` (SHA-256) or `AllowClientCN()` entry. A client whose certificate fails is closed. The accepted client's identity is `PeerFingerprint()` and `PeerCN()`.
- **Client:** `StreamClient()` connects. With `UseTLS(1)` or `SetClientCert()` it runs TLS and checks the server's certificate and host name (`SetServerCAFile()`, `SetServerName()`) unless `SetVerifyServer(0)`. In TLS mode the block functions work only on the TLS connection, so a failed handshake never falls back to cleartext.
- **Legacy plain TCP:** a server that calls `AllowAnonymousClients(true)` without a server certificate exchanges blocks over plain TCP, as 6.x servers did before 6.3.1, and a client without TLS settings connects that way. This mode has no confidentiality, integrity or authentication. The specification lists it as a legacy transport that new deployments must not use, and DataReel 7 does not have it.

## 4. gxDatagram over UDP

`gxDatagram` (`DatagramServer()`, `DatagramClient()`) sends the same blocks over UDP: one datagram with the 32-byte header, then one datagram with the body (at most 65,507 bytes). The control codes and exchanges are those of `gxStream`. There is no retransmission, ordering, encryption or authentication: a lost or reordered datagram leaves the two sides out of step, and any host that can reach the port can send blocks. Receivers check the header form and the length against `SetMaxBlockSize()` before they read the body. Use it only on a trusted network segment.

## 5. Replication and the object server (dr7features)

DataReel 6.4 has no replication or object server of its own. The optional dr7features build (`DATAREEL7_FEATURES_DIR`, a DataReel 7.4 or later tree) links the DataReel 7.4 code, so the protocol is exactly DataReel 7.4's:

- **`gxv7ObjRepl`** is DataReel 7.4's `gxObjRepl`. The primary connects to the copy with `gxStream` over mutual TLS and sends objects (never file pages): a hello with its epoch, name, role, last commit and a running digest; the copy's reply; an epoch frame; then one commit at a time, each acknowledged once it is durable on the copy, or a full send when the copy is too far behind. The copy refuses older epochs, gaps, reordering and changed histories with an alarm. A 6.4 program and a 7.4 program (or the 7.4 `objrepl` tool) replicate to each other on Linux and Windows (`docs/replication_6x.html`, `examples_database/replication_6x`). Replication is a gxStream object stream, so it uses port 2073: with a DataReel 7.4.1 or later tree the copy listens on 2073 and the primary connects to 2073 unless the configuration names another port (with 7.4 the port must be set).
- **`gxv7ObjectClient`** opens a channel on a DataReel 7.4 database object server (the multiplex on port 2073) and sends read-only vector searches (operations 7 and 8 of the object service). The `vector_server` tool (`dr7features/inner`) is that server, serving one database and its vectors.

The DataReel 7.4 version of this paper and the specification describe the replication frames, the multiplex and the object service in full.

## 6. Compatibility

- 6.4 and 7.4 send the same block header, and their `gxStream` peers interoperate over TLS, as server and as client (the DataReel 7.4 `utils/objrepl/xver` tests).
- 6.4 does not talk to peers older than 6.4 that were built on Windows or for 32 bits (the old header is refused). 64-bit Linux peers of every release sent the canonical bytes.
- A 6.x plain TCP or UDP peer cannot talk to DataReel 7, which requires TLS.

## 7. Security summary

- Database servers authenticate clients with TLS client certificates and an allow list unless the program opts out explicitly.
- TLS 1.2 or 1.3; TLS clients check the server certificate and host name by default.
- Every length from the peer is checked before memory is allocated or data is read.
- The legacy plain TCP mode and `gxDatagram` have no security of their own: use them only on trusted networks, firewall port 2073 (TCP and UDP) to the hosts that need it, and plan the move to DataReel 7.4.
