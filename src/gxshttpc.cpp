// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxshttp.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 01/25/2000
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

HTTP client class.

Changes:
==============================================================
09/07/2001: Fixed the gxsHTTPClient::RequestHeader() function
to remove all characters after the end of the header marker.

09/27/2001: Added new gxsHTTPClient::Request() functions to 
replace the slower gxsHTTPClient::RequestFile() functions. The 
gxsHTTPClient::RequestFile() functions have been depreciated 
in version 4.11 and retained for backward compatibility.

01/24/2002: Added (const char *) and (unsigned) type casts
to all FindMatch() function calls to correct type errors
encountered during Linux library builds

04/05/2002: Removed the gxDEVICE_CONSOLE case from the 
gxsHTTPClient::Write() function. Writing to the console is
only used for test purposes.

04/08/2002: Changed the gxsHTTPClient() constructor to use
the version number set in the gxs_ver.h header file to set 
the HTTP client version number.

05/05/2002: Added the __GUI_THREAD_SAFE__ preprocessor directive 
used to conditionally omit functions calls that are not safe to
use within multithreaded GUI applications.

10/03/2002: Modified the gxsHTTPClient::RequestHeader() function
to reset the HTTP header before parsing it.

10/03/2002: Modified the gxsHTTPClient::SimpleRequest(), 
gxsHTTPClient::RequestHeader(), and gxsHTTPClient::Request() functions 
to use timeouts and external loop control.

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
#include "gxdlcode.h"

#include <fstream>
#include "gxshttpc.h"
#include "gxs_b64.h"
#include "strutil.h"

using namespace datareel;

int gxsHTTPClient::Get(const std::string &url, gxsHTTPMessage &response)
{
  return Request("GET", url, std::string(), std::string(), response);
}

int gxsHTTPClient::Head(const std::string &url, gxsHTTPMessage &response)
{
  return Request("HEAD", url, std::string(), std::string(), response);
}

int gxsHTTPClient::Post(const std::string &url, const std::string &body,
			const std::string &content_type, 
			gxsHTTPMessage &response)
{
  return Request("POST", url, body, content_type, response);
}

int gxsHTTPClient::send_one(const std::string &method, const gxsURLInfo &u,
			    const std::string &body, 
			    const std::string &content_type,
			    gxsHTTPMessage &response)
{
  gxsTLSConnection conn;
  conn.SetTimeout(timeout_sec);
  conn.SetVerifyPeer(verify_peer);
  if(!ca_file.empty()) conn.SetCAFile(ca_file);
  if(conn.Connect(u.host, u.port, true) != 0) return fail(conn.Error());

  gxsHTTPMessage req;
  req.method = method;
  req.target = u.PathAndQuery();
  req.SetHeader("Host", u.HostAndPort());
  req.SetHeader("User-Agent", user_agent);
  req.SetHeader("Accept", "*/*");
  req.SetHeader("Connection", "close");
  std::string user = !u.user.empty() ? u.user : auth_user;
  std::string pass = !u.user.empty() ? u.passwd : auth_passwd;
  // Credentials are not sent to another host after a redirect
  if(!auth_host.empty() && u.host != auth_host) user.clear();
  if(!user.empty()) {
    req.SetHeader("Authorization", gxsBasicAuthenticationEncode(user, pass));
  }
  for(const auto &h : extra_headers) req.SetHeader(h.first, h.second);
  if(!body.empty() || method == "POST" || method == "PUT") {
    if(!content_type.empty()) req.SetHeader("Content-Type", content_type);
    req.body = body;
  }
  if(gxsHTTPWriteRequest(conn, req) != 0) {
    return fail(conn.Error().empty() ? "HTTP exception: Bad request header" : 
		conn.Error());
  }
  if(gxsHTTPReadResponse(conn, response, method == "HEAD", max_body) != 0) {
    return fail("HTTP exception: Bad or incomplete response from " + u.host +
		(conn.Error().empty() ? std::string() : ": " + conn.Error()));
  }
  return 0;
}

int gxsHTTPClient::Request(const std::string &method, const std::string &url,
			   const std::string &body, 
			   const std::string &content_type,
			   gxsHTTPMessage &response)
{
  error.clear();
  std::string current = url;
  std::string current_method = method;
  std::string current_body = body;
  auth_host.clear();
  for(int redirects = 0; ; redirects++) {
    gxsURLInfo u;
    if(!gxsURL::Parse(current, u)) return fail("HTTP exception: Bad URL " + current);
    if(u.scheme != "https") {
      return fail("HTTP exception: Only https:// URLs are supported: " + current);
    }
    final_url = current;
    if(auth_host.empty()) auth_host = u.host;
    if(send_one(current_method, u, current_body, content_type, response) != 0) {
      return -1;
    }
    int s = response.status;
    bool redirect = (s == 301 || s == 302 || s == 303 || s == 307 || s == 308);
    std::string location = response.Header("Location");
    if(!redirect || location.empty() || max_redirects <= 0) return 0;
    if(redirects >= max_redirects) return fail("HTTP exception: Too many redirects");
    current = gxsURL::Resolve(u, location);
    if(s == 303 || ((s == 301 || s == 302) && current_method == "POST")) {
      current_method = "GET"; // Browsers change these to GET
      current_body.clear();
    }
  }
}

int gxsHTTPClient::Download(const std::string &url, const std::string &fname,
			    gxsHTTPMessage &response)
{
  if(Get(url, response) != 0) return -1;
  if(response.status < 200 || response.status > 299) {
    return fail("HTTP exception: " + std::to_string(response.status) + " " +
		response.reason);
  }
  std::ofstream out(fname, std::ios::out | std::ios::binary | std::ios::trunc);
  if(!out) return fail("HTTP exception: Cannot create " + fname);
  out.write(response.body.data(), (std::streamsize)response.body.size());
  if(!out) return fail("HTTP exception: Cannot write " + fname);
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
