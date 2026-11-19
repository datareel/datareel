# DataReel 6.4 Security Design (White Paper)

**Release:** DataReel 6.4 legacy release (6.4.1 and the 6.4.2 branch), 11/18/2026. **Audience:** security reviewers, system administrators and developers who maintain programs built on DataReel 6.x. DataReel 6.4 is the legacy release: it gets bug fixes and security updates only. New programs should use DataReel 7.4, whose security design is in its own white paper (https://datareel.com/clib7/security_design.php). This paper was written from the 6.4 source; each section names the files it describes. When this paper and the code disagree, the code wins: report it (section 11).

The companion papers are the [gxStream and replication white paper](gxstream_replication_white_paper.html), which explains the database protocol on the IANA-registered port 2073, and its [protocol specification](rfc/draft-gaer-datareel-gxstream-00.html) (an Internet-Draft for the Independent Submission stream).

## 1. What DataReel 6.4 protects, and from whom

DataReel 6.x is a C++ library: an embedded object database (`gxDatabase`, `gxBtree`, `POD`, `InfoHog`, the page cache and the `gxdJournal` write-ahead journal since 6.3, and the relational classes), sockets and TLS (`gxSocket`, `gxSSL`, HTTP, SMTP, POP3, FTP, IMAP, telnet and the `gxStream` and `gxDatagram` database block protocols), threads, strings, containers, configuration and log files. Its security goals are:

- **Authenticated database servers:** a `gxStream` server refuses to run unless it authenticates its clients with TLS client certificates and an allow list, or the program explicitly opts out.
- **TLS where the program asks for it:** `gxStream` clients and servers, HTTPS, IMAP and the TLS classes use TLS 1.2 or 1.3. A TLS `gxStream` client checks the server's certificate and host name by default.
- **Robust handling of hostile input:** lengths and counts read from files and from the network are checked before use (the 6.3 and 6.4 security fixes).
- **Crash-safe database files** with the journal, and detection of accidental damage with block check words and CRC-32 checksums.

DataReel 6.x does **not** make every connection secure. Unlike DataReel 7, it keeps plain text network classes for compatibility: SMTP, POP3, FTP, HTTP and telnet clients, `gxDatagram` over UDP, and `gxStream` over plain TCP when a server opts out of client authentication. It does not encrypt database files at rest, and it does not protect against an attacker who controls the computer a program runs on or who holds a certificate and key a server trusts.

## 2. TLS

Source: `include/gxssl.h`, `src/gxssl.cpp`, `src/gxsschan.cpp`, `include/gxstream.h`, `src/gxstream.cpp`.

- **Protocol versions:** TLS 1.2 is the lowest version allowed. SSL 2, SSL 3, TLS 1.0, TLS 1.1 and DTLS 1.0 are refused. A program may require TLS 1.3 only.
- **TLS libraries:** Linux builds use the operating system's OpenSSL. Windows builds use Schannel by default, or OpenSSL 3.5 LTS (`README_windows_openssl.txt`). DataReel does not choose cipher suites or key exchange groups; the TLS library and the system's crypto policy do. Since the 10/14/2026 code review (6.0) the default Diffie-Hellman group is the 2048-bit RFC 7919 ffdhe2048 group.
- **gxStream clients:** `UseTLS(1)` or `SetClientCert()` turns TLS on. The client then checks the server's certificate chain (the system store, or `SetServerCAFile()`) and host name (`SetServerName()`, by default the host it connects to) unless the program calls `SetVerifyServer(0)`. A failed handshake or check closes the socket, so in TLS mode nothing is sent in cleartext. TLS handshakes time out after 60 seconds by default.
- **Other TLS clients:** the `gxSSL` object behind the HTTPS client checks the server's certificate only after `RequireVerifyCert()`; programs that use HTTPS must call it. The IMAP client uses TLS from the start or STARTTLS, and refuses to log in over plain text unless the program calls `AllowPlainLogin(1)`; data received before a STARTTLS handshake is discarded.
- **Keys:** `gxSSL::MakeRSAPrivateKey()` writes the private key file readable by its owner only.

## 3. Database servers on port 2073

Source: `include/gxstream.h`, `src/gxstream.cpp`, `include/gxheader.h`.

- **Client authentication is required by default (6.3.1).** `StreamServer()`, `Accept()` and `AcceptClient()` refuse to run unless the server either uses TLS with client certificates (`SetServerCert()`, `SetClientCAFile()` and at least one `AllowClientFingerprint()` or `AllowClientCN()` entry) or the program calls `AllowAnonymousClients(true)`. The client CAs are the only CAs trusted for clients (not the system store), and a client must be named in the allow list by its certificate's SHA-256 fingerprint or its subject common name. A client that fails is closed.
- **The opt-out is explicit and visible in the code.** With `AllowAnonymousClients(true)` and no server certificate, the server takes clients over plain TCP, as all 6.x servers did before 6.3.1. Such a server has no confidentiality and no authentication. Use it only on a trusted network, and move it to TLS with client certificates.
- **Lengths are checked.** A block length from the peer is checked against `SetMaxBlockSize()` (64 MB by default) before memory is allocated or data is read, and `ReadBlock()` overloads take the size of the caller's buffer. Negative lengths and lengths over `INT_MAX` are refused on send.
- **One header layout.** Since 6.4 every platform sends the canonical 32-byte block header that DataReel 7.4 sends. A peer that sends the older 4-byte-word header (a 64-bit Windows or 32-bit build before 6.4) is detected and refused (`PeerOldHeader()`), not misread. Upgrade both ends together.
- **gxDatagram (UDP)** sends the same blocks over UDP, one datagram for the header and one for the body. UDP has no peer authentication and no encryption: any host that can reach the port can send blocks. Lengths are checked against `SetMaxBlockSize()` before memory is allocated. Use it only on a trusted network segment.

## 4. Replication and the 7.x object server

Source: `dr7features/include/dr7features.h`, `dr7features/src/dr7features.cpp`, `docs/replication_6x.html`.

DataReel 6.4 has no replication or object server code of its own. The optional dr7features build (`DATAREEL7_FEATURES_DIR`, a DataReel 7.4 or later source tree) links the DataReel 7.4 code:

- `gxv7ObjRepl` is DataReel 7.4's `gxObjRepl`: objects (never file pages) from a primary to a copy over `gxStream` with mutual TLS, epoch fencing, history digests and alarms, as described in the DataReel 7.4 security white paper. A 6.4 program and a 7.4 program replicate to each other.
- `gxv7ObjectClient` connects to a DataReel 7.4 database object server with mutual TLS for read-only vector searches.

Their security is DataReel 7.4's, with its limits: a replication peer the copy trusts can raise the copy's epoch or replace its data, and the replication digests detect mistakes, not forgeries.

## 5. Hostile input

Source: `RELEASE_NOTES_6.3.txt`, `RELEASE_NOTES_6.4.txt`, `RELEASE_NOTES_6.4.1.txt`, `examples_database/fuzz`, `examples_general/fuzz`, `examples_socket/stream_fuzz`.

- **6.3** took the fixes of the DataReel 7.3 security scan (damaged block lengths that looped or read out of place, checksum reads bounded by the file size, journal recovery bounds, file handles on failed opens, owner-only key and log files) and fixed what a sweep of the 6.x-only code found (Base64, string, configuration and other support code). Each fix has a test with a damaged or crafted input (`examples_database/damaged`, `examples_general/security_sweep`, `examples_general/support_security`, `examples_socket/comm_security`).
- **6.3.1** checks block lengths in `gxStream` and `gxDatagram` before allocation and adds the TLS client authentication of section 3.
- **6.4** sends the canonical block header on every platform and makes journal salts unique.
- **6.4.1** fixes socket and thread findings (`examples_socket/socket_security`, `examples_threads/thread_security`).
- libFuzzer targets cover the database files, the support code and the stream protocol, and the 6.4 test suite (`examples_database/run_db_tests.sh`, 39 checks including the damaged-file and security sweeps, the networking and TLS checks and 50 kill -9 crash rounds) passes under AddressSanitizer and UndefinedBehaviorSanitizer.

## 6. Database files

Source: `include/gxdbase.h`, `include/gxdjrnl.h`.

- Block headers carry a check word, and objects can carry CRC-32 checksums. These detect accidental damage; they are not cryptographic.
- The journal (6.3 and later) makes a group of writes to a data file and its indexes survive a crash or power loss as a whole or not at all. Without it, a crash can leave part of a batch.
- File formats are unchanged across 6.x and shared with DataReel 7 for the same build settings. Database files made on Linux and on Windows are laid out differently and should be used on the system that made them.
- Files are created with the process's umask; the application decides who may read them.

## 7. Use DataReel 6.4 securely

- Run every `gxStream` server with TLS, client certificates and an allow list; do not call `AllowAnonymousClients(true)` on a network you do not trust.
- Turn TLS on in every `gxStream` client (`UseTLS(1)` or `SetClientCert()`), and keep server verification on.
- Call `RequireVerifyCert()` before HTTPS connections. Do not send passwords over the plain text SMTP, POP3, FTP, HTTP or telnet classes; use DataReel 7.4 for TLS mail and file transfer.
- Use `gxDatagram` only on a trusted network segment, or replace it with `gxStream` over TLS.
- Firewall port 2073 (TCP and UDP) to the hosts that need it.
- Encrypt sensitive records yourself, or keep the files on an encrypted file system, if they must be protected at rest.
- Plan the move to DataReel 7.4, which has no plain text database transport.

## 8. Limits, stated plainly

- Plain text transports remain in 6.x for compatibility (section 1). Their traffic can be read and changed by anyone on the path.
- `gxDatagram` cannot authenticate peers, and lost or reordered datagrams leave the two sides out of step.
- No encryption at rest; CRC-32 checksums detect damage, not deliberate changes.
- A certificate a server trusts is trusted with everything that server's program allows.
- TLS choices (cipher suites, groups, revocation) belong to the TLS library and its policy.
- While a program runs, its data and keys are in its memory.

## 9. FIPS 140-3

DataReel 6.4 is not a FIPS-validated cryptographic module and has no FIPS-only mode of its own. Its TLS uses the TLS library it is built with: on a RHEL system in FIPS mode, the system OpenSSL's FIPS provider; on Windows, Schannel under the Windows FIPS policy, or OpenSSL 3.5 built with its FIPS provider (`README_windows_openssl.txt`). DataReel 6.4 should not be described as FIPS compliant or FIPS validated. The plain text transports use no cryptography at all.

## 10. Quantum computing

DataReel leaves key exchange to the TLS library. OpenSSL 3.5 offers the hybrid X25519MLKEM768 group (ML-KEM, NIST FIPS 203) in TLS 1.3 by default, so peers built with it can agree on keys that a future quantum computer cannot recover from recorded traffic, where the system crypto policy allows it. Certificates are RSA or ECDSA. Plain text transports protect nothing, now or later. For traffic that must stay confidential for years, use TLS 1.3 with OpenSSL 3.5, or move to DataReel 7.4.

## 11. Reporting a security issue

Report security issues privately through the contact page (https://datareel.com/contact.php), not in public issue trackers. DataReel 6.4.x gets security fixes; projects maintained by DataReel Software Development report DataReel issues through DataReel's internal handoff process.
