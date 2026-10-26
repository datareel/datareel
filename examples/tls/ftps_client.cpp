// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: ftps_client.cpp
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

FTPS client. Lists a directory, downloads or uploads a file.
Explicit TLS (AUTH TLS on port 21) is the default; --implicit uses
implicit TLS (port 990). Data connections are always encrypted.

Usage: ftps_client host port user password list [dir]
       ftps_client host port user password get remote local
       ftps_client host port user password put local remote
       [--implicit] [--ca ca.pem] [--insecure]
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include "gxsftp.h"
#include "common_opts.h"

int main(int argc, char **argv)
{
  bool implicit = false;
  std::vector<char *> rest;
  for(int i = 0; i < argc; i++) {
    if(strcmp(argv[i], "--implicit") == 0) implicit = true;
    else rest.push_back(argv[i]);
  }
  TLSOptions opt;
  opt.Parse((int)rest.size(), rest.data());
  const std::vector<std::string> &a = opt.args;
  if(a.size() < 5) {
    std::cerr << "Usage: " << argv[0] << " host port user password "
	      << "list [dir] | get remote local | put local remote "
	      << "[--implicit] [--ca ca.pem] [--insecure]\n";
    return 1;
  }

  gxsFTPClient ftp;
  opt.Apply(ftp);
  if(ftp.Connect(a[0], std::atoi(a[1].c_str()), 
		 implicit ? gxsMAIL_IMPLICIT_TLS : gxsMAIL_STARTTLS) != 0 ||
     ftp.Login(a[2], a[3]) != 0) {
    std::cerr << ftp.Error() << "\n";
    return 1;
  }

  int rv = 0;
  long long bytes = 0;
  if(a[4] == "list") {
    std::string listing;
    rv = ftp.List(a.size() > 5 ? a[5] : std::string(), listing);
    if(rv == 0) std::cout << listing;
  }
  else if(a[4] == "get" && a.size() > 6) {
    rv = ftp.Get(a[5], a[6], bytes);
    if(rv == 0) std::cout << "Downloaded " << bytes << " bytes\n";
  }
  else if(a[4] == "put" && a.size() > 6) {
    rv = ftp.Put(a[5], a[6], bytes);
    if(rv == 0) std::cout << "Uploaded " << bytes << " bytes\n";
  }
  else {
    std::cerr << "Unknown command: " << a[4] << "\n";
    rv = -1;
  }
  if(rv != 0 && !ftp.Error().empty()) std::cerr << ftp.Error() << "\n";
  ftp.Quit();
  return rv == 0 ? 0 : 1;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
