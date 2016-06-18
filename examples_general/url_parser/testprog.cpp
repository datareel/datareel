// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: testprog.cpp
// Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 01/25/2000
// Date Last Modified: 06/17/2016
// Copyright (c) 2001-2016 DataReel Software Development
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

This program is used to test gxsURL classes. 
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include "gxsurl.h"
#include "gxs_ver.h"

#ifdef __MSVC_DEBUG__
#include "leaktest.h"
#endif

// Version number and program name
const double ProgramVersionNumber = gxSocketVersion;
const char *ProgramName = "gxsURL test program";

// Author information
const char *ProducedBy = "DataReel Software Development";
const char *EmailAddress = "https://github.com/datareel";

// Program globals
int parsing_url = 1;

void PrintURLInfo(gxsURLInfo &u)
{
  cout << "Uniform resource locator information" << "\n";
  cout << "-------------------------------------------------------" << "\n";
  cout << "Unchanged URL = " << u.url << "\n"; 
  cout << "URL protocol = " << u.proto << "\n"; 
  cout << "Extracted hostname = " << u.host << "\n";
  cout << "Port number = " << u.port << "\n";
  cout << "FTP type = " << u.ftp_type << "\n";
  cout << "Path = " << u.path << "\n";
  cout << "Directory = " << u.dir << "\n";
  cout << "File = " << u.file << "\n";
  cout << "Username = " << u.user << "\n";
  cout << "Password = " << u.passwd << "\n"; 
  cout << "Local filename of the URL document = " << u.local << "\n";
  cout << "Source that requested URI was obtained = " << u.referer << "\n";

  if(u.is_dynamic) {
    cout << "\n";
    cout << "Dyanmic page information" << "\n";
    cout << "-------------------------------------------------------" << "\n";
    cout << "Query string = " << u.query_string.c_str() << "\n";
    if(!u.dynamic_bin_dir.is_null()) {
      cout << "Binary dir = " << u.dynamic_bin_dir.c_str() << "\n";
    }
    if(!u.dynamic_file_extension.is_null()) {
      cout << "File extension = " << u.dynamic_file_extension.c_str() << "\n";
    }
    cout << "Local query file = " << u.local_query_file.c_str() << "\n";
  }

  if(u.proxy) { // The exact string to pass to proxy server
    cout << "\n";
    cout << "Proxy server information" << "\n";
    cout << "-------------------------------------------------------" << "\n";
    cout << "Unchanged URL = " << u.proxy->url << "\n"; 
    cout << "URL protocol = " << u.proxy->proto << "\n"; 
    cout << "Extracted hostname = " << u.proxy->host << "\n";
    cout << "Port number = " << u.proxy->port << "\n";
    cout << "FTP type = " << u.proxy->ftp_type << "\n";
    cout << "Path = " << u.proxy->path << "\n";
    cout << "Directory = " << u.proxy->dir << "\n";
    cout << "File = " << u.proxy->file << "\n";
    cout << "Username = " << u.proxy->user << "\n";
    cout << "Password = " << u.proxy->passwd << "\n"; 
    cout << "Local filename of the URL document = " << u.proxy->local << "\n";
    cout << "Source that requested URI was obtained = "
	 << u.proxy->referer << "\n";
  }
  cout << "\n";
}

void ListSupportedProtocols()
{
  gxsURL url;
  for(int i = 0; i < gxsMAX_PROTOCOL_STRINGS; i++) {
    cout << url.GetProtocolString(i) << "//" << " Port Number - ";
    gxString u(url.GetProtocolString(i)); u += "//";
    int port; url.GetPortNumber(u, port);
    cout << port << "\n";
  }
}

void ParseURL(const char *URL)
{
  gxsURLInfo u;
  gxsURL gxsurl;
  gxString url_buf = URL;
  gxsurl.SetDefaultProto(url_buf);
  
  if(!gxsurl.ParseURL(url_buf, u)) {
    cout << "Error parsing URL" << "\n";
    return;
  }
			       
  PrintURLInfo(u);  
}

void HelpMessage(const char *program_name, const double version_number)
{
  cout << "\n";
  cout.setf(ios::showpoint | ios::fixed);
  cout.precision(3);
  cout << "\n";
  cout << program_name << " version " << version_number  << "\n";
  cout << "Produced by: " << ProducedBy << ' ' << EmailAddress << "\n";
  cout << "\n";
  cout << "Usage: " << program_name << " [switches] URL" << "\n";
  cout << "Example: " << program_name << " -p http://www.xyz.com/index.html" << "\n";
  cout << "Switches: " << "\n";
  cout << "          -L = List all the protocols supported by this program." 
       << "\n";
  cout << "          -p = Parse the specified URL (default)" << "\n";
  cout << "\n";
  return;
}

int ProcessArgs(char *arg)
{
  switch(arg[1]) {
    case 'L' :
      parsing_url = 0;
      ListSupportedProtocols();
      return 0;

    case 'p' :
      parsing_url = 1;
      break;

    default:
      cerr << "\n";
      cerr << "Unknown switch " << arg << "\n";
      cerr << "Exiting..." << "\n";
      cerr << "\n";
      return 0;
  }
  arg[0] = '\0';
  return 1; // All command line arguments were valid
}

int main(int argc, char **argv)
{
#ifdef __MSVC_DEBUG__
  InitLeakTest();
#endif

  if(argc < 2) { 
    HelpMessage(ProgramName, ProgramVersionNumber);
    return 0;
  }
  
  // Process command ling arguments and files 
  int narg;
  char *arg = argv[narg = 1];

  while (narg < argc) {
    if(arg[0] != '\0') {
      if(arg[0] == '-') { // Look for command line arguments
	if(!ProcessArgs(arg)) return 0; // Exit if argument is not valid
      }
      else { 
	if(parsing_url) {
	  ParseURL((const char *)arg);
	}
      }
      arg = argv[++narg];
    }
  }

  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
