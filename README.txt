DataReel 7.4.1 Readme File
DataReel Copyright (c) 2001-2026 DataReel Software Development

https://datareel.com

CONTENTS
--------
Overview
What DataReel Is Good For
What Changed in DataReel 7
License
Supported Platforms and Compilers
Package Map
Building
Using DataReel in Your Project
File Formats
Example Programs and Tests
Documentation
Release History

OVERVIEW
--------
DataReel is an object database (OODB) and cross platform
communications toolkit for C++: applications keep their objects in
local database files and talk to other systems over TLS. It began in
1997, when C++ had no standard string, container, thread or file
system classes, as a general purpose library that supplied all of
them. The C++ standard library now does that job, so DataReel 7 keeps
the parts the standard library still does not have and removes the
rest:

- An embedded object database: block files with free space reuse,
  CRC checks and file locking, disk B-tree indexes and persistent
  objects, with tools to check and repair files.
- TLS networking: IPv4 and IPv6 sockets, TLS 1.2 and 1.3 with
  certificate checking, and HTTPS, SMTP, POP3, FTPS and database block
  clients and servers.
- Object replication between Windows and Linux nodes, in both
  directions, with takeover and rejoin (7.4).
- Stable, portable file formats: files written by DataReel releases
  going back to 2001 still open, byte for byte.

The library has no dependencies except the system TLS library
(OpenSSL on Linux, Schannel or OpenSSL on Windows).

DataReel 7.4.1, released 11/20/2026, is the current DataReel release
(7.4 with fixes: see "NEW IN 7.4.1" in RELEASE_NOTES_7.4.txt).
DataReel 7 is the main, fully supported release line. DataReel 6
(6.4.2) is the legacy release line: it is still supported with bug
fixes and security updates, but gets no new features. Online documentation:
https://datareel.com/clib7/ (Version 7) and https://datareel.com/clib/
(Version 6, legacy).

README.md is a short overview of DataReel and its two release tracks:
DataReel 7.x (main) and DataReel 6.4.2 (legacy).

The DataReel development package was produced by independent work and
contract work released to the public through non-exclusive license
agreements. The initial work began independently in 1997 and was
augmented from 1999 to 2004 by code produced under contract to support
various applications. Bug fixes, enhancements and additions are
welcome through the DataReel website:

https://datareel.com/downloads/datareel/
https://datareel.com/contact.php

WHAT DATAREEL IS GOOD FOR
-------------------------
Object database
 gxObjRepl         Object replication between Windows and Linux nodes,
                   both ways, with takeover and rejoin (7.4)
 gxDatabase        32/64-bit block file engine: allocate, write, read,
                   delete and reuse variable size blocks, per-block CRC,
                   file and record locking, large files
 gxBtree           Disk B-tree index with your own key class, optional
                   node cache (BtreeCache)
 POD               A data file and its index files opened together
 InfoHog           Persistent objects: a fixed key plus any number of
                   variable length members, indexed by key
 gxDatabaseCache   Write-back cache for database files
 db_debug/ix_debug Utilities that analyze and rebuild damaged files

TLS networking
 gxsTLSConnection  TCP connection with implicit TLS or STARTTLS,
                   certificate and host name checks, line and block I/O
 gxsTLSServer      TLS server; pairs well with datareel::ThreadPool
 gxsHTTPClient     HTTPS client with redirects and chunked bodies
 gxsHTTPMessage    HTTP/1.1 messages for clients and servers
 gxsSMTPClient     Mail over implicit TLS (465) or STARTTLS (587)
 gxsPOP3Client     Mail over implicit TLS (995) or STLS (110)
 gxsIMAPClient     Mailboxes over implicit TLS (993) or STARTTLS (143)
 gxsFTPClient      FTPS with encrypted data connections
 gxStream          Database blocks between client and server over TLS
 gxSocket          IPv4/IPv6 TCP and UDP socket under all of the above
 gxSerialComm      Serial ports (and a serial database block server)
 gxsPing, telnet   Plain text diagnostics (ICMP and telnet cannot be
                   encrypted; use telnet only on trusted networks)

Portable types and helpers
 gxINT16..gxUINT64, gxFLOAT64, CDate   Fixed layout types used in files
 gxConfig          name=value configuration files
 LogFile           Thread safe time stamped log file
 ThreadPool        Task pool on std::thread (the standard has none)
 strutil.h         std::string helpers (trim, case-insensitive find,
                   split, str_append, ...)
 systime.h         Time stamps for logs, SMTP and HTTP
 gxs_b64.h, gxcrc32.h   Base 64 and CRC-32

For general purpose work (strings, containers, threads, files, time)
use the C++ standard library; DataReel 7 itself does.

