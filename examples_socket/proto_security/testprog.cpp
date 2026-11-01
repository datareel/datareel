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
  httpc     gxsHTTPClient requests: a header that spans packets, an
            endless header, the MemoryBuffer size limit, control
            characters in the URL and a long URL in
            BuildSimpleRequestString(), gxsHTTPMethodMessage() bounds.
  url       gxsURL: protocol table bounds, GetProtocolType(), strict
            ports, control characters, DecodeStrict(), % decoding of
            dynamic page names.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include "gxsocket.h"
#include "gxshttp.h"
#include "gxshttpc.h"
#include "gxsurl.h"
#include "membuf.h"

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
// Fake server on 127.0.0.1 run in a thread. It accepts one
// connection and runs a script of steps.
// --------------------------------------------------------------
enum {
  FS_SEND,      // Send data
  FS_SLEEP,     // Pause (data = milliseconds)
  FS_READLINE,  // Read up to and including LF
  FS_READHDR,   // Read up to and including an empty line
  FS_DATA_LISTEN, // Open a data listener; "%PORT%" in later sends is
                  // replaced by its port (FTP PASV)
  FS_DATA_SEND  // Accept on the data listener, send data, close
};

struct FSStep {
  int type;
  std::string data;
};

class FakeServer
{
public:
  FakeServer() : port(0), data_port(0), accepted(0), worker(0) { }
  ~FakeServer() { Join(); }

  int Start() { // Returns the port number or -1
    if(listener.InitSocketLibrary() != 0) return -1;
    if(listener.InitSocket(SOCK_STREAM, 0, (char *)"127.0.0.1") < 0) return -1;
    if(listener.Bind() < 0) return -1;
    if(listener.Listen(1) < 0) return -1;
    if(listener.GetSockName() < 0) return -1;
    port = (int)ntohs(listener.sin.sin_port);
    worker = new std::thread(Run, this);
    return port;
  }
  void Send(const std::string &s) { Add(FS_SEND, s); }
  void Sleep(int ms) { char b[32]; sprintf(b, "%d", ms); Add(FS_SLEEP, b); }
  void ReadLine() { Add(FS_READLINE, ""); }
  void ReadHeader() { Add(FS_READHDR, ""); }
  void Reply(const std::string &s) { ReadLine(); Send(s); }
  void DataListen() { Add(FS_DATA_LISTEN, ""); }
  void DataSend(const std::string &s) { Add(FS_DATA_SEND, s); }
  void Add(int type, const std::string &s) {
    FSStep st; st.type = type; st.data = s; steps.push_back(st);
  }
  void Join() {
    if(worker) { worker->join(); delete worker; worker = 0; }
  }

public:
  int port;
  int data_port;
  int accepted;
  std::string received; // Everything read from the client

private:
  static int WaitRead(gxSocket &s, gxsSocket_t fd) {
    return s.ReadSelect(fd, 5, 0);
  }
  static void Run(FakeServer *fs) { fs->RunSteps(); }
  void RunSteps() {
    if(!WaitRead(listener, listener.GetSocket())) { listener.Close(); return; }
    if(listener.Accept() < 0) { listener.Close(); return; }
    accepted = 1;
    gxsSocket_t c = listener.GetRemoteSocket();
    for(size_t i = 0; i < steps.size(); i++) {
      FSStep &st = steps[i];
      if(st.type == FS_SEND) {
	std::string d = st.data;
	size_t p = d.find("%PORT%");
	if(p != std::string::npos) {
	  char b[32]; sprintf(b, "%d,%d", data_port / 256, data_port % 256);
	  d.replace(p, 6, b);
	}
	if(listener.Send(c, d.data(), (int)d.size()) < 0) break;
      }
      else if(st.type == FS_SLEEP) {
	std::this_thread::sleep_for(std::chrono::milliseconds(atoi(st.data.c_str())));
      }
      else if(st.type == FS_READLINE || st.type == FS_READHDR) {
	std::string got;
	for(;;) {
	  if(!WaitRead(listener, c)) break;
	  char ch;
	  if(listener.RawRead(c, &ch, 1) != 1) break;
	  got += ch;
	  if(st.type == FS_READLINE && ch == '\n') break;
	  if(st.type == FS_READHDR && got.size() >= 4 &&
	     got.compare(got.size() - 4, 4, "\r\n\r\n") == 0) break;
	}
	received += got;
	if(got.empty()) break; // The client closed or timed out
      }
      else if(st.type == FS_DATA_LISTEN) {
	data.InitSocket(SOCK_STREAM, 0, (char *)"127.0.0.1");
	data.Bind();
	data.Listen(1);
	data.GetSockName();
	data_port = (int)ntohs(data.sin.sin_port);
      }
      else if(st.type == FS_DATA_SEND) {
	if(WaitRead(data, data.GetSocket()) && data.Accept() >= 0) {
	  gxsSocket_t d = data.GetRemoteSocket();
	  data.Send(d, st.data.data(), (int)st.data.size());
	  data.Close(d);
	}
	data.Close();
      }
    }
    // Let the client read what was sent before the close
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    listener.Close(c);
    listener.Close();
  }

private:
  gxSocket listener;
  gxSocket data;
  std::vector<FSStep> steps;
  std::thread *worker;
};

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

