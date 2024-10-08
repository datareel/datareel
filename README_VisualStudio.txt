Microsoft Visual Studio install and testing for Windows

On a Windows sytem install visual studio, free version (VS Community 2022)
Run the VS installer and select:

Desktop development with C++
Under installation details select all options

Windows allocation development
Under installation details select all options

Following the install instrucitons to use the ENVs setup BAT file:

https://learn.microsoft.com/en-us/cpp/build/building-on-the-command-line?view=msvc-170 

Open a command prompt by running the CMD command. At the command line
look for the latest batch file used to setup the compiler ENV:

> cd \Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools
> dir *.bat

VsDevCmd.bat

Run the ENV script to setup the compiler ENV:

> cd %USERPROFILE%\Desktop
> "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"

Create a test program for compiler testing:

> notepad testprog.c

#include <stdio.h>

int main() 
{
  printf("Test program\n");
  return 0;
}

Compile and run the program

> cl testprog.c
> testprog.exe

