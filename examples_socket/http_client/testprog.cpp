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

This program is used to test the HTTP, HTML, and URL classes.
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include "gxshttp.h"
#include "gxshttpc.h"
#include "dfileb.h"
#include "gxs_ver.h"

#ifdef __MSVC_DEBUG__
#include "leaktest.h"
#endif

// Version number and program name
const double ProgramVersionNumber = gxSocketVersion;
const char *ProgramName = "http testprog";

// Program globals
int get_doc_header = 0; // True if request a document header only
int get_document = 0;   // True if requesting a document 
int record_proto = 0;   // True to record protocol during session

// Functions
void HelpMessage(const char *program_name, const double version_number);
int ProcessArgs(char *arg);
void PrintHTTPHeader(const gxsHTTPHeader &hdr);
void GetDocumentHeader(char *URL);
void DownloadFile(const char *URL);
void TestMethods();
void TestStatusCodes();

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

void HelpMessage(const char *program_name, const double version_number)
{
  cout << "\n";
  cout.setf(ios::showpoint | ios::fixed);
  cout.precision(3);
  cout << "\n";
  cout << program_name << " version " << version_number  << "\n";
  cout << "Usage: " << program_name << " [switches] URL" << "\n";
  cout << "Example: " << program_name << " -h http://www.xyz.com/index.html"
       << "\n";
  cout << "Switches: " << "\n";
  cout << "          -d = Download the specified file only (default)." << "\n";
  cout << "          -h = Get a document header." << "\n";
  cout << "          -m = Display gxsHTTP methods." << "\n";
  cout << "          -r = Record protocol during session" << "\n";
  cout << "          -s = Display gxsHTTP status codes." << "\n";
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

    case 'm' :
      TestMethods();
      return 0;

    case 'r' :
      record_proto = 1;
      break;

    case 's' :
      TestStatusCodes();
      return 0;
      
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
  gxsHTTPClient httpc;
  gxsHTTPHeader hdr;
  
  if(record_proto) httpc.record_proto = 1;

  if(!url.ParseURL(web_url, u)) {
    cout << "Error parsing url" << "\n";
    return;
  }

  if(httpc.RequestHeader(u, hdr) != 0) {
    cout << httpc.SocketExceptionMessage() << "\n";
    return;
  }


  if(record_proto) {
    cout << "\n";
    cout << "HTTP header exchange recorded during session" << "\n";
    cout << "\n";
    cout << httpc.proto_string.c_str() << "\n" << flush;
    cout << "\n";
  }

  PrintHTTPHeader(hdr);
}

void DownloadFile(const char *URL)
{
  gxsHTTPClient client;
  if(record_proto) client.record_proto = 1;

  gxsURL url;
  gxsURLInfo u;
  if(!url.ParseURL(URL, u)) {
    cout << "Error parsing URL " << URL << "\n";
    return;
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

  // Check for dyanmic Web content before downloading
  if(u.is_dynamic) {
    u.file = u.local_query_file; 
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

  gxsHTTPHeader hdr;
  if(client.Request(u, hdr, outfile.df_FileStream()) != 0) {
    cout << client.SocketExceptionMessage() << "\n";
    return;
  }

  client.Flush();

  if(record_proto) {
    cout << "\n";
    cout << "HTTP header exchange recorded during session" << "\n";
    cout << "\n";
    cout << client.proto_string.c_str() << "\n" << flush;
    cout << "\n";
  }
}

void TestMethods()
{
  cout << "gxsHTTP methods" << "\n";
  cout << "\n";
  cout << gxsHTTPMethodMessage(gxHTTP_UNKNOWN) << "\n";
  cout << gxsHTTPMethodMessage(gxsHTTP_GET) << "\n";
  cout << gxsHTTPMethodMessage(gxsHTTP_HEAD) << "\n";
  cout << gxsHTTPMethodMessage(gxsHTTP_PUT) << "\n";
  cout << gxsHTTPMethodMessage(gxsHTTP_DELETE) << "\n";
  cout << gxsHTTPMethodMessage(gxsHTTP_POST) << "\n";
  cout << gxsHTTPMethodMessage(gxsHTTP_LINK) << "\n";
  cout << gxsHTTPMethodMessage(gxsHTTP_UNLINK) << "\n";
  cout << gxsHTTPMethodMessage(gxsHTTP_TRACE) << "\n";
  cout << gxsHTTPMethodMessage(gxsHTTP_OPTIONS) << "\n";
  cout << gxsHTTPMethodMessage(gxsHTTP_CONNECT) << "\n";
  cout << "\n";
}

void TestStatusCodes()
{
  cout << "gxsHTTP status codes" << "\n";
  cout << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_UNKNOWN) << " 0" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_DISK_ERROR) << " 17" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_NETWORK_ERROR) << " 18" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_CLIENT_ERROR) << " 19" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_CONTINUE) << " 100" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_SWITCHING_PROTOCOLS) << " 101" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_OK) << " 200" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_CREATED) << " 201" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_ACCEPTED) << " 202" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_NON_AUTH_INFO) << " 203" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_NO_CONTENT) << " 204" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_RESET_CONTENT) << " 205" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_PARTIAL_CONTENT) << " 206" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_MULTIPLE_CHOICES) << " 300" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_MOVED_PERMANENTLY) << " 301" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_MOVED_TEMPORARILY) << " 302" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_SEE_OTHER) << " 303" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_NOT_MODIFIED) << " 304" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_USE_PROXY) << " 305" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_306_UNUSED) << " 306" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_TEMPORARY_REDIRECT) << " 307" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_BAD_REQUEST) << " 400" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_UNAUTHORIZED) << " 401" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_PAYMENT_REQUIRED) << " 402" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_FORBIDDEN) << " 403" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_NOT_FOUND) << " 404" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_METHOD_NOT_ALLOWED) << " 405" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_NOT_ACCEPTABLE) << " 406" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_PROXY_AUTH_REQUIRED) << " 407" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_REQUEST_TIMEOUT) << " 408" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_CONFLICT) << " 409" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_GONE) << " 410" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_LENGTH_REQUIRED) << " 411" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_PRECONDITION_FAILED) << " 412" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_REQUEST_ENTITY_TOO_LARGE) << " 413" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_REQUEST_URI_TOO_LONG) << " 414" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_UNSUPPORTED_MEDIA_TYPE) << " 415" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_REQUESTED_RANGE_NOT_SATISFIABLE) << " 416" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_EXPECTATION_FAILED) << " 417" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_INTERNAL) << " 500" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_NOT_IMPLEMENTED) << " 501" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_BAD_GATEWAY) << " 502" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_UNAVAILABLE) << " 503" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_GATEWAY_TIMEOUT) << " 504" << "\n";
  cout << gxsHTTPStatusCodeMessage(gxsHTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED) << " 505" << "\n";
  cout << "\n";
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
