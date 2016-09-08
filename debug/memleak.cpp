// Memory leak program used to test your GDB and valgrind setup
// Last modified: 09/08/2016

// You must have valgrind installed
//
// # yum install valgrind

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

  strcpy(p, (char *)"$ g++ -O0 -Wall -g memleak.cpp\n$ valgrind --tool=memcheck --leak-check=yes ./a.out\n");
  std::cout << p << "\n" << std::flush;

  std::cout << "Causing a memory leak" << "\n" << std::flush;
  p = 0;

  return 0;
}
