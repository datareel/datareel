DataReel 5.X Readme File
DataReel Copyright (c) 2001-2025 DataReel Software Development

https://github.com/datareel

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
from all developers. Project git repo:

https://github.com/datareel/datareel
 
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
Development, Copyright (C) 2001-2024, all rights reserved. DataReel
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
Enterprise Linux 8/9/10, Windows 10/11, and Windows 2025 server.

The datareel library can be compiled on any Linux variant with the
GNU compiler. 

The datareel library can be compiled for Windows using Microsoft
Visual Sudio.

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
Visual Sudio 2022 or 2025.

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

BUILDING UNIX SHARED AND STATIC LIBRARIES
--------------------__-------------------
Example install and build:

> mkdir -pv ~/git
> cd ~/git
> git clone https://github.com/datareel/datareel
> cd datareel
> cd env

In the env directory edit the linux.env file if you need to make any
adjustments or modifications to the build 

> source gnu_env.sh
> cd ../build
> make 

> make install
> make clean

To build with OpenSSL:

> export USE_OPEN_SSL=YES
> make -f makefile_ssl
> make -f makefile_ssl install
> make -f makefile_ssl clean

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
env directory. 

To build the datareel static library without SSL enabled:

> cd %USERPROFILE%\datareel
> env\msvc.bat
> cd winslib 
> nmake -f msvc.mak

The utility programs and examples programs will link to the static
library. 

To build the example programs:

> cd %USERPROFILE%\datareel\examples_general
> cd string_class
> nmake -f msvc.mak

To build the datareel library as a DLL without SSL enabled:

> cd %USERPROFILE%\datareel
> env\msvc.bat
> cd dll
> nmake -f msvc.mak

To build the datareel static library with SSL enabled:

> cd %USERPROFILE%\datareel
> env\msvc.bat
> cd winslib 
> notepad openssl.env

In the openssl.env file set the path to your openssl installation directory.

> nmake -f msvc_ssl.mak

The utility programs and examples programs will link to the static
library. Before you can build the utils or example programs with SSL
enabled you must set the path to your openssl install in the following
ENV file: 

> cd %USERPROFILE%\datareel\env
> notepad openssl_windows.env

In the openssl_windows.env  set the path to your openssl installation
directory.

To build the example programs:

> cd %USERPROFILE%\datareel\examples_socket
> cd https_client
> nmake -f msvc.mak

Before running the example program in you user environment add the
openssl\bin dir to your PATH:

> set PATH=%PATH%;e:\3plibs\openssl\bin

To build the datareel library as a DLL with SSL enabled:

> cd %USERPROFILE%\datareel
> env\msvc.bat
> cd dll
> notepad openssl.env

In the openssl.env file set the path to your openssl installation directory.

> nmake -f msvc.mak

DataReel Copyright (c) 2001-2025 DataReel Software Development
