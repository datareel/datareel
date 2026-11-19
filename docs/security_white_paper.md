# DataReel 7.4 Security Design (White Paper)

**Release:** DataReel 7.4 (the 7.4.1 branch), 11/18/2026. **Audience:** security reviewers, system administrators and developers deciding whether to trust DataReel with their data and their network traffic. DataReel is free software and its source is published, so everything in this paper can be checked against the code; the design does not depend on keeping any of it secret. Each section names the source files it was written from. When this paper and the code disagree, the code wins, and the paper is wrong: report it (section 12).

The companion papers are the [gxStream and replication white paper](docs/gxstream_replication_white_paper.md), which explains the database protocol on the IANA-registered port 2073, and its [protocol specification](docs/rfc/draft-gaer-datareel-gxstream-00.html) (`docs/rfc/draft-gaer-datareel-gxstream-00.txt`, an Internet-Draft for the Independent Submission stream).

## 1. What DataReel protects, and from whom

DataReel is a C++ library, not a program: it has an embedded object database (`gxDatabase`, `gxBtree`, `POD`, `InfoHog`, `gxdJournal`, `gxVectorIndex`), TLS network classes (`gxsTLSConnection`, `gxsTLSServer`) and protocols built on them (HTTPS, SMTP, POP3, IMAP, FTPS and `gxStream`, the database block protocol). Its security goals are:

- **Confidentiality and integrity on the network:** every DataReel protocol that carries data uses TLS 1.2 or 1.3. A server's certificate and host name are checked by default. There is no plain text HTTP, SMTP, POP3, IMAP, FTP or database traffic in DataReel 7.
- **Authenticated database clients:** a database server on port 2073 requires a client certificate by default (mutual TLS), and accepts only the clients named in an allow list.
- **Robust handling of hostile input:** every length or count read from the network or from a file is checked before it is used, so a damaged file or a hostile peer cannot make the library read or write outside its buffers or allocate unbounded memory.
- **Crash-safe database files:** with a journal, a group of writes survives a crash, a kill or a power loss as a whole or not at all.
- **Detection of accidental damage:** database blocks carry check words and optional CRC-32 checksums, and replication compares running digests of the history on both nodes.