// --------------------------------------------------------------
// httpc
// --------------------------------------------------------------
static void TestHTTPClient()
{
  const char *sec = "httpc";
  gxsURL url;

  { // The header spans two packets; the document is only the body
    FakeServer fs;
    int port = fs.Start();
    Check(port > 0, sec, "fake server start");
    fs.ReadHeader();
    fs.Send("HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n");
    fs.Sleep(200);
    fs.Send("\r\nBODY");
    char ubuf[128]; sprintf(ubuf, "http://127.0.0.1:%d/doc.txt", port);
    gxsURLInfo u;
    url.ParseURL(ubuf, u);
    gxsHTTPClient client;
    client.SetTimeOut(5, 0);
    gxsHTTPHeader hdr;
    MemoryBuffer mbuf;
    int rv = client.Request(u, hdr, mbuf);
    fs.Join();
    std::string body((const char *)mbuf.m_buf(), mbuf.length());
    Check(rv == 0 && hdr.http_status == 200, sec, "split header request");
    Check(body == "BODY", sec, "split header: document is the body only");
  }

  { // A header with no end is not read without limit
    FakeServer fs;
    int port = fs.Start();
    fs.ReadHeader();
    fs.Send("HTTP/1.0 200 OK\r\n");
    std::string line = "X-Fill: " + std::string(1000, 'x') + "\r\n";
    std::string fill;
    for(int i = 0; i < 512; i++) fill += line;
    fs.Send(fill);
    char ubuf[128]; sprintf(ubuf, "http://127.0.0.1:%d/", port);
    gxsURLInfo u;
    url.ParseURL(ubuf, u);
    gxsHTTPClient client;
    client.SetTimeOut(5, 0);
    gxsHTTPHeader hdr;
    int rv = client.RequestHeader(u, hdr);
    fs.Join();
    Check(rv == gxSOCKET_BUFOVER_ERROR, sec, "endless header refused");
  }

  { // MemoryBuffer size limit
    FakeServer fs;
    int port = fs.Start();
    fs.ReadHeader();
    fs.Send("HTTP/1.0 200 OK\r\n\r\n" + std::string(5000, 'b'));
    char ubuf[128]; sprintf(ubuf, "http://127.0.0.1:%d/", port);
    gxsURLInfo u;
    url.ParseURL(ubuf, u);
    gxsHTTPClient client;
    client.SetTimeOut(5, 0);
    client.SetMaxBodySize(1000);
    gxsHTTPHeader hdr;
    MemoryBuffer mbuf;
    int rv = client.Request(u, hdr, mbuf);
    fs.Join();
    Check(rv == gxSOCKET_BUFOVER_ERROR && mbuf.length() <= 1000, sec,
	  "SetMaxBodySize() limit");
  }

  { // CR LF in the path is not sent as extra headers
    FakeServer fs;
    int port = fs.Start();
    fs.ReadHeader();
    fs.Send("HTTP/1.0 200 OK\r\n\r\nx");
    char ubuf[128]; sprintf(ubuf, "http://127.0.0.1:%d/", port);
    gxsURLInfo u;
    url.ParseURL(ubuf, u);
    u.path = "/a\r\nX-Injected: 1\r\n";
    gxsHTTPClient client;
    client.SetTimeOut(5, 0);
    gxsHTTPHeader hdr;
    MemoryBuffer mbuf;
    int rv = client.Request(u, hdr, mbuf);
    fs.Join();
    Check(rv == gxSOCKET_HTTP_ERROR, sec, "CR LF in the path refused");
    Check(fs.received.find("X-Injected") == std::string::npos, sec,
	  "no injected header sent");
  }

  // Method table bounds
  Check(strcmp(gxsHTTPMethodMessage(-1), "UNKNOWN") == 0 &&
	strcmp(gxsHTTPMethodMessage(gxsMAX_HTTP_METHODS), "UNKNOWN") == 0 &&
	strcmp(gxsHTTPMethodMessage(gxsHTTP_GET), "GET") == 0, sec,
	"gxsHTTPMethodMessage() bounds");

  { // BuildSimpleRequestString() with a long host and path
    gxsHTTPClient client;
    gxsURLInfo u;
    u.host = std::string(3000, 'h').c_str();
    u.path = ("/" + std::string(3000, 'p')).c_str();
    u.port = 8080;
    u.proto_type = gxsURL::gxs_http;
    gxString request;
    client.BuildSimpleRequestString(u, gxsHTTP_GET, request);
    Check(request.length() > 6000 && request.Find(":8080/ppp") != -1, sec,
	  "long simple request string");
  }
}

