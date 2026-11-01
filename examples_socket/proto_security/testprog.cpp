// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: testprog.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 10/31/2026
// Date Last Modified: 10/31/2026
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

6.3 tests for the HTTP, URL, SMTP, POP3 and FTP security fixes
(DataReel 7.3 security sweep findings that apply to 6.x and 6.x-only
parsing code). Network tests use fake servers on 127.0.0.1 run in
a thread of this program. Usage:

  testprog [section ...]

With no section all tests are run. Prints a FAIL line for each
failed check and returns non-zero if any check failed.

Sections:
  httphdr   gxsHTTPClient::ParseHTTPHeader(): long header lines (stack
            overflow), fields matched at the start of a line, strict
            status line and Content-Length, control characters and
            field names that are not tokens.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include "gxshttp.h"
#include "gxshttpc.h"
#include "gxsurl.h"

static int num_failed = 0;
static int num_checks = 0;

static void Check(int ok, const char *section, const char *what)
{
  num_checks++;
  if(!ok) {
    num_failed++;
    printf("FAIL: %s: %s\n", section, what);
  }
}

static int Enabled(int argc, char **argv, const char *section)
{
  if(argc < 2) return 1;
  for(int i = 1; i < argc; i++) {
    if(strcmp(argv[i], section) == 0) return 1;
  }
  return 0;
}

// --------------------------------------------------------------
// httphdr
// --------------------------------------------------------------
static int ParseHeader(const std::string &text, gxsHTTPHeader &hdr)
{
  gxsHTTPClient client;
  gxsURLInfo u;
  u.host = "127.0.0.1";
  gxString header;
  header.Cat(text.c_str(), (unsigned)text.size());
  hdr.Reset();
  hdr.netscape_cookies.ClearList(); // Reset() keeps the cookie list
  return client.ParseHTTPHeader(u, header, hdr);
}

