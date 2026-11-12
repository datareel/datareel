// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File Name: proto_security_test.cpp
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 11/01/2026
// Date Last Modified: 11/01/2026
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

Regression tests for the DataReel 7.3.1 security sweep of the HTTP,
FTPS, SMTP, POP3 and URL code, and of the path mapping in the
https_server example. No network is used: the parsers are fed
buffers. Prints "PASS <name>" per test and returns non-zero on a
failure. Run it in a scratch folder; it makes and removes the folder
proto_security_www.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <cstdint>
#include <cstdio>
#include <string>

#include "gxsftp.h"
#include "gxshttp.h"
#include "gxsmtp.h"
#include "gxspop3.h"
#include "gxsurl.h"

// The path mapping lives in the example program
#define HTTPS_SERVER_NO_MAIN
#include "../examples/tls/https_server.cpp"

static int failures = 0;
static bool test_ok = true;

#define CHECK(cond) do { if(!(cond)) { \
  printf("  check failed line %d: %s\n", __LINE__, #cond); \
  test_ok = false; } } while(0)

static void start() { test_ok = true; }
static void finish(const char *name)
{
  if(test_ok) printf("PASS %s\n", name);
  else { printf("FAIL %s\n", name); failures++; }
}

// A string literal that holds NUL characters
#define S(lit) std::string(lit, sizeof(lit) - 1)

// ------------------------------------------------------------------
// 1, 9: HTTP response status line, interim responses, framing
static void test_http_status()
{
  start();
  gxsHTTPMessage m;
  CHECK(gxsHTTPParseResponse("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nhi", m) == 0);
  CHECK(m.status == 200 && m.reason == "OK" && m.body == "hi");
  CHECK(gxsHTTPParseResponse("HTTP/1.1 204\r\n\r\n", m) == 0 && m.status == 204);
  CHECK(gxsHTTPParseResponse("HTTP/1.1 200abc OK\r\n\r\n", m) != 0);
  CHECK(gxsHTTPParseResponse("HTTP/1.1 20 OK\r\n\r\n", m) != 0);
  CHECK(gxsHTTPParseResponse("HTTP/1.1 2000 OK\r\n\r\n", m) != 0);
  CHECK(gxsHTTPParseResponse("HTTP/1.1 +200 OK\r\n\r\n", m) != 0);
  CHECK(gxsHTTPParseResponse("HTTP/1.1  200 OK\r\n\r\n", m) != 0);
  CHECK(gxsHTTPParseResponse("HTTP/1.1 099 OK\r\n\r\n", m) != 0);
  CHECK(gxsHTTPParseResponse("HTTP/1.1 200 O\x01K\r\n\r\n", m) != 0);
  CHECK(gxsHTTPParseResponse("FTP/1.1 200 OK\r\n\r\n", m) != 0);
  finish("http_status_code_3_digits");
}

static void test_http_interim()
{
  start();
  gxsHTTPMessage m;
  std::string cont = "HTTP/1.1 100 Continue\r\n\r\n";
  std::string final_resp = "HTTP/1.1 200 OK\r\nContent-Length: 1\r\n\r\nx";
  std::string ten, eleven;
  for(int i = 0; i < 10; i++) ten += cont;
  eleven = ten + cont;
  CHECK(gxsHTTPParseResponse(ten + final_resp, m) == 0 && m.status == 200);
  CHECK(gxsHTTPParseResponse(eleven + final_resp, m) != 0);
  // An endless stream of 1xx responses ends with an error
  std::string many;
  for(int i = 0; i < 10000; i++) many += "HTTP/1.1 102 Processing\r\n\r\n";
  CHECK(gxsHTTPParseResponse(many, m) != 0);
  CHECK(gxsHTTPParseResponse("HTTP/1.1 101 Switching Protocols\r\n"
			     "Upgrade: websocket\r\n\r\n" + final_resp, m) != 0);
  finish("http_interim_responses");
}