// --------------------------------------------------------------
// url
// --------------------------------------------------------------
static void TestURL()
{
  const char *sec = "url";
  gxsURL url;
  gxsURLInfo u;

  Check(strcmp(url.GetProtocolString(gxsMAX_PROTOCOL_STRINGS),
	       "Unknown_URL_protocol:") == 0, sec, 
	"GetProtocolString(gxsMAX_PROTOCOL_STRINGS) in bounds");
  Check(strcmp(url.GetProtocolString(-1), "Unknown_URL_protocol:") == 0, 
	sec, "GetProtocolString(-1) in bounds");
  Check(url.GetProtocolType("http:") == gxsURL::gxs_http, sec,
	"GetProtocolType(http:)");
  Check(url.GetProtocolType("whodp:") == gxsURL::gxs_whodp, sec,
	"GetProtocolType(whodp:)");
  Check(url.GetProtocolType("nope:") == gxsURL::gxs_Unknown_URL_protocol, 
	sec, "GetProtocolType(nope:)");

  // Ports
  Check(url.ParseURL("http://127.0.0.1:8080/x", u) && u.port == 8080, sec,
	"port 8080");
  Check(url.ParseURL("http://127.0.0.1/x", u) && u.port == 80, sec,
	"default port");
  Check(!url.ParseURL("http://127.0.0.1:-1/x", u), sec, "port -1 refused");
  Check(!url.ParseURL("http://127.0.0.1:99999999999/x", u), sec,
	"port overflow refused");
  Check(!url.ParseURL("http://127.0.0.1:80abc/x", u), sec, 
	"port 80abc refused");
  Check(!url.ParseURL("http://127.0.0.1:65536/x", u), sec, 
	"port 65536 refused");

  // Control characters
  Check(!url.ParseURL("http://127.0.0.1/a\r\nX-Injected: 1", u), sec,
	"CR LF inside a URL refused");
  Check(url.ParseURL("http://127.0.0.1/a.html\r\n", u) && 
	u.path == "/a.html", sec, "trailing CR LF removed");
  Check(gxsURL::HasControlChars("a\tb") && !gxsURL::HasControlChars("a b"),
	sec, "HasControlChars()");

  // DecodeStrict()
  gxString out;
  Check(gxsURL::DecodeStrict("%41%42c", out) && out == "ABc", sec,
	"DecodeStrict() %41%42c");
  Check(!gxsURL::DecodeStrict("a%00b", out) && out.is_null(), sec,
	"DecodeStrict() refuses %00");
  Check(!gxsURL::DecodeStrict("a%0d%0a", out), sec,
	"DecodeStrict() refuses %0d%0a");
  Check(!gxsURL::DecodeStrict("a%4", out) && !gxsURL::DecodeStrict("%", out) &&
	!gxsURL::DecodeStrict("%zz", out), sec, 
	"DecodeStrict() refuses bad escapes");
  Check(gxsURL::DecodeStrict("a+b", out, 1) && out == "a b", sec,
	"DecodeStrict() plus as space");
  Check(gxsURL::DecodeStrict("a+b", out) && out == "a+b", sec,
	"DecodeStrict() plus kept");

  // % decoding of dynamic page names
  Check(url.ParseURL("http://127.0.0.1/a.php?x=..%5C..%5Cy", u) &&
	u.local_query_file.Find("\\") == -1, sec,
	"no backslash in a local query file name");
  Check(url.ParseURL("http://127.0.0.1/a.php?x=%00y%", u) &&
	!gxsURL::HasControlChars(u.local_query_file) && 
	u.query_string.Find("y%") != -1, sec,
	"%00 and a trailing % in a query");
}

int main(int argc, char **argv)
{
  if(Enabled(argc, argv, "httphdr")) TestHTTPHeader();
  if(Enabled(argc, argv, "httpc")) TestHTTPClient();
  if(Enabled(argc, argv, "url")) TestURL();

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
