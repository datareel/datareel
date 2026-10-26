// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: ping.cpp
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

Ping a host with ICMP or ICMPv6 echo requests.

Usage: ping host [count] [-4|-6]
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#include "gxsping.h"

int main(int argc, char **argv)
{
  if(argc < 2) {
    std::cerr << "Usage: " << argv[0] << " host [count] [-4|-6]\n";
    return 1;
  }
  int count = 4, family = AF_UNSPEC;
  for(int i = 2; i < argc; i++) {
    if(strcmp(argv[i], "-4") == 0) family = AF_INET;
    else if(strcmp(argv[i], "-6") == 0) family = AF_INET6;
    else count = std::atoi(argv[i]);
  }
  gxsPing ping;
  ping.SetTimeout(2000);
  int received = 0;
  for(int i = 0; i < count; i++) {
    gxsPingResult r;
    if(ping.Ping(argv[1], r, family) == 0) {
      received++;
      std::cout << "Reply from " << r.address << ": time=" << r.rtt_ms << " ms";
      if(r.ttl >= 0) std::cout << " ttl=" << r.ttl;
      std::cout << "\n";
    }
    else {
      std::cout << r.error << "\n";
    }
    if(i + 1 < count) std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  std::cout << count << " sent, " << received << " received\n";
  return received > 0 ? 0 : 1;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
