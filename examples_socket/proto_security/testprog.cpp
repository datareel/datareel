// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: testprog.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 10/31/2026
// Date Last Modified: 11/16/2026
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

6.3 tests for the HTTP, URL, SMTP, POP3 and FTP security fixes, and
6.4 tests for the IMAP client
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
  smtp      gxsSMTPClient: multi-line replies, strict reply codes, long
            arguments, CR LF in commands, NormalizeAddress() (ESMTP
            parameter injection), dot-stuffing (SMTP smuggling) and
            header injection in SMTPSendMessage().
  pop3      gxsPOP3Client: "+OK" must start the status line, "-ERR" to
            a multi-line command, no write past a full buffer, long
            arguments and CR LF in commands, CheckReply().
  imap      gxsIMAPClient (6.4): a reply ends only at its tagged line
            (not at NO/BAD or a tag inside message text), NO and BAD
            are errors, literals read by count, quoting and literals
            for arguments, no password without TLS, LIST/SEARCH/SELECT
            parsing, the reply size limit, data sent before a STARTTLS
            handshake.
  ftp       gxsFTPClient: multi-line and left over 226 replies, PASV
            address not used, PASV field checks and a long PASV reply
            (stack overflow), long file names, CR LF in commands, no
            write past a full FTPList() buffer, ParseReply().
  utils     ParseServiceFileEntry() with a long port/protocol field
            (stack overflow), InChecksum16() on a large buffer.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if defined (__UNIX__)