WHAT CHANGED IN DATAREEL 7
--------------------------
See RELEASE_NOTES_7.0.txt for the full list and for porting notes,
RELEASE_NOTES_7.1.txt, RELEASE_NOTES_7.2.txt, RELEASE_NOTES_7.3.txt,
RELEASE_NOTES_7.3.1.txt, RELEASE_NOTES_7.3.2.txt, RELEASE_NOTES_7.3.3.txt
and RELEASE_NOTES_7.4.txt for the changes in 7.1 to 7.4. In short:

- C++17 is required. Strings, lists, trees, threads, mutexes, files
  and time now come from the standard library. The UString/gxString,
  MemoryBuffer, gxList, gxBStree, DiskFileB, futils, SysTime, gxThread,
  gxMutex, gxCondition, gxSemaphore and thrPool classes, the HTML/XML/
  RSS parsers, the report writers, the terminal and device cache
  classes, gxIP32, gxMAC48, FString and gxDatagram were removed.
- All networking uses TLS 1.2 or later and checks certificates by
  default. Sockets support IPv6. http://, plain SMTP, plain POP3 and
  plain FTP are no longer supported.
- Everything is in the datareel namespace. "using namespace datareel;"
  is added for you so 6.x code that uses unqualified names compiles;
  define DATAREEL_NO_GLOBAL_NAMES to turn that off.
- CMake builds the library, examples and tests on Linux and Windows
  and installs a find_package(DataReel) package.
- Database, index and InfoHog files are unchanged.
- 7.1 removed the gxRDBMS relational tables; use an embedded SQL
  database for relational needs.
- 7.2 adds gxdJournal (atomic, crash and power loss safe transactions
  over a data file and its indexes), an optional page cache, faster
  file I/O and a faster B-tree cache. File formats are unchanged.
- 7.3 reads records through a memory map and makes key-order scans
  and random reads faster.
- 7.3.1 fixes the problems a full security sweep found (damaged
  files and journals, TLS, the database stream protocol, HTTP, mail,
  FTPS, telnet, RS-232, ping and the support code), and gxStream
  database servers use mutual TLS: gxStreamServer requires client
  certificates from a client CA plus an allow list of fingerprints or
  names, and will not start without them unless the program
  explicitly opts out (AllowAnonymousClients(true), stream_server
  --open). Other TLS servers are unchanged. 7.3.1 also adds database
  replication (gxrepl, utils/gxrepl) and the database object server.
- 7.3.2 fixes database replication problems found in production:
  channels resync after a timeout, the seal and the applied position
  are never read split, unit copies are staged and swapped in whole
  ("gxrepl promote"), and a copy's folder goes when its source folder
  goes. 7.4 has all of them.
- 7.3.3: a copy's seal names the last record that changed its folder
  (idle folders keep their derived indexes), "gxrepl torn" reads each
  folder under a shared apply lock, and a verify sees files a stream
  names during its pass. 7.4 has them too.
- 7.4 adds vector extensions to the object database: gxVectorIndex
  keeps vectors in a ".vix" file (an ordinary gxDatabase file) with
  exact search and an HNSW graph for fast approximate nearest-neighbor
  search, filters, Merge() of day files and Repair(), float16 and
  int8 vectors, vectors kept in records, sets of many index files
  searched as one (gxVectorSet), vector searches through the database
  object server, and vector indexes that a replicated copy rebuilds
  from its records (RELEASE_NOTES_7.4.txt). Existing file formats are
  unchanged.
- 7.4 also adds object replication between Windows and Linux
  (gxObjRepl, first in 7.4 beta 5): nodes on either platform replicate
  objects both ways, with the primary on either side, and takeover and
  rejoin work across them. One database file layout and one 32-byte
  stream header on every platform. gxrepl page-shipping replication is
  removed.
- 7.4 brings back the 6.x file utilities hexdump, ffind and txfilter,
  adds the objrepl, vix_rebuild and dr_digest tools, an IMAP client
  (gxsIMAPClient), CSV files (gxCSVReader, gxCSVWriter) and strip_ansi(),
  and adds the examples imap_fetch, https_check, udp_broadcast,
  udp_multicast, csv_merge, service_example, serial_read and dir_upload.

LICENSE
-------
The DataReel open source library is copyrighted by DataReel Software
Development, Copyright (c) 2001-2026, all rights reserved. DataReel
open source is available to non-profit, commercial, individual, and
academic developers. The open-source archive is distributed under the
GNU Lesser General Public License (LGPL), version 3 or any later
version, with provisions for use in commercial and non-commercial
applications under the terms of the LGPL. See the LICENSE file.

Under the terms of the LGPL you can use the open source code in
commercial applications as a static or dynamically linked library.
This allows commercial developers to compile the library and link to
it without making any changes to the DataReel source code.

