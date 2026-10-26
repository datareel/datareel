// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: thread_pool.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 10/26/2026
// Date Last Modified: 10/26/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- // 
// ------------- Program Description and Details ------------- // 
// ----------------------------------------------------------- // 
/*
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.
 
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  
USA

ThreadPool demo. Submits tasks that return values and one that
throws, then collects the results from the futures.

Usage: thread_pool [threads]
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "thrpool.h"

using namespace datareel;

static long long sum_to(long long n)
{
  long long s = 0;
  for(long long i = 1; i <= n; i++) s += i;
  return s;
}

int main(int argc, char **argv)
{
  std::size_t threads = argc > 1 ? (std::size_t)std::atoi(argv[1]) : 0;
  ThreadPool pool(threads);
  std::cout << "Pool has " << pool.Size() << " threads\n";

  std::vector<std::future<long long>> results;
  for(long long n = 1000; n <= 5000; n += 1000) {
    results.push_back(pool.Submit(sum_to, n));
  }
  auto bad = pool.Submit([]() -> int { throw std::runtime_error("task failed"); });

  int errors = 0;
  long long n = 1000;
  for(auto &r : results) {
    long long v = r.get();
    std::cout << "sum_to(" << n << ") = " << v << "\n";
    if(v != n * (n + 1) / 2) errors++;
    n += 1000;
  }
  try {
    bad.get();
    errors++;
  }
  catch(const std::exception &e) {
    std::cout << "Exception from task: " << e.what() << "\n";
  }
  pool.Wait();
  return errors == 0 ? 0 : 1;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