static void test_http_framing()
{
  start();
  gxsHTTPMessage m;
  // Chunked and Content-Length together
  CHECK(gxsHTTPParseResponse("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n"
			     "Content-Length: 3\r\n\r\n3\r\nabc\r\n0\r\n\r\n", m) != 0);
  CHECK(gxsHTTPParseRequest("POST / HTTP/1.1\r\nContent-Length: 3\r\n"
			    "Transfer-Encoding: chunked\r\n\r\n3\r\nabc\r\n0\r\n\r\n", m) != 0);
  // Chunked alone
  CHECK(gxsHTTPParseRequest("POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n"
			    "3\r\nabc\r\n0\r\n\r\n", m) == 0 && m.body == "abc");
  // Chunked must be last; unknown coding on a request has no length
  CHECK(gxsHTTPParseRequest("POST / HTTP/1.1\r\nTransfer-Encoding: chunked, gzip\r\n\r\n"
			    "3\r\nabc\r\n0\r\n\r\n", m) != 0);
  CHECK(gxsHTTPParseRequest("POST / HTTP/1.1\r\nTransfer-Encoding: gzip\r\n\r\n", m) != 0);
  // Conflicting and duplicate Content-Length
  CHECK(gxsHTTPParseRequest("POST / HTTP/1.1\r\nContent-Length: 3\r\n"
			    "Content-Length: 4\r\n\r\nabcd", m) != 0);
  CHECK(gxsHTTPParseRequest("POST / HTTP/1.1\r\nContent-Length: 3, 4\r\n\r\nabcd", m) != 0);
  CHECK(gxsHTTPParseRequest("POST / HTTP/1.1\r\nContent-Length: 3\r\n"
			    "Content-Length: 3\r\n\r\nabc", m) == 0 && m.body == "abc");
  CHECK(gxsHTTPParseRequest("POST / HTTP/1.1\r\nContent-Length: 3, 3\r\n\r\nabc", m) == 0);
  // Not a plain decimal number
  CHECK(gxsHTTPParseRequest("POST / HTTP/1.1\r\nContent-Length: +3\r\n\r\nabc", m) != 0);
  CHECK(gxsHTTPParseRequest("POST / HTTP/1.1\r\nContent-Length: -1\r\n\r\nabc", m) != 0);
  CHECK(gxsHTTPParseRequest("POST / HTTP/1.1\r\nContent-Length: 0x3\r\n\r\nabc", m) != 0);
  CHECK(gxsHTTPParseRequest("POST / HTTP/1.1\r\nContent-Length: 99999999999999999999999\r\n\r\n", m) != 0);
  // Chunk sizes are hex digits only
  CHECK(gxsHTTPParseRequest("POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n"
			    "0x3\r\nabc\r\n0\r\n\r\n", m) != 0);
  CHECK(gxsHTTPParseRequest("POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n"
			    "-3\r\nabc\r\n0\r\n\r\n", m) != 0);
  CHECK(gxsHTTPParseRequest("POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n"
			    "ffffffffffffffffff\r\nabc\r\n0\r\n\r\n", m) != 0);
  finish("http_body_framing");
}

static void test_http_fields()
{
  start();
  gxsHTTPMessage m;
  CHECK(gxsHTTPIsToken("Content-Type") && gxsHTTPIsToken("X-a!#$%&'*+.^_`|~1"));
  CHECK(!gxsHTTPIsToken("") && !gxsHTTPIsToken("A B") && !gxsHTTPIsToken("A:B") &&
	!gxsHTTPIsToken(S("A\0B")) && !gxsHTTPIsToken("A\x7f") &&
	!gxsHTTPIsToken("A(B)") && !gxsHTTPIsToken("\xc3\xa9"));
  CHECK(gxsHTTPHasControlChars(S("a\0b")) && gxsHTTPHasControlChars("a\x7f"));
  CHECK(gxsHTTPHasControlChars("a\tb") && !gxsHTTPHasControlChars("a\tb", true));
  CHECK(!gxsHTTPValidHeader("X-A", "a\rb") && !gxsHTTPValidHeader("X-A", "a\x01") &&
	!gxsHTTPValidHeader("X A", "b") && gxsHTTPValidHeader("X-A", "b\tc \xc3\xa9"));

  CHECK(gxsHTTPParseRequest("GET /a HTTP/1.1\r\nHost: x\r\n\r\n", m) == 0 &&
	m.target == "/a" && m.Header("Host") == "x");
  // Header name with white space before the colon, or not a token
  CHECK(gxsHTTPParseRequest("GET / HTTP/1.1\r\nHost : x\r\n\r\n", m) != 0);
  CHECK(gxsHTTPParseRequest("GET / HTTP/1.1\r\nHo(st: x\r\n\r\n", m) != 0);
  // Control characters in a value, a name or the target
  CHECK(gxsHTTPParseRequest(S("GET / HTTP/1.1\r\nHost: a\0b\r\n\r\n"), m) != 0);
  CHECK(gxsHTTPParseRequest("GET / HTTP/1.1\r\nHost: a\x01" "b\r\n\r\n", m) != 0);
  CHECK(gxsHTTPParseRequest("GET / HTTP/1.1\r\nX-A: a\rb\r\n\r\n", m) != 0);
  CHECK(gxsHTTPParseRequest("GET /a\x01 HTTP/1.1\r\n\r\n", m) != 0);
  CHECK(gxsHTTPParseRequest("GET /a\x7f HTTP/1.1\r\n\r\n", m) != 0);
  CHECK(gxsHTTPParseRequest("G(T / HTTP/1.1\r\n\r\n", m) != 0);
  CHECK(gxsHTTPParseRequest("GET  / HTTP/1.1\r\n\r\n", m) != 0);
  CHECK(gxsHTTPParseRequest("GET / HTTP/1.1 x\r\n\r\n", m) != 0);
  CHECK(gxsHTTPParseRequest("GET / XTTP/1.1\r\n\r\n", m) != 0);
  // Folded value with a control character
  CHECK(gxsHTTPParseRequest("GET / HTTP/1.1\r\nX-A: a\r\n b\x02\r\n\r\n", m) != 0);
  CHECK(gxsHTTPParseRequest("GET / HTTP/1.1\r\nX-A: a\r\n\tb\r\n\r\n", m) == 0 &&
	m.Header("X-A") == "a b");
  finish("http_field_checks");
}

