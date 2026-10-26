// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: https_get.cpp
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

HTTPS client. Fetches a URL and prints the status, headers and
body, or saves the body to a file. Only https:// URLs are accepted.

Usage: https_get url [output_file] [--ca ca.pem] [--insecure] [--head]
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <cstring>
#include <iostream>
#include "gxshttpc.h"

int main(int argc, char **argv)
{
  if(argc < 2) {
    std::cerr << "Usage: " << argv[0] 
	      << " url [output_file] [--ca ca.pem] [--insecure] [--head]\n";
    return 1;
  }
  std::string url = argv[1], out_file;
  bool head = false;
  gxsHTTPClient client;
  for(int i = 2; i < argc; i++) {
    if(strcmp(argv[i], "--ca") == 0 && i + 1 < argc) client.SetCAFile(argv[++i]);
    else if(strcmp(argv[i], "--insecure") == 0) client.SetVerifyPeer(false);
    else if(strcmp(argv[i], "--head") == 0) head = true;
    else out_file = argv[i];
  }
  client.SetTimeout(30);

  gxsHTTPMessage resp;
  int rv;
  if(!out_file.empty()) rv = client.Download(url, out_file, resp);
  else if(head) rv = client.Head(url, resp);
  else rv = client.Get(url, resp);
  if(rv != 0 && resp.status == 0) {
    std::cerr << client.Error() << "\n";
    return 1;
  }

  std::cout << resp.version << " " << resp.status << " " << resp.reason << "\n";
  for(const auto &h : resp.headers) std::cout << h.first << ": " << h.second << "\n";
  std::cout << "\n";
  if(out_file.empty()) std::cout << resp.body;
  else std::cout << "Saved " << resp.body.size() << " bytes to " << out_file << "\n";
  if(rv != 0) std::cerr << client.Error() << "\n";
  return (resp.status >= 200 && resp.status < 300) ? 0 : 2;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
