DataReel 6.1 Readme File
DataReel Copyright (c) 2001-2026 DataReel Software Development

https://datareel.com

CONTENTS
--------
Overview 
Features 
License 
Supported Platforms 
Supported Compilers 
Package Map 
Example Programs 
Documentation 
Code Review and Fixes 
Building

OVERVIEW
--------
What is DataReel?
Datareel is a comprehensive cross-platform C++ development kit used to
build multi-threaded database and communication applications. C++ is a
programming language that produces fast executing compiled programs
and offers very powerful programming capabilities. Unlike interpreted
languages such as JAVA and PERL the C++ language by itself does not
contain built-in programming interfaces for database, communications,
and multi-threaded programming. By using DataReel you can extend the
power of the C++ programming language by using high-level programming
interfaces for database, communications, and multi-threaded
programming.

The DataReel development package was produced by independent work and
contract work released to the public through non-exclusive license
agreements. The initial work began independently in 1997 and was
augmented from 1999 to 2004 by code produced under contract to support
various applications. Several developers throughout the World have
made contributions to enhance the DataReel code and promote its
stability. In 2005 the DataReel code library underwent intense
analysis to produce a bulletproof code base suitable for use in
complex commercial applications. 

Why Use DataReel?
DataReel simplifies complex time consuming database, socket, and
multi-threaded programming tasks by providing JAVA like programming
interfaces for database, communications, and multi-threaded
programming. Using DataReel you can harness the full power of the C++
programming language and build stable end-user applications, embedded
systems, and reusable libraries for multiple operating systems. 

DataReel is flexible. Using DataReel gives your developers the
flexibility to develop core application components independently of
complex user interfaces. DataReel is portable. DataReel not only
offers portability but also interoperability between multiple
platforms. DataReel is modular. DataReel is a modular approach to
network and database programming making code adaptation and
cross-platform testing easy. 

Who Can Use DataReel?
The DataReel toolkit is available to commercial, individual, and
academic developers. The DataReel code base is distributed to the
public in an open source format. This keeps the code stable through
the continued support of worldwide developers who submit code
enhancements and report potential problems. The open source format is
also intended to promote the C++ programming language as the language
of choice for any programming task. 

Who Can Contribute?
The DataReel project accepts bug fixes, enhancements, and additions
from all developers. Source code releases are on the DataReel
website, and contributions are sent through the contact page:

https://datareel.com/downloads/datareel/
https://datareel.com/contact.php
 
FEATURES
--------
Database
 WIN32/UNIX Interoperability
 32-Bit DB Engine
 64-Bit DB Engine
 Large file support
 CRC Checking
 Portable File Locking
 Portable Record Locking
 B-tree Indexing
 Persistent Objects
 Supports OODM Design
 Supports RDBMS Design
 Supports Multi-threading
 Supports Client/Server Apps
 Built-in Network Database
 Real-time TCP Streaming
 Real-time UDP Streaming
 RS232 streaming

Sockets
 WIN32/UNIX Interoperability
 Winsock/BSD Wrappers
 Object-Oriented Design
 Stream Sockets
 Datagram Sockets
 RS232 Support
 Supports Multi-threading
 Embedded Ping
 Embedded FTP
 Embedded Telnet
 Embedded SMTP
 Embedded POP3
 Embedded HTTP
 URL Parsing
 HTML Parsing
 HTML Generator
 Embedded SSL
 XML Parsing

Threads
 WIN32/UNIX Interoperability
 Windows/POSIX Wrappers
 Object-Oriented Design 
 Thread Creation/Construction 
 Thread Destruction 
 Cancellation 
 Exit 
 Join 
 Suspend 
 Resume 
 Sleep 
 Priority Functions 
 Thread Specific Storage 
 Thread Pooling
 Mutex Locks 
 Conditional Variables 
 Semaphore Synchronization

General
 String Classes
 Memory Buffers
 Device Caching
 Linked List Classes
 Binary Search Tree
 Stack Classes
 Queue Classes
 Date/Time Classes
 Configuration Manager
 Log Generator
 Postscript Text Generator
 Portable TERM I/O
 Text Utilities
 String Utilities
 Portable Directory Functions
 Portable File Functions

