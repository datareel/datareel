// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: config_file.cpp
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

Configuration file demo. Writes a small config file with gxConfig,
loads it, reads typed values, changes one value in place and reads
it back.

Usage: config_file [file_name]
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <fstream>
#include <iostream>
#include "gxconfig.h"

using namespace datareel;

int main(int argc, char **argv)
{
  std::string fname = argc > 1 ? argv[1] : "testcfg.cfg";

  gxConfig cfg(fname);
  { // Start with a fresh file
    std::ofstream(fname, std::ios::trunc);
  }
  cfg.WriteCommentLine("DataReel 7 configuration file demo");
  cfg.WriteConfigLine("server_name", "db.example.com");
  cfg.WriteConfigLine("port", "2073");
  cfg.WriteConfigLine("timeout", "30.5");
  cfg.WriteConfigLine("data_dir", "/var/lib/my app/data # Comment after a space");

  if(!cfg.Load()) {
    std::cerr << "Cannot load " << fname << "\n";
    return 1;
  }
  std::cout << "server_name = " << cfg.GetString("server_name") << "\n";
  std::cout << "port        = " << cfg.GetIntValue("port") << "\n";
  std::cout << "timeout     = " << cfg.GetFloatValue("timeout") << "\n";
  std::cout << "data_dir    = " << cfg.GetString("data_dir") << "\n";
  std::cout << "missing     = " << cfg.GetString("missing", "(default)") << "\n";

  std::cout << "Changing port to 4443" << "\n";
  if(!cfg.ChangeConfigValue("port", 4443)) {
    std::cerr << "Cannot change the port value\n";
    return 1;
  }
  std::cout << "port        = " << cfg.GetIntValue("port") << "\n";

  std::cout << "All entries:\n";
  for(const auto &e : cfg.Entries()) {
    std::cout << "  " << e.first << " -> " << e.second << "\n";
  }
  return cfg.GetIntValue("port") == 4443 ? 0 : 1;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