// 7: max_body is clamped to INT_MAX and still enforced
static void test_http_max_body()
{
  start();
  gxsHTTPMessage m;
  std::string r = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello";
  CHECK(gxsHTTPParseResponse(r, m, false, SIZE_MAX) == 0 && m.body == "hello");
  CHECK(gxsHTTPParseResponse(r, m, false, 4) != 0);
  CHECK(gxsHTTPParseResponse("HTTP/1.1 200 OK\r\nContent-Length: 4294967301\r\n\r\nhello",
			     m, false, SIZE_MAX) != 0);
  CHECK(gxsHTTPParseResponse("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
			     "100000005\r\nhello\r\n0\r\n\r\n", m, false, SIZE_MAX) != 0);
  CHECK(gxsHTTPParseResponse("HTTP/1.1 200 OK\r\n\r\nto the end", m, false, 5) != 0);
  CHECK(gxsHTTPParseResponse("HTTP/1.1 200 OK\r\n\r\nto the end", m) == 0 &&
	m.body == "to the end");
  finish("http_max_body");
}

// 6: URL decoding checks for servers
static void test_url()
{
  start();
  std::string out;
  CHECK(gxsURL::DecodeStrict("/a%20b/%2e%2e", out) && out == "/a b/..");
  CHECK(!gxsURL::DecodeStrict("/a%00b", out) && out.empty());
  CHECK(!gxsURL::DecodeStrict("/a%0d%0aX", out));
  CHECK(!gxsURL::DecodeStrict("/a%7f", out));
  CHECK(!gxsURL::DecodeStrict("/a%1F", out));
  CHECK(!gxsURL::DecodeStrict("/a%", out) && !gxsURL::DecodeStrict("/a%4", out) &&
	!gxsURL::DecodeStrict("/a%zz", out));
  CHECK(!gxsURL::DecodeStrict(S("/a\0"), out));
  CHECK(gxsURL::DecodeStrict("a+b", out, true) && out == "a b");
  CHECK(gxsURL::HasControlChars(gxsURL::Decode("%00")) &&
	!gxsURL::HasControlChars("/a b\xc3\xa9"));
  gxsURLInfo u;
  CHECK(gxsURL::Parse("https://example.com:8443/a?b#c", u) && u.port == 8443 &&
	u.host == "example.com" && u.path == "/a");
  CHECK(!gxsURL::Parse("https://example.com/a\r\nX: y", u));
  CHECK(!gxsURL::Parse(S("https://example.com/\0"), u));
  CHECK(!gxsURL::Parse("https://exa mple.com/", u));
  finish("url_decode_checks");
}