This library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Lesser General Public License for more details.

SUPPORTED PLATFORMS AND COMPILERS
---------------------------------
- Red Hat Enterprise Linux 10 (the default build platform), 9 and 8,
  with the system GCC (8 or later) and the system OpenSSL (1.1.1 or
  later). RHEL 8's GCC 8 links std::filesystem from libstdc++fs; the
  build files add it.
- Windows 11 and Windows Server 2022 and 2025, 64-bit, with Visual
  Studio 2022 or 2026. TLS uses Windows Schannel by default, or
  OpenSSL 3.5.x LTS (see README_windows_openssl.txt).
- CMake 3.16 or later.

PACKAGE MAP
-----------
 README.md       Overview: what DataReel is, the 7.x and 6.4 release tracks
 CMakeLists.txt  Main build file (library, examples, utilities, tests)
 include/        DataReel headers
 src/            DataReel source files
 examples/       Example programs: database/, tls/, general/, diagnostics/
 utils/          db_debug and ix_debug database utilities
 tests/          run_tests.sh, mock mail/FTPS servers, damaged file test
 build/          Linux makefile (cd build; make; make install)
 env/            Settings for the Linux makefile (linux.env, gnu_env.sh)
 lib/            Library install folder used by build/makefile
 docs/           HTML documentation (start with docs/index.html)
 docs/ai/        Usage guides for AI coding assistants
 AGENTS.md       Rules for AI assistants that write code with DataReel
 llms.txt        Index of the documentation for AI tools
 llms-full.txt   The AI guides in one file

BUILDING
--------
Linux (RHEL 8, 9, 10):

 sudo dnf install gcc-c++ cmake openssl-devel
 cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release
 cmake --build cmake-build -j
 sudo cmake --install cmake-build          # /usr/local by default

Or with the makefile, which installs into ../lib:

 cd build
 make                 # make LARGEFILES=1 for the 64-bit database engine
 make install

Windows (Visual Studio 2022 or 2026, from a Developer Command Prompt):

 cmake -S . -B cmake-build -G "Visual Studio 17 2022" -A x64
 cmake --build cmake-build --config Release
 cmake --install cmake-build --config Release --prefix C:\datareel

Use -G "Visual Studio 18 2026" with Visual Studio 2026. To build with
OpenSSL instead of Schannel add:

 -DDATAREEL_SSL=OPENSSL -DOPENSSL_ROOT_DIR=E:/3plibs/openssl64

CMake options:
 DATAREEL_64BIT_DATABASE  64-bit database engine (default OFF)
 DATAREEL_NATIVE_TYPES    Native integer/float types (default ON)
 DATAREEL_SSL             OPENSSL or SCHANNEL
 DATAREEL_GUI             CONSOLE or WX (wxWidgets 3.x)
 BUILD_SHARED_LIBS        Shared library instead of static
 DATAREEL_BUILD_EXAMPLES  Examples, utilities and tests (default ON)

USING DATAREEL IN YOUR PROJECT
------------------------------
With an installed DataReel:

 find_package(DataReel 7 REQUIRED)
 target_link_libraries(myapp PRIVATE DataReel::datareel)

Or as a subdirectory of your project:

 add_subdirectory(datareel)
 target_link_libraries(myapp PRIVATE DataReel::datareel)

The DataReel::datareel target carries the include path and the
configuration macros, which must match between the library and your
program because some headers change layout with them.

FILE FORMATS
------------
DataReel 7 reads and writes the same files as 6.x. Two build settings
change the file layout, so a program must use the same settings as
the files it opens:

- DATAREEL_64BIT_DATABASE (makefile: LARGEFILES=1) selects 64-bit file
  addresses. 32-bit and 64-bit engine files are not interchangeable.
- DATAREEL_NATIVE_TYPES (default ON) makes gxINT32 and the other gx
  integer and float types the native C++ types; OFF makes them the
  portable byte-order classes. 7.4: OFF changes only the in-memory
  types of the gx integer and float classes. The library's files
  (block headers, B-tree nodes, journals) and the wire use the same
  canonical layout either way, so ON and OFF builds, Linux and
  Windows, share files and talk to each other. Your own records are
  raw bytes: a record that holds gx types has the size those types
  have in the build, so keep the setting the same for programs that
  share such records (or use fixed-size types such as int32_t).

EXAMPLE PROGRAMS AND TESTS
--------------------------
The examples are built with the library. Each is one source file:

 examples/database/   gxDatabase, gxBtree, POD, InfoHog
 examples/tls/        TLS echo server/client, HTTPS server/client,
                      SMTP, POP3, FTPS, gxStream server/client
 examples/oodb/       Object database patterns: typed objects with a
                      text export and import, gxdJournal crash
                      recovery, a day-organized event database and
                      replication over gxStream
 examples/general/    gxConfig, LogFile, ThreadPool, US trading
                      sessions (CDate), strutil/base 64/CRC-32
 examples/diagnostics ping, telnet

