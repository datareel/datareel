// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Header File Name: gxshttpc.h
// C++ Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 01/25/2000
// Date Last Modified: 10/26/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- // 
// ---------- Include File Description and Details  ---------- // 
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

HTTP client class.

Changes:
==============================================================
09/27/2001: Added new gxsHTTPClient::Request() functions to 
replace the slower gxsHTTPClient::RequestFile() functions. The 
gxsHTTPClient::RequestFile() functions have been depreciated 
in version 4.11 and retained for backward compatibility.

03/10/2002: The gxsHTTPClient class no longer uses the C++ fstream 
class as the underlying file system. The gxDatabase file pointer 
routines are now used to define the underlying file system used by 
the gxsHTTPClient class. This change was made to support large files 
and NTFS file system enhancements. To enable large file support 
users must define the __64_BIT_DATABASE_ENGINE__ preprocessor 
directive.

10/03/2002: Added gxsHTTPClient::revc_loop, gxsHTTPClient::time_out_sec,
and gxsHTTPClient::time_out_usec data members used to break the receive 
loops and set timeout values.

10/03/2002: Added the following function used to break the receive  
loops and set timeout values: gxsHTTPClient::SetTimeOut(),
gxsHTTPClient::TerminateRevc(), gxsHTTPClient::ResetRecv() , 
gxsHTTPClient::ResetTimeOut(), and gxsHTTPClient::ResetRecv().

12/19/2005: Added SSL class and functions used to support HTTPS
connections.

10/26/2026: DataReel 7 replaces gxsHTTPClient and gxsHTTPSClient 
with one HTTPS-only client built on gxsTLSConnection. http:// URLs 
and redirects to them are refused. The server certificate and host 
name are checked by default. Responses can be chunked, and bodies 
are returned in a std::string or written to a file.
==============================================================
*/
// ----------------------------------------------------------- // 
#ifndef __GX_HTTP_CLIENT_HPP__
#define __GX_HTTP_CLIENT_HPP__

#include "gxdlcode.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "gxshttp.h"
#include "gxstls.h"
#include "gxsurl.h"

namespace datareel {


// HTTPS client. Each request opens a new TLS connection.
class GXDLCODE_API gxsHTTPClient
{
public:
  gxsHTTPClient() { }

public: // Settings
  void SetTimeout(int seconds) { timeout_sec = seconds; }
  void SetCAFile(const std::string &fname) { ca_file = fname; }
  void SetVerifyPeer(bool on) { verify_peer = on; }
  void SetUserAgent(const std::string &s) { user_agent = s; }
  void SetMaxRedirects(int n) { max_redirects = n; }
  void SetMaxBodySize(std::size_t n) { max_body = n; }
  void SetBasicAuth(const std::string &user, const std::string &passwd) {
    auth_user = user; auth_passwd = passwd;
  }
  // Extra headers sent with every request
  void AddHeader(const std::string &name, const std::string &value) {
    extra_headers.emplace_back(name, value);
  }
  void ClearHeaders() { extra_headers.clear(); }

public: // Requests. Each returns 0 if a response was received.
  int Get(const std::string &url, gxsHTTPMessage &response);
  int Head(const std::string &url, gxsHTTPMessage &response);
  int Post(const std::string &url, const std::string &body,
	   const std::string &content_type, gxsHTTPMessage &response);
  int Request(const std::string &method, const std::string &url,
	      const std::string &body, const std::string &content_type,
	      gxsHTTPMessage &response);

  // GET url and write the body to fname. Returns 0 if the response
  // status was 2xx and the file was written.
  int Download(const std::string &url, const std::string &fname,
	       gxsHTTPMessage &response);

public:
  const std::string &Error() const { return error; }
  const std::string &FinalURL() const { return final_url; }

private:
  int fail(const std::string &msg) { error = msg; return -1; }
  int send_one(const std::string &method, const gxsURLInfo &u,
	       const std::string &body, const std::string &content_type,
	       gxsHTTPMessage &response);

private:
  int timeout_sec = 60;
  bool verify_peer = true;
  int max_redirects = 5;
  std::size_t max_body = 1024*1024*1024;
  std::string ca_file;
  std::string user_agent = "DataReel/7";
  std::string auth_user, auth_passwd;
  std::string auth_host; // Host of the first request in a redirect chain
  std::vector<std::pair<std::string, std::string>> extra_headers;
  std::string error;
  std::string final_url;
};

} // namespace datareel

#endif // __GX_HTTP_CLIENT_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
