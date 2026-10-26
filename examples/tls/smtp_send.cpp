// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: smtp_send.cpp
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

Send a plain text email over TLS. The message body is read from
standard input. Implicit TLS is used unless --starttls is given.

Usage: smtp_send host port user password from to subject
       [--starttls] [--ca ca.pem] [--insecure]
Use - for user to skip the login.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <cstdlib>
#include <iostream>
#include <iterator>
#include "gxsmtp.h"
#include "common_opts.h"

int main(int argc, char **argv)
{
  TLSOptions opt;
  opt.Parse(argc, argv);
  if(opt.args.size() < 7) {
    std::cerr << "Usage: " << argv[0] << " host port user password from to "
	      << "subject [--starttls] [--ca ca.pem] [--insecure]\n";
    return 1;
  }
  const std::vector<std::string> &a = opt.args;
  std::string body((std::istreambuf_iterator<char>(std::cin)),
		   std::istreambuf_iterator<char>());

  gxsSMTPClient smtp;
  opt.Apply(smtp);
  if(smtp.Connect(a[0], std::atoi(a[1].c_str()), 
		  opt.starttls ? gxsMAIL_STARTTLS : gxsMAIL_IMPLICIT_TLS) != 0) {
    std::cerr << smtp.Error() << "\n";
    return 1;
  }
  if(a[2] != "-" && smtp.Login(a[2], a[3]) != 0) {
    std::cerr << smtp.Error() << "\n";
    return 1;
  }
  if(smtp.SendMessage(a[4], { a[5] }, a[6], body) != 0) {
    std::cerr << smtp.Error() << "\n";
    return 1;
  }
  smtp.Quit();
  std::cout << "Message sent" << "\n";
  return 0;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