examples/README.txt says what each of the newer examples shows and how
to run it.

tests/run_tests.sh builds everything with CMake and runs the examples
against each other, the mail and FTPS clients against mock servers
(tests/mock_servers.py, needs python3), and tests/damaged_files, which
reads deliberately damaged database files. Pass CMAKE_OPTS to test
other configurations:

 tests/run_tests.sh
 CMAKE_OPTS="-DDATAREEL_64BIT_DATABASE=ON" tests/run_tests.sh /tmp/b64

DOCUMENTATION
-------------
Open docs/index.html in a browser. The class library pages are under
docs/classes, the examples under docs/examples and the utilities under
docs/utils. The guides in docs/ai (also collected in llms-full.txt)
are short task-oriented descriptions of the API with working code,
written for AI coding assistants but useful to anyone.

Security and the database port: docs/security_white_paper.html is
the security design of the library (TLS, mutual TLS, input checks,
database files, replication, limits, FIPS and quantum computing).
docs/gxstream_replication_white_paper.html explains gxStream and
object replication on the IANA-registered port 2073 (gxs-data-port),
and docs/rfc/draft-gaer-datareel-gxstream-00.txt is the protocol's
specification, an Internet-Draft for the Independent Submission
stream.

RELEASE HISTORY
---------------
 7.4.1 11/20/2026 Fix release: the object service address check
                  (security), gxObjRepl port 2073 by default, a stalled
                  copy no longer holds the primary, reconnects taken at
                  once, gxObjReplReader (read-only access from another
                  process), KNOWN_ISSUES.md ("NEW IN 7.4.1" in
                  RELEASE_NOTES_7.4.txt).
 7.4   11/17/2026 Final release: 7.4 beta 5 plus the fixes and additions
                  listed under "NEW IN 7.4 (AFTER BETA 5)" in
                  RELEASE_NOTES_7.4.txt.
       11/16/2026 Beta 5: object replication between Windows and Linux
                  (gxObjRepl), one file layout and one stream header on
                  every platform, the IMAP client and examples; gxrepl
                  page shipping removed (RELEASE_NOTES_7.4.txt).
       11/16/2026 Beta 4 (tag 7.4-beta4): gxrepl recovery gap fixes,
                  stamp-keeping fill.
       11/14/2026 Beta 3 (tag 7.4-beta3): gxrepl drain-verify fixes (no
                  verify cascade, position gone, a log made again,
                  ReadLogOf), per-object apply fixes A1-A3, scan fixes
                  S2/S3; security scan clean.
       11/14/2026 Beta 2 (tag 7.4-beta2). Vector extensions phases A-D:
                  vector index, HNSW search, float16/int8, vector sets,
                  vector searches in the object server, vector indexes
                  under gxrepl; gxrepl F4/F6/F7, the 7.3.2 fixes and the
                  per-object derived apply (off by default)
                  (RELEASE_NOTES_7.4.txt). Follows the 11/12/2026
                  7.4-beta preview (phases A-B).
 7.3.3 11/14/2026 Beta. Database replication fixes
                  (RELEASE_NOTES_7.3.3.txt)
 7.3.2 11/14/2026 Beta. Database replication fixes
                  (RELEASE_NOTES_7.3.2.txt)
 7.3.1 11/01/2026 Beta. Security scan fixes (RELEASE_NOTES_7.3.1.txt)
 7.3  10/31/2026  Beta. Memory-mapped reads, faster scans
                  (RELEASE_NOTES_7.3.txt)
 7.2  10/31/2026  Beta. Journal, page cache, positional I/O, hashed
                  B-tree cache (RELEASE_NOTES_7.2.txt)
 7.1  10/31/2026  Beta. Object database and TLS networking only:
                  gxRDBMS removed (RELEASE_NOTES_7.1.txt)
 7.0  10/26/2026  Beta. Modern C++17 rewrite, TLS-only networking, IPv6,
                  datareel namespace, CMake (RELEASE_NOTES_7.0.txt)
 6.2  10/25/2026  Loop guards for damaged files, SetWriteHook()
 6.1  10/25/2026  Security fixes in gxBtree and InfoHog
 6.0              Code review fixes (RELEASE_NOTES_6.0.txt and
                  CODE_REVIEW_FIXES.txt)

Release notes for the earlier versions are in
docs/release_notes_6_2.html, docs/release_notes_6_1.html and
docs/release_notes_6_0.html.
