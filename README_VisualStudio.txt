Microsoft Visual Studio install and testing for Windows

On a Windows sytem install visual studio, free version (VS Community 2026)

Reference links:
https://visualstudio.microsoft.com/downloads/
https://visualstudio.microsoft.com/free-developer-offers/ 

Download the Visual Studio 2026 installer from the downloads page
above. The Insiders installer also works:

https://visualstudio.microsoft.com/insiders/ 

Run the VS installer and select:

Desktop development with C++
Under installation details select all options

WinUI application development
Under installation details select all options

After installation with the above options there will be 4 command
prompts you can open with all the C/C++ environmental variables set.
The examples below are for the Insiders edition. For a release install
replace Insiders with Community, Professional, or Enterprise: 

%comspec% /k "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat"
%comspec% /k "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvarsamd64_x86.bat"
%comspec% /k "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars32.bat"
%comspec% /k "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvarsx86_amd64.bat"

To test the installation:

In a Run dialog paste the vcvars32.bat command prompt to start a build
environment: 

Search->Run
%comspec% /k "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars32.bat"

C:\Windows\System32>mkdir c:\tmp
C:\Windows\System32>cd \tmp

Create a test program for compiler testing:

C:\tmp>notepad testprog.c

#include <stdio.h>

int main() 
{
  printf("Test program\n");
  return 0;
}

C:\tmp>cl testprog.c
C:\tmp>testprog.exe

To build the datareel library start with the static library build:

> cd %USERPROFILE%\datareel
> env\msvc.bat
> cd winslib 
> nmake -f msvc.mak

The env\msvc.bat file calls env\vs2026_vars.bat, which finds your VS
2026 install with vswhere.exe and sets the ENV for 64-bit x64 builds,
which is the default.
It works with the Community, Professional, Enterprise, Build Tools, and
Insiders editions. To use a VS install it does not find, set DR_VSDIR
first:

> set DR_VSDIR=C:\Program Files\Microsoft Visual Studio\18\Community
> env\msvc.bat

For 32-bit x86 builds of legacy code use env\msvc32_vs2026.bat instead
of env\msvc.bat. env\msvc64_vs2026.bat is the same as env\msvc.bat.
Use a new command prompt when you switch between 32-bit
and 64-bit builds, and run "nmake -f msvc.mak clean" first, because the
32-bit and 64-bit builds use the same library file names. For VS 2022
use env\msvc_vs2022.bat, which also sets up x64 builds.

SSL is built into the gxcode library. By default the gxSSL class uses
the native Windows Schannel (SSPI) TLS API, so you do not need to
build OpenSSL. Programs link with secur32.lib, crypt32.lib and
ncrypt.lib from the Windows SDK. OpenSSL is still an option for code
that calls OpenSSL functions directly: build the library with
msvc_ssl.mak and your programs with USESSL=1.

The OpenSSL ENV files (winslib\openssl.env, dll\openssl.env and
env\openssl_windows.env) point to a 64-bit OpenSSL install by default.
For 32-bit builds set OPENSSL_DIR to your 32-bit OpenSSL install.

The library builds use these settings:

FINAL=1      Release build with /MT /O2 (default in msvc.mak)
FINAL=0      Debug build with /MTd /Z7 /Od
64BITCFG=1   64-bit database engine, which can use files larger than
             2 GB. Builds gxcode64.lib. This is not the same as an
             x64 build. It works with the 32-bit and 64-bit compilers.
USESSL=0     SSL with native Windows Schannel (default)
USESSL=1     SSL with OpenSSL instead of Schannel (see msvc_ssl.mak
             and README_windows_openssl.txt)

To build a debug static library and the 64-bit database engine:

> cd %USERPROFILE%\datareel\winslib
> nmake -f project.mak FINAL=0 64BITCFG=1 USESSL=0

Programs that link to the library must be built with the same FINAL
and 64BITCFG settings, for example:

> cd %USERPROFILE%\datareel\utils\hexdump
> nmake -f msvc.mak FINAL=0 64BITCFG=1

The utility programs and examples programs will link to the static
library. To build the utils:

> cd %USERPROFILE%\datareel\utils
> cd hexdump
> nmake -f msvc.mak

To build the examples:

> cd %USERPROFILE%\datareel\examples_database
> cd simple
> nmake -f msvc.mak

> cd %USERPROFILE%\datareel\examples_general
> cd string_class
> nmake -f msvc.mak

> cd %USERPROFILE%\datareel\examples_socket
> cd simple
> nmake -f msvc.mak

> cd %USERPROFILE%\datareel\examples_threads
> cd simple
> nmake -f msvc.mak

To build the datareel library as a DLL:

> cd %USERPROFILE%\datareel
> env\msvc.bat
> cd dll
> nmake -f msvc.mak