LICENSE
-------
The DataReel open source library is copyrighted by DataReel Software
Development, Copyright (C) 2001-2026, all rights reserved. DataReel
open source is available to non-profit, commercial, individual, and
academic developers. The open-source archive is distributed under the
GNU Lesser General Public License (LGPL) with provisions for use in
commercial and non-commercial applications under the terms of the
LGPL.

Under the terms of the LGPL you can use the open source code in
commercial applications as a static or dynamically linked
library. This allows commercial developers to compile the library and
link to it without making any changes to the DataReel source code. 

GNU Lesser General Public License 
This library is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation; either version 3.0 of the
License, or (at your option) any later version.  

This library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Lesser General Public License for more details.  

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, a copy is available on the
gnu.org Website: 

https://www.gnu.org/licenses/lgpl.html

SUPPORTED PLATFORMS
-------------------
The current Datareel release was developed and tested on Red Hat
Enterprise Linux 10 (the default build platform), RHEL 9 and RHEL 8,
Windows 11, and Windows Server 2022 and 2025. RHEL 6 and 7, and
Windows releases older than Windows 11 and Windows Server 2022, are
no longer supported.

On Linux the library uses the OpenSSL that comes with the operating
system. On Windows use only an OpenSSL 3.5.x LTS release (see
README_windows_openssl.txt). The Windows build is 64-bit by default,
32-bit builds are kept for legacy code only.

The datareel library can be compiled on any Linux variant with the
GNU compiler. 

The datareel library can be compiled for Windows using Microsoft
Visual Studio.

Legacy platforms in the code base include HPUX and Solaris but make
files are no longer distributed. 
 
Datareel has been compiled for use in 16-bit, 32-bit, and 64-bit
applications. The code base still supports 32-bit applications but
current development is mainly for use in 64-bit applications.
      
SUPPORTED COMPILERS
-------------------
For UNIX the current Datareel release supports GNU and Intel compiler
builds. The code base has been used in super computer applications
compiled with Cray C++ compilers.

For Windows the current Datareel release supports Microsoft
Visual Studio 2022 or 2026.

Previous releases supported cross platform Windows builds using MSVC,
Visual C++, BCC, and DJGPP. 

PACKAGE MAP
-----------
DataReel Directory Structure 

    bin - Directory used to intall executables
    docs - Directory for all DataReel documentation sets
    env - Makefile includes used to build libraries example programs
    examples_general - General support example programs
    examples_database - Database example programs
    examples_threads - Thread example programs
    examples_socket - Socket example programs
    include - DataReel include files
    lib - Empty directory used to install library files
    src - DataReel source code files
    utils - DataReel utility programs

EXAMPLE PROGRAMS
----------------
The Datareel distribution contains several example programs. The
examples are fully functional programs used to test and demonstrate
each component of the library individually. The example programs are
located within subdirectories of the "examples_general",
"examples_database", "examples_threads", and "examples_socket"
directories.

This HTML documentation set include many more example programs that
can by used as templates to build applications. 

DOCUMENTATION
-------------
All documentation is included in the docs sub directory in an HTML
format, viewable in any Web browser. 

VERSION 6.1
-----------
DataReel 6.1, 10/25/2026, is a security fix release. gxBtree and
InfoHog now check the lengths and counts they read from database and
index files, so a damaged or crafted file is rejected instead of
overflowing memory. The shared library is built as libdatareel.so.6.1.
The only interface change from 6.0 is the new
gxDatabase::SetWriteHook(); recompile and relink. See
RELEASE_NOTES_6.1.txt. Every change in the source is marked with a
"10/25/2026" comment.
Supported platforms are now RHEL 10 (default), 9 and 8, Windows 11,
and Windows Server 2022 and 2025.

