-------------------------
OpenSSL Build for Windows
-------------------------

For the OpenSSL build you will need to install Perl for Windows and
NASM if you build OpenSSL with machine language optimizations.  

To install Perl for Windows:

Download the latest portable version strawberry Perl:

https://github.com/StrawberryPerl/Perl-Dist-Strawberry/releases 

Unzip the 64-bit portable package to C:\strawberry-perl

To setup the perl environment in a command prompt window run:

C:\strawberry-perl\portableshell.bat

Install NASM for Windows:

Download the latest release version:

https://www.nasm.us/pub/nasm/releasebuilds/2.16.03

Copy the binary EXE files to:

C:\nasm\bin

Add the NASM bin path to the path for your account:

Search->Edit environment variables for your account
User variables for username->Path->Edit->New->C:\nasm\bin

For the OpenSSL build you will need to build the ZLIB compression library:

https://zlib.net/

For all the build examples below this document will reference
E:\3plibs\distrib for the location of source code download packages
and reference E:\3plibs as the build location for 3rd party libs. 

The examples use a command line version of unzip.exe to unpack the zip
archives. 

To build ZLIB from a Windows command prompt:

E:
E:\>cd 3plibs
E:\3plibs>cd distrib
E:\3plibs\distrib>unzip -d e:\3plibs zlib131.zip
E:\3plibs\distrib>cd e:\3plibs
E:\3plibs>move zlib-1.3.1 zlib32-1.3.1
E:\3plibs>cd zlib32-1.3.1
E:\3plibs\zlib32-1.3.1> "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars32.bat"
 "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars32.bat"
E:\3plibs\zlib32-1.3.1>nmake -f win32\Makefile.msc

For the OpenSSL build you will need to download the release you wish
to use from: 

https://github.com/openssl/openssl
https://github.com/openssl/openssl/releases 

In this document we will be using version 3.2.2 to match the RHEL 9
and 10 version. 

To build OpenSSL from a Windows command prompt:

E:
E:\>cd e:\3plibs
E:\3plibs\distrib>unzip -d e:\3plibs openssl-openssl-3.2.2.zip
E:\3plibs\distrib>cd e:\3plibs
E:\3plibs>move openssl-openssl-3.2.2 openssl32-openssl-3.2.2
E:\3plibs>cd openssl32-openssl-3.2.2
E:\3plibs\openssl32-openssl-3.2.2>"C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars32.bat"
E:\3plibs\openssl32-openssl-3.2.2>C:\strawberry-perl\portableshell.bat
E:\3plibs\openssl32-openssl-3.2.2>perl Configure VC-WIN32 --prefix=e:\3plibs\openssl --openssldir=e:\3plibs\openssl --with-zlib-include=e:\3plibs\zlib32-1.3.1 --with-zlib-lib=e:\3plibs\zlib32-1.3.1 enable-fips no-shared
E:\3plibs\openssl32-openssl-3.2.2>perl configdata.pm --dump
E:\3plibs\openssl32-openssl-3.2.2>nmake
E:\3plibs\openssl32-openssl-3.2.2>nmake test

All tests successful.
Files=298, Tests=3164, 978 wallclock secs ( 7.58 usr +  1.17 sys =  8.75 CPU)
Result: PASS

E:\3plibs\openssl32-openssl-3.2.2>nmake install
E:\3plibs\openssl32-openssl-3.2.2>move e:\3plibs\openssl e:\3plibs\openssl32
E:\3plibs\openssl32-openssl-3.2.2>e:\3plibs\openssl32\bin\openssl.exe version
OpenSSL 3.2.2 4 Jun 2024 (Library: OpenSSL 3.2.2 4 Jun 2024)

NOTE: To build a debug version of OpenSSL run the perl configure script with the following option: 
>perl Configure debug-VC-WIN32

-----------------------------------
DataReel Build with OpenSSL Enabled
-----------------------------------
In the example build below we will assume you are working with a copy of the datareel
library unpacked to the "%USERPROFILE%\datareel" location.

To build the datareel library with SSL enabled start with the static library build:

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

In the openssl_windows.env  set the path to your openssl installation directory.

--
To build the example programs:

> cd %USERPROFILE%\datareel\examples_socket
> cd https_client
> nmake -f msvc.mak

Before running the example program in you user environment add the
openssl\bin dir to your PATH:

> set PATH=%PATH%;e:\3plibs\openssl\bin

--
To build the datareel library as a DLL with SSL enabled:

> cd %USERPROFILE%\datareel
> env\msvc.bat
> cd dll
> notepad openssl.env

In the openssl.env file set the path to your openssl installation directory.

> nmake -f msvc.mak
