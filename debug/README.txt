Readme file for GDB and valgrind debugging
Last modified: 09/21/2016

* Overview
* GDB source code debugging
* valgrind memory leak testing

Overview
--------
This document contains general purpose instructions to set a Linux/GNU
environment for debugging source code and checking for memory
leaks. These instructions can be applied to the Datareel source
builds, other projects, or single source code files. 
 
GDB source code debugging
-------------------------
GDB is the GNU debugging tool. To use GDB source code must be compiled
with symbolic debugging. The most effective way to setup symbolic
debugging to override the environmental variables used by  implicit
make rule. In the BASH shell, export the following environmental
variables: 

export CC="gcc"
export CXX="g++"
export CPP="${CC} -E"
export CFLAGS="-O0 -Wall -v -g"
export CXXFLAGS="-O0 -Wall -v -g"

The "-g" flag enables symbolic debugging. The -O0 (letter O, number
zero) flag turns off all compiler optimization. The "-Wall" flag turns
on all compiler warnings. NOTE: When debugging code it's important to
treat compiler warnings as error and fix wherever possible. 

Next setup your hard and soft user limit value for core dumps. In the
BASH shell run the following commands: 

ulimit -H -c unlimited
ulimit -S -c unlimited
ulimit -c  

To test GDB, compile the simple program below:

$ vi coredump.cpp

#include <iostream>
#include <string.h>

int main()
{
  char *p = new char[255];
  memset(p, 0, 255);

  strcpy(p, (char *)"GDB test program");
  std::cout << p << "\n" << std::flush;

  std::cout << "Causing a core dump" << "\n" << std::flush;
  delete p; p = 0;
  strcpy(p, (char *)"We will crash here");

  return 0;
}

Save changes and exit VI. To compile:

$CXX $CXXFLAGS coredump.cpp

To debug:

gdb a.out 

This will take you to the GDB prompt:

(gdb) 

Enter run to execute the program. NOTE: If your program takes or
requires input arguments supply the arguments after the run command: 

(gdb) run

Program received signal SIGSEGV, Segmentation fault.
0x0000000000400a16 in main () at coredump.cpp:14
14	  strcpy(p, (char *)"We will crash here");

(gdb) quit

NOTE: If you need install any additional GDB dependencies, GDB will
alert you and provide instructions to install the additional
dependencies.   

Another way to invoke GDB is to call the program with core dump file,
for example: 

./a.out

Segmentation fault (core dumped)

ls -rtal core.*
gdb ./a.out  core.16591

NOTE: Substitute “core.16591” with the name of your core dump file.

valgrind memory leak testing
---------------------------
valgrind combined with GDB is a good tool for memory leak testing. In
C/C++ program it is very important to check for memory leaks to ensure
memory resources are properly released when dynamically
allocated. This includes variables you put in heap space using
malloc/new and variables dynamically allocated by C library function
calls.  

Below is a simple C++ program you can use to test your valgrind
installation. If you do not have valgrind installed you will need to
install: 

# yum install valgrind

To test valgrind, compile the simple program below:

$ vi memleak.cpp
  
#include <iostream>
#include <string.h>

int main()
{
  char *p = new char[255];
  memset(p, 0, 255);

  strcpy(p, (char *)"Memory leak test program");
  std::cout << p << "\n" << std::flush;

  std::cout << "Causing a memory leak" << "\n" << std::flush;
  p = 0;

  return 0;
}

Save your changes and exit VI. Next, compile the program and run the
program using valgrind: 

$ g++ -O0 -Wall -g memleak.cpp
$ valgrind --tool=memcheck --leak-check=yes ./a.out

==30670== LEAK SUMMARY:
==30670==    definitely lost: 255 bytes in 1 blocks

To correct:

$ vi memleak.cpp
  
...
  std::cout << "Causing a memory leak" << "\n" << std::flush;
  delete[] p; 	
  p = 0;

  return 0;
}

$ g++ -O0 -Wall -g memleak.cpp
$ valgrind --tool=memcheck --leak-check=yes ./a.out

==30932== All heap blocks were freed -- no leaks are possible