// 3, 4: FTP reply parsers
static void test_ftp()
{
  start();
  long long n = 0;
  CHECK(!gxsFTPClient::ParseSizeReply("213", n) && n == -1);
  CHECK(!gxsFTPClient::ParseSizeReply("213 ", n));
  CHECK(!gxsFTPClient::ParseSizeReply("213 -5", n));
  CHECK(!gxsFTPClient::ParseSizeReply("213 12x", n));
  CHECK(!gxsFTPClient::ParseSizeReply("213 99999999999999999999", n));
  CHECK(!gxsFTPClient::ParseSizeReply("550 nope", n));
  CHECK(gxsFTPClient::ParseSizeReply("213 150000", n) && n == 150000);
  CHECK(gxsFTPClient::ParseSizeReply("213-info\n213 42", n) && n == 42);
  finish("ftp_size_reply");

  start();
  CHECK(gxsFTPClient::ParsePASVPort("227 Entering Passive Mode (127,0,0,1,4,1).") == 1025);
  CHECK(gxsFTPClient::ParsePASVPort("227 Entering Passive Mode ( 127, 0,0,1,255,255)") == 65535);
  CHECK(gxsFTPClient::ParsePASVPort("227 =127,0,0,1,4,1") == 1025);
  CHECK(gxsFTPClient::ParsePASVPort("227 (256,0,0,1,4,1)") == -1);
  CHECK(gxsFTPClient::ParsePASVPort("227 (127,0,0,1,256,1)") == -1);
  CHECK(gxsFTPClient::ParsePASVPort("227 (127,0,0,1,4,-1)") == -1);
  CHECK(gxsFTPClient::ParsePASVPort("227 (127,0,0,1,99999999999,1)") == -1);
  CHECK(gxsFTPClient::ParsePASVPort("227 (127,0,0,1,0,0)") == -1);
  CHECK(gxsFTPClient::ParsePASVPort("227 (127,0,0,1,4)") == -1);
  CHECK(gxsFTPClient::ParsePASVPort("227 (127,0,0,1,4,1,1)") == -1);
  CHECK(gxsFTPClient::ParsePASVPort("227 (127,0,0,1,4,1") == -1);
  CHECK(gxsFTPClient::ParsePASVPort("227 (1 2,0,0,1,4,1)") == -1);
  CHECK(gxsFTPClient::ParsePASVPort("227 (127,0,0,1,4,0x1)") == -1);
  CHECK(gxsFTPClient::ParsePASVPort("") == -1 && gxsFTPClient::ParsePASVPort("227") == -1);
  finish("ftp_pasv_parse");

  start();
  CHECK(gxsFTPClient::ParseEPSVPort("229 Entering Extended Passive Mode (|||6446|)") == 6446);
  CHECK(gxsFTPClient::ParseEPSVPort("229 ok (!!!21!)") == 21);
  CHECK(gxsFTPClient::ParseEPSVPort("229 (|||0|)") == -1);
  CHECK(gxsFTPClient::ParseEPSVPort("229 (|||65536|)") == -1);
  CHECK(gxsFTPClient::ParseEPSVPort("229 (|||99999999999999|)") == -1);
  CHECK(gxsFTPClient::ParseEPSVPort("229 (|||-5|)") == -1);
  CHECK(gxsFTPClient::ParseEPSVPort("229 (|||12") == -1);
  CHECK(gxsFTPClient::ParseEPSVPort("229 (|||12|") == -1);
  CHECK(gxsFTPClient::ParseEPSVPort("229 (||") == -1);
  CHECK(gxsFTPClient::ParseEPSVPort("229 (11121)") == -1);
  CHECK(gxsFTPClient::ParseEPSVPort("229 (|||12!)") == -1);
  CHECK(gxsFTPClient::ParseEPSVPort("229 (") == -1);
  finish("ftp_epsv_parse");

  start();
  std::string r;
  std::size_t used = 0;
  CHECK(gxsFTPClient::ParseReply("220 hi\r\nnext", r, &used) == 220 && r == "220 hi" &&
	used == 8);
  CHECK(gxsFTPClient::ParseReply("150-a\r\n b\r\n150x\r\n150 done\r\n", r) == 150 &&
	r == "150-a\n b\n150x\n150 done");
  CHECK(gxsFTPClient::ParseReply("abc\r\n", r) == -1);
  CHECK(gxsFTPClient::ParseReply("22\r\n", r) == -1);
  CHECK(gxsFTPClient::ParseReply("2x0 hi\r\n", r) == -1);
  CHECK(gxsFTPClient::ParseReply("220xhi\r\n", r) == -1);
  CHECK(gxsFTPClient::ParseReply("150-a\r\nb\r\n", r) == -1);
  CHECK(gxsFTPClient::ParseReply("220 no line end", r) == -1);
  finish("ftp_reply_parse");
}

