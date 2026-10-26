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

The default Windows build is 64-bit x64. The 32-bit x86 builds are
only for legacy systems. Build each library in a new command prompt
for the target you are building.

To build the 64-bit ZLIB from a Windows command prompt:

E:
E:\>cd 3plibs
E:\3plibs>cd distrib
E:\3plibs\distrib>unzip -d e:\3plibs zlib131.zip
E:\3plibs\distrib>cd e:\3plibs
E:\3plibs>move zlib-1.3.1 zlib64-1.3.1
E:\3plibs>cd zlib64-1.3.1
E:\3plibs\zlib64-1.3.1>"C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat"
E:\3plibs\zlib64-1.3.1>nmake -f win32\Makefile.msc

For a 32-bit legacy build unpack a second copy to zlib32-1.3.1 and
use vcvars32.bat instead of vcvars64.bat.

----------------------------
OpenSSL Version Requirements
----------------------------
When you build DataReel or your programs with OpenSSL on Windows, use
only an OpenSSL 3.5.x LTS (long term support) release. Do not use
OpenSSL 3.0 through 3.4 or a newer non-LTS release. When a new 3.5.x
release is published, rebuild OpenSSL with it and rebuild the DataReel
library and your programs. This document uses OpenSSL 3.5.9, the
latest 3.5.x LTS release. The default DataReel SSL build uses the
native Windows Schannel API and does not need OpenSSL.

Download the latest 3.5.x LTS release from:

https://github.com/openssl/openssl/releases 
https://openssl-library.org/source/

To build the 64-bit OpenSSL from a Windows command prompt:

E:
E:\>cd e:\3plibs
E:\3plibs>tar -xzf distrib\openssl-3.5.9.tar.gz
E:\3plibs>move openssl-3.5.9 openssl64-openssl-3.5.9
E:\3plibs>cd openssl64-openssl-3.5.9
E:\3plibs\openssl64-openssl-3.5.9>"C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat"
E:\3plibs\openssl64-openssl-3.5.9>C:\strawberry-perl\portableshell.bat
E:\3plibs\openssl64-openssl-3.5.9>perl Configure VC-WIN64A --prefix=e:\3plibs\openssl64 --openssldir=e:\3plibs\openssl64 --with-zlib-include=e:\3plibs\zlib64-1.3.1 --with-zlib-lib=e:\3plibs\zlib64-1.3.1 enable-fips no-shared
E:\3plibs\openssl64-openssl-3.5.9>perl configdata.pm --dump
E:\3plibs\openssl64-openssl-3.5.9>nmake
E:\3plibs\openssl64-openssl-3.5.9>nmake test

All tests successful.
Files=347, Tests=4214, 2135 wallclock secs ( 8.45 usr +  2.03 sys = 10.48 CPU)
Result: PASS

E:\3plibs\openssl64-openssl-3.5.9>nmake install_sw install_ssldirs install_fips
E:\3plibs\openssl64-openssl-3.5.9>e:\3plibs\openssl64\bin\openssl.exe version
OpenSSL 3.5.9 29 Sep 2026 (Library: OpenSSL 3.5.9 29 Sep 2026)

To build the 32-bit OpenSSL for legacy systems, unpack a second copy to
openssl32-openssl-3.5.9 and in a new command prompt use vcvars32.bat,
VC-WIN32, zlib32-1.3.1 and --prefix=e:\3plibs\openssl32
--openssldir=e:\3plibs\openssl32:

E:\3plibs\openssl32-openssl-3.5.9>"C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars32.bat"
E:\3plibs\openssl32-openssl-3.5.9>C:\strawberry-perl\portableshell.bat
E:\3plibs\openssl32-openssl-3.5.9>perl Configure VC-WIN32 --prefix=e:\3plibs\openssl32 --openssldir=e:\3plibs\openssl32 --with-zlib-include=e:\3plibs\zlib32-1.3.1 --with-zlib-lib=e:\3plibs\zlib32-1.3.1 enable-fips no-shared
E:\3plibs\openssl32-openssl-3.5.9>nmake
E:\3plibs\openssl32-openssl-3.5.9>nmake test
E:\3plibs\openssl32-openssl-3.5.9>nmake install_sw install_ssldirs install_fips

NOTE: To build a debug version of OpenSSL run the perl configure
script with debug-VC-WIN64A, or debug-VC-WIN32 for 32-bit builds.

-----------------------------------
DataReel Build with OpenSSL Enabled
-----------------------------------

DataReel 7 uses the Windows Schannel TLS library by default, which
needs nothing installed. To build with OpenSSL instead, point CMake at
the OpenSSL install. From a Visual Studio Developer Command Prompt:

> cd %USERPROFILE%\datareel
> cmake -S . -B build-openssl -G "Visual Studio 18 2026" -A x64 ^
    -DDATAREEL_SSL=OPENSSL -DOPENSSL_ROOT_DIR=E:/3plibs/openssl64 ^
    -DOPENSSL_USE_STATIC_LIBS=ON
> cmake --build build-openssl --config Release

Use -G "Visual Studio 17 2022" with Visual Studio 2022. The examples
are built into build-openssl\examples\Release. With static OpenSSL
libraries the programs need no OpenSSL DLLs; with the shared libraries
add the OpenSSL bin directory to your PATH before running them:

> set PATH=%PATH%;E:\3plibs\openssl64\bin

OpenSSL 3.2 and later builds also trust the Windows certificate store,
so HTTPS servers with public certificates verify without a CA file.