DataReel does **not** encrypt database files at rest. Data written with `gxDatabase` is stored as the application wrote it; an application that needs encryption at rest encrypts its records itself or keeps the files on an encrypted file system. (DataReel's own products that do encrypt at rest, the [Encrypted Password Database](https://datareel.com/encrypted_password_database/security_design.php) and [fcrypt](https://datareel.com/fcrypt/security_design.php), have their own security papers.) DataReel does not protect against an attacker who controls the computer a DataReel program runs on, or who holds a valid certificate and key that a server trusts.

## 2. TLS on every network protocol

Source: `include/gxstls.h`, `src/gxstls.cpp`, `src/gxssl.cpp`, `src/gxsschan.cpp`.

- **One TLS class for everything.** `gxsTLSConnection` is a TCP connection that is encrypted from the start (implicit TLS) or upgraded after a STARTTLS command. Every protocol class in DataReel 7 is written on it. The only plain text classes left are telnet and ping, kept for diagnostics.
- **Protocol versions:** TLS 1.2 is the lowest version allowed (`SSL_CTX_set_min_proto_version(TLS1_2_VERSION)`). SSL 2, SSL 3, TLS 1.0 and TLS 1.1 are refused by both the OpenSSL and the Schannel builds. A program may require TLS 1.3 only.
- **TLS libraries:** Linux builds use the system OpenSSL (1.1.1 or later; OpenSSL 3 on RHEL 9 and 10). Windows builds use Schannel, the Windows TLS library, by default, or OpenSSL 3.5 LTS when built with `DATAREEL_SSL=OPENSSL`. DataReel does not choose cipher suites or key exchange groups: the TLS library and the system's crypto policy do, so a site's policy (for example RHEL `update-crypto-policies`) applies to DataReel programs too.
- **Server checks on the client side:** a client checks the server's certificate chain against the system certificate store, or against a private CA file (`SetCAFile()`), and checks the host name with `X509_check_host()` (wildcards only as a whole left-most label) or the IP address with `X509_check_ip()`. A failed check closes the connection before any data is sent. `SetVerifyPeer(false)` turns the check off and exists for tests only.
- **Failures close the connection.** A failed handshake, a failed STARTTLS or a failed certificate check closes the TCP connection, and anything received before a STARTTLS handshake is discarded so it cannot be mistaken for protected data.
- **Bounded waits:** every handshake, send and receive has a timeout, applied as a socket timeout (60 seconds for a `gxsTLSConnection` and 2 hours for a `gxStream` unless the program sets another; the database object server uses 10 seconds for a handshake and 120 seconds for a block), so a peer that connects and says nothing cannot hold a thread forever. SIGPIPE is ignored so a peer that closes early cannot kill the program, and sockets are opened close-on-exec so child processes do not inherit them.

## 3. Mutual TLS for database servers

Source: `include/gxstls.h`, `include/gxstream.h`, `src/gxstls.cpp`.

- `gxsTLSServer` can require a client certificate: `SetClientCAFile()` names the CAs that issue client certificates, and those CAs are the only ones trusted for clients (not the system store), so a certificate bought from a public CA does not pass.
- An allow list narrows that further: a client is accepted only if its certificate's SHA-256 fingerprint (`AllowClientFingerprint()`) or its subject common name (`AllowClientCN()`, exact and case sensitive) is listed. A client that fails is closed before it can send a block.
- **Secure by default for gxStream:** `gxStreamServer`, the server class for the database protocol, requires client certificates by default. `Listen()` fails unless a client CA file and at least one allow list entry are set, or the program explicitly opts out with `AllowAnonymousClients(true)`. The opt-out does not turn off a CA file or allow list that is set.
- The accepted connection reports the client's identity (`PeerFingerprint()`, `PeerCN()`). Both come from the client's certificate, so they are escaped before they go to a log.
- Other TLS servers (HTTPS, for example) are not affected: they ask for no client certificate unless a client CA file is set.

## 4. The database protocol on port 2073

Source: `include/gxstream.h`, `src/gxstream.cpp`, `include/gxheader.h`, `include/gxdbserver.h`, `src/gxdbserver.cpp`, `include/gxdbmux.h`, `src/gxdbmux.cpp`, `include/gxdbobject.h`, `src/gxdbobject.cpp`. The protocol itself is described in the [gxStream and replication white paper](docs/gxstream_replication_white_paper.md).

- **Never in cleartext.** Every `gxStream` block function refuses to run unless the connection's TLS handshake has finished (`gxSOCKET_SSL_ERROR`), so a failed or skipped handshake can never send or accept a database block in cleartext.
- **Lengths are checked before memory is allocated.** A block header's length comes from the peer, so it is checked against a maximum (64 MB by default, 17 MB on the database object server) before any buffer is allocated. Lengths over `INT_MAX` are refused on send, negative counts are refused, and a header whose check word is wrong is refused as out of step.
- **One header layout on every platform.** Since 7.4 every platform sends the block header in one canonical 32-byte little endian form. A peer that sends the older 4-byte-word form (before 7.4 beta 5 on 64-bit Windows, or a 32-bit build) is detected and refused, never misread.
- **The database object server** (`gxDatabaseServer`) owns one listener and makes the same checks for every service it carries: mutual TLS with the allow list, each TLS handshake in its own thread with a short timeout (10 seconds) and at most 16 handshakes at once, so silent connections from a scanner cannot keep real clients out, and at most `max_clients` (4 by default) authenticated sessions. Only clients that passed the certificate checks count against that limit. A session's first block chooses its service; a block no service claims closes the session.
- **Multiplexed channels** (`gxdMuxSession`) carry many databases over one connection. Each channel has its own flow control: the receiver grants credit for the room in that channel's bounded queue (16 MB by default), a frame larger than 1 MB is never sent, and a peer that sends past its credit has that channel closed with an error. Every string and byte field in a frame is length checked against the frame.
- **Access modes:** the object service opens a channel only for a database its configuration names, in mode `read` or `write`; a `write` open of a read-only database, or of a replicated copy, is refused. Writes to a writable database go through a `gxdJournal`, one transaction per request. Reads of a replicated copy hold the copy's apply journal read lock, so a reader never sees a half-applied commit.
- **Bounded vector searches:** a vector search request is checked field by field (version, k at most 1,000, ef at most 4,096, the index's dimension, known flag bits, finite floats, exact body length), and at most four run at once; one past that waits up to 5 seconds and is then answered "busy".

## 5. Replication

Source: `include/gxobjrepl.h`, `src/gxobjrepl.cpp`, `docs/replication_admin_guide.md`.

- `gxObjRepl` sends objects, never file pages, from a primary to a copy over `gxStream` with mutual TLS. The copy listens with `gxStreamServer` and requires the primary's certificate to chain to the configured CA and to carry the configured common name (`allow-cn` is required). The primary checks the copy's certificate and host name as any TLS client does.
- When a node opens, it checks its own certificate, key and CA: readable, within their dates, signed by the CA, key and certificate matching, and (on Linux and other POSIX systems) the private key not readable by group or others.
- **Fencing:** every node has an epoch. A copy refuses a primary with an older epoch, so after a takeover the old primary cannot write to any copy that has heard from the new one. Object ids carry the epoch in their high bits, so a new primary never reissues an id the old one handed out.
- **History checks:** at every connect both sides say their name, role, epoch, sequence number and a running digest of every commit they have; any change of roles or of history refuses the stream and writes an alarm file. A copy refuses frames that arrive out of order, after a gap, before the primary accepted it, or with an older epoch, and a full send that arrives incomplete or changed is not ended.
- **Atomic applies:** a node's databases, indexes, outbox and state are under one journal, so a commit is applied on the copy as a whole or not at all, and is acknowledged only after it is durable.

The running digests are 64-bit FNV-1a hashes: they detect a mistake (two nodes with different histories, a lost or repeated frame), not a forgery. The protection against a forged stream is the mutual TLS check: any peer whose certificate the copy accepts is trusted with the stream, including raising the epoch and sending a full replacement of the copy's data (section 10).

## 6. Hostile input from files and the network

Source: `src/gxdbase.cpp`, `src/gxbtree.cpp`, `src/gxvindex.cpp`, `src/gxdbmux.cpp`, `src/gxobjrepl.cpp`, `tests/damaged_files.cpp`, `tests/fuzz/`.

- The rule written into the code base (AGENTS.md section 4.4) is that every length and count read from a file or the network is untrusted and is checked against its buffer or a limit before use. Damaged block headers whose length is shorter than their own overhead are read as length 0 instead of underflowing; message readers in the multiplex and in replication mark a message bad as soon as a field would run past its end.
- `tests/damaged_files.cpp` opens deliberately damaged database, index and journal files and checks that each is refused cleanly.
- `tests/stream_security_test.cpp` checks that gxStream refuses cleartext, that a failed STARTTLS closes the connection, that huge and negative lengths are refused, that a damaged object length is safe, and that reads larger than `INT_MAX` and stalled connects are refused or time out.
- Five libFuzzer targets (`tests/fuzz/`: database files, gxStream, the text protocols, the support classes and vector requests) run under AddressSanitizer and UndefinedBehaviorSanitizer.

## 7. Database files

Source: `include/gxdbase.h`, `include/gxdjrnl.h`, `src/gxdjrnl.cpp`, `include/gxproclock.h`.

- **Block check words and CRCs:** every block in a database file starts with a header with a check word, and revision A and later files can store a CRC-32 checksum for each object (`WriteObjectChecksum()`, `ReadObjectChecksum()`). These detect accidental damage. They are not cryptographic: anyone who can write the file can change an object and its checksum together.
- **Crash safety:** `gxdJournal` is a write-ahead log over a data file and its indexes. `Commit()` appends the changed pages and a commit record to the journal and syncs it; the next `Open()` after a crash writes every committed transaction to the files and ignores one that was not committed. `gxdJournalReadLock` lets readers in other processes wait out a commit instead of reading half of it.
- **File formats are fixed.** DataReel 7 writes files byte-identical to 6.2 with the same build settings, and the security fixes of 7.x changed no file format, so a file's integrity checks mean the same thing in every release.
- **Permissions:** DataReel creates files with the process's umask; the application decides who may read its database files. Replication refuses a private key that others can read.

## 8. How DataReel is checked

- The test suite (`tests/run_tests.sh`) runs on every build of the release matrix: the 64-bit and 32-bit database engines, native types on and off, Linux and Windows.
- DataReel 7.3 and 7.4 each had a security scan before their release tags: AddressSanitizer and UndefinedBehaviorSanitizer on the test suite, ThreadSanitizer on the servers and replication suites, static analysis (clang-tidy and `gcc -fanalyzer`), the fuzz targets, and a scan of the source tree and its history for secrets. The 7.4 results are summarized in `RELEASE_NOTES_7.4.txt`.
- Replication is tested with kill -9 and power-loss simulations during committed batches, and with cross-version tests between DataReel 6.4 and 7.4 (`utils/objrepl/xver`).

## 9. Use DataReel securely

- Keep certificate verification on; for a private network use your own CA with `SetCAFile()` and `SetClientCAFile()`.
- Run database servers with client certificates and an allow list (the default for `gxStreamServer`). Do not call `AllowAnonymousClients(true)` on a server that is reachable by anyone you do not trust.
- Bind servers that only local programs use to the loopback address, and firewall port 2073 to the hosts that need it.
- Give each client and each replication node its own certificate, keep private keys mode 600, and watch certificate expiry (`gxObjRepl::Check()` warns 30 days ahead by default).
- Encrypt sensitive records yourself, or keep database files on an encrypted file system, if they must be protected at rest.
- Build every program that shares files or connections from one DataReel tree with the same build settings.

## 10. Limits, stated plainly

- **No encryption at rest** (section 1). CRC-32 checksums and replication digests detect damage and mistakes, not deliberate changes.
- **A trusted certificate is trusted with everything it can reach.** A database client allowed in `write` mode may change any object in that database. A replication peer whose certificate the copy accepts may raise the copy's epoch (fencing out the real primary) or replace the copy's data with a full send. Protect those keys like passwords.
- **Object addresses (fixed in 7.4.1).** The object service takes file addresses from clients. In 7.4 it checked only that a block header with a valid check word was in front of the address, so a client that could write could put a copy of a block header inside its own object and then change or delete "objects" at addresses inside it, damaging the objects after it, and a client that could read could read deleted blocks. Since 7.4.1 an address must be the start of a normal (not deleted) block that lies inside the file, and for a writable database one of the live objects the server keeps a list of. A read-only database (opened for each request, with no such list) can still be read at an address behind a copied header inside an object; that reads bytes of the same file only and changes nothing. Give `write` access only to trusted programs in any case.
- **Denial of service:** the database object server limits handshakes and sessions, but anyone who can reach the port can use up its handshake slots for up to 10 seconds each. A replication copy handles one connection at a time, so a connection that stalls its TLS handshake delays replication for up to 30 seconds. Firewall the port to the hosts that need it.
- **TLS choices belong to the TLS library.** Cipher suites, key exchange groups and certificate revocation checking are whatever the system's OpenSSL or Schannel and its policy allow; DataReel does not add revocation checks of its own.
- **Cleartext diagnostics:** the telnet and ping classes are plain text by design and must not carry secrets.
- While a program runs, its data and TLS keys are in its memory.

## 11. FIPS 140-3 and quantum computing

**FIPS 140-3.** DataReel itself is not a FIPS-validated cryptographic module, and it has no FIPS-only mode of its own; all of its cryptography is done by the TLS library it is built with. On a RHEL system in FIPS mode, the system OpenSSL uses its FIPS provider, so DataReel's TLS connections use only the algorithms that provider allows. The Windows OpenSSL builds described in `README_windows_openssl.txt` include the OpenSSL 3.5 FIPS provider. Schannel follows the Windows FIPS policy setting. DataReel should not be described as FIPS compliant or FIPS validated; sites that require FIPS-validated cryptography should check their configuration with their security office.

**Quantum computing.** A large quantum computer, if one is built, would break the RSA and elliptic curve cryptography that TLS certificates and classic key exchange use today. Where DataReel stands:

- **Key exchange:** DataReel leaves the key exchange groups to the TLS library. OpenSSL 3.5 offers the hybrid X25519MLKEM768 group (classic X25519 with ML-KEM-768, NIST FIPS 203) by default in TLS 1.3, so two peers built with it can already agree on keys that a future quantum computer cannot recover from recorded traffic, where the system crypto policy allows it. Older OpenSSL releases and other TLS libraries may not offer such groups yet.
- **Certificates:** DataReel uses whatever certificates the site issues. Today's certificates are RSA or ECDSA; they protect against an attacker who must break them during the handshake, not against recorded traffic, so they matter less than key exchange.
- **Database files and replication digests** use no public-key cryptography.
- **Plan:** require TLS 1.3 (`SetProtocol(gxSSL_TLSv1_3)`) between DataReel peers that should be protected against "harvest now, decrypt later", and build them with OpenSSL 3.5 LTS or a TLS library with hybrid ML-KEM key exchange.

## 12. Reporting a security issue

Report security issues privately through the [contact page](https://datareel.com/contact.php), not in public issue trackers. Projects maintained by DataReel Software Development report DataReel issues through DataReel's internal handoff process. Fixes go into the next release: 7.4.x for DataReel 7, and 6.4.x (bug fixes and security updates only) for the legacy DataReel 6 release.
