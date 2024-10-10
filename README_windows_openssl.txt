-------------------------
OpenSSL Build for Windows
-------------------------

Must install a Perl interpreter:

https://strawberryperl.com/releases.html

Download the lasted version:

https://github.com/StrawberryPerl/Perl-Dist-Strawberry/releases/download/SP_53822_64bit/strawberry-perl-5.38.2.2-64bit-portable.zip

Unzip the package to:

C:\strawberry-perl

Must install a machine language compiler:

https://www.nasm.us/

Download the latest version:

https://www.nasm.us/pub/nasm/releasebuilds/2.16.03/win64/nasm-2.16.03-win64.zip

Unpack and copy the .exe files to:

C:\nasm\bin

Edit environment variables for your account

Edit the PATH variable and add C:\nasm\bin

Download the latest version of openssl or an earlier version starting with the 3.0 releases:

https://openssl-library.org/source/index.html

https://github.com/openssl/openssl/archive/refs/tags/openssl-3.3.2.zip

Unpack the distribution in your home folder:

%USERPROFILE%\3plisbs\openssl-3.3.2

To build setup the Visual Studio environment. Open a command prompt by running the CMD command in a run dialog. At the command prompt run:

> "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"

Setup the perl environment:

> "C:\strawberry-perl\portableshell.bat"

Run the openssl configure script:

> cd %USERPROFILE%\3plisbs\openssl-3.3.2
> perl Configure VC-WIN32 --prefix=c:\openssl -static

NOTE: Set "--prefix=" to match your installation location. 

For the 64-bit build run: perl Configure VC-WIN64A

NOTE: If your applicaion has to be FIPS compliant download an
openssl version that is FIPS certified. To build a FIPS
compliant version: perl Configure enable-fips

Start the build:

> nmake

Once complete test the build:

> nmake test

Install:

> nmake install

-----------------------------------
DataReel Build with OpenSSL Enabled
-----------------------------------

To build the datareel library with SSL enabled start with the static library build:

> cd %USERPROFILE%\datareel
> env\msvc.bat
> cd winslib 
> notepad openssl.env

Set the path to your openssl installation directory.

> nmake -f msvc_ssl.mak

The utility programs and examples programs will link to the static
library. Before you can build the utils or example programs with SSL
enabled you must set the path to your openssl install in the following
ENV file: 

> cd %USERPROFILE%\datareel\env
> notepad openssl_windows.env

To build the example programs:

> cd %USERPROFILE%\datareel\examples_socket
> cd https_client
> nmake -f msvc.mak

To build the datareel library as a DLL with SSL enabled:

> cd %USERPROFILE%\datareel
> env\msvc.bat
> cd dll
> notepad openssl.env

Set the path to your openssl installation directory.

> nmake -f msvc.mak

----------------------
ZLIB Build for Windows
----------------------

Download the latest version of ZLIB

https://zlib.net/

https://zlib.net/zlib131.zip

Unpack the distribution in your home folder:

%USERPROFILE%\3plisbs\zlib-1.3.1

To build setup the Visual Studio environment. Open a command prompt by
running the CMD command in a run dialog. At the command prompt run: 

> "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"

Build ZLIB using the MSC makefile:

> cd %USERPROFILE%\3plisbs\zlib-1.3.1
> nmake -f win32\Makefile.msc
