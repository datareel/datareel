// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: logfile.cpp
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

Log file demo. Several threads write time-stamped lines to one
LogFile at the same time; each line is written whole.

Usage: logfile [file_name]
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include "logfile.h"

using namespace datareel;

int main(int argc, char **argv)
{
  std::string fname = argc > 1 ? argv[1] : "testlog.log";
  LogFile log;
  if(!log.OverWrite(fname)) {
    std::cerr << "Cannot create " << fname << "\n";
    return 1;
  }
  log.WriteLine("Log started");

  std::vector<std::thread> threads;
  for(int t = 1; t <= 4; t++) {
    threads.emplace_back([&log, t] {
      for(int i = 1; i <= 5; i++) {
	log.WriteLine("thread " + std::to_string(t) + " message " + 
		      std::to_string(i));
      }
    });
  }
  for(auto &th : threads) th.join();

  log << "A line built with operator<<: " << 42 << "\n";
  log.WriteLine("Log finished");
  log.Close();
  std::cout << "Wrote 23 lines to " << fname << "\n";
  return 0;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
