DataReel 7.0 Readme File
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
DataReel is a C++ toolkit for applications that keep their data in
local database files and talk to other systems over TLS. It began in
1997, when C++ had no standard string, container, thread or file
system classes, as a general purpose library that supplied all of
them. The C++ standard library now does that job, so DataReel 7 keeps
the parts the standard library still does not have and removes the
rest:

- An embedded object database: block files with free space reuse,
  CRC checks and file locking, disk B-tree indexes, persistent objects
  and simple relational tables, with tools to check and repair files.
- TLS networking: IPv4 and IPv6 sockets, TLS 1.2 and 1.3 with
  certificate checking, and HTTPS, SMTP, POP3, FTPS and database block
  clients and servers.
- Stable, portable file formats: files written by DataReel releases
  going back to 2001 still open, byte for byte.

The library has no dependencies except the system TLS library
(OpenSSL on Linux, Schannel or OpenSSL on Windows).

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
 gxDatabase        32/64-bit block file engine: allocate, write, read,
                   delete and reuse variable size blocks, per-block CRC,
                   file and record locking, large files
 gxBtree           Disk B-tree index with your own key class, optional
                   node cache (BtreeCache)
 POD               A data file and its index files opened together
 InfoHog           Persistent objects: a fixed key plus any number of
                   variable length members, indexed by key
 gxRDBMS           Relational tables defined by a text definition, with
                   primary and secondary keys, search and sort
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
See RELEASE_NOTES_7.0.txt for the full list and for porting notes.
In short:

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
- Database, index, InfoHog and gxRDBMS files are unchanged.

LICENSE
-------
The DataReel open source library is copyrighted by DataReel Software
Development, Copyright (c) 2001-2026, all rights reserved. DataReel
open source is available to non-profit, commercial, individual, and
academic developers. The open-source archive is distributed under the
GNU Lesser General Public License (LGPL) with provisions for use in
commercial and non-commercial applications under the terms of the
LGPL. See the LICENSE file.

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
- DATAREEL_NATIVE_TYPES (default ON) stores gxINT32 and the other
  fixed types as the native C++ types. On 64-bit Linux "long" is 8
  bytes and on Windows it is 4, so native builds on the two systems
  write different layouts. To share files between Linux and Windows
  build both with DATAREEL_NATIVE_TYPES=OFF, which uses the portable
  byte-order classes.

EXAMPLE PROGRAMS AND TESTS
--------------------------
The examples are built with the library. Each is one source file:

 examples/database/   gxDatabase, gxBtree, POD, InfoHog, gxRDBMS
 examples/tls/        TLS echo server/client, HTTPS server/client,
                      SMTP, POP3, FTPS, gxStream server/client
 examples/general/    gxConfig, LogFile, ThreadPool
 examples/diagnostics ping, telnet

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

RELEASE HISTORY
---------------
 7.0  10/26/2026  Modern C++17 rewrite, TLS-only networking, IPv6,
                  datareel namespace, CMake (RELEASE_NOTES_7.0.txt)
 6.2  10/25/2026  Loop guards for damaged files, SetWriteHook()
 6.1  10/25/2026  Security fixes in gxBtree and InfoHog
 6.0              Code review fixes (RELEASE_NOTES_6.0.txt and
                  CODE_REVIEW_FIXES.txt)

Release notes for the earlier versions are in
docs/release_notes_6_2.html, docs/release_notes_6_1.html and
docs/release_notes_6_0.html.
