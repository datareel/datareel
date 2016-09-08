// Core dump program used to test your GDB setup
// Last modified: 09/08/2016

// You must setup the debug env before testing:
//
// $ source $HOME/git/datareel/env/gnu_debug_env.sh
// $ ulimit -c  

#include <iostream>
#include <string.h>

int main()
{
  char *p = new char[255];
  memset(p, 0, 255);

  strcpy(p, (char *)"$ g++ -O0 -Wall -g coredump.cpp\n$ gdb ./a.out\n(gdb) run");
  std::cout << p << "\n" << std::flush;

  std::cout << "Causing a core dump" << "\n" << std::flush;
  delete p; p = 0;
  strcpy(p, (char *)"We will crash here");

  return 0;
}