static void TestHTTPHeader()
{
  const char *sec = "httphdr";
  gxsHTTPHeader hdr;

  // A normal header
  int rv = ParseHeader("HTTP/1.1 200 OK\r\nServer: test\r\n"
		       "Content-Type: text/html; charset=utf-8\r\n"
		       "Content-Length: 1234\r\n"
		       "Location: http://127.0.0.1/x\r\n"
		       "Set-Cookie: a=b; Path=/; Secure\r\n"
		       "Connection: close\r\n\r\n", hdr);
  Check(rv == 0 && hdr.http_status == 200, sec, "normal header status");
  Check(hdr.length == 1234, sec, "normal header Content-Length");
  Check(hdr.current_server == "test", sec, "normal header Server");
  Check(hdr.mime_type == "text/html", sec, "normal header Content-Type");
  Check(hdr.file_extension == ".html", sec, "normal header file extension");
  Check(hdr.location == "http://127.0.0.1/x", sec, "normal header Location");
  Check(hdr.keep_alive == 0, sec, "normal header Connection: close");
  Check(hdr.netscape_cookies.GetHead() != 0 &&
	hdr.netscape_cookies.GetHead()->data.name == "a" &&
	hdr.netscape_cookies.GetHead()->data.value == "b" &&
	hdr.netscape_cookies.GetHead()->data.secure == 1, sec,
	"normal header Set-Cookie");

  // Long header lines overflowed 1024 and 256 byte stack buffers
  std::string longval(20000, 'A');
  rv = ParseHeader("HTTP/1.1 200 OK\r\nServer: " + longval + "\r\n"
		   "Location: " + longval + "\r\n"
		   "WWW-Authenticate: " + longval + " " + longval + "\r\n"
		   "Accept-Ranges: " + longval + "\r\n"
		   "Connection: " + longval + "," + longval + "\r\n"
		   "Content-Type: text/" + longval + "\r\n"
		   "Set-Cookie: " + longval + "=" + longval + "; " + longval +
		   "\r\n\r\n", hdr);
  Check(rv == 0 && hdr.current_server.length() == 20000, sec,
	"long Server line kept whole");
  Check(hdr.location.length() == 20000, sec, "long Location line kept whole");
  rv = ParseHeader("HTTP/1.1 200 " + longval + "\r\n\r\n", hdr);
  Check(rv == 0 && hdr.http_status == 200, sec, "long reason phrase");

  // Fields are matched at the start of a line only
  rv = ParseHeader("HTTP/1.1 200 OK\r\nX-Content-Length: 99\r\n"
		   "X-Location: http://evil/\r\n"
		   "X-Note: Set-Cookie: c=d\r\n\r\n", hdr);
  Check(rv == 0 && hdr.length == -1, sec, "X-Content-Length ignored");
  Check(hdr.location.is_null(), sec, "X-Location ignored");
  Check(hdr.netscape_cookies.GetHead() == 0, sec,
	"Set-Cookie inside a value ignored");

  // Strict status line
  rv = ParseHeader("HTTP/1.1 2000 OK\r\n\r\n", hdr);
  Check(rv != 0 && hdr.http_status == gxsHTTP_STATUS_UNKNOWN, sec,
	"4 digit status refused");
  rv = ParseHeader("HTTP/1.1 +200 OK\r\n\r\n", hdr);
  Check(rv != 0 && hdr.http_status == gxsHTTP_STATUS_UNKNOWN, sec,
	"signed status refused");
  rv = ParseHeader("garbage\r\n\r\n", hdr);
  Check(rv != 0 && hdr.http_status == gxsHTTP_STATUS_UNKNOWN, sec,
	"no status line refused (was left at 200)");
  rv = ParseHeader("HTTP/1.0 404\r\n\r\n", hdr);
  Check(rv == 0 && hdr.http_status == 404 && hdr.not_found, sec,
	"status with no reason phrase");

  // Strict Content-Length
  rv = ParseHeader("HTTP/1.1 200 OK\r\nContent-Length: -5\r\n\r\n", hdr);
  Check(rv == 0 && hdr.length == -1, sec, "negative Content-Length ignored");
  rv = ParseHeader("HTTP/1.1 200 OK\r\nContent-Length: 99999999999\r\n\r\n",
		   hdr);
  Check(rv == 0 && hdr.length == -1, sec, "Content-Length over int ignored");
  rv = ParseHeader("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n"
		   "Content-Length: 6\r\n\r\n", hdr);
  Check(rv == 0 && hdr.length == -1, sec, "conflicting Content-Length");

  // Control characters and field names that are not tokens
  std::string nul("HTTP/1.1 200 OK\r\nServer: a");
  nul += '\0';
  nul += "b\r\n\r\n";
  rv = ParseHeader(nul, hdr);
  Check(rv != 0 && hdr.http_status == gxsHTTP_STATUS_UNKNOWN, sec,
	"NUL in a header refused");
  rv = ParseHeader("HTTP/1.1 200 OK\r\nServer: a\rb\r\n\r\n", hdr);
  Check(rv != 0, sec, "lone CR in a header refused");
  rv = ParseHeader("HTTP/1.1 200 OK\r\nContent-Length : 5\r\n\r\n", hdr);
  Check(rv != 0, sec, "space before the colon refused");

  // A path in the Content-Type subtype is not used as a file extension
  rv = ParseHeader("HTTP/1.1 200 OK\r\nContent-Type: text/../../x\r\n\r\n",
		   hdr);
  Check(rv == 0 && hdr.file_extension.Find("/") == -1, sec,
	"no path in the file extension");
}

int main(int argc, char **argv)
{
  if(Enabled(argc, argv, "httphdr")) TestHTTPHeader();

  printf("%d checks, %d failed\n", num_checks, num_failed);
  if(num_failed) {
    printf("FAIL\n");
    return 1;
  }
  printf("PASS\n");
  return 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
