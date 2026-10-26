// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Header File Name: common_opts.h
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 10/26/2026
// Date Last Modified: 10/26/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- // 
/*
Command line options shared by the TLS client examples:
  --starttls     Start plain and upgrade (SMTP 587, POP3 110, FTP 21)
  --ca file      Trust the CA certificates in file
  --insecure     Do not check the server certificate (tests only)
*/
// ----------------------------------------------------------- // 
#ifndef __EXAMPLE_COMMON_OPTS_HPP__
#define __EXAMPLE_COMMON_OPTS_HPP__

#include <cstring>
#include <string>
#include <vector>

struct TLSOptions
{
  bool starttls = false;
  bool insecure = false;
  std::string ca_file;
  std::vector<std::string> args; // Everything else, in order

  void Parse(int argc, char **argv) {
    for(int i = 1; i < argc; i++) {
      if(strcmp(argv[i], "--starttls") == 0) starttls = true;
      else if(strcmp(argv[i], "--insecure") == 0) insecure = true;
      else if(strcmp(argv[i], "--ca") == 0 && i + 1 < argc) ca_file = argv[++i];
      else args.push_back(argv[i]);
    }
  }

  template<class Client>
  void Apply(Client &c) const {
    if(!ca_file.empty()) c.SetCAFile(ca_file);
    if(insecure) c.SetVerifyPeer(false);
    c.SetTimeout(30);
  }
};

#endif // __EXAMPLE_COMMON_OPTS_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