// 5: SMTP addresses and replies
static void test_smtp()
{
  start();
  std::string a;
  CHECK(gxsSMTPClient::NormalizeAddress("a@b.com", a) && a == "<a@b.com>");
  CHECK(gxsSMTPClient::NormalizeAddress(" <a@b.com> ", a) && a == "<a@b.com>");
  CHECK(gxsSMTPClient::NormalizeAddress("Some One <a@b.com>", a) && a == "<a@b.com>");
  CHECK(!gxsSMTPClient::NormalizeAddress("a@b.com> NOTIFY=SUCCESS", a));
  CHECK(!gxsSMTPClient::NormalizeAddress("<a@b.com> NOTIFY=SUCCESS", a));
  CHECK(!gxsSMTPClient::NormalizeAddress("Name <a@b.com> SIZE=1", a));
  CHECK(!gxsSMTPClient::NormalizeAddress("<a@b.com>>", a));
  CHECK(!gxsSMTPClient::NormalizeAddress("<a@b.com><c@d.com>", a));
  CHECK(!gxsSMTPClient::NormalizeAddress("<a<b@c.com>", a));
  CHECK(!gxsSMTPClient::NormalizeAddress("<a b@c.com>", a));
  CHECK(!gxsSMTPClient::NormalizeAddress("a b@c.com", a));
  CHECK(!gxsSMTPClient::NormalizeAddress("<a@b.com", a));
  CHECK(!gxsSMTPClient::NormalizeAddress("a@b.com>", a));
  CHECK(!gxsSMTPClient::NormalizeAddress("x> <a@b.com>", a));
  CHECK(!gxsSMTPClient::NormalizeAddress("<a\x01@b.com>", a));
  CHECK(!gxsSMTPClient::NormalizeAddress("<a@b.com\x7f>", a));
  CHECK(!gxsSMTPClient::NormalizeAddress("Na\x01me <a@b.com>", a));
  CHECK(!gxsSMTPClient::NormalizeAddress("<>", a) && !gxsSMTPClient::NormalizeAddress("", a));
  CHECK(gxsSMTPClient::NormalizeAddress("<>", a, true) && a == "<>");
  finish("smtp_address_injection");

  start();
  std::string r;
  CHECK(gxsSMTPClient::ParseReply("250-mock\r\n250-AUTH PLAIN\r\n250 8BITMIME\r\n", r) == 250 &&
	r == "250-mock\n250-AUTH PLAIN\n250 8BITMIME");
  CHECK(gxsSMTPClient::ParseReply("220\r\n", r) == 220);
  CHECK(gxsSMTPClient::ParseReply("2a0 x\r\n", r) == -1);
  CHECK(gxsSMTPClient::ParseReply("250x\r\n", r) == -1);
  CHECK(gxsSMTPClient::ParseReply("25\r\n", r) == -1);
  CHECK(gxsSMTPClient::ParseReply("250-a\r\n", r) == -1);
  std::string many;
  for(int i = 0; i < 2000; i++) many += "250-x\r\n";
  CHECK(gxsSMTPClient::ParseReply(many + "250 end\r\n", r) == -1);
  finish("smtp_reply_parse");
}

// 7: POP3 multi-line size cap
static void test_pop3()
{
  start();
  std::string d;
  std::size_t used = 0;
  CHECK(gxsPOP3Client::ParseMultiline("line 1\r\n..dot line\r\n.\r\nnext", d, 1024, &used) == 0 &&
	d == "line 1\r\n.dot line\r\n" && used == 23);
  CHECK(gxsPOP3Client::ParseMultiline("line 1\r\n", d) == -1);
  std::string big(100, 'x');
  CHECK(gxsPOP3Client::ParseMultiline(big + "\r\n" + big + "\r\n.\r\n", d, 150) == -1);
  CHECK(gxsPOP3Client::ParseMultiline(big + "\r\n" + big + "\r\n.\r\n", d, 204) == 0);
  finish("pop3_multiline_max_size");
}