#include <signal.h>
#endif
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include "gxsocket.h"
#include "gxshttp.h"
#include "gxshttpc.h"
#include "gxsurl.h"
#include "membuf.h"
#include "dfileb.h"
#include "gxsmtp.h"
#include "gxspop3.h"
#include "gxsimap.h"
#include "gxsftp.h"
#include "gxsutils.h"

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
  FS_READDATA,  // Read up to and including CR LF . CR LF (SMTP DATA)
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
  void ReadData() { Add(FS_READDATA, ""); }
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
      else if(st.type == FS_READLINE || st.type == FS_READHDR ||
	      st.type == FS_READDATA) {
	std::string got;
	for(;;) {
	  if(!WaitRead(listener, c)) break;
	  char ch;
	  if(listener.RawRead(c, &ch, 1) != 1) break;
	  got += ch;
	  if(st.type == FS_READLINE && ch == '\n') break;
	  if(st.type == FS_READHDR && got.size() >= 4 &&
	     got.compare(got.size() - 4, 4, "\r\n\r\n") == 0) break;
	  if(st.type == FS_READDATA && got.size() >= 5 &&
	     got.compare(got.size() - 5, 5, "\r\n.\r\n") == 0) break;
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

// --------------------------------------------------------------
// smtp
// --------------------------------------------------------------
static void TestSMTP()
{
  const char *sec = "smtp";

  // NormalizeAddress()
  gxString a;
  Check(gxsSMTPClient::NormalizeAddress("me@x.org", a) && a == "<me@x.org>",
	sec, "NormalizeAddress() bare address");
  Check(gxsSMTPClient::NormalizeAddress(" Me Too <me@x.org> ", a) && 
	a == "<me@x.org>", sec, "NormalizeAddress() display name");
  Check(!gxsSMTPClient::NormalizeAddress("a@b> NOTIFY=SUCCESS", a), sec,
	"NormalizeAddress() refuses text after '>'");
  Check(!gxsSMTPClient::NormalizeAddress("<a@b> SIZE=1", a), sec,
	"NormalizeAddress() refuses ESMTP parameters");
  Check(!gxsSMTPClient::NormalizeAddress("a@b SIZE=1", a), sec,
	"NormalizeAddress() refuses spaces");
  Check(!gxsSMTPClient::NormalizeAddress("<a<b@c>", a), sec,
	"NormalizeAddress() refuses a second '<'");
  Check(!gxsSMTPClient::NormalizeAddress("<>", a) &&
	gxsSMTPClient::NormalizeAddress("<>", a, 1) && a == "<>", sec,
	"NormalizeAddress() null sender");

  // ParseReply()
  int code = 0, used = 0;
  const char *r1 = "250-a\r\n250-b\r\n250 ok\r\nXX";
  Check(gxsSMTPClient::ParseReply(r1, (int)strlen(r1), &code, &used) == 1 &&
	code == 250 && used == (int)strlen(r1) - 2, sec, 
	"ParseReply() multi-line");
  Check(gxsSMTPClient::ParseReply("250-a\r\n250", 10) == 0, sec,
	"ParseReply() incomplete");
  Check(gxsSMTPClient::ParseReply("2x0 hi\r\n", 8) == -1 &&
	gxsSMTPClient::ParseReply("2500 hi\r\n", 9) == -1 &&
	gxsSMTPClient::ParseReply("25\r\n", 4) == -1, sec,
	"ParseReply() refuses bad codes");

  FakeServer fs;
  int port = fs.Start();
  Check(port > 0, sec, "fake server start");
  fs.Send("220 test\r\n");
  fs.Reply("250-test\r\n");           // EHLO, sent in two parts
  fs.Sleep(200);
  fs.Send("250 OK\r\n");
  fs.Reply("334 VXNlcm5hbWU6\r\n");   // AUTH LOGIN
  fs.Reply("334 UGFzc3dvcmQ6\r\n");   // user
  fs.Reply("235 ok\r\n");             // password
  fs.Reply("250 ok\r\n");             // MAIL FROM
  fs.Reply("250 ok\r\n");             // RCPT TO
  fs.Reply("354 go\r\n");             // DATA
  fs.ReadData();
  fs.Send("250 queued\r\n");
  fs.Reply("221 bye\r\n");            // QUIT

  gxsSMTPClient smtp;
  smtp.SetTimeOut(5, 0);
  int rv = smtp.ConnectClient("127.0.0.1", port);
  Check(rv == 0, sec, "connect");
  rv = smtp.ESMTPLogin("client");
  Check(rv == 0, sec, "EHLO multi-line reply");
  rv = smtp.ESMTPAuthLogin("dXNlcg==", "cGFzcw==");
  Check(rv == 0, sec, "AUTH LOGIN after a multi-line reply");

  // Refused before anything is sent
  std::string longarg(5000, 'd');
  Check(smtp.SMTPLogin(longarg.c_str()) == gxSOCKET_BUFOVER_ERROR, sec,
	"long HELO argument refused");
  Check(smtp.SMTPRcptTo("a@b> NOTIFY=SUCCESS") != 0, sec,
	"RCPT TO parameter injection refused");
  Check(smtp.SMTPRcptTo("a@b\r\nRSET") != 0, sec,
	"RCPT TO CR LF refused");
  Check(smtp.SendCommand("NOOP", "250", "x\r\nRSET") != 0, sec,
	"SendCommand() CR LF refused");
  Check(smtp.SMTPSendMessage("you@y.org", "me@x.org", 
			     "Hi\r\nBcc: other@z.org", "body") != 0, sec,
	"SMTPSendMessage() header injection refused");

  rv = smtp.SMTPMailFrom("Me <me@x.org>");
  Check(rv == 0, sec, "MAIL FROM");
  rv = smtp.SMTPRcptTo("you@y.org");
  Check(rv == 0, sec, "RCPT TO");
  const char *body = "line1\n.\nQUIT\r\n.hidden\rend";
  rv = smtp.SMTPData(body, (int)strlen(body));
  Check(rv == 0, sec, "DATA");
  rv = smtp.SMTPLogout();
  Check(rv == 0, sec, "QUIT");
  smtp.Close();
  fs.Join();

  Check(fs.received.find("MAIL FROM:<me@x.org>\r\n") != std::string::npos,
	sec, "MAIL FROM:<address> sent");
  Check(fs.received.find("RCPT TO:<you@y.org>\r\n") != std::string::npos,
	sec, "RCPT TO:<address> sent");
  Check(fs.received.find("NOTIFY") == std::string::npos &&
	fs.received.find("RSET") == std::string::npos &&
	fs.received.find("Bcc") == std::string::npos &&
	fs.received.find("ddddd") == std::string::npos, sec,
	"refused commands not sent");
  Check(fs.received.find("line1\r\n..\r\nQUIT\r\n..hidden\r\nend\r\n.\r\n")
	!= std::string::npos, sec, "body dot-stuffed with CR LF line ends");
}

// --------------------------------------------------------------
// pop3
// --------------------------------------------------------------
static void TestPOP3()
{
  const char *sec = "pop3";

  Check(gxsPOP3Client::CheckReply("+OK 2\r\n", 7, "+OK") == 1 &&
	gxsPOP3Client::CheckReply("+OK", 3, "+OK") == 0 &&
	gxsPOP3Client::CheckReply("-ERR no +OK\r\n", 13, "+OK") == -1, sec,
	"CheckReply() status line");
  Check(gxsPOP3Client::CheckReply("+OK\r\n.\r\n", 8, "\r\n.\r\n") == 1 &&
	gxsPOP3Client::CheckReply("+OK\r\n1 2\r\n", 11, "\r\n.\r\n") == 0 &&
	gxsPOP3Client::CheckReply("-ERR\r\n.\r\n", 9, "\r\n.\r\n") == -1,
	sec, "CheckReply() multi-line");

  FakeServer fs;
  int port = fs.Start();
  Check(port > 0, sec, "fake server start");
  fs.Send("+OK ready\r\n");
  fs.Reply("+OK\r\n");                       // USER
  fs.Reply("-ERR bad password +OK\r\n");     // PASS
  fs.Reply("+OK\r\n");                       // USER
  fs.Reply("+OK logged in\r\n");             // PASS
  fs.Reply("-ERR no such message\r\n");      // RETR 9
  fs.Reply("+OK\r\n" + std::string(200, 'L') + "\r\n.\r\n"); // LIST
  fs.Reply("+OK bye\r\n");                   // QUIT

  gxsPOP3Client pop3;
  pop3.SetTimeOut(3, 0);
  int rv = pop3.ConnectClient("127.0.0.1", port);
  Check(rv == 0, sec, "connect");
  rv = pop3.POP3Login("user", "pass");
  Check(rv != 0, sec, "-ERR holding +OK is an error");

  std::string longarg(5000, 'u');
  Check(pop3.POP3Login(longarg.c_str(), "x") == gxSOCKET_BUFOVER_ERROR, sec,
	"long user name refused");
  Check(pop3.POP3Login("user\r\nDELE 1", "x") != 0, sec,
	"CR LF in a user name refused");
  rv = pop3.POP3Login("user", "pass");
  Check(rv == 0, sec, "login");

  char big[1024];
  rv = pop3.POP3Retr(9, big, sizeof(big));
  Check(rv == gxSOCKET_POP3_ERROR, sec, "-ERR to RETR is an error at once");

  char area[100];
  memset(area, 'Z', sizeof(area));
  rv = pop3.POP3List(area, 64);
  Check(rv == gxSOCKET_BUFOVER_ERROR, sec, "LIST over the buffer size");
  Check(area[64] == 'Z' && pop3.BytesRead() < 64, sec, 
	"no write past a full buffer");
  pop3.Close();
  fs.Join();
  Check(fs.received.find("DELE") == std::string::npos &&
	fs.received.find("uuuuu") == std::string::npos, sec,
	"refused commands not sent");
}

// --------------------------------------------------------------
// imap (6.4)
// --------------------------------------------------------------
static int IMAPConnect(gxsIMAPClient &imap, FakeServer &fs)
{
  int port = fs.Start();
  if(port <= 0) return -1;
  imap.SetTimeOut(3, 0);
  imap.AllowPlainLogin();
  return imap.ConnectClient("127.0.0.1", port, 0);
}

static void TestIMAP()
{
  const char *sec = "imap";

  Check(gxsIMAPClient::Quote("a\"b\\c") == "\"a\\\"b\\\\c\"" &&
	gxsIMAPClient::Quote("x\r\ny").is_null() &&
	gxsIMAPClient::Quote("caf\xc3\xa9").is_null(), sec, "Quote()");

  { // Login: NO is an error, a literal for 8-bit passwords, the
    // password is not recorded
    FakeServer fs;
    fs.Send("* OK IMAP4rev1 ready\r\n");
    fs.Reply("A0001 NO [AUTHENTICATIONFAILED] Invalid credentials\r\n");
    fs.Reply("+ Ready for literal data\r\n");    // A0002 LOGIN "user" {5}
    fs.Reply("A0002 OK LOGIN completed\r\n");    // the literal
    fs.Reply("* BYE logging out\r\nA0003 OK LOGOUT completed\r\n");
    gxsIMAPClient imap;
    imap.RecordSession();
    Check(IMAPConnect(imap, fs) == 0, sec, "connect");
    int rv = imap.Login("user", "wrong");
    Check(rv == gxSOCKET_IMAP_ERROR && 
	  strstr(imap.IMAPExceptionMessage(), "Invalid credentials") != 0,
	  sec, "LOGIN NO is an error with the server's text");
    rv = imap.Login("user", "p\xc3\xa4ss");
    Check(rv == 0, sec, "8-bit password sent as a literal");
    Check(imap.Logout() == 0 && !imap.IMAPConnected(), sec, "logout");
    fs.Join();
    Check(fs.received.find("A0001 LOGIN \"user\" \"wrong\"\r\n") == 0, sec,
	  "LOGIN arguments quoted");
    Check(fs.received.find("A0002 LOGIN \"user\" {5}\r\np\xc3\xa4ss\r\n") 
	  != std::string::npos, sec, "literal sent after the continuation");
    Check(imap.ProtoLog().Find("wrong") == -1 && 
	  imap.ProtoLog().Find("p\xc3\xa4ss") == -1, sec, 
	  "passwords not recorded");
  }

  { // A tagged NO instead of a continuation request ends the command
    FakeServer fs;
    fs.Send("* OK ready\r\n");
    fs.Reply("A0001 NO literal too big\r\n");
    gxsIMAPClient imap;
    Check(IMAPConnect(imap, fs) == 0, sec, "connect 2");
    Check(imap.Login("u", "\xff\xfe") == gxSOCKET_IMAP_ERROR, sec, 
	  "NO instead of + is an error");
    imap.CloseClient();
    fs.Join();
  }

  { // No password over a connection without TLS unless allowed
    FakeServer fs;
    fs.Send("* OK ready\r\n");
    int port = fs.Start();
    gxsIMAPClient imap;
    imap.SetTimeOut(3, 0);
    Check(imap.ConnectClient("127.0.0.1", port, 0) == 0, sec, "connect 3");
    Check(imap.Login("u", "p") != 0, sec, "plain text login refused");
    imap.CloseClient();
    fs.Join();
    Check(fs.received.find("LOGIN") == std::string::npos, sec, 
	  "password not sent");
  }

  { // Replies: message text with NO, BAD and our tag inside a literal,
    // SELECT, SEARCH, LIST, FETCH, BAD
    std::string body = "A0003 OK not the end\r\nNO\r\nBAD\r\n* 9 EXISTS\r\n";
    char lit[64];
    sprintf(lit, "{%d}\r\n", (int)body.size());
    FakeServer fs;
    fs.Send("* OK ready\r\n");
    fs.Reply("* FLAGS (\\Seen \\Deleted)\r\n* 3 EXISTS\r\n* 0 RECENT\r\n"
	     "A0001 OK [READ-WRITE] SELECT completed\r\n");
    fs.Reply("* SEARCH 1 3\r\nA0002 OK SEARCH completed\r\n");
    fs.Reply(std::string("* 3 FETCH (BODY[TEXT] ") + lit + body + 
	     ")\r\nA0003 OK FETCH completed\r\n");
    fs.Reply("* LIST (\\HasNoChildren) \"/\" \"INBOX\"\r\n"
	     "* LIST () \"/\" {9}\r\nSent Mail\r\n"
	     "* LIST () \".\" Archive\r\nA0004 OK LIST completed\r\n");
    std::string dhdr = "Date: Mon, 16 Nov 2026\r\n 09:14:59 -0500\r\n\r\n";
    char dlit[32];
    sprintf(dlit, "{%d}\r\n", (int)dhdr.size());
    fs.Reply(std::string("* 2 FETCH (INTERNALDATE \"16-Nov-2026 09:15:00 -0500\" "
	     "BODY[HEADER.FIELDS (DATE)] ") + dlit + dhdr + 
	     ")\r\nA0005 OK done\r\n");
    fs.Reply("A0006 BAD unknown command\r\n");
    gxsIMAPClient imap;
    Check(IMAPConnect(imap, fs) == 0, sec, "connect 4");
    int n = 0;
    Check(imap.SelectMailbox("INBOX", n) == 0 && n == 3, sec, 
	  "SELECT EXISTS count");
    gxString ids;
    Check(imap.Search("UNSEEN", ids) == 0 && ids == "1 3", sec, "SEARCH");
    gxString text;
    int rv = imap.FetchBody(3, text);
    Check(rv == 0 && text == body.c_str(), sec, 
	  "NO, BAD and the tag inside a literal do not end the reply");
    gxString names;
    Check(imap.ListMailboxes(names) == 0 && 
	  names == "INBOX\nSent Mail\nArchive\n", sec, 
	  "LIST quoted, literal and atom names");
    gxString rd, sd;
    Check(imap.FetchDates(2, rd, sd) == 0 && 
	  rd == "16-Nov-2026 09:15:00 -0500" &&
	  sd == "Mon, 16 Nov 2026 09:14:59 -0500", sec, "FetchDates()");
    gxString reply;
    Check(imap.SendCommand("XYZZY", reply) == gxSOCKET_IMAP_ERROR, sec,
	  "BAD is an error");
    Check(imap.SendCommand("NOOP\r\nA9 LOGOUT", reply) != 0, sec,
	  "CR LF in a command refused");
    Check(imap.Search("ALL\r\nA9 LOGOUT", ids) != 0, sec,
	  "CR LF in search criteria refused");
    Check(imap.StoreFlags(1, "\\Seen) (x") != 0, sec, 
	  "parentheses in flags refused");
    imap.CloseClient();
    fs.Join();
    Check(fs.received.find("A0001 SELECT \"INBOX\"\r\n") == 0 &&
	  fs.received.find("A0003 FETCH 3 (BODY.PEEK[TEXT])") != 
	  std::string::npos, sec, "commands as sent");
    Check(fs.received.find("LOGOUT") == std::string::npos, sec,
	  "refused commands not sent");
  }

  { // Reply size limit, and a literal size beyond it
    FakeServer fs;
    fs.Send("* OK ready\r\n");
    fs.Reply("* 1 FETCH (BODY[] {999999999}\r\n");
    gxsIMAPClient imap;
    Check(IMAPConnect(imap, fs) == 0, sec, "connect 5");
    imap.SetMaxReply(4096);
    gxString msg;
    Check(imap.FetchMessage(1, msg) != 0 && msg.is_null(), sec,
	  "literal larger than the reply limit refused");
    imap.CloseClient();
    fs.Join();
  }

  { // A server that never sends the tagged reply times out
    FakeServer fs;
    fs.Send("* OK ready\r\n");
    fs.Reply("* 1 EXISTS\r\n");
    fs.Sleep(3000);
    gxsIMAPClient imap;
    Check(IMAPConnect(imap, fs) == 0, sec, "connect 6");
    imap.SetTimeOut(1, 0);
    Check(imap.Noop() != 0, sec, "missing tagged reply times out");
    imap.CloseClient();
    fs.Join();
  }

  { // A greeting that is not OK
    FakeServer fs;
    fs.Send("* BYE too many connections\r\n");
    gxsIMAPClient imap;
    Check(IMAPConnect(imap, fs) != 0 && 
	  strstr(imap.IMAPExceptionMessage(), "too many") != 0, sec,
	  "BYE greeting refused");
    fs.Join();
  }

  { // The DataReel 7.4 interface and the 2007 calls (std::string)
    std::string hdr = "Date: Mon, 16 Nov 2026 09:14:59 -0500\r\nSubject: x\r\n\r\n";
    char hl[32];
    sprintf(hl, "{%d}\r\n", (int)hdr.size());
    FakeServer fs;
    fs.Send("* OK ready\r\n");
    fs.Reply("* CAPABILITY IMAP4rev1 LOGINDISABLED STARTTLS\r\n"
	     "A0001 OK CAPABILITY completed\r\n");                    // CAPABILITY
    fs.Reply("* 2 EXISTS\r\nA0002 OK [READ-ONLY] EXAMINE completed\r\n"); // EXAMINE
    fs.Reply("* SEARCH 2 1\r\nA0003 OK SEARCH completed\r\n");       // SEARCH
    fs.Reply("* LIST () \"/\" INBOX\r\n* LIST () \"/\" {6}\r\nDrafts\r\n"
	     "A0004 OK LIST completed\r\n");                           // LIST
    fs.Reply(std::string("* 1 FETCH (INTERNALDATE \"16-Nov-2026 09:15:00 -0500\" "
	     "ENVELOPE (\"Mon, 16 Nov 2026 09:14:59 -0500\" \"x\" NIL NIL NIL NIL NIL NIL NIL NIL) "
	     "BODY[HEADER] ") + hl + hdr + ")\r\nA0005 OK FETCH completed\r\n"); // FETCH 1
    fs.Reply("* 3 EXISTS\r\nA0006 OK [READ-WRITE] SELECT completed\r\n"); // SELECT
    fs.ReadLine();                                                      // A0007 NOOP
    fs.Send("* 4 EXISTS\r\nA0007 OK NOOP completed\r\n");
    fs.Reply("A0008 OK STORE completed\r\n");                          // Delete
    fs.Reply("A0009 NO [CANNOT] cannot expunge\r\n");                  // EXPUNGE
    fs.Reply("* BYE\r\nA0010 OK LOGOUT completed\r\n");                // LOGOUT
    gxsIMAPClient imap;
    imap.SetTimeout(3);
    int port = fs.Start();
    Check(imap.ConnectPlain("127.0.0.1", port) == 0, sec, "7.4: ConnectPlain");
    std::vector<std::string> caps;
    Check(imap.Capability(caps) == 0 && caps.size() == 3 &&
	  imap.HasCapability("logindisabled"), sec, "7.4: Capability, HasCapability");
    Check(imap.Login(std::string("u"), std::string("p")) == -1 &&
	  imap.Error().find("LOGINDISABLED") != std::string::npos, sec,
	  "7.4: no LOGIN when the server says LOGINDISABLED");
    int n = 0;
    Check(imap.Examine("INBOX", n) == 0 && n == 2 &&
	  imap.LastReply().find("A0002 OK") == 0, sec, "7.4: Examine, LastReply");
    std::vector<unsigned> nums;
    Check(imap.Search("ALL", nums) == 0 && nums.size() == 2 && nums[0] == 2 &&
	  nums[1] == 1, sec, "7.4: Search into numbers");
    std::vector<std::string> names;
    Check(imap.ListMailboxes(names) == 0 && names.size() == 2 &&
	  names[0] == "INBOX" && names[1] == "Drafts", sec,
	  "7.4: ListMailboxes, literal name");
    std::string h, dt, sdt;
    Check(imap.FetchHeader(1, h, dt, sdt) == 0 && h == hdr &&
	  dt == "16-Nov-2026 09:15:00 -0500" &&
	  sdt == "Mon, 16 Nov 2026 09:14:59 -0500", sec,
	  "7.4: FetchHeader with the 2007 dates");
    Check(imap.Untagged().size() == 1 && imap.Untagged()[0].literals.size() == 1,
	  sec, "7.4: Untagged() holds the FETCH and its literal");
    std::string rx;
    Check(imap.SelectMailbox("INBOX", rx, n) == 0 && n == 3 &&
	  rx.find("* 3 EXISTS") != std::string::npos, sec,
	  "7.4: SelectMailbox with the 2007 rxbuf");
    std::string tag, buf, line;
    Check(imap.SendCommand(std::string("NOOP"), tag) == 0 && tag == "A0007" &&
	  imap.RecvBuffer(tag, buf) == 0 &&
	  gxsIMAPClient::ParseResponse(buf, "*", "\r\n", "EXISTS", line, 1) == 1 &&
	  line == "4", sec, "7.4: SendCommand, RecvBuffer, ParseResponse (2007)");
    Check(imap.Delete(1) == 0, sec, "7.4: Delete");
    Check(imap.Expunge() != 0 && imap.Error().find("cannot expunge") != std::string::npos,
	  sec, "7.4: NO to EXPUNGE is an error with the server's text");
    Check(imap.Logout() == 0 && !imap.IsConnected(), sec, "7.4: Logout");
    std::string q;
    Check(gxsIMAPClient::Quote(std::string("a\"b"), q) && q == "\"a\\\"b\"" &&
	  !gxsIMAPClient::Quote(std::string("x\ny"), q), sec, "7.4: Quote");
    fs.Join();
    Check(fs.received.find("A0002 EXAMINE \"INBOX\"") != std::string::npos &&
	  fs.received.find("A0008 STORE 1 +FLAGS.SILENT (\\Deleted)") != std::string::npos &&
	  fs.received.find("LOGIN") == std::string::npos, sec,
	  "7.4: commands as sent, no LOGIN");
  }

#if defined (__USE_GX_SSL_EXTENSIONS__)
  { // STARTTLS: bytes sent after the OK, before the handshake, are not
    // read as TLS data
    FakeServer fs;
    fs.Send("* OK ready\r\n");
    fs.Reply("A0001 OK Begin TLS negotiation now\r\n* 1 EXISTS\r\n");
    gxsIMAPClient imap;
    Check(IMAPConnect(imap, fs) == 0, sec, "connect 7");
    Check(imap.StartTLS() != 0 && !imap.IMAPConnected(), sec,
	  "data after the STARTTLS OK refused");
    fs.Join();
  }
#endif
}

// --------------------------------------------------------------
// ftp
// --------------------------------------------------------------
static void TestFTP()
{
  const char *sec = "ftp";

  int code = 0, used = 0;
  const char *r1 = "230-Hi\r\nany text\r\n230 ok\r\n226";
  Check(gxsFTPClient::ParseReply(r1, (int)strlen(r1), &code, &used) == 1 &&
	code == 230 && used == (int)strlen(r1) - 3, sec, 
	"ParseReply() multi-line");
  Check(gxsFTPClient::ParseReply("230-Hi\r\n230", 11) == 0 &&
	gxsFTPClient::ParseReply("2x0 hi\r\n", 8) == -1 &&
	gxsFTPClient::ParseReply("20\r\n", 4) == -1, sec,
	"ParseReply() incomplete and bad codes");
  Check(gxsFTPClient::ParsePASVPort("227 Entering (10,0,0,1,4,1)") == 1025 &&
	gxsFTPClient::ParsePASVPort("227 Entering 10,0,0,1,4,1") == 1025 &&
	gxsFTPClient::ParsePASVPort("227 Entering (10,0,0,1,256,1)") == -1 &&
	gxsFTPClient::ParsePASVPort("227 Entering (1,2,3,4,99999999999,1)") 
	== -1 &&
	gxsFTPClient::ParsePASVPort("227 Entering (1,2,3,4,-1,1)") == -1 &&
	gxsFTPClient::ParsePASVPort("227 Entering (1,2,3,4,0,0)") == -1 &&
	gxsFTPClient::ParsePASVPort("227 (1,2,3)") == -1, sec,
	"ParsePASVPort()");

  FakeServer fs;
  int port = fs.Start();
  Check(port > 0, sec, "fake server start");
  fs.Send("220-Welcome\r\n");
  fs.Sleep(100);
  fs.Send("220 ready\r\n");
  fs.Reply("331 password\r\n");                    // USER
  fs.Reply("230-Hi\r\n");                          // PASS
  fs.Sleep(100);
  fs.Send("230 ok\r\n");
  fs.DataListen();
  fs.Reply("227 Entering Passive Mode (127,0,0,2,%PORT%)\r\n"); // PASV
  fs.Reply("150 here\r\n");                        // LIST
  fs.DataSend("file1\r\nfile2\r\n");
  fs.Sleep(100);
  fs.Send("226 done\r\n");
  fs.Reply("213 42\r\n");                          // SIZE
  fs.DataListen();
  fs.Reply("227 Entering Passive Mode (127,0,0,1,%PORT%)\r\n"); // PASV
  fs.Reply("150 here\r\n");                        // LIST
  fs.DataSend(std::string(200, 'f'));
  fs.Sleep(100);
  fs.Send("226 done\r\n");
  fs.Reply("227 Entering Passive Mode " + std::string(5000, 'A') +
	   " (127,0,0,1,999,1)\r\n");               // PASV
  fs.Reply("221 bye\r\n");                         // QUIT

  gxsFTPClient ftp;
  ftp.SetTimeOut(3, 0);
  ftp.FTPPassive();
  int rv = ftp.ConnectClient("127.0.0.1", port);
  Check(rv == 0, sec, "connect with a multi-line greeting");
  rv = ftp.FTPLogin("user", "pass");
  Check(rv == 0, sec, "login with a multi-line 230 reply");

  // Refused before anything is sent
  std::string longname(10000, 'n');
  DiskFileB nofile;
  FAU_t nbytes;
  Check(ftp.FTPGet(longname.c_str(), nofile, nbytes) == gxSOCKET_BUFOVER_ERROR,
	sec, "long RETR file name refused");
  Check(ftp.FTPChDir("dir\r\nDELE x") != 0, sec, "CR LF in CWD refused");

  char list[256];
  rv = ftp.FTPList(list, sizeof(list));
  Check(rv == 0 && strcmp(list, "file1\r\nfile2\r\n") == 0, sec,
	"PASV to the server's own address, not the one in the reply");
  rv = ftp.FTPSize("x");
  Check(rv == 0 && strncmp(ftp.reply_buf, "213 42", 6) == 0, sec,
	"left over 226 reply skipped");

  char area[100];
  memset(area, 'Z', sizeof(area));
  rv = ftp.FTPList(area, 64);
  Check(rv == gxSOCKET_BUFOVER_ERROR && area[64] == 'Z', sec,
	"no write past a full FTPList() buffer");

  rv = ftp.FTPList(list, sizeof(list));
  Check(rv == gxSOCKET_FTP_ERROR, sec, "long PASV reply with a bad port");
  rv = ftp.FTPLogout();
  Check(rv == 0, sec, "QUIT");
  ftp.Close();
  fs.Join();
  Check(fs.received.find("DELE") == std::string::npos &&
	fs.received.find("nnnnn") == std::string::npos, sec,
	"refused commands not sent");
}

// --------------------------------------------------------------
// utils
// --------------------------------------------------------------
static void TestUtils()
{
  const char *sec = "utils";

  std::string entry = "http 80/" + std::string(3000, 't') + " www # web\n";
  std::vector<char> e(entry.begin(), entry.end());
  e.push_back(0);
  std::vector<char> service(e.size()), protocol(e.size()), aliases(e.size()),
    comment(e.size());
  int port = -1;
  int rv = ParseServiceFileEntry(&e[0], &service[0], &port, &protocol[0],
				 &aliases[0], &comment[0]);
  Check(rv == 1 && port == 80 && strlen(&protocol[0]) == 3000, sec,
	"ParseServiceFileEntry() long protocol field");
  char e2[] = "svc 99999999999/tcp";
  char b1[64], b2[64], b3[64], b4[64];
  rv = ParseServiceFileEntry(e2, b1, &port, b2, b3, b4);
  Check(rv == 1 && port == 0, sec, "ParseServiceFileEntry() port overflow");

  // InChecksum16() of a large buffer of 0xFFFF words. The one's
  // complement sum of 0xFFFF words is 0xFFFF, so the checksum is 0.
  std::vector<u_short> words(100000, 0xFFFF);
  Check(InChecksum16(&words[0], (int)(words.size() * 2)) == 0, sec,
	"InChecksum16() large buffer");
  std::vector<u_short> w2(100000, 0x1234);
  unsigned long long ref = 0;
  for(size_t i = 0; i < w2.size(); i++) ref += w2[i];
  while(ref >> 16) ref = (ref & 0xffff) + (ref >> 16);
  Check(InChecksum16(&w2[0], (int)(w2.size() * 2)) == (u_short)~ref, sec,
	"InChecksum16() matches a 64-bit reference");
}

int main(int argc, char **argv)
{
#if defined (__UNIX__)
  // A failed check can leave a client writing to a closed fake server
  signal(SIGPIPE, SIG_IGN);
#endif

  if(Enabled(argc, argv, "httphdr")) TestHTTPHeader();
  if(Enabled(argc, argv, "httpc")) TestHTTPClient();
  if(Enabled(argc, argv, "url")) TestURL();
  if(Enabled(argc, argv, "smtp")) TestSMTP();
  if(Enabled(argc, argv, "pop3")) TestPOP3();
  if(Enabled(argc, argv, "imap")) TestIMAP();
  if(Enabled(argc, argv, "ftp")) TestFTP();
  if(Enabled(argc, argv, "utils")) TestUtils();

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
