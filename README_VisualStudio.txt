Microsoft Visual Studio install and testing for Windows

On a Windows sytem install visual studio, free version (VS Community 2026)

Reference links:
https://visualstudio.microsoft.com/downloads/
https://visualstudio.microsoft.com/free-developer-offers/ 

Download the Visual Studio 2026 Insiders installer from:

https://visualstudio.microsoft.com/insiders/ 

Run the VS installer and select:

Desktop development with C++
Under installation details select all options

WinUI application development
Under installation details select all options

After installation with the above options there will be 4 command
prompts you can open with all the C/C++ environmental variables set: 

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
