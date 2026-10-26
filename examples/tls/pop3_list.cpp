// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: pop3_list.cpp
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

List the messages in a POP3 mailbox over TLS, and print one with
-r number. Implicit TLS (port 995) is used unless --starttls is
given (port 110 with STLS).

Usage: pop3_list host port user password [-r number]
       [--starttls] [--ca ca.pem] [--insecure]
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <cstdlib>
#include <iostream>
#include "gxspop3.h"
#include "common_opts.h"

int main(int argc, char **argv)
{
  TLSOptions opt;
  opt.Parse(argc, argv);
  if(opt.args.size() < 4) {
    std::cerr << "Usage: " << argv[0] << " host port user password "
	      << "[-r number] [--starttls] [--ca ca.pem] [--insecure]\n";
    return 1;
  }
  const std::vector<std::string> &a = opt.args;
  int retr = 0;
  if(a.size() >= 6 && a[4] == "-r") retr = std::atoi(a[5].c_str());

  gxsPOP3Client pop3;
  opt.Apply(pop3);
  if(pop3.Connect(a[0], std::atoi(a[1].c_str()),
		  opt.starttls ? gxsMAIL_STARTTLS : gxsMAIL_IMPLICIT_TLS) != 0 ||
     pop3.Login(a[2], a[3]) != 0) {
    std::cerr << pop3.Error() << "\n";
    return 1;
  }
  int count = 0;
  long long bytes = 0;
  std::string text;
  if(pop3.Stat(count, bytes) != 0 || pop3.List(text) != 0) {
    std::cerr << pop3.Error() << "\n";
    return 1;
  }
  std::cout << count << " messages, " << bytes << " bytes\n" << text;
  if(retr > 0) {
    if(pop3.Retr(retr, text) != 0) {
      std::cerr << pop3.Error() << "\n";
      return 1;
    }
    std::cout << "\n--- Message " << retr << " ---\n" << text;
  }
  pop3.Quit();
  return 0;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
