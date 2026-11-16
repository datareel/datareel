// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: https_check.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/16/2026
// Date Last Modified: 11/16/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- //
// ------------- Program Description and Details ------------- //
// ----------------------------------------------------------- //
/*
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library. If not, see
<https://www.gnu.org/licenses/>.

Check a list of web sites with gxsHTTPClient (gxshttpc.h), based on
the 2007 http_ping and http_header_util programs: each URL is
requested over TLS and one line gives its status, the time it took,
the body size and, after a redirect, where it ended. --headers
also prints the response headers, to help troubleshoot a site.

Usage: https_check [--head] [--headers] [--count N] [--list file]
       [--ca ca.pem] [--insecure] url ...

  --head     HEAD instead of GET (no body)
  --count N  request each URL N times; the line gives the fastest,
             the median and the slowest time
  --list     read more URLs from a file, one a line (# comments)

The exit code is 0 when every URL answered with a status under 400,
1 if one did not (an error, a refused certificate, 4xx or 5xx).
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include "gxshttpc.h"
#include "strutil.h"
#include "common_opts.h"

using namespace datareel;

int main(int argc, char **argv)
{
  TLSOptions opt;
  opt.Parse(argc, argv);
  std::vector<std::string> urls;
  bool head = false, show_headers = false;
  int count = 1;
  for(std::size_t i = 0; i < opt.args.size(); i++) {
    const std::string &s = opt.args[i];
    bool more = i + 1 < opt.args.size();
    if(s == "--head") head = true;
    else if(s == "--headers") show_headers = true;
    else if(s == "--count" && more) count = std::max(1, std::atoi(opt.args[++i].c_str()));
    else if(s == "--list" && more) {
      std::ifstream in(opt.args[++i]);
      if(!in) {
	std::cerr << "Cannot read " << opt.args[i] << "\n";
	return 1;
      }
      std::string line;
      while(std::getline(in, line)) {
	line = trim(line);
	if(!line.empty() && line[0] != '#') urls.push_back(line);
      }
    }
    else urls.push_back(s);
  }
  if(urls.empty()) {
    std::cerr << "Usage: " << argv[0] << " [--head] [--headers] [--count N] "
	      << "[--list file] [--ca ca.pem] [--insecure] url ...\n";
    return 1;
  }

  gxsHTTPClient client;
  opt.Apply(client);
  int failed = 0;
  for(const std::string &url : urls) {
    std::vector<double> ms;
    gxsHTTPMessage response;
    std::string error;
    for(int n = 0; n < count && error.empty(); n++) {
      response = gxsHTTPMessage();
      auto t0 = std::chrono::steady_clock::now();
      int rv = head ? client.Head(url, response) : client.Get(url, response);
      auto t1 = std::chrono::steady_clock::now();
      if(rv != 0) error = client.Error();
      else ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    if(!error.empty()) {
      std::cout << "ERR  " << url << ": " << error << "\n";
      failed++;
      continue;
    }
    std::sort(ms.begin(), ms.end());
    char times[96];
    if(count == 1) std::snprintf(times, sizeof(times), "%.1f ms", ms[0]);
    else std::snprintf(times, sizeof(times), "%.1f/%.1f/%.1f ms",
		       ms.front(), ms[ms.size() / 2], ms.back());
    std::cout << response.status << "  " << times << "  "
	      << response.body.size() << " bytes  " << url;
    if(client.FinalURL() != url) std::cout << " -> " << client.FinalURL();
    std::cout << "\n";
    if(response.status >= 400) failed++;
    if(show_headers) {
      std::cout << "  " << response.version << " " << response.status << " "
		<< response.reason << "\n";
      for(const auto &h : response.headers) {
	std::cout << "  " << h.first << ": " << h.second << "\n";
      }
    }
  }
  return failed ? 1 : 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
