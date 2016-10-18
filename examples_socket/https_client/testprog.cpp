// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: server.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 10/17/2001
// Date Last Modified: 10/17/2016
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

HTTPS client example program using the gxSSL class.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include "gxshttpc.h"
#include "dfileb.h"

#ifdef __MSVC_DEBUG__
#include "leaktest.h"
#endif

// Version number and program name
const double ProgramVersionNumber = 1.1;
const char *ProgramName = "testprog";

// Program globals
int get_doc_header = 0; // True if request a document header only
int get_document = 0;   // True if requesting a document 

void HelpMessage(const char *program_name, const double version_number)
{
  cout << "\n";
  cout.setf(ios::showpoint | ios::fixed);
  cout.precision(1);
  cout << "\n";
  cout << program_name << " version " << version_number  << "\n";
  cout << "Usage: " << program_name << " [switches] URL" << "\n";
  cout << "Example: " << program_name << " -h https://www.yahoo.com"
       << "\n";
  cout << "Switches: " << "\n";
  cout << "          -d = Download the specified file only (default)." << "\n";
  cout << "          -h = Get a document header." << "\n";
  cout << "\n";
}

int ProcessArgs(char *arg)
{
  switch(arg[1]) {
    case 'h':
      get_doc_header = 1;
      get_document = 0;
      break;

    case 'd':
      get_doc_header = 0;
      get_document = 1;
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

void PrintHTTPHeader(const gxsHTTPHeader &hdr)
{
  cout << "\n" << flush;  
  cout << "<------ Document Header ------>" << "\n" << flush;
  cout << hdr.http_header.c_str();
  cout << "<----------------------------->" << "\n" << flush;
  cout << "\n" << flush;
  cout << "Press Enter to continue..." << "\n" << flush;
  cin.get();
  
  cout << "Processing the header information..." << "\n" 
	      << flush;
  cout << gxsHTTPStatusCodeMessage(hdr.http_status) << "\n" 
	      << flush;

  cout.setf(ios::showpoint | ios::fixed);
  cout.precision(1);
  cout << "HTTP version: " << hdr.http_version << "\n" << flush;

  cout << "Document status code: " << hdr.http_status << "\n" 
	      << flush;
  if(hdr.current_server.length() > 0) {
    cout << "Current Server: " << hdr.current_server.c_str() << "\n" 
		<< flush;
  }
  if(hdr.location.length() > 0) {
    cout << "Location: " << hdr.location.c_str() << "\n" 
		<< flush;
  }
  if(hdr.http_last_modified.length() > 0) {
    cout << "Date Document Last Modified: " 
		<< hdr.http_last_modified.c_str() << "\n" << flush;
  }
  if(hdr.date.length() > 0) {
    cout << "Date: " << hdr.date.c_str() << "\n" << flush;
  }
  if(hdr.http_expires.length() > 0) {
    cout << "Expires: " << hdr.http_expires.c_str() << "\n" 
		<< flush;
  }
  if(hdr.etag.length() > 0) {
    cout << "Entity tag: " << hdr.etag.c_str() << "\n" << flush;
  }
  if(hdr.authentication_needed) {
    cout << "Authentication required" << "\n" << flush;
  }
  if(hdr.authentication_scheme.length() > 0) {
    cout << "Authentication scheme: " 
		<< hdr.authentication_scheme.c_str() << "\n" << flush;
  }
  if(hdr.realm.length() > 0) {
    cout << "Authentication realm: " << hdr.realm.c_str() << "\n" 
		<< flush;
  }
  if(hdr.auth_cookie.length() > 0) {
    cout << "Authentication cookie: " << hdr.auth_cookie.c_str() 
		<< "\n" << flush;
  }
  if(hdr.content_encoding.length() > 0) {
    cout << "Content encoding: " << hdr.content_encoding.c_str() 
		<< "\n" << flush;
  }
  if(hdr.pragma.length() > 0) {
    cout << "Pragma: " << hdr.pragma.c_str() << "\n" << flush;
  }
  if(hdr.cache_control.length() > 0) {
    cout << "Cache control: " << hdr.cache_control.c_str() << "\n" 
		<< flush;
  }
  
  if(hdr.file_extension.length() > 0) {
    cout << "File extension: " << hdr.file_extension.c_str() << "\n" 
		<< flush;
  }
  if(hdr.length > -1) {
    cout << "Document length: " << hdr.length << "\n" << flush;
  }
  if(hdr.not_found) {
    cout << "The requested document was not found" << "\n" 
		<< flush;
  }
  if(!hdr.no_cache) {
    cout << "Using cached copy of the requested document" << "\n" 
		<< flush;
  }
  if(hdr.accept_ranges) {
    cout << "Accepting ranges" << "\n" << flush;
  }
  if(hdr.timeout > -1) {
    cout << "Timeout: " << hdr.timeout << "\n" << flush;
  }
  if(hdr.max_conns > -1) {
    cout << "Max connects: " << hdr.max_conns << "\n" << flush;
  }
  if(!hdr.keep_alive) {
    cout << "The server has closed this connection" << "\n" 
		<< flush;
  }

  gxListNode<gxsNetscapeCookie> *netscape_cookies = \
    hdr.netscape_cookies.GetHead();
  if(netscape_cookies) {
    cout << "\n" << flush;
    cout << "Cookie information. Press Enter to continue..." 
		<< "\n" << flush;
    cin.get();

    while(netscape_cookies) {
      gxsNetscapeCookie citem(netscape_cookies->data);

      cout << "Hostname: " << citem.host.c_str() << "\n" 
		  << flush;
      cout << "Name: " << citem.name.c_str() << "\n" << flush;
      cout << "Value: " << citem.value.c_str() << "\n" << flush;
      if(citem.expires.length() > 0) {
	cout << "Expires: " << citem.expires.c_str() << "\n" 
		    << flush;
      }
      if(citem.domain.length() > 0) {
	cout << "Domain: " << citem.domain.c_str() << "\n" 
		    << flush;
      }
      if(citem.path.length() > 0) {
	cout << "Path: " << citem.path.c_str() << "\n" << flush;
      }
      if(citem.secure) {
	cout << "This is a secure cookie" << "\n" << flush;
      }

      netscape_cookies = netscape_cookies->next;
      
      if(netscape_cookies) {
	cout << "\n" << flush;
	cout << "Cookie information. Press Enter to continue..." 
		    << "\n" << flush;
	cin.get();
      }
    }
  }
}

void GetDocumentHeader(char *URL)
{
  gxsURL url;
  gxsURLInfo u;
  gxString web_url(URL);
  
  if(!url.ParseURL(web_url, u)) {
    cout << "Error parsing url" << "\n";
    return;
  }

  gxsHTTPSClient httpc;
  gxsHTTPHeader hdr;

  if(httpc.RequestHeader(u, hdr) != 0) {
    cout << httpc.SocketExceptionMessage() << "\n";
    cout << httpc.openssl.SSLExceptionMessage() << "\n";
    return;
  }

  PrintHTTPHeader(hdr);
}

void DownloadFile(const char *URL)
{
  gxsHTTPSClient client;

  gxsURL url;
  gxsURLInfo u;
  if(!url.ParseURL(URL, u)) {
    cout << "Error parsing URL" << "\n";
  }

  // Construct the local file name based on the URL information
  gxString sbuf(u.dir);
  if(u.file == "?") { // Is this a file or directory
    while(sbuf.DeleteBeforeIncluding("/")) ;
    u.file = sbuf;
    u.dir.DeleteAfterIncluding(sbuf.c_str());
  }
  if(!u.file) {
    u.file = "index.html";
  }

  // Open the local file
  DiskFileB outfile(u.file.c_str(), DiskFileB::df_WRITEONLY,
		    DiskFileB::df_CREATE);
  if(!outfile) {
    cout << outfile.df_ExceptionMessage() << "\n";
    return;
  }
  
  cout << "Connecting to: " << u.host << "\n";
  cout << "Downloading: " << u.path << "\n";

  // client.openssl.SetCertFile("client_cert.pem");
  // client.openssl.SetKeyFile("client_key.pem");
  // client.openssl.SetPasswd("keypassphrase");
  // client.openssl.SetRandomFile("random_number_file");

  // Cert verification, requires a trusted CA list 
  // client.openssl.SetCAList("/etc/pki/tls/certs/ca-bundle.crt");
  // client.openssl.RequireVerifyCert();

  gxsHTTPHeader hdr;
  if(client.Request(u, hdr, outfile.df_FileStream()) != 0) {
    cout << client.SocketExceptionMessage() << "\n";
    cout << client.openssl.SSLExceptionMessage() << "\n";
    return;
  }

  client.Flush();
}

// Program's main thread of execution.
// ----------------------------------------------------------- 
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
    if (arg[0] != '\0') {
      if (arg[0] == '-') { // Look for command line arguments
	if(!ProcessArgs(arg)) return 0; // Exit if argument is not valid
      }
      else { 
	if(get_document) {
	  cout << "\n";
	  cout << "Downloading: " << arg << "\n";
	  DownloadFile((const char *)arg);
	}
	else if(get_doc_header) {
	  cout << "\n";
	  cout << "Requesting document header: " << arg << "\n";
	  GetDocumentHeader(arg);
	}
	else { // Default if no argument is specifed
	  cout << "\n";
	  cout << "Downloading: " << arg << "\n";
	  DownloadFile((const char *)arg);
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