VERSION 6.0
-----------
DataReel 6.0, 10/24/2026, is the first major release in many years.
Version 6.0 fixes every item that the 10/14/2026 code review
left as is: linked list array cleanup, gxConfig::Add(), stopping
threads when a thread pool is cleared, database cache Bind() and
ReleaseSegments(), RDBMS duplicate key lookups and BLOB fields in
all-field keys, mixed text in the XML parser, the RSS <cloud> element,
the glibc robust mutex problem, and all -Wall -Wextra compiler
warnings. The shared library is built as libdatareel.so.6.0. Programs
must be recompiled, and some interfaces changed. See
RELEASE_NOTES_6.0.txt and docs/release_notes_6_0.html. Every change in
the source is marked with a "10/24/2026" comment.

CODE REVIEW AND FIXES
---------------------
On 10/14/2026 the DataReel code in this repository was reviewed and
fixed by Claude Code, Anthropic's AI coding assistant
(https://claude.ai/code). The review covered compiler warnings,
AddressSanitizer and UndefinedBehaviorSanitizer runs of the example
programs, and valgrind checks. Fixes include:

 - Mismatched new[]/delete calls in UString, strutil, gxDatabase,
   MemoryBuffer and other classes
 - Undefined behavior in the gxINT16/32/64 byte packing, UString
   Find/IFind, and gxConfig::ChangeConfigLine()
 - Bounds checks in POD, gxRDBMS and gxsXML, and leaked buffers in
   gxSocket
 - Replaced the deprecated readdir_r() and _np robust mutex calls
 - SSL updates for OpenSSL 3: a 2048-bit (RFC 7919 ffdhe2048) default
   DH group, OpenSSL 3 DH parameter and RSA key interfaces, error
   checking and cleanup on every path, and new ssl_server example
   keys, certificate and DH parameters
 - New gxSSL class documentation: docs/classes/socket/gxssl.htm
 - New documentation pages for gxListB, the thread pool and thread
   helpers, the XML parser, the RSS feed writer, the RDBMS layer, the
   B-tree and database caches, the debug manager, ix_debug and the
   example programs, and a modern style for all documentation pages
 - Fixes for bugs found while writing the new pages: the RDBMS layer,
   thread helpers and thread pool, XML parser and RSS writer, B-tree
   and database caches, debug manager, B-tree Find() and gxDatabase

These fixes are included in DataReel 6.0 and later. The full list of fixes, SSL
updates and behavior changes is in
CODE_REVIEW_FIXES.txt and docs/code_review_fixes.html. Every change
in the source is marked with a "10/14/2026" comment.

BUILDING UNIX SHARED AND STATIC LIBRARIES
--------------------__-------------------
Example install and build:

> mkdir -pv ~/git
> cd ~/git
> wget https://datareel.com/downloads/datareel/datareel-<version>.tar.gz
> tar xzf datareel-<version>.tar.gz
> mv datareel-<version> datareel
> cd datareel
> cd env

In the env directory edit the linux.env file if you need to make any
adjustments or modifications to the build 

> source gnu_env.sh
> cd ../build
> make 

> make install
> make clean

SSL is built into the libdatareel library with OpenSSL by default.
There is no separate libdatareel_ssl library. The OpenSSL development
files must be installed (the libssl-dev package on Debian and Ubuntu,
openssl-devel on RHEL and Fedora). Linux builds always use the
operating system's OpenSSL. Do not build against a private OpenSSL
copy on Linux, and do not mix it with the system version.

On a system without OpenSSL you can build libdatareel without SSL:

> make USE_OPEN_SSL=NO
> make USE_OPEN_SSL=NO install
> make USE_OPEN_SSL=NO clean

You can also export USE_OPEN_SSL=NO before running make. Programs
that link to the library must be built with the same setting.

To build example programs:

> cd ~/git/datareel/examples_database
> ls -l 
> cd simple
> make

To build utility programs

> cd ~/git/datareel/utils
> ls
> cd hexdump
> make

BUILDING WITH CMAKE
-------------------
A CMakeLists.txt in the top-level directory builds the same objects as
the makefiles into a static library. It works on Linux and Windows
(Visual Studio 2022/2026) and needs CMake 3.16 or later. The makefiles
are unchanged and can still be used.

> cd ~/git/datareel
> cmake -S . -B cmake-build
> cmake --build cmake-build -j

Build options (pass with -D<option>=<value>):

DATAREEL_SSL            OPENSSL (Linux default), SCHANNEL (Windows
                        default) or NONE
DATAREEL_GUI            CONSOLE (default) or WX for wxWidgets 3.x
                        message boxes and wxString support
DATAREEL_64BIT_DATABASE ON for the 64-bit database engine with large
                        file support (default OFF)
DATAREEL_NATIVE_TYPES   ON (default) to use native integer and float
                        types

Other CMake projects can build and link the library directly:

add_subdirectory(path/to/datareel datareel)
target_link_libraries(myapp PRIVATE DataReel::datareel)

The configuration macros are passed on to programs that link the
library, so they are always compiled with matching settings.

wxWidgets applications can define __wxWIN3__ to select the wxWidgets
3.x code for any 3.x release. It is interchangeable with the older
__wxWIN302__ macro.

BUILDING WINDOWS STATIC LIBRARY and DLL
---------------------------------------
In the example build below we will assume you are working with a copy
of the datareel library unpacked to the "%USERPROFILE%\datareel"
location. 

In this document we will assume you have installed Visual Studio 2026
and will be working on the command line.

To setup a command line build open a Run dialog and enter cmd to open
a command prompt.

To setup the VS 2026 environment the msvc.bat file is provided in the
env directory. The default build is 64-bit x64. For 32-bit x86 builds
of legacy code use env\msvc32_vs2026.bat instead of env\msvc.bat.

SSL is built into the gxcode library. By default the gxSSL class uses
the native Windows Schannel (SSPI) TLS API, so OpenSSL is not needed.
OpenSSL is still available as an option for existing code, see below.
When you build with OpenSSL on Windows, use only an OpenSSL 3.5.x LTS
(long term support) release, 64-bit by default and 32-bit for legacy
builds. See README_windows_openssl.txt.

To build the datareel static library:

> cd %USERPROFILE%\datareel
> env\msvc.bat
> cd winslib 
> nmake -f msvc.mak

The utility programs and examples programs will link to the static
library. Programs link to secur32.lib, crypt32.lib, and ncrypt.lib
for SSL, which are part of the Windows SDK.

To build the example programs:

> cd %USERPROFILE%\datareel\examples_general
> cd string_class
> nmake -f msvc.mak

> cd %USERPROFILE%\datareel\examples_socket
> cd https_client
> nmake -f msvc.mak

To build the datareel library as a DLL:

> cd %USERPROFILE%\datareel
> env\msvc.bat
> cd dll
> nmake -f msvc.mak

To build the datareel static library with OpenSSL instead of Schannel:

> cd %USERPROFILE%\datareel
> env\msvc.bat
> cd winslib 
> notepad openssl.env

In the openssl.env file set the path to your OpenSSL 3.5.x LTS
installation directory (3plibs\openssl64 by default, or
3plibs\openssl32 for a legacy 32-bit build).

> nmake -f msvc_ssl.mak

Programs that link to the OpenSSL build of the library must also be
built with USESSL=1. Set the path to your OpenSSL 3.5.x LTS install
in the following ENV file: 

> cd %USERPROFILE%\datareel\env
> notepad openssl_windows.env

Then build your program or an example program with USESSL=1:

> cd %USERPROFILE%\datareel\examples_socket
> cd https_client
> nmake -f msvc.mak USESSL=1

Before running the example program in you user environment add the
OpenSSL 3.5.x bin dir to your PATH:

> set PATH=%PATH%;e:\3plibs\openssl64\bin

To build the datareel library as a DLL with OpenSSL:

> cd %USERPROFILE%\datareel
> env\msvc.bat
> cd dll
> notepad openssl.env

In the openssl.env file set the path to your OpenSSL 3.5.x LTS
installation directory (3plibs\openssl64 by default, or
3plibs\openssl32 for a legacy 32-bit build).

> nmake -f msvc_ssl.mak

See README_windows_openssl.txt for building OpenSSL 3.5.x LTS on
Windows.

DataReel Copyright (c) 2001-2026 DataReel Software Development