// 8: https_server path mapping
static void test_path_mapping()
{
  start();
  std::error_code ec;
  fs::path www = fs::current_path() / "proto_security_www";
  fs::remove_all(www, ec);
  fs::create_directories(www / "sub");
  fs::create_directories(www / ".git");
  std::ofstream(www / "index.html") << "index";
  std::ofstream(www / "sub" / "a.txt") << "a";
  std::ofstream(www / ".git" / "config") << "secret";
  std::ofstream(www / "sub" / ".htpasswd") << "secret";
  fs::path root = fs::weakly_canonical(www);
  fs::path f;

  CHECK(map_request_path(root, "/", f, false) == 0 && f == root / "index.html");
  CHECK(map_request_path(root, "/sub/a.txt?x=1", f, false) == 0 && f == root / "sub" / "a.txt");
  CHECK(map_request_path(root, "/sub//a.txt", f, false) == 0);
  CHECK(map_request_path(root, "/sub/", f, false) == 0 && f == root / "sub" / "index.html");
  CHECK(map_request_path(root, "/missing.txt", f, false) == 0);
  // Traversal
  CHECK(map_request_path(root, "/../etc/passwd", f, false) == 403);
  CHECK(map_request_path(root, "/%2e%2e/%2e%2e/etc/passwd", f, false) == 403);
  CHECK(map_request_path(root, "/sub/%2E%2E/%2E%2E/x", f, false) == 403);
  CHECK(map_request_path(root, "/sub/./a.txt", f, false) == 403);
  CHECK(map_request_path(root, "/sub%2f..%2f..%2fx", f, false) == 403);
  CHECK(map_request_path(root, "/a%5c..%5c..%5cx", f, false) == 403);
  CHECK(map_request_path(root, "relative", f, false) == 400);
  CHECK(map_request_path(root, "", f, false) == 400);
  // Dotfiles
  CHECK(map_request_path(root, "/.git/config", f, false) == 403);
  CHECK(map_request_path(root, "/sub/.htpasswd", f, false) == 403);
  CHECK(map_request_path(root, "/%2egit/config", f, false) == 403);
  // NUL and control characters
  CHECK(map_request_path(root, "/index.html%00.txt", f, false) == 400);
  CHECK(map_request_path(root, "/a%0d%0aX", f, false) == 400);
  CHECK(map_request_path(root, S("/a\0b"), f, false) == 400);
  CHECK(map_request_path(root, "/a%zz", f, false) == 400);
  // Windows names, checked on every platform with windows_rules
  CHECK(map_request_path(root, "/CON", f, true) == 403);
  CHECK(map_request_path(root, "/sub/nul.txt", f, true) == 403);
  CHECK(map_request_path(root, "/com1", f, true) == 403);
  CHECK(map_request_path(root, "/LPT9.log", f, true) == 403);
  CHECK(map_request_path(root, "/aux%20.txt", f, true) == 403);
  CHECK(map_request_path(root, "/a:b", f, true) == 403);
  CHECK(map_request_path(root, "/c:%2fwindows", f, true) == 403);
  CHECK(map_request_path(root, "/index.html.", f, true) == 403);
  CHECK(map_request_path(root, "/index.html%20", f, true) == 403);
  CHECK(map_request_path(root, "/console.txt", f, true) == 0);
  CHECK(map_request_path(root, "/com10", f, true) == 0);
#ifndef _WIN32
  // Without the Windows rules CON is an ordinary name
  CHECK(map_request_path(root, "/CON", f, false) == 0);
  // Symbolic link escape
  fs::create_directory_symlink("/etc", www / "esc", ec);
  fs::create_directory_symlink(www / "sub", www / "inside", ec);
  fs::create_symlink("/etc/passwd", www / "pw.txt", ec);
  CHECK(map_request_path(root, "/esc/passwd", f, false) == 403);
  CHECK(map_request_path(root, "/pw.txt", f, false) == 403);
  CHECK(map_request_path(root, "/inside/a.txt", f, false) == 0 &&
	f == root / "sub" / "a.txt");
#endif
  // Log escaping
  CHECK(log_escape("GET /a\r\nX") == "GET /a\\x0D\\x0AX");
  CHECK(log_escape(S("a\0\x1b[31m")) == "a\\x00\\x1B[31m");
  fs::remove_all(www, ec);
  finish("https_server_path_mapping");
}

int main()
{
  test_http_status();
  test_http_interim();
  test_http_framing();
  test_http_fields();
  test_http_max_body();
  test_url();
  test_ftp();
  test_smtp();
  test_pop3();
  test_path_mapping();
  if(failures) printf("%d test(s) failed\n", failures);
  return failures ? 1 : 0;
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
